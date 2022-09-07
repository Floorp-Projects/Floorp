// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(feature = "deny-warnings", deny(warnings))]
#![warn(clippy::pedantic)]

use bindgen::Builder;
use serde_derive::Deserialize;
use std::collections::HashMap;
use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

const BINDINGS_DIR: &str = "bindings";
const BINDINGS_CONFIG: &str = "bindings.toml";

// This is the format of a single section of the configuration file.
#[derive(Deserialize)]
struct Bindings {
    /// types that are explicitly included
    #[serde(default)]
    types: Vec<String>,
    /// functions that are explicitly included
    #[serde(default)]
    functions: Vec<String>,
    /// variables (and `#define`s) that are explicitly included
    #[serde(default)]
    variables: Vec<String>,
    /// types that should be explicitly marked as opaque
    #[serde(default)]
    opaque: Vec<String>,
    /// enumerations that are turned into a module (without this, the enum is
    /// mapped using the default, which means that the individual values are
    /// formed with an underscore as <enum_type>_<enum_value_name>).
    #[serde(default)]
    enums: Vec<String>,

    /// Any item that is specifically excluded; if none of the types, functions,
    /// or variables fields are specified, everything defined will be mapped,
    /// so this can be used to limit that.
    #[serde(default)]
    exclude: Vec<String>,

    /// Whether the file is to be interpreted as C++
    #[serde(default)]
    cplusplus: bool,
}

fn is_debug() -> bool {
    env::var("DEBUG")
        .map(|d| d.parse::<bool>().unwrap_or(false))
        .unwrap_or(false)
}

// bindgen needs access to libclang.
// On windows, this doesn't just work, you have to set LIBCLANG_PATH.
// Rather than download the 400Mb+ files, like gecko does, let's just reuse their work.
fn setup_clang() {
    if env::consts::OS != "windows" {
        return;
    }
    println!("rerun-if-env-changed=LIBCLANG_PATH");
    println!("rerun-if-env-changed=MOZBUILD_STATE_PATH");
    if env::var("LIBCLANG_PATH").is_ok() {
        return;
    }
    let mozbuild_root = if let Ok(dir) = env::var("MOZBUILD_STATE_PATH") {
        PathBuf::from(dir.trim())
    } else {
        eprintln!("warning: Building without a gecko setup is not likely to work.");
        eprintln!("         A working libclang is needed to build neqo.");
        eprintln!("         Either LIBCLANG_PATH or MOZBUILD_STATE_PATH needs to be set.");
        eprintln!();
        eprintln!("    We recommend checking out https://github.com/mozilla/gecko-dev");
        eprintln!("    Then run `./mach bootstrap` which will retrieve clang.");
        eprintln!("    Make sure to export MOZBUILD_STATE_PATH when building.");
        return;
    };
    let libclang_dir = mozbuild_root.join("clang").join("lib");
    if libclang_dir.is_dir() {
        env::set_var("LIBCLANG_PATH", libclang_dir.to_str().unwrap());
        println!("rustc-env:LIBCLANG_PATH={}", libclang_dir.to_str().unwrap());
    } else {
        println!("warning: LIBCLANG_PATH isn't set; maybe run ./mach bootstrap with gecko");
    }
}

fn nss_dir() -> PathBuf {
    let dir = if let Ok(dir) = env::var("NSS_DIR") {
        let path = PathBuf::from(dir.trim());
        assert!(
            !path.is_relative(),
            "The NSS_DIR environment variable is expected to be an absolute path."
        );
        path
    } else {
        let out_dir = env::var("OUT_DIR").unwrap();
        let dir = Path::new(&out_dir).join("nss");
        if !dir.exists() {
            Command::new("hg")
                .args(&[
                    "clone",
                    "https://hg.mozilla.org/projects/nss",
                    dir.to_str().unwrap(),
                ])
                .status()
                .expect("can't clone nss");
        }
        let nspr_dir = Path::new(&out_dir).join("nspr");
        if !nspr_dir.exists() {
            Command::new("hg")
                .args(&[
                    "clone",
                    "https://hg.mozilla.org/projects/nspr",
                    nspr_dir.to_str().unwrap(),
                ])
                .status()
                .expect("can't clone nspr");
        }
        dir
    };
    assert!(dir.is_dir(), "NSS_DIR {:?} doesn't exist", dir);
    // Note that this returns a relative path because UNC
    // paths on windows cause certain tools to explode.
    dir
}

fn get_bash() -> PathBuf {
    // When running under MOZILLABUILD, we need to make sure not to invoke
    // another instance of bash that might be sitting around (like WSL).
    match env::var("MOZILLABUILD") {
        Ok(d) => PathBuf::from(d).join("msys").join("bin").join("bash.exe"),
        Err(_) => PathBuf::from("bash"),
    }
}

fn build_nss(dir: PathBuf) {
    let mut build_nss = vec![
        String::from("./build.sh"),
        String::from("-Ddisable_tests=1"),
    ];
    if is_debug() {
        build_nss.push(String::from("--static"));
    } else {
        build_nss.push(String::from("-o"));
    }
    if let Ok(d) = env::var("NSS_JOBS") {
        build_nss.push(String::from("-j"));
        build_nss.push(d);
    }
    let status = Command::new(get_bash())
        .args(build_nss)
        .current_dir(dir)
        .status()
        .expect("couldn't start NSS build");
    assert!(status.success(), "NSS build failed");
}

fn dynamic_link() {
    let libs = if env::consts::OS == "windows" {
        &["nssutil3.dll", "nss3.dll", "ssl3.dll"]
    } else {
        &["nssutil3", "nss3", "ssl3"]
    };
    dynamic_link_both(libs);
}

fn dynamic_link_both(extra_libs: &[&str]) {
    let nspr_libs = if env::consts::OS == "windows" {
        &["libplds4", "libplc4", "libnspr4"]
    } else {
        &["plds4", "plc4", "nspr4"]
    };
    for lib in nspr_libs.iter().chain(extra_libs) {
        println!("cargo:rustc-link-lib=dylib={}", lib);
    }
}

fn static_link() {
    let mut static_libs = vec![
        "certdb",
        "certhi",
        "cryptohi",
        "freebl",
        "nss_static",
        "nssb",
        "nssdev",
        "nsspki",
        "nssutil",
        "pk11wrap",
        "pkcs12",
        "pkcs7",
        "smime",
        "softokn_static",
        "ssl",
    ];
    if env::consts::OS != "macos" {
        static_libs.push("sqlite");
    }
    for lib in static_libs {
        println!("cargo:rustc-link-lib=static={}", lib);
    }

    // Dynamic libs that aren't transitively included by NSS libs.
    let mut other_libs = Vec::new();
    if env::consts::OS != "windows" {
        other_libs.extend_from_slice(&["pthread", "dl", "c", "z"]);
    }
    if env::consts::OS == "macos" {
        other_libs.push("sqlite3");
    }
    dynamic_link_both(&other_libs);
}

fn get_includes(nsstarget: &Path, nssdist: &Path) -> Vec<PathBuf> {
    let nsprinclude = nsstarget.join("include").join("nspr");
    let nssinclude = nssdist.join("public").join("nss");
    let includes = vec![nsprinclude, nssinclude];
    for i in &includes {
        println!("cargo:include={}", i.to_str().unwrap());
    }
    includes
}

fn build_bindings(base: &str, bindings: &Bindings, flags: &[String], gecko: bool) {
    let suffix = if bindings.cplusplus { ".hpp" } else { ".h" };
    let header_path = PathBuf::from(BINDINGS_DIR).join(String::from(base) + suffix);
    let header = header_path.to_str().unwrap();
    let out = PathBuf::from(env::var("OUT_DIR").unwrap()).join(String::from(base) + ".rs");

    println!("cargo:rerun-if-changed={}", header);

    let mut builder = Builder::default().header(header);
    builder = builder.generate_comments(false);
    builder = builder.size_t_is_usize(true);

    builder = builder.clang_arg("-v");

    if !gecko {
        builder = builder.clang_arg("-DNO_NSPR_10_SUPPORT");
        if env::consts::OS == "windows" {
            builder = builder.clang_arg("-DWIN");
        } else if env::consts::OS == "macos" {
            builder = builder.clang_arg("-DDARWIN");
        } else if env::consts::OS == "linux" {
            builder = builder.clang_arg("-DLINUX");
        } else if env::consts::OS == "android" {
            builder = builder.clang_arg("-DLINUX");
            builder = builder.clang_arg("-DANDROID");
        }
        if bindings.cplusplus {
            builder = builder.clang_args(&["-x", "c++", "-std=c++11"]);
        }
    }

    builder = builder.clang_args(flags);

    // Apply the configuration.
    for v in &bindings.types {
        builder = builder.allowlist_type(v);
    }
    for v in &bindings.functions {
        builder = builder.allowlist_function(v);
    }
    for v in &bindings.variables {
        builder = builder.allowlist_var(v);
    }
    for v in &bindings.exclude {
        builder = builder.blocklist_item(v);
    }
    for v in &bindings.opaque {
        builder = builder.opaque_type(v);
    }
    for v in &bindings.enums {
        builder = builder.constified_enum_module(v);
    }

    let bindings = builder.generate().expect("unable to generate bindings");
    bindings
        .write_to_file(out)
        .expect("couldn't write bindings");
}

fn setup_standalone() -> Vec<String> {
    setup_clang();

    println!("cargo:rerun-if-env-changed=NSS_DIR");
    let nss = nss_dir();
    build_nss(nss.clone());

    // $NSS_DIR/../dist/
    let nssdist = nss.parent().unwrap().join("dist");
    println!("cargo:rerun-if-env-changed=NSS_TARGET");
    let nsstarget = env::var("NSS_TARGET")
        .unwrap_or_else(|_| fs::read_to_string(nssdist.join("latest")).unwrap());
    let nsstarget = nssdist.join(nsstarget.trim());

    let includes = get_includes(&nsstarget, &nssdist);

    let nsslibdir = nsstarget.join("lib");
    println!(
        "cargo:rustc-link-search=native={}",
        nsslibdir.to_str().unwrap()
    );
    if is_debug() {
        static_link();
    } else {
        dynamic_link();
    }

    let mut flags: Vec<String> = Vec::new();
    for i in includes {
        flags.push(String::from("-I") + i.to_str().unwrap());
    }

    flags
}

#[cfg(feature = "gecko")]
fn setup_for_gecko() -> Vec<String> {
    use mozbuild::TOPOBJDIR;

    let mut flags: Vec<String> = Vec::new();

    let fold_libs = mozbuild::config::MOZ_FOLD_LIBS;
    let libs = if fold_libs {
        vec!["nss3"]
    } else {
        vec!["nssutil3", "nss3", "ssl3", "plds4", "plc4", "nspr4"]
    };

    for lib in &libs {
        println!("cargo:rustc-link-lib=dylib={}", lib);
    }

    if fold_libs {
        println!(
            "cargo:rustc-link-search=native={}",
            TOPOBJDIR.join("security").to_str().unwrap()
        );
    } else {
        println!(
            "cargo:rustc-link-search=native={}",
            TOPOBJDIR.join("dist").join("bin").to_str().unwrap()
        );
        let nsslib_path = TOPOBJDIR.join("security").join("nss").join("lib");
        println!(
            "cargo:rustc-link-search=native={}",
            nsslib_path.join("nss").join("nss_nss3").to_str().unwrap()
        );
        println!(
            "cargo:rustc-link-search=native={}",
            nsslib_path.join("ssl").join("ssl_ssl3").to_str().unwrap()
        );
        println!(
            "cargo:rustc-link-search=native={}",
            TOPOBJDIR
                .join("config")
                .join("external")
                .join("nspr")
                .join("pr")
                .to_str()
                .unwrap()
        );
    }

    let flags_path = TOPOBJDIR.join("netwerk/socket/neqo/extra-bindgen-flags");

    println!("cargo:rerun-if-changed={}", flags_path.to_str().unwrap());
    flags = fs::read_to_string(flags_path)
        .expect("Failed to read extra-bindgen-flags file")
        .split_whitespace()
        .map(std::borrow::ToOwned::to_owned)
        .collect();

    flags.push(String::from("-include"));
    flags.push(
        TOPOBJDIR
            .join("dist")
            .join("include")
            .join("mozilla-config.h")
            .to_str()
            .unwrap()
            .to_string(),
    );
    flags
}

#[cfg(not(feature = "gecko"))]
fn setup_for_gecko() -> Vec<String> {
    unreachable!()
}

fn main() {
    let flags = if cfg!(feature = "gecko") {
        setup_for_gecko()
    } else {
        setup_standalone()
    };

    let config_file = PathBuf::from(BINDINGS_DIR).join(BINDINGS_CONFIG);
    println!("cargo:rerun-if-changed={}", config_file.to_str().unwrap());
    let config = fs::read_to_string(config_file).expect("unable to read binding configuration");
    let config: HashMap<String, Bindings> = ::toml::from_str(&config).unwrap();

    for (k, v) in &config {
        build_bindings(k, v, &flags[..], cfg!(feature = "gecko"));
    }
}
