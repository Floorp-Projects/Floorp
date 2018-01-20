// Copyright 2014 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::mem;
use std::mem::size_of;
use std::os::raw::{c_char, c_int, c_void};
use std::ptr;
use std::rc::Rc;
use std::str;
use std::iter::repeat;
use std::ffi::{CString, CStr};
use ffi;

pub use ffi::types::*;
pub use ffi::*;

pub use ffi_gl::Gl as GlFfi;
pub use ffi_gles::Gles2 as GlesFfi;

#[derive(Debug, Eq, PartialEq)]
pub enum GlType {
    Gl,
    Gles,
}

impl Default for GlType {
    #[cfg(any(target_os="android", target_os="ios"))]
    fn default() -> GlType {
        GlType::Gles
    }
    #[cfg(not(any(target_os="android", target_os="ios")))]
    fn default() -> GlType {
        GlType::Gl
    }
}

fn calculate_length(width: GLsizei, height: GLsizei, format: GLenum, pixel_type: GLenum) -> usize {
    let colors = match format {
        ffi::RED => 1,
        ffi::RGB => 3,
        ffi::BGR => 3,

        ffi::RGBA => 4,
        ffi::BGRA => 4,

        ffi::ALPHA => 1,
        ffi::LUMINANCE => 1,
        ffi::DEPTH_COMPONENT => 1,
        _ => panic!("unsupported format for read_pixels: {:?}", format),
    };
    let depth = match pixel_type {
        ffi::UNSIGNED_BYTE => 1,
        ffi::FLOAT=> 4,
        _ => panic!("unsupported pixel_type for read_pixels: {:?}", pixel_type),
    };

    return (width * height * colors * depth) as usize;
}

pub trait Gl {
    fn get_type(&self) -> GlType;
    fn buffer_data_untyped(&self,
                           target: GLenum,
                           size: GLsizeiptr,
                           data: *const GLvoid,
                           usage: GLenum);
    fn buffer_sub_data_untyped(&self,
                               target: GLenum,
                               offset: isize,
                               size: GLsizeiptr,
                               data: *const GLvoid);
    fn tex_buffer(&self, target: GLenum, internal_format: GLenum, buffer: GLuint);
    fn shader_source(&self, shader: GLuint, strings: &[&[u8]]);
    fn read_buffer(&self, mode: GLenum);
    fn read_pixels_into_buffer(&self,
                               x: GLint,
                               y: GLint,
                               width: GLsizei,
                               height: GLsizei,
                               format: GLenum,
                               pixel_type: GLenum,
                               dst_buffer: &mut [u8]);
    fn read_pixels(&self,
                   x: GLint,
                   y: GLint,
                   width: GLsizei,
                   height: GLsizei,
                   format: GLenum,
                   pixel_type: GLenum)
                   -> Vec<u8>;
    fn sample_coverage(&self, value: GLclampf, invert: bool);
    fn polygon_offset(&self, factor: GLfloat, units: GLfloat);
    fn pixel_store_i(&self, name: GLenum, param: GLint);
    fn gen_buffers(&self, n: GLsizei) -> Vec<GLuint>;
    fn gen_renderbuffers(&self, n: GLsizei) -> Vec<GLuint>;
    fn gen_framebuffers(&self, n: GLsizei) -> Vec<GLuint>;
    fn gen_textures(&self, n: GLsizei) -> Vec<GLuint>;
    fn gen_vertex_arrays(&self, n: GLsizei) -> Vec<GLuint>;
    fn gen_queries(&self, n: GLsizei) -> Vec<GLuint>;
    fn begin_query(&self, target: GLenum, id: GLuint);
    fn end_query(&self, target: GLenum);
    fn query_counter(&self, id: GLuint, target: GLenum);
    fn get_query_object_iv(&self, id: GLuint, pname: GLenum) -> i32;
    fn get_query_object_uiv(&self, id: GLuint, pname: GLenum) -> u32;
    fn get_query_object_i64v(&self, id: GLuint, pname: GLenum) -> i64;
    fn get_query_object_ui64v(&self, id: GLuint, pname: GLenum) -> u64;
    fn delete_queries(&self, queries: &[GLuint]);
    fn delete_vertex_arrays(&self, vertex_arrays: &[GLuint]);
    fn delete_buffers(&self, buffers: &[GLuint]);
    fn delete_renderbuffers(&self, renderbuffers: &[GLuint]);
    fn delete_framebuffers(&self, framebuffers: &[GLuint]);
    fn delete_textures(&self, textures: &[GLuint]);
    fn framebuffer_renderbuffer(&self,
                                target: GLenum,
                                attachment: GLenum,
                                renderbuffertarget: GLenum,
                                renderbuffer: GLuint);
    fn renderbuffer_storage(&self,
                            target: GLenum,
                            internalformat: GLenum,
                            width: GLsizei,
                            height: GLsizei);
    fn depth_func(&self, func: GLenum);
    fn active_texture(&self, texture: GLenum);
    fn attach_shader(&self, program: GLuint, shader: GLuint);
    fn bind_attrib_location(&self, program: GLuint, index: GLuint, name: &str);
    fn get_uniform_block_index(&self, program: GLuint, name: &str) -> GLuint;
    fn get_uniform_indices(&self,  program: GLuint, names: &[&str]) -> Vec<GLuint>;
    fn bind_buffer_base(&self, target: GLenum, index: GLuint, buffer: GLuint);
    fn bind_buffer_range(&self, target: GLenum, index: GLuint, buffer: GLuint, offset: GLintptr, size: GLsizeiptr);
    fn uniform_block_binding(&self,
                             program: GLuint,
                             uniform_block_index: GLuint,
                             uniform_block_binding: GLuint);
    fn bind_buffer(&self, target: GLenum, buffer: GLuint);
    fn bind_vertex_array(&self, vao: GLuint);
    fn bind_renderbuffer(&self, target: GLenum, renderbuffer: GLuint);
    fn bind_framebuffer(&self, target: GLenum, framebuffer: GLuint);
    fn bind_texture(&self, target: GLenum, texture: GLuint);
    fn tex_image_2d(&self,
                    target: GLenum,
                    level: GLint,
                    internal_format: GLint,
                    width: GLsizei,
                    height: GLsizei,
                    border: GLint,
                    format: GLenum,
                    ty: GLenum,
                    opt_data: Option<&[u8]>);
    fn compressed_tex_image_2d(&self,
                               target: GLenum,
                               level: GLint,
                               internal_format: GLenum,
                               width: GLsizei,
                               height: GLsizei,
                               border: GLint,
                               data: &[u8]);
    fn compressed_tex_sub_image_2d(&self,
                                   target: GLenum,
                                   level: GLint,
                                   xoffset: GLint,
                                   yoffset: GLint,
                                   width: GLsizei,
                                   height: GLsizei,
                                   format: GLenum,
                                   data: &[u8]);
    fn tex_image_3d(&self,
                    target: GLenum,
                    level: GLint,
                    internal_format: GLint,
                    width: GLsizei,
                    height: GLsizei,
                    depth: GLsizei,
                    border: GLint,
                    format: GLenum,
                    ty: GLenum,
                    opt_data: Option<&[u8]>);
    fn copy_tex_image_2d(&self,
                         target: GLenum,
                         level: GLint,
                         internal_format: GLenum,
                         x: GLint,
                         y: GLint,
                         width: GLsizei,
                         height: GLsizei,
                         border: GLint);
    fn copy_tex_sub_image_2d(&self,
                             target: GLenum,
                             level: GLint,
                             xoffset: GLint,
                             yoffset: GLint,
                             x: GLint,
                             y: GLint,
                             width: GLsizei,
                             height: GLsizei);
    fn copy_tex_sub_image_3d(&self,
                             target: GLenum,
                             level: GLint,
                             xoffset: GLint,
                             yoffset: GLint,
                             zoffset: GLint,
                             x: GLint,
                             y: GLint,
                             width: GLsizei,
                             height: GLsizei);
    fn tex_sub_image_2d(&self,
                        target: GLenum,
                        level: GLint,
                        xoffset: GLint,
                        yoffset: GLint,
                        width: GLsizei,
                        height: GLsizei,
                        format: GLenum,
                        ty: GLenum,
                        data: &[u8]);
    fn tex_sub_image_2d_pbo(&self,
                            target: GLenum,
                            level: GLint,
                            xoffset: GLint,
                            yoffset: GLint,
                            width: GLsizei,
                            height: GLsizei,
                            format: GLenum,
                            ty: GLenum,
                            offset: usize);
    fn tex_sub_image_3d(&self,
                        target: GLenum,
                        level: GLint,
                        xoffset: GLint,
                        yoffset: GLint,
                        zoffset: GLint,
                        width: GLsizei,
                        height: GLsizei,
                        depth: GLsizei,
                        format: GLenum,
                        ty: GLenum,
                        data: &[u8]);
    fn tex_sub_image_3d_pbo(&self,
                            target: GLenum,
                            level: GLint,
                            xoffset: GLint,
                            yoffset: GLint,
                            zoffset: GLint,
                            width: GLsizei,
                            height: GLsizei,
                            depth: GLsizei,
                            format: GLenum,
                            ty: GLenum,
                            offset: usize);
    fn get_integer_v(&self, name: GLenum) -> GLint;
    fn get_integer_64v(&self, name: GLenum) -> GLint64;
    fn get_integer_iv(&self, name: GLenum, index: GLuint) -> GLint;
    fn get_integer_64iv(&self, name: GLenum, index: GLuint) -> GLint64;
    fn get_boolean_v(&self, name: GLenum) -> GLboolean;
    fn get_float_v(&self, name: GLenum) -> GLfloat;
    fn tex_parameter_i(&self, target: GLenum, pname: GLenum, param: GLint);
    fn tex_parameter_f(&self, target: GLenum, pname: GLenum, param: GLfloat);
    fn framebuffer_texture_2d(&self,
                              target: GLenum,
                              attachment: GLenum,
                              textarget: GLenum,
                              texture: GLuint,
                              level: GLint);
    fn framebuffer_texture_layer(&self,
                                 target: GLenum,
                                 attachment: GLenum,
                                 texture: GLuint,
                                 level: GLint,
                                 layer: GLint);
    fn blit_framebuffer(&self,
                        src_x0: GLint,
                        src_y0: GLint,
                        src_x1: GLint,
                        src_y1: GLint,
                        dst_x0: GLint,
                        dst_y0: GLint,
                        dst_x1: GLint,
                        dst_y1: GLint,
                        mask: GLbitfield,
                        filter: GLenum);
    fn vertex_attrib_4f(&self, index: GLuint, x: GLfloat, y: GLfloat, z: GLfloat, w: GLfloat);
    fn vertex_attrib_pointer_f32(&self,
                                 index: GLuint,
                                 size: GLint,
                                 normalized: bool,
                                 stride: GLsizei,
                                 offset: GLuint);
    fn vertex_attrib_pointer(&self,
                             index: GLuint,
                             size: GLint,
                             type_: GLenum,
                             normalized: bool,
                             stride: GLsizei,
                             offset: GLuint);
    fn vertex_attrib_i_pointer(&self,
                               index: GLuint,
                               size: GLint,
                               type_: GLenum,
                               stride: GLsizei,
                               offset: GLuint);
    fn vertex_attrib_divisor(&self, index: GLuint, divisor: GLuint);
    fn viewport(&self, x: GLint, y: GLint, width: GLsizei, height: GLsizei);
    fn scissor(&self, x: GLint, y: GLint, width: GLsizei, height: GLsizei);
    fn line_width(&self, width: GLfloat);
    fn use_program(&self, program: GLuint);
    fn validate_program(&self, program: GLuint);
    fn draw_arrays(&self, mode: GLenum, first: GLint, count: GLsizei);
    fn draw_arrays_instanced(&self,
                             mode: GLenum,
                             first: GLint,
                             count: GLsizei,
                             primcount: GLsizei);
    fn draw_elements(&self,
                     mode: GLenum,
                     count: GLsizei,
                     element_type: GLenum,
                     indices_offset: GLuint);
    fn draw_elements_instanced(&self,
                               mode: GLenum,
                               count: GLsizei,
                               element_type: GLenum,
                               indices_offset: GLuint,
                               primcount: GLsizei);
    fn blend_color(&self, r: f32, g: f32, b: f32, a: f32);
    fn blend_func(&self, sfactor: GLenum, dfactor: GLenum);
    fn blend_func_separate(&self,
                           src_rgb: GLenum,
                           dest_rgb: GLenum,
                           src_alpha: GLenum,
                           dest_alpha: GLenum);
    fn blend_equation(&self, mode: GLenum);
    fn blend_equation_separate(&self, mode_rgb: GLenum, mode_alpha: GLenum);
    fn color_mask(&self, r: bool, g: bool, b: bool, a: bool);
    fn cull_face(&self, mode: GLenum);
    fn front_face(&self, mode: GLenum);
    fn enable(&self, cap: GLenum);
    fn disable(&self, cap: GLenum);
    fn hint(&self, param_name: GLenum, param_val: GLenum);
    fn is_enabled(&self, cap: GLenum) -> GLboolean;
    fn is_shader(&self, shader: GLuint) -> GLboolean;
    fn is_texture(&self, texture: GLenum) -> GLboolean;
    fn is_framebuffer(&self, framebuffer: GLenum) -> GLboolean;
    fn is_renderbuffer(&self, renderbuffer: GLenum) -> GLboolean;
    fn check_frame_buffer_status(&self, target: GLenum) -> GLenum;
    fn enable_vertex_attrib_array(&self, index: GLuint);
    fn disable_vertex_attrib_array(&self, index: GLuint);
    fn uniform_1f(&self, location: GLint, v0: GLfloat);
    fn uniform_1fv(&self, location: GLint, values: &[f32]);
    fn uniform_1i(&self, location: GLint, v0: GLint);
    fn uniform_1iv(&self, location: GLint, values: &[i32]);
    fn uniform_1ui(&self, location: GLint, v0: GLuint);
    fn uniform_2f(&self, location: GLint, v0: GLfloat, v1: GLfloat);
    fn uniform_2fv(&self, location: GLint, values: &[f32]);
    fn uniform_2i(&self, location: GLint, v0: GLint, v1: GLint);
    fn uniform_2iv(&self, location: GLint, values: &[i32]);
    fn uniform_2ui(&self, location: GLint, v0: GLuint, v1: GLuint);
    fn uniform_3f(&self, location: GLint, v0: GLfloat, v1: GLfloat, v2: GLfloat);
    fn uniform_3fv(&self, location: GLint, values: &[f32]);
    fn uniform_3i(&self, location: GLint, v0: GLint, v1: GLint, v2: GLint);
    fn uniform_3iv(&self, location: GLint, values: &[i32]);
    fn uniform_3ui(&self, location: GLint, v0: GLuint, v1: GLuint, v2: GLuint);
    fn uniform_4f(&self, location: GLint, x: GLfloat, y: GLfloat, z: GLfloat, w: GLfloat);
    fn uniform_4i(&self, location: GLint, x: GLint, y: GLint, z: GLint, w: GLint);
    fn uniform_4iv(&self, location: GLint, values: &[i32]);
    fn uniform_4ui(&self, location: GLint, x: GLuint, y: GLuint, z: GLuint, w: GLuint);
    fn uniform_4fv(&self, location: GLint, values: &[f32]);
    fn uniform_matrix_2fv(&self, location: GLint, transpose: bool, value: &[f32]);
    fn uniform_matrix_3fv(&self, location: GLint, transpose: bool, value: &[f32]);
    fn uniform_matrix_4fv(&self, location: GLint, transpose: bool, value: &[f32]);
    fn depth_mask(&self, flag: bool);
    fn depth_range(&self, near: f64, far: f64);
    fn get_active_attrib(&self, program: GLuint, index: GLuint) -> (i32, u32, String);
    fn get_active_uniform(&self, program: GLuint, index: GLuint) -> (i32, u32, String);
    fn get_active_uniforms_iv(&self, program: GLuint, indices: Vec<GLuint>, pname: GLenum) -> Vec<GLint>;
    fn get_active_uniform_block_i(&self, program: GLuint, index: GLuint, pname: GLenum) -> GLint;
    fn get_active_uniform_block_iv(&self, program: GLuint, index: GLuint, pname: GLenum) -> Vec<GLint>;
    fn get_active_uniform_block_name(&self, program: GLuint, index: GLuint) -> String;
    fn get_attrib_location(&self, program: GLuint, name: &str) -> c_int;
    fn get_frag_data_location(&self, program: GLuint, name: &str) -> c_int;
    fn get_uniform_location(&self, program: GLuint, name: &str) -> c_int;
    fn get_program_info_log(&self, program: GLuint) -> String;
    fn get_program_iv(&self, program: GLuint, pname: GLenum) -> GLint;
    fn get_program_binary(&self, program: GLuint) -> (Vec<u8>, GLenum);
    fn program_binary(&self, program: GLuint, format: GLenum, binary: &[u8]);
    fn program_parameter_i(&self, program: GLuint, pname: GLenum, value: GLint);
    fn get_vertex_attrib_iv(&self, index: GLuint, pname: GLenum) -> GLint;
    fn get_vertex_attrib_fv(&self, index: GLuint, pname: GLenum) -> Vec<GLfloat>;
    fn get_vertex_attrib_pointer_v(&self, index: GLuint, pname: GLenum) -> GLsizeiptr;
    fn get_buffer_parameter_iv(&self, target: GLuint, pname: GLenum) -> GLint;
    fn get_shader_info_log(&self, shader: GLuint) -> String;
    fn get_string(&self, which: GLenum) -> String;
    fn get_string_i(&self, which: GLenum, index: GLuint) -> String;
    fn get_shader_iv(&self, shader: GLuint, pname: GLenum) -> GLint;
    fn get_shader_precision_format(&self,
                                   shader_type: GLuint,
                                   precision_type: GLuint)
                                   -> (GLint, GLint, GLint);
    fn compile_shader(&self, shader: GLuint);
    fn create_program(&self) -> GLuint;
    fn delete_program(&self, program: GLuint);
    fn create_shader(&self, shader_type: GLenum) -> GLuint;
    fn delete_shader(&self, shader: GLuint);
    fn detach_shader(&self, program: GLuint, shader: GLuint);
    fn link_program(&self, program: GLuint);
    fn clear_color(&self, r: f32, g: f32, b: f32, a: f32);
    fn clear(&self, buffer_mask: GLbitfield);
    fn clear_depth(&self, depth: f64);
    fn clear_stencil(&self, s: GLint);
    fn flush(&self);
    fn finish(&self);
    fn get_error(&self) -> GLenum;
    fn stencil_mask(&self, mask: GLuint);
    fn stencil_mask_separate(&self, face: GLenum, mask: GLuint);
    fn stencil_func(&self, func: GLenum, ref_: GLint, mask: GLuint);
    fn stencil_func_separate(&self, face: GLenum, func: GLenum, ref_: GLint, mask: GLuint);
    fn stencil_op(&self, sfail: GLenum, dpfail: GLenum, dppass: GLenum);
    fn stencil_op_separate(&self, face: GLenum, sfail: GLenum, dpfail: GLenum, dppass: GLenum);
    fn egl_image_target_texture2d_oes(&self, target: GLenum, image: GLeglImageOES);
    fn generate_mipmap(&self, target: GLenum);
    fn insert_event_marker_ext(&self, message: &str);
    fn push_group_marker_ext(&self, message: &str);
    fn pop_group_marker_ext(&self);
    fn fence_sync(&self, condition: GLenum, flags: GLbitfield) -> GLsync;
    fn client_wait_sync(&self, sync: GLsync, flags: GLbitfield, timeout: GLuint64);
    fn wait_sync(&self, sync: GLsync, flags: GLbitfield, timeout: GLuint64);
    fn delete_sync(&self, sync: GLsync);
    fn texture_range_apple(&self, target: GLenum, data: &[u8]);
    fn gen_fences_apple(&self, n: GLsizei) -> Vec<GLuint>;
    fn delete_fences_apple(&self, fences: &[GLuint]);
    fn set_fence_apple(&self, fence: GLuint);
    fn finish_fence_apple(&self, fence: GLuint);
    fn test_fence_apple(&self, fence: GLuint);

    // GL_ARB_blend_func_extended
    fn bind_frag_data_location_indexed(
        &self,
        program: GLuint,
        color_number: GLuint,
        index: GLuint,
        name: &str,
    );
    fn get_frag_data_index(
        &self,
        program: GLuint,
        name: &str,
    ) -> GLint;
}

#[inline]
pub fn buffer_data<T>(gl_: &Gl, target: GLenum, data: &[T], usage: GLenum) {
    gl_.buffer_data_untyped(target,
                            (data.len() * size_of::<T>()) as GLsizeiptr,
                            data.as_ptr() as *const GLvoid,
                            usage)
}

#[inline]
pub fn buffer_data_raw<T>(gl_: &Gl, target: GLenum, data: &T, usage: GLenum) {
    gl_.buffer_data_untyped(target,
                            size_of::<T>() as GLsizeiptr,
                            data as *const T as *const GLvoid,
                            usage)
}

#[inline]
pub fn buffer_sub_data<T>(gl_: &Gl, target: GLenum, offset: isize, data: &[T]) {
    gl_.buffer_sub_data_untyped(target,
                                offset,
                                (data.len() * size_of::<T>()) as GLsizeiptr,
                                data.as_ptr() as *const GLvoid);
}

include!("gl_fns.rs");
include!("gles_fns.rs");
