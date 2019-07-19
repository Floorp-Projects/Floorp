extern crate rustc_version;
use rustc_version::{version, version_meta, Channel};


fn main() {
    // Assert we haven't travelled back in time
    assert!(version().unwrap().major >= 1);

    // Set cfg flags depending on release channel
    match version_meta().unwrap().channel {
        Channel::Stable => {
            println!("cargo:rustc-cfg=rustc_stable");
        }
        Channel::Beta => {
            println!("cargo:rustc-cfg=rustc_beta");
        }
        Channel::Nightly => {
            println!("cargo:rustc-cfg=rustc_nightly");
        }
        Channel::Dev => {
            println!("cargo:rustc-cfg=rustc_dev");
        }
    }
}
