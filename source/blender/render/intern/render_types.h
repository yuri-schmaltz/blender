/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 */

#pragma once

/* ------------------------------------------------------------------------- */
/* exposed internal in render module only! */
/* ------------------------------------------------------------------------- */

#include <memory>

#include "DNA_scene_types.h"

#include "BLI_threads.hh"

#include "RE_pipeline.h"

#include "tile_highlight.h"

class GHOST_IContext;

namespace blender {

struct bNodeTree;
struct Depsgraph;
struct Main;
struct Object;
struct RenderDisplay;
struct RenderEngine;
struct ReportList;
struct Scene;

struct BaseRender {
  BaseRender() = default;
  virtual ~BaseRender();

  /* Get class which manages highlight of tiles.
   * Note that it might not exist: for example, viewport render does not support the tile
   * highlight. */
  virtual render::TilesHighlight *get_tile_highlight() = 0;

  /**
   * Executed right before the initialization of the depsgraph, in order to modify some stuff in
   * the viewlayer. The modified ids must be tagged in the depsgraph.
   *
   * If false is returned then rendering is aborted,
   */
  virtual bool prepare_viewlayer(struct ViewLayer *view_layer, struct Depsgraph *depsgraph) = 0;

  /* Result of rendering */
  RenderResult *result = nullptr;

  /* Read/write mutex, all internal code that writes to the `result` must use a
   * write lock, all external code must use a read lock. Internal code is assumed
   * to not conflict with writes, so no lock used for that. */
  ThreadRWMutex resultmutex = BLI_RWLOCK_INITIALIZER;

  /* Render engine. */
  struct RenderEngine *engine = nullptr;

  /* Guard for drawing render result using engine's `draw()` callback. */
  ThreadMutex engine_draw_mutex = BLI_MUTEX_INITIALIZER;

  /** GPU context and callbacks shared with recursive renders displayed together. */
  std::shared_ptr<RenderDisplay> display;
  bool display_shared = false;
};

struct ViewRender : public BaseRender {
  render::TilesHighlight *get_tile_highlight() override
  {
    return nullptr;
  }

  bool prepare_viewlayer(struct ViewLayer * /*view_layer*/,
                         struct Depsgraph * /*depsgraph*/) override
  {
    return true;
  }
};

/** Controls state of render, everything that's read-only during render stage. */
struct Render : public BaseRender {
  Render();
  ~Render() override;

  render::TilesHighlight *get_tile_highlight() override
  {
    return &tile_highlight;
  }

  bool prepare_viewlayer(struct ViewLayer *view_layer, struct Depsgraph *depsgraph) override;

  /* Owner pointer that uniquely identifiers the owner of this scene. */
  const void *owner = nullptr;

  /* state settings */
  short flag = 0;
  bool ok = false;

  /* if render with single-layer option, other rendered layers are stored here */
  RenderResult *pushedresult = nullptr;
  /** A list of #RenderResults, for full-samples. */
  ListBaseT<RenderResult> fullresult = {nullptr, nullptr};
  /* True if result has GPU textures, to quickly skip cache clear. */
  bool result_has_gpu_texture_caches = false;

  /**
   * Window size, display rect, viewplane.
   * \note Buffer width and height with percentage applied
   * without border & crop. convert to long before multiplying together to avoid overflow.
   */
  int winx = 0, winy = 0;
  rcti disprect = {0, 0, 0, 0};  /* part within winx winy */
  rctf viewplane = {0, 0, 0, 0}; /* mapped on winx winy */

  /* final picture width and height (within disprect) */
  int rectx = 0, recty = 0;

  /* Camera transform. Used by Freestyle, Eevee, and other draw manager engines.. */
  float winmat[4][4] = {{0}};

  /* Clipping. */
  float clip_start = 0.0f;
  float clip_end = 0.0f;

  /* main, scene, and its full copy of renderdata and world */
  struct Main *main = nullptr;
  Scene *scene = nullptr;
  RenderData r = {};
  char single_view_layer[MAX_NAME] = "";
  struct Object *camera_override = nullptr;

  render::TilesHighlight tile_highlight;

  /* Minimal dependency graph for the evaluated scene and view layer visibility. */
  struct Depsgraph *pipeline_depsgraph = nullptr;
  Scene *pipeline_scene_eval = nullptr;

  /* Callbacks for the corresponding base class method implementation. */
  bool (*prepare_viewlayer_cb)(void *handle,
                               struct ViewLayer *vl,
                               struct Depsgraph *depsgraph) = nullptr;
  void *prepare_vl_handle = nullptr;

  RenderStats i = {};

  /**
   * Optional report list which may be null (borrowed memory).
   * Callers to rendering functions are responsible for setting can clearing, see: #RE_SetReports.
   */
  struct ReportList *reports = nullptr;

  Vector<MovieWriter *> movie_writers;
  char viewname[MAX_NAME] = "";
};

struct RenderDisplay {
  ~RenderDisplay();

  void free_gpu_context();

  void ensure_system_gpu_context();
  void *ensure_blender_gpu_context();

  void display_update(RenderResult *render_result);
  void current_scene_update(struct Scene *scene);

  void stats_draw(RenderStats *render_stats);
  void progress(float progress);

  void draw_lock();
  void draw_unlock();

  bool test_break();

  /* Callbacks */
  void (*display_update_cb)(void *handle, RenderResult *rr) = nullptr;
  void *duh = nullptr;
  void (*current_scene_update_cb)(void *handle, struct Scene *scene) = nullptr;
  void *suh = nullptr;

  void (*stats_draw_cb)(void *handle, RenderStats *ri) = nullptr;
  void *sdh = nullptr;
  void (*progress_cb)(void *handle, float i) = nullptr;
  void *prh = nullptr;

  void (*draw_lock_cb)(void *handle, bool lock) = nullptr;
  void *dlh = nullptr;
  bool (*test_break_cb)(void *handle) = nullptr;
  void *tbh = nullptr;

  /* GPU contexts.
   * TODO: replace by a whole draw manager. */
  GHOST_IContext *system_gpu_context = nullptr;
  void *blender_gpu_context = nullptr;
};

/* **************** defines ********************* */

/** #R.flag */
#define R_ANIMATION 1 << 0
}  // namespace blender
