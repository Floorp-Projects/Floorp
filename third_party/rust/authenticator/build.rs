#[cfg(all(target_os = "linux", feature = "binding-recompile"))]
extern crate bindgen;

#[cfg(all(target_os = "linux", feature = "binding-recompile"))]
use std::path::PathBuf;

#[cfg(any(not(target_os = "linux"), not(feature = "binding-recompile")))]
fn main() {}

#[cfg(all(target_os = "linux", feature = "binding-recompile"))]
fn main() {
    let bindings = bindgen::Builder::default()
        .header("src/transport/linux/hidwrapper.h")
        .allowlist_var("_HIDIOCGRDESCSIZE")
        .allowlist_var("_HIDIOCGRDESC")
        .generate()
        .expect("Unable to get hidraw bindings");

    let out_path = PathBuf::new();
    let name = if cfg!(target_arch = "x86") {
        "ioctl_x86.rs"
    } else if cfg!(target_arch = "x86_64") {
        "ioctl_x86_64.rs"
    } else if cfg!(all(target_arch = "mips", target_endian = "big")) {
        "ioctl_mipsbe.rs"
    } else if cfg!(all(target_arch = "mips", target_endian = "little")) {
        "ioctl_mipsle.rs"
    } else if cfg!(all(target_arch = "mips64", target_endian = "little")) {
        "ioctl_mips64le.rs"
    } else if cfg!(all(target_arch = "powerpc", target_endian = "little")) {
        "ioctl_powerpcle.rs"
    } else if cfg!(all(target_arch = "powerpc", target_endian = "big")) {
        "ioctl_powerpcbe.rs"
    } else if cfg!(all(target_arch = "powerpc64", target_endian = "little")) {
        "ioctl_powerpc64le.rs"
    } else if cfg!(all(target_arch = "powerpc64", target_endian = "big")) {
        "ioctl_powerpc64be.rs"
    } else if cfg!(all(target_arch = "arm", target_endian = "little")) {
        "ioctl_armle.rs"
    } else if cfg!(all(target_arch = "arm", target_endian = "big")) {
        "ioctl_armbe.rs"
    } else if cfg!(all(target_arch = "aarch64", target_endian = "little")) {
        "ioctl_aarch64le.rs"
    } else if cfg!(all(target_arch = "aarch64", target_endian = "big")) {
        "ioctl_aarch64be.rs"
    } else if cfg!(all(target_arch = "s390x", target_endian = "big")) {
        "ioctl_s390xbe.rs"
    } else if cfg!(all(target_arch = "riscv64", target_endian = "little")) {
        "ioctl_riscv64.rs"
    } else if cfg!(all(target_arch = "loongarch64", target_endian = "little")) {
        "ioctl_loongarch64.rs"
    } else {
        panic!("architecture not supported");
    };
    bindings
        .write_to_file(
            out_path
                .join("src")
                .join("transport")
                .join("linux")
                .join(name),
        )
        .expect("Couldn't write hidraw bindings");
}
