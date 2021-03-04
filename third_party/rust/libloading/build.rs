use std::io::Write;
use std::env;

fn dlerror_is_mtsafe(target_os: &str) {
    match target_os {
        // Confirmed MT-safe:
        "linux"
        | "android"
        | "openbsd"
        | "macos"
        | "ios"
        | "solaris"
        | "illumos"
        | "redox"
        | "fuchsia" => {
            println!("cargo:rustc-cfg=mtsafe_dlerror");
        }
        // Confirmed not MT-safe:
        "freebsd"
        | "dragonfly"
        | "netbsd"
        | "bitrig"
        | "haiku" => {}
        // Unknown:
        _ => {}
    }
}

fn link_libraries(target_os: &str) {
    match target_os {
        "linux" | "android" => println!("cargo:rustc-link-lib=dl"),
        "freebsd" | "dragonfly" => println!("cargo:rustc-link-lib=c"),
        // netbsd claims dl* will be available to any dynamically linked binary, but I haven’t
        // found any libraries that have to be linked to on other platforms.
        // What happens if the executable is not linked up dynamically?
        "openbsd" | "bitrig" | "netbsd" | "macos" | "ios" => {}
        "solaris" | "illumos" => {}
        "haiku" => {}
        "redox" => {}
        "fuchsia" => {}
        // dependencies come with winapi
        "windows" => {}
        tos => {
            writeln!(::std::io::stderr(),
                     "Building for an unknown target_os=`{:?}`!\nPlease report an issue ",
                     tos).expect("could not report the error");
            ::std::process::exit(0xfc);
        }
    }
}

fn main() {
    match env::var("CARGO_CFG_TARGET_OS") {
        Ok(target_os) => {
            dlerror_is_mtsafe(&target_os);
            link_libraries(&target_os);
        }
        Err(e) => {
            writeln!(::std::io::stderr(),
                     "Unable to get target_os=`{}`!", e).expect("could not report the error");
            ::std::process::exit(0xfd);
        }
    }
}
