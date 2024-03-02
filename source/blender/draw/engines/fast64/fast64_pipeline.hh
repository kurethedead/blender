/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fast64
 *
 * Shading passes contain drawcalls specific to shading pipelines.
 * They are shared across views.
 * This file is only for shading passes. Other passes are declared in their own module.
 */

#pragma once

#include "BLI_math_bits.h"

#include "DRW_render.h"
#include "draw_shader_shared.h"

namespace blender::fast64 {

class Instance;

/* -------------------------------------------------------------------- */
/** \name World Background Pipeline
 *
 * Render world background values.
 * \{ */

class BackgroundPipeline {
 private:
  Instance &inst_;

  PassSimple world_ps_ = {"World.Background"};

 public:
  BackgroundPipeline(Instance &inst) : inst_(inst){};

  void sync(GPUMaterial *gpumat, float background_opacity);
  void render(View &view);
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Forward Pass
 *
 * Handles alpha blended surfaces and NPR materials (using Closure to RGBA).
 * \{ */

class ForwardPipeline {
 private:
  Instance &inst_;

  PassMain opaque_ps_ = {"Shading"};
  PassMain::Sub *opaque_front_side_ps_ = nullptr;
  PassMain::Sub *opaque_back_side_ps_ = nullptr;
  PassMain::Sub *opaque_double_sided_ps_ = nullptr;

  PassSortable transparent_ps_ = {"Forward.Transparent"};
  float3 camera_forward_;

  bool has_opaque_ = false;
  bool has_transparent_ = false;

 public:
  ForwardPipeline(Instance &inst) : inst_(inst){};

  void sync();

  PassMain::Sub *material_opaque_add(::Material *blender_mat, GPUMaterial *gpumat);
  PassMain::Sub *material_transparent_add(const Object *ob,
                                          ::Material *blender_mat,
                                          GPUMaterial *gpumat);

  void render(View &view, Framebuffer &combined_fb);
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pipelines
 *
 * Contains Shading passes. Shared between views. Objects will subscribe to at least one of them.
 * \{ */

class PipelineModule {
 public:
  BackgroundPipeline background;
  ForwardPipeline forward;

 public:
  PipelineModule(Instance &inst)
      : background(inst),
        forward(inst){};

  void begin_sync()
  {
    forward.sync();
  }

  void end_sync()
  {
  }

  PassMain::Sub *material_add(Object * /*ob*/ /* TODO remove. */,
                              ::Material *blender_mat,
                              GPUMaterial *gpumat)
  {
    return forward.material_opaque_add(blender_mat, gpumat); // transparent happens in material_sync()
  }
};

/** \} */

}  // namespace blender::fast64
