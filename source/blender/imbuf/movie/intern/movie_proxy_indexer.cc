/* SPDX-FileCopyrightText: 2011 Peter Schlaile <peter [at] schlaile [dot] de>.
 * SPDX-FileCopyrightText: 2024-2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "DNA_color_types.h"

#include "BLI_endian_switch.hh"
#include "BLI_fileops.hh"
#include "BLI_math_base.hh"
#include "BLI_math_base_c.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.hh"
#include "BLI_string_utils.hh"
#include "BLI_time.hh"
#include "BLI_utildefines.hh"

#include "CLG_log.h"

#include "IMB_colormanagement.hh"

#include "MOV_read.hh"

#include "movie_proxy_indexer.hh"
#include "movie_read.hh"
#include "movie_util.hh"


namespace blender {

static CLG_LogRef LOG = {"video.proxy"};

static const IMB_Proxy_Size proxy_sizes[] = {
    IMB_PROXY_25, IMB_PROXY_50, IMB_PROXY_75, IMB_PROXY_100};
static const float proxy_fac[] = {0.25, 0.50, 0.75, 1.00};

static int proxy_size_to_array_index(IMB_Proxy_Size pr_size)
{
  switch (pr_size) {
    case IMB_PROXY_NONE:
      return -1;
    case IMB_PROXY_25:
      return 0;
    case IMB_PROXY_50:
      return 1;
    case IMB_PROXY_75:
      return 2;
    case IMB_PROXY_100:
      return 3;
    default:
      BLI_assert_msg(0, "Unhandled proxy size enum!");
      return -1;
  }
}

static void get_proxy_dir(const MovieReader *anim, MutableCStr proxy_dir)
{
  if (!anim->proxy_dir[0]) {
    char filename[FILE_MAXFILE];
    char dirname[FILE_MAXDIR];
    BLI_path_split_dir_file(anim->filepath, dirname, filename);
    BLI_path_join(proxy_dir, dirname, "BL_proxy", filename);
  }
  else {
    STRNCPY(proxy_dir, anim->proxy_dir);
  }
}

static bool get_proxy_filepath(const MovieReader *anim,
                               IMB_Proxy_Size preview_size,
                               char *filepath,
                               bool temp)
{
  char proxy_dir[FILE_MAXDIR];
  int i = proxy_size_to_array_index(preview_size);

  BLI_assert(i >= 0);

  char proxy_name[FILE_MAXFILE];
  char stream_suffix[20];
  const char *name = (temp) ? "proxy_%d%s_part.avi" : "proxy_%d%s.avi";

  stream_suffix[0] = 0;

  if (anim->streamindex > 0) {
    SNPRINTF(stream_suffix, "_st%d", anim->streamindex);
  }

  SNPRINTF(proxy_name, name, int(proxy_fac[i] * 100), stream_suffix, anim->suffix);

  get_proxy_dir(anim, proxy_dir);

  if (BLI_path_ncmp(anim->filepath, proxy_dir, FILE_MAXDIR) == 0) {
    return false;
  }

  BLI_path_join(MutableCStr(filepath, FILE_MAXFILE + FILE_MAXDIR), proxy_dir, proxy_name);
  return true;
}

/* ----------------------------------------------------------------------
 * - ffmpeg rebuilder
 * ---------------------------------------------------------------------- */


/* ----------------------------------------------------------------------
 * - public API
 * ---------------------------------------------------------------------- */

MovieProxyBuilder *MOV_proxy_builder_start(MovieReader *anim,
                                           int proxy_sizes_in_use,
                                           int quality,
                                           const bool overwrite,
                                           Set<std::string> *processed_paths,
                                           bool build_only_on_bad_performance)
{
  int proxy_sizes_to_build = proxy_sizes_in_use;

  /* Check which proxies are going to be generated in this session already. */
  if (processed_paths != nullptr) {
    for (int i = 0; i < IMB_PROXY_MAX_SLOT; i++) {
      IMB_Proxy_Size proxy_size = proxy_sizes[i];
      if ((proxy_size & proxy_sizes_to_build) == 0) {
        continue;
      }
      char filepath[FILE_MAX];
      if (!get_proxy_filepath(anim, proxy_size, filepath, false)) {
        return nullptr;
      }
      if (!processed_paths->add(filepath)) {
        proxy_sizes_to_build &= ~int(proxy_size);
      }
    }
  }

  /* When not overwriting existing proxies, skip the ones that already exist. */
  if (!overwrite) {
    int built_proxies = MOV_get_existing_proxies(anim);
    if (built_proxies != 0) {
      for (int i = 0; i < IMB_PROXY_MAX_SLOT; i++) {
        IMB_Proxy_Size proxy_size = proxy_sizes[i];
        if (proxy_size & built_proxies) {
          char filepath[FILE_MAX];
          if (!get_proxy_filepath(anim, proxy_size, filepath, false)) {
            return nullptr;
          }
          CLOG_INFO_NOCHECK(&LOG, "Skipping proxy: %s", filepath);
        }
      }
    }
    proxy_sizes_to_build &= ~built_proxies;
  }

  if (proxy_sizes_to_build == 0) {
    return nullptr;
  }

  MovieProxyBuilder *context = nullptr;
  UNUSED_VARS(build_only_on_bad_performance);

  return context;

  UNUSED_VARS(proxy_sizes_in_use, quality);
}

void MOV_proxy_builder_process(MovieProxyBuilder *context,
                               /* NOLINTNEXTLINE: readability-non-const-parameter. */
                               const bool *stop,
                               /* NOLINTNEXTLINE: readability-non-const-parameter. */
                               bool *do_update,
                               const FunctionRef<void(float progress)> set_progress_fn)
{
  UNUSED_VARS(context, stop, do_update, set_progress_fn);
}

void MOV_proxy_builder_finish(MovieProxyBuilder *context, const bool stop)
{
  /* static defined at top of the file */
  UNUSED_VARS(context, stop, proxy_sizes);
}

void MOV_close_proxies(MovieReader *anim)
{
  if (anim == nullptr) {
    return;
  }

  for (int i = 0; i < IMB_PROXY_MAX_SLOT; i++) {
    if (anim->proxy_anim[i]) {
      MOV_close(anim->proxy_anim[i]);
      anim->proxy_anim[i] = nullptr;
    }
  }

  anim->proxies_tried = 0;
}

void MOV_set_custom_proxy_dir(MovieReader *anim, const char *dir)
{
  if (STREQ(anim->proxy_dir, dir)) {
    return;
  }
  STRNCPY(anim->proxy_dir, dir);

  MOV_close_proxies(anim);
}

MovieReader *movie_open_proxy(MovieReader *anim, IMB_Proxy_Size preview_size)
{
  char filepath[FILE_MAX];
  int i = proxy_size_to_array_index(preview_size);

  if (i < 0) {
    return nullptr;
  }

  if (anim->proxy_anim[i]) {
    return anim->proxy_anim[i];
  }

  if (anim->proxies_tried & preview_size) {
    return nullptr;
  }

  get_proxy_filepath(anim, preview_size, filepath, false);

  /* Proxies are generated in the same color space as animation itself.
   *
   * Also skip any colorspace conversion to the color pipeline design as it helps performance and
   * the image buffers from the proxy builder are not used anywhere else in Blender. */
  ColorManagedColorspaceSettings colorspace_settings;
  IMB_colormanagement_colorspace_settings_set(&colorspace_settings, anim->colorspace);
  anim->proxy_anim[i] = MOV_open_file(
      filepath, anim->ib_flags & ImBufFlags::PoolPixels, 0, true, &colorspace_settings);

  anim->proxies_tried |= preview_size;

  return anim->proxy_anim[i];
}

int MOV_get_existing_proxies(const MovieReader *anim)
{
  const int num_proxy_sizes = IMB_PROXY_MAX_SLOT;
  int existing = IMB_PROXY_NONE;
  for (int i = 0; i < num_proxy_sizes; i++) {
    IMB_Proxy_Size proxy_size = proxy_sizes[i];
    char filepath[FILE_MAX];
    get_proxy_filepath(anim, proxy_size, filepath, false);
    if (BLI_exists(filepath)) {
      existing |= int(proxy_size);
    }
  }
  return existing;
}

}  // namespace blender
