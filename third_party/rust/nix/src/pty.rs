//! Create master and slave virtual pseudo-terminals (PTYs)

use libc;

pub use libc::pid_t as SessionId;
pub use libc::winsize as Winsize;

use std::ffi::CStr;
use std::mem;
use std::os::unix::prelude::*;

use sys::termios::Termios;
use unistd::ForkResult;
use {Result, Error, fcntl};
use errno::Errno;

/// Representation of a master/slave pty pair
///
/// This is returned by `openpty`.  Note that this type does *not* implement `Drop`, so the user
/// must manually close the file descriptors.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub struct OpenptyResult {
    /// The master port in a virtual pty pair
    pub master: RawFd,
    /// The slave port in a virtual pty pair
    pub slave: RawFd,
}

/// Representation of a master with a forked pty
///
/// This is returned by `forkpty`. Note that this type does *not* implement `Drop`, so the user
/// must manually close the file descriptors.
#[derive(Clone, Copy, Debug)]
pub struct ForkptyResult {
    /// The master port in a virtual pty pair
    pub master: RawFd,
    /// Metadata about forked process
    pub fork_result: ForkResult,
}


/// Representation of the Master device in a master/slave pty pair
///
/// While this datatype is a thin wrapper around `RawFd`, it enforces that the available PTY
/// functions are given the correct file descriptor. Additionally this type implements `Drop`,
/// so that when it's consumed or goes out of scope, it's automatically cleaned-up.
#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub struct PtyMaster(RawFd);

impl AsRawFd for PtyMaster {
    fn as_raw_fd(&self) -> RawFd {
        self.0
    }
}

impl IntoRawFd for PtyMaster {
    fn into_raw_fd(self) -> RawFd {
        let fd = self.0;
        mem::forget(self);
        fd
    }
}

impl Drop for PtyMaster {
    fn drop(&mut self) {
        // On drop, we ignore errors like EINTR and EIO because there's no clear
        // way to handle them, we can't return anything, and (on FreeBSD at
        // least) the file descriptor is deallocated in these cases.  However,
        // we must panic on EBADF, because it is always an error to close an
        // invalid file descriptor.  That frequently indicates a double-close
        // condition, which can cause confusing errors for future I/O
        // operations.
        let e = ::unistd::close(self.0);
        if e == Err(Error::Sys(Errno::EBADF)) {
            panic!("Closing an invalid file descriptor!");
        };
    }
}

/// Grant access to a slave pseudoterminal (see
/// [`grantpt(3)`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/grantpt.html))
///
/// `grantpt()` changes the mode and owner of the slave pseudoterminal device corresponding to the
/// master pseudoterminal referred to by `fd`. This is a necessary step towards opening the slave.
#[inline]
pub fn grantpt(fd: &PtyMaster) -> Result<()> {
    if unsafe { libc::grantpt(fd.as_raw_fd()) } < 0 {
        return Err(Error::last());
    }

    Ok(())
}

/// Open a pseudoterminal device (see
/// [`posix_openpt(3)`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/posix_openpt.html))
///
/// `posix_openpt()` returns a file descriptor to an existing unused pseuterminal master device.
///
/// # Examples
///
/// A common use case with this function is to open both a master and slave PTY pair. This can be
/// done as follows:
///
/// ```
/// use std::path::Path;
/// use nix::fcntl::{OFlag, open};
/// use nix::pty::{grantpt, posix_openpt, ptsname, unlockpt};
/// use nix::sys::stat::Mode;
///
/// # #[allow(dead_code)]
/// # fn run() -> nix::Result<()> {
/// // Open a new PTY master
/// let master_fd = posix_openpt(OFlag::O_RDWR)?;
///
/// // Allow a slave to be generated for it
/// grantpt(&master_fd)?;
/// unlockpt(&master_fd)?;
///
/// // Get the name of the slave
/// let slave_name = unsafe { ptsname(&master_fd) }?;
///
/// // Try to open the slave
/// let _slave_fd = open(Path::new(&slave_name), OFlag::O_RDWR, Mode::empty())?;
/// # Ok(())
/// # }
/// ```
#[inline]
pub fn posix_openpt(flags: fcntl::OFlag) -> Result<PtyMaster> {
    let fd = unsafe {
        libc::posix_openpt(flags.bits())
    };

    if fd < 0 {
        return Err(Error::last());
    }

    Ok(PtyMaster(fd))
}

/// Get the name of the slave pseudoterminal (see
/// [`ptsname(3)`](http://man7.org/linux/man-pages/man3/ptsname.3.html))
///
/// `ptsname()` returns the name of the slave pseudoterminal device corresponding to the master
/// referred to by `fd`.
///
/// This value is useful for opening the slave pty once the master has already been opened with
/// `posix_openpt()`.
///
/// # Safety
///
/// `ptsname()` mutates global variables and is *not* threadsafe.
/// Mutating global variables is always considered `unsafe` by Rust and this
/// function is marked as `unsafe` to reflect that.
///
/// For a threadsafe and non-`unsafe` alternative on Linux, see `ptsname_r()`.
#[inline]
pub unsafe fn ptsname(fd: &PtyMaster) -> Result<String> {
    let name_ptr = libc::ptsname(fd.as_raw_fd());
    if name_ptr.is_null() {
        return Err(Error::last());
    }

    let name = CStr::from_ptr(name_ptr);
    Ok(name.to_string_lossy().into_owned())
}

/// Get the name of the slave pseudoterminal (see
/// [`ptsname(3)`](http://man7.org/linux/man-pages/man3/ptsname.3.html))
///
/// `ptsname_r()` returns the name of the slave pseudoterminal device corresponding to the master
/// referred to by `fd`. This is the threadsafe version of `ptsname()`, but it is not part of the
/// POSIX standard and is instead a Linux-specific extension.
///
/// This value is useful for opening the slave ptty once the master has already been opened with
/// `posix_openpt()`.
#[cfg(any(target_os = "android", target_os = "linux"))]
#[inline]
pub fn ptsname_r(fd: &PtyMaster) -> Result<String> {
    let mut name_buf = vec![0u8; 64];
    let name_buf_ptr = name_buf.as_mut_ptr() as *mut libc::c_char;
    if unsafe { libc::ptsname_r(fd.as_raw_fd(), name_buf_ptr, name_buf.capacity()) } != 0 {
        return Err(Error::last());
    }

    // Find the first null-character terminating this string. This is guaranteed to succeed if the
    // return value of `libc::ptsname_r` is 0.
    let null_index = name_buf.iter().position(|c| *c == b'\0').unwrap();
    name_buf.truncate(null_index);

    let name = String::from_utf8(name_buf)?;
    Ok(name)
}

/// Unlock a pseudoterminal master/slave pseudoterminal pair (see
/// [`unlockpt(3)`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/unlockpt.html))
///
/// `unlockpt()` unlocks the slave pseudoterminal device corresponding to the master pseudoterminal
/// referred to by `fd`. This must be called before trying to open the slave side of a
/// pseuoterminal.
#[inline]
pub fn unlockpt(fd: &PtyMaster) -> Result<()> {
    if unsafe { libc::unlockpt(fd.as_raw_fd()) } < 0 {
        return Err(Error::last());
    }

    Ok(())
}


/// Create a new pseudoterminal, returning the slave and master file descriptors
/// in `OpenptyResult`
/// (see [`openpty`](http://man7.org/linux/man-pages/man3/openpty.3.html)).
///
/// If `winsize` is not `None`, the window size of the slave will be set to
/// the values in `winsize`. If `termios` is not `None`, the pseudoterminal's
/// terminal settings of the slave will be set to the values in `termios`.
#[inline]
pub fn openpty<'a, 'b, T: Into<Option<&'a Winsize>>, U: Into<Option<&'b Termios>>>(winsize: T, termios: U) -> Result<OpenptyResult> {
    use std::ptr;

    let mut slave: libc::c_int = unsafe { mem::uninitialized() };
    let mut master: libc::c_int = unsafe { mem::uninitialized() };
    let ret = {
        match (termios.into(), winsize.into()) {
            (Some(termios), Some(winsize)) => {
                let inner_termios = termios.get_libc_termios();
                unsafe {
                    libc::openpty(
                        &mut master,
                        &mut slave,
                        ptr::null_mut(),
                        &*inner_termios as *const libc::termios as *mut _,
                        winsize as *const Winsize as *mut _,
                    )
                }
            }
            (None, Some(winsize)) => {
                unsafe {
                    libc::openpty(
                        &mut master,
                        &mut slave,
                        ptr::null_mut(),
                        ptr::null_mut(),
                        winsize as *const Winsize as *mut _,
                    )
                }
            }
            (Some(termios), None) => {
                let inner_termios = termios.get_libc_termios();
                unsafe {
                    libc::openpty(
                        &mut master,
                        &mut slave,
                        ptr::null_mut(),
                        &*inner_termios as *const libc::termios as *mut _,
                        ptr::null_mut(),
                    )
                }
            }
            (None, None) => {
                unsafe {
                    libc::openpty(
                        &mut master,
                        &mut slave,
                        ptr::null_mut(),
                        ptr::null_mut(),
                        ptr::null_mut(),
                    )
                }
            }
        }
    };

    Errno::result(ret)?;

    Ok(OpenptyResult {
        master: master,
        slave: slave,
    })
}

/// Create a new pseudoterminal, returning the master file descriptor and forked pid.
/// in `ForkptyResult`
/// (see [`forkpty`](http://man7.org/linux/man-pages/man3/forkpty.3.html)).
///
/// If `winsize` is not `None`, the window size of the slave will be set to
/// the values in `winsize`. If `termios` is not `None`, the pseudoterminal's
/// terminal settings of the slave will be set to the values in `termios`.
pub fn forkpty<'a, 'b, T: Into<Option<&'a Winsize>>, U: Into<Option<&'b Termios>>>(
    winsize: T,
    termios: U,
) -> Result<ForkptyResult> {
    use std::ptr;
    use unistd::Pid;
    use unistd::ForkResult::*;

    let mut master: libc::c_int = unsafe { mem::uninitialized() };

    let term = match termios.into() {
        Some(termios) => {
            let inner_termios = termios.get_libc_termios();
            &*inner_termios as *const libc::termios as *mut _
        },
        None => ptr::null_mut(),
    };

    let win = winsize
        .into()
        .map(|ws| ws as *const Winsize as *mut _)
        .unwrap_or(ptr::null_mut());

    let res = unsafe {
        libc::forkpty(&mut master, ptr::null_mut(), term, win)
    };

    let fork_result = Errno::result(res).map(|res| match res {
        0 => Child,
        res => Parent { child: Pid::from_raw(res) },
    })?;

    Ok(ForkptyResult {
        master: master,
        fork_result: fork_result,
    })
}

