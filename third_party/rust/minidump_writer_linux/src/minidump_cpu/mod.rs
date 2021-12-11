#[cfg(target_arch = "x86_64")]
#[path = "minidump_cpu_amd64.rs"]
pub mod imp;
#[cfg(target_arch = "x86")]
#[path = "minidump_cpu_x86.rs"]
pub mod imp;
#[cfg(target_arch = "arm")]
#[path = "minidump_cpu_arm.rs"]
pub mod imp;
#[cfg(target_arch = "aarch64")]
#[path = "minidump_cpu_aarch64.rs"]
pub mod imp;
#[cfg(target_arch = "mips")]
#[path = "minidump_cpu_mips.rs"]
pub mod imp;

#[cfg(target_arch = "x86_64")]
pub type RawContextCPU = imp::MDRawContextAMD64;
#[cfg(target_arch = "x86")]
pub type RawContextCPU = imp::MDRawContextX86;
#[cfg(target_arch = "arm")]
pub type RawContextCPU = imp::MDRawContextARM;
#[cfg(target_arch = "aarch64")]
pub type RawContextCPU = imp::MDRawContextX86;
#[cfg(target_arch = "mips")]
pub type RawContextCPU = i32;
