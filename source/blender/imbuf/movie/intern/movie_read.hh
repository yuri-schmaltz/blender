/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2024-2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#pragma once

#include <cstdint>

#include "IMB_imbuf_enums.h"

struct AVFormatContext;
struct AVCodecContext;
struct AVCodec;
struct AVFrame;
struct AVPacket;
struct SwsContext;


namespace blender {

struct IDProperty;

struct MovieReader {
  enum class State { Uninitialized, Failed, Valid };
  ImBufFlags ib_flags = ImBufFlags::Zero;
  State state = State::Uninitialized;
  int cur_frame_index = 0; /* index  0 = 1e,  1 = 2e, enz. */
  int duration_in_frames = 0;
  int frs_sec = 0;
  double frs_sec_base = 0.0;
  double start_offset = 0.0;
  int x = 0;
  int y = 0;
  int video_rotation = 0;

  /* for number */
  char filepath[/*FILE_MAX*/ 1024] = {};

  int streamindex = 0;

  bool keep_original_colorspace = false;


  char proxy_dir[768] = {};

  int proxies_tried = 0;

  MovieReader *proxy_anim[IMB_PROXY_MAX_SLOT] = {};

  char colorspace[/*MAX_COLORSPACE_NAME*/ 64] = {};
  /** The maximum name from multi-view. */
  char suffix[/*MAX_NAME*/ 64] = {};

  IDProperty *metadata = nullptr;
};

}  // namespace blender
