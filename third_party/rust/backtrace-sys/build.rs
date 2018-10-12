extern crate cc;

use std::env;
use std::path::PathBuf;
use std::fs::File;

fn main() {
    let target = env::var("TARGET").unwrap();

    // libbacktrace isn't used on windows
    if target.contains("windows") {
        return
    }

    // no way this will ever compile for emscripten
    if target.contains("emscripten") {
        return
    }

    let out_dir = PathBuf::from(env::var_os("OUT_DIR").unwrap());

    let mut build = cc::Build::new();
    build
        .include("src/libbacktrace")
        .include(&out_dir)
        .warnings(false)
        .file("src/libbacktrace/alloc.c")
        .file("src/libbacktrace/dwarf.c")
        .file("src/libbacktrace/fileline.c")
        .file("src/libbacktrace/posix.c")
        .file("src/libbacktrace/read.c")
        .file("src/libbacktrace/sort.c")
        .file("src/libbacktrace/state.c");

    if target.contains("darwin") {
        build.file("src/libbacktrace/macho.c");
    } else if target.contains("windows") {
        build.file("src/libbacktrace/pecoff.c");
    } else {
        build.flag("-fvisibility=hidden");
        build.file("src/libbacktrace/elf.c");

        let pointer_width = env::var("CARGO_CFG_TARGET_POINTER_WIDTH").unwrap();
        if pointer_width == "64" {
            build.define("BACKTRACE_ELF_SIZE", "64");
        } else {
            build.define("BACKTRACE_ELF_SIZE", "32");
        }
    }

    File::create(out_dir.join("backtrace-supported.h")).unwrap();
    build.define("BACKTRACE_SUPPORTED", "1");
    build.define("BACKTRACE_USES_MALLOC", "1");
    build.define("BACKTRACE_SUPPORTS_THREADS", "0");
    build.define("BACKTRACE_SUPPORTS_DATA", "0");

    File::create(out_dir.join("config.h")).unwrap();
    if !target.contains("apple-ios") &&
       !target.contains("solaris") &&
       !target.contains("redox") &&
       !target.contains("android") &&
       !target.contains("haiku") {
        build.define("HAVE_DL_ITERATE_PHDR", "1");
    }
    build.define("_GNU_SOURCE", "1");
    build.define("_LARGE_FILES", "1");

    let syms = [
        "backtrace_full",
        "backtrace_dwarf_add",
        "backtrace_initialize",
        "backtrace_pcinfo",
        "backtrace_syminfo",
        "backtrace_get_view",
        "backtrace_release_view",
        "backtrace_alloc",
        "backtrace_free",
        "backtrace_vector_finish",
        "backtrace_vector_grow",
        "backtrace_vector_release",
        "backtrace_close",
        "backtrace_open",
        "backtrace_print",
        "backtrace_simple",
        "backtrace_qsort",
        "backtrace_create_state",
        "backtrace_uncompress_zdebug",
    ];
    for sym in syms.iter() {
        build.define(sym, &format!("__rbt_{}", sym)[..]);
    }

    build.compile("backtrace");
}
