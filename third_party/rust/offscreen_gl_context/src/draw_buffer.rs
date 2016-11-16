use euclid::Size2D;
use gleam::gl;
use gleam::gl::types::{GLuint, GLenum, GLint};

use GLContext;
use NativeGLContextMethods;

use std::ptr;

#[derive(Debug)]
pub enum ColorAttachmentType {
    Texture,
    Renderbuffer,
}

impl Default for ColorAttachmentType {
    fn default() -> ColorAttachmentType {
        ColorAttachmentType::Renderbuffer
    }
}


/// We either have a color renderbuffer
/// Or a surface bound to a texture
/// bound to a framebuffer as a color
/// attachment
#[derive(Debug)]
pub enum ColorAttachment {
    Renderbuffer(GLuint),
    Texture(GLuint),
}

impl ColorAttachment {
    pub fn color_attachment_type(&self) -> ColorAttachmentType {
        match *self {
            ColorAttachment::Renderbuffer(_) => ColorAttachmentType::Renderbuffer,
            ColorAttachment::Texture(_) => ColorAttachmentType::Texture,
        }
    }
}

impl Drop for ColorAttachment {
    fn drop(&mut self) {
        unsafe {
            match *self {
                ColorAttachment::Renderbuffer(mut id) => gl::DeleteRenderbuffers(1, &mut id),
                ColorAttachment::Texture(mut tex_id) => gl::DeleteTextures(1, &mut tex_id),
            }
        }
    }
}

/// This structure represents an offscreen context
/// draw buffer. It has a framebuffer, with at least
/// color renderbuffer (alpha or not). It may also have
/// a depth or stencil buffer, depending on context
/// requirements.
pub struct DrawBuffer {
    size: Size2D<i32>,
    framebuffer: GLuint,
    stencil_renderbuffer: GLuint,
    depth_renderbuffer: GLuint,
    color_attachment: Option<ColorAttachment>
    // samples: GLsizei,
}

/// Helper function to create a render buffer
/// TODO(ecoal95): We'll need to switch between `glRenderbufferStorage` and
///   `glRenderbufferStorageMultisample` when we support antialising
fn create_renderbuffer(format: GLenum,
                       size: &Size2D<i32>) -> GLuint {
    let mut ret: GLuint = 0;

    unsafe {
        gl::GenRenderbuffers(1, &mut ret);
        gl::BindRenderbuffer(gl::RENDERBUFFER, ret);
        gl::RenderbufferStorage(gl::RENDERBUFFER, format, size.width, size.height);
        gl::BindRenderbuffer(gl::RENDERBUFFER, 0);
    }

    ret
}

impl DrawBuffer {
    pub fn new<T: NativeGLContextMethods>(context: &GLContext<T>,
                                          mut size: Size2D<i32>,
                                          color_attachment_type: ColorAttachmentType)
                                          -> Result<DrawBuffer, &'static str>
    {
        const MIN_DRAWING_BUFFER_SIZE: i32 = 16;
        use std::cmp;

        let attrs = context.borrow_attributes();
        let capabilities = context.borrow_capabilities();

        debug!("Creating draw buffer {:?}, {:?}, attrs: {:?}, caps: {:?}",
               size, color_attachment_type, attrs, capabilities);

        if attrs.antialias && capabilities.max_samples == 0 {
            return Err("The given GLContext doesn't support requested antialising");
        }

        if attrs.preserve_drawing_buffer {
            return Err("preserveDrawingBuffer is not supported yet");
        }

        // See https://github.com/servo/servo/issues/12320
        size.width = cmp::max(MIN_DRAWING_BUFFER_SIZE, size.width);
        size.height = cmp::max(MIN_DRAWING_BUFFER_SIZE, size.height);

        let mut draw_buffer = DrawBuffer {
            size: size,
            framebuffer: 0,
            color_attachment: None,
            stencil_renderbuffer: 0,
            depth_renderbuffer: 0,
            // samples: 0,
        };

        try!(context.make_current());

        try!(draw_buffer.init(context, color_attachment_type));

        unsafe {
            debug_assert!(gl::CheckFramebufferStatus(gl::FRAMEBUFFER) == gl::FRAMEBUFFER_COMPLETE);
            debug_assert!(gl::get_error() == gl::NO_ERROR);
        }

        Ok(draw_buffer)
    }

    #[inline(always)]
    pub fn get_framebuffer(&self) -> GLuint {
        self.framebuffer
    }

    #[inline(always)]
    pub fn size(&self) -> Size2D<i32> {
        self.size
    }

    #[inline(always)]
    // NOTE: We unwrap here because after creation the draw buffer
    // always have a color attachment
    pub fn color_attachment_type(&self) -> ColorAttachmentType {
        self.color_attachment.as_ref().unwrap().color_attachment_type()
    }

    pub fn get_bound_color_renderbuffer_id(&self) -> Option<GLuint> {
        match self.color_attachment.as_ref().unwrap() {
            &ColorAttachment::Renderbuffer(id) => Some(id),
            _ => None,
        }
    }

    pub fn get_bound_texture_id(&self) -> Option<GLuint> {
        match self.color_attachment.as_ref().unwrap() {
            &ColorAttachment::Renderbuffer(_) => None,
            &ColorAttachment::Texture(id) => Some(id),
        }
    }
}

// NOTE: The initially associated GLContext MUST be the current gl context
// when drop is called. I know this is an important constraint.
// Right now there are no problems, if not, consider using a pointer to a
// parent with Rc<GLContext> and call make_current()
impl Drop for DrawBuffer {
    fn drop(&mut self) {
        unsafe {
            gl::DeleteFramebuffers(1, &mut self.framebuffer);

            // NOTE: Color renderbuffer is destroyed on drop of
            //   ColorAttachment
            let mut renderbuffers = [
                self.stencil_renderbuffer,
                self.depth_renderbuffer
            ];

            gl::DeleteRenderbuffers(2, renderbuffers.as_mut_ptr());
        }
    }
}

trait DrawBufferHelpers {
    fn init<T: NativeGLContextMethods>(&mut self,
                                       &GLContext<T>,
                                       color_attachment_type: ColorAttachmentType)
        -> Result<(), &'static str>;
    fn attach_to_framebuffer(&mut self)
        -> Result<(), &'static str>;
}

impl DrawBufferHelpers for DrawBuffer {
    fn init<T: NativeGLContextMethods>(&mut self,
                                       context: &GLContext<T>,
                                       color_attachment_type: ColorAttachmentType)
        -> Result<(), &'static str> {
        let attrs = context.borrow_attributes();
        let formats = context.borrow_formats();

        self.color_attachment = match color_attachment_type {
            ColorAttachmentType::Renderbuffer => {
                let color_renderbuffer = create_renderbuffer(formats.color_renderbuffer, &self.size);
                debug_assert!(color_renderbuffer != 0);

                Some(ColorAttachment::Renderbuffer(color_renderbuffer))
            },

            // TODO(ecoal95): Allow more customization of textures
            ColorAttachmentType::Texture => {
                let mut texture = 0;

                // TODO(ecoal95): Check gleam safe wrappers for these functions
                unsafe {
                    gl::GenTextures(1, &mut texture);
                    debug_assert!(texture != 0);

                    gl::BindTexture(gl::TEXTURE_2D, texture);
                    gl::TexImage2D(gl::TEXTURE_2D, 0,
                                   formats.texture_internal as GLint, self.size.width, self.size.height, 0, formats.texture, formats.texture_type, ptr::null_mut());

                    // Low filtering to allow rendering
                    gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MAG_FILTER, gl::NEAREST as GLint);
                    gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MIN_FILTER, gl::NEAREST as GLint);

                    // TODO(ecoal95): Check if these two are neccessary, probably not
                    gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_WRAP_S, gl::CLAMP_TO_EDGE as GLint);
                    gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_WRAP_T, gl::CLAMP_TO_EDGE as GLint);

                    gl::BindTexture(gl::TEXTURE_2D, 0);

                    debug_assert!(gl::get_error() == gl::NO_ERROR);

                    Some(ColorAttachment::Texture(texture))
                }
            },
        };

        // After this we check if we need stencil and depth buffers
        if attrs.depth {
            self.depth_renderbuffer = create_renderbuffer(formats.depth, &self.size);
            debug_assert!(self.depth_renderbuffer != 0);
        }

        if attrs.stencil {
            self.stencil_renderbuffer = create_renderbuffer(formats.stencil, &self.size);
            debug_assert!(self.stencil_renderbuffer != 0);
        }

        unsafe {
            gl::GenFramebuffers(1, &mut self.framebuffer);
            debug_assert!(self.framebuffer != 0);
        }

        // Finally we attach them to the framebuffer
        self.attach_to_framebuffer()
    }

    fn attach_to_framebuffer(&mut self) -> Result<(), &'static str> {
        unsafe {
            gl::BindFramebuffer(gl::FRAMEBUFFER, self.framebuffer);
            // NOTE: The assertion fails if the framebuffer is not bound
            debug_assert!(gl::IsFramebuffer(self.framebuffer) == gl::TRUE);

            match *self.color_attachment.as_ref().unwrap() {
                ColorAttachment::Renderbuffer(color_renderbuffer) => {
                    gl::FramebufferRenderbuffer(gl::FRAMEBUFFER,
                                                gl::COLOR_ATTACHMENT0,
                                                gl::RENDERBUFFER,
                                                color_renderbuffer);
                    debug_assert!(gl::IsRenderbuffer(color_renderbuffer) == gl::TRUE);
                },
                ColorAttachment::Texture(texture_id) => {
                    gl::FramebufferTexture2D(gl::FRAMEBUFFER,
                                             gl::COLOR_ATTACHMENT0,
                                             gl::TEXTURE_2D,
                                             texture_id, 0);
                },
            }

            if self.depth_renderbuffer != 0 {
                gl::FramebufferRenderbuffer(gl::FRAMEBUFFER,
                                            gl::DEPTH_ATTACHMENT,
                                            gl::RENDERBUFFER,
                                            self.depth_renderbuffer);
                debug_assert!(gl::IsRenderbuffer(self.depth_renderbuffer) == gl::TRUE);
            }

            if self.stencil_renderbuffer != 0 {
                gl::FramebufferRenderbuffer(gl::FRAMEBUFFER,
                                            gl::STENCIL_ATTACHMENT,
                                            gl::RENDERBUFFER,
                                            self.stencil_renderbuffer);
                debug_assert!(gl::IsRenderbuffer(self.stencil_renderbuffer) == gl::TRUE);
            }
        }

        Ok(())
    }
}
