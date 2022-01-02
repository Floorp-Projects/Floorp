// Silence invalid warnings due to rust-lang/rust#16719
#![allow(improper_ctypes)]

use Result;
use errno::Errno;
use libc::{self, c_int, c_void, size_t, off_t};
use std::marker::PhantomData;
use std::os::unix::io::RawFd;

pub fn writev(fd: RawFd, iov: &[IoVec<&[u8]>]) -> Result<usize> {
    let res = unsafe { libc::writev(fd, iov.as_ptr() as *const libc::iovec, iov.len() as c_int) };

    Errno::result(res).map(|r| r as usize)
}

pub fn readv(fd: RawFd, iov: &mut [IoVec<&mut [u8]>]) -> Result<usize> {
    let res = unsafe { libc::readv(fd, iov.as_ptr() as *const libc::iovec, iov.len() as c_int) };

    Errno::result(res).map(|r| r as usize)
}

/// Write to `fd` at `offset` from buffers in `iov`.
///
/// Buffers in `iov` will be written in order until all buffers have been written
/// or an error occurs. The file offset is not changed.
///
/// See also: [`writev`](fn.writev.html) and [`pwrite`](fn.pwrite.html)
#[cfg(any(target_os = "dragonfly",
          target_os = "freebsd",
          target_os = "linux",
          target_os = "netbsd",
          target_os = "openbsd"))]
pub fn pwritev(fd: RawFd, iov: &[IoVec<&[u8]>],
               offset: off_t) -> Result<usize> {
    let res = unsafe {
        libc::pwritev(fd, iov.as_ptr() as *const libc::iovec, iov.len() as c_int, offset)
    };

    Errno::result(res).map(|r| r as usize)
}

/// Read from `fd` at `offset` filling buffers in `iov`.
///
/// Buffers in `iov` will be filled in order until all buffers have been filled,
/// no more bytes are available, or an error occurs. The file offset is not
/// changed.
///
/// See also: [`readv`](fn.readv.html) and [`pread`](fn.pread.html)
#[cfg(any(target_os = "dragonfly",
          target_os = "freebsd",
          target_os = "linux",
          target_os = "netbsd",
          target_os = "openbsd"))]
pub fn preadv(fd: RawFd, iov: &[IoVec<&mut [u8]>],
              offset: off_t) -> Result<usize> {
    let res = unsafe {
        libc::preadv(fd, iov.as_ptr() as *const libc::iovec, iov.len() as c_int, offset)
    };

    Errno::result(res).map(|r| r as usize)
}

pub fn pwrite(fd: RawFd, buf: &[u8], offset: off_t) -> Result<usize> {
    let res = unsafe {
        libc::pwrite(fd, buf.as_ptr() as *const c_void, buf.len() as size_t,
                    offset)
    };

    Errno::result(res).map(|r| r as usize)
}

pub fn pread(fd: RawFd, buf: &mut [u8], offset: off_t) -> Result<usize>{
    let res = unsafe {
        libc::pread(fd, buf.as_mut_ptr() as *mut c_void, buf.len() as size_t,
                   offset)
    };

    Errno::result(res).map(|r| r as usize)
}

/// A slice of memory in a remote process, starting at address `base`
/// and consisting of `len` bytes.
///
/// This is the same underlying C structure as [`IoVec`](struct.IoVec.html),
/// except that it refers to memory in some other process, and is
/// therefore not represented in Rust by an actual slice as `IoVec` is. It
/// is used with [`process_vm_readv`](fn.process_vm_readv.html)
/// and [`process_vm_writev`](fn.process_vm_writev.html).
#[cfg(target_os = "linux")]
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub struct RemoteIoVec {
    /// The starting address of this slice (`iov_base`).
    pub base: usize,
    /// The number of bytes in this slice (`iov_len`).
    pub len: usize,
}

/// Write data directly to another process's virtual memory
/// (see [`process_vm_writev`(2)]).
///
/// `local_iov` is a list of [`IoVec`]s containing the data to be written,
/// and `remote_iov` is a list of [`RemoteIoVec`]s identifying where the
/// data should be written in the target process. On success, returns the
/// number of bytes written, which will always be a whole
/// number of `remote_iov` chunks.
///
/// This requires the same permissions as debugging the process using
/// [ptrace]: you must either be a privileged process (with
/// `CAP_SYS_PTRACE`), or you must be running as the same user as the
/// target process and the OS must have unprivileged debugging enabled.
///
/// This function is only available on Linux.
///
/// [`process_vm_writev`(2)]: http://man7.org/linux/man-pages/man2/process_vm_writev.2.html
/// [ptrace]: ../ptrace/index.html
/// [`IoVec`]: struct.IoVec.html
/// [`RemoteIoVec`]: struct.RemoteIoVec.html
#[cfg(target_os = "linux")]
pub fn process_vm_writev(pid: ::unistd::Pid, local_iov: &[IoVec<&[u8]>], remote_iov: &[RemoteIoVec]) -> Result<usize> {
    let res = unsafe {
        libc::process_vm_writev(pid.into(),
                                local_iov.as_ptr() as *const libc::iovec, local_iov.len() as libc::c_ulong,
                                remote_iov.as_ptr() as *const libc::iovec, remote_iov.len() as libc::c_ulong, 0)
    };

    Errno::result(res).map(|r| r as usize)
}

/// Read data directly from another process's virtual memory
/// (see [`process_vm_readv`(2)]).
///
/// `local_iov` is a list of [`IoVec`]s containing the buffer to copy
/// data into, and `remote_iov` is a list of [`RemoteIoVec`]s identifying
/// where the source data is in the target process. On success,
/// returns the number of bytes written, which will always be a whole
/// number of `remote_iov` chunks.
///
/// This requires the same permissions as debugging the process using
/// [`ptrace`]: you must either be a privileged process (with
/// `CAP_SYS_PTRACE`), or you must be running as the same user as the
/// target process and the OS must have unprivileged debugging enabled.
///
/// This function is only available on Linux.
///
/// [`process_vm_readv`(2)]: http://man7.org/linux/man-pages/man2/process_vm_readv.2.html
/// [`ptrace`]: ../ptrace/index.html
/// [`IoVec`]: struct.IoVec.html
/// [`RemoteIoVec`]: struct.RemoteIoVec.html
#[cfg(any(target_os = "linux"))]
pub fn process_vm_readv(pid: ::unistd::Pid, local_iov: &[IoVec<&mut [u8]>], remote_iov: &[RemoteIoVec]) -> Result<usize> {
    let res = unsafe {
        libc::process_vm_readv(pid.into(),
                               local_iov.as_ptr() as *const libc::iovec, local_iov.len() as libc::c_ulong,
                               remote_iov.as_ptr() as *const libc::iovec, remote_iov.len() as libc::c_ulong, 0)
    };

    Errno::result(res).map(|r| r as usize)
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub struct IoVec<T>(libc::iovec, PhantomData<T>);

impl<T> IoVec<T> {
    #[inline]
    pub fn as_slice(&self) -> &[u8] {
        use std::slice;

        unsafe {
            slice::from_raw_parts(
                self.0.iov_base as *const u8,
                self.0.iov_len as usize)
        }
    }
}

impl<'a> IoVec<&'a [u8]> {
    pub fn from_slice(buf: &'a [u8]) -> IoVec<&'a [u8]> {
        IoVec(libc::iovec {
            iov_base: buf.as_ptr() as *mut c_void,
            iov_len: buf.len() as size_t,
        }, PhantomData)
    }
}

impl<'a> IoVec<&'a mut [u8]> {
    pub fn from_mut_slice(buf: &'a mut [u8]) -> IoVec<&'a mut [u8]> {
        IoVec(libc::iovec {
            iov_base: buf.as_ptr() as *mut c_void,
            iov_len: buf.len() as size_t,
        }, PhantomData)
    }
}
