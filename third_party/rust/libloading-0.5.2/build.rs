extern crate cc;

use std::io::Write;
use std::env;

fn main(){
    let target_os = env::var("CARGO_CFG_TARGET_OS");
    let is_unix = env::var_os("CARGO_CFG_UNIX").is_some();
    match target_os.as_ref().map(|x| &**x) {
        Ok("linux") | Ok("android") => println!("cargo:rustc-link-lib=dl"),
        Ok("freebsd") | Ok("dragonfly") => println!("cargo:rustc-link-lib=c"),
        // netbsd claims dl* will be available to any dynamically linked binary, but I haven’t
        // found any libraries that have to be linked to on other platforms.
        // What happens if the executable is not linked up dynamically?
        Ok("openbsd") | Ok("bitrig") | Ok("netbsd") | Ok("macos") | Ok("ios") => {}
        Ok("solaris") => {}
        Ok("haiku") => {}
        // dependencies come with winapi
        Ok("windows") => {}
        tos => {
            writeln!(::std::io::stderr(),
                     "Building for an unknown target_os=`{:?}`!\nPlease report an issue ",
                     tos).expect("could not report the error");
            ::std::process::exit(0xfc);
        }
    }
    if is_unix {
        cc::Build::new()
            .file("src/os/unix/global_static.c")
            .compile("global_static");
    }
}
