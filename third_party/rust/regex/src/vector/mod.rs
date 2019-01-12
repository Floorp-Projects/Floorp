#[cfg(target_arch = "x86_64")]
pub mod avx2;
#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
pub mod ssse3;
