use gleam::gl::types::GLenum;
use gleam::gl;
use GLContextAttributes;

/// This structure is here to allow
/// cross-platform formatting
pub struct GLFormats {
    pub color_renderbuffer: GLenum,
    pub texture_internal: GLenum,
    pub texture: GLenum,
    pub texture_type: GLenum,
    pub depth: GLenum,
    pub stencil: GLenum
}

impl GLFormats {
    // In the future we may use extension detection et-al to improve this, for now
    // platform dependent.
    //
    // FIXME: In linux with GLES2 texture attachments create INVALID_ENUM errors.
    // I suspect that it's because of texture formats, but I need time to debugit.
    #[cfg(not(target_os="android"))]
    pub fn detect(attrs: &GLContextAttributes) -> GLFormats {
        if attrs.alpha {
            GLFormats {
                color_renderbuffer: gl::RGBA8,
                texture_internal: gl::RGBA,
                texture: gl::RGBA,
                texture_type: gl::UNSIGNED_BYTE,
                depth: gl::DEPTH_COMPONENT24,
                stencil: gl::STENCIL_INDEX8,
            }
        } else {
            GLFormats {
                color_renderbuffer: gl::RGB8,
                texture_internal: gl::RGB8,
                texture: gl::RGB,
                texture_type: gl::UNSIGNED_BYTE,
                depth: gl::DEPTH_COMPONENT24,
                stencil: gl::STENCIL_INDEX8,
            }
        }
    }

    #[cfg(target_os="android")]
    pub fn detect(attrs: &GLContextAttributes) -> GLFormats {
        // detect if the GPU supports RGB8 and RGBA8 renderbuffer/texture storage formats.
        // GL_ARM_rgba8 extension is similar to OES_rgb8_rgba8, but only exposes RGBA8.
        let extensions = gl::get_string(gl::EXTENSIONS);
        let extensions: Vec<&str> = extensions.split(&[',',' '][..]).collect();
        let has_rgb8 = extensions.contains(&"GL_OES_rgb8_rgba8");
        let has_rgba8 = has_rgb8 || extensions.contains(&"GL_ARM_rgba8");

        if attrs.alpha {
            GLFormats {
                color_renderbuffer: if has_rgba8 { gl::RGBA8 } else { gl::RGBA4 },
                texture_internal: gl::RGBA,
                texture: gl::RGBA,
                texture_type: if has_rgba8 { gl::UNSIGNED_BYTE } else { gl::UNSIGNED_SHORT_4_4_4_4 },
                depth: gl::DEPTH_COMPONENT16,
                stencil: gl::STENCIL_INDEX8,
            }
        } else {
            GLFormats {
                color_renderbuffer: if has_rgb8 { gl::RGB8 } else { gl::RGB565 },
                texture_internal: gl::RGB,
                texture: gl::RGB,
                texture_type: if has_rgb8 { gl::UNSIGNED_BYTE } else { gl::UNSIGNED_SHORT_4_4_4_4 },
                depth: gl::DEPTH_COMPONENT16,
                stencil: gl::STENCIL_INDEX8,
            }
        }
    }
}

