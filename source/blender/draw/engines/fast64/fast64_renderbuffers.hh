/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fast64
 *
 * Render buffers are textures that are filled during a view rendering.
 * Their content is then added to the accumulation buffers of the film class.
 * They are short lived and can be reused when doing multi view rendering.
 */

#pragma once

#include "DRW_render.h"

#include "fast64_shader_shared.hh"

namespace blender::fast64 {

class Instance;

class RenderBuffers {
 public:
  static constexpr eGPUTextureFormat color_format = GPU_RGBA16F;
  static constexpr eGPUTextureFormat float_format = GPU_R16F;

  Texture depth_tx;
  TextureFromPool combined_tx;

 private:
  Instance &inst_;

  int2 extent_;

 public:
  RenderBuffers(Instance &inst) : inst_(inst){};

  void sync();

  /* Acquires (also ensures) the render buffer before rendering to them. */
  void acquire(int2 extent);
  void release();

  /* Return the size of the allocated render buffers. Undefined if called before `acquire()`. */
  int2 extent_get() const
  {
    return extent_;
  }
};

}  // namespace blender::fast64
