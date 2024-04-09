// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{
    collections::HashMap,
    env, fs,
    path::{Path, PathBuf},
    process::Command,
};

use bindgen::Builder;
use semver::{Version, VersionReq};
use serde_derive::Deserialize;

#[path = "src/min_version.rs"]
mod min_version;
use min_version::MINIMUM_NSS_VERSION;

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
    /// formed with an underscore as <`enum_type`>_<`enum_value_name`>).
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
    // Check the build profile and not whether debug symbols are enabled (i.e.,
    // `env::var("DEBUG")`), because we enable those for benchmarking/profiling and still want
    // to build NSS in release mode.
    env::var("PROFILE").unwrap_or_default() == "debug"
}

// bindgen needs access to libclang.
// On windows, this doesn't just work, you have to set LIBCLANG_PATH.
// Rather than download the 400Mb+ files, like gecko does, let's just reuse their work.
fn setup_clang() {
    // If this isn't Windows, or we're in CI, then we don't need to do anything.
    if env::consts::OS != "windows" || env::var("GITHUB_WORKFLOW").unwrap() == "CI" {
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

fn get_bash() -> PathBuf {
    // If BASH is set, use that.
    if let Ok(bash) = env::var("BASH") {
        return PathBuf::from(bash);
    }

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
        // Generate static libraries in addition to shared libraries.
        String::from("--static"),
    ];
    if !is_debug() {
        build_nss.push(String::from("-o"));
    }
    if let Ok(d) = env::var("NSS_JOBS") {
        build_nss.push(String::from("-j"));
        build_nss.push(d);
    }
    let target = env::var("TARGET").unwrap();
    if target.strip_prefix("aarch64-").is_some() {
        build_nss.push(String::from("--target=arm64"));
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
        println!("cargo:rustc-link-lib=dylib={lib}");
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
        println!("cargo:rustc-link-lib=static={lib}");
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

    println!("cargo:rerun-if-changed={header}");

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
            builder = builder.clang_args(&["-x", "c++", "-std=c++14"]);
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

fn pkg_config() -> Vec<String> {
    let modversion = Command::new("pkg-config")
        .args(["--modversion", "nss"])
        .output()
        .expect("pkg-config reports NSS as absent")
        .stdout;
    let modversion = String::from_utf8(modversion).expect("non-UTF8 from pkg-config");
    let modversion = modversion.trim();
    // The NSS version number does not follow semver numbering, because it omits the patch version
    // when that's 0. Deal with that.
    let modversion_for_cmp = if modversion.chars().filter(|c| *c == '.').count() == 1 {
        modversion.to_owned() + ".0"
    } else {
        modversion.to_owned()
    };
    let modversion_for_cmp =
        Version::parse(&modversion_for_cmp).expect("NSS version not in semver format");
    let version_req = VersionReq::parse(&format!(">={}", MINIMUM_NSS_VERSION.trim())).unwrap();
    assert!(
        version_req.matches(&modversion_for_cmp),
        "neqo has NSS version requirement {version_req}, found {modversion}"
    );

    let cfg = Command::new("pkg-config")
        .args(["--cflags", "--libs", "nss"])
        .output()
        .expect("NSS flags not returned by pkg-config")
        .stdout;
    let cfg_str = String::from_utf8(cfg).expect("non-UTF8 from pkg-config");

    let mut flags: Vec<String> = Vec::new();
    for f in cfg_str.split(' ') {
        if let Some(include) = f.strip_prefix("-I") {
            flags.push(String::from(f));
            println!("cargo:include={include}");
        } else if let Some(path) = f.strip_prefix("-L") {
            println!("cargo:rustc-link-search=native={path}");
        } else if let Some(lib) = f.strip_prefix("-l") {
            println!("cargo:rustc-link-lib=dylib={lib}");
        } else {
            println!("Warning: Unknown flag from pkg-config: {f}");
        }
    }

    flags
}

fn setup_standalone(nss: &str) -> Vec<String> {
    setup_clang();

    println!("cargo:rerun-if-env-changed=NSS_DIR");
    let nss = PathBuf::from(nss);
    assert!(
        !nss.is_relative(),
        "The NSS_DIR environment variable is expected to be an absolute path."
    );

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
    if is_debug() || env::consts::OS == "windows" {
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
    let mut flags = fs::read_to_string(flags_path)
        .expect("Failed to read extra-bindgen-flags file")
        .split_whitespace()
        .map(String::from)
        .collect::<Vec<_>>();

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
    } else if let Ok(nss_dir) = env::var("NSS_DIR") {
        setup_standalone(nss_dir.trim())
    } else {
        pkg_config()
    };

    let config_file = PathBuf::from(BINDINGS_DIR).join(BINDINGS_CONFIG);
    println!("cargo:rerun-if-changed={}", config_file.to_str().unwrap());
    let config = fs::read_to_string(config_file).expect("unable to read binding configuration");
    let config: HashMap<String, Bindings> = ::toml::from_str(&config).unwrap();

    for (k, v) in &config {
        build_bindings(k, v, &flags[..], cfg!(feature = "gecko"));
    }
}
