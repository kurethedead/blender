/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Film accumulation utils functions.
 */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(draw_math_geom_lib.glsl)

float film_display_depth_ammend(ivec2 texel, float depth)
{
  /* This effectively offsets the depth of the whole 2x2 region to the lowest value of the region
   * twice. One for X and one for Y direction. */
  /* TODO(fclem): This could be improved as it gives flickering result at depth discontinuity.
   * But this is the quickest stable result I could come with for now. */
#ifdef GPU_FRAGMENT_SHADER
  depth += fwidth(depth);
#endif
  /* Small offset to avoid depth test lessEqual failing because of all the conversions loss. */
  depth += 2.4e-7 * 4.0;
  return clamp(depth, 0.0, 1.0);
}

/** NOTE: out_depth is scene linear depth from the camera origin. */
void film_process_data(ivec2 texel_film, out vec4 out_color, out float out_depth)
{
  out_depth = 0.0;
  out_color = texelFetch(combined_tx, texel_film, 0);
}
