/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.h"
#include "DNA_object_types.h"
#include "DNA_view3d_enums.h"

namespace blender::fast64 {

enum class eGeometryType {
  MESH = 0,
  CURVES,
};
static constexpr int geometry_type_len = static_cast<int>(eGeometryType::CURVES) + 1;

static inline const char *get_name(eGeometryType type)
{
  switch (type) {
    case eGeometryType::MESH:
      return "Mesh";
    case eGeometryType::CURVES:
      return "Curves";
    default:
      BLI_assert_unreachable();
      return "";
  }
}

static inline eGeometryType geometry_type_from_object(Object *ob)
{
  switch (ob->type) {
    case OB_CURVES:
      return eGeometryType::CURVES;
    default:
      return eGeometryType::MESH;
  }
}

enum class ePipelineType {
  OPAQUE = 0,
  TRANSPARENT,
};
static constexpr int pipeline_type_len = static_cast<int>(ePipelineType::TRANSPARENT) + 1;

enum class eLightingType {
  FLAT = 0,
  STUDIO,
  MATCAP,
};
static constexpr int lighting_type_len = static_cast<int>(eLightingType::MATCAP) + 1;

static inline eLightingType lighting_type_from_v3d_lighting(char lighting)
{
  switch (lighting) {
    case V3D_LIGHTING_FLAT:
      return eLightingType::FLAT;
    case V3D_LIGHTING_MATCAP:
      return eLightingType::MATCAP;
    case V3D_LIGHTING_STUDIO:
      return eLightingType::STUDIO;
    default:
      BLI_assert_unreachable();
      return static_cast<eLightingType>(-1);
  }
}
}  // namespace blender::fast64
