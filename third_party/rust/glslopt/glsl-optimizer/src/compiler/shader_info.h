/*
 * Copyright © 2016 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#ifndef SHADER_INFO_H
#define SHADER_INFO_H

#include "shader_enums.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct spirv_supported_capabilities {
   bool address;
   bool atomic_storage;
   bool demote_to_helper_invocation;
   bool derivative_group;
   bool descriptor_array_dynamic_indexing;
   bool descriptor_array_non_uniform_indexing;
   bool descriptor_indexing;
   bool device_group;
   bool draw_parameters;
   bool float64;
   bool fragment_shader_sample_interlock;
   bool fragment_shader_pixel_interlock;
   bool geometry_streams;
   bool image_ms_array;
   bool image_read_without_format;
   bool image_write_without_format;
   bool int8;
   bool int16;
   bool int64;
   bool int64_atomics;
   bool integer_functions2;
   bool kernel;
   bool min_lod;
   bool multiview;
   bool physical_storage_buffer_address;
   bool post_depth_coverage;
   bool runtime_descriptor_array;
   bool float_controls;
   bool shader_clock;
   bool shader_viewport_index_layer;
   bool stencil_export;
   bool storage_8bit;
   bool storage_16bit;
   bool storage_image_ms;
   bool subgroup_arithmetic;
   bool subgroup_ballot;
   bool subgroup_basic;
   bool subgroup_quad;
   bool subgroup_shuffle;
   bool subgroup_vote;
   bool tessellation;
   bool transform_feedback;
   bool variable_pointers;
   bool vk_memory_model;
   bool vk_memory_model_device_scope;
   bool float16;
   bool amd_fragment_mask;
   bool amd_gcn_shader;
   bool amd_shader_ballot;
   bool amd_trinary_minmax;
   bool amd_image_read_write_lod;
   bool amd_shader_explicit_vertex_parameter;
};

typedef struct shader_info {
   const char *name;

   /* Descriptive name provided by the client; may be NULL */
   const char *label;

   /** The shader stage, such as MESA_SHADER_VERTEX. */
   gl_shader_stage stage:8;

   /** The shader stage in a non SSO linked program that follows this stage,
     * such as MESA_SHADER_FRAGMENT.
     */
   gl_shader_stage next_stage:8;

   /* Number of textures used by this shader */
   uint8_t num_textures;
   /* Number of uniform buffers used by this shader */
   uint8_t num_ubos;
   /* Number of atomic buffers used by this shader */
   uint8_t num_abos;
   /* Number of shader storage buffers (max .driver_location + 1) used by this
    * shader.  In the case of nir_lower_atomics_to_ssbo being used, this will
    * be the number of actual SSBOs in gl_program->info, and the lowered SSBOs
    * and atomic counters in nir_shader->info.
    */
   uint8_t num_ssbos;
   /* Number of images used by this shader */
   uint8_t num_images;
   /* Index of the last MSAA image. */
   int8_t last_msaa_image;

   /* Which inputs are actually read */
   uint64_t inputs_read;
   /* Which outputs are actually written */
   uint64_t outputs_written;
   /* Which outputs are actually read */
   uint64_t outputs_read;
   /* Which system values are actually read */
   uint64_t system_values_read;

   /* Which patch inputs are actually read */
   uint32_t patch_inputs_read;
   /* Which patch outputs are actually written */
   uint32_t patch_outputs_written;
   /* Which patch outputs are read */
   uint32_t patch_outputs_read;

   /* Which inputs are read indirectly (subset of inputs_read) */
   uint64_t inputs_read_indirectly;
   /* Which outputs are read or written indirectly */
   uint64_t outputs_accessed_indirectly;
   /* Which patch inputs are read indirectly (subset of patch_inputs_read) */
   uint64_t patch_inputs_read_indirectly;
   /* Which patch outputs are read or written indirectly */
   uint64_t patch_outputs_accessed_indirectly;

   /** Bitfield of which textures are used */
   uint32_t textures_used;

   /** Bitfield of which textures are used by texelFetch() */
   uint32_t textures_used_by_txf;

   /** Bitfield of which images are used */
   uint32_t images_used;

   /* SPV_KHR_float_controls: execution mode for floating point ops */
   uint16_t float_controls_execution_mode;

   /* The size of the gl_ClipDistance[] array, if declared. */
   uint8_t clip_distance_array_size:4;

   /* The size of the gl_CullDistance[] array, if declared. */
   uint8_t cull_distance_array_size:4;

   /* Whether or not this shader ever uses textureGather() */
   bool uses_texture_gather:1;

   /**
    * True if this shader uses the fddx/fddy opcodes.
    *
    * Note that this does not include the "fine" and "coarse" variants.
    */
   bool uses_fddx_fddy:1;

   /**
    * True if this shader uses 64-bit ALU operations
    */
   bool uses_64bit:1;

   /* Whether the first UBO is the default uniform buffer, i.e. uniforms. */
   bool first_ubo_is_default_ubo:1;

   /* Whether or not separate shader objects were used */
   bool separate_shader:1;

   /** Was this shader linked with any transform feedback varyings? */
   bool has_transform_feedback_varyings:1;

   /* Whether flrp has been lowered. */
   bool flrp_lowered:1;

   /* Whether the shader writes memory, including transform feedback. */
   bool writes_memory:1;

   /* Whether gl_Layer is viewport-relative */
   bool layer_viewport_relative:1;

   union {
      struct {
         /* Which inputs are doubles */
         uint64_t double_inputs;

         /* For AMD-specific driver-internal shaders. It replaces vertex
          * buffer loads with code generating VS inputs from scalar registers.
          *
          * Valid values: SI_VS_BLIT_SGPRS_POS_*
          */
         uint8_t blit_sgprs_amd:4;

         /* True if the shader writes position in window space coordinates pre-transform */
         bool window_space_position:1;
      } vs;

      struct {
         /** The output primitive type (GL enum value) */
         uint16_t output_primitive;

         /** The input primitive type (GL enum value) */
         uint16_t input_primitive;

         /** The maximum number of vertices the geometry shader might write. */
         uint16_t vertices_out;

         /** 1 .. MAX_GEOMETRY_SHADER_INVOCATIONS */
         uint8_t invocations;

         /** The number of vertices recieves per input primitive (max. 6) */
         uint8_t vertices_in:3;

         /** Whether or not this shader uses EndPrimitive */
         bool uses_end_primitive:1;

         /** Whether or not this shader uses non-zero streams */
         bool uses_streams:1;
      } gs;

      struct {
         bool uses_discard:1;
         bool uses_demote:1;

         /**
          * True if this fragment shader requires helper invocations.  This
          * can be caused by the use of ALU derivative ops, texture
          * instructions which do implicit derivatives, and the use of quad
          * subgroup operations.
          */
         bool needs_helper_invocations:1;

         /**
          * Whether any inputs are declared with the "sample" qualifier.
          */
         bool uses_sample_qualifier:1;

         /**
          * Whether early fragment tests are enabled as defined by
          * ARB_shader_image_load_store.
          */
         bool early_fragment_tests:1;

         /**
          * Defined by INTEL_conservative_rasterization.
          */
         bool inner_coverage:1;

         bool post_depth_coverage:1;

         /**
          * \name ARB_fragment_coord_conventions
          * @{
          */
         bool pixel_center_integer:1;
         bool origin_upper_left:1;
         /*@}*/

         bool pixel_interlock_ordered:1;
         bool pixel_interlock_unordered:1;
         bool sample_interlock_ordered:1;
         bool sample_interlock_unordered:1;

         /**
          * Flags whether NIR's base types on the FS color outputs should be
          * ignored.
          *
          * GLSL requires that fragment shader output base types match the
          * render target's base types for the behavior to be defined.  From
          * the GL 4.6 spec:
          *
          *     "If the values written by the fragment shader do not match the
          *      format(s) of the corresponding color buffer(s), the result is
          *      undefined."
          *
          * However, for NIR shaders translated from TGSI, we don't have the
          * output types any more, so the driver will need to do whatever
          * fixups are necessary to handle effectively untyped data being
          * output from the FS.
          */
         bool untyped_color_outputs:1;

         /** gl_FragDepth layout for ARB_conservative_depth. */
         enum gl_frag_depth_layout depth_layout:3;
      } fs;

      struct {
         uint16_t local_size[3];
         uint16_t max_variable_local_size;

         bool local_size_variable:1;
         uint8_t user_data_components_amd:3;

         /*
          * Arrangement of invocations used to calculate derivatives in a compute
          * shader.  From NV_compute_shader_derivatives.
          */
         enum gl_derivative_group derivative_group:2;

         /**
          * Size of shared variables accessed by the compute shader.
          */
         unsigned shared_size;

         /**
          * pointer size is:
          *   AddressingModelLogical:    0    (default)
          *   AddressingModelPhysical32: 32
          *   AddressingModelPhysical64: 64
          */
         unsigned ptr_size;
      } cs;

      /* Applies to both TCS and TES. */
      struct {
         uint16_t primitive_mode; /* GL_TRIANGLES, GL_QUADS or GL_ISOLINES */

         /** The number of vertices in the TCS output patch. */
         uint8_t tcs_vertices_out;
         enum gl_tess_spacing spacing:2;

         /** Is the vertex order counterclockwise? */
         bool ccw:1;
         bool point_mode:1;

         /* Bit mask of TCS per-vertex inputs (VS outputs) that are used
          * with a vertex index that is NOT the invocation id
          */
         uint64_t tcs_cross_invocation_inputs_read;

         /* Bit mask of TCS per-vertex outputs that are used
          * with a vertex index that is NOT the invocation id
          */
         uint64_t tcs_cross_invocation_outputs_read;
      } tess;
   };
} shader_info;

#ifdef __cplusplus
}
#endif

#endif /* SHADER_INFO_H */
