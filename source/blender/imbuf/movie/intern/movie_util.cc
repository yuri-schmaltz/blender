/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include "BLI_map.hh"
#include "BLI_mutex.hh"
#include "BLI_path_utils.hh"
#include "BLI_threads.hh"
#include "BLI_utildefines.hh"

#include "CLG_log.h"
#include "DNA_scene_types.h"

#include "MOV_enums.hh"
#include "MOV_util.hh"

#include "movie_util.hh"
#include <mutex>


namespace blender {


bool MOV_is_movie_file(const char *filepath)
{
  BLI_assert(!BLI_path_is_rel(filepath));

  UNUSED_VARS(filepath);

  return false;
}

void MOV_init()
{
}

void MOV_exit()
{
}

void MOV_validate_output_settings(RenderData *rd, const ImageFormatData *imf)
{
  UNUSED_VARS(rd, imf);
}

eImageFormatDepth MOV_codec_valid_bit_depths(IMB_Ffmpeg_Codec_ID codec_id)
{
  UNUSED_VARS(codec_id);
  return R_IMF_CHAN_DEPTH_8;
}

bool MOV_codec_supports_alpha(IMB_Ffmpeg_Codec_ID codec_id, int ffmpeg_profile)
{
  UNUSED_VARS(codec_id, ffmpeg_profile);
  return false;
}

bool MOV_codec_supports_crf(IMB_Ffmpeg_Codec_ID codec_id)
{
  UNUSED_VARS(codec_id);
  return false;
}

}  // namespace blender
