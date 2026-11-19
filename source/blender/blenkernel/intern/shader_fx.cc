/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstdio>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.hh"
#include "BLI_string.hh"
#include "BLI_string_utf8.hh"
#include "BLI_string_utils.hh"

#include "BLT_translation.hh"

#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_shader_fx_types.h"

#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_screen.hh"
#include "BKE_shader_fx.hh"

#include "BLO_read_write.hh"

namespace blender {

/* Viewport shader effects are no longer supported: no effect implementation is
 * registered and no UI is exposed for them.
 *
 * The type table below is kept only so that `.blend` files containing shader
 * effects can still be read (and written back) without corrupting the data
 * stream. It carries the DNA struct name and size of every effect type, but
 * none of the behavior callbacks. */
static ShaderFxTypeInfo shader_fx_types[NUM_SHADER_FX_TYPES] = {};

template<typename T> static ShaderFxTypeInfo shader_fx_type_info(const char *name,
                                                            const char *struct_name)
{
  ShaderFxTypeInfo info = {};
  STRNCPY(info.name, name);
  STRNCPY(info.struct_name, struct_name);
  info.struct_size = int(sizeof(T));
  info.type = eShaderFxType_NoneType;
  info.flags = static_cast<ShaderFxTypeFlag>(0);
  return info;
}

static void shader_fx_types_init()
{
  shader_fx_types[eShaderFxType_Blur] =
      shader_fx_type_info<BlurShaderFxData>("Blur", "BlurShaderFxData");
  shader_fx_types[eShaderFxType_Colorize] =
      shader_fx_type_info<ColorizeShaderFxData>("Colorize", "ColorizeShaderFxData");
  shader_fx_types[eShaderFxType_Flip] =
      shader_fx_type_info<FlipShaderFxData>("Flip", "FlipShaderFxData");
  shader_fx_types[eShaderFxType_Glow] =
      shader_fx_type_info<GlowShaderFxData>("Glow", "GlowShaderFxData");
  shader_fx_types[eShaderFxType_Pixel] =
      shader_fx_type_info<PixelShaderFxData>("Pixel", "PixelShaderFxData");
  shader_fx_types[eShaderFxType_Rim] = shader_fx_type_info<RimShaderFxData>("Rim", "RimShaderFxData");
  shader_fx_types[eShaderFxType_Shadow] =
      shader_fx_type_info<ShadowShaderFxData>("Shadow", "ShadowShaderFxData");
  shader_fx_types[eShaderFxType_Swirl] =
      shader_fx_type_info<SwirlShaderFxData>("Swirl", "SwirlShaderFxData");
  shader_fx_types[eShaderFxType_Wave] =
      shader_fx_type_info<WaveShaderFxData>("Wave", "WaveShaderFxData");
}

/* *************************************************** */
/* Methods - Evaluation Loops, etc. */

bool BKE_shaderfx_has_gpencil(const Object *ob)
{
  for (const ShaderFxData &fx : ob->shader_fx) {
    const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(ShaderFxType(fx.type));
    if (fxi != nullptr && fxi->type == eShaderFxType_GpencilType) {
      return true;
    }
  }
  return false;
}

void BKE_shaderfx_init()
{
  shader_fx_types_init();
}

ShaderFxData *BKE_shaderfx_new(ShaderFxType type)
{
  const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(type);
  if (fxi == nullptr) {
    return nullptr;
  }

  ShaderFxData *fx = static_cast<ShaderFxData *>(
      MEM_new_zeroed(fxi->struct_size, fxi->struct_name));

  /* NOTE: this name must be made unique later. */
  STRNCPY_UTF8(fx->name, DATA_(fxi->name));

  fx->type = type;
  fx->mode = eShaderFxMode_Realtime | eShaderFxMode_Render;
  fx->flag = eShaderFxFlag_OverrideLibrary_Local;
  /* Expand only the parent panel by default. */
  fx->ui_expand_flag = UI_PANEL_DATA_EXPAND_ROOT;

  return fx;
}

static void shaderfx_free_data_id_us_cb(void * /*user_data*/,
                                        Object * /*ob*/,
                                        ID **idpoin,
                                        const LibraryForeachIDCallbackFlag cb_flag)
{
  ID *id = *idpoin;
  if (id != nullptr && (cb_flag & IDWALK_CB_USER) != 0) {
    id_us_min(id);
  }
}

void BKE_shaderfx_free_ex(ShaderFxData *fx, const int flag)
{
  UNUSED_VARS(flag);
  const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(ShaderFxType(fx->type));

  if (fxi != nullptr && fxi->free_data) {
    fxi->free_data(fx);
  }
  if (fx->error) {
    MEM_delete(fx->error);
  }

  MEM_delete(fx);
}

void BKE_shaderfx_free(ShaderFxData *fx)
{
  BKE_shaderfx_free_ex(fx, 0);
}

void BKE_shaderfx_unique_name(ListBaseT<ShaderFxData> *shaders, ShaderFxData *fx)
{
  if (shaders && fx) {
    const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(ShaderFxType(fx->type));
    if (fxi == nullptr) {
      return;
    }
    BLI_uniquename(
        shaders, fx, DATA_(fxi->name), '.', offsetof(ShaderFxData, name), sizeof(fx->name));
  }
}

bool BKE_shaderfx_depends_ontime(ShaderFxData *fx)
{
  const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(ShaderFxType(fx->type));

  return fxi != nullptr && fxi->depends_on_time != nullptr && fxi->depends_on_time(fx);
}

const ShaderFxTypeInfo *BKE_shaderfx_get_info(ShaderFxType type)
{
  /* type unsigned, no need to check < 0 */
  if (type < NUM_SHADER_FX_TYPES && type > 0 && shader_fx_types[type].struct_size > 0) {
    return &shader_fx_types[type];
  }

  return nullptr;
}

bool BKE_shaderfx_is_nonlocal_in_liboverride(const Object *ob, const ShaderFxData *shaderfx)
{
  return (ID_IS_OVERRIDE_LIBRARY(ob) &&
          ((shaderfx == nullptr) || (shaderfx->flag & eShaderFxFlag_OverrideLibrary_Local) == 0));
}

void BKE_shaderfxType_panel_id(ShaderFxType type, char *r_idname)
{
  const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(type);
  BLI_string_join(MutableCStr(r_idname, BKE_ST_MAXNAME), SHADERFX_TYPE_PANEL_PREFIX, fxi->name);
}

void BKE_shaderfx_panel_expand(ShaderFxData *fx)
{
  fx->ui_expand_flag |= UI_PANEL_DATA_EXPAND_ROOT;
}

void BKE_shaderfx_copydata_generic(const ShaderFxData *fx_src, ShaderFxData *fx_dst)
{
  const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(ShaderFxType(fx_src->type));

  /* `fx_dst` may have already be fully initialized with some extra allocated data,
   * we need to free it now to avoid a memory leak. */
  if (fxi->free_data) {
    fxi->free_data(fx_dst);
  }

  const size_t data_size = sizeof(ShaderFxData);
  const char *fx_src_data = (reinterpret_cast<const char *>(fx_src)) + data_size;
  char *fx_dst_data = (reinterpret_cast<char *>(fx_dst)) + data_size;
  BLI_assert(data_size <= size_t(fxi->struct_size));
  memcpy(fx_dst_data, fx_src_data, size_t(fxi->struct_size) - data_size);
}

static void shaderfx_copy_data_id_us_cb(void * /*user_data*/,
                                        Object * /*ob*/,
                                        ID **idpoin,
                                        const LibraryForeachIDCallbackFlag cb_flag)
{
  ID *id = *idpoin;
  if (id != nullptr && (cb_flag & IDWALK_CB_USER) != 0) {
    id_us_plus(id);
  }
}

void BKE_shaderfx_copydata_ex(ShaderFxData *fx, ShaderFxData *target, const int flag)
{
  const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(ShaderFxType(fx->type));

  target->mode = fx->mode;
  target->flag = fx->flag;
  target->ui_expand_flag = fx->ui_expand_flag;

  if (fxi == nullptr) {
    return;
  }

  if (fxi->copy_data) {
    fxi->copy_data(fx, target);
  }

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    if (fxi->foreach_ID_link) {
      fxi->foreach_ID_link(target, nullptr, shaderfx_copy_data_id_us_cb, nullptr);
    }
  }
}

void BKE_shaderfx_copydata(ShaderFxData *fx, ShaderFxData *target)
{
  BKE_shaderfx_copydata_ex(fx, target, 0);
}

void BKE_shaderfx_copy(ListBaseT<ShaderFxData> *dst, const ListBaseT<ShaderFxData> *src)
{
  ShaderFxData *fx;
  ShaderFxData *srcfx;

  dst->clear_no_delete();
  BLI_duplicatelist(dst, src);

  for (srcfx = src->first(), fx = dst->first(); srcfx && fx; srcfx = srcfx->next, fx = fx->next) {
    BKE_shaderfx_copydata(srcfx, fx);
  }
}

ShaderFxData *BKE_shaderfx_findby_type(Object *ob, ShaderFxType type)
{
  ShaderFxData *fx = ob->shader_fx.first();

  for (; fx; fx = fx->next) {
    if (fx->type == type) {
      break;
    }
  }

  return fx;
}

void BKE_shaderfx_foreach_ID_link(Object *ob, ShaderFxIDWalkFunc walk, void *user_data)
{
  ShaderFxData *fx = ob->shader_fx.first();

  for (; fx; fx = fx->next) {
    const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(ShaderFxType(fx->type));

    if (fxi != nullptr && fxi->foreach_ID_link) {
      fxi->foreach_ID_link(fx, ob, walk, user_data);
    }
  }
}

ShaderFxData *BKE_shaderfx_findby_name(Object *ob, const char *name)
{
  return static_cast<ShaderFxData *>(
      BLI_findstring(&(ob->shader_fx), name, offsetof(ShaderFxData, name)));
}

void BKE_shaderfx_blend_write(BlendWriter *writer, ListBaseT<ShaderFxData> *fxbase)
{
  if (fxbase == nullptr) {
    return;
  }

  for (ShaderFxData &fx : *fxbase) {
    const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(ShaderFxType(fx.type));
    if (fxi == nullptr) {
      continue;
    }

    writer->write_struct_by_name(fxi->struct_name, &fx);
  }
}

void BKE_shaderfx_blend_read_data(BlendDataReader *reader, ListBaseT<ShaderFxData> *lb, Object *ob)
{
  BLO_read_struct_list_with_type_size<ShaderFxData>(reader, lb, [](const ShaderFxData &fx) {
    const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(ShaderFxType(fx.type));
    return fxi ? size_t(fxi->struct_size) : sizeof(ShaderFxData);
  });

  for (ShaderFxData &fx : lb->items_mutable()) {
    fx.error = nullptr;

    /* Remove if shader disappeared, for upward compatibility. There is no type info for the None
     * type, so this can't be used. */
    if (BKE_shaderfx_get_info(ShaderFxType(fx.type)) == nullptr) {
      BLI_freelinkN(lb, &fx);
      continue;
    }

    /* If linking from a library, clear 'local' library override flag. */
    if (ID_IS_LINKED(ob)) {
      fx.flag &= ~eShaderFxFlag_OverrideLibrary_Local;
    }
  }
}

}  // namespace blender
