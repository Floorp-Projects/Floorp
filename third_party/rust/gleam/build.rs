extern crate gl_generator;
extern crate pkg_config;

use std::env;
use std::fs::File;
use std::path::Path;
use gl_generator::{Registry, Api, Profile, Fallbacks};

fn main() {
    let dest = env::var("OUT_DIR").unwrap();
    let mut file = File::create(&Path::new(&dest).join("gl_bindings.rs")).unwrap();

    let target = env::var("TARGET").unwrap();

    if target.contains("android") {
        // GLES 2.0 bindings for Android
        Registry::new(Api::Gles2, (3, 0), Profile::Core, Fallbacks::All, ["GL_EXT_texture_format_BGRA8888"])
            .write_bindings(gl_generator::StaticGenerator, &mut file)
            .unwrap();

        println!("cargo:rustc-link-lib=GLESv3");
    } else {
        // OpenGL 3.3 bindings for Linux/Mac/Windows
        Registry::new(Api::Gl, (3, 3), Profile::Core, Fallbacks::All, ["GL_ARB_texture_rectangle"])
            .write_bindings(gl_generator::GlobalGenerator, &mut file)
            .unwrap();

        if target.contains("darwin") {
            println!("cargo:rustc-link-lib=framework=OpenGL");
        } else if target.contains("windows") {
            println!("cargo:rustc-link-lib=opengl32");
        } else {
            if let Err(_) = pkg_config::probe_library("gl") {
                println!("cargo:rustc-link-lib=GL");
            }
        }
    }
}
