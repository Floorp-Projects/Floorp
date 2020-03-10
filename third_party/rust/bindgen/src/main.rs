extern crate bindgen;
#[cfg(feature = "logging")]
extern crate env_logger;
#[macro_use]
#[cfg(feature = "logging")]
extern crate log;
extern crate clap;

use bindgen::clang_version;
use std::env;
use std::panic;

#[macro_use]
#[cfg(not(feature = "logging"))]
mod log_stubs;

mod options;
use options::builder_from_flags;

fn clang_version_check() {
    let version = clang_version();
    let expected_version = if cfg!(feature = "testing_only_libclang_9") {
        Some((9, 0))
    } else if cfg!(feature = "testing_only_libclang_5") {
        Some((5, 0))
    } else if cfg!(feature = "testing_only_libclang_4") {
        Some((4, 0))
    } else if cfg!(feature = "testing_only_libclang_3_9") {
        Some((3, 9))
    } else if cfg!(feature = "testing_only_libclang_3_8") {
        Some((3, 8))
    } else {
        None
    };

    info!(
        "Clang Version: {}, parsed: {:?}",
        version.full, version.parsed
    );

    if expected_version.is_some() {
        assert_eq!(version.parsed, version.parsed);
    }
}

pub fn main() {
    #[cfg(feature = "logging")]
    env_logger::init();

    let bind_args: Vec<_> = env::args().collect();

    match builder_from_flags(bind_args.into_iter()) {
        Ok((builder, output, verbose)) => {
            clang_version_check();
            let builder_result = panic::catch_unwind(|| {
                builder.generate().expect("Unable to generate bindings")
            });

            if builder_result.is_err() {
                if verbose {
                    print_verbose_err();
                }
                std::process::exit(1);
            }

            let bindings = builder_result.unwrap();
            bindings.write(output).expect("Unable to write output");
        }
        Err(error) => {
            println!("{}", error);
            std::process::exit(1);
        }
    };
}

fn print_verbose_err() {
    println!("Bindgen unexpectedly panicked");
    println!(
        "This may be caused by one of the known-unsupported \
         things (https://rust-lang.github.io/rust-bindgen/cpp.html), \
         please modify the bindgen flags to work around it as \
         described in https://rust-lang.github.io/rust-bindgen/cpp.html"
    );
    println!(
        "Otherwise, please file an issue at \
         https://github.com/rust-lang/rust-bindgen/issues/new"
    );
}
