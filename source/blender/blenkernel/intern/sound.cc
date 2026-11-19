/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <numeric>
#include <optional>
#include <string>
#include <thread>
#include <tuple>
#include <utility>

#ifdef WITH_FFTW3
#  include <fftw3.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_bounds.hh"
#include "BLI_build_config.hh"
#include "BLI_enum_flags.hh"
#include "BLI_function_ref.hh"
#include "BLI_listbase.hh"
#include "BLI_map.hh"
#include "BLI_math_base_c.hh"
#include "BLI_math_rotation_c.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.hh"
#include "BLI_threads.hh"

#include "BLT_translation.hh"

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"
#include "DNA_speaker_types.h"
#include "DNA_userdef_types.h"


#include "BKE_anim_data.hh"
#include "BKE_animsys.hh"
#include "BKE_bpath.hh"
#include "BKE_global.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_packedFile.hh"
#include "BKE_scene.hh"
#include "BKE_scene_runtime.hh"
#include "BKE_sound.hh"
#include "BKE_sound_sample.hh"


#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "BLO_read_write.hh"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "SEQ_iterator.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_sound.hh"

#include "BLI_concurrent_map.hh"

#include "CLG_log.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.hh"
#endif

namespace blender {

namespace bke {
struct SceneAudioRuntime;

enum class SoundTags {
  None = 0,
  /* Do not free/reset waveform on sound load, only used by undo code. */
  WaveformNoReload = 1 << 0,
  WaveformLoading = 1 << 1,
};
ENUM_OPERATORS(SoundTags);

using bSoundFrequencySamplerMap =
    ConcurrentMap<bSoundFrequencySampler::Key, std::shared_ptr<bSoundFrequencySampler>>;


struct SoundRuntime {
  AUD_Sound handle;
  AUD_Sound cache;
  /* The audaspace handle that should actually be played back.
   * Should be cache if cache != NULL; otherwise its handle. */
  AUD_Sound playback_handle;
  /* Spin-lock for asynchronous loading of sounds. */
  SpinLock spinlock;

  /* Note: not by-value since #sound_foreach_cache can only
   * save/restore a pointer. */
  Vector<float> *waveform = nullptr;
  SoundTags tags = SoundTags::None;

};

}  // namespace bke

static void sound_free_audio(bSound *sound);

static void sound_init_runtime(bSound *sound)
{
  sound->runtime = MEM_new<bke::SoundRuntime>(__func__);
  BLI_spin_init(&sound->runtime->spinlock);
}


static void sound_free_waveform(bSound *sound)
{
  bke::SoundRuntime *runtime = sound->runtime;
  if (!flag_is_set(runtime->tags, bke::SoundTags::WaveformNoReload)) {
    MEM_SAFE_DELETE(runtime->waveform);
  }
  /* This tag is only valid once. */
  runtime->tags &= ~bke::SoundTags::WaveformNoReload;
}

static void sound_copy_data(Main * /*bmain*/,
                            std::optional<Library *> /*owner_library*/,
                            ID *id_dst,
                            const ID *id_src,
                            const int /*flag*/)
{
  bSound *sound_dst = id_cast<bSound *>(id_dst);
  const bSound *sound_src = id_cast<const bSound *>(id_src);

  /* Just to be sure, should not have any value actually after reading time. */
  sound_dst->newpackedfile = nullptr;

  if (sound_src->packedfile != nullptr) {
    sound_dst->packedfile = BKE_packedfile_duplicate(sound_src->packedfile);
  }

  sound_init_runtime(sound_dst);
}

static void sound_free_data(ID *id)
{
  bSound *sound = id_cast<bSound *>(id);

  if (sound->packedfile) {
    BKE_packedfile_free(sound->packedfile);
    sound->packedfile = nullptr;
  }

  if (sound->runtime) {
    sound_free_audio(sound);
    sound_free_waveform(sound);
    BLI_spin_end(&sound->runtime->spinlock);
    MEM_delete(sound->runtime);
  }
}

static void sound_foreach_cache(ID *id,
                                IDTypeForeachCacheFunctionCallback function_callback,
                                void *user_data)
{
  bSound *sound = id_cast<bSound *>(id);
  IDCacheKey key = {id->session_uid, 1};
  function_callback(id, &key, reinterpret_cast<void **>(&sound->runtime->waveform), 0, user_data);
}

static void sound_foreach_path(ID *id, BPathForeachPathData *bpath_data)
{
  bSound *sound = id_cast<bSound *>(id);
  if (sound->packedfile != nullptr && (bpath_data->flag & BKE_BPATH_FOREACH_PATH_SKIP_PACKED) != 0)
  {
    return;
  }

  /* FIXME: This does not check for empty path... */
  BKE_bpath_foreach_path_fixed_process(bpath_data, sound->filepath);
}

static void sound_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  bSound *sound = id_cast<bSound *>(id);
  const bool is_undo = writer->is_undo();

  /* Clean up, important in undo case to reduce false detection of changed datablocks. */
  sound->runtime = nullptr;

  /* Do not store packed files in case this is a library override ID. */
  if (ID_IS_OVERRIDE_LIBRARY(sound) && !is_undo) {
    sound->packedfile = nullptr;
  }

  /* write LibData */
  writer->write_id_struct(id_address, sound);
  BKE_id_blend_write(writer, &sound->id);

  BKE_packedfile_blend_write(writer, sound->packedfile);
}

static void sound_blend_read_data(BlendDataReader *reader, ID *id)
{
  bSound *sound = id_cast<bSound *>(id);
  sound_init_runtime(sound);
  if (BLO_read_data_is_undo(reader)) {
    sound->runtime->tags |= bke::SoundTags::WaveformNoReload;
  }

  BKE_packedfile_blend_read(reader, &sound->packedfile, sound->filepath);
  BKE_packedfile_blend_read(reader, &sound->newpackedfile, sound->filepath);
}

IDTypeInfo IDType_ID_SO = {
    .id_code = bSound::id_type,
    .id_filter = FILTER_ID_SO,
    .dependencies_id_types = 0,
    .main_listbase_index = INDEX_ID_SO,
    .struct_size = sizeof(bSound),
    .name = "Sound",
    .name_plural = N_("sounds"),
    .translation_context = BLT_I18NCONTEXT_ID_SOUND,
    .flags = IDTYPE_FLAGS_NO_ANIMDATA | IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    .asset_type_info = nullptr,

    /* A fuzzy case, think NULLified content is OK here... */
    .init_data = nullptr,
    .copy_data = sound_copy_data,
    .free_data = sound_free_data,
    .make_local = nullptr,
    .foreach_id = nullptr,
    .foreach_cache = sound_foreach_cache,
    .foreach_path = sound_foreach_path,
    .foreach_working_space_color = nullptr,
    .foreach_asset_weak_reference = nullptr,
    .owner_pointer_get = nullptr,

    .blend_write = sound_blend_write,
    .blend_read_data = sound_blend_read_data,
    .blend_read_after_liblink = nullptr,

    .blend_read_undo_preserve = nullptr,

    .lib_override_apply_post = nullptr,
};


BLI_INLINE void sound_verify_evaluated_id(const ID *id)
{
  UNUSED_VARS_NDEBUG(id);
  /* This is a bit tricky and not quite reliable, but good enough check.
   *
   * We don't want audio system handles to be allocated on an original data-blocks, and only want
   * them to be allocated on a data-blocks which are result of dependency graph evaluation.
   *
   * Data-blocks which are covered by a copy-on-evaluation system of dependency graph will have
   * ID_TAG_COPIED_ON_EVAL tag set on them.
   *
   * Additionally, we also allow data-blocks outside of main database. Those can not be "original"
   * and could be used as a temporary evaluated result during operations like baking.
   *
   * NOTE: We consider ID evaluated if ANY of those flags is set. We do NOT require ALL of them.
   */
  BLI_assert(id->tag & (ID_TAG_COPIED_ON_EVAL | ID_TAG_NO_MAIN));
}

bSound *BKE_sound_new_file(Main *bmain, const char *filepath, short stream_index)
{
  bSound *sound;
  const char *blendfile_path = BKE_main_blendfile_path(bmain);
  char filepath_abs[FILE_MAX];

  STRNCPY(filepath_abs, filepath);
  BLI_path_abs(filepath_abs, blendfile_path);

  sound = static_cast<bSound *>(BKE_libblock_alloc(bmain, ID_SO, BLI_path_basename(filepath), 0));
  STRNCPY(sound->filepath, filepath);
  sound_init_runtime(sound);
  sound->stream_index = stream_index;

  /* Extract sound specs for bSound */
  SoundInfo info;
  bool success = BKE_sound_info_get(bmain, sound, &info);
  if (success) {
    sound->samplerate = info.specs.samplerate;
    sound->audio_channels = info.specs.channels;
  }

  return sound;
}

static bSound *sound_new_file_exists_ex(Main *bmain, const char *filepath, short stream_index)
{
  bSound *sound;
  char filepath_abs[FILE_MAX], filepath_test[FILE_MAX];

  STRNCPY(filepath_abs, filepath);
  BLI_path_abs(filepath_abs, BKE_main_blendfile_path(bmain));

  /* Search for an existing sound matching both filepath and stream index. */
  for (sound = bmain->sounds.first(); sound; sound = static_cast<bSound *>(sound->id.next)) {
    if (sound->stream_index != stream_index) {
      continue;
    }
    STRNCPY(filepath_test, sound->filepath);
    BLI_path_abs(filepath_test, ID_BLEND_PATH(bmain, &sound->id));

    if (BLI_path_cmp(filepath_test, filepath_abs) == 0) {
      id_us_plus(&sound->id); /* officially should not, it doesn't link here! */
      return sound;
    }
  }

  return BKE_sound_new_file(bmain, filepath, stream_index);
}

bSound *BKE_sound_new_file_exists(Main *bmain, const char *filepath, short stream_index)
{
  return sound_new_file_exists_ex(bmain, filepath, stream_index);
}

void BKE_sound_invalidate_scene_audio_animation_cache(Scene *scene)
{
  BLI_assert(DEG_is_evaluated(scene));
  scene->runtime->audio.animation_cache_dirty = true;
}

int BKE_sound_scene_audio_frame(const Depsgraph *depsgraph, const Scene *scene)
{
  if (scene != DEG_get_evaluated_scene(depsgraph) ||
      (scene->runtime->audio.animation_cache_type != bke::AudioAnimationCacheType::None &&
       scene->r.subframe != 0.0f))
  {
    return -1;
  }
  return scene->r.cfra;
}


void BKE_sound_ensure_playback_audio_animation(Main *bmain,
                                               Scene *scene_orig,
                                               Depsgraph *depsgraph)
{
  UNUSED_VARS(bmain, scene_orig, depsgraph);
}

void BKE_sound_discard_outdated_audio_animation(Main *bmain, Depsgraph *depsgraph)
{
  UNUSED_VARS(bmain, depsgraph);
}

std::shared_ptr<bke::SoundAnimationUse> BKE_sound_audio_animation_use(const Scene *scene)
{
  UNUSED_VARS(scene);
  return nullptr;
}

void BKE_sound_record_scene_audio_animation(Depsgraph *depsgraph)
{
  UNUSED_VARS(depsgraph);
}

void BKE_sound_end_scene_audio_recording(Depsgraph *depsgraph)
{
  UNUSED_VARS(depsgraph);
}

std::shared_ptr<bke::SoundRecording> BKE_sound_recording_ensure(const Scene *scene)
{
  UNUSED_VARS(scene);
  return nullptr;
}

std::unique_lock<std::mutex> BKE_sound_recording_lock(bke::SoundRecording &recording)
{
  UNUSED_VARS(recording);
  return {};
}

bool BKE_sound_recording_references_recorded(const bke::SoundRecording &recording,
                                             const double time)
{
  UNUSED_VARS(recording, time);
  return true;
}

bool BKE_sound_bake_animation(Main *bmain,
                              Scene *scene_orig,
                              Depsgraph *depsgraph,
                              const int frame_start,
                              const int frame_end)
{
  UNUSED_VARS(bmain, scene_orig, depsgraph, frame_start, frame_end);
  return true;
}

static void sound_free_audio(bSound *sound)
{
  UNUSED_VARS(sound);
}


void BKE_sound_update_sequence_handle(const Scene *scene,
                                      AUD_SequenceEntry handle,
                                      AUD_Sound sound_handle)
{
  UNUSED_VARS(scene, handle, sound_handle);
}


#  include "BLI_utildefines.hh"

void BKE_sound_force_device(const char * /*device*/) {}
void BKE_sound_init_once() {}
void BKE_sound_init(Main * /*bmain*/) {}
void BKE_sound_exit_once() {}
void BKE_sound_load(Main * /*bmain*/, bSound * /*sound*/) {}
void BKE_sound_packfile_ensure(Main * /*bmain*/, bSound * /*sound*/, ReportList * /*reports*/) {}
void BKE_sound_create_scene(Scene * /*scene*/) {}
void BKE_sound_destroy_scene(Scene * /*scene*/) {}
void BKE_sound_lock() {}
void BKE_sound_unlock() {}
void BKE_sound_refresh_callback_bmain(Main * /*bmain*/) {}
void BKE_sound_reset_scene_specs(Scene * /*scene*/) {}
void BKE_sound_mute_scene(Scene * /*scene*/, bool /*muted*/) {}
AUD_SequenceEntry BKE_sound_scene_add_scene_sound(Scene * /*scene*/, Strip * /*strip*/)
{
  return nullptr;
}
AUD_SequenceEntry BKE_sound_add_scene_sound(Scene * /*scene*/, Strip * /*strip*/)
{
  return nullptr;
}
void BKE_sound_remove_scene_sound(Scene * /*scene*/, AUD_SequenceEntry /*handle*/) {}
void BKE_sound_mute_scene_sound(const Scene * /*scene*/,
                                AUD_SequenceEntry /*handle*/,
                                bool /*mute*/)
{
}
void BKE_sound_move_scene_sound(const Scene * /*scene*/,
                                AUD_SequenceEntry /*handle*/,
                                int /*startframe*/,
                                int /*endframe*/,
                                int /*frameskip*/,
                                double /*audio_offset*/)
{
}
void BKE_sound_move_scene_sound_defaults(Scene * /*scene*/, Strip * /*strip*/) {}
void BKE_sound_play_scene(Scene * /*scene*/) {}
void BKE_sound_stop_scene(Scene * /*scene*/) {}
void BKE_sound_seek_scene(Main * /*bmain*/, Scene * /*scene*/) {}
double BKE_sound_sync_scene(Scene * /*scene*/)
{
  return NAN_FLT;
}
void BKE_sound_read_waveform(Main *bmain,
                             bSound *sound,
                             /* NOLINTNEXTLINE: readability-non-const-parameter. */
                             bool *stop)
{
  UNUSED_VARS(sound, stop, bmain);
}

void BKE_sound_update_sequencer(Main * /*main*/, bSound * /*sound*/) {}
void BKE_sound_update_scene(Depsgraph * /*depsgraph*/, Scene * /*scene*/) {}
void BKE_sound_update_scene_sound(const Scene * /*scene*/,
                                  AUD_SequenceEntry /*handle*/,
                                  bSound * /*sound*/)
{
}
void BKE_sound_update_scene_listener(Scene * /*scene*/) {}
void BKE_sound_update_fps(Main * /*bmain*/, Scene * /*scene*/) {}
void BKE_sound_set_scene_sound_volume_at_frame(AUD_SequenceEntry /*handle*/,
                                               int /*frame*/,
                                               float /*volume*/,
                                               bool /*animated*/)
{
}
void BKE_sound_set_scene_sound_pan_at_frame(AUD_SequenceEntry /*handle*/,
                                            int /*frame*/,
                                            float /*pan*/,
                                            bool /*animated*/)
{
}
void BKE_sound_set_scene_volume(Scene * /*scene*/, float /*volume*/) {}
void BKE_sound_set_scene_volume_at_frame(Scene * /*scene*/, int /*frame*/, float /*volume*/) {}
void BKE_sound_set_scene_sound_pitch_at_frame(const Scene * /*scene*/,
                                              AUD_SequenceEntry /*handle*/,
                                              int /*frame*/,
                                              float /*pitch*/)
{
}
void BKE_sound_set_scene_sound_pitch_constant_range(const Scene * /*scene*/,
                                                    AUD_SequenceEntry /*handle*/,
                                                    int /*frame_start*/,
                                                    int /*frame_end*/,
                                                    float /*pitch*/)
{
}
float BKE_sound_get_length(Main * /*bmain*/, bSound * /*sound*/)
{
  return 0;
}
char **BKE_sound_get_device_names()
{
  static char *names[1] = {nullptr};
  return names;
}

bool BKE_sound_info_get(Main * /*main*/, bSound * /*sound*/, SoundInfo * /*sound_info*/)
{
  return false;
}

bool BKE_sound_stream_info_get(Main * /*main*/,
                               const char * /*filepath*/,
                               int /*stream*/,
                               SoundStreamInfo * /*sound_info*/)
{
  return false;
}

int BKE_sound_stream_count(Main * /*main*/, const char * /*filepath*/)
{
  return 0;
}


AUD_Sound BKE_sound_ensure_time_stretch_effect(const Strip * /*strip*/, float /*fps*/)
{
  return nullptr;
}

void BKE_sound_set_scene_sound_time_stretch_at_frame(const Scene * /*scene*/,
                                                     AUD_Sound /*handle*/,
                                                     int /*frame*/,
                                                     float /*time_stretch*/)
{
}
void BKE_sound_set_scene_sound_time_stretch_constant_range(const Scene * /*scene*/,
                                                           AUD_Sound /*handle*/,
                                                           int /*frame_start*/,
                                                           int /*frame_end*/,
                                                           float /*time_stretch*/)
{
}

void BKE_sound_ensure_scene(Scene *scene)
{
  if (scene->runtime->audio.sound_scene != nullptr) {
    return;
  }
  BKE_sound_create_scene(scene);
}

static void sound_ensure_loaded(Main *bmain, bSound *sound)
{
  if (sound->runtime->cache != nullptr) {
    return;
  }
  BKE_sound_load(bmain, sound);
}

void BKE_sound_jack_sync_callback_set(SoundJackSyncCallback callback)
{
  UNUSED_VARS(callback);
}

void BKE_sound_jack_scene_update(Scene *scene, int mode, double time)
{
  sound_verify_evaluated_id(&scene->id);

  /* Ugly: Blender doesn't like it when the animation is played back during rendering. */
  if (G.is_rendering) {
    return;
  }
  UNUSED_VARS(mode, time);
}

void BKE_sound_evaluate(Depsgraph *depsgraph, Main *bmain, bSound *sound)
{
  DEG_debug_print_eval(depsgraph, __func__, sound->id.name, sound);
  if (sound->id.recalc & ID_RECALC_SOURCE) {
    /* Sequencer checks this flag to see if the strip sound is to be updated from the Audaspace
     * side. */
    sound->id.recalc |= ID_RECALC_AUDIO;
  }

  if (sound->id.recalc & ID_RECALC_AUDIO) {
    BKE_sound_load(bmain, sound);
    return;
  }
  sound_ensure_loaded(bmain, sound);
}

void BKE_sound_runtime_state_get_and_clear(const bSound *sound,
                                           AUD_Sound *r_cache,
                                           AUD_Sound *r_playback_handle,
                                           Vector<float> **r_waveform)
{
  bke::SoundRuntime *runtime = sound->runtime;
  *r_cache = runtime->cache;
  *r_playback_handle = runtime->playback_handle;
  *r_waveform = runtime->waveform;
  runtime->cache = nullptr;
  runtime->playback_handle = nullptr;
  runtime->waveform = nullptr;
}

void BKE_sound_runtime_state_set(const bSound *sound,
                                 AUD_Sound cache,
                                 AUD_Sound playback_handle,
                                 Vector<float> *waveform)
{
  bke::SoundRuntime *runtime = sound->runtime;
  runtime->cache = cache;
  runtime->playback_handle = playback_handle;
  runtime->waveform = waveform;
}

AUD_Sound BKE_sound_playback_handle_get(const bSound *sound)
{
  if (sound == nullptr) {
    return nullptr;
  }
  return sound->runtime->playback_handle;
}

void BKE_sound_runtime_clear_waveform_loading_tag(bSound *sound)
{
  bke::SoundRuntime *runtime = sound->runtime;
  BLI_spin_lock(&runtime->spinlock);
  runtime->tags &= ~bke::SoundTags::WaveformLoading;
  BLI_spin_unlock(&runtime->spinlock);
}

bool BKE_sound_runtime_start_waveform_loading(bSound *sound)
{
  bke::SoundRuntime *runtime = sound->runtime;
  bool result = false;
  BLI_spin_lock(&runtime->spinlock);
  if (runtime->waveform == nullptr) {
    /* Load the waveform data if it hasn't been loaded and cached already. */
    if (!flag_is_set(runtime->tags, bke::SoundTags::WaveformLoading)) {
      /* Prevent sounds from reloading. */
      runtime->tags |= bke::SoundTags::WaveformLoading;
      result = true;
    }
  }
  BLI_spin_unlock(&runtime->spinlock);
  return result;
}

const Vector<float> *BKE_sound_runtime_get_waveform(const bSound *sound)
{
  return sound->runtime->waveform;
}

namespace bke {

std::shared_ptr<const bSoundFrequencySampler> bSoundFrequencySampler::get_cached(
    const bSound &sound, const Key &key)
{
  UNUSED_VARS(sound, key);
  return nullptr;
}



std::optional<Array<float>> bSoundFrequencySampler::compute_fft(const int start_sample) const
{
  /* Since the result of the dft algorithm is symmetric in this case, only the first half is
   * computed. */
  const int frequencies_num = key_.fft_size / 2;

  UNUSED_VARS(start_sample);
  return Array<float>(frequencies_num, 0.0f);
}

static float catmul_rom_interpolation(
    const float p0, const float p1, const float p2, const float p3, const float t)
{
  const float t2 = t * t;
  const float t3 = t2 * t;

  const float value = 0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 +
                             (-p0 + 3 * p1 - 3 * p2 + p3) * t3);
  return value;
}

static float bspline_interpolation(
    const float p0, const float p1, const float p2, const float p3, const float t)
{
  const float t2 = t * t;
  const float t3 = t2 * t;

  const float b0 = (1.0f - t) * (1.0f - t) * (1.0f - t) / 6.0f;
  const float b1 = (3.0f * t3 - 6.0f * t2 + 4.0f) / 6.0f;
  const float b2 = (-3.0f * t3 + 3.0f * t2 + 3.0f * t + 1.0f) / 6.0f;
  const float b3 = t3 / 6.0f;

  return (b0 * p0 + b1 * p1 + b2 * p2 + b3 * p3);
}

float bSoundFrequencySampler::sample(const float time,
                                     const float low,
                                     const float high,
                                     const InterpolationMethod time_interpolation,
                                     const InterpolationMethod frequency_interpolation) const
{
  if (low >= high) {
    return 0.0f;
  }
  const float i_float = std::max(
      0.0f, (time * samples_per_second_ - key_.fft_size / 2) / window_cache_stride_);
  const int i_pre = floorf(i_float);
  const int i_post = i_pre + 1;
  const float t = fractf(i_float);

  switch (time_interpolation) {
    case InterpolationMethod::Linear: {
      const float v_prev = this->sample_frequency_range_in_window(
          i_pre, low, high, frequency_interpolation);
      const float v_next = this->sample_frequency_range_in_window(
          i_post, low, high, frequency_interpolation);

      return math::interpolate(v_prev, v_next, t);
    }
    case InterpolationMethod::CatmullRom: {
      const int i_pre2 = i_pre - 1;
      const int i_post2 = i_post + 1;

      const float p0 = this->sample_frequency_range_in_window(
          i_pre2, low, high, frequency_interpolation);
      const float p1 = this->sample_frequency_range_in_window(
          i_pre, low, high, frequency_interpolation);
      const float p2 = this->sample_frequency_range_in_window(
          i_post, low, high, frequency_interpolation);
      const float p3 = this->sample_frequency_range_in_window(
          i_post2, low, high, frequency_interpolation);

      return catmul_rom_interpolation(p0, p1, p2, p3, t);
    }
    default:
    case InterpolationMethod::BSpline: {
      const int i_pre2 = i_pre - 1;
      const int i_post2 = i_post + 1;

      const float p0 = this->sample_frequency_range_in_window(
          i_pre2, low, high, frequency_interpolation);
      const float p1 = this->sample_frequency_range_in_window(
          i_pre, low, high, frequency_interpolation);
      const float p2 = this->sample_frequency_range_in_window(
          i_post, low, high, frequency_interpolation);
      const float p3 = this->sample_frequency_range_in_window(
          i_post2, low, high, frequency_interpolation);

      return bspline_interpolation(p0, p1, p2, p3, t);
    }
  }
}

float bSoundFrequencySampler::sample_frequency_range_in_window(int window_i,
                                                               float low,
                                                               float high,
                                                               InterpolationMethod method) const
{
  const std::optional<Span<float>> window_opt = this->ensure_window_cache(window_i);
  if (!window_opt.has_value()) {
    return 0.0f;
  }
  const Span<float> window_values = *window_opt;
  const float cumulative_low = this->sample_cumulative_frequency(window_values, low, method);
  const float cumulative_high = this->sample_cumulative_frequency(window_values, high, method);
  return cumulative_high - cumulative_low;
}

float bSoundFrequencySampler::sample_cumulative_frequency(const Span<float> window_values,
                                                          const float frequency,
                                                          const InterpolationMethod method) const
{
  const int max_i = window_values.size() - 1;
  const float i_float = frequency * key_.fft_size / samples_per_second_;
  const int i_pre = std::clamp<int>(std::floor(i_float), 0, max_i);
  const int i_post = std::min(i_pre + 1, max_i);
  const float t = fractf(i_float);

  switch (method) {
    case InterpolationMethod::Linear: {
      return math::interpolate(window_values[i_pre], window_values[i_post], t);
    }
    case InterpolationMethod::CatmullRom: {
      const int i_pre2 = std::max(i_pre - 1, 0);
      const int i_post2 = std::min(i_post + 1, max_i);

      const float p0 = window_values[i_pre2];
      const float p1 = window_values[i_pre];
      const float p2 = window_values[i_post];
      const float p3 = window_values[i_post2];

      return catmul_rom_interpolation(p0, p1, p2, p3, t);
    }
    case InterpolationMethod::BSpline: {
      const int i_pre2 = std::max(i_pre - 1, 0);
      const int i_post2 = std::min(i_post + 1, max_i);

      const float p0 = window_values[i_pre2];
      const float p1 = window_values[i_pre];
      const float p2 = window_values[i_post];
      const float p3 = window_values[i_post2];

      return bspline_interpolation(p0, p1, p2, p3, t);
    }
  }
  return 0.0f;
}

std::optional<Span<float>> bSoundFrequencySampler::ensure_window_cache(int window_i) const
{
  if (window_caches_.is_empty()) {
    return std::nullopt;
  }

  /* Clamp window so that too low or high indices get mapped to the closest valid index. */
  window_i = std::clamp<int>(window_i, 0, window_caches_.size() - 1);

  const WindowCache &window = window_caches_[window_i];
  /* Compute the FFT of that window if that wasn't done already. */
  window.mutex.ensure([&]() {
    std::optional<Array<float>> fft_array = this->compute_fft(window_i * window_cache_stride_);
    if (!fft_array.has_value()) {
      return;
    }
    /* Compute prefix sum for the fft values. */
    const Span<float> fft_values = fft_array->as_span();
    window.cumulative_amplitudes.emplace(fft_values.size() + 1);
    float sum = 0.0f;
    for (const int i : fft_array->index_range()) {
      const float value = std::abs(fft_values[i]);
      (*window.cumulative_amplitudes)[i] = sum;
      sum += value;
    }
    window.cumulative_amplitudes->last() = sum;
  });
  return window.cumulative_amplitudes;
}

}  // namespace bke

}  // namespace blender
