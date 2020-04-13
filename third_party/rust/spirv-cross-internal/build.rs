fn main() {
    // Prevent building SPIRV-Cross on wasm32 target
    let target_arch = std::env::var("CARGO_CFG_TARGET_ARCH");
    if let Ok(arch) = target_arch {
        if "wasm32" == arch {
            return;
        }
    }

    let target_vendor = std::env::var("CARGO_CFG_TARGET_VENDOR");
    let is_apple = target_vendor.is_ok() && target_vendor.unwrap() == "apple";

    let target_os = std::env::var("CARGO_CFG_TARGET_OS");
    let is_ios = target_os.is_ok() && target_os.unwrap() == "ios";

    let mut build = cc::Build::new();
    build.cpp(true);

    let compiler = build.try_get_compiler();
    let is_clang = compiler.is_ok() && compiler.unwrap().is_like_clang();

    if is_apple && (is_clang || is_ios) {
        build.flag("-std=c++14").cpp_set_stdlib("c++");
    } else {
        build.flag_if_supported("-std=c++14");
    }

    build
        .flag("-DSPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS")
        .flag("-DSPIRV_CROSS_WRAPPER_NO_EXCEPTIONS");

    build
        .file("src/wrapper.cpp")
        .file("src/vendor/SPIRV-Cross/spirv_cfg.cpp")
        .file("src/vendor/SPIRV-Cross/spirv_cross.cpp")
        .file("src/vendor/SPIRV-Cross/spirv_cross_parsed_ir.cpp")
        .file("src/vendor/SPIRV-Cross/spirv_parser.cpp")
        .file("src/vendor/SPIRV-Cross/spirv_cross_util.cpp");

    // Ideally the GLSL compiler would be omitted here, but the HLSL and MSL compiler
    // currently inherit from it. So it's necessary to unconditionally include it here.
    build
        .file("src/vendor/SPIRV-Cross/spirv_glsl.cpp")
        .flag("-DSPIRV_CROSS_WRAPPER_GLSL");

    #[cfg(feature = "hlsl")]
    build
        .file("src/vendor/SPIRV-Cross/spirv_hlsl.cpp")
        .flag("-DSPIRV_CROSS_WRAPPER_HLSL");

    #[cfg(feature = "msl")]
    build
        .file("src/vendor/SPIRV-Cross/spirv_msl.cpp")
        .flag("-DSPIRV_CROSS_WRAPPER_MSL");

    build.compile("spirv-cross-rust-wrapper");
}
