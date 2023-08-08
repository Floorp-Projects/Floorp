#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
// sadly we need this file so we can avoid the suprious warnings that
// would come with bindgen, as well as to avoid cluttering the mod.rs
// with spurious architecture specific modules.

#[cfg(target_arch = "x86")]
include!("ioctl_x86.rs");

#[cfg(target_arch = "x86_64")]
include!("ioctl_x86_64.rs");

#[cfg(all(target_arch = "mips", target_endian = "little"))]
include!("ioctl_mipsle.rs");

#[cfg(all(target_arch = "mips", target_endian = "big"))]
include!("ioctl_mipsbe.rs");

#[cfg(all(target_arch = "mips64", target_endian = "little"))]
include!("ioctl_mips64le.rs");

#[cfg(all(target_arch = "powerpc", target_endian = "little"))]
include!("ioctl_powerpcle.rs");

#[cfg(all(target_arch = "powerpc", target_endian = "big"))]
include!("ioctl_powerpcbe.rs");

#[cfg(all(target_arch = "powerpc64", target_endian = "little"))]
include!("ioctl_powerpc64le.rs");

#[cfg(all(target_arch = "powerpc64", target_endian = "big"))]
include!("ioctl_powerpc64be.rs");

#[cfg(all(target_arch = "arm", target_endian = "little"))]
include!("ioctl_armle.rs");

#[cfg(all(target_arch = "arm", target_endian = "big"))]
include!("ioctl_armbe.rs");

#[cfg(all(target_arch = "aarch64", target_endian = "little"))]
include!("ioctl_aarch64le.rs");

#[cfg(all(target_arch = "aarch64", target_endian = "big"))]
include!("ioctl_aarch64be.rs");

#[cfg(all(target_arch = "s390x", target_endian = "big"))]
include!("ioctl_s390xbe.rs");

#[cfg(all(target_arch = "riscv64", target_endian = "little"))]
include!("ioctl_riscv64.rs");

#[cfg(all(target_arch = "loongarch64", target_endian = "little"))]
include!("ioctl_loongarch64.rs");
