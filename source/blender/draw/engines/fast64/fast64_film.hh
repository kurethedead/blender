/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fast64
 *
 * The film class handles accumulation of samples with any distorted camera_type
 * using a pixel filter. Inputs needs to be jittered so that the filter converges to the right
 * result.
 *
 * In viewport, we switch between 2 accumulation mode depending on the scene state.
 * - For static scene, we use a classic weighted accumulation.
 * - For dynamic scene (if an update is detected), we use a more temporally stable accumulation
 *   following the Temporal Anti-Aliasing method (a.k.a. Temporal Super-Sampling). This does
 *   history reprojection and rectification to avoid most of the flickering.
 *
 * The Film module uses the following terms to refer to different spaces/extents:
 *
 * - Display: The full output extent (matches the full viewport or the final image resolution).
 *
 * - Film: The same extent as display, or a subset of it when a Render Region is used.
 *
 * - Render: The extent used internally by the engine for rendering the main views.
 *   Equals to the full display extent + overscan (even when a Render Region is used)
 *   and its resolution can be scaled.
 */

#pragma once

#include "DRW_render.h"

#include "fast64_shader_shared.hh"

#include <sstream>

namespace blender::fast64 {

class Instance;

/* -------------------------------------------------------------------- */
/** \name Film
 * \{ */

class Film {
 public:
  /** For debugging purpose but could be a user option in the future. */
  static constexpr bool use_box_filter = false;

 private:
  Instance &inst_;

  /** Incoming combined buffer with post FX applied (motion blur + depth of field). */
  GPUTexture *combined_final_tx_ = nullptr;
  
  /** Combined "Color" buffer. Double buffered to allow re-projection. */
  SwapChain<Texture, 2> combined_tx_;

  PassSimple draw_ps_ = {"Film.Draw"};

  FilmData &data_;
  int2 display_extent;


 public:
  Film(Instance &inst, FilmData &data) : inst_(inst), data_(data){};
  ~Film(){};

  void init(const int2 &full_extent, const rcti *output_rect);

  void sync();
  void end_sync();

  const FilmData &get_data()
  {
    return data_;
  }

  /** Accumulate the newly rendered sample contained in #RenderBuffers and blit to display. */
  void render(View &view, GPUTexture *combined_final_tx);

  /** Blit to display. No rendered sample needed. */
  void display();

  /** Returns shading views internal resolution. */
  int2 render_extent_get() const
  {
    return data_.render_extent;
  }

  /** Returns final output resolution. */
  int2 display_extent_get() const
  {
    return display_extent;
  }

  float background_opacity_get() const
  {
    return data_.background_opacity;
  }

};

/** \} */

}  // namespace blender::eevee
