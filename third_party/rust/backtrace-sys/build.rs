extern crate cc;

use std::env;
use std::ffi::OsString;
use std::fs;
use std::io;
use std::path::PathBuf;
use std::process::Command;

macro_rules! t {
    ($e:expr) => (match $e {
        Ok(e) => e,
        Err(e) => panic!("{} failed with {}", stringify!($e), e),
    })
}

fn try_tool(compiler: &cc::Tool, cc: &str, compiler_suffix: &str, tool_suffix: &str)
            -> Option<PathBuf> {
    if !cc.ends_with(compiler_suffix) {
        return None
    }
    let cc = cc.replace(compiler_suffix, tool_suffix);
    let candidate = compiler.path().parent().unwrap().join(cc);
    if Command::new(&candidate).output().is_ok() {
        Some(candidate)
    } else {
        None
    }
}

fn find_tool(compiler: &cc::Tool, cc: &str, tool: &str) -> PathBuf {
    // Allow overrides via env var
    if let Some(s) = env::var_os(tool.to_uppercase()) {
        return s.into()
    }
    let tool_suffix = format!("-{}", tool);
    try_tool(compiler, cc, "-gcc", &tool_suffix)
        .or_else(|| try_tool(compiler, cc, "-clang", &tool_suffix))
        .or_else(|| try_tool(compiler, cc, "-cc", &tool_suffix))
        .unwrap_or_else(|| PathBuf::from(tool))
}

fn main() {
    let src = env::current_dir().unwrap();
    let dst = PathBuf::from(env::var_os("OUT_DIR").unwrap());
    let target = env::var("TARGET").unwrap();
    let host = env::var("HOST").unwrap();

    // libbacktrace doesn't currently support Mach-O files
    if target.contains("darwin") {
        return
    }

    // libbacktrace isn't used on windows
    if target.contains("windows") {
        return
    }

    // no way this will ever compile for emscripten
    if target.contains("emscripten") {
        return
    }

    let mut make = "make";

    // host BSDs has GNU-make as gmake
    if host.contains("bitrig") || host.contains("dragonfly") ||
        host.contains("freebsd") || host.contains("netbsd") ||
        host.contains("openbsd") {

        make = "gmake"
    }

    let configure = src.join("src/libbacktrace/configure").into_os_string();

    // When cross-compiling on Windows, this path will contain backslashes,
    // but configure doesn't like that. Replace them with forward slashes.
    #[cfg(windows)]
    let configure = {
        use std::os::windows::ffi::{OsStrExt, OsStringExt};
        let mut chars: Vec<u16> = configure.encode_wide().collect();
        for c in chars.iter_mut() {
            if *c == '\\' as u16 {
                *c = '/' as u16;
            }
        }
        OsString::from_wide(&chars)
    };

    let cfg = cc::Build::new();
    let compiler = cfg.get_compiler();
    let cc = compiler.path().file_name().unwrap().to_str().unwrap();
    let mut flags = OsString::new();
    for (i, flag) in compiler.args().iter().enumerate() {
        if i > 0 {
            flags.push(" ");
        }
        flags.push(flag);
    }
    let ar = find_tool(&compiler, cc, "ar");
    let ranlib = find_tool(&compiler, cc, "ranlib");
    let mut cmd = Command::new("sh");

    cmd.arg(configure)
       .current_dir(&dst)
       .env("AR", &ar)
       .env("RANLIB", &ranlib)
       .env("CC", compiler.path())
       .env("CFLAGS", flags)
       .arg("--with-pic")
       .arg("--disable-multilib")
       .arg("--disable-shared")
       .arg("--disable-host-shared")
       .arg(format!("--host={}", target));

    // Apparently passing this flag causes problems on Windows
    if !host.contains("windows") {
       cmd.arg(format!("--build={}", host));
    }

    run(&mut cmd, "sh");
    let mut cmd = Command::new(make);
    if let Some(makeflags) = env::var_os("CARGO_MAKEFLAGS") {
        cmd.env("MAKEFLAGS", makeflags);
    }

    run(cmd.current_dir(&dst)
           .arg(format!("INCDIR={}",
                        src.join("src/libbacktrace").display())),
        "make");

    println!("cargo:rustc-link-search=native={}/.libs", dst.display());
    println!("cargo:rustc-link-lib=static=backtrace");

    // The standard library currently bundles in libbacktrace, but it's
    // compiled with hidden visibility (naturally) so we can't use it.
    //
    // To prevent conflicts with a second statically linked copy we rename all
    // symbols with a '__rbt_' prefix manually here through `objcopy`.
    let lib = dst.join(".libs/libbacktrace.a");
    let tmpdir = dst.join("__tmp");
    drop(fs::remove_dir_all(&tmpdir));
    t!(fs::create_dir_all(&tmpdir));
    run(Command::new(&ar).arg("x").arg(&lib).current_dir(&tmpdir),
        ar.to_str().unwrap());

    t!(fs::remove_file(&lib));
    let mut objs = Vec::new();
    let objcopy = find_tool(&compiler, cc, "objcopy");
    for obj in t!(tmpdir.read_dir()) {
        let obj = t!(obj);
        run(Command::new(&objcopy)
                    .arg("--redefine-syms=symbol-map")
                    .arg(obj.path()),
            objcopy.to_str().unwrap());
        objs.push(obj.path());
    }

    run(Command::new(&ar).arg("crus").arg(&lib).args(&objs),
        ar.to_str().unwrap());
}

fn run(cmd: &mut Command, program: &str) {
    println!("running: {:?}", cmd);
    let status = match cmd.status() {
        Ok(s) => s,
        Err(ref e) if e.kind() == io::ErrorKind::NotFound => {
            panic!("\n\nfailed to execute command: {}\nIs `{}` \
                    not installed?\n\n",
                   e,
                   program);
        }
        Err(e) => panic!("failed to get status: {}", e),
    };
    if !status.success() {
        panic!("failed with: {}", status);
    }
}
