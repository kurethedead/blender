/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fast64
 *
 * Shader module that manage shader libraries, deferred compilation,
 * and static shader usage.
 */

#include "GPU_capabilities.h"

#include "gpu_shader_create_info.hh"

#include "fast64_shader.hh"

#include "fast64_shadow.hh"

#include "BLI_assert.h"

namespace blender::fast64 {

/* -------------------------------------------------------------------- */
/** \name Module
 *
 * \{ */

ShaderModule *ShaderModule::g_shader_module = nullptr;

ShaderModule *ShaderModule::module_get()
{
  if (g_shader_module == nullptr) {
    /* TODO(@fclem) thread-safety. */
    g_shader_module = new ShaderModule();
  }
  return g_shader_module;
}

void ShaderModule::module_free()
{
  if (g_shader_module != nullptr) {
    /* TODO(@fclem) thread-safety. */
    delete g_shader_module;
    g_shader_module = nullptr;
  }
}

ShaderModule::ShaderModule()
{
  for (GPUShader *&shader : shaders_) {
    shader = nullptr;
  }
  for (GPUPass *&gpu_pass : gpu_passes_) {
    gpu_pass = nullptr;
  }

#ifndef NDEBUG
  /* Ensure all shader are described. */
  for (auto i : IndexRange(MAX_SHADER_TYPE)) {
    const char *name = static_shader_create_info_name_get(eShaderType(i));
    if (name == nullptr) {
      std::cerr << "FAST64: Missing case for eShaderType(" << i
                << ") in static_shader_create_info_name_get().";
      BLI_assert(0);
    }
    const GPUShaderCreateInfo *create_info = GPU_shader_create_info_get(name);
    BLI_assert_msg(create_info != nullptr, "FAST64: Missing create info for static shader.");
  }
#endif
}

ShaderModule::~ShaderModule()
{
  for (GPUShader *&shader : shaders_) {
    DRW_SHADER_FREE_SAFE(shader);
  }
  // This is handled by GPUMaterial free functions
  //for (GPUPass *&gpu_pass : gpu_passes_) {
  //  
  //}
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Static shaders
 *
 * \{ */

// these correspond to GPU_SHADER_CREATE_INFO() with .do_static_compilation(true)
const char *ShaderModule::static_shader_create_info_name_get(eShaderType shader_type)
{
  switch (shader_type) {
    case F3D_MESH:
      return "fast64_mesh_opaque_surf_forward";
    case FILM_FRAG:
      return "fast64_film_frag";
    case FILM_COMP:
      return "fast64_film_comp";
    default:
      return "";
  }
  return "";
}

GPUShader *ShaderModule::static_shader_get(eShaderType shader_type)
{
  if (shaders_[shader_type] == nullptr) {
    const char *shader_name = static_shader_create_info_name_get(shader_type);

    shaders_[shader_type] = GPU_shader_create_from_info_name(shader_name);

    if (shaders_[shader_type] == nullptr) {
      fprintf(stderr, "FAST64: error: Could not compile static shader \"%s\"\n", shader_name);
    }
    BLI_assert(shaders_[shader_type] != nullptr);
  }
  return shaders_[shader_type];
}

// Version of GPU_generate_pass that uses eShaderType as hash, since we only use pre-compiled shaders
// also removes all codegen releated code
// Note: Its important to use this function to set a GPUPass on a GPUMaterial, in order to handle ref counting
GPUPass *ShaderModule::generate_pass(GPUMaterial *material, GPUNodeGraph *graph, eShaderType shader_type)
{
  if(gpu_passes_[shader_type] == nullptr) {
    GPUPass* pass = (GPUPass *)MEM_callocN(sizeof(GPUPass), "GPUPass");
    pass->shader = static_shader_get(shader_type);
    pass->refcount = 1;
    pass->create_info = nullptr;
    pass->hash = shader_type;
    pass->compiled = true;
    pass->cached = false;
    /* Only flag pass optimization hint if this is the first generated pass for a material.
     * Optimized passes cannot be optimized further, even if the heuristic is still not
     * favorable. */
    pass->should_optimize = false;
    gpu_passes_[shader_type] = pass;
  }
  else {
    GPUPass* pass = gpu_passes_[shader_type];
    pass->refcount++;
  }
  return gpu_passes_[shader_type];
}

// GPUMaterial is structured for handling node graph codegen.
// Its overkill for our needs - workbench itself doesn't use it.
// However, in order to avoid larger rewrites, we reuse this struct anyway.
// Mainly, we need custom data layers for vertex colors, and a place to store UBOs.
// Otherwise, most of it goes unused.
GPUMaterial *ShaderModule::material_shader_get(::Material *blender_mat,
                                               eMaterialGeometry geometry_type,
                                               eShaderType shader_type)
{
  uint64_t shader_uuid = shader_uuid_from_material_type(geometry_type);
  Scene *scene = (Scene *)DEG_get_original_id(&DST.draw_ctx.scene->id);

  /* Search if this material is not already cached. */
  LISTBASE_FOREACH (LinkData *, link, &blender_mat->gpumaterial) {
    GPUMaterial *mat = (GPUMaterial *)link->data;
    if (mat->uuid == shader_uuid) {
      return mat;
    }
  }

  // Create new material
  GPUMaterial *mat = static_cast<GPUMaterial *>(MEM_callocN(sizeof(GPUMaterial), "GPUMaterial")); // calloc zeroes out data
  mat->ma = blender_mat;
  mat->scene = scene;
  mat->uuid = shader_uuid;
  mat->flag = GPU_MATFLAG_UPDATED;
  mat->status = GPU_MAT_CREATED;
  mat->default_mat = nullptr;
  mat->is_volume_shader = false;
  //mat->graph.used_libraries = BLI_gset_new(
  //    BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "GPUNodeGraph.used_libraries");
  mat->refcount = 1;
  STRNCPY(mat->name, blender_mat->id.name);
  //if (is_lookdev) {
  //  mat->flag |= GPU_MATFLAG_LOOKDEV_HACK;
  //}

  if (blender_mat.f3d.is_transparent) {
    GPU_material_flag_set(mat, GPU_MATFLAG_TRANSPARENT);
  }

  GPUNodeGraph *graph = gpu_material_node_graph(mat);
  mat->pass = generate_pass(mat, graph, shader_type);

  // Normally node graph codegen automatically handles retrieving attributes/textures/uniforms
  // However, in order to avoid larger rewrites, we reuse graph structure and manually add our desired attributes
  // so that they can be extracted in GPUBatches, and can be used with material_set()
  
  GPUMaterialAttribute *uv_attr = gpu_node_graph_add_attribute(graph, CD_MTFACE, "UVMap", false, false);
  GPUMaterialAttribute *col_attr = gpu_node_graph_add_attribute(graph, CD_PROP_COLOR, "Col", false, false);
  GPUMaterialAttribute *alpha_attr = gpu_node_graph_add_attribute(graph, CD_PROP_COLOR, "Alpha", false, false);

  // textures
  GPUSamplerState sampler_state = {
    GPU_SAMPLER_FILTERING_DEFAULT,
    GPU_SAMPLER_EXTEND_MODE_EXTEND,
    GPU_SAMPLER_EXTEND_MODE_EXTEND,
    GPU_SAMPLER_CUSTOM_COMPARE,
    GPU_SAMPLER_STATE_TYPE_PARAMETERS};

  //gpu_node_graph_add_texture(graph, tex0_image, tex0_image_user, nullptr, nullptr, false, sampler_state);
  //gpu_node_graph_add_texture(graph, tex1_image, tex1_image_user, nullptr, nullptr, false, sampler_state);

  // uniforms
  //mat->ubo = GPU_uniformbuf_create_ex(sizeof(UniformDataBuf), uniform_data, "f3d_state");

  // Add a linked list node for cached material list
  LinkData *link = static_cast<LinkData *>(MEM_callocN(sizeof(LinkData), "GPUMaterialLink"));
  link->data = mat;
  BLI_addtail(&blender_mat->gpumaterial, link);

  return mat;
}

}  // namespace blender::fast64
