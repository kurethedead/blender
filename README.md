<!--
Keep this document short & concise,
linking to external resources instead of including content in-line.
See 'release/text/readme.html' for the end user read-me.
-->

![](https://avatars.githubusercontent.com/u/88108965?s=200 "")
fast64_native
======
This is an attempt to create an n64-style render engine for fast64. This is a work in progress. The following are some notes I've made. The developer pages are also useful for more context.
- https://developer.blender.org/docs/features/
- https://developer.blender.org/docs/handbook/

Intro
---
- DNA (`source\blender\makesdna\`) refers to Blender structs that can be persisted to disk (ex. Material, Mesh)
- RNA (`source/blender/makesrna/intern`) defines how the Blender struct is exposed in the editor(?)


Overview
---
Render engines are located at `source\blender\draw\engines`. The `fast64` engine is loosely based off of `eevee_next` (as of Blender 4.1) with most of the functionality removed in favor of a simple forward renderer. Note that `eevee_next` is a refactor of the `eevee` render engine.

- `fast64_engine.cc/h`: The starting point of a render engine. It contains two structs (`DrawEngineType` and a `RenderEngineType`) which are basically both a list of callbacks for Blender to call at different points of the rendering process. These callbacks mostly call functions implemented in the instance.
- `fast64_instance.cc/hh`: The "meat" of the render engine. This defines an `Instance` struct that stores state and does all of the rendering. The instance is split into multiple module files which each handle a specific part of the rendering process. (i.e. `ShaderModule`, `SyncModule`) It also contains references to the current Blender scene/depsgraph.
- `fast64_sync.cc/hh`: Contains the sync module. In Blender, "sync" refers to the process of updating the render engine state based on the changes that have occurred in the Blender scene/depsgraph/etc. This is where geometry/materials are converted into draw commands.
- `fast64_film.cc/hh`: In eevee, this class is used for handling accumulation of samples for the final image. For fast64 we do not need this, so this file may be removed soon.
- The other files/modules are mostly self-explanatory.

Shaders
---
- `fast64_shader_shared.hh`: Stores structs that are shared between C code and GLSL files, using macros to handle both cases.
- `shaders/infos/*`: Defines shaders and their inputs/outputs/interpolators in C code. The syntax allows for "including" other library files into shaders, along with setting options such as static compilation.
- `shaders/*`: Defines GLSL shader files.

Drawing
---
`PassMain`/`PassMain::Sub` represents a buffer of draw commands. This is what most engines use to build draw commands that are eventually submitted to a draw manager. However, there are also other structs which are unfortunately coupled with nodegraph handling code:
1. `GPUBatch`: Represents primitive data that will be drawn (vertices, triangles)
2. `GPUShader`: Represents a shader program
3. `GPUPass`: Represents a nodegraph compiled `GPUShader`, along with other creation related info
4. `GPUMaterial`: Represents a nodegraph compiled material

Materials
--- 
There are three important material structs:
1. `blender::Material`, aka `::Material`: This is the Blender material data block that the user usually interacts with.
2. `fast64::Material`, aka `Material`: This is a struct that contains the draw commands for the material, usually in the form of `PassMain` or related drawing pass structs.
3. `GPUMaterial`: A representation of a nodegraph compiled shader + uniform data + other info. This struct contains a `uuid` field that acts as a hash when caching compiled materials. This cache is stored as a linked list of materials `ListBase gpumaterial` on the `blender::Material`. An important note is that even though we don't use the nodegraph (only static precompiled shaders), we still reuse this struct since:
    1. A lot of other rendering code requires `GPUMaterial` arguments
    2. We want to reuse the existing `gpumaterial` list for caching materials
    3. We want to store material UBOs here, since they shouldn't be moved around / copied during rendering 
        - Note that `workbench` engine handles this by not using `GPUMaterial` or any function using it, and by storing UBO-related info in one big array that's sent to the GPU at once. However this only works because each material has very little info required to render it.

EEVEE vs. fast64
---
One of the big difference between `eevee_next` and `fast64` is that `fast64` does not do deferred material compilation from nodetrees, and only uses precompiled shaders. However, an issue is that a lot of the material handling code in Blender assumes the use of the nodetree. In order to avoid rewriting a lot of code, we reuse some of this functionality and just ignore the nodetree parts. In particular, these files are modified in `source\blender\gpu\intern\`:
- `gpu_codegen.cc/h`
- `gpu_material.cc`
- `gpu_node_graph.cc/h`
This is done to solve two issues:
1. The material building code auto extracts vertex attributes if they are present on both the mesh and the nodetree (in the form of attribute nodes). In order to handle vertex attribute extractions, we have to expose the function `gpu_node_graph_add_attribute` and manually call it for our desired attributes.
2. In order to extract `GPUBatch`es from the mesh, we are required to pass in an array of `GPUMaterial`s. The `workbench` and `basic` engines both handle this by just passing in a NULL pointer array of `GPUMaterial`s, since they don't require custom vertex attributes. However, we need these for vertex colors, so we must reuse `GPUmaterial`, even if its designed around nodegraphs.

Important Terms
---

- `mesh_cd_ldata_get_from_mesh`: cd = Custom Data
    - ldata (loop) = corner_data
    - pdata (poly) = face_data
    - edata (edge) = edge_data
    - vdata (vert) = vert_data
- ac = active vertex color / au = active UV layer
- orco = undeformed vertex coordinates, normalized to 0..1 range ?

Building
---
Modified CMakeLists:
    - `source\blender\gpu\CMakeLists.txt` (shader infos)
    - `source\blender\draw\CMakeLists.txt` (source files + glsl shaders)

Struct Alignment:
    - 128 bit (16 byte) aligned structs for GLSL
    - 64 bit (8 byte) aligned structs for DNA
