/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edrend
 */

#include <cstdlib>

#include "ED_render.hh"

#include "WM_api.hh"

#include "render_intern.hh" /* own include */

namespace blender {

/***************************** render ***********************************/

void ED_operatortypes_render()
{
  WM_operatortype_append(OBJECT_OT_material_slot_add);
  WM_operatortype_append(OBJECT_OT_material_slot_remove);
  WM_operatortype_append(OBJECT_OT_material_slot_assign);
  WM_operatortype_append(OBJECT_OT_material_slot_select);
  WM_operatortype_append(OBJECT_OT_material_slot_deselect);
  WM_operatortype_append(OBJECT_OT_material_slot_copy);
  WM_operatortype_append(OBJECT_OT_material_slot_move);
  WM_operatortype_append(OBJECT_OT_material_slot_remove_unused);
  WM_operatortype_append(OBJECT_OT_material_slot_remove_all);

  WM_operatortype_append(OBJECT_OT_lightprobe_cache_bake);
  WM_operatortype_append(OBJECT_OT_lightprobe_cache_free);

  WM_operatortype_append(MATERIAL_OT_new);
  WM_operatortype_append(TEXTURE_OT_new);
  WM_operatortype_append(WORLD_OT_new);

  WM_operatortype_append(MATERIAL_OT_copy);
  WM_operatortype_append(MATERIAL_OT_paste);

  WM_operatortype_append(SCENE_OT_view_layer_add);
  WM_operatortype_append(SCENE_OT_view_layer_remove);
  WM_operatortype_append(SCENE_OT_view_layer_add_aov);
  WM_operatortype_append(SCENE_OT_view_layer_remove_aov);
  WM_operatortype_append(SCENE_OT_view_layer_add_lightgroup);
  WM_operatortype_append(SCENE_OT_view_layer_remove_lightgroup);
  WM_operatortype_append(SCENE_OT_view_layer_add_used_lightgroups);
  WM_operatortype_append(SCENE_OT_view_layer_remove_unused_lightgroups);

  WM_operatortype_append(SCENE_OT_render_view_add);
  WM_operatortype_append(SCENE_OT_render_view_remove);


  WM_operatortype_append(TEXTURE_OT_slot_copy);
  WM_operatortype_append(TEXTURE_OT_slot_paste);
  WM_operatortype_append(TEXTURE_OT_slot_move);


  /* `render_internal.cc` */
  WM_operatortype_append(RENDER_OT_view_show);
  WM_operatortype_append(RENDER_OT_render);
  WM_operatortype_append(RENDER_OT_view_cancel);
  WM_operatortype_append(RENDER_OT_shutter_curve_preset);

  /* `render_opengl.cc` */
  WM_operatortype_append(RENDER_OT_opengl);
}

}  // namespace blender
