use crate::errno::Errno;
use crate::Result;
use std::os::unix::io::{FromRawFd, OwnedFd};

libc_bitflags! {
    pub struct EfdFlags: libc::c_int {
        EFD_CLOEXEC; // Since Linux 2.6.27
        EFD_NONBLOCK; // Since Linux 2.6.27
        EFD_SEMAPHORE; // Since Linux 2.6.30
    }
}

pub fn eventfd(initval: libc::c_uint, flags: EfdFlags) -> Result<OwnedFd> {
    let res = unsafe { libc::eventfd(initval, flags.bits()) };

    Errno::result(res).map(|r| unsafe { OwnedFd::from_raw_fd(r) })
}
