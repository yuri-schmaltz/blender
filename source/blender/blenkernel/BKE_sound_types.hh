/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include <memory>

typedef std::shared_ptr<void> AUD_Device;
typedef std::shared_ptr<void> AUD_Handle;
typedef std::shared_ptr<void> AUD_Sequence;
typedef std::shared_ptr<void> AUD_Sound;
typedef std::shared_ptr<void> AUD_SequenceEntry;

namespace blender::bke {

struct NlaStripRuntime {
  AUD_SequenceEntry speaker_handle;
};

}  // namespace blender::bke
