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
use std::str::{self};
use std::iter::repeat;
use std::ffi::{CString, CStr};
use ffi;

pub use ffi::types::*;
pub use ffi::*;

#[inline]
pub fn buffer_data<T>(target: GLenum, data: &[T], usage: GLenum) {
    unsafe {
        ffi::BufferData(target,
                       (data.len() * size_of::<T>()) as GLsizeiptr,
                       data.as_ptr() as *const GLvoid,
                       usage);
    }
}

#[inline]
pub fn buffer_data_raw<T>(target: GLenum, data: &T, usage: GLenum) {
    unsafe {
        ffi::BufferData(target,
                        size_of::<T>() as GLsizeiptr,
                        data as *const T as *const GLvoid,
                        usage);
    }
}

#[inline]
pub fn buffer_sub_data<T>(target: GLenum, offset: isize, data: &[T]) {
    unsafe {
        ffi::BufferSubData(target,
                           offset,
                           (data.len() * size_of::<T>()) as GLsizeiptr,
                           data.as_ptr() as *const GLvoid);
    }
}

pub fn shader_source(shader: GLuint, strings: &[&[u8]]) {
    let pointers: Vec<*const u8> = strings.iter().map(|string| (*string).as_ptr()).collect();
    let lengths: Vec<GLint> = strings.iter().map(|string| string.len() as GLint).collect();
    unsafe {
        ffi::ShaderSource(shader, pointers.len() as GLsizei,
                         pointers.as_ptr() as *const *const GLchar, lengths.as_ptr());
    }
    drop(lengths);
    drop(pointers);
}

#[cfg(not(target_os="android"))]
pub fn read_buffer(mode: GLenum) {
    unsafe {
        ffi::ReadBuffer(mode);
    }
}

fn calculate_length(width: GLsizei, height: GLsizei, format: GLenum, pixel_type: GLenum) -> usize {
    let colors = match format {
        ffi::RGB => 3,
#[cfg(not(target_os="android"))]
        ffi::BGR => 3,

        ffi::RGBA => 4,
#[cfg(not(target_os="android"))]
        ffi::BGRA => 4,

        ffi::ALPHA => 1,
#[cfg(target_os="android")]
        ffi::LUMINANCE => 1,
        _ => panic!("unsupported format for read_pixels"),
    };
    let depth = match pixel_type {
        ffi::UNSIGNED_BYTE => 1,
        _ => panic!("unsupported pixel_type for read_pixels"),
    };

    return (width * height * colors * depth) as usize;
}

pub fn read_pixels_into_buffer(x: GLint, y: GLint, width: GLsizei, height: GLsizei,
                               format: GLenum, pixel_type: GLenum, dst_buffer: &mut [u8]) {
    // Assumes that the user properly allocated the size for dst_buffer.
    assert!(calculate_length(width, height, format, pixel_type) == dst_buffer.len());

    unsafe {
        // We don't want any alignment padding on pixel rows.
        ffi::PixelStorei(ffi::PACK_ALIGNMENT, 1);
        ffi::ReadPixels(x, y, width, height, format, pixel_type, dst_buffer.as_mut_ptr() as *mut c_void);
    }
}

pub fn read_pixels(x: GLint, y: GLint, width: GLsizei, height: GLsizei, format: GLenum, pixel_type: GLenum) -> Vec<u8> {
    let len = calculate_length(width, height, format, pixel_type);
    let mut pixels: Vec<u8> = Vec::new();
    pixels.reserve(len);
    unsafe { pixels.set_len(len); }

    read_pixels_into_buffer(x, y, width, height, format, pixel_type, pixels.as_mut_slice());

    pixels
}

#[inline]
pub fn sample_coverage(value: GLclampf, invert: bool) {
    unsafe {
        ffi::SampleCoverage(value, invert as GLboolean);
    }
}

#[inline]
pub fn polygon_offset(factor: GLfloat, units: GLfloat) {
    unsafe {
        ffi::PolygonOffset(factor, units);
    }
}

#[inline]
pub fn pixel_store_i(name: GLenum, param: GLint) {
    unsafe {
        ffi::PixelStorei(name, param);
    }
}

#[inline]
pub fn gen_buffers(n: GLsizei) -> Vec<GLuint> {
    unsafe {
        let mut result: Vec<_> = repeat(0 as GLuint).take(n as usize).collect();
        ffi::GenBuffers(n, result.as_mut_ptr());
        return result;
    }
}

#[inline]
pub fn gen_renderbuffers(n: GLsizei) -> Vec<GLuint> {
    unsafe {
        let mut result: Vec<_> = repeat(0 as GLuint).take(n as usize).collect();
        ffi::GenRenderbuffers(n, result.as_mut_ptr());
        return result;
    }
}

#[inline]
pub fn gen_framebuffers(n: GLsizei) -> Vec<GLuint> {
    unsafe {
        let mut result: Vec<_> = repeat(0 as GLuint).take(n as usize).collect();
        ffi::GenFramebuffers(n, result.as_mut_ptr());
        return result;
    }
}

#[inline]
pub fn gen_textures(n: GLsizei) -> Vec<GLuint> {
    unsafe {
        let mut result: Vec<_> = repeat(0 as GLuint).take(n as usize).collect();
        ffi::GenTextures(n, result.as_mut_ptr());
        return result;
    }
}

#[inline]
pub fn gen_vertex_arrays(n: GLsizei) -> Vec<GLuint> {
    unsafe {
        let mut result: Vec<_> = repeat(0 as GLuint).take(n as usize).collect();
        ffi::GenVertexArrays(n, result.as_mut_ptr());
        return result;
    }
}

#[cfg(not(target_os="android"))]
#[inline]
pub fn gen_queries(n: GLsizei) -> Vec<GLuint> {
    unsafe {
        let mut result = vec![0; n as usize];
        ffi::GenQueries(n, result.as_mut_ptr());
        return result;
    }
}

#[cfg(not(target_os="android"))]
#[inline]
pub fn begin_query(target: GLenum, id: GLuint) {
    unsafe {
        ffi::BeginQuery(target, id);
    }
}

#[cfg(not(target_os="android"))]
#[inline]
pub fn end_query(target: GLenum) {
    unsafe {
        ffi::EndQuery(target);
    }
}

#[cfg(not(target_os="android"))]
#[inline]
pub fn query_counter(id: GLuint, target: GLenum) {
    unsafe {
        ffi::QueryCounter(id, target);
    }
}

#[cfg(not(target_os="android"))]
#[inline]
pub fn get_query_object_iv(id: GLuint, pname: GLenum) -> i32 {
    unsafe {
        let mut result = 0;
        ffi::GetQueryObjectiv(id, pname, &mut result);
        result
    }
}

#[cfg(not(target_os="android"))]
#[inline]
pub fn get_query_object_uiv(id: GLuint, pname: GLenum) -> u32 {
    unsafe {
        let mut result = 0;
        ffi::GetQueryObjectuiv(id, pname, &mut result);
        result
    }
}

#[cfg(not(target_os="android"))]
#[inline]
pub fn get_query_object_i64v(id: GLuint, pname: GLenum) -> i64 {
    unsafe {
        let mut result = 0;
        ffi::GetQueryObjecti64v(id, pname, &mut result);
        result
    }
}

#[cfg(not(target_os="android"))]
#[inline]
pub fn get_query_object_ui64v(id: GLuint, pname: GLenum) -> u64 {
    unsafe {
        let mut result = 0;
        ffi::GetQueryObjectui64v(id, pname, &mut result);
        result
    }
}

#[cfg(not(target_os="android"))]
#[inline]
pub fn delete_queries(queries: &[GLuint]) {
    unsafe {
        ffi::DeleteQueries(queries.len() as GLsizei, queries.as_ptr());
    }
}

#[inline]
pub fn delete_vertex_arrays(vertex_arrays: &[GLuint]) {
    unsafe {
        ffi::DeleteVertexArrays(vertex_arrays.len() as GLsizei, vertex_arrays.as_ptr());
    }
}

#[inline]
pub fn delete_buffers(buffers: &[GLuint]) {
    unsafe {
        ffi::DeleteBuffers(buffers.len() as GLsizei, buffers.as_ptr());
    }
}

#[inline]
pub fn delete_renderbuffers(renderbuffers: &[GLuint]) {
    unsafe {
        ffi::DeleteRenderbuffers(renderbuffers.len() as GLsizei, renderbuffers.as_ptr());
    }
}

#[inline]
pub fn delete_framebuffers(framebuffers: &[GLuint]) {
    unsafe {
        ffi::DeleteFramebuffers(framebuffers.len() as GLsizei, framebuffers.as_ptr());
    }
}

// NB: The name of this function is wrong, it's here for compatibility reasons,
// but should be removed.
#[inline]
pub fn delete_frame_buffers(framebuffers: &[GLuint]) {
    delete_framebuffers(framebuffers);
}

#[inline]
pub fn delete_textures(textures: &[GLuint]) {
    unsafe {
        ffi::DeleteTextures(textures.len() as GLsizei, textures.as_ptr());
    }
}

#[inline]
pub fn framebuffer_renderbuffer(target: GLenum,
                                attachment: GLenum,
                                renderbuffertarget: GLenum,
                                renderbuffer: GLuint) {
    unsafe {
        ffi::FramebufferRenderbuffer(target,
                                     attachment,
                                     renderbuffertarget,
                                     renderbuffer);
    }
}

#[inline]
pub fn renderbuffer_storage(target: GLenum,
                            internalformat: GLenum,
                            width: GLsizei,
                            height: GLsizei) {
    unsafe {
        ffi::RenderbufferStorage(target,
                                 internalformat,
                                 width,
                                 height);
    }
}

#[inline]
pub fn depth_func(func: GLenum) {
    unsafe {
        ffi::DepthFunc(func);
    }
}

#[inline]
pub fn active_texture(texture: GLenum) {
    unsafe {
        ffi::ActiveTexture(texture);
    }
}

#[inline]
pub fn attach_shader(program: GLuint, shader: GLuint) {
    unsafe {
        ffi::AttachShader(program, shader);
    }
}

#[inline]
pub fn bind_attrib_location(program: GLuint, index: GLuint, name: &str) {
    let c_string = CString::new(name).unwrap();
    unsafe {
        ffi::BindAttribLocation(program, index, c_string.as_ptr())
    }
}

#[inline]
pub fn get_uniform_block_index(program: GLuint, name: &str) -> GLuint {
    let c_string = CString::new(name).unwrap();
    unsafe {
        ffi::GetUniformBlockIndex(program, c_string.as_ptr())
    }
}

#[inline]
pub fn bind_buffer_base(target: GLenum, index: GLuint, buffer: GLuint) {
    unsafe {
        ffi::BindBufferBase(target, index, buffer);
    }
}

#[inline]
pub fn uniform_block_binding(program: GLuint, uniform_block_index: GLuint, uniform_block_binding: GLuint) {
    unsafe {
        ffi::UniformBlockBinding(program, uniform_block_index, uniform_block_binding);
    }
}

#[inline]
pub fn bind_buffer(target: GLenum, buffer: GLuint) {
    unsafe {
        ffi::BindBuffer(target, buffer);
    }
}

#[inline]
pub fn bind_vertex_array(vao: GLuint) {
    unsafe {
        ffi::BindVertexArray(vao);
    }
}

#[inline]
pub fn bind_renderbuffer(target: GLenum, renderbuffer: GLuint) {
    unsafe {
        ffi::BindRenderbuffer(target, renderbuffer);
    }
}

#[inline]
pub fn bind_framebuffer(target: GLenum, framebuffer: GLuint) {
    unsafe {
        ffi::BindFramebuffer(target, framebuffer);
    }
}

#[inline]
pub fn bind_texture(target: GLenum, texture: GLuint) {
    unsafe {
        ffi::BindTexture(target, texture);
    }
}

// FIXME: Does not verify buffer size -- unsafe!
pub fn tex_image_2d(target: GLenum,
                    level: GLint,
                    internal_format: GLint,
                    width: GLsizei,
                    height: GLsizei,
                    border: GLint,
                    format: GLenum,
                    ty: GLenum,
                    opt_data: Option<&[u8]>) {
    match opt_data {
        Some(data) => {
            unsafe {
                ffi::TexImage2D(target, level, internal_format, width, height, border, format, ty,
                                data.as_ptr() as *const GLvoid);
            }
        }
        None => {
            unsafe {
                ffi::TexImage2D(target, level, internal_format, width, height, border, format, ty,
                               ptr::null());
            }
        }
    }
}

pub fn compressed_tex_image_2d(target: GLenum,
                               level: GLint,
                               internal_format: GLenum,
                               width: GLsizei,
                               height: GLsizei,
                               border: GLint,
                               data: &[u8]) {
    unsafe {
        ffi::CompressedTexImage2D(target, level, internal_format, width, height, border,
                                  data.len() as GLsizei, data.as_ptr() as *const GLvoid);
    }
}

pub fn compressed_tex_sub_image_2d(target: GLenum,
                                   level: GLint,
                                   xoffset: GLint,
                                   yoffset: GLint,
                                   width: GLsizei,
                                   height: GLsizei,
                                   format: GLenum,
                                   data: &[u8]) {
    unsafe {
        ffi::CompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format,
                                     data.len() as GLsizei, data.as_ptr() as *const GLvoid);
    }
}

// FIXME: Does not verify buffer size -- unsafe!
pub fn tex_image_3d(target: GLenum,
                    level: GLint,
                    internal_format: GLint,
                    width: GLsizei,
                    height: GLsizei,
                    depth: GLsizei,
                    border: GLint,
                    format: GLenum,
                    ty: GLenum,
                    opt_data: Option<&[u8]>) {
    unsafe {
        let pdata = match opt_data {
            Some(data) => mem::transmute(data.as_ptr()),
            None => ptr::null(),
        };
        ffi::TexImage3D(target,
                        level,
                        internal_format,
                        width,
                        height,
                        depth,
                        border,
                        format,
                        ty,
                        pdata);
    }
}

pub fn copy_tex_image_2d(target: GLenum,
                         level: GLint,
                         internal_format: GLenum,
                         x: GLint,
                         y: GLint,
                         width: GLsizei,
                         height: GLsizei,
                         border: GLint) {
    unsafe {
        ffi::CopyTexImage2D(target,
                            level,
                            internal_format,
                            x,
                            y,
                            width,
                            height,
                            border);
    }
}

pub fn copy_tex_sub_image_2d(target: GLenum,
                             level: GLint,
                             xoffset: GLint,
                             yoffset: GLint,
                             x: GLint,
                             y: GLint,
                             width: GLsizei,
                             height: GLsizei) {
    unsafe {
        ffi::CopyTexSubImage2D(target,
                               level,
                               xoffset,
                               yoffset,
                               x,
                               y,
                               width,
                               height);
    }
}

#[inline]
pub fn copy_tex_sub_image_3d(target: GLenum,
                             level: GLint,
                             xoffset: GLint,
                             yoffset: GLint,
                             zoffset: GLint,
                             x: GLint,
                             y: GLint,
                             width: GLsizei,
                             height: GLsizei) {
    unsafe {
        ffi::CopyTexSubImage3D(target,
                               level,
                               xoffset,
                               yoffset,
                               zoffset,
                               x,
                               y,
                               width,
                               height);
    }
}

pub fn tex_sub_image_2d(target: GLenum,
                        level: GLint,
                        xoffset: GLint,
                        yoffset: GLint,
                        width: GLsizei,
                        height: GLsizei,
                        format: GLenum,
                        ty: GLenum,
                        data: &[u8]) {
    unsafe {
        ffi::TexSubImage2D(target, level, xoffset, yoffset, width, height, format, ty, data.as_ptr() as *const c_void);
    }
}

pub fn tex_sub_image_3d(target: GLenum,
                        level: GLint,
                        xoffset: GLint,
                        yoffset: GLint,
                        zoffset: GLint,
                        width: GLsizei,
                        height: GLsizei,
                        depth: GLsizei,
                        format: GLenum,
                        ty: GLenum,
                        data: &[u8]) {
    unsafe {
        ffi::TexSubImage3D(target,
                           level,
                           xoffset,
                           yoffset,
                           zoffset,
                           width,
                           height,
                           depth,
                           format,
                           ty,
                           data.as_ptr() as *const c_void);
    }
}

#[inline]
pub fn get_integer_v(name: GLenum) -> GLint {
    let mut result: GLint = 0 as GLint;
    unsafe {
        ffi::GetIntegerv(name, &mut result);
    }
    result
}

#[inline]
pub fn get_boolean_v(name: GLenum) -> GLboolean {
    let mut result: GLboolean = 0 as GLboolean;
    unsafe {
        ffi::GetBooleanv(name, &mut result);
    }
    result
}


#[inline]
pub fn get_float_v(name: GLenum) -> GLfloat {
    let mut result: GLfloat = 0 as GLfloat;
    unsafe {
        ffi::GetFloatv(name, &mut result);
    }
    result
}

#[inline]
pub fn tex_parameter_i(target: GLenum, pname: GLenum, param: GLint) {
    unsafe {
        ffi::TexParameteri(target, pname, param);
    }
}

#[inline]
pub fn tex_parameter_f(target: GLenum, pname: GLenum, param: GLfloat) {
    unsafe {
        ffi::TexParameterf(target, pname, param);
    }
}

#[inline]
pub fn framebuffer_texture_2d(target: GLenum,
                              attachment: GLenum,
                              textarget: GLenum,
                              texture: GLuint,
                              level: GLint) {
    unsafe {
        ffi::FramebufferTexture2D(target, attachment, textarget, texture, level);
    }
}

#[inline]
pub fn framebuffer_texture_layer(target: GLenum,
                                 attachment: GLenum,
                                 texture: GLuint,
                                 level: GLint,
                                 layer: GLint) {
    unsafe {
        ffi::FramebufferTextureLayer(target, attachment, texture, level, layer);
    }
}

#[inline]
pub fn blit_framebuffer(srcX0: GLint,
                        srcY0: GLint,
                        srcX1: GLint,
                        srcY1: GLint,
                        dstX0: GLint,
                        dstY0: GLint,
                        dstX1: GLint,
                        dstY1: GLint,
                        mask: GLbitfield,
                        filter: GLenum) {
    unsafe {
        ffi::BlitFramebuffer(srcX0,
                             srcY0,
                             srcX1,
                             srcY1,
                             dstX0,
                             dstY0,
                             dstX1,
                             dstY1,
                             mask,
                             filter);
    }
}

#[inline]
pub fn vertex_attrib_4f(index: GLuint,
                        x: GLfloat,
                        y: GLfloat,
                        z: GLfloat,
                        w: GLfloat) {
    unsafe {
        ffi::VertexAttrib4f(index, x, y, z, w)
    }
}

#[inline]
pub fn vertex_attrib_pointer_f32(index: GLuint,
                                 size: GLint,
                                 normalized: bool,
                                 stride: GLsizei,
                                 offset: GLuint) {
    unsafe {
        ffi::VertexAttribPointer(index,
                                size,
                                ffi::FLOAT,
                                normalized as GLboolean,
                                stride,
                                offset as *const GLvoid)
    }
}

#[inline]
pub fn vertex_attrib_pointer(index: GLuint,
                             size: GLint,
                             type_: GLenum,
                             normalized: bool,
                             stride: GLsizei,
                             offset: GLuint) {
    unsafe {
        ffi::VertexAttribPointer(index,
                                 size,
                                 type_,
                                 normalized as GLboolean,
                                 stride,
                                 offset as *const GLvoid)
    }
}

#[inline]
pub fn vertex_attrib_i_pointer(index: GLuint,
                               size: GLint,
                               type_: GLenum,
                               stride: GLsizei,
                               offset: GLuint) {
    unsafe {
        ffi::VertexAttribIPointer(index,
                                  size,
                                  type_,
                                  stride,
                                  offset as *const GLvoid)
    }
}

#[inline]
pub fn vertex_attrib_divisor(index: GLuint, divisor: GLuint) {
    unsafe {
        ffi::VertexAttribDivisor(index, divisor)
    }
}

#[inline]
pub fn viewport(x: GLint, y: GLint, width: GLsizei, height: GLsizei) {
    unsafe {
        ffi::Viewport(x, y, width, height);
    }
}

#[inline]
pub fn scissor(x: GLint, y: GLint, width: GLsizei, height: GLsizei) {
    unsafe {
        ffi::Scissor(x, y, width, height);
    }
}

#[inline]
pub fn line_width(width: GLfloat) {
    unsafe {
        ffi::LineWidth(width);
    }
}

#[inline]
pub fn use_program(program: GLuint) {
    unsafe {
        ffi::UseProgram(program);
    }
}

#[inline]
pub fn validate_program(program: GLuint) {
    unsafe {
        ffi::ValidateProgram(program);
    }
}

#[inline]
pub fn draw_arrays(mode: GLenum, first: GLint, count: GLsizei) {
    unsafe {
        return ffi::DrawArrays(mode, first, count);
    }
}

#[inline]
pub fn draw_arrays_instanced(mode: GLenum, first: GLint, count: GLsizei, primcount: GLsizei) {
    unsafe {
        return ffi::DrawArraysInstanced(mode, first, count, primcount);
    }
}

#[inline]
pub fn draw_elements(mode: GLenum, count: GLsizei, element_type: GLenum, indices_offset: GLuint) {
    unsafe {
        return ffi::DrawElements(mode, count, element_type, indices_offset as *const c_void)
    }
}

#[inline]
pub fn draw_elements_instanced(mode: GLenum,
                               count: GLsizei,
                               element_type: GLenum,
                               indices_offset: GLuint,
                               primcount: GLsizei) {
    unsafe {
        return ffi::DrawElementsInstanced(mode,
                                          count,
                                          element_type,
                                          indices_offset as *const c_void,
                                          primcount)
    }
}

#[inline]
pub fn blend_color(r: f32, g: f32, b: f32, a: f32) {
    unsafe {
        ffi::BlendColor(r, g, b, a);
    }
}

#[inline]
pub fn blend_func(sfactor: GLenum, dfactor: GLenum) {
    unsafe {
        ffi::BlendFunc(sfactor, dfactor);
    }
}

#[inline]
pub fn blend_func_separate(src_rgb: GLenum, dest_rgb: GLenum, src_alpha: GLenum, dest_alpha: GLenum) {
    unsafe {
        ffi::BlendFuncSeparate(src_rgb, dest_rgb, src_alpha, dest_alpha);
    }
}

#[inline]
pub fn blend_equation(mode: GLenum) {
    unsafe {
        ffi::BlendEquation(mode);
    }
}

#[inline]
pub fn blend_equation_separate(mode_rgb: GLenum, mode_alpha: GLenum) {
    unsafe {
        ffi::BlendEquationSeparate(mode_rgb, mode_alpha);
    }
}

#[inline]
pub fn color_mask(r: bool, g: bool, b: bool, a: bool) {
    unsafe {
        ffi::ColorMask(r as GLboolean, g as GLboolean, b as GLboolean, a as GLboolean);
    }
}

#[inline]
pub fn cull_face(mode: GLenum) {
    unsafe {
        ffi::CullFace(mode);
    }
}

#[inline]
pub fn front_face(mode: GLenum) {
    unsafe {
        ffi::FrontFace(mode);
    }
}

#[inline]
pub fn enable(cap: GLenum) {
    unsafe {
        ffi::Enable(cap);
    }
}

#[inline]
pub fn disable(cap: GLenum) {
    unsafe {
        ffi::Disable(cap);
    }
}

#[inline]
pub fn hint(param_name: GLenum, param_val: GLenum) {
    unsafe {
        ffi::Hint(param_name, param_val);
    }
}

#[inline]
pub fn is_enabled(cap: GLenum) -> GLboolean {
    unsafe {
        ffi::IsEnabled(cap)
    }
}

#[inline]
pub fn is_shader(shader: GLuint) -> GLboolean {
    unsafe {
        ffi::IsShader(shader)
    }
}

#[inline]
pub fn is_texture(texture: GLenum) -> GLboolean {
    unsafe {
        ffi::IsTexture(texture)
    }
}

#[inline]
pub fn is_framebuffer(framebuffer: GLenum) -> GLboolean {
    unsafe {
        ffi::IsFramebuffer(framebuffer)
    }
}

#[inline]
pub fn is_renderbuffer(renderbuffer: GLenum) -> GLboolean {
    unsafe {
        ffi::IsRenderbuffer(renderbuffer)
    }
}

#[inline]
pub fn check_frame_buffer_status(target: GLenum) -> GLenum {
    unsafe {
        ffi::CheckFramebufferStatus(target)
    }
}

#[inline]
pub fn enable_vertex_attrib_array(index: GLuint) {
    unsafe {
        ffi::EnableVertexAttribArray(index);
    }
}

#[inline]
pub fn disable_vertex_attrib_array(index: GLuint) {
    unsafe {
        ffi::DisableVertexAttribArray(index);
    }
}

#[inline]
pub fn uniform_1f(location: GLint, v0: GLfloat) {
    unsafe {
        ffi::Uniform1f(location, v0);
    }
}

#[inline]
pub fn uniform_1fv(location: GLint, values: &[f32]) {
    unsafe {
        ffi::Uniform1fv(location,
                        values.len() as GLsizei,
                        values.as_ptr());
    }
}

#[inline]
pub fn uniform_1i(location: GLint, v0: GLint) {
    unsafe {
        ffi::Uniform1i(location, v0);
    }
}

#[inline]
pub fn uniform_1iv(location: GLint, values: &[i32]) {
    unsafe {
        ffi::Uniform1iv(location,
                        values.len() as GLsizei,
                        values.as_ptr());
    }
}

#[cfg(not(target_os="android"))]
#[inline]
pub fn uniform_1ui(location: GLint, v0: GLuint) {
    unsafe {
        ffi::Uniform1ui(location, v0);
    }
}

#[inline]
pub fn uniform_2f(location: GLint, v0: GLfloat, v1: GLfloat) {
    unsafe {
        ffi::Uniform2f(location, v0, v1);
    }
}

#[inline]
pub fn uniform_2fv(location: GLint, values: &[f32]) {
    unsafe {
        ffi::Uniform2fv(location,
                        (values.len() / 2) as GLsizei,
                        values.as_ptr());
    }
}

#[inline]
pub fn uniform_2i(location: GLint, v0: GLint, v1: GLint) {
    unsafe {
        ffi::Uniform2i(location, v0, v1);
    }
}

#[inline]
pub fn uniform_2iv(location: GLint, values: &[i32]) {
    unsafe {
        ffi::Uniform2iv(location,
                        (values.len() / 2) as GLsizei,
                        values.as_ptr());
    }
}

#[cfg(not(target_os="android"))]
#[inline]
pub fn uniform_2ui(location: GLint, v0: GLuint, v1: GLuint) {
    unsafe {
        ffi::Uniform2ui(location, v0, v1);
    }
}

#[inline]
pub fn uniform_3f(location: GLint, v0: GLfloat, v1: GLfloat, v2: GLfloat) {
    unsafe {
        ffi::Uniform3f(location, v0, v1, v2);
    }
}

#[inline]
pub fn uniform_3fv(location: GLint, values: &[f32]) {
    unsafe {
        ffi::Uniform3fv(location,
                        (values.len() / 3) as GLsizei,
                        values.as_ptr());
    }
}

#[inline]
pub fn uniform_3i(location: GLint, v0: GLint, v1: GLint, v2: GLint) {
    unsafe {
        ffi::Uniform3i(location, v0, v1, v2);
    }
}

#[inline]
pub fn uniform_3iv(location: GLint, values: &[i32]) {
    unsafe {
        ffi::Uniform3iv(location,
                        (values.len() / 3) as GLsizei,
                        values.as_ptr());
    }
}

#[cfg(not(target_os="android"))]
#[inline]
pub fn uniform_3ui(location: GLint, v0: GLuint, v1: GLuint, v2: GLuint) {
    unsafe {
        ffi::Uniform3ui(location, v0, v1, v2);
    }
}

#[inline]
pub fn uniform_4f(location: GLint, x: GLfloat, y: GLfloat, z: GLfloat, w: GLfloat) {
    unsafe {
        ffi::Uniform4f(location, x, y, z, w);
    }
}

#[inline]
pub fn uniform_4i(location: GLint, x: GLint, y: GLint, z: GLint, w: GLint) {
    unsafe {
        ffi::Uniform4i(location, x, y, z, w);
    }
}

#[inline]
pub fn uniform_4iv(location: GLint, values: &[i32]) {
    unsafe {
        ffi::Uniform4iv(location,
                        (values.len() / 4) as GLsizei,
                        values.as_ptr());
    }
}

#[cfg(not(target_os="android"))]
#[inline]
pub fn uniform_4ui(location: GLint, x: GLuint, y: GLuint, z: GLuint, w: GLuint) {
    unsafe {
        ffi::Uniform4ui(location, x, y, z, w);
    }
}

#[inline]
pub fn uniform_4fv(location: GLint, values: &[f32]) {
    unsafe {
        ffi::Uniform4fv(location,
                        (values.len() / 4) as GLsizei,
                        values.as_ptr());
    }
}

#[inline]
pub fn uniform_matrix_2fv(location: GLint, transpose: bool, value: &[f32]) {
    unsafe {
        ffi::UniformMatrix2fv(location,
                              (value.len() / 4) as GLsizei,
                              transpose as GLboolean,
                              value.as_ptr());
    }
}

#[inline]
pub fn uniform_matrix_3fv(location: GLint, transpose: bool, value: &[f32]) {
    unsafe {
        ffi::UniformMatrix3fv(location,
                              (value.len() / 9) as GLsizei,
                              transpose as GLboolean,
                              value.as_ptr());
    }
}

#[inline]
pub fn uniform_matrix_4fv(location: GLint, transpose: bool, value: &[f32]) {
    unsafe {
        ffi::UniformMatrix4fv(location,
                              (value.len() / 16) as GLsizei,
                              transpose as GLboolean,
                              value.as_ptr());
    }
}

#[inline]
pub fn depth_mask(flag: bool) {
    unsafe {
        ffi::DepthMask(flag as GLboolean);
    }
}

#[cfg(not(target_os="android"))]
#[inline]
pub fn depth_range(near: f64, far: f64) {
    unsafe {
        ffi::DepthRange(near as GLclampd, far as GLclampd);
    }
}

#[cfg(target_os="android")]
#[inline]
pub fn depth_range(near: f64, far: f64) {
    unsafe {
        ffi::DepthRangef(near as GLclampf, far as GLclampf);
    }
}

#[inline]
pub fn get_active_attrib(program: GLuint, index: GLuint) -> (i32, u32, String) {
    let buf_size = get_program_iv(program, ffi::ACTIVE_ATTRIBUTE_MAX_LENGTH);
    let mut name = vec![0u8; buf_size as usize];
    let mut length = 0 as GLsizei;
    let mut size = 0 as i32;
    let mut type_ = 0 as u32;
    unsafe {
        ffi::GetActiveAttrib(program, index, buf_size, &mut length, &mut size, &mut type_, name.as_mut_ptr() as *mut GLchar);
    }
    name.truncate(if length > 0 {length as usize} else {0});
    (size, type_, String::from_utf8(name).unwrap())
}

#[inline]
pub fn get_active_uniform(program: GLuint, index: GLuint) -> (i32, u32, String) {
    let buf_size = get_program_iv(program, ffi::ACTIVE_UNIFORM_MAX_LENGTH);
    let mut name = vec![0 as u8; buf_size as usize];
    let mut length: GLsizei = 0;
    let mut size: i32 = 0;
    let mut type_: u32 = 0;

    unsafe {
        ffi::GetActiveUniform(program, index, buf_size, &mut length, &mut size,
                              &mut type_, name.as_mut_ptr() as *mut GLchar);
    }

    name.truncate(if length > 0 { length as usize } else { 0 });

    (size, type_, String::from_utf8(name).unwrap())
}

#[inline]
pub fn get_attrib_location(program: GLuint, name: &str) -> c_int {
    let name = CString::new(name).unwrap();
    unsafe {
        ffi::GetAttribLocation(program, name.as_ptr())
    }
}

#[cfg(not(target_os="android"))]
#[inline]
pub fn get_frag_data_location(program: GLuint, name: &str) -> c_int {
    let name = CString::new(name).unwrap();
    unsafe {
        ffi::GetFragDataLocation(program, name.as_ptr())
    }
}

#[inline]
pub fn get_uniform_location(program: GLuint, name: &str) -> c_int {
    let name = CString::new(name).unwrap();
    unsafe {
        ffi::GetUniformLocation(program, name.as_ptr())
    }
}

pub fn get_program_info_log(program: GLuint) -> String {
    unsafe {
        let mut result = vec![0u8; 1024];
        let mut result_len: GLsizei = 0 as GLsizei;
        ffi::GetProgramInfoLog(program,
                            1024 as GLsizei,
                            &mut result_len,
                            result.as_mut_ptr() as *mut GLchar);
        result.truncate(if result_len > 0 {result_len as usize} else {0});
        String::from_utf8(result).unwrap()
    }
}

#[inline]
pub fn get_program_iv(program: GLuint, pname: GLenum) -> GLint {
    unsafe {
        let mut result: GLint = 0 as GLint;
        ffi::GetProgramiv(program, pname, &mut result);
        return result;
    }
}

#[inline]
pub fn get_vertex_attrib_iv(index: GLuint, pname: GLenum) -> GLint {
    unsafe {
        let mut result: GLint = 0 as GLint;
        ffi::GetVertexAttribiv(index, pname, &mut result);
        return result;
    }
}

#[inline]
pub fn get_vertex_attrib_fv(index: GLuint, pname: GLenum) -> Vec<GLfloat> {
    unsafe {
        let mut result = vec![0 as GLfloat; 4];
        ffi::GetVertexAttribfv(index, pname, result.as_mut_ptr());
        return result;
    }
}

#[inline]
pub fn get_buffer_parameter_iv(target: GLuint, pname: GLenum) -> GLint {
    unsafe {
        let mut result: GLint = 0 as GLint;
        ffi::GetBufferParameteriv(target, pname, &mut result);
        return result;
    }
}

pub fn get_shader_info_log(shader: GLuint) -> String {
    unsafe {
        let mut result = vec![0u8; 1024];
        let mut result_len: GLsizei = 0 as GLsizei;
        ffi::GetShaderInfoLog(shader,
                           1024 as GLsizei,
                           &mut result_len,
                           result.as_mut_ptr() as *mut GLchar);
        result.truncate(if result_len > 0 {result_len as usize} else {0});
        String::from_utf8(result).unwrap()
    }
}

#[inline]
pub fn get_string(which: GLenum) -> String {
    unsafe {
        let llstr = ffi::GetString(which);
        if !llstr.is_null() {
            return str::from_utf8_unchecked(CStr::from_ptr(llstr as *const c_char).to_bytes()).to_string();
        } else {
            return "".to_string();
        }
    }
}

#[inline]
pub fn get_shader_iv(shader: GLuint, pname: GLenum) -> GLint {
    unsafe {
        let mut result: GLint = 0 as GLint;
        ffi::GetShaderiv(shader, pname, &mut result);
        return result;
    }
}

#[inline]
pub fn compile_shader(shader: GLuint) {
    unsafe {
        ffi::CompileShader(shader);
    }
}

#[inline]
pub fn create_program() -> GLuint {
    unsafe {
        return ffi::CreateProgram();
    }
}

#[inline]
pub fn delete_program(program: GLuint) {
    unsafe {
        ffi::DeleteProgram(program);
    }
}

#[inline]
pub fn create_shader(shader_type: GLenum) -> GLuint {
    unsafe {
        return ffi::CreateShader(shader_type);
    }
}

#[inline]
pub fn delete_shader(shader: GLuint) {
    unsafe {
        ffi::DeleteShader(shader);
    }
}

#[inline]
pub fn detach_shader(program: GLuint, shader: GLuint) {
    unsafe {
        ffi::DetachShader(program, shader);
    }
}

#[inline]
pub fn link_program(program: GLuint) {
    unsafe {
        return ffi::LinkProgram(program);
    }
}

#[inline]
pub fn clear_color(r: f32, g: f32, b: f32, a: f32) {
    unsafe {
        ffi::ClearColor(r, g, b, a);
    }
}

#[inline]
pub fn clear(buffer_mask: GLbitfield) {
    unsafe {
        ffi::Clear(buffer_mask);
    }
}

#[cfg(not(target_os="android"))]
#[inline]
pub fn clear_depth(depth: f64) {
    unsafe {
        ffi::ClearDepth(depth as GLclampd);
    }
}

#[cfg(target_os="android")]
#[inline]
pub fn clear_depth(depth: f64) {
    unsafe {
        ffi::ClearDepthf(depth as GLclampf);
    }
}

#[inline]
pub fn clear_stencil(s: GLint) {
    unsafe {
        ffi::ClearStencil(s);
    }
}

#[inline]
pub fn flush() {
    unsafe {
        ffi::Flush();
    }
}

#[inline]
pub fn finish() {
    unsafe {
        ffi::Finish();
    }
}

#[inline]
pub fn get_error() -> GLenum {
    unsafe {
        ffi::GetError()
    }
}

#[inline]
pub fn stencil_mask(mask: GLuint) {
    unsafe {
        ffi::StencilMask(mask)
    }
}

#[inline]
pub fn stencil_mask_separate(face: GLenum, mask: GLuint) {
    unsafe {
        ffi::StencilMaskSeparate(face, mask)
    }
}

#[inline]
pub fn stencil_func(func: GLenum,
                    ref_: GLint,
                    mask: GLuint) {
    unsafe {
        ffi::StencilFunc(func, ref_, mask)
    }
}

#[inline]
pub fn stencil_func_separate(face: GLenum,
                             func: GLenum,
                             ref_: GLint,
                             mask: GLuint) {
    unsafe {
        ffi::StencilFuncSeparate(face, func, ref_, mask)
    }
}

#[inline]
pub fn stencil_op(sfail: GLenum,
                  dpfail: GLenum,
                  dppass: GLenum) {
    unsafe {
        ffi::StencilOp(sfail, dpfail, dppass)
    }
}

#[inline]
pub fn stencil_op_separate(face: GLenum,
                           sfail: GLenum,
                           dpfail: GLenum,
                           dppass: GLenum) {
    unsafe {
        ffi::StencilOpSeparate(face, sfail, dpfail, dppass)
    }
}

#[cfg(target_os="android")]
extern {
    pub fn glEGLImageTargetTexture2DOES(target: GLenum, image: GLeglImageOES);
}

#[cfg(target_os="android")]
pub fn egl_image_target_texture2d_oes(target: GLenum, image: GLeglImageOES) {
    unsafe {
        glEGLImageTargetTexture2DOES(target, image);
    }
}

#[inline]
pub fn generate_mipmap(target: GLenum) {
    unsafe {
        ffi::GenerateMipmap(target)
    }
}

#[inline]
#[cfg(not(target_os="android"))]
pub fn insert_event_marker_ext(message: &str) {
    if ffi::InsertEventMarkerEXT::is_loaded() {
        unsafe {
            ffi::InsertEventMarkerEXT(message.len() as GLsizei, message.as_ptr() as *const _);
        }
    }
}

#[inline]
#[cfg(not(target_os="android"))]
pub fn push_group_marker_ext(message: &str) {
    if ffi::PushGroupMarkerEXT::is_loaded() {
        unsafe {
            ffi::PushGroupMarkerEXT(message.len() as GLsizei, message.as_ptr() as *const _);
        }
    }
}

#[inline]
#[cfg(not(target_os="android"))]
pub fn pop_group_marker_ext() {
    if ffi::PopGroupMarkerEXT::is_loaded() {
        unsafe {
            ffi::PopGroupMarkerEXT();
        }
    }
}
