/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(draw_model_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_matrix_lib.glsl)

/* MAT_GEOM_CURVES */
#pragma BLENDER_REQUIRE(common_hair_lib.glsl) /* TODO rename to curve. */

#define FAST64_ATTRIBUTE_LIB

#if defined(MAT_GEOM_MESH)

/* -------------------------------------------------------------------- */
/** \name Mesh
 *
 * Mesh objects attributes are loaded using vertex input attributes.
 * \{ */

#  ifdef OBINFO_LIB
vec3 attr_load_orco(vec4 orco)
{
#    ifdef GPU_VERTEX_SHADER
  /* We know when there is no orco layer when orco.w is 1.0 because it uses the generic vertex
   * attribute (which is [0,0,0,1]). */
  if (orco.w == 1.0) {
    /* If the object does not have any deformation, the orco layer calculation is done on the fly
     * using the orco_madd factors. */
    return OrcoTexCoFactors[0].xyz + pos * OrcoTexCoFactors[1].xyz;
  }
#    endif
  return orco.xyz * 0.5 + 0.5;
}
#  endif
vec4 attr_load_tangent(vec4 tangent)
{
  tangent.xyz = safe_normalize(drw_normal_object_to_world(tangent.xyz));
  return tangent;
}
vec4 attr_load_vec4(vec4 attr)
{
  return attr;
}
vec3 attr_load_vec3(vec3 attr)
{
  return attr;
}
vec2 attr_load_vec2(vec2 attr)
{
  return attr;
}
float attr_load_float(float attr)
{
  return attr;
}
vec4 attr_load_color(vec4 attr)
{
  return attr;
}
vec3 attr_load_uv(vec3 attr)
{
  return attr;
}

/** \} */

#elif defined(MAT_GEOM_GPENCIL)

/* -------------------------------------------------------------------- */
/** \name Grease Pencil
 *
 * Grease Pencil objects have one uv and one color attribute layer.
 * \{ */

/* Globals to feed the load functions. */
vec2 g_uvs;
vec4 g_color;

#  ifdef OBINFO_LIB
vec3 attr_load_orco(vec4 orco)
{
  vec3 lP = point_world_to_object(interp.P);
  return OrcoTexCoFactors[0].xyz + lP * OrcoTexCoFactors[1].xyz;
}
#  endif
vec4 attr_load_tangent(vec4 tangent)
{
  return vec4(0.0, 0.0, 0.0, 1.0);
}
vec3 attr_load_uv(vec3 dummy)
{
  return vec3(g_uvs, 0.0);
}
vec4 attr_load_color(vec4 dummy)
{
  return g_color;
}
vec4 attr_load_vec4(vec4 attr)
{
  return vec4(0.0);
}
vec3 attr_load_vec3(vec3 attr)
{
  return vec3(0.0);
}
vec2 attr_load_vec2(vec2 attr)
{
  return vec2(0.0);
}
float attr_load_float(float attr)
{
  return 0.0;
}

/** \} */

#elif defined(MAT_GEOM_CURVES) && defined(GPU_VERTEX_SHADER)

/* -------------------------------------------------------------------- */
/** \name Curve
 *
 * Curve objects loads attributes from buffers through sampler buffers.
 * Per attribute scope follows loading order.
 * \{ */

#  ifdef OBINFO_LIB
vec3 attr_load_orco(vec4 orco)
{
  vec3 P = hair_get_strand_pos();
  vec3 lP = transform_point(ModelMatrixInverse, P);
  return OrcoTexCoFactors[0].xyz + lP * OrcoTexCoFactors[1].xyz;
}
#  endif

int g_curves_attr_id = 0;

/* Return the index to use for looking up the attribute value in the sampler
 * based on the attribute scope (point or spline). */
int curves_attribute_element_id()
{
  int id = curve_interp_flat.strand_id;
  if (drw_curves.is_point_attribute[g_curves_attr_id][0] != 0u) {
#  ifdef COMMON_HAIR_LIB
    id = hair_get_base_id();
#  endif
  }

  g_curves_attr_id += 1;
  return id;
}

vec4 attr_load_tangent(samplerBuffer cd_buf)
{
  /* Not supported for the moment. */
  return vec4(0.0, 0.0, 0.0, 1.0);
}
vec3 attr_load_uv(samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, curve_interp_flat.strand_id).rgb;
}
vec4 attr_load_color(samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, curve_interp_flat.strand_id).rgba;
}
vec4 attr_load_vec4(samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, curves_attribute_element_id()).rgba;
}
vec3 attr_load_vec3(samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, curves_attribute_element_id()).rgb;
}
vec2 attr_load_vec2(samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, curves_attribute_element_id()).rg;
}
float attr_load_float(samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, curves_attribute_element_id()).r;
}

/** \} */

#elif defined(MAT_GEOM_WORLD)

/* -------------------------------------------------------------------- */
/** \name World
 *
 * World has no attributes other than orco.
 * \{ */

vec3 attr_load_orco(vec4 orco)
{
  return -g_data.N;
}
vec4 attr_load_tangent(vec4 tangent)
{
  return vec4(0);
}
vec4 attr_load_vec4(vec4 attr)
{
  return vec4(0);
}
vec3 attr_load_vec3(vec3 attr)
{
  return vec3(0);
}
vec2 attr_load_vec2(vec2 attr)
{
  return vec2(0);
}
float attr_load_float(float attr)
{
  return 0.0;
}
vec4 attr_load_color(vec4 attr)
{
  return vec4(0);
}
vec3 attr_load_uv(vec3 attr)
{
  return vec3(0);
}

/** \} */

#endif
