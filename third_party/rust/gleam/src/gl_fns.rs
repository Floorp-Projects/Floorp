// Copyright 2014 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

pub struct GlFns {
    ffi_gl_: GlFfi,
}

impl GlFns
{
    pub unsafe fn load_with<'a, F>(loadfn: F) -> Rc<Gl> where F: FnMut(&str) -> *const c_void {
        let ffi_gl_ = GlFfi::load_with(loadfn);
        Rc::new(GlFns {
            ffi_gl_: ffi_gl_,
        }) as Rc<Gl>
    }
}

impl Gl for GlFns {
    fn get_type(&self) -> GlType {
        GlType::Gl
    }

    fn buffer_data_untyped(&self, target: GLenum, size: GLsizeiptr, data: *const GLvoid, usage: GLenum) {
        unsafe {
            self.ffi_gl_.BufferData(target,
                                    size,
                                    data,
                                    usage);
        }
    }

    fn buffer_sub_data_untyped(&self, target: GLenum, offset: isize, size: GLsizeiptr, data: *const GLvoid) {
        unsafe {
            self.ffi_gl_.BufferSubData(target,
                                       offset,
                                       size,
                                       data);
        }
    }

    fn shader_source(&self, shader: GLuint, strings: &[&[u8]]) {
        let pointers: Vec<*const u8> = strings.iter().map(|string| (*string).as_ptr()).collect();
        let lengths: Vec<GLint> = strings.iter().map(|string| string.len() as GLint).collect();
        unsafe {
            self.ffi_gl_.ShaderSource(shader, pointers.len() as GLsizei,
                                      pointers.as_ptr() as *const *const GLchar, lengths.as_ptr());
        }
        drop(lengths);
        drop(pointers);
    }

    fn tex_buffer(&self, target: GLenum, internal_format: GLenum, buffer: GLuint) {
        unsafe {
            self.ffi_gl_.TexBuffer(target, internal_format, buffer);
        }
    }

    fn read_buffer(&self, mode: GLenum) {
        unsafe {
            self.ffi_gl_.ReadBuffer(mode);
        }
    }

    fn read_pixels_into_buffer(&self, x: GLint, y: GLint, width: GLsizei, height: GLsizei,
                               format: GLenum, pixel_type: GLenum, dst_buffer: &mut [u8]) {
        // Assumes that the user properly allocated the size for dst_buffer.
        assert!(calculate_length(width, height, format, pixel_type) == dst_buffer.len());

        unsafe {
            // We don't want any alignment padding on pixel rows.
            self.ffi_gl_.PixelStorei(ffi::PACK_ALIGNMENT, 1);
            self.ffi_gl_.ReadPixels(x, y, width, height, format, pixel_type, dst_buffer.as_mut_ptr() as *mut c_void);
        }
    }

    fn read_pixels(&self, x: GLint, y: GLint, width: GLsizei, height: GLsizei, format: GLenum, pixel_type: GLenum) -> Vec<u8> {
        let len = calculate_length(width, height, format, pixel_type);
        let mut pixels: Vec<u8> = Vec::new();
        pixels.reserve(len);
        unsafe { pixels.set_len(len); }

        self.read_pixels_into_buffer(x, y, width, height, format, pixel_type, pixels.as_mut_slice());

        pixels
    }

    fn sample_coverage(&self, value: GLclampf, invert: bool) {
        unsafe {
            self.ffi_gl_.SampleCoverage(value, invert as GLboolean);
        }
    }

    fn polygon_offset(&self, factor: GLfloat, units: GLfloat) {
        unsafe {
            self.ffi_gl_.PolygonOffset(factor, units);
        }
    }

    fn pixel_store_i(&self, name: GLenum, param: GLint) {
        unsafe {
            self.ffi_gl_.PixelStorei(name, param);
        }
    }

    fn gen_buffers(&self, n: GLsizei) -> Vec<GLuint> {
        let mut result = vec![0 as GLuint; n as usize];
        unsafe {
            self.ffi_gl_.GenBuffers(n, result.as_mut_ptr());
        }
        result
    }

    fn gen_renderbuffers(&self, n: GLsizei) -> Vec<GLuint> {
        let mut result = vec![0 as GLuint; n as usize];
        unsafe {
            self.ffi_gl_.GenRenderbuffers(n, result.as_mut_ptr());
        }
        result
    }

    fn gen_framebuffers(&self, n: GLsizei) -> Vec<GLuint> {
        let mut result = vec![0 as GLuint; n as usize];
        unsafe {
            self.ffi_gl_.GenFramebuffers(n, result.as_mut_ptr());
        }
        result
    }

    fn gen_textures(&self, n: GLsizei) -> Vec<GLuint> {
        let mut result = vec![0 as GLuint; n as usize];
        unsafe {
            self.ffi_gl_.GenTextures(n, result.as_mut_ptr());
        }
        result
    }

    fn gen_vertex_arrays(&self, n: GLsizei) -> Vec<GLuint> {
        let mut result = vec![0 as GLuint; n as usize];
        unsafe {
            self.ffi_gl_.GenVertexArrays(n, result.as_mut_ptr());
        }
        result
    }

    fn gen_queries(&self, n: GLsizei) -> Vec<GLuint> {
        let mut result = vec![0 as GLuint; n as usize];
        unsafe {
            self.ffi_gl_.GenQueries(n, result.as_mut_ptr());
        }
        result
    }

    fn begin_query(&self, target: GLenum, id: GLuint) {
        unsafe {
            self.ffi_gl_.BeginQuery(target, id);
        }
    }

    fn end_query(&self, target: GLenum) {
        unsafe {
            self.ffi_gl_.EndQuery(target);
        }
    }

    fn query_counter(&self, id: GLuint, target: GLenum) {
        unsafe {
            self.ffi_gl_.QueryCounter(id, target);
        }
    }

    fn get_query_object_iv(&self, id: GLuint, pname: GLenum) -> i32 {
        let mut result = 0;
        unsafe {
            self.ffi_gl_.GetQueryObjectiv(id, pname, &mut result);
        }
        result
    }

    fn get_query_object_uiv(&self, id: GLuint, pname: GLenum) -> u32 {
        let mut result = 0;
        unsafe {
            self.ffi_gl_.GetQueryObjectuiv(id, pname, &mut result);
        }
        result
    }

    fn get_query_object_i64v(&self, id: GLuint, pname: GLenum) -> i64 {
        let mut result = 0;
        unsafe {
            self.ffi_gl_.GetQueryObjecti64v(id, pname, &mut result);
        }
        result
    }

    fn get_query_object_ui64v(&self, id: GLuint, pname: GLenum) -> u64 {
        let mut result = 0;
        unsafe {
            self.ffi_gl_.GetQueryObjectui64v(id, pname, &mut result);
        }
        result
    }

    fn delete_queries(&self, queries: &[GLuint]) {
        unsafe {
            self.ffi_gl_.DeleteQueries(queries.len() as GLsizei, queries.as_ptr());
        }
    }

    fn delete_vertex_arrays(&self, vertex_arrays: &[GLuint]) {
        unsafe {
            self.ffi_gl_.DeleteVertexArrays(vertex_arrays.len() as GLsizei, vertex_arrays.as_ptr());
        }
    }

    fn delete_buffers(&self, buffers: &[GLuint]) {
        unsafe {
            self.ffi_gl_.DeleteBuffers(buffers.len() as GLsizei, buffers.as_ptr());
        }
    }

    fn delete_renderbuffers(&self, renderbuffers: &[GLuint]) {
        unsafe {
            self.ffi_gl_.DeleteRenderbuffers(renderbuffers.len() as GLsizei, renderbuffers.as_ptr());
        }
    }

    fn delete_framebuffers(&self, framebuffers: &[GLuint]) {
        unsafe {
            self.ffi_gl_.DeleteFramebuffers(framebuffers.len() as GLsizei, framebuffers.as_ptr());
        }
    }

    fn delete_textures(&self, textures: &[GLuint]) {
        unsafe {
            self.ffi_gl_.DeleteTextures(textures.len() as GLsizei, textures.as_ptr());
        }
    }

    fn framebuffer_renderbuffer(&self,
                                target: GLenum,
                                attachment: GLenum,
                                renderbuffertarget: GLenum,
                                renderbuffer: GLuint) {
        unsafe {
            self.ffi_gl_.FramebufferRenderbuffer(target,
                                                 attachment,
                                                 renderbuffertarget,
                                                 renderbuffer);
        }
    }

    fn renderbuffer_storage(&self,
                                target: GLenum,
                                internalformat: GLenum,
                                width: GLsizei,
                                height: GLsizei) {
        unsafe {
            self.ffi_gl_.RenderbufferStorage(target,
                                             internalformat,
                                             width,
                                             height);
        }
    }

    fn depth_func(&self, func: GLenum) {
        unsafe {
            self.ffi_gl_.DepthFunc(func);
        }
    }

    fn active_texture(&self, texture: GLenum) {
        unsafe {
            self.ffi_gl_.ActiveTexture(texture);
        }
    }

    fn attach_shader(&self, program: GLuint, shader: GLuint) {
        unsafe {
            self.ffi_gl_.AttachShader(program, shader);
        }
    }

    fn bind_attrib_location(&self, program: GLuint, index: GLuint, name: &str) {
        let c_string = CString::new(name).unwrap();
        unsafe {
            self.ffi_gl_.BindAttribLocation(program, index, c_string.as_ptr())
        }
    }

    // https://www.khronos.org/registry/OpenGL-Refpages/es2.0/xhtml/glGetUniform.xml
    unsafe fn get_uniform_iv(&self, program: GLuint, location: GLint, result: &mut [GLint]) {
        assert!(!result.is_empty());
        self.ffi_gl_.GetUniformiv(program, location, result.as_mut_ptr());
    }

    // https://www.khronos.org/registry/OpenGL-Refpages/es2.0/xhtml/glGetUniform.xml
    unsafe fn get_uniform_fv(&self, program: GLuint, location: GLint, result: &mut [GLfloat]) {
        assert!(!result.is_empty());
        self.ffi_gl_.GetUniformfv(program, location, result.as_mut_ptr());
    }

    fn get_uniform_block_index(&self, program: GLuint, name: &str) -> GLuint {
        let c_string = CString::new(name).unwrap();
        unsafe {
            self.ffi_gl_.GetUniformBlockIndex(program, c_string.as_ptr())
        }
    }

    fn get_uniform_indices(&self,  program: GLuint, names: &[&str]) -> Vec<GLuint> {
        let c_strings: Vec<CString> = names.iter().map(|n| CString::new(*n).unwrap()).collect();
        let pointers: Vec<*const GLchar> = c_strings.iter().map(|string| string.as_ptr()).collect();
        let mut result = Vec::with_capacity(c_strings.len());
        unsafe {
            result.set_len(c_strings.len());
            self.ffi_gl_.GetUniformIndices(program,
                                           pointers.len() as GLsizei,
                                           pointers.as_ptr(),
                                           result.as_mut_ptr());
        }
        result
    }

    fn bind_buffer_base(&self, target: GLenum, index: GLuint, buffer: GLuint) {
        unsafe {
            self.ffi_gl_.BindBufferBase(target, index, buffer);
        }
    }

    fn bind_buffer_range(&self, target: GLenum, index: GLuint, buffer: GLuint, offset: GLintptr, size: GLsizeiptr) {
        unsafe {
            self.ffi_gl_.BindBufferRange(target, index, buffer, offset, size);
        }
    }

    fn uniform_block_binding(&self, program: GLuint, uniform_block_index: GLuint, uniform_block_binding: GLuint) {
        unsafe {
            self.ffi_gl_.UniformBlockBinding(program, uniform_block_index, uniform_block_binding);
        }
    }

    fn bind_buffer(&self, target: GLenum, buffer: GLuint) {
        unsafe {
            self.ffi_gl_.BindBuffer(target, buffer);
        }
    }

    fn bind_vertex_array(&self, vao: GLuint) {
        unsafe {
            self.ffi_gl_.BindVertexArray(vao);
        }
    }

    fn bind_renderbuffer(&self, target: GLenum, renderbuffer: GLuint) {
        unsafe {
            self.ffi_gl_.BindRenderbuffer(target, renderbuffer);
        }
    }

    fn bind_framebuffer(&self, target: GLenum, framebuffer: GLuint) {
        unsafe {
            self.ffi_gl_.BindFramebuffer(target, framebuffer);
        }
    }

    fn bind_texture(&self, target: GLenum, texture: GLuint) {
        unsafe {
            self.ffi_gl_.BindTexture(target, texture);
        }
    }

    fn draw_buffers(&self, bufs: &[GLenum]) {
        unsafe {
            self.ffi_gl_.DrawBuffers(bufs.len() as GLsizei, bufs.as_ptr());
        }
    }

    // FIXME: Does not verify buffer size -- unsafe!
    fn tex_image_2d(&self,
                    target: GLenum,
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
                    self.ffi_gl_.TexImage2D(target, level, internal_format, width, height, border, format, ty,
                                            data.as_ptr() as *const GLvoid);
                }
            }
            None => {
                unsafe {
                    self.ffi_gl_.TexImage2D(target, level, internal_format, width, height, border, format, ty,
                                            ptr::null());
                }
            }
        }
    }

    fn compressed_tex_image_2d(&self,
                               target: GLenum,
                               level: GLint,
                               internal_format: GLenum,
                               width: GLsizei,
                               height: GLsizei,
                               border: GLint,
                               data: &[u8]) {
        unsafe {
            self.ffi_gl_.CompressedTexImage2D(target, level, internal_format, width, height, border,
                                              data.len() as GLsizei, data.as_ptr() as *const GLvoid);
        }
    }

    fn compressed_tex_sub_image_2d(&self,
                                   target: GLenum,
                                   level: GLint,
                                   xoffset: GLint,
                                   yoffset: GLint,
                                   width: GLsizei,
                                   height: GLsizei,
                                   format: GLenum,
                                   data: &[u8]) {
        unsafe {
            self.ffi_gl_.CompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format,
                                                 data.len() as GLsizei, data.as_ptr() as *const GLvoid);
        }
    }

    // FIXME: Does not verify buffer size -- unsafe!
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
                    opt_data: Option<&[u8]>) {
        unsafe {
            let pdata = match opt_data {
                Some(data) => mem::transmute(data.as_ptr()),
                None => ptr::null(),
            };
            self.ffi_gl_.TexImage3D(target,
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

    fn copy_tex_image_2d(&self,
                         target: GLenum,
                         level: GLint,
                         internal_format: GLenum,
                         x: GLint,
                         y: GLint,
                         width: GLsizei,
                         height: GLsizei,
                         border: GLint) {
        unsafe {
            self.ffi_gl_.CopyTexImage2D(target,
                                        level,
                                        internal_format,
                                        x,
                                        y,
                                        width,
                                        height,
                                        border);
        }
    }

    fn copy_tex_sub_image_2d(&self,
                             target: GLenum,
                             level: GLint,
                             xoffset: GLint,
                             yoffset: GLint,
                             x: GLint,
                             y: GLint,
                             width: GLsizei,
                             height: GLsizei) {
        unsafe {
            self.ffi_gl_.CopyTexSubImage2D(target,
                                           level,
                                           xoffset,
                                           yoffset,
                                           x,
                                           y,
                                           width,
                                           height);
        }
    }

    fn copy_tex_sub_image_3d(&self,
                             target: GLenum,
                             level: GLint,
                             xoffset: GLint,
                             yoffset: GLint,
                             zoffset: GLint,
                             x: GLint,
                             y: GLint,
                             width: GLsizei,
                             height: GLsizei) {
        unsafe {
            self.ffi_gl_.CopyTexSubImage3D(target,
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

    fn tex_sub_image_2d(&self,
                        target: GLenum,
                        level: GLint,
                        xoffset: GLint,
                        yoffset: GLint,
                        width: GLsizei,
                        height: GLsizei,
                        format: GLenum,
                        ty: GLenum,
                        data: &[u8]) {
        unsafe {
            self.ffi_gl_.TexSubImage2D(target, level, xoffset, yoffset, width, height, format, ty, data.as_ptr() as *const c_void);
        }
    }

    fn tex_sub_image_2d_pbo(&self,
                            target: GLenum,
                            level: GLint,
                            xoffset: GLint,
                            yoffset: GLint,
                            width: GLsizei,
                            height: GLsizei,
                            format: GLenum,
                            ty: GLenum,
                            offset: usize) {
        unsafe {
            self.ffi_gl_.TexSubImage2D(target, level, xoffset, yoffset, width, height, format, ty, offset as *const c_void);
        }
    }

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
                        data: &[u8]) {
        unsafe {
            self.ffi_gl_.TexSubImage3D(target,
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
                            offset: usize) {
        unsafe {
            self.ffi_gl_.TexSubImage3D(target,
                                       level,
                                       xoffset,
                                       yoffset,
                                       zoffset,
                                       width,
                                       height,
                                       depth,
                                       format,
                                       ty,
                                       offset as *const c_void);
        }
    }

    fn tex_storage_2d(&self,
                      target: GLenum,
                      levels: GLint,
                      internal_format: GLenum,
                      width: GLsizei,
                      height: GLsizei) {
        if self.ffi_gl_.TexStorage2D.is_loaded() {
            unsafe {
                self.ffi_gl_.TexStorage2D(target,
                                          levels,
                                          internal_format,
                                          width,
                                          height);
            }
        }
    }

    fn tex_storage_3d(&self,
                      target: GLenum,
                      levels: GLint,
                      internal_format: GLenum,
                      width: GLsizei,
                      height: GLsizei,
                      depth: GLsizei) {
        if self.ffi_gl_.TexStorage3D.is_loaded() {
            unsafe {
                self.ffi_gl_.TexStorage3D(target,
                                          levels,
                                          internal_format,
                                          width,
                                          height,
                                          depth);
            }
        }
    }


    fn get_tex_image_into_buffer(&self,
                                 target: GLenum,
                                 level: GLint,
                                 format: GLenum,
                                 ty: GLenum,
                                 output: &mut [u8]) {
        unsafe {
            self.ffi_gl_.GetTexImage(target,
                                     level,
                                     format,
                                     ty,
                                     output.as_mut_ptr() as *mut _);
        }
    }

    fn invalidate_framebuffer(&self,
                              target: GLenum,
                              attachments: &[GLenum]) {
        if self.ffi_gl_.InvalidateFramebuffer.is_loaded() {
            unsafe {
                self.ffi_gl_.InvalidateFramebuffer(
                    target,
                    attachments.len() as GLsizei,
                    attachments.as_ptr(),
                );
            }
        }
    }

    fn invalidate_sub_framebuffer(&self,
                                  target: GLenum,
                                  attachments: &[GLenum],
                                  xoffset: GLint,
                                  yoffset: GLint,
                                  width: GLsizei,
                                  height: GLsizei) {
        if self.ffi_gl_.InvalidateSubFramebuffer.is_loaded() {
            unsafe {
                self.ffi_gl_.InvalidateSubFramebuffer(
                    target,
                    attachments.len() as GLsizei,
                    attachments.as_ptr(),
                    xoffset,
                    yoffset,
                    width,
                    height,
                );
            }
        }
    }

    #[inline]
    unsafe fn get_integer_v(&self, name: GLenum, result: &mut [GLint]) {
        assert!(!result.is_empty());
        self.ffi_gl_.GetIntegerv(name, result.as_mut_ptr());
    }

    #[inline]
    unsafe fn get_integer_64v(&self, name: GLenum, result: &mut [GLint64]) {
        assert!(!result.is_empty());
        self.ffi_gl_.GetInteger64v(name, result.as_mut_ptr());
    }

    #[inline]
    unsafe fn get_integer_iv(&self, name: GLenum, index: GLuint, result: &mut [GLint]) {
        assert!(!result.is_empty());
        self.ffi_gl_.GetIntegeri_v(name, index, result.as_mut_ptr());
    }

    #[inline]
    unsafe fn get_integer_64iv(&self, name: GLenum, index: GLuint, result: &mut [GLint64]) {
        assert!(!result.is_empty());
        self.ffi_gl_.GetInteger64i_v(name, index, result.as_mut_ptr());
    }

    #[inline]
    unsafe fn get_boolean_v(&self, name: GLenum, result: &mut [GLboolean]) {
        assert!(!result.is_empty());
        self.ffi_gl_.GetBooleanv(name, result.as_mut_ptr());
    }

    #[inline]
    unsafe fn get_float_v(&self, name: GLenum, result: &mut [GLfloat]) {
        assert!(!result.is_empty());
        self.ffi_gl_.GetFloatv(name, result.as_mut_ptr());
    }

    fn get_framebuffer_attachment_parameter_iv(&self, target: GLenum, attachment: GLenum, pname: GLenum) -> GLint {
        let mut result: GLint = 0;
        unsafe {
            self.ffi_gl_.GetFramebufferAttachmentParameteriv(target, attachment, pname, &mut result);
        }
        result
    }

    fn get_renderbuffer_parameter_iv(&self, target: GLenum, pname: GLenum) -> GLint {
        let mut result: GLint = 0;
        unsafe {
            self.ffi_gl_.GetRenderbufferParameteriv(target, pname, &mut result);
        }
        result
    }

    fn get_tex_parameter_iv(&self, target: GLenum, pname: GLenum) -> GLint {
        let mut result: GLint = 0;
        unsafe {
            self.ffi_gl_.GetTexParameteriv(target, pname, &mut result);
        }
        result
    }

    fn get_tex_parameter_fv(&self, target: GLenum, pname: GLenum) -> GLfloat {
        let mut result: GLfloat = 0.0;
        unsafe {
            self.ffi_gl_.GetTexParameterfv(target, pname, &mut result);
        }
        result
    }

    fn tex_parameter_i(&self, target: GLenum, pname: GLenum, param: GLint) {
        unsafe {
            self.ffi_gl_.TexParameteri(target, pname, param);
        }
    }

    fn tex_parameter_f(&self, target: GLenum, pname: GLenum, param: GLfloat) {
        unsafe {
            self.ffi_gl_.TexParameterf(target, pname, param);
        }
    }

    fn framebuffer_texture_2d(&self,
                              target: GLenum,
                              attachment: GLenum,
                              textarget: GLenum,
                              texture: GLuint,
                              level: GLint) {
        unsafe {
            self.ffi_gl_.FramebufferTexture2D(target, attachment, textarget, texture, level);
        }
    }

    fn framebuffer_texture_layer(&self,
                                 target: GLenum,
                                 attachment: GLenum,
                                 texture: GLuint,
                                 level: GLint,
                                 layer: GLint) {
        unsafe {
            self.ffi_gl_.FramebufferTextureLayer(target, attachment, texture, level, layer);
        }
    }

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
                        filter: GLenum) {
        unsafe {
            self.ffi_gl_.BlitFramebuffer(src_x0,
                                         src_y0,
                                         src_x1,
                                         src_y1,
                                         dst_x0,
                                         dst_y0,
                                         dst_x1,
                                         dst_y1,
                                         mask,
                                         filter);
        }
    }

    fn vertex_attrib_4f(&self,
                        index: GLuint,
                        x: GLfloat,
                        y: GLfloat,
                        z: GLfloat,
                        w: GLfloat) {
        unsafe {
            self.ffi_gl_.VertexAttrib4f(index, x, y, z, w)
        }
    }

    fn vertex_attrib_pointer_f32(&self,
                                 index: GLuint,
                                 size: GLint,
                                 normalized: bool,
                                 stride: GLsizei,
                                 offset: GLuint) {
        unsafe {
            self.ffi_gl_.VertexAttribPointer(index,
                                             size,
                                             ffi::FLOAT,
                                             normalized as GLboolean,
                                             stride,
                                             offset as *const GLvoid)
        }
    }

    fn vertex_attrib_pointer(&self,
                                 index: GLuint,
                                 size: GLint,
                                 type_: GLenum,
                                 normalized: bool,
                                 stride: GLsizei,
                                 offset: GLuint) {
        unsafe {
            self.ffi_gl_.VertexAttribPointer(index,
                                             size,
                                             type_,
                                             normalized as GLboolean,
                                             stride,
                                             offset as *const GLvoid)
        }
    }

    fn vertex_attrib_i_pointer(&self,
                               index: GLuint,
                               size: GLint,
                               type_: GLenum,
                               stride: GLsizei,
                               offset: GLuint) {
        unsafe {
            self.ffi_gl_.VertexAttribIPointer(index,
                                              size,
                                              type_,
                                              stride,
                                              offset as *const GLvoid)
        }
    }

    fn vertex_attrib_divisor(&self, index: GLuint, divisor: GLuint) {
        unsafe {
            self.ffi_gl_.VertexAttribDivisor(index, divisor)
        }
    }

    fn viewport(&self, x: GLint, y: GLint, width: GLsizei, height: GLsizei) {
        unsafe {
            self.ffi_gl_.Viewport(x, y, width, height);
        }
    }

    fn scissor(&self, x: GLint, y: GLint, width: GLsizei, height: GLsizei) {
        unsafe {
            self.ffi_gl_.Scissor(x, y, width, height);
        }
    }

    fn line_width(&self, width: GLfloat) {
        unsafe {
            self.ffi_gl_.LineWidth(width);
        }
    }

    fn use_program(&self, program: GLuint) {
        unsafe {
            self.ffi_gl_.UseProgram(program);
        }
    }

    fn validate_program(&self, program: GLuint) {
        unsafe {
            self.ffi_gl_.ValidateProgram(program);
        }
    }

    fn draw_arrays(&self, mode: GLenum, first: GLint, count: GLsizei) {
        unsafe {
            return self.ffi_gl_.DrawArrays(mode, first, count);
        }
    }

    fn draw_arrays_instanced(&self, mode: GLenum, first: GLint, count: GLsizei, primcount: GLsizei) {
        unsafe {
            return self.ffi_gl_.DrawArraysInstanced(mode, first, count, primcount);
        }
    }

    fn draw_elements(&self, mode: GLenum, count: GLsizei, element_type: GLenum, indices_offset: GLuint) {
        unsafe {
            return self.ffi_gl_.DrawElements(mode, count, element_type, indices_offset as *const c_void)
        }
    }

    fn draw_elements_instanced(&self,
                                   mode: GLenum,
                                   count: GLsizei,
                                   element_type: GLenum,
                                   indices_offset: GLuint,
                                   primcount: GLsizei) {
        unsafe {
            return self.ffi_gl_.DrawElementsInstanced(mode,
                                                      count,
                                                      element_type,
                                                      indices_offset as *const c_void,
                                                      primcount)
        }
    }

    fn blend_color(&self, r: f32, g: f32, b: f32, a: f32) {
        unsafe {
            self.ffi_gl_.BlendColor(r, g, b, a);
        }
    }

    fn blend_func(&self, sfactor: GLenum, dfactor: GLenum) {
        unsafe {
            self.ffi_gl_.BlendFunc(sfactor, dfactor);
        }
    }

    fn blend_func_separate(&self, src_rgb: GLenum, dest_rgb: GLenum, src_alpha: GLenum, dest_alpha: GLenum) {
        unsafe {
            self.ffi_gl_.BlendFuncSeparate(src_rgb, dest_rgb, src_alpha, dest_alpha);
        }
    }

    fn blend_equation(&self, mode: GLenum) {
        unsafe {
            self.ffi_gl_.BlendEquation(mode);
        }
    }

    fn blend_equation_separate(&self, mode_rgb: GLenum, mode_alpha: GLenum) {
        unsafe {
            self.ffi_gl_.BlendEquationSeparate(mode_rgb, mode_alpha);
        }
    }

    fn color_mask(&self, r: bool, g: bool, b: bool, a: bool) {
        unsafe {
            self.ffi_gl_.ColorMask(r as GLboolean, g as GLboolean, b as GLboolean, a as GLboolean);
        }
    }

    fn cull_face(&self, mode: GLenum) {
        unsafe {
            self.ffi_gl_.CullFace(mode);
        }
    }

    fn front_face(&self, mode: GLenum) {
        unsafe {
            self.ffi_gl_.FrontFace(mode);
        }
    }

    fn enable(&self, cap: GLenum) {
        unsafe {
            self.ffi_gl_.Enable(cap);
        }
    }

    fn disable(&self, cap: GLenum) {
        unsafe {
            self.ffi_gl_.Disable(cap);
        }
    }

    fn hint(&self, param_name: GLenum, param_val: GLenum) {
        unsafe {
            self.ffi_gl_.Hint(param_name, param_val);
        }
    }

    fn is_enabled(&self, cap: GLenum) -> GLboolean {
        unsafe {
            self.ffi_gl_.IsEnabled(cap)
        }
    }

    fn is_shader(&self, shader: GLuint) -> GLboolean {
        unsafe {
            self.ffi_gl_.IsShader(shader)
        }
    }

    fn is_texture(&self, texture: GLenum) -> GLboolean {
        unsafe {
            self.ffi_gl_.IsTexture(texture)
        }
    }

    fn is_framebuffer(&self, framebuffer: GLenum) -> GLboolean {
        unsafe {
            self.ffi_gl_.IsFramebuffer(framebuffer)
        }
    }

    fn is_renderbuffer(&self, renderbuffer: GLenum) -> GLboolean {
        unsafe {
            self.ffi_gl_.IsRenderbuffer(renderbuffer)
        }
    }

    fn check_frame_buffer_status(&self, target: GLenum) -> GLenum {
        unsafe {
            self.ffi_gl_.CheckFramebufferStatus(target)
        }
    }

    fn enable_vertex_attrib_array(&self, index: GLuint) {
        unsafe {
            self.ffi_gl_.EnableVertexAttribArray(index);
        }
    }

    fn disable_vertex_attrib_array(&self, index: GLuint) {
        unsafe {
            self.ffi_gl_.DisableVertexAttribArray(index);
        }
    }

    fn uniform_1f(&self, location: GLint, v0: GLfloat) {
        unsafe {
            self.ffi_gl_.Uniform1f(location, v0);
        }
    }

    fn uniform_1fv(&self, location: GLint, values: &[f32]) {
        unsafe {
            self.ffi_gl_.Uniform1fv(location,
                                    values.len() as GLsizei,
                                    values.as_ptr());
        }
    }

    fn uniform_1i(&self, location: GLint, v0: GLint) {
        unsafe {
            self.ffi_gl_.Uniform1i(location, v0);
        }
    }

    fn uniform_1iv(&self, location: GLint, values: &[i32]) {
        unsafe {
            self.ffi_gl_.Uniform1iv(location,
                                    values.len() as GLsizei,
                                    values.as_ptr());
        }
    }

    fn uniform_1ui(&self, location: GLint, v0: GLuint) {
        unsafe {
            self.ffi_gl_.Uniform1ui(location, v0);
        }
    }

    fn uniform_2f(&self, location: GLint, v0: GLfloat, v1: GLfloat) {
        unsafe {
            self.ffi_gl_.Uniform2f(location, v0, v1);
        }
    }

    fn uniform_2fv(&self, location: GLint, values: &[f32]) {
        unsafe {
            self.ffi_gl_.Uniform2fv(location,
                                    (values.len() / 2) as GLsizei,
                                    values.as_ptr());
        }
    }

    fn uniform_2i(&self, location: GLint, v0: GLint, v1: GLint) {
        unsafe {
            self.ffi_gl_.Uniform2i(location, v0, v1);
        }
    }

    fn uniform_2iv(&self, location: GLint, values: &[i32]) {
        unsafe {
            self.ffi_gl_.Uniform2iv(location,
                                    (values.len() / 2) as GLsizei,
                                    values.as_ptr());
        }
    }

    fn uniform_2ui(&self, location: GLint, v0: GLuint, v1: GLuint) {
        unsafe {
            self.ffi_gl_.Uniform2ui(location, v0, v1);
        }
    }

    fn uniform_3f(&self, location: GLint, v0: GLfloat, v1: GLfloat, v2: GLfloat) {
        unsafe {
            self.ffi_gl_.Uniform3f(location, v0, v1, v2);
        }
    }

    fn uniform_3fv(&self, location: GLint, values: &[f32]) {
        unsafe {
            self.ffi_gl_.Uniform3fv(location,
                                    (values.len() / 3) as GLsizei,
                                    values.as_ptr());
        }
    }

    fn uniform_3i(&self, location: GLint, v0: GLint, v1: GLint, v2: GLint) {
        unsafe {
            self.ffi_gl_.Uniform3i(location, v0, v1, v2);
        }
    }

    fn uniform_3iv(&self, location: GLint, values: &[i32]) {
        unsafe {
            self.ffi_gl_.Uniform3iv(location,
                                    (values.len() / 3) as GLsizei,
                                    values.as_ptr());
        }
    }

    fn uniform_3ui(&self, location: GLint, v0: GLuint, v1: GLuint, v2: GLuint) {
        unsafe {
            self.ffi_gl_.Uniform3ui(location, v0, v1, v2);
        }
    }

    fn uniform_4f(&self, location: GLint, x: GLfloat, y: GLfloat, z: GLfloat, w: GLfloat) {
        unsafe {
            self.ffi_gl_.Uniform4f(location, x, y, z, w);
        }
    }

    fn uniform_4i(&self, location: GLint, x: GLint, y: GLint, z: GLint, w: GLint) {
        unsafe {
            self.ffi_gl_.Uniform4i(location, x, y, z, w);
        }
    }

    fn uniform_4iv(&self, location: GLint, values: &[i32]) {
        unsafe {
            self.ffi_gl_.Uniform4iv(location,
                                    (values.len() / 4) as GLsizei,
                                    values.as_ptr());
        }
    }

    fn uniform_4ui(&self, location: GLint, x: GLuint, y: GLuint, z: GLuint, w: GLuint) {
        unsafe {
            self.ffi_gl_.Uniform4ui(location, x, y, z, w);
        }
    }

    fn uniform_4fv(&self, location: GLint, values: &[f32]) {
        unsafe {
            self.ffi_gl_.Uniform4fv(location,
                                    (values.len() / 4) as GLsizei,
                                    values.as_ptr());
        }
    }

    fn uniform_matrix_2fv(&self, location: GLint, transpose: bool, value: &[f32]) {
        unsafe {
            self.ffi_gl_.UniformMatrix2fv(location,
                                          (value.len() / 4) as GLsizei,
                                          transpose as GLboolean,
                                          value.as_ptr());
        }
    }

    fn uniform_matrix_3fv(&self, location: GLint, transpose: bool, value: &[f32]) {
        unsafe {
            self.ffi_gl_.UniformMatrix3fv(location,
                                          (value.len() / 9) as GLsizei,
                                          transpose as GLboolean,
                                          value.as_ptr());
        }
    }

    fn uniform_matrix_4fv(&self, location: GLint, transpose: bool, value: &[f32]) {
        unsafe {
            self.ffi_gl_.UniformMatrix4fv(location,
                                          (value.len() / 16) as GLsizei,
                                          transpose as GLboolean,
                                          value.as_ptr());
        }
    }

    fn depth_mask(&self, flag: bool) {
        unsafe {
            self.ffi_gl_.DepthMask(flag as GLboolean);
        }
    }

    fn depth_range(&self, near: f64, far: f64) {
        unsafe {
            self.ffi_gl_.DepthRange(near as GLclampd, far as GLclampd);
        }
    }

    fn get_active_attrib(&self, program: GLuint, index: GLuint) -> (i32, u32, String) {
        let mut buf_size = [0];
        unsafe {
            self.get_program_iv(program, ffi::ACTIVE_ATTRIBUTE_MAX_LENGTH, &mut buf_size);
        }
        let mut name = vec![0u8; buf_size[0] as usize];
        let mut length = 0 as GLsizei;
        let mut size = 0 as i32;
        let mut type_ = 0 as u32;
        unsafe {
            self.ffi_gl_.GetActiveAttrib(
                program,
                index,
                buf_size[0],
                &mut length,
                &mut size,
                &mut type_,
                name.as_mut_ptr() as *mut GLchar,
            );
        }
        name.truncate(if length > 0 {length as usize} else {0});
        (size, type_, String::from_utf8(name).unwrap())
    }

    fn get_active_uniform(&self, program: GLuint, index: GLuint) -> (i32, u32, String) {
        let mut buf_size = [0];
        unsafe {
            self.get_program_iv(program, ffi::ACTIVE_UNIFORM_MAX_LENGTH, &mut buf_size);
        }
        let mut name = vec![0 as u8; buf_size[0] as usize];
        let mut length: GLsizei = 0;
        let mut size: i32 = 0;
        let mut type_: u32 = 0;

        unsafe {
            self.ffi_gl_.GetActiveUniform(
                program,
                index,
                buf_size[0],
                &mut length,
                &mut size,
                &mut type_,
                name.as_mut_ptr() as *mut GLchar,
            );
        }

        name.truncate(if length > 0 { length as usize } else { 0 });

        (size, type_, String::from_utf8(name).unwrap())
    }

    fn get_active_uniforms_iv(&self, program: GLuint, indices: Vec<GLuint>, pname: GLenum) -> Vec<GLint> {
        let mut result = Vec::with_capacity(indices.len());
        unsafe {
            result.set_len(indices.len());
            self.ffi_gl_.GetActiveUniformsiv(program,
                                             indices.len() as GLsizei,
                                             indices.as_ptr(),
                                             pname,
                                             result.as_mut_ptr());
        }
        result
    }

    fn get_active_uniform_block_i(&self, program: GLuint, index: GLuint, pname: GLenum) -> GLint {
        let mut result = 0;
        unsafe {
            self.ffi_gl_.GetActiveUniformBlockiv(program, index, pname, &mut result);
        }
        result
    }

    fn get_active_uniform_block_iv(&self, program: GLuint, index: GLuint, pname: GLenum) -> Vec<GLint> {
        let count = self.get_active_uniform_block_i(program, index, ffi::UNIFORM_BLOCK_ACTIVE_UNIFORMS);
        let mut result = Vec::with_capacity(count as usize);
        unsafe {
            result.set_len(count as usize);
            self.ffi_gl_.GetActiveUniformBlockiv(program, index, pname, result.as_mut_ptr());
        }
        result
    }

    fn get_active_uniform_block_name(&self, program: GLuint, index: GLuint) -> String {
        let buf_size = self.get_active_uniform_block_i(program, index, ffi::UNIFORM_BLOCK_NAME_LENGTH);
        let mut name = vec![0 as u8; buf_size as usize];
        let mut length: GLsizei = 0;
        unsafe {
            self.ffi_gl_.GetActiveUniformBlockName(program,
                                                   index,
                                                   buf_size,
                                                   &mut length,
                                                   name.as_mut_ptr() as *mut GLchar);
        }
        name.truncate(if length > 0 { length as usize } else { 0 });

        String::from_utf8(name).unwrap()
    }

    fn get_attrib_location(&self, program: GLuint, name: &str) -> c_int {
        let name = CString::new(name).unwrap();
        unsafe {
            self.ffi_gl_.GetAttribLocation(program, name.as_ptr())
        }
    }

    fn get_frag_data_location(&self, program: GLuint, name: &str) -> c_int {
        let name = CString::new(name).unwrap();
        unsafe {
            self.ffi_gl_.GetFragDataLocation(program, name.as_ptr())
        }
    }

    fn get_uniform_location(&self, program: GLuint, name: &str) -> c_int {
        let name = CString::new(name).unwrap();
        unsafe {
            self.ffi_gl_.GetUniformLocation(program, name.as_ptr())
        }
    }

    fn get_program_info_log(&self, program: GLuint) -> String {
        let mut max_len = [0];
        unsafe {
            self.get_program_iv(program, ffi::INFO_LOG_LENGTH, &mut max_len);
        }
        let mut result = vec![0u8; max_len[0] as usize];
        let mut result_len = 0 as GLsizei;
        unsafe {
            self.ffi_gl_.GetProgramInfoLog(
                program,
                max_len[0] as GLsizei,
                &mut result_len,
                result.as_mut_ptr() as *mut GLchar,
            );
        }
        result.truncate(if result_len > 0 {result_len as usize} else {0});
        String::from_utf8(result).unwrap()
    }

    #[inline]
    unsafe fn get_program_iv(&self, program: GLuint, pname: GLenum, result: &mut [GLint]) {
        assert!(!result.is_empty());
        self.ffi_gl_.GetProgramiv(program, pname, result.as_mut_ptr());
    }

    fn get_program_binary(&self, program: GLuint) -> (Vec<u8>, GLenum) {
        if !self.ffi_gl_.GetProgramBinary.is_loaded() {
            return (Vec::new(), NONE);
        }
        let mut len = [0];
        unsafe {
            self.get_program_iv(program, ffi::PROGRAM_BINARY_LENGTH, &mut len);
        }
        if len[0] <= 0 {
            return (Vec::new(), NONE);
        }
        let mut binary: Vec<u8> = Vec::with_capacity(len[0] as usize);
        let mut format = NONE;
        let mut out_len = 0;
        unsafe {
            binary.set_len(len[0] as usize);
            self.ffi_gl_.GetProgramBinary(
                program,
                len[0],
                &mut out_len as *mut GLsizei,
                &mut format,
                binary.as_mut_ptr() as *mut c_void,
            );
        }
        if len[0] != out_len {
            return (Vec::new(), NONE);
        }

        (binary, format)
    }

    fn program_binary(&self, program: GLuint, format: GLenum, binary: &[u8]) {
        if !self.ffi_gl_.ProgramBinary.is_loaded() {
            return;
        }
        unsafe {
            self.ffi_gl_.ProgramBinary(program,
                                       format,
                                       binary.as_ptr() as *const c_void,
                                       binary.len() as GLsizei);
        }
    }

    fn program_parameter_i(&self, program: GLuint, pname: GLenum, value: GLint) {
        if !self.ffi_gl_.ProgramParameteri.is_loaded() {
            return;
        }
        unsafe {
            self.ffi_gl_.ProgramParameteri(program, pname, value);
        }
    }

    #[inline]
    unsafe fn get_vertex_attrib_iv(&self, index: GLuint, pname: GLenum, result: &mut [GLint]) {
        assert!(!result.is_empty());
        self.ffi_gl_.GetVertexAttribiv(index, pname, result.as_mut_ptr());
    }

    #[inline]
    unsafe fn get_vertex_attrib_fv(&self, index: GLuint, pname: GLenum, result: &mut [GLfloat]) {
        assert!(!result.is_empty());
        self.ffi_gl_.GetVertexAttribfv(index, pname, result.as_mut_ptr());
    }

    fn get_vertex_attrib_pointer_v(&self, index: GLuint, pname: GLenum) -> GLsizeiptr {
        let mut result = 0 as *mut GLvoid;
        unsafe {
            self.ffi_gl_.GetVertexAttribPointerv(index, pname, &mut result)
        }
        result as GLsizeiptr
    }

    fn get_buffer_parameter_iv(&self, target: GLuint, pname: GLenum) -> GLint {
        let mut result = 0 as GLint;
        unsafe {
            self.ffi_gl_.GetBufferParameteriv(target, pname, &mut result);
        }
        result
    }

    fn get_shader_info_log(&self, shader: GLuint) -> String {
        let mut max_len = [0];
        unsafe {
            self.get_shader_iv(shader, ffi::INFO_LOG_LENGTH, &mut max_len);
        }
        let mut result = vec![0u8; max_len[0] as usize];
        let mut result_len = 0 as GLsizei;
        unsafe {
            self.ffi_gl_.GetShaderInfoLog(
                shader,
                max_len[0] as GLsizei,
                &mut result_len,
                result.as_mut_ptr() as *mut GLchar,
            );
        }
        result.truncate(if result_len > 0 {result_len as usize} else {0});
        String::from_utf8(result).unwrap()
    }

    fn get_string(&self, which: GLenum) -> String {
        unsafe {
            let llstr = self.ffi_gl_.GetString(which);
            if !llstr.is_null() {
                return str::from_utf8_unchecked(CStr::from_ptr(llstr as *const c_char).to_bytes()).to_string();
            } else {
                return "".to_string();
            }
        }
    }

    fn get_string_i(&self, which: GLenum, index: GLuint) -> String {
        unsafe {
            let llstr = self.ffi_gl_.GetStringi(which, index);
            if !llstr.is_null() {
                str::from_utf8_unchecked(CStr::from_ptr(llstr as *const c_char).to_bytes()).to_string()
            } else {
                "".to_string()
            }
        }
    }

    unsafe fn get_shader_iv(&self, shader: GLuint, pname: GLenum, result: &mut [GLint]) {
        assert!(!result.is_empty());
        self.ffi_gl_.GetShaderiv(shader, pname, result.as_mut_ptr());
    }

    fn get_shader_precision_format(&self, _shader_type: GLuint, precision_type: GLuint) -> (GLint, GLint, GLint) {
        // gl.GetShaderPrecisionFormat is not available until OpenGL 4.1.
        // Fallback to OpenGL standard precissions that most desktop hardware support.
        match precision_type {
            ffi::LOW_FLOAT | ffi::MEDIUM_FLOAT | ffi::HIGH_FLOAT => {
                // Fallback to IEEE 754 single precision
                // Range: from -2^127 to 2^127
                // Significand precision: 23 bits
                (127, 127, 23)
            },
            ffi::LOW_INT | ffi::MEDIUM_INT | ffi::HIGH_INT => {
                // Fallback to single precision integer
                // Range: from -2^24 to 2^24
                // Precision: For integer formats this value is always 0
                (24, 24, 0)
            },
            _ => {
                (0, 0, 0)
            }
        }
    }

    fn compile_shader(&self, shader: GLuint) {
        unsafe {
            self.ffi_gl_.CompileShader(shader);
        }
    }

    fn create_program(&self) -> GLuint {
        unsafe {
            return self.ffi_gl_.CreateProgram();
        }
    }

    fn delete_program(&self, program: GLuint) {
        unsafe {
            self.ffi_gl_.DeleteProgram(program);
        }
    }

    fn create_shader(&self, shader_type: GLenum) -> GLuint {
        unsafe {
            return self.ffi_gl_.CreateShader(shader_type);
        }
    }

    fn delete_shader(&self, shader: GLuint) {
        unsafe {
            self.ffi_gl_.DeleteShader(shader);
        }
    }

    fn detach_shader(&self, program: GLuint, shader: GLuint) {
        unsafe {
            self.ffi_gl_.DetachShader(program, shader);
        }
    }

    fn link_program(&self, program: GLuint) {
        unsafe {
            return self.ffi_gl_.LinkProgram(program);
        }
    }

    fn clear_color(&self, r: f32, g: f32, b: f32, a: f32) {
        unsafe {
            self.ffi_gl_.ClearColor(r, g, b, a);
        }
    }

    fn clear(&self, buffer_mask: GLbitfield) {
        unsafe {
            self.ffi_gl_.Clear(buffer_mask);
        }
    }

    fn clear_depth(&self, depth: f64) {
        unsafe {
            self.ffi_gl_.ClearDepth(depth as GLclampd);
        }
    }

    fn clear_stencil(&self, s: GLint) {
        unsafe {
            self.ffi_gl_.ClearStencil(s);
        }
    }

    fn flush(&self) {
        unsafe {
            self.ffi_gl_.Flush();
        }
    }

    fn finish(&self) {
        unsafe {
            self.ffi_gl_.Finish();
        }
    }

    fn get_error(&self) -> GLenum {
        unsafe {
            self.ffi_gl_.GetError()
        }
    }

    fn stencil_mask(&self, mask: GLuint) {
        unsafe {
            self.ffi_gl_.StencilMask(mask)
        }
    }

    fn stencil_mask_separate(&self, face: GLenum, mask: GLuint) {
        unsafe {
            self.ffi_gl_.StencilMaskSeparate(face, mask)
        }
    }

    fn stencil_func(&self,
                    func: GLenum,
                    ref_: GLint,
                    mask: GLuint) {
        unsafe {
            self.ffi_gl_.StencilFunc(func, ref_, mask)
        }
    }

    fn stencil_func_separate(&self,
                             face: GLenum,
                             func: GLenum,
                             ref_: GLint,
                             mask: GLuint) {
        unsafe {
            self.ffi_gl_.StencilFuncSeparate(face, func, ref_, mask)
        }
    }

    fn stencil_op(&self,
                  sfail: GLenum,
                  dpfail: GLenum,
                  dppass: GLenum) {
        unsafe {
            self.ffi_gl_.StencilOp(sfail, dpfail, dppass)
        }
    }

    fn stencil_op_separate(&self,
                           face: GLenum,
                           sfail: GLenum,
                           dpfail: GLenum,
                           dppass: GLenum) {
        unsafe {
            self.ffi_gl_.StencilOpSeparate(face, sfail, dpfail, dppass)
        }
    }

    #[allow(unused_variables)]
    fn egl_image_target_texture2d_oes(&self, target: GLenum, image: GLeglImageOES) {
        panic!("not supported")
    }

    fn generate_mipmap(&self, target: GLenum) {
        unsafe {
            self.ffi_gl_.GenerateMipmap(target)
        }
    }

    fn insert_event_marker_ext(&self, message: &str) {
        if self.ffi_gl_.InsertEventMarkerEXT.is_loaded() {
            unsafe {
                self.ffi_gl_.InsertEventMarkerEXT(message.len() as GLsizei, message.as_ptr() as *const _);
            }
        }
    }

    fn push_group_marker_ext(&self, message: &str) {
        if self.ffi_gl_.PushGroupMarkerEXT.is_loaded() {
            unsafe {
                self.ffi_gl_.PushGroupMarkerEXT(message.len() as GLsizei, message.as_ptr() as *const _);
            }
        }
    }

    fn pop_group_marker_ext(&self) {
        if self.ffi_gl_.PopGroupMarkerEXT.is_loaded() {
            unsafe {
                self.ffi_gl_.PopGroupMarkerEXT();
            }
        }
    }

    fn fence_sync(&self, condition: GLenum, flags: GLbitfield) -> GLsync {
        unsafe {
           self.ffi_gl_.FenceSync(condition, flags) as *const _
        }
    }

    fn client_wait_sync(&self, sync: GLsync, flags: GLbitfield, timeout: GLuint64) {
        unsafe {
            self.ffi_gl_.ClientWaitSync(sync as *const _, flags, timeout);
        }
    }

    fn wait_sync(&self, sync: GLsync, flags: GLbitfield, timeout: GLuint64) {
        unsafe {
            self.ffi_gl_.WaitSync(sync as *const _, flags, timeout);
        }
    }

    fn texture_range_apple(&self, target: GLenum, data: &[u8]) {
        unsafe {
            self.ffi_gl_.TextureRangeAPPLE(target, data.len() as GLsizei, data.as_ptr() as *const c_void);
        }
    }

    fn delete_sync(&self, sync: GLsync) {
        unsafe {
            self.ffi_gl_.DeleteSync(sync as *const _);
        }
    }

    fn gen_fences_apple(&self, n: GLsizei) -> Vec<GLuint> {
        let mut result = vec![0 as GLuint; n as usize];
        unsafe {
            self.ffi_gl_.GenFencesAPPLE(n, result.as_mut_ptr());
        }
        result
    }

    fn delete_fences_apple(&self, fences: &[GLuint]) {
        unsafe {
            self.ffi_gl_.DeleteFencesAPPLE(fences.len() as GLsizei, fences.as_ptr());
        }
    }

    fn set_fence_apple(&self, fence: GLuint) {
        unsafe {
            self.ffi_gl_.SetFenceAPPLE(fence);
        }
    }

    fn finish_fence_apple(&self, fence: GLuint) {
        unsafe {
            self.ffi_gl_.FinishFenceAPPLE(fence);
        }
    }

    fn test_fence_apple(&self, fence: GLuint) {
        unsafe {
            self.ffi_gl_.TestFenceAPPLE(fence);
        }
    }

    // GL_ARB_blend_func_extended
    fn bind_frag_data_location_indexed(
        &self,
        program: GLuint,
        color_number: GLuint,
        index: GLuint,
        name: &str,
    ) {
        if !self.ffi_gl_.BindFragDataLocationIndexed.is_loaded() {
            return;
        }

        let c_string = CString::new(name).unwrap();

        unsafe {
            self.ffi_gl_.BindFragDataLocationIndexed(
                program,
                color_number,
                index,
                c_string.as_ptr(),
            )
        }
    }

    fn get_frag_data_index(
        &self,
        program: GLuint,
        name: &str,
    ) -> GLint {
        if !self.ffi_gl_.GetFragDataIndex.is_loaded() {
            return -1;
        }

        let c_string = CString::new(name).unwrap();

        unsafe {
            self.ffi_gl_.GetFragDataIndex(
                program,
                c_string.as_ptr(),
            )
        }
    }

    // GL_KHR_debug
    fn get_debug_messages(&self) -> Vec<DebugMessage> {
        if !self.ffi_gl_.GetDebugMessageLog.is_loaded() {
            return Vec::new();
        }

        let mut max_message_len = 0;
        unsafe {
            self.ffi_gl_.GetIntegerv(
                ffi::MAX_DEBUG_MESSAGE_LENGTH,
                &mut max_message_len
            )
        }

        let mut output = Vec::new();
        const CAPACITY: usize = 4;

        let mut msg_data = vec![0u8; CAPACITY * max_message_len as usize];
        let mut sources = [0 as GLenum; CAPACITY];
        let mut types = [0 as GLenum; CAPACITY];
        let mut severities = [0 as GLenum; CAPACITY];
        let mut ids = [0 as GLuint; CAPACITY];
        let mut lengths = [0 as GLsizei; CAPACITY];

        loop {
            let count = unsafe {
                self.ffi_gl_.GetDebugMessageLog(
                    CAPACITY as _,
                    msg_data.len() as _,
                    sources.as_mut_ptr(),
                    types.as_mut_ptr(),
                    ids.as_mut_ptr(),
                    severities.as_mut_ptr(),
                    lengths.as_mut_ptr(),
                    msg_data.as_mut_ptr() as *mut _,
                )
            };

            let mut offset = 0;
            output.extend((0 .. count as usize).map(|i| {
                let len = lengths[i] as usize;
                let slice = &msg_data[offset .. offset + len];
                offset += len;
                DebugMessage {
                    message: String::from_utf8_lossy(slice).to_string(),
                    source: sources[i],
                    ty: types[i],
                    id: ids[i],
                    severity: severities[i],
                }
            }));

            if (count as usize) < CAPACITY {
                return output
            }
        }
    }
}

