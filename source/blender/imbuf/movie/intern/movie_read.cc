/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2024-2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdio>
#include <sys/types.h>

#include "BLI_build_config.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.hh"
#include "BLI_string_utf8.hh"
#include "BLI_task.hh"
#include "BLI_utildefines.hh"

#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "CLG_log.h"

#include "MOV_read.hh"

#include "IMB_metadata.hh"
#include "movie_proxy_indexer.hh"
#include "movie_read.hh"


namespace blender {



static bool anim_getnew(MovieReader *anim);

void MOV_close(MovieReader *anim)
{
  if (anim == nullptr) {
    return;
  }

  MOV_close_proxies(anim);
  IMB_metadata_free(anim->metadata);

  MEM_delete(anim);
}

void MOV_get_filename(const MovieReader *anim, MutableCStr filename)
{
  BLI_path_split_file_part(anim->filepath, filename);
}

IDProperty *MOV_load_metadata(MovieReader *anim)
{
  if (anim->state == MovieReader::State::Valid) {
  }
  return anim->metadata;
}

static void probe_video_colorspace(MovieReader *anim, char r_colorspace_name[IM_MAX_SPACE])
{
  /* Use default role as fallback (i.e. it is an unknown combination of colorspace and primaries)
   */
  BLI_strncpy_utf8(r_colorspace_name,
                   IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DEFAULT_BYTE),
                   IM_MAX_SPACE);

  if (anim->state == MovieReader::State::Uninitialized) {
    if (!anim_getnew(anim)) {
      return;
    }
  }

}

MovieReader *MOV_open_file(const char *filepath,
                           const ImBufFlags ib_flags,
                           const int streamindex,
                           const bool keep_original_colorspace,
                           ColorManagedColorspaceSettings *colorspace_settings)
{
  MovieReader *anim;

  BLI_assert(!BLI_path_is_rel(filepath));

  anim = MEM_new<MovieReader>("anim struct");
  if (anim != nullptr) {

    STRNCPY(anim->filepath, filepath);
    anim->ib_flags = ib_flags;
    anim->streamindex = streamindex;
    anim->keep_original_colorspace = keep_original_colorspace;

    if (colorspace_settings && colorspace_settings->name[0] != '\0') {
      /* Use colorspace from argument, if provided. */
      STRNCPY_UTF8(anim->colorspace, colorspace_settings->name);
    }
    else {
      /* Try to initialize colorspace from the FFmpeg stream by interpreting color information from
       * it. */
      char file_colorspace[IM_MAX_SPACE];
      probe_video_colorspace(anim, file_colorspace);
      STRNCPY_UTF8(anim->colorspace, file_colorspace);
      if (colorspace_settings) {
        /* Copy the used colorspace into output argument. */
        IMB_colormanagement_colorspace_settings_set(colorspace_settings, file_colorspace);
      }
    }
  }
  return anim;
}

bool MOV_is_initialized_and_valid(const MovieReader *anim)
{
  UNUSED_VARS(anim);

  return false;
}

void MOV_set_multiview_suffix(MovieReader *anim, const char *suffix)
{
  STRNCPY(anim->suffix, suffix);
}


/**
 * Try to initialize the #anim struct.
 * Returns true on success.
 */
static bool anim_getnew(MovieReader *anim)
{
  if (anim == nullptr) {
    /* Nothing to initialize. */
    return false;
  }

  BLI_assert(anim->state == MovieReader::State::Uninitialized);

  anim->state = MovieReader::State::Valid;
  return true;
}

ImBuf *MOV_decode_preview_frame(MovieReader *anim)
{
  ImBuf *ibuf = nullptr;
  int frame_index = 0;

  ibuf = MOV_decode_frame(anim, 0, IMB_PROXY_NONE);
  if (ibuf) {
    IMB_freeImBuf(ibuf);
    frame_index = anim->duration_in_frames / 2;
    ibuf = MOV_decode_frame(anim, frame_index, IMB_PROXY_NONE);

    char value[128];
    IDProperty *metadata = ibuf->metadata_for_write();
    SNPRINTF_UTF8(value, "%i", anim->x);
    IMB_metadata_set_field(metadata, "Thumb::Video::Width", value);
    SNPRINTF_UTF8(value, "%i", anim->y);
    IMB_metadata_set_field(metadata, "Thumb::Video::Height", value);
    SNPRINTF_UTF8(value, "%i", anim->duration_in_frames);
    IMB_metadata_set_field(metadata, "Thumb::Video::Frames", value);

  }
  return ibuf;
}

ImBuf *MOV_decode_frame(MovieReader *anim, int frame_index, IMB_Proxy_Size preview_size)
{
  ImBuf *ibuf = nullptr;
  if (anim == nullptr) {
    return nullptr;
  }

  if (preview_size == IMB_PROXY_NONE) {
    if (anim->state == MovieReader::State::Uninitialized) {
      if (!anim_getnew(anim)) {
        return nullptr;
      }
    }

    if (frame_index < 0) {
      return nullptr;
    }
    if (frame_index >= anim->duration_in_frames) {
      return nullptr;
    }
  }
  else {
    MovieReader *proxy = movie_open_proxy(anim, preview_size);
    if (proxy) {
      return MOV_decode_frame(proxy, frame_index, IMB_PROXY_NONE);
    }
  }


  if (ibuf) {
    ibuf->filepath = anim->filepath;
    ibuf->fileframe = anim->cur_frame_index + 1;
  }
  return ibuf;
}

int MOV_get_video_stream_count(MovieReader *anim)
{
  UNUSED_VARS(anim);
  return 0;
}

int MOV_get_duration_frames(const MovieReader *anim)
{
  return anim->duration_in_frames;
}

double MOV_get_start_offset_seconds(const MovieReader *anim)
{
  return anim->start_offset;
}

float MOV_get_fps(const MovieReader *anim)
{
  if (anim->frs_sec > 0 && anim->frs_sec_base > 0) {
    return float(double(anim->frs_sec) / anim->frs_sec_base);
  }
  return 0.0f;
}

bool MOV_get_fps_num_denom(const MovieReader *anim, short &r_fps_num, float &r_fps_denom)
{
  if (anim->frs_sec > 0 && anim->frs_sec_base > 0) {
    if (anim->frs_sec > SHRT_MAX) {
      /* If numerator is larger than the max short, we need to approximate. */
      r_fps_num = SHRT_MAX;
      r_fps_denom = float(anim->frs_sec_base * double(SHRT_MAX) / double(anim->frs_sec));
    }
    else {
      r_fps_num = anim->frs_sec;
      r_fps_denom = float(anim->frs_sec_base);
    }
    return true;
  }
  return false;
}

int MOV_get_image_width(const MovieReader *anim)
{
  return ELEM(anim->video_rotation, 90, 270) ? anim->y : anim->x;
}

int MOV_get_image_height(const MovieReader *anim)
{
  return ELEM(anim->video_rotation, 90, 270) ? anim->x : anim->y;
}

}  // namespace blender
