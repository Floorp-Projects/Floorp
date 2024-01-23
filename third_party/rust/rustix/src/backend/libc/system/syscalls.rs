//! libc syscalls supporting `rustix::process`.

use super::types::RawUname;
use crate::backend::c;
#[cfg(not(target_os = "wasi"))]
use crate::backend::conv::ret_infallible;
#[cfg(target_os = "linux")]
use crate::system::RebootCommand;
#[cfg(linux_kernel)]
use crate::system::Sysinfo;
use core::mem::MaybeUninit;
#[cfg(not(any(
    target_os = "emscripten",
    target_os = "espidf",
    target_os = "redox",
    target_os = "vita",
    target_os = "wasi"
)))]
use {crate::backend::conv::ret, crate::io};

#[cfg(not(target_os = "wasi"))]
#[inline]
pub(crate) fn uname() -> RawUname {
    let mut uname = MaybeUninit::<RawUname>::uninit();
    unsafe {
        let r = c::uname(uname.as_mut_ptr());

        // On POSIX, `uname` is documented to return non-negative on success
        // instead of the usual 0, though some specific systems do document
        // that they always use zero allowing us to skip this check.
        #[cfg(not(any(apple, freebsdlike, linux_like, target_os = "netbsd")))]
        let r = core::cmp::min(r, 0);

        ret_infallible(r);
        uname.assume_init()
    }
}

#[cfg(linux_kernel)]
pub(crate) fn sysinfo() -> Sysinfo {
    let mut info = MaybeUninit::<Sysinfo>::uninit();
    unsafe {
        ret_infallible(c::sysinfo(info.as_mut_ptr()));
        info.assume_init()
    }
}

#[cfg(not(any(
    target_os = "emscripten",
    target_os = "espidf",
    target_os = "redox",
    target_os = "vita",
    target_os = "wasi"
)))]
pub(crate) fn sethostname(name: &[u8]) -> io::Result<()> {
    unsafe {
        ret(c::sethostname(
            name.as_ptr().cast(),
            name.len().try_into().map_err(|_| io::Errno::INVAL)?,
        ))
    }
}

#[cfg(target_os = "linux")]
pub(crate) fn reboot(cmd: RebootCommand) -> io::Result<()> {
    unsafe { ret(c::reboot(cmd as i32)) }
}
