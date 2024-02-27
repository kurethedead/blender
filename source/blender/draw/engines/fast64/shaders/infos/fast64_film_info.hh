/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "fast64_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(fast64_film)
    .sampler(0, ImageType::DEPTH_2D, "depth_tx")
    .sampler(1, ImageType::FLOAT_2D, "combined_tx")
    .additional_info("fast64_shared")
    .additional_info("fast64_global_ubo")
    .additional_info("draw_view");

GPU_SHADER_CREATE_INFO(fast64_film_frag)
    .do_static_compilation(true)
    .fragment_out(0, Type::VEC4, "out_color")
    .fragment_source("fast64_film_frag.glsl")
    .additional_info("draw_fullscreen", "fast64_film")
    .depth_write(DepthWrite::ANY);

GPU_SHADER_CREATE_INFO(fast64_film_comp)
    .do_static_compilation(true)
    .local_group_size(FILM_GROUP_SIZE, FILM_GROUP_SIZE)
    .compute_source("fast64_film_comp.glsl")
    .additional_info("fast64_film");