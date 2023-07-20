/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This shouldn't exist, but does because if something isn't going to link
//! against `nss` but has an `nss`-enabled `sqlcipher` turned on (for example,
//! by a `cargo` feature activated by something else in the workspace).
//! it might need to issue link commands for NSS.

use std::{
    env,
    ffi::OsString,
    path::{Path, PathBuf},
};

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum LinkingKind {
    Dynamic { folded_libs: bool },
    Static,
}

#[derive(Debug, PartialEq, Eq, Clone)]
pub struct NoNssDir;

pub fn link_nss() -> Result<(), NoNssDir> {
    let is_gecko = env::var_os("MOZ_TOPOBJDIR").is_some();
    if !is_gecko {
        let (lib_dir, include_dir) = get_nss()?;
        println!(
            "cargo:rustc-link-search=native={}",
            lib_dir.to_string_lossy()
        );
        println!("cargo:include={}", include_dir.to_string_lossy());
        let kind = determine_kind();
        link_nss_libs(kind);
    } else {
        let libs = match env::var("CARGO_CFG_TARGET_OS")
            .as_ref()
            .map(std::string::String::as_str)
        {
            Ok("android") | Ok("macos") => vec!["nss3"],
            _ => vec!["nssutil3", "nss3", "plds4", "plc4", "nspr4"],
        };
        for lib in &libs {
            println!("cargo:rustc-link-lib=dylib={}", lib);
        }
    }
    Ok(())
}

fn get_nss() -> Result<(PathBuf, PathBuf), NoNssDir> {
    let nss_dir = env("NSS_DIR").ok_or(NoNssDir)?;
    let nss_dir = Path::new(&nss_dir);
    if !nss_dir.exists() {
        println!(
            "NSS_DIR path (obtained via `env`) does not exist: {}",
            nss_dir.display()
        );
        panic!("It looks like NSS is not built. Please run `libs/verify-[platform]-environment.sh` first!");
    }
    let lib_dir = nss_dir.join("lib");
    let include_dir = nss_dir.join("include");
    Ok((lib_dir, include_dir))
}

fn determine_kind() -> LinkingKind {
    if env_flag("NSS_STATIC") {
        LinkingKind::Static
    } else {
        let folded_libs = env_flag("NSS_USE_FOLDED_LIBS");
        LinkingKind::Dynamic { folded_libs }
    }
}

fn link_nss_libs(kind: LinkingKind) {
    let libs = get_nss_libs(kind);
    // Emit -L flags
    let kind_str = match kind {
        LinkingKind::Dynamic { .. } => "dylib",
        LinkingKind::Static => "static",
    };
    for lib in libs {
        println!("cargo:rustc-link-lib={}={}", kind_str, lib);
    }
    // Link against C++ stdlib (for mozpkix)
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap();
    if target_os == "android" || target_os == "linux" {
        println!("cargo:rustc-link-lib=stdc++");
    } else {
        println!("cargo:rustc-link-lib=c++");
    }
    let target_arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap();
    if target_arch == "x86_64" && target_os == "android" {
        let android_home = env::var("ANDROID_HOME").expect("ANDROID_HOME not set");
        const ANDROID_NDK_VERSION: &str = "25.2.9519653";
        // One of these will exist, depending on the host platform.
        const DARWIN_X86_64_LIB_DIR: &str =
            "/toolchains/llvm/prebuilt/darwin-x86_64/lib64/clang/14.0.7/lib/linux/";
        println!("cargo:rustc-link-search={android_home}/ndk/{ANDROID_NDK_VERSION}/{DARWIN_X86_64_LIB_DIR}");
        const LINUX_X86_64_LIB_DIR: &str =
            "/toolchains/llvm/prebuilt/linux-x86_64/lib64/clang/14.0.7/lib/linux/";
        println!("cargo:rustc-link-search={android_home}/ndk/{ANDROID_NDK_VERSION}/{LINUX_X86_64_LIB_DIR}");
        println!("cargo:rustc-link-lib=static=clang_rt.builtins-x86_64-android");
    }
}

fn get_nss_libs(kind: LinkingKind) -> Vec<&'static str> {
    match kind {
        LinkingKind::Static => {
            let mut static_libs = vec![
                "certdb",
                "certhi",
                "cryptohi",
                "freebl_static",
                "mozpkix",
                "nspr4",
                "nss_static",
                "nssb",
                "nssdev",
                "nsspki",
                "nssutil",
                "pk11wrap_static",
                "plc4",
                "plds4",
                "softokn_static",
            ];
            // Hardware specific libs.
            let target_arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap();
            let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap();
            // https://searchfox.org/nss/rev/0d5696b3edce5124353f03159d2aa15549db8306/lib/freebl/freebl.gyp#508-542
            if target_arch == "arm" || target_arch == "aarch64" {
                static_libs.push("armv8_c_lib");
            }
            if target_arch == "x86_64" || target_arch == "x86" {
                static_libs.push("gcm-aes-x86_c_lib");
                static_libs.push("sha-x86_c_lib");
            }
            if target_arch == "arm" {
                static_libs.push("gcm-aes-arm32-neon_c_lib")
            }
            if target_arch == "aarch64" {
                static_libs.push("gcm-aes-aarch64_c_lib");
            }
            if target_arch == "x86_64" {
                static_libs.push("hw-acc-crypto-avx");
                static_libs.push("hw-acc-crypto-avx2");
            }
            // https://searchfox.org/nss/rev/08c4d05078d00089f8d7540651b0717a9d66f87e/lib/freebl/freebl.gyp#315-324
            if ((target_os == "android" || target_os == "linux") && target_arch == "x86_64")
                || target_os == "windows"
            {
                static_libs.push("intel-gcm-wrap_c_lib");
                // https://searchfox.org/nss/rev/08c4d05078d00089f8d7540651b0717a9d66f87e/lib/freebl/freebl.gyp#43-47
                if (target_os == "android" || target_os == "linux") && target_arch == "x86_64" {
                    static_libs.push("intel-gcm-s_lib");
                }
            }
            static_libs
        }
        LinkingKind::Dynamic { folded_libs } => {
            let mut dylibs = vec!["freebl3", "nss3", "nssckbi", "softokn3"];
            if !folded_libs {
                dylibs.append(&mut vec!["nspr4", "nssutil3", "plc4", "plds4"]);
            }
            dylibs
        }
    }
}

pub fn env(name: &str) -> Option<OsString> {
    println!("cargo:rerun-if-env-changed={}", name);
    env::var_os(name)
}

pub fn env_str(name: &str) -> Option<String> {
    println!("cargo:rerun-if-env-changed={}", name);
    env::var(name).ok()
}

pub fn env_flag(name: &str) -> bool {
    match env_str(name).as_ref().map(String::as_ref) {
        Some("1") => true,
        Some("0") => false,
        Some(s) => {
            println!(
                "cargo:warning=unknown value for environment var {:?}: {:?}. Ignoring",
                name, s
            );
            false
        }
        None => false,
    }
}
