/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fast64
 *
 * Shading passes contain drawcalls specific to shading pipelines.
 * They are to be shared across views.
 * This file is only for shading passes. Other passes are declared in their own module.
 */

#include "GPU_capabilities.h"

#include "fast64_instance.hh"

#include "fast64_pipeline.hh"

#include "draw_common.hh"

namespace blender::fast64 {

/* -------------------------------------------------------------------- */
/** \name World Pipeline
 *
 * Used to draw background.
 * \{ */

void BackgroundPipeline::sync(GPUMaterial *gpumat, const float background_opacity)
{
  Manager &manager = *inst_.manager;

  world_ps_.init();
  world_ps_.state_set(DRW_STATE_WRITE_COLOR);
  world_ps_.material_set(manager, gpumat);
  world_ps_.push_constant("world_opacity_fade", background_opacity);

  /* Required by validation layers. */
  world_ps_.bind_resources(inst_.uniform_data);
  world_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  /* To allow opaque pass rendering over it. */
  world_ps_.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
}

void BackgroundPipeline::render(View &view)
{
  inst_.manager->submit(world_ps_, view);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Forward Pass
 *
 * NPR materials (using Closure to RGBA) or material using ALPHA_BLEND.
 * \{ */

void ForwardPipeline::sync()
{
  camera_forward_ = inst_.camera.forward();
  has_opaque_ = false;
  has_transparent_ = false;

  DRWState state_depth_only = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS;
  DRWState state_depth_color = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS |
                               DRW_STATE_WRITE_COLOR;
  
  {
    opaque_ps_.init();

    {
      /* Common resources. */

      opaque_ps_.bind_resources(inst_.uniform_data);
      opaque_ps_.bind_resources(inst_.lights);
    }

    opaque_front_side_ps_ = &opaque_ps_.sub("FrontSide");
    opaque_front_side_ps_->state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL |
                                       DRW_STATE_CULL_BACK);
    
    opaque_back_side_ps_ = &opaque_ps_.sub("BackSide");
    opaque_back_side_ps_->state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL |
                                       DRW_STATE_CULL_FRONT);

    opaque_double_sided_ps_ = &opaque_ps_.sub("DoubleSided");
    opaque_double_sided_ps_->state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL);
  }
  {
    transparent_ps_.init();
    /* Workaround limitation of PassSortable. Use dummy pass that will be sorted first in all
     * circumstances. */
    PassMain::Sub &sub = transparent_ps_.sub("ResourceBind", -FLT_MAX);

    /* Common resources. */

    /* Textures. */

    sub.bind_resources(inst_.uniform_data);
    sub.bind_resources(inst_.lights);
  }
}

PassMain::Sub *ForwardPipeline::material_opaque_add(::Material *blender_mat, GPUMaterial *gpumat)
{
  BLI_assert_msg(GPU_material_flag_get(gpumat, GPU_MATFLAG_TRANSPARENT) == false,
                 "Forward Transparent should be registered directly without calling "
                 "PipelineModule::material_add()");

  // TODO: don't read from blender_mat->blend_flag, read from f3d property
  PassMain::Sub *pass = blender_mat->f3d.g_cull_back && blender_mat->f3d.g_cull_back ? opaque_double_sided_ps_ :
                          (blender_mat->f3d.g_cull_front ? opaque_back_side_ps_ : opaque_front_side_ps_);
  has_opaque_ = true;
  return &pass->sub(GPU_material_get_name(gpumat));
}

PassMain::Sub *ForwardPipeline::material_transparent_add(const Object *ob,
                                                         ::Material *blender_mat,
                                                         GPUMaterial *gpumat)
{
  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM | DRW_STATE_DEPTH_LESS_EQUAL;

  // TODO: Don't read from blender_mat->blend_flag, read from f3d property
  if (blender_mat->f3d.g_cull_back) {
    state |= DRW_STATE_CULL_BACK;
  }
  if(blender_mat->f3d.g_cull_front) {
    state |= DRW_STATE_CULL_FRONT;
  }
  has_transparent_ = true;

  // TODO: Don't sort, since that technically doesn't happen on n64? At least not in sm64?
  float sorting_value = math::dot(float3(ob->object_to_world[3]), camera_forward_);
  PassMain::Sub *pass = &transparent_ps_.sub(GPU_material_get_name(gpumat), sorting_value);
  pass->state_set(state);
  pass->material_set(*inst_.manager, gpumat);
  return pass;
}

void ForwardPipeline::render(View &view, Framebuffer &combined_fb)
{
  if (!has_transparent_ && !has_opaque_) {
    return;
  }

  DRW_stats_group_start("Forward.Opaque");

  if (has_opaque_) {
    combined_fb.bind();
    inst_.manager->submit(opaque_ps_, view);
  }

  DRW_stats_group_end();

  if (has_transparent_) {
    combined_fb.bind();
    inst_.manager->submit(transparent_ps_, view);
  }
}

/** \} */

}  // namespace blender::fast64
