/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
//#pragma BLENDER_REQUIRE(fast64_surf_lib.glsl)
#pragma BLENDER_REQUIRE(common_hair_lib.glsl)

void main()
{
  //init_globals();
  
  vec3 lightDir = vec3(0.5, 0.5, 0.5);
  vec3 normal = safe_normalize(interp.nor);
  out_color = interp.vcol * dot(normal, lightDir);
}
