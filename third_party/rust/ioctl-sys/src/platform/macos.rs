#[cfg(any(target_arch = "aarch64", target_arch = "x86_64"))]
mod consts {
    #[doc(hidden)]
    pub const NONE: u8 = 1;
    #[doc(hidden)]
    pub const READ: u8 = 2;
    #[doc(hidden)]
    pub const WRITE: u8 = 4;
    #[doc(hidden)]
    pub const SIZEBITS: u8 = 13;
    #[doc(hidden)]
    pub const DIRBITS: u8 = 3;
}

#[cfg(not(any(target_arch = "aarch64", target_arch = "x86_64")))]
use this_arch_not_supported;

#[doc(hidden)]
pub use self::consts::*;
