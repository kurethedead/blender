/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fast64
 *
 * A view is either:
 * - The entire main view.
 * - A fragment of the main view (for panoramic projections).
 * - A shadow map view.
 * - A light-probe view (either planar, cube-map, irradiance grid).
 *
 * A pass is a container for scene data. It is view agnostic but has specific logic depending on
 * its type. Passes are shared between views.
 */

#include "BKE_global.h"
#include "DRW_render.hh"

#include "fast64_instance.hh"

#include "fast64_view.hh"

namespace blender::fast64 {

/* -------------------------------------------------------------------- */
/** \name ShadingView
 * \{ */

void ShadingView::init() {}

void ShadingView::sync()
{
  int2 render_extent = inst_.film.render_extent_get();

  if (false /* inst_.camera.is_panoramic() */) {
    int64_t render_pixel_count = render_extent.x * int64_t(render_extent.y);
    /* Divide pixel count between the 6 views. Rendering to a square target. */
    extent_[0] = extent_[1] = ceilf(sqrtf(1 + (render_pixel_count / 6)));
    /* TODO(@fclem): Clip unused views here. */
    is_enabled_ = true;
  }
  else {
    extent_ = render_extent;
    /* Only enable -Z view. */
    is_enabled_ = (StringRefNull(name_) == "negZ_view");
  }

  if (!is_enabled_) {
    return;
  }

  /* Create views. */
  const CameraData &cam = inst_.camera.data_get();

  float4x4 viewmat, winmat;
  if (false /* inst_.camera.is_panoramic() */) {
    /* TODO(@fclem) Over-scans. */
    /* For now a mandatory 5% over-scan for DoF. */
    float side = cam.clip_near * 1.05f;
    float near = cam.clip_near;
    float far = cam.clip_far;
    winmat = math::projection::perspective(-side, side, -side, side, near, far);
    viewmat = face_matrix_ * cam.viewmat;
  }
  else {
    viewmat = cam.viewmat;
    winmat = cam.winmat;
  }

  main_view_.sync(viewmat, winmat);
}

void ShadingView::render()
{
  if (!is_enabled_) {
    return;
  }

  update_view();

  DRW_stats_group_start(name_);

  /* Needs to be before planar_probes because it needs correct crypto-matte & render-pass buffers
   * to reuse the same deferred shaders. */
  RenderBuffers &rbufs = inst_.render_buffers;
  rbufs.acquire(extent_);

  combined_fb_.ensure(GPU_ATTACHMENT_TEXTURE(rbufs.depth_tx),
                      GPU_ATTACHMENT_TEXTURE(rbufs.combined_tx));

  /* Alpha stores transmittance. So start at 1. */
  float4 clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
  GPU_framebuffer_bind(combined_fb_);
  GPU_framebuffer_clear_color_depth(combined_fb_, clear_color, 1.0f);

  /* TODO(fclem): Move it after the first prepass (and hiz update) once pipeline is stabilized. */
  inst_.lights.set_view(render_view_, extent_);
  inst_.pipelines.background.render(render_view_);

  // inst_.lookdev.render_overlay(view_fb_);

  inst_.pipelines.forward.render(render_view_, combined_fb_);

  //render_transparent_pass(rbufs);

  inst_.lights.debug_draw(render_view_, combined_fb_);

  //GPUTexture *combined_final_tx = render_postfx(rbufs.combined_tx);
  inst_.film.render(jitter_view_, rbufs.combined_tx);

  rbufs.release();
  //postfx_tx_.release();

  DRW_stats_group_end();
}

void ShadingView::update_view()
{
  float4x4 viewmat = main_view_.viewmat();
  float4x4 winmat = main_view_.winmat();
  
  render_view_.sync(viewmat, winmat);
}

/** \} */

}  // namespace blender::fast64
