/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "fast64_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Common
 * \{ */

// Contains struct definitions
GPU_SHADER_CREATE_INFO(fast64_shared)
    .typedef_source("fast64_shader_shared.hh");

// Contains UBO definitions
GPU_SHADER_CREATE_INFO(fast64_global_ubo)
    .uniform_buf(UNIFORM_BUF_SLOT, "UniformData", "uniform_buf");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Surface Mesh Type
 * \{ */

/* Common interface */
GPU_SHADER_INTERFACE_INFO(fast64_surf_iface, "interp")
    /* World Position. */
    .smooth(Type::VEC3, "pos")
    /* UV */
    .smooth(Type::VEC2, "uv")
    /* UV with no perspective correction*/
    //.no_perspective(Type::VEC2, "uv_no_persp") // Can't mix interpolation modes with Vulkan backend
    /* World Normal. */
    .smooth(Type::VEC3, "nor")
    /* Vertex Color */
    .smooth(Type::VEC4, "vert_col")
    /* Light Color */
    .smooth(Type::VEC4, "vert_light");

// vertex
GPU_SHADER_CREATE_INFO(fast64_geom_mesh)
    .additional_info("fast64_shared")
    .define("MAT_GEOM_MESH")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC2, "uv")
    .vertex_in(2, Type::VEC3, "nor")
    .vertex_in(3, Type::VEC4, "vertex_color")
    .vertex_in(4, Type::VEC4, "vertex_alpha") // separate layer for legacy reasons
    .vertex_source("fast64_geom_mesh_vert.glsl")
    .vertex_out(fast64_surf_iface)
    .additional_info("draw_modelmat_new", "draw_resource_id_varying", "draw_view");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Surface
 * \{ */

//#define image_out(slot, qualifier, format, name) \
//  image(slot, format, qualifier, ImageType::FLOAT_2D, name, Frequency::PASS)
//#define image_array_out(slot, qualifier, format, name) \
//  image(slot, format, qualifier, ImageType::FLOAT_2D_ARRAY, name, Frequency::PASS)
//
//GPU_SHADER_CREATE_INFO(fast64_render_pass_out)
//    .define("MAT_RENDER_PASS_SUPPORT")
//    .additional_info("fast64_global_ubo")
//    .image_array_out(RBUFS_COLOR_SLOT, Qualifier::WRITE, GPU_RGBA16F, "rp_color_img")
//    .image_array_out(RBUFS_VALUE_SLOT, Qualifier::WRITE, GPU_R16F, "rp_value_img");

// fragment
GPU_SHADER_CREATE_INFO(fast64_surf_forward)
    .define("MAT_FORWARD")
    /* Early fragment test is needed for render passes support for forward surfaces. */
    /* NOTE: This removes the possibility of using gl_FragDepth. */
    .early_fragment_test(true)
    .fragment_out(0, Type::VEC4, "out_color")
    .fragment_source("fast64_surf_forward_frag.glsl")
    .additional_info("fast64_global_ubo");

// complete
GPU_SHADER_CREATE_INFO(fast64_mesh_opaque_surf_forward)
    .additional_info("fast64_geom_mesh")
    .additional_info("fast64_surf_forward")
    .do_static_compilation(true);

//GPU_SHADER_CREATE_INFO(fast64_surf_depth)
//    .define("MAT_DEPTH")
//    .fragment_source("fast64_surf_depth_frag.glsl")
//    .additional_info("fast64_global_ubo", "fast64_sampling_data", "fast64_utility_texture");
//
//GPU_SHADER_CREATE_INFO(fast64_surf_world)
//    .push_constant(Type::FLOAT, "world_opacity_fade")
//    .fragment_out(0, Type::VEC4, "out_background")
//    .fragment_source("fast64_surf_world_frag.glsl")
//    .additional_info("fast64_global_ubo",
//                     /* Optionally added depending on the material. */
//                     //  "fast64_render_pass_out",
//                     //  "fast64_cryptomatte_out",
//                     "fast64_utility_texture");

#undef image_out
#undef image_array_out

/** \} */
