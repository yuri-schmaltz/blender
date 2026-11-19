/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_view.bsl.hh"

namespace gpencil::depth_merge {

struct Resources {
  [[push_constant]] float4x4 gp_model_matrix;
  [[push_constant]] float4 clipping_rect;
  [[push_constant]] bool stroke_order3d;

  [[sampler(0)]] sampler2DDepth depth_buf;
};

[[vertex]]
void vert_main([[resource_table]] const Resources &srt,
               [[resource_table]] const draw::View &views,
               [[vertex_id]] const int vert_id,
               [[position]] float4 &out_pos)
{
  const ViewMatrices view = views.get(0);

  int v = vert_id % 3;
  float x = -1.0f + float((v & 1) << 2);
  float y = -1.0f + float((v & 2) << 1);
  out_pos = view.winmat * (view.viewmat * (srt.gp_model_matrix * float4(x, y, 0.0f, 1.0f)));
}

[[fragment]]
void frag_main([[resource_table]] const Resources &srt,
               [[frag_coord]] const float4 frag_co,
               [[frag_depth(any)]] float &out_depth)
{
  if (any(lessThan(frag_co.xy, srt.clipping_rect.xy)) ||
      any(greaterThan(frag_co.xy, srt.clipping_rect.zw)))
  {
    gpu_discard_fragment();
    return;
  }

  float depth = textureLod(srt.depth_buf, frag_co.xy / float2(textureSize(srt.depth_buf, 0)), 0).r;
  if (srt.stroke_order3d) {
    out_depth = depth;
  }
  else {
    out_depth = (depth != 0.0f) ? frag_co.z : 1.0f;
  }
}

}  // namespace gpencil::depth_merge

PipelineGraphic gpencil_depth_merge(gpencil::depth_merge::vert_main,
                                    gpencil::depth_merge::frag_main);
