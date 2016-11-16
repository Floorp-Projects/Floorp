use std::mem;
use super::wgl_attributes::*;

use std::ffi::{CStr, CString, OsStr};
use std::os::raw::{c_void, c_int};
use std::os::windows::ffi::OsStrExt;
use std::io;
use std::ptr;

use winapi;
use kernel32;
use user32;
use gdi32;
use super::wgl;
use super::wgl_ext;


// #Attributions
// This WGL implementation has been inspired by the code originating in Glutin.
// We have used slightly modified version util functions to manage WGL contexts.
// We simplified the win32 window creation because we don't need the event handler thread
// We'd like to credit and thank all of the Glutin contributors for their work.
// (https://github.com/tomaka/glutin)

pub unsafe fn create_offscreen(shared_with: winapi::HGLRC,
                               settings: &WGLAttributes)
                               -> Result<(winapi::HGLRC, winapi::HDC), String> {
    let mut ctx = WGLScopedContext::default();
    ctx.window = try!(create_hidden_window());
    ctx.device_ctx = user32::GetDC(ctx.window);
    if ctx.device_ctx.is_null() {
        return Err("GetDC function failed".to_owned());
    }

    let extra = try!(load_extra_functions(ctx.window));

    let extensions = if extra.GetExtensionsStringARB.is_loaded() {
        let data = extra.GetExtensionsStringARB(ctx.device_ctx as *const _);
        let data = CStr::from_ptr(data).to_bytes().to_vec();
        String::from_utf8(data).unwrap()
    } else if extra.GetExtensionsStringEXT.is_loaded() {
        let data = extra.GetExtensionsStringEXT();
        let data = CStr::from_ptr(data).to_bytes().to_vec();
        String::from_utf8(data).unwrap()
    } else {
        String::new()
    };

    let (id, _) = if extensions.split(' ').find(|&i| i == "WGL_ARB_pixel_format").is_some() {
        try!(choose_arb_pixel_format(&extra, &extensions, ctx.device_ctx, &settings.pixel_format)
            .map_err(|_| "ARB pixel format not available".to_owned()))
    } else {
        try!(choose_native_pixel_format(ctx.device_ctx, &settings.pixel_format)
            .map_err(|_| "Native pixel format not available".to_owned()))
    };

    try!(set_pixel_format(ctx.device_ctx, id));

    let result = create_full_context(settings, &extra, &extensions, ctx.device_ctx, shared_with);
    if result.is_ok() {
       mem::forget(ctx); // Everything is ok, don't dispose the scoped ctx
    }
    result
}

// creates a basic context
unsafe fn create_basic_context(hdc: winapi::HDC,
                               share: winapi::HGLRC)
                               -> Result<(winapi::HGLRC, winapi::HDC), String> {
    let mut ctx = WGLScopedContext::default();
    ctx.render_ctx = wgl::CreateContext(hdc as *const _) as winapi::HGLRC;
    if ctx.render_ctx.is_null() {
        return Err(format!("wglCreateContext failed: {}", io::Error::last_os_error()));
    }

    if !share.is_null() {
        if wgl::ShareLists(share as *const _, ctx.render_ctx as *const _) == 0 {
            return Err(format!("wglShareLists failed: {}", io::Error::last_os_error()));
        }
    };

    let result = (ctx.render_ctx, hdc);
    mem::forget(ctx); // Everything is ok, don't dispose the scoped ctx
    Ok(result)
}

// creates a full context: attempts to use optional ext WGL functions
unsafe fn create_full_context(settings: &WGLAttributes,
                              extra: &wgl_ext::Wgl,
                              extensions: &str,
                              hdc: winapi::HDC,
                              share: winapi::HGLRC)
                              -> Result<(winapi::HGLRC, winapi::HDC), String> {
    let mut extensions = extensions.split(' ');
    if extensions.find(|&i| i == "WGL_ARB_create_context").is_none() {
        let ctx = create_basic_context(hdc, share);
        if let Ok(ctx) = ctx {
            if wgl::MakeCurrent(ctx.1 as *const _, ctx.0 as *const _) == 0  {
                return Err("wglMakeCurrent failed creating a basic context".to_owned());
            }
        }
        return ctx;
    }

    let mut attributes = Vec::new();
    if settings.opengl_es {
        if extensions.find(|&i| i == "WGL_EXT_create_context_es2_profile").is_some() {
            attributes.push(wgl_ext::CONTEXT_PROFILE_MASK_ARB as c_int);
            attributes.push(wgl_ext::CONTEXT_ES2_PROFILE_BIT_EXT as c_int);
        } else {
            return Err("OpenGl Version Not Supported".to_owned());
        }
    }

    if settings.major_version > 0 {
        attributes.push(wgl_ext::CONTEXT_MAJOR_VERSION_ARB as c_int);
        attributes.push(settings.major_version as c_int);
        attributes.push(wgl_ext::CONTEXT_MINOR_VERSION_ARB as c_int);
        attributes.push(settings.minor_version as c_int);
    }

    attributes.push(wgl_ext::CONTEXT_FLAGS_ARB as c_int);
    attributes.push((if settings.debug {
        wgl_ext::CONTEXT_FLAGS_ARB
    } else {
        0
    }) as c_int);

    attributes.push(0);

    let mut ctx = WGLScopedContext::default();
    ctx.render_ctx = extra.CreateContextAttribsARB(hdc as *const _,
                                            share as *const _,
                                            attributes.as_ptr()) as winapi::HGLRC;

    if ctx.render_ctx.is_null() {
        return Err(format!("wglCreateContextAttribsARB failed: {}",
                           io::Error::last_os_error()));
    }

    if wgl::MakeCurrent(hdc as *const _, ctx.render_ctx as *const _) == 0  {
        return Err("wglMakeCurrent failed creating full context".to_owned());
    }
    // Disable or enable vsync
    if extensions.find(|&i| i == "WGL_EXT_swap_control").is_some() {
        if extra.SwapIntervalEXT(if settings.vsync { 1 } else { 0 }) == 0 {
            return Err("wglSwapIntervalEXT failed".to_owned());
        }
    }
    let result = (ctx.render_ctx as winapi::HGLRC, hdc);
    mem::forget(ctx);
    Ok(result)
}

unsafe fn create_hidden_window() -> Result<winapi::HWND, &'static str> {
    let class_name = register_window_class();
    let mut rect = winapi::RECT {
        left: 0,
        right: 1024 as winapi::LONG,
        top: 0,
        bottom: 768 as winapi::LONG,
    };
    let ex_style = winapi::WS_EX_APPWINDOW | winapi::WS_EX_WINDOWEDGE;
    let style = winapi::WS_OVERLAPPEDWINDOW | winapi::WS_CLIPSIBLINGS | winapi::WS_CLIPCHILDREN;

    user32::AdjustWindowRectEx(&mut rect, style, 0, ex_style);
    let title = OsStr::new("WGLwindow")
        .encode_wide()
        .chain(Some(0).into_iter())
        .collect::<Vec<_>>();

    let win = user32::CreateWindowExW(ex_style,
                                      class_name.as_ptr(),
                                      title.as_ptr(),
                                      style,
                                      winapi::CW_USEDEFAULT,
                                      winapi::CW_USEDEFAULT,
                                      rect.right - rect.left,
                                      rect.bottom - rect.top,
                                      ptr::null_mut(),
                                      ptr::null_mut(),
                                      kernel32::GetModuleHandleW(ptr::null()),
                                      ptr::null_mut());
    if win.is_null() {
        return Err("CreateWindowEx function failed");
    }

    Ok(win)
}

// Helper struct used to dispose resources in error paths
struct WGLScopedContext {
    pub window: winapi::HWND,
    pub device_ctx: winapi::HDC,
    pub render_ctx: winapi::HGLRC,
}

impl Drop for WGLScopedContext {
    fn drop(&mut self) {
        unsafe {
            if !self.render_ctx.is_null() {
                wgl::DeleteContext(self.render_ctx as *const _);
            }
            if !self.device_ctx.is_null() && !self.window.is_null() {
                user32::ReleaseDC(self.window, self.device_ctx);
            }
            if !self.window.is_null() {
                user32::DestroyWindow(self.window);
            }
        }
    }
}

impl Default for WGLScopedContext {
    #[inline]
    fn default() -> WGLScopedContext {
        WGLScopedContext {
            window: ptr::null_mut(),
            device_ctx: ptr::null_mut(),
            render_ctx: ptr::null_mut(),
        }
    }
}

impl WGLScopedContext {
    fn new(window: winapi::HWND, device_ctx: winapi::HDC, render_ctx: winapi::HGLRC) -> WGLScopedContext {
        WGLScopedContext {
            window: window,
            device_ctx: device_ctx,
            render_ctx: render_ctx,
        }
    }
}

// ***********
// Utilities to ease WGL context creation
// Slightly modified versions of util functions taken from Glutin
// (https://github.com/tomaka/glutin)
// ***********

unsafe fn register_window_class() -> Vec<u16> {
    let class_name = OsStr::new("Window Class")
        .encode_wide()
        .chain(Some(0).into_iter())
        .collect::<Vec<_>>();

    let class = winapi::WNDCLASSEXW {
        cbSize: mem::size_of::<winapi::WNDCLASSEXW>() as winapi::UINT,
        style: winapi::CS_HREDRAW | winapi::CS_VREDRAW | winapi::CS_OWNDC,
        lpfnWndProc: Some(proc_callback),
        cbClsExtra: 0,
        cbWndExtra: 0,
        hInstance: kernel32::GetModuleHandleW(ptr::null()),
        hIcon: ptr::null_mut(),
        hCursor: ptr::null_mut(), // must be null in order for cursor state to work properly
        hbrBackground: ptr::null_mut(),
        lpszMenuName: ptr::null(),
        lpszClassName: class_name.as_ptr(),
        hIconSm: ptr::null_mut(),
    };

    // We ignore errors because registering the same window class twice would trigger
    //  an error, and because errors here are detected during CreateWindowEx anyway.
    // Also since there is no weird element in the struct, there is no reason for this
    //  call to fail.
    user32::RegisterClassExW(&class);

    class_name
}

pub unsafe extern "system" fn proc_callback(window: winapi::HWND,
                                            msg: winapi::UINT,
                                            wparam: winapi::WPARAM,
                                            lparam: winapi::LPARAM)
                                            -> winapi::LRESULT {
    match msg {
        winapi::WM_PAINT => 0,
        winapi::WM_ERASEBKGND => 0,
        _ => user32::DefWindowProcW(window, msg, wparam, lparam),
    }
}

#[derive(Debug, Clone)]
pub struct PixelFormat {
    pub hardware_accelerated: bool,
    pub color_bits: u8,
    pub alpha_bits: u8,
    pub depth_bits: u8,
    pub stencil_bits: u8,
    pub stereoscopy: bool,
    pub double_buffer: bool,
    pub multisampling: Option<u16>,
    pub srgb: bool,
}

unsafe fn choose_arb_pixel_format(extra: &wgl_ext::Wgl,
                                  extensions: &str,
                                  hdc: winapi::HDC,
                                  reqs: &WGLPixelFormat)
                                  -> Result<(c_int, PixelFormat), ()> {
    let descriptor = {
        let mut out: Vec<c_int> = Vec::with_capacity(37);

        out.push(wgl_ext::DRAW_TO_WINDOW_ARB as c_int);
        out.push(1);

        out.push(wgl_ext::SUPPORT_OPENGL_ARB as c_int);
        out.push(1);

        out.push(wgl_ext::PIXEL_TYPE_ARB as c_int);
        if reqs.float_color_buffer {
            if extensions.split(' ').find(|&i| i == "WGL_ARB_pixel_format_float").is_some() {
                out.push(wgl_ext::TYPE_RGBA_FLOAT_ARB as c_int);
            } else {
                return Err(());
            }
        } else {
            out.push(wgl_ext::TYPE_RGBA_ARB as c_int);
        }

        // Force hardware aceleration
        out.push(wgl_ext::ACCELERATION_ARB as c_int);
        out.push(wgl_ext::FULL_ACCELERATION_ARB as c_int);

        if let Some(color) = reqs.color_bits {
            out.push(wgl_ext::COLOR_BITS_ARB as c_int);
            out.push(color as c_int);
        }

        if let Some(alpha) = reqs.alpha_bits {
            out.push(wgl_ext::ALPHA_BITS_ARB as c_int);
            out.push(alpha as c_int);
        }

        if let Some(depth) = reqs.depth_bits {
            out.push(wgl_ext::DEPTH_BITS_ARB as c_int);
            out.push(depth as c_int);
        }

        if let Some(stencil) = reqs.stencil_bits {
            out.push(wgl_ext::STENCIL_BITS_ARB as c_int);
            out.push(stencil as c_int);
        }

        // Prefer double buffering if unspecified (probably shouldn't once you can choose)
        let double_buffer = reqs.double_buffer.unwrap_or(true);
        out.push(wgl_ext::DOUBLE_BUFFER_ARB as c_int);
        out.push(if double_buffer { 1 } else { 0 });

        if let Some(multisampling) = reqs.multisampling {
            if extensions.split(' ').find(|&i| i == "WGL_ARB_multisample").is_some() {
                out.push(wgl_ext::SAMPLE_BUFFERS_ARB as c_int);
                out.push(if multisampling == 0 { 0 } else { 1 });
                out.push(wgl_ext::SAMPLES_ARB as c_int);
                out.push(multisampling as c_int);
            } else {
                return Err(());
            }
        }

        out.push(wgl_ext::STEREO_ARB as c_int);
        out.push(if reqs.stereoscopy { 1 } else { 0 });

        if reqs.srgb {
            if extensions.split(' ').find(|&i| i == "WGL_ARB_framebuffer_sRGB").is_some() {
                out.push(wgl_ext::FRAMEBUFFER_SRGB_CAPABLE_ARB as c_int);
                out.push(1);
            } else if extensions.split(' ').find(|&i| i == "WGL_EXT_framebuffer_sRGB").is_some() {
                out.push(wgl_ext::FRAMEBUFFER_SRGB_CAPABLE_EXT as c_int);
                out.push(1);
            } else {
                return Err(());
            }
        }

        out.push(0);
        out
    };

    let mut format_id = mem::uninitialized();
    let mut num_formats = mem::uninitialized();
    if extra.ChoosePixelFormatARB(hdc as *const _,
                                  descriptor.as_ptr(),
                                  ptr::null(),
                                  1,
                                  &mut format_id,
                                  &mut num_formats) == 0 {
        return Err(());
    }

    if num_formats == 0 {
        return Err(());
    }

    let get_info = |attrib: u32| {
        let mut value = mem::uninitialized();
        extra.GetPixelFormatAttribivARB(hdc as *const _,
                                        format_id as c_int,
                                        0,
                                        1,
                                        [attrib as c_int].as_ptr(),
                                        &mut value);
        value as u32
    };

    let pf_desc = PixelFormat {
        hardware_accelerated: get_info(wgl_ext::ACCELERATION_ARB) != wgl_ext::NO_ACCELERATION_ARB,
        color_bits: get_info(wgl_ext::RED_BITS_ARB) as u8 +
                    get_info(wgl_ext::GREEN_BITS_ARB) as u8 +
                    get_info(wgl_ext::BLUE_BITS_ARB) as u8,
        alpha_bits: get_info(wgl_ext::ALPHA_BITS_ARB) as u8,
        depth_bits: get_info(wgl_ext::DEPTH_BITS_ARB) as u8,
        stencil_bits: get_info(wgl_ext::STENCIL_BITS_ARB) as u8,
        stereoscopy: get_info(wgl_ext::STEREO_ARB) != 0,
        double_buffer: get_info(wgl_ext::DOUBLE_BUFFER_ARB) != 0,
        multisampling: {
            if extensions.split(' ').find(|&i| i == "WGL_ARB_multisample").is_some() {
                match get_info(wgl_ext::SAMPLES_ARB) {
                    0 => None,
                    a => Some(a as u16),
                }
            } else {
                None
            }
        },
        srgb: if extensions.split(' ').find(|&i| i == "WGL_ARB_framebuffer_sRGB").is_some() {
            get_info(wgl_ext::FRAMEBUFFER_SRGB_CAPABLE_ARB) != 0
        } else if extensions.split(' ')
            .find(|&i| i == "WGL_EXT_framebuffer_sRGB")
            .is_some() {
            get_info(wgl_ext::FRAMEBUFFER_SRGB_CAPABLE_EXT) != 0
        } else {
            false
        },
    };

    Ok((format_id, pf_desc))
}

// Chooses a pixel formats without using WGL.
// Gives less precise results than `enumerate_arb_pixel_formats`.
unsafe fn choose_native_pixel_format(hdc: winapi::HDC,
                                     reqs: &WGLPixelFormat)
                                     -> Result<(c_int, PixelFormat), ()> {
    // handling non-supported stuff
    if reqs.float_color_buffer {
        return Err(());
    }

    match reqs.multisampling {
        Some(0) | None => (),
        Some(_) => return Err(()),
    };

    if reqs.stereoscopy {
        return Err(());
    }

    if reqs.srgb {
        return Err(());
    }

    // building the descriptor to pass to ChoosePixelFormat
    let descriptor = winapi::PIXELFORMATDESCRIPTOR {
        nSize: mem::size_of::<winapi::PIXELFORMATDESCRIPTOR>() as u16,
        nVersion: 1,
        dwFlags: {
            let f1 = match reqs.double_buffer {
                // Should be PFD_DOUBLEBUFFER_DONTCARE after you can choose
                None => winapi::PFD_DOUBLEBUFFER,
                Some(true) => winapi::PFD_DOUBLEBUFFER,
                Some(false) => 0,
            };

            let f2 = if reqs.stereoscopy {
                winapi::PFD_STEREO
            } else {
                0
            };

            winapi::PFD_DRAW_TO_WINDOW | winapi::PFD_SUPPORT_OPENGL | f1 | f2
        },
        iPixelType: winapi::PFD_TYPE_RGBA,
        cColorBits: reqs.color_bits.unwrap_or(0),
        cRedBits: 0,
        cRedShift: 0,
        cGreenBits: 0,
        cGreenShift: 0,
        cBlueBits: 0,
        cBlueShift: 0,
        cAlphaBits: reqs.alpha_bits.unwrap_or(0),
        cAlphaShift: 0,
        cAccumBits: 0,
        cAccumRedBits: 0,
        cAccumGreenBits: 0,
        cAccumBlueBits: 0,
        cAccumAlphaBits: 0,
        cDepthBits: reqs.depth_bits.unwrap_or(0),
        cStencilBits: reqs.stencil_bits.unwrap_or(0),
        cAuxBuffers: 0,
        iLayerType: winapi::PFD_MAIN_PLANE,
        bReserved: 0,
        dwLayerMask: 0,
        dwVisibleMask: 0,
        dwDamageMask: 0,
    };

    // now querying
    let pf_id = gdi32::ChoosePixelFormat(hdc, &descriptor);
    if pf_id == 0 {
        return Err(());
    }

    // querying back the capabilities of what windows told us
    let mut output: winapi::PIXELFORMATDESCRIPTOR = mem::zeroed();
    if gdi32::DescribePixelFormat(hdc,
                                  pf_id,
                                  mem::size_of::<winapi::PIXELFORMATDESCRIPTOR>() as u32,
                                  &mut output) == 0 {
        return Err(());
    }

    // windows may return us a non-conforming pixel format if none are supported, so we have to
    // check this
    if (output.dwFlags & winapi::PFD_DRAW_TO_WINDOW) == 0 {
        return Err(());
    }
    if (output.dwFlags & winapi::PFD_SUPPORT_OPENGL) == 0 {
        return Err(());
    }
    if output.iPixelType != winapi::PFD_TYPE_RGBA {
        return Err(());
    }

    let pf_desc = PixelFormat {
        hardware_accelerated: (output.dwFlags & winapi::PFD_GENERIC_FORMAT) == 0,
        color_bits: output.cRedBits + output.cGreenBits + output.cBlueBits,
        alpha_bits: output.cAlphaBits,
        depth_bits: output.cDepthBits,
        stencil_bits: output.cStencilBits,
        stereoscopy: (output.dwFlags & winapi::PFD_STEREO) != 0,
        double_buffer: (output.dwFlags & winapi::PFD_DOUBLEBUFFER) != 0,
        multisampling: None,
        srgb: false,
    };

    if pf_desc.alpha_bits < reqs.alpha_bits.unwrap_or(0) {
        return Err(());
    }
    if pf_desc.depth_bits < reqs.depth_bits.unwrap_or(0) {
        return Err(());
    }
    if pf_desc.stencil_bits < reqs.stencil_bits.unwrap_or(0) {
        return Err(());
    }
    if pf_desc.color_bits < reqs.color_bits.unwrap_or(0) {
        return Err(());
    }
    if !pf_desc.hardware_accelerated {
        return Err(());
    }
    if let Some(req) = reqs.double_buffer {
        if pf_desc.double_buffer != req {
            return Err(());
        }
    }

    Ok((pf_id, pf_desc))
}

// Calls `SetPixelFormat` on a window.
unsafe fn set_pixel_format(hdc: winapi::HDC, id: c_int) -> Result<(), String> {
    let mut output: winapi::PIXELFORMATDESCRIPTOR = mem::zeroed();

    if gdi32::DescribePixelFormat(hdc, id, mem::size_of::<winapi::PIXELFORMATDESCRIPTOR>()
                                  as winapi::UINT, &mut output) == 0 {
        return Err(format!("DescribePixelFormat function failed: {}",io::Error::last_os_error()));
    }

    if gdi32::SetPixelFormat(hdc, id, &output) == 0 {
        return Err(format!("SetPixelFormat function failed: {}",
                           io::Error::last_os_error()));
    }

    Ok(())
}

// Loads the WGL functions that are not guaranteed to be supported.
//
// The `window` must be passed because the driver can vary depending on the window's
// characteristics.
unsafe fn load_extra_functions(window: winapi::HWND) -> Result<wgl_ext::Wgl, String> {
    let (ex_style, style) = (winapi::WS_EX_APPWINDOW,
                             winapi::WS_POPUP | winapi::WS_CLIPSIBLINGS | winapi::WS_CLIPCHILDREN);
    // creating a dummy invisible window
    let mut dummy_window = {
        // getting the rect of the real window
        let rect = {
            let mut placement: winapi::WINDOWPLACEMENT = mem::zeroed();
            placement.length = mem::size_of::<winapi::WINDOWPLACEMENT>() as winapi::UINT;
            if user32::GetWindowPlacement(window, &mut placement) == 0 {
                return Err("user32::GetWindowPlacement failed".to_owned());
            }
            placement.rcNormalPosition
        };

        // getting the class name of the real window
        let mut class_name = [0u16; 128];
        if user32::GetClassNameW(window, class_name.as_mut_ptr(), 128) == 0 {
            return Err(format!("GetClassNameW function failed: {}",
                               io::Error::last_os_error()));
        }

        // this dummy window should match the real one enough to get the same OpenGL driver
        let title = OsStr::new("Dummy")
            .encode_wide()
            .chain(Some(0).into_iter())
            .collect::<Vec<_>>();
        let win = user32::CreateWindowExW(ex_style,
                                          class_name.as_ptr(),
                                          title.as_ptr(),
                                          style,
                                          winapi::CW_USEDEFAULT,
                                          winapi::CW_USEDEFAULT,
                                          rect.right - rect.left,
                                          rect.bottom - rect.top,
                                          ptr::null_mut(),
                                          ptr::null_mut(),
                                          kernel32::GetModuleHandleW(ptr::null()),
                                          ptr::null_mut());

        if win.is_null() {
            return Err(format!("CreateWindowEx function failed: {}", io::Error::last_os_error()));
        }

        let hdc = user32::GetDC(win);
        let ctx = WGLScopedContext::new(win, hdc, ptr::null_mut());

        if hdc.is_null() {
            return Err(format!("GetDC function failed: {}", io::Error::last_os_error()));
        }

        ctx
    };

    // getting the pixel format that we will use and setting it
    {
        let id = try!(choose_dummy_pixel_format(dummy_window.device_ctx));
        try!(set_pixel_format(dummy_window.device_ctx, id));
    }

    // creating the dummy OpenGL context and making it current
    dummy_window.render_ctx = try!(create_basic_context(dummy_window.device_ctx, ptr::null_mut())).0;
    if wgl::MakeCurrent(dummy_window.device_ctx as *const _, dummy_window.render_ctx as *const _) == 0 {
        return Err("WGL::MakeCurrent failed before loading extra WGL functions".to_owned());
    }

    // loading the extra WGL functions
    let result = wgl_ext::Wgl::load_with(|addr| {
        let addr = CString::new(addr.as_bytes()).unwrap();
        let addr = addr.as_ptr();
        wgl::GetProcAddress(addr) as *const c_void
    });

    // Unbind the current context so WGLScopedContext can release resources properly
    // the context to be deleted needs to be unbound.
    wgl::MakeCurrent(ptr::null_mut(), ptr::null_mut());

    Ok(result)
}

// This function chooses a pixel format that is likely to be provided by
// the main video driver of the system.
fn choose_dummy_pixel_format(hdc: winapi::HDC) -> Result<c_int, &'static str> {
    // building the descriptor to pass to ChoosePixelFormat
    let descriptor = winapi::PIXELFORMATDESCRIPTOR {
        nSize: mem::size_of::<winapi::PIXELFORMATDESCRIPTOR>() as u16,
        nVersion: 1,
        dwFlags: winapi::PFD_DRAW_TO_WINDOW | winapi::PFD_SUPPORT_OPENGL | winapi::PFD_DOUBLEBUFFER,
        iPixelType: winapi::PFD_TYPE_RGBA,
        cColorBits: 24,
        cRedBits: 0,
        cRedShift: 0,
        cGreenBits: 0,
        cGreenShift: 0,
        cBlueBits: 0,
        cBlueShift: 0,
        cAlphaBits: 8,
        cAlphaShift: 0,
        cAccumBits: 0,
        cAccumRedBits: 0,
        cAccumGreenBits: 0,
        cAccumBlueBits: 0,
        cAccumAlphaBits: 0,
        cDepthBits: 24,
        cStencilBits: 8,
        cAuxBuffers: 0,
        iLayerType: winapi::PFD_MAIN_PLANE,
        bReserved: 0,
        dwLayerMask: 0,
        dwVisibleMask: 0,
        dwDamageMask: 0,
    };

    // now querying
    let pf_id = unsafe { gdi32::ChoosePixelFormat(hdc, &descriptor) };
    if pf_id == 0 {
        return Err("No available pixel format");
    }

    Ok(pf_id)
}