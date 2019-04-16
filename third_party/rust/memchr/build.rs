use std::env;
use std::ffi::OsString;
use std::process::Command;

fn main() {
    let version = match Version::read() {
        Ok(version) => version,
        Err(err) => {
            eprintln!("failed to parse `rustc --version`: {}", err);
            return;
        }
    };
    enable_simd_optimizations(version);
    enable_libc();
}

// This adds various simd cfgs if this compiler supports it.
//
// This can be disabled with RUSTFLAGS="--cfg memchr_disable_auto_simd", but
// this is generally only intended for testing.
fn enable_simd_optimizations(version: Version) {
    if is_env_set("CARGO_CFG_MEMCHR_DISABLE_AUTO_SIMD") {
        return;
    }
    if version < (Version { major: 1, minor: 27, patch: 0 }) {
        return;
    }

    println!("cargo:rustc-cfg=memchr_runtime_simd");
    println!("cargo:rustc-cfg=memchr_runtime_sse2");
    println!("cargo:rustc-cfg=memchr_runtime_sse42");
    println!("cargo:rustc-cfg=memchr_runtime_avx");
}

// This adds a `memchr_libc` cfg if and only if libc can be used, if no other
// better option is available.
//
// This could be performed in the source code, but it's simpler to do it once
// here and consolidate it into one cfg knob.
//
// Basically, we use libc only if its enabled and if we aren't targeting a
// known bad platform. For example, wasm32 doesn't have a libc and the
// performance of memchr on Windows is seemingly worse than the fallback
// implementation.
fn enable_libc() {
    const NO_ARCH: &'static [&'static str] = &["wasm32", "windows"];
    const NO_ENV: &'static [&'static str] = &["sgx"];

    if !is_feature_set("LIBC") {
        return;
    }

    let arch = match env::var("CARGO_CFG_TARGET_ARCH") {
        Err(_) => return,
        Ok(arch) => arch,
    };
    let env = match env::var("CARGO_CFG_TARGET_ENV") {
        Err(_) => return,
        Ok(env) => env,
    };
    if NO_ARCH.contains(&&*arch) || NO_ENV.contains(&&*env) {
        return;
    }

    println!("cargo:rustc-cfg=memchr_libc");
}

fn is_feature_set(name: &str) -> bool {
    is_env_set(&format!("CARGO_FEATURE_{}",  name))
}

fn is_env_set(name: &str) -> bool {
    env::var_os(name).is_some()
}

#[derive(Clone, Copy, Debug, Eq, PartialEq, PartialOrd, Ord)]
struct Version {
    major: u32,
    minor: u32,
    patch: u32,
}

impl Version {
    fn read() -> Result<Version, String> {
        let rustc = env::var_os("RUSTC").unwrap_or(OsString::from("rustc"));
        let output = Command::new(&rustc)
            .arg("--version")
            .output()
            .unwrap()
            .stdout;
        Version::parse(&String::from_utf8(output).unwrap())
    }

    fn parse(mut s: &str) -> Result<Version, String> {
        if !s.starts_with("rustc ") {
            return Err(format!("unrecognized version string: {}", s));
        }
        s = &s["rustc ".len()..];

        let parts: Vec<&str> = s.split(".").collect();
        if parts.len() < 3 {
            return Err(format!("not enough version parts: {:?}", parts));
        }

        let mut num = String::new();
        for c in parts[0].chars() {
            if !c.is_digit(10) {
                break;
            }
            num.push(c);
        }
        let major = num.parse::<u32>().map_err(|e| e.to_string())?;

        num.clear();
        for c in parts[1].chars() {
            if !c.is_digit(10) {
                break;
            }
            num.push(c);
        }
        let minor = num.parse::<u32>().map_err(|e| e.to_string())?;

        num.clear();
        for c in parts[2].chars() {
            if !c.is_digit(10) {
                break;
            }
            num.push(c);
        }
        let patch = num.parse::<u32>().map_err(|e| e.to_string())?;

        Ok(Version { major, minor, patch })
    }
}
