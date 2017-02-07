
pub enum GLFeature {
    FramebufferMultisample, // Framebuffer multisample, for antialising
}

/// Possible feature requirements:
///  * OpenGL version
///  * GLES version
///  * Extensions
#[allow(dead_code)]
pub struct GLFeatureRequirements {
    opengl_version: u32, // OpenGL version: 1.0 => 10, etc...
    gles_version: u32,
    // Better not check for extensions unless necessary
    // They use different symbols, and that adds a lot of complexity
    // extensions: Vec<&'static str>
}

// TODO: optimize (maybe use `rust-phf`)?
fn get_feature_requirements(feature: GLFeature) -> GLFeatureRequirements {
    match feature {
        GLFeature::FramebufferMultisample => GLFeatureRequirements {
            opengl_version: 30,
            gles_version: 30,
            // extensions: vec!["GL_EXT_framebuffer_multisample"]
        }
    }
}

impl GLFeature {
    pub fn is_supported(feature: GLFeature) -> bool {
        let _ = get_feature_requirements(feature);
        // TODO
        false
    }
}
