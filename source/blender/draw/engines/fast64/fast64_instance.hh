/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fast64
 *
 * An renderer instance that contains all data to render a full frame.
 */

#pragma once

#include "BKE_object.hh"
#include "DEG_depsgraph.hh"
#include "DNA_lightprobe_types.h"
#include "DRW_render.h"

#include "fast64_camera.hh"
#include "fast64_light.hh"
//#include "fast64_lookdev.hh"
#include "fast64_material.hh"
#include "fast64_pipeline.hh"
#include "fast64_renderbuffers.hh"
#include "fast64_shader.hh"
#include "fast64_sync.hh"
#include "fast64_view.hh"
//#include "fast64_world.hh"
#include "fast64_defines.hh"

namespace blender::fast64 {

/* Combines data from several modules to avoid wasting binding slots. */
struct UniformDataModule {
  UniformDataBuf data;

  /* This data buffer is then immutable after this point. */
  void push_update()
  {
    data.push_update();
  }

  template<typename PassType> void bind_resources(PassType &pass)
  {
    pass.bind_ubo(UNIFORM_BUF_SLOT, &data);
  }
};

/**
 * \class Instance
 * \brief A running instance of the engine.
 */
class Instance {

  uint64_t depsgraph_last_update_ = 0;
  bool overlays_enabled_;

 public:
  ShaderModule &shaders;
  SyncModule sync;
  UniformDataModule uniform_data;
  MaterialModule materials;
  PipelineModule pipelines;
  LightModule lights;
  Camera camera;
  RenderBuffers render_buffers;
  MainView main_view;
  //World world;
  //LookdevView lookdev_view;
  //LookdevModule lookdev;

  /** Input data. */
  Depsgraph *depsgraph;
  Manager *manager;
  /** Evaluated IDs. */
  Scene *scene;
  ViewLayer *view_layer;
  /** Camera object if rendering through a camera. nullptr otherwise. */
  Object *camera_eval_object;
  Object *camera_orig_object;
  /** Only available when rendering for final render. */
  const RenderLayer *render_layer;
  RenderEngine *render;
  /** Only available when rendering for viewport. */
  const DRWView *drw_view;
  const View3D *v3d;
  const RegionView3D *rv3d;

  /** True if the grease pencil engine might be running. */
  bool gpencil_engine_enabled;

  /** Info string displayed at the top of the render / viewport. */
  std::string info = "";
  /** Debug mode from debug value. */
  eDebugMode debug_mode = eDebugMode::DEBUG_NONE;

 public:
  Instance()
      : shaders(*ShaderModule::module_get()),
        sync(*this),
        materials(*this),
        pipelines(*this, uniform_data.data.pipeline),
        lights(*this),
        camera(*this, uniform_data.data.camera),
        render_buffers(*this, uniform_data.data.render_pass),
        main_view(*this),
        //capture_view(*this),
        //world(*this),
        //lookdev_view(*this),
        //lookdev(*this),
  ~Instance(){};

  /* Render & Viewport. */
  /* TODO(fclem): Split for clarity. */
  void init(const int2 &output_res,
            const rcti *output_rect,
            const rcti *visible_rect,
            RenderEngine *render,
            Depsgraph *depsgraph,
            Object *camera_object = nullptr,
            const RenderLayer *render_layer = nullptr,
            const DRWView *drw_view = nullptr,
            const View3D *v3d = nullptr,
            const RegionView3D *rv3d = nullptr);

  void view_update();

  void begin_sync();
  void object_sync(Object *ob);
  void end_sync();

  /* Render. */

  void render_sync();
  void render_frame(RenderLayer *render_layer, const char *view_name);
  void store_metadata(RenderResult *render_result);

  /* Viewport. */

  void draw_viewport();
  void draw_viewport_image_render();

  static void update_passes(RenderEngine *engine, Scene *scene, ViewLayer *view_layer);

  bool is_viewport() const
  {
    return render == nullptr && !is_baking();
  }

  bool is_viewport_image_render() const
  {
    return DRW_state_is_viewport_image_render();
  }

  bool overlays_enabled() const
  {
    return overlays_enabled_;
  }

  bool use_scene_lights() const
  {
    return (!v3d) ||
           ((v3d->shading.type == OB_MATERIAL) &&
            (v3d->shading.flag & V3D_SHADING_SCENE_LIGHTS)) ||
           ((v3d->shading.type == OB_RENDER) &&
            (v3d->shading.flag & V3D_SHADING_SCENE_LIGHTS_RENDER));
  }

  /* Light the scene using the selected HDRI in the viewport shading pop-over. */
  bool use_studio_light() const
  {
    return (v3d) && (((v3d->shading.type == OB_MATERIAL) &&
                      ((v3d->shading.flag & V3D_SHADING_SCENE_WORLD) == 0)) ||
                     ((v3d->shading.type == OB_RENDER) &&
                      ((v3d->shading.flag & V3D_SHADING_SCENE_WORLD_RENDER) == 0)));
  }

  bool use_lookdev_overlay() const
  {
    return (v3d) &&
           ((v3d->shading.type == OB_MATERIAL) && (v3d->overlay.flag & V3D_OVERLAY_LOOK_DEV));
  }

  int get_recalc_flags(const ObjectRef &ob_ref)
  {
    auto get_flags = [&](const ObjectRuntimeHandle &runtime) {
      int flags = 0;
      SET_FLAG_FROM_TEST(
          flags, runtime.last_update_transform > depsgraph_last_update_, ID_RECALC_TRANSFORM);
      SET_FLAG_FROM_TEST(
          flags, runtime.last_update_geometry > depsgraph_last_update_, ID_RECALC_GEOMETRY);
      SET_FLAG_FROM_TEST(
          flags, runtime.last_update_shading > depsgraph_last_update_, ID_RECALC_SHADING);
      return flags;
    };

    int flags = get_flags(*ob_ref.object->runtime);
    if (ob_ref.dupli_parent) {
      flags |= get_flags(*ob_ref.dupli_parent->runtime);
    }

    return flags;
  }

 private:
  static void object_sync_render(void *instance_,
                                 Object *ob,
                                 RenderEngine *engine,
                                 Depsgraph *depsgraph);
  void render_sample();

  void mesh_sync(Object *ob, ObjectHandle &ob_handle);

  void update_eval_members();

  void set_time(float time);

  struct DebugScope {
    void *scope;

    DebugScope(void *&scope_p, const char *name)
    {
      if (scope_p == nullptr) {
        scope_p = GPU_debug_capture_scope_create(name);
      }
      scope = scope_p;
      GPU_debug_capture_scope_begin(scope);
    }

    ~DebugScope()
    {
      GPU_debug_capture_scope_end(scope);
    }
  };
};

}  // namespace blender::fast64
