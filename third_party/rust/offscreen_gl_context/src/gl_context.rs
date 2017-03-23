use euclid::Size2D;
use gleam::gl;
use gleam::gl::types::{GLuint};
use std::rc::Rc;

use NativeGLContextMethods;
use GLContextAttributes;
use GLContextCapabilities;
use GLFormats;
use GLLimits;
use DrawBuffer;
use ColorAttachmentType;

/// This is a wrapper over a native headless GL context
pub struct GLContext<Native> {
    gl_: Rc<gl::Gl>,
    native_context: Native,
    /// This an abstraction over a custom framebuffer
    /// with attachments according to WebGLContextAttributes
    // TODO(ecoal95): Ideally we may want a read and a draw
    // framebuffer, but this is not supported in GLES2, review
    // when we have better support
    draw_buffer: Option<DrawBuffer>,
    attributes: GLContextAttributes,
    capabilities: GLContextCapabilities,
    formats: GLFormats,
    limits: GLLimits,
}

impl<Native> GLContext<Native>
    where Native: NativeGLContextMethods,
{
    pub fn create(api_type: gl::GlType,
                  shared_with: Option<&Native::Handle>)
                  -> Result<Self, &'static str> {
        Self::create_shared_with_dispatcher(api_type, shared_with, None)
    }

    pub fn create_shared_with_dispatcher(api_type: gl::GlType,
                                         shared_with: Option<&Native::Handle>,
                                         dispatcher: Option<Box<GLContextDispatcher>>)
        -> Result<Self, &'static str> {
        let native_context = try!(Native::create_shared_with_dispatcher(shared_with, dispatcher));
        let gl_ = match api_type {
            gl::GlType::Gl => unsafe { gl::GlFns::load_with(|s| Self::get_proc_address(s) as *const _) },
            gl::GlType::Gles => unsafe { gl::GlesFns::load_with(|s| Self::get_proc_address(s) as *const _) },
        };

        try!(native_context.make_current());
        let attributes = GLContextAttributes::any();
        let formats = GLFormats::detect(&attributes);
        let limits = GLLimits::detect(&*gl_);

        Ok(GLContext {
            gl_: gl_,
            native_context: native_context,
            draw_buffer: None,
            attributes: attributes,
            capabilities: GLContextCapabilities::detect(),
            formats: formats,
            limits: limits,
        })
    }

    #[inline(always)]
    pub fn get_proc_address(addr: &str) -> *const () {
        Native::get_proc_address(addr)
    }

    #[inline(always)]
    pub fn current_handle() -> Option<Native::Handle> {
        Native::current_handle()
    }

    pub fn new(size: Size2D<i32>,
               attributes: GLContextAttributes,
               color_attachment_type: ColorAttachmentType,
               api_type: gl::GlType,
               shared_with: Option<&Native::Handle>)
        -> Result<Self, &'static str> {
        Self::new_shared_with_dispatcher(size,
                                         attributes,
                                         color_attachment_type,
                                         api_type,
                                         shared_with,
                                         None)
    }

    pub fn new_shared_with_dispatcher(size: Size2D<i32>,
                                      attributes: GLContextAttributes,
                                      color_attachment_type: ColorAttachmentType,
                                      api_type: gl::GlType,
                                      shared_with: Option<&Native::Handle>,
                                      dispatcher: Option<Box<GLContextDispatcher>>)
        -> Result<Self, &'static str> {
        // We create a headless context with a dummy size, we're painting to the
        // draw_buffer's framebuffer anyways.
        let mut context =
            try!(Self::create_shared_with_dispatcher(api_type,
                                                     shared_with,
                                                     dispatcher));

        context.formats = GLFormats::detect(&attributes);
        context.attributes = attributes;

        try!(context.init_offscreen(size, color_attachment_type));

        Ok(context)
    }

    #[inline(always)]
    pub fn with_default_color_attachment(size: Size2D<i32>,
                                         attributes: GLContextAttributes,
                                         api_type: gl::GlType,
                                         shared_with: Option<&Native::Handle>)
        -> Result<Self, &'static str> {
        Self::new(size, attributes, ColorAttachmentType::default(), api_type, shared_with)
    }

    #[inline(always)]
    pub fn make_current(&self) -> Result<(), &'static str> {
        self.native_context.make_current()
    }

    #[inline(always)]
    pub fn unbind(&self) -> Result<(), &'static str> {
        let ret = self.native_context.unbind();

        // OSMesa doesn't allow any API to unbind a context before [1], and just
        // bails out on null context, buffer, or whatever, so not much we can do
        // here. Thus, ignore the failure and just flush the context if we're
        // using an old OSMesa version.
        //
        // [1]: https://www.mail-archive.com/mesa-dev@lists.freedesktop.org/msg128408.html
        if self.native_context.is_osmesa() && ret.is_err() {
            self.gl().flush();
            return Ok(())
        }

        ret
    }

    #[inline(always)]
    pub fn is_current(&self) -> bool {
        self.native_context.is_current()
    }

    #[inline(always)]
    pub fn handle(&self) -> Native::Handle {
        self.native_context.handle()
    }

    pub fn gl(&self) -> &gl::Gl {
        &*self.gl_
    }

    pub fn clone_gl(&self) -> Rc<gl::Gl> {
        self.gl_.clone()
    }

    // Allow borrowing these unmutably
    pub fn borrow_attributes(&self) -> &GLContextAttributes {
        &self.attributes
    }

    pub fn borrow_capabilities(&self) -> &GLContextCapabilities {
        &self.capabilities
    }

    pub fn borrow_formats(&self) -> &GLFormats {
        &self.formats
    }

    pub fn borrow_limits(&self) -> &GLLimits {
        &self.limits
    }

    pub fn borrow_draw_buffer(&self) -> Option<&DrawBuffer> {
        self.draw_buffer.as_ref()
    }

    pub fn get_framebuffer(&self) -> GLuint {
        if let Some(ref db) = self.draw_buffer {
            return db.get_framebuffer();
        }

        let ret = self.gl().get_integer_v(gl::FRAMEBUFFER_BINDING);
        ret as GLuint
    }

    pub fn draw_buffer_size(&self) -> Option<Size2D<i32>> {
        self.draw_buffer.as_ref().map(|db| db.size())
    }

    // We resize just replacing the draw buffer, we don't perform size optimizations
    // in order to keep this generic
    pub fn resize(&mut self, size: Size2D<i32>) -> Result<(), &'static str> {
        if self.draw_buffer.is_some() {
            let color_attachment_type =
                self.borrow_draw_buffer().unwrap().color_attachment_type();
            self.init_offscreen(size, color_attachment_type)
        } else {
            Err("No DrawBuffer found")
        }
    }

    fn init_offscreen(&mut self, size: Size2D<i32>, color_attachment_type: ColorAttachmentType) -> Result<(), &'static str> {
        try!(self.create_draw_buffer(size, color_attachment_type));

        debug_assert!(self.is_current());

        self.gl().clear_color(0.0, 0.0, 0.0, 0.0);
        self.gl().clear(gl::COLOR_BUFFER_BIT | gl::DEPTH_BUFFER_BIT | gl::STENCIL_BUFFER_BIT);
        self.gl().scissor(0, 0, size.width, size.height);
        self.gl().viewport(0, 0, size.width, size.height);

        Ok(())
    }

    fn create_draw_buffer(&mut self, size: Size2D<i32>, color_attachment_type: ColorAttachmentType) -> Result<(), &'static str> {
        self.draw_buffer = Some(try!(DrawBuffer::new(self, size, color_attachment_type)));
        Ok(())
    }
}

// Dispatches functions to the thread where a NativeGLContext is bound.
// Right now it's used in the WGL implementation to dispatch functions to the thread
// where the context we share from is bound. See the WGL implementation for more details.
pub trait GLContextDispatcher {
    fn dispatch(&self, Box<Fn() + Send>);
}
