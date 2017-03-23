use gleam::gl;
use euclid::Size2D;

use GLContext;
#[cfg(all(target_os = "linux", feature = "test_egl_in_linux"))]
use platform::with_egl::NativeGLContext;
#[cfg(feature="test_osmesa")]
use platform::OSMesaContext as NativeGLContext;
#[cfg(not(any(feature = "test_egl_in_linux", feature = "test_osmesa")))]
use NativeGLContext;
use NativeGLContextMethods;
use GLContextAttributes;
use ColorAttachmentType;
use std::thread;
use std::sync::mpsc;

#[cfg(target_os="macos")]
#[link(name="OpenGL", kind="framework")]
extern {}

#[cfg(all(target_os="linux", feature="x11"))]
#[link(name="GL")]
extern {}

fn test_gl_context<T: NativeGLContextMethods>(context: &GLContext<T>) {
    context.make_current().unwrap();

    context.gl().clear_color(1.0, 0.0, 0.0, 1.0);
    context.gl().clear(gl::COLOR_BUFFER_BIT);

    let size = context.draw_buffer_size().unwrap();

    let pixels = context.gl().read_pixels(0, 0, size.width, size.height, gl::RGBA, gl::UNSIGNED_BYTE);

    assert!(pixels.len() == (size.width * size.height * 4) as usize);
    test_pixels(&pixels);
}

fn test_pixels_eq(pixels: &[u8], to: &[u8]) {
    assert!(to.len() == 4);
    for pixel in pixels.chunks(4) {
        assert_eq!(pixel, to);
    }

}

fn test_pixels(pixels: &[u8]) {
    test_pixels_eq(pixels, &[255, 0, 0, 255]);
}

#[test]
#[cfg(not(feature = "test_osmesa"))]
fn test_unbinding() {
    let ctx = GLContext::<NativeGLContext>::new(Size2D::new(256, 256),
                                                GLContextAttributes::default(),
                                                ColorAttachmentType::Renderbuffer,
                                                gl::GlType::default(),
                                                None).unwrap();

    assert!(NativeGLContext::current_handle().is_some());

    ctx.unbind().unwrap();
    assert!(NativeGLContext::current_handle().is_none());
}

#[test]
fn test_renderbuffer_color_attachment() {
    test_gl_context(&GLContext::<NativeGLContext>::new(Size2D::new(256, 256),
                                                       GLContextAttributes::default(),
                                                       ColorAttachmentType::Renderbuffer,
                                                       gl::GlType::default(),
                                                       None).unwrap());
}

#[test]
fn test_texture_color_attachment() {
    let size = Size2D::new(256, 256);
    let context = GLContext::<NativeGLContext>::new(size,
                                                    GLContextAttributes::default(),
                                                    ColorAttachmentType::Texture,
                                                    gl::GlType::default(),
                                                    None).unwrap();
    test_gl_context(&context);

    // Get the bound texture and check we're painting on it
    let texture_id = context.borrow_draw_buffer().unwrap().get_bound_texture_id().unwrap();
    assert!(texture_id != 0);

    assert!(context.gl().get_error() == gl::NO_ERROR);

    // Actually we just check that writing to the framebuffer works, and that there's a texture
    // attached to it. Doing a getTexImage should be a good idea, but it's not available on gles,
    // so what we should do is rebinding to another FBO.
    //
    // This is done in the `test_sharing` test though, so if that passes we know everything
    // works and we're just happy.
    let vec = context.gl().read_pixels(0, 0, size.width, size.height, gl::RGBA, gl::UNSIGNED_BYTE);
    test_pixels(&vec);
}

#[test]
fn test_sharing() {
    let size = Size2D::new(256, 256);
    let primary = GLContext::<NativeGLContext>::new(size,
                                                    GLContextAttributes::default(),
                                                    ColorAttachmentType::Texture,
                                                    gl::GlType::default(),
                                                    None).unwrap();

    let primary_texture_id = primary.borrow_draw_buffer().unwrap().get_bound_texture_id().unwrap();
    assert!(primary_texture_id != 0);

    let secondary = GLContext::<NativeGLContext>::new(size,
                                                      GLContextAttributes::default(),
                                                      ColorAttachmentType::Texture,
                                                      gl::GlType::default(),
                                                      Some(&primary.handle())).unwrap();

    // Paint the second context red
    test_gl_context(&secondary);

    // Now the secondary context is bound, get the texture id, switch contexts, and check the
    // texture is there.
    let secondary_texture_id = secondary.borrow_draw_buffer().unwrap().get_bound_texture_id().unwrap();
    assert!(secondary_texture_id != 0);

    primary.make_current().unwrap();
    assert!(primary.gl().is_texture(secondary_texture_id) != 0);

    // Clearing and re-binding to a framebuffer instead of using getTexImage
    // since it's not available in GLES2
    primary.gl().clear_color(0.0, 0.0, 0.0, 1.0);
    primary.gl().clear(gl::COLOR_BUFFER_BIT);

    let vec = primary.gl().read_pixels(0, 0, size.width, size.height, gl::RGBA, gl::UNSIGNED_BYTE);
    test_pixels_eq(&vec, &[0, 0, 0, 255]);

    primary.gl().bind_texture(gl::TEXTURE_2D, secondary_texture_id);

    primary.gl().framebuffer_texture_2d(gl::FRAMEBUFFER,
                                        gl::COLOR_ATTACHMENT0,
                                        gl::TEXTURE_2D,
                                        secondary_texture_id, 0);

    let vec = primary.gl().read_pixels(0, 0, size.width, size.height, gl::RGBA, gl::UNSIGNED_BYTE);
    assert!(primary.gl().get_error() == gl::NO_ERROR);

    test_pixels(&vec);
}

#[test]
fn test_multithread_render() {
    let size = Size2D::new(256, 256);
    let primary = GLContext::<NativeGLContext>::new(size,
                                                    GLContextAttributes::default(),
                                                    ColorAttachmentType::Texture,
                                                    gl::GlType::default(),
                                                    None).unwrap();
    test_gl_context(&primary);
    let (tx, rx) = mpsc::channel();
    let (end_tx, end_rx) = mpsc::channel();
    thread::spawn(move ||{
        //create the context in a different thread
        let secondary = GLContext::<NativeGLContext>::new(size,
                                                          GLContextAttributes::default(),
                                                          ColorAttachmentType::Texture,
                                                          gl::GlType::default(),
                                                          None).unwrap();
        secondary.make_current().unwrap();
        assert!(secondary.is_current());
        //render green adn test pixels
        secondary.gl().clear_color(0.0, 1.0, 0.0, 1.0);
        secondary.gl().clear(gl::COLOR_BUFFER_BIT);

        let vec = secondary.gl().read_pixels(0, 0, size.width, size.height, gl::RGBA, gl::UNSIGNED_BYTE);
        test_pixels_eq(&vec, &[0, 255, 0, 255]);

        tx.send(()).unwrap();

        // Avoid drop until test ends
        end_rx.recv().unwrap();
    });
    // Wait until thread has drawn the texture
    rx.recv().unwrap();
    // This context must remain to be current in this thread
    assert!(primary.is_current());

    // The colors must remain unchanged
    let vec = primary.gl().read_pixels(0, 0, size.width, size.height, gl::RGBA, gl::UNSIGNED_BYTE);
    test_pixels_eq(&vec, &[255, 0, 0, 255]);

    end_tx.send(()).unwrap();
}

struct SGLUint(gl::GLuint);
unsafe impl Sync for SGLUint {}
unsafe impl Send for SGLUint {}

#[test]
fn test_multithread_sharing() {
    let size = Size2D::new(256, 256);
    let primary = GLContext::<NativeGLContext>::new(size,
                                                    GLContextAttributes::default(),
                                                    ColorAttachmentType::Texture,
                                                    gl::GlType::default(),
                                                    None).unwrap();
    primary.make_current().unwrap();

    let primary_texture_id = primary.borrow_draw_buffer().unwrap().get_bound_texture_id().unwrap();
    assert!(primary_texture_id != 0);

    let (tx, rx) = mpsc::channel();
    let (end_tx, end_rx) = mpsc::channel();
    let primary_handle = primary.handle();

    // Unbind required by some APIs as WGL
    primary.unbind().unwrap();

    thread::spawn(move || {
        // Create the context in a different thread
        let secondary = GLContext::<NativeGLContext>::new(size,
                                                      GLContextAttributes::default(),
                                                      ColorAttachmentType::Texture,
                                                      gl::GlType::default(),
                                                      Some(&primary_handle)).unwrap();
        // Make the context current on this thread only
        secondary.make_current().unwrap();
        // Paint the second context red
        test_gl_context(&secondary);
        // Send texture_id to main thread
        let texture_id = secondary.borrow_draw_buffer().unwrap().get_bound_texture_id().unwrap();
        assert!(texture_id != 0);
        tx.send(SGLUint(texture_id)).unwrap();
        // Avoid drop until test ends
        end_rx.recv().unwrap();
    });
    // Wait until thread has drawn the texture
    let secondary_texture_id = rx.recv().unwrap().0;

    primary.make_current().unwrap();

    // Clearing and re-binding to a framebuffer instead of using getTexImage
    // since it's not available in GLES2
    primary.gl().clear_color(0.0, 0.0, 0.0, 1.0);
    primary.gl().clear(gl::COLOR_BUFFER_BIT);

    let vec = primary.gl().read_pixels(0, 0, size.width, size.height, gl::RGBA, gl::UNSIGNED_BYTE);
    test_pixels_eq(&vec, &[0, 0, 0, 255]);


    primary.gl().framebuffer_texture_2d(gl::FRAMEBUFFER,
                                        gl::COLOR_ATTACHMENT0,
                                        gl::TEXTURE_2D,
                                        secondary_texture_id,
                                        0);

    let vec = primary.gl().read_pixels(0, 0, size.width, size.height, gl::RGBA, gl::UNSIGNED_BYTE);
    assert!(primary.gl().get_error() == gl::NO_ERROR);

    test_pixels(&vec);
    end_tx.send(()).unwrap();
}

#[test]
fn test_limits() {
    let size = Size2D::new(256, 256);
    let context = GLContext::<NativeGLContext>::new(size,
                                                    GLContextAttributes::default(),
                                                    ColorAttachmentType::Texture,
                                                    gl::GlType::default(),
                                                    None).unwrap();
    assert!(context.borrow_limits().max_vertex_attribs != 0);
}

#[test]
fn test_no_alpha() {
    let mut attributes = GLContextAttributes::default();
    attributes.alpha = false;

    let size = Size2D::new(256, 256);
    let context = GLContext::<NativeGLContext>::new(size,
                                                    attributes,
                                                    ColorAttachmentType::Texture,
                                                    gl::GlType::default(),
                                                    None).unwrap();
    assert!(context.borrow_limits().max_vertex_attribs != 0);
}

#[test]
fn test_no_depth() {
    let mut attributes = GLContextAttributes::default();
    attributes.depth = false;

    let size = Size2D::new(256, 256);
    let context = GLContext::<NativeGLContext>::new(size,
                                                    attributes,
                                                    ColorAttachmentType::Texture,
                                                    gl::GlType::default(),
                                                    None).unwrap();
    assert!(context.borrow_limits().max_vertex_attribs != 0);
}

#[test]
fn test_no_depth_no_alpha() {
    let mut attributes = GLContextAttributes::default();
    attributes.depth = false;
    attributes.alpha = false;

    let size = Size2D::new(256, 256);
    let context = GLContext::<NativeGLContext>::new(size,
                                                    attributes,
                                                    ColorAttachmentType::Texture,
                                                    gl::GlType::default(),
                                                    None).unwrap();
    assert!(context.borrow_limits().max_vertex_attribs != 0);
}

#[test]
fn test_no_premul_alpha() {
    let mut attributes = GLContextAttributes::default();
    attributes.depth = false;
    attributes.alpha = false;
    attributes.premultiplied_alpha = false;

    let size = Size2D::new(256, 256);
    let context = GLContext::<NativeGLContext>::new(size,
                                                    attributes,
                                                    ColorAttachmentType::Texture,
                                                    gl::GlType::default(),
                                                    None).unwrap();
    assert!(context.borrow_limits().max_vertex_attribs != 0);
}

#[test]
fn test_in_a_row() {
    let mut attributes = GLContextAttributes::default();
    attributes.depth = false;
    attributes.alpha = false;
    attributes.premultiplied_alpha = false;

    let size = Size2D::new(256, 256);
    let context = GLContext::<NativeGLContext>::new(size,
                                                    attributes.clone(),
                                                    ColorAttachmentType::Texture,
                                                    gl::GlType::default(),
                                                    None).unwrap();

    let handle = context.handle();

    GLContext::<NativeGLContext>::new(size,
                                      attributes.clone(),
                                      ColorAttachmentType::Texture,
                                      gl::GlType::default(),
                                      Some(&handle)).unwrap();

    GLContext::<NativeGLContext>::new(size,
                                      attributes.clone(),
                                      ColorAttachmentType::Texture,
                                      gl::GlType::default(),
                                      Some(&handle)).unwrap();
}

#[test]
fn test_zero_size() {
    GLContext::<NativeGLContext>::new(Size2D::new(0, 320),
                                      GLContextAttributes::default(),
                                      ColorAttachmentType::Texture,
                                      gl::GlType::default(),
                                      None).unwrap();
}
