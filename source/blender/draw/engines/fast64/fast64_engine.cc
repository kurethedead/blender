/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_global.h"
#include "BLI_rect.h"

#include "GPU_capabilities.h"
#include "GPU_framebuffer.h"

#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "DRW_render.hh"

#include "RE_pipeline.h"

#include "fast64_engine.h" /* Own include. */

#include "fast64_instance.hh"

using namespace blender;

struct FAST64_Data {
  DrawEngineType *engine_type;
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  DRWViewportEmptyList *psl;
  DRWViewportEmptyList *stl;
  fast64::Instance *instance;

  char info[GPU_INFO_SIZE];
};

static void fast64_engine_init(void *vedata)
{
  FAST64_Data *ved = reinterpret_cast<FAST64_Data *>(vedata);
  if (ved->instance == nullptr) {
    ved->instance = new fast64::Instance();
  }

  const DRWContextState *ctx_state = DRW_context_state_get();
  Depsgraph *depsgraph = ctx_state->depsgraph;
  Scene *scene = ctx_state->scene;
  View3D *v3d = ctx_state->v3d;
  ARegion *region = ctx_state->region;
  RegionView3D *rv3d = ctx_state->rv3d;

  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  int2 size = int2(GPU_texture_width(dtxl->color), GPU_texture_height(dtxl->color));

  const DRWView *default_view = DRW_view_default_get();

  Object *camera = nullptr;
  /* Get render borders. */
  rcti rect;
  BLI_rcti_init(&rect, 0, size[0], 0, size[1]);
  rcti visible_rect = rect;
  if (v3d) {
    if (rv3d && (rv3d->persp == RV3D_CAMOB)) {
      camera = v3d->camera;
    }

    if (camera) {
      rctf default_border;
      BLI_rctf_init(&default_border, 0.0f, 1.0f, 0.0f, 1.0f);
      bool is_default_border = BLI_rctf_compare(&scene->r.border, &default_border, 0.0f);
      bool use_border = scene->r.mode & R_BORDER;
      if (!is_default_border && use_border) {
        rctf viewborder;
        /* TODO(fclem) Might be better to get it from DRW. */
        ED_view3d_calc_camera_border(scene, depsgraph, region, v3d, rv3d, &viewborder, false);
        float viewborder_sizex = BLI_rctf_size_x(&viewborder);
        float viewborder_sizey = BLI_rctf_size_y(&viewborder);
        rect.xmin = floorf(viewborder.xmin + (scene->r.border.xmin * viewborder_sizex));
        rect.ymin = floorf(viewborder.ymin + (scene->r.border.ymin * viewborder_sizey));
        rect.xmax = floorf(viewborder.xmin + (scene->r.border.xmax * viewborder_sizex));
        rect.ymax = floorf(viewborder.ymin + (scene->r.border.ymax * viewborder_sizey));
      }
    }
    else if (v3d->flag2 & V3D_RENDER_BORDER) {
      rect.xmin = v3d->render_border.xmin * size[0];
      rect.ymin = v3d->render_border.ymin * size[1];
      rect.xmax = v3d->render_border.xmax * size[0];
      rect.ymax = v3d->render_border.ymax * size[1];
    }

    if (DRW_state_is_viewport_image_render()) {
      const float *vp_size = DRW_viewport_size_get();
      visible_rect.xmax = vp_size[0];
      visible_rect.ymax = vp_size[1];
      visible_rect.xmin = visible_rect.ymin = 0;
    }
    else {
      visible_rect = *ED_region_visible_rect(region);
    }
  }

  ved->instance->init(
      size, &rect, &visible_rect, nullptr, depsgraph, camera, nullptr, default_view, v3d, rv3d);
}

static void fast64_draw_scene(void *vedata)
{
  FAST64_Data *ved = reinterpret_cast<FAST64_Data *>(vedata);
  if (DRW_state_is_viewport_image_render()) {
    ved->instance->draw_viewport_image_render();
  }
  else {
    ved->instance->draw_viewport();
  }
  STRNCPY(ved->info, ved->instance->info.c_str());
  /* Reset view for other following engines. */
  DRW_view_set_active(nullptr);
}

static void fast64_cache_init(void *vedata)
{
  reinterpret_cast<FAST64_Data *>(vedata)->instance->begin_sync();
}

static void fast64_cache_populate(void *vedata, Object *object)
{
  reinterpret_cast<FAST64_Data *>(vedata)->instance->object_sync(object);
}

static void fast64_cache_finish(void *vedata)
{
  reinterpret_cast<FAST64_Data *>(vedata)->instance->end_sync();
}

static void fast64_view_update(void *vedata)
{
  if (fast64::Instance *instance = reinterpret_cast<FAST64_Data *>(vedata)->instance) {
    instance->view_update();
  }
}

static void fast64_engine_free()
{
  fast64::ShaderModule::module_free();
}

static void fast64_instance_free(void *instance)
{
  delete reinterpret_cast<fast64::Instance *>(instance);
}

static void fast64_render_to_image(void *vedata,
                                  RenderEngine *engine,
                                  RenderLayer *layer,
                                  const rcti * /*rect*/)
{
  fast64::Instance *instance = new fast64::Instance();

  Render *render = engine->re;
  Depsgraph *depsgraph = DRW_context_state_get()->depsgraph;
  Object *camera_original_ob = RE_GetCamera(engine->re);
  const char *viewname = RE_GetActiveRenderView(engine->re);
  int size[2] = {engine->resolution_x, engine->resolution_y};

  rctf view_rect;
  rcti rect;
  RE_GetViewPlane(render, &view_rect, &rect);
  rcti visible_rect = rect;

  instance->init(size, &rect, &visible_rect, engine, depsgraph, camera_original_ob, layer);
  instance->render_frame(layer, viewname);

  FAST64_Data *ved = static_cast<FAST64_Data *>(vedata);
  delete ved->instance;
  ved->instance = instance;
}

static void fast64_store_metadata(void *vedata, RenderResult *render_result)
{
  FAST64_Data *ved = static_cast<FAST64_Data *>(vedata);
  fast64::Instance *instance = ved->instance;
  instance->store_metadata(render_result);
  delete instance;
  ved->instance = nullptr;
}

static void fast64_render_update_passes(RenderEngine *engine, Scene *scene, ViewLayer *view_layer)
{
  fast64::Instance::update_passes(engine, scene, view_layer);
}

static const DrawEngineDataSize fast64_data_size = DRW_VIEWPORT_DATA_SIZE(FAST64_Data);

extern "C" {

DrawEngineType draw_engine_fast64_type = {
    /*next*/ nullptr,
    /*prev*/ nullptr,
    /*idname*/ N_("FAST64"),
    /*vedata_size*/ &fast64_data_size,
    /*engine_init*/ &fast64_engine_init,
    /*engine_free*/ &fast64_engine_free,
    /*instance_free*/ &fast64_instance_free,
    /*cache_init*/ &fast64_cache_init,
    /*cache_populate*/ &fast64_cache_populate,
    /*cache_finish*/ &fast64_cache_finish,
    /*draw_scene*/ &fast64_draw_scene,
    /*view_update*/ &fast64_view_update,
    /*id_update*/ nullptr,
    /*render_to_image*/ &fast64_render_to_image,
    /*store_metadata*/ &fast64_store_metadata,
};

RenderEngineType DRW_engine_viewport_fast64_type = {
    /*next*/ nullptr,
    /*prev*/ nullptr,
    /*idname*/ "BLENDER_FAST64",
    /*name*/ N_("Fast64"),
    /*flag*/ RE_INTERNAL | RE_USE_PREVIEW | RE_USE_STEREO_VIEWPORT | RE_USE_GPU_CONTEXT,
    /*update*/ nullptr,
    /*render*/ &DRW_render_to_image,
    /*render_frame_finish*/ nullptr,
    /*draw*/ nullptr,
    /*bake*/ nullptr,
    /*view_update*/ nullptr,
    /*view_draw*/ nullptr,
    /*update_script_node*/ nullptr,
    /*update_render_passes*/ &fast64_render_update_passes,
    /*draw_engine*/ &draw_engine_fast64_type,
    /*rna_ext*/
    {
        /*data*/ nullptr,
        /*srna*/ nullptr,
        /*call*/ nullptr,
    },
};
}
