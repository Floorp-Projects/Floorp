// Attributes to use when creating an OpenGL context.
#[derive(Clone, Debug)]
pub struct WGLAttributes {
    pub opengl_es: bool, // enable or disable OpenGL ES contest
    pub major_version: u32, // OpenGL major version. Set 0 to select the latest
    pub minor_version: u32, // OpenGL minor version.
    pub debug: bool, // Debug mode improves error information. Disabled by default.
    pub vsync: bool, // Enable or disable vsync for swap_buffers. Disabled by default.
    pub pixel_format: WGLPixelFormat, // Pixel format requirements
}

impl Default for WGLAttributes {
    #[inline]
    fn default() -> WGLAttributes {
        WGLAttributes {
            opengl_es: false,
            major_version: 2,
            minor_version: 1,
            debug: false,
            vsync: false,
            pixel_format: WGLPixelFormat::default(),
        }
    }
}

#[derive(Clone, Debug)]
pub struct WGLPixelFormat {
    // Minimum number of bits for the color buffer, excluding alpha. `None` means "don't care".
    // The default is `Some(24)`.
    pub color_bits: Option<u8>,

    // If true, the color buffer must be in a floating point format. Default is `false`.
    //
    // Using floating points allows you to write values outside of the `[0.0, 1.0]` range.
    pub float_color_buffer: bool,

    // Minimum number of bits for the alpha in the color buffer. `None` means "don't care".
    // The default is `Some(8)`.
    pub alpha_bits: Option<u8>,

    // Minimum number of bits for the depth buffer. `None` means "don't care".
    // The default value is `Some(24)`.
    pub depth_bits: Option<u8>,

    // Minimum number of bits for the depth buffer. `None` means "don't care".
    // The default value is `Some(8)`.
    pub stencil_bits: Option<u8>,

    // If true, only double-buffered formats will be considered. If false, only single-buffer
    // formats. `None` means "don't care". The default is `Some(true)`.
    pub double_buffer: Option<bool>,

    // Contains the minimum number of samples per pixel in the color, depth and stencil buffers.
    // `None` means "don't care". Default is `None`.
    // A value of `Some(0)` indicates that multisampling must not be enabled.
    pub multisampling: Option<u16>,

    // If true, only stereoscopic formats will be considered. If false, only non-stereoscopic
    // formats. The default is `false`.
    pub stereoscopy: bool,

    // If true, only sRGB-capable formats will be considered. If false, don't care.
    // The default is `false`.
    pub srgb: bool,
}

impl Default for WGLPixelFormat {
    #[inline]
    fn default() -> WGLPixelFormat {
        WGLPixelFormat {
            color_bits: Some(24),
            float_color_buffer: false,
            alpha_bits: Some(8),
            depth_bits: Some(24),
            stencil_bits: Some(8),
            double_buffer: None,
            multisampling: None,
            stereoscopy: false,
            srgb: false,
        }
    }
}