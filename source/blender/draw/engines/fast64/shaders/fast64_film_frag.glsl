/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(fast64_film_lib.glsl)

void main()
{
  ivec2 texel_film = ivec2(gl_FragCoord.xy) - uniform_buf.film.offset;
  float out_depth;

  if (uniform_buf.film.display_only) {
    out_depth = imageLoad(depth_img, texel_film).r;
    out_color = texelFetch(in_combined_tx, texel_film, 0);
  }
  else {
    film_process_data(texel_film, out_color, out_depth);
  }

  gl_FragDepth = drw_depth_view_to_screen(-out_depth);

  gl_FragDepth = film_display_depth_ammend(texel_film, gl_FragDepth);
}
