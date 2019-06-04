extern crate pkg_config;
#[cfg(target_env = "msvc")]
extern crate vcpkg;
extern crate cc;

use std::env;
use std::fs;
use std::path::PathBuf;
use std::process::Command;

fn main() {
    println!("cargo:rerun-if-env-changed=LIBZ_SYS_STATIC");
    println!("cargo:rerun-if-changed=build.rs");
    let host = env::var("HOST").unwrap();
    let target = env::var("TARGET").unwrap();
    let wants_asm = cfg!(feature = "asm");

    let host_and_target_contain = |s| host.contains(s) && target.contains(s);

    // Don't run pkg-config if we're linking statically (we'll build below) and
    // also don't run pkg-config on macOS/FreeBSD/DragonFly. That'll end up printing
    // `-L /usr/lib` which wreaks havoc with linking to an OpenSSL in /usr/local/lib
    // (Homebrew, Ports, etc.)
    let want_static =
        cfg!(feature = "static") || env::var("LIBZ_SYS_STATIC").unwrap_or(String::new()) == "1";
    if !wants_asm &&
       !want_static &&
       !target.contains("msvc") && // pkg-config just never works here
       !(host_and_target_contain("apple") ||
         host_and_target_contain("freebsd") ||
         host_and_target_contain("dragonfly")) &&
        pkg_config::Config::new().cargo_metadata(true).probe("zlib").is_ok() {
        return
    }

    if target.contains("msvc") {
        if !wants_asm && try_vcpkg() {
            return;
        }
    }

    // All android compilers should come with libz by default, so let's just use
    // the one already there.
    if !wants_asm && target.contains("android") {
        println!("cargo:rustc-link-lib=z");
        return
    }

    let mut cfg = cc::Build::new();

    // Whitelist a bunch of situations where we build unconditionally.
    //
    // MSVC basically never has it preinstalled, MinGW picks up a bunch of weird
    // paths we don't like, `want_static` may force us, cross compiling almost
    // never has a prebuilt version, and musl is almost always static.
    if wants_asm ||
        target.contains("msvc") ||
        target.contains("pc-windows-gnu") ||
        want_static ||
        target != host ||
        target.contains("musl")
    {
        return build_zlib(&mut cfg, &target);
    }

    // If we've gotten this far we're probably a pretty standard platform.
    // Almost all platforms here ship libz by default, but some don't have
    // pkg-config files that we would find above.
    //
    // In any case test if zlib is actually installed and if so we link to it,
    // otherwise continue below to build things.
    if zlib_installed(&mut cfg) {
        println!("cargo:rustc-link-lib=z");
        return
    }

    build_zlib(&mut cfg, &target)
}

fn build_zlib(cfg: &mut cc::Build, target: &str) {
    let dst = PathBuf::from(env::var_os("OUT_DIR").unwrap());
    let build = dst.join("build");
    let asm = cfg!(feature = "asm");

    cfg.warnings(false)
        .out_dir(&build)
        .include("src/zlib");

    cfg.file("src/zlib/adler32.c")
        .file("src/zlib/compress.c")
        .file("src/zlib/crc32.c")
        .file("src/zlib/deflate.c")
        .file("src/zlib/gzclose.c")
        .file("src/zlib/gzlib.c")
        .file("src/zlib/gzread.c")
        .file("src/zlib/gzwrite.c")
        .file("src/zlib/infback.c")
        .file("src/zlib/inffast.c")
        .file("src/zlib/inflate.c")
        .file("src/zlib/inftrees.c")
        .file("src/zlib/trees.c")
        .file("src/zlib/uncompr.c")
        .file("src/zlib/zutil.c");
    if !target.contains("windows") {
        cfg.define("STDC", None);
        cfg.define("_LARGEFILE64_SOURCE", None);
        cfg.define("_POSIX_SOURCE", None);
        cfg.flag("-fvisibility=hidden");
    }
    if target.contains("ios") {
        cfg.define("_C99_SOURCE", None);
    }
    if target.contains("solaris") {
        cfg.define("_XOPEN_SOURCE", "700");
    }

    if asm {
        if target.contains("windows-msvc") {
            if target.starts_with("x86_64") {
                cfg.file("src/zlib/contrib/masmx64/inffasx64.asm")
                    .file("src/zlib/contrib/masmx64/gvmat64.asm")
                    .define("ASMV", None)
                    .define("ASMINF", None);
            } else if target.starts_with("i686") {
                cfg.file("src/zlib/contrib/masmx86/inffas32.asm")
                    .file("src/zlib/contrib/masmx86/match686.asm")
                    .define("ASMV", None)
                    .define("ASMINF", None);
            }
        } else {
            if target.starts_with("x86_64") {
                cfg.file("src/zlib/contrib/amd64/amd64-match.S")
                    .define("ASMV", None);
            } else if target.starts_with("i686") {
                cfg.file("src/zlib/contrib/inflate86/inffast.S")
                    .define("ASMINF", None);
            }
        }
    }

    cfg.compile("z");

    fs::create_dir_all(dst.join("lib/pkgconfig")).unwrap();
    fs::create_dir_all(dst.join("include")).unwrap();
    fs::copy("src/zlib/zlib.h", dst.join("include/zlib.h")).unwrap();
    fs::copy("src/zlib/zconf.h", dst.join("include/zconf.h")).unwrap();

    fs::write(
        dst.join("lib/pkgconfig/zlib.pc"),
        fs::read_to_string("src/zlib/zlib.pc.in")
            .unwrap()
            .replace("@prefix@", dst.to_str().unwrap()),
    ).unwrap();

    println!("cargo:root={}", dst.to_str().unwrap());
    println!("cargo:include={}/include", dst.to_str().unwrap());
}

#[cfg(not(target_env = "msvc"))]
fn try_vcpkg() -> bool {
    false
}

#[cfg(target_env = "msvc")]
fn try_vcpkg() -> bool {
    // see if there is a vcpkg tree with zlib installed
    match vcpkg::Config::new()
            .emit_includes(true)
            .lib_names("zlib", "zlib1")
            .probe("zlib") {
        Ok(_) => { true },
        Err(e) => {
            println!("note, vcpkg did not find zlib: {}", e);
            false
        },
    }
}

fn zlib_installed(cfg: &mut cc::Build) -> bool {
    let compiler = cfg.get_compiler();
    let mut cmd = Command::new(compiler.path());
    cmd.arg("src/smoke.c")
        .arg("-o").arg("/dev/null")
        .arg("-lz");

    println!("running {:?}", cmd);
    if let Ok(status) = cmd.status() {
        if status.success() {
            return true
        }
    }

    false
}
