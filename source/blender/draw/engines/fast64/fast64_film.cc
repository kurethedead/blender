/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * A film is a buffer (usually at display extent)
 * that will be able to accumulate sample in any distorted camera_type
 * using a pixel filter.
 *
 * Input needs to be jittered so that the filter converges to the right result.
 */

#include "BLI_hash.h"
#include "BLI_rect.h"

#include "GPU_framebuffer.h"
#include "GPU_texture.h"

#include "DRW_render.hh"
#include "RE_pipeline.h"

#include "eevee_film.hh"
#include "eevee_instance.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name FilmData
 * \{ */

inline bool operator==(const FilmData &a, const FilmData &b)
{
  return (a.extent == b.extent) && (a.offset == b.offset) &&
         (a.render_extent == b.render_extent) && (a.render_offset == b.render_offset) &&
         (a.filter_radius == b.filter_radius) && (a.scaling_factor == b.scaling_factor) &&
         (a.background_opacity == b.background_opacity);
}

inline bool operator!=(const FilmData &a, const FilmData &b)
{
  return !(a == b);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Film
 * \{ */


void Film::init(const int2 &extent, const rcti *output_rect)
{
  //Sampling &sampling = inst_.sampling;
  Scene &scene = *inst_.scene;
  SceneFAST64 &scene_fast64 = scene.fast64;
  
  {
    rcti fallback_rect;
    if (BLI_rcti_is_empty(output_rect)) {
      BLI_rcti_init(&fallback_rect, 0, extent[0], 0, extent[1]);
      output_rect = &fallback_rect;
    }

    display_extent = extent;

    data_.extent = int2(BLI_rcti_size_x(output_rect), BLI_rcti_size_y(output_rect));
    data_.offset = int2(output_rect->xmin, output_rect->ymin);
    data_.extent_inv = 1.0f / float2(data_.extent);
    /* TODO(fclem): parameter hidden in experimental.
     * We need to figure out LOD bias first in order to preserve texture crispiness. */
    data_.scaling_factor = 1;
    data_.render_extent = math::divide_ceil(extent, int2(data_.scaling_factor));
    data_.render_offset = data_.offset;

    if (inst_.camera.overscan() != 0.0f) {
      int2 overscan = int2(inst_.camera.overscan() * math::max(UNPACK2(data_.render_extent)));
      data_.render_extent += overscan * 2;
      data_.render_offset += overscan;
    }

    /* Disable filtering if sample count is 1. */
    //data_.filter_radius = (sampling.sample_count() == 1) ? 0.0f :
    //                                                       clamp_f(scene.r.gauss, 0.0f, 100.0f);

    data_.background_opacity = (scene.r.alphamode == R_ALPHAPREMUL) ? 0.0f : 1.0f;
    if (inst_.is_viewport() && false /* TODO(fclem): StudioLight */) {
      data_.background_opacity = inst_.v3d->shading.studiolight_background;
    }
  }
  {
    /* Combined is in a separate buffer. */
    //data_.combined_id = 0;
    /* Depth is in a separate buffer. */
    //data_.depth_id = 0;
  }
  {
    eGPUTextureFormat color_format = GPU_RGBA16F;
    eGPUTextureFormat depth_format = GPU_R32F;

    int reset = 0;
    reset += depth_tx_.ensure_2d(depth_format, data_.extent);
    reset += combined_tx_.current().ensure_2d(color_format, data_.extent);
    reset += combined_tx_.next().ensure_2d(color_format, data_.extent);

    if (reset > 0) {

      /* Avoid NaN in uninitialized texture memory making history blending dangerous. */
      combined_tx_.current().clear(float4(0.0f));
      depth_tx_.clear(float4(0.0f));
    }
  }
}

void Film::sync()
{
  /* We use a fragment shader for viewport because we need to output the depth. */
  bool use_compute = (inst_.is_viewport() == false);

  eShaderType shader = use_compute ? FILM_COMP : FILM_FRAG;

  /* TODO(fclem): Shader variation for panoramic & scaled resolution. */

  RenderBuffers &rbuffers = inst_.render_buffers;
  GPUSamplerState filter = {GPU_SAMPLER_FILTERING_LINEAR};

  draw_ps_.init();
  draw_ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
  draw_ps_.shader_set(inst_.shaders.static_shader_get(shader));
  draw_ps_.bind_resources(inst_.uniform_data);
  draw_ps_.bind_texture("depth_tx", &rbuffers.depth_tx);
  draw_ps_.bind_texture("combined_tx", &combined_final_tx_);

  /* Sync with rendering passes. */
  draw_ps_.barrier(GPU_BARRIER_TEXTURE_FETCH | GPU_BARRIER_SHADER_IMAGE_ACCESS);
  if (use_compute) {
    draw_ps_.dispatch(int3(math::divide_ceil(data_.extent, int2(FILM_GROUP_SIZE)), 1));
  }
  else {
    draw_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
}

void Film::end_sync()
{
  //data_.use_reprojection = inst_.sampling.interactive_mode();
//
  ///* Just bypass the reprojection and reset the accumulation. */
  //if (inst_.is_viewport() && force_disable_reprojection_ && inst_.sampling.is_reset()) {
  //  data_.use_reprojection = false;
  //  data_.use_history = false;
  //}
//
  //aovs_info.push_update();
//
  //sync_mist();
}

void Film::render(View &view, GPUTexture *combined_final_tx)
{
  if (inst_.is_viewport()) {
    DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
    DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
    GPU_framebuffer_bind(dfbl->default_fb);
    /* Clear when using render borders. */
    if (data_.extent != int2(GPU_texture_width(dtxl->color), GPU_texture_height(dtxl->color))) {
      float4 clear_color = {0.0f, 0.0f, 0.0f, 0.0f};
      GPU_framebuffer_clear_color(dfbl->default_fb, clear_color);
    }
    GPU_framebuffer_viewport_set(dfbl->default_fb, UNPACK2(data_.offset), UNPACK2(data_.extent));
  }

  combined_final_tx_ = combined_final_tx;

  data_.display_only = false;
  inst_.uniform_data.push_update();

  inst_.manager->submit(draw_ps_, view);

  combined_tx_.swap();
  //weight_tx_.swap();
}

void Film::display()
{
  BLI_assert(inst_.is_viewport());

  /* Acquire dummy render buffers for correct binding. They will not be used. */
  inst_.render_buffers.acquire(int2(1));

  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
  GPU_framebuffer_bind(dfbl->default_fb);
  GPU_framebuffer_viewport_set(dfbl->default_fb, UNPACK2(data_.offset), UNPACK2(data_.extent));

  combined_final_tx_ = inst_.render_buffers.combined_tx;

  data_.display_only = true;
  inst_.uniform_data.push_update();

  draw::View drw_view("MainView", DRW_view_default_get());

  DRW_manager_get()->submit(draw_ps_, drw_view);

  inst_.render_buffers.release();

  /* IMPORTANT: Do not swap! No accumulation has happened. */
}

/** \} */

}  // namespace blender::eevee
