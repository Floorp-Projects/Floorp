#[cfg(any(
    target_arch = "x86_64",
    target_arch = "x86",
    target_arch = "mips",
    target_arch = "mips64"
))]
#[path = "cpu_info_x86_mips.rs"]
pub mod imp;
#[cfg(any(target_arch = "arm", target_arch = "aarch64"))]
#[path = "cpu_info_arm.rs"]
pub mod imp;

pub use imp::write_cpu_information;

use crate::errors::MemoryWriterError;
use crate::minidump_format::{MDOSPlatform, MDRawSystemInfo};
use crate::sections::write_string_to_location;
use nix::sys::utsname::uname;
use std::io::Cursor;

type Result<T> = std::result::Result<T, MemoryWriterError>;

pub fn write_os_information(
    buffer: &mut Cursor<Vec<u8>>,
    sys_info: &mut MDRawSystemInfo,
) -> Result<()> {
    let info = uname();
    if cfg!(target_os = "android") {
        sys_info.platform_id = MDOSPlatform::Android as u32;
    } else {
        sys_info.platform_id = MDOSPlatform::Linux as u32;
    }
    let merged = vec![
        info.sysname(),
        info.release(),
        info.version(),
        info.machine(),
    ]
    .join(" ");

    let location = write_string_to_location(buffer, &merged)?;
    sys_info.csd_version_rva = location.rva;

    Ok(())
}
