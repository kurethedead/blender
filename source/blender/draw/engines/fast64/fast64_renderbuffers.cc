/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fast64
 *
 * A film is a full-screen buffer (usually at output extent)
 * that will be able to accumulate sample in any distorted camera_type
 * using a pixel filter.
 *
 * Input needs to be jittered so that the filter converges to the right result.
 */

#include "BLI_rect.h"

#include "GPU_framebuffer.h"
#include "GPU_texture.h"

#include "DRW_render.h"

//#include "fast64_film.hh"
#include "fast64_instance.hh"

namespace blender::fast64 {

void RenderBuffers::sync()
{
}

void RenderBuffers::acquire(int2 extent)
{
  extent_ = extent;

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;

  /* Depth and combined are always needed. */
  depth_tx.ensure_2d(GPU_DEPTH24_STENCIL8, extent, usage);
  /* TODO(fclem): depth_tx should ideally be a texture from pool but we need stencil_view
   * which is currently unsupported by pool textures. */
  // depth_tx.acquire(extent, GPU_DEPTH24_STENCIL8);
  combined_tx.acquire(extent, color_format);
}

void RenderBuffers::release()
{
  /* TODO(fclem): depth_tx should ideally be a texture from pool but we need stencil_view
   * which is currently unsupported by pool textures. */
  // depth_tx.release();
  combined_tx.release();

}

}  // namespace blender::fast64
