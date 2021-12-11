use std::os::raw::{c_int, c_ulong};

#[cfg(any(target_os = "linux", target_os = "macos", target_os = "android"))]
#[macro_use]
mod platform;

#[cfg(any(target_os = "linux", target_os = "macos", target_os = "android"))]
pub use platform::*;

extern "C" {
    #[doc(hidden)]
    pub fn ioctl(fd: c_int, req: c_ulong, ...) -> c_int;
}

#[doc(hidden)]
pub fn check_res(res: c_int) -> std::io::Result<()> {
    if res < 0 {
        Err(std::io::Error::last_os_error())
    } else {
        Ok(())
    }
}

#[cfg(not(any(target_os = "linux", target_os = "macos", target_os = "android")))]
use platform_not_supported;

#[cfg(doctest)]
mod test_readme {
    macro_rules! external_doc_test {
        ($x:expr) => {
            #[doc = $x]
            extern "C" {}
        };
    }

    external_doc_test!(include_str!("../../README.md"));
}
