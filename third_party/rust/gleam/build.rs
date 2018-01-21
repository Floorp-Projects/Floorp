extern crate gl_generator;
extern crate pkg_config;

use std::env;
use std::fs::File;
use std::path::Path;
use gl_generator::{Registry, Api, Profile, Fallbacks};

fn main() {
    let dest = env::var("OUT_DIR").unwrap();
    let mut file_gl_and_gles = File::create(&Path::new(&dest).join("gl_and_gles_bindings.rs")).unwrap();
    let mut file_gl = File::create(&Path::new(&dest).join("gl_bindings.rs")).unwrap();
    let mut file_gles = File::create(&Path::new(&dest).join("gles_bindings.rs")).unwrap();

    // OpenGL 3.3 bindings
    let gl_extensions = ["GL_ARB_texture_rectangle",
                         "GL_EXT_debug_marker",
                         "GL_APPLE_client_storage",
                         "GL_APPLE_texture_range",
                         "GL_APPLE_fence",
                         "GL_ARB_get_program_binary",
                         "GL_ARB_blend_func_extended"];
    let gl_reg = Registry::new(Api::Gl, (3, 3), Profile::Core, Fallbacks::All, gl_extensions);
    gl_reg.write_bindings(gl_generator::StructGenerator, &mut file_gl)
          .unwrap();

    // GLES 3.0 bindings
    let gles_extensions = [
        "GL_EXT_texture_format_BGRA8888",
        "GL_OES_EGL_image",
        "GL_OES_EGL_image_external",
        "GL_EXT_disjoint_timer_query",
        "GL_EXT_debug_marker",
    ];
    let gles_reg = Registry::new(Api::Gles2, (3, 0), Profile::Core, Fallbacks::All, gles_extensions);
    gles_reg.write_bindings(gl_generator::StructGenerator, &mut file_gles)
            .unwrap();

    // OpenGL 3.3 + GLES 3.0 bindings. Used to get all enums
    let gl_reg = gl_reg + gles_reg;
    gl_reg.write_bindings(gl_generator::StructGenerator, &mut file_gl_and_gles)
          .unwrap();
}
