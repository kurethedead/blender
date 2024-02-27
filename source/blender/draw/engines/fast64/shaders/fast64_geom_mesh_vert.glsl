/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(draw_model_lib.glsl)
#pragma BLENDER_REQUIRE(fast64_attributes_lib.glsl)
//#pragma BLENDER_REQUIRE(fast64_surf_lib.glsl) 

void main()
{
//  DRW_VIEW_FROM_RESOURCE_ID;
//#ifdef MAT_SHADOW
//  shadow_viewport_layer_set(int(drw_view_id), int(viewport_index_buf[drw_view_id]));
//#endif

  init_interface();

  interp.pos = drw_point_object_to_world(pos);
  interp.nor = drw_normal_object_to_world(nor);

  interp.uv = uv;
  interp.uv_no_persp = uv;
  
  interp.vert_col = vertex_color;
  interp.vert_col.a = dot(vertex_alpha.rgb, vec3(0.2126729, 0.7151522, 0.0721750)); // see colorToLuminance() in fast64 python

  interp.vert_light = interp.vert_col; // TODO: actually get lights

  // TODO: we need to check if lighting is enabled, then calculate vcol from lights

  // TODO: Do we need init_globals()? is it simple enough to just setup interp from here?
  // init_globals() has some curve related code though...

  //init_globals();
  //attrib_load();

  //interp.P += nodetree_displacement();

//#ifdef MAT_CLIP_PLANE
//  clip_interp.clip_distance = dot(clip_plane.plane, vec4(interp.P, 1.0));
//#endif

  gl_Position = drw_point_world_to_homogenous(interp.pos);
  //gl_Position = drw_point_object_to_homogenous(pos);
}
