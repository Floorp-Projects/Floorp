//! Filesystem-oriented `ioctl` functions.

#![allow(unsafe_code)]

#[cfg(linux_kernel)]
use {
    crate::fd::AsFd,
    crate::{backend, io, ioctl},
    backend::c,
};

#[cfg(all(linux_kernel, not(any(target_arch = "sparc", target_arch = "sparc64"))))]
use crate::fd::{AsRawFd, BorrowedFd};

/// `ioctl(fd, BLKSSZGET)`—Returns the logical block size of a block device.
///
/// This is mentioned in the [Linux `openat` manual page].
///
/// [Linux `openat` manual page]: https://man7.org/linux/man-pages/man2/openat.2.html
#[cfg(linux_kernel)]
#[inline]
#[doc(alias = "BLKSSZGET")]
pub fn ioctl_blksszget<Fd: AsFd>(fd: Fd) -> io::Result<u32> {
    // SAFETY: BLZSSZGET is a getter opcode that gets a u32.
    unsafe {
        let ctl = ioctl::Getter::<ioctl::BadOpcode<{ c::BLKSSZGET }>, c::c_uint>::new();
        ioctl::ioctl(fd, ctl)
    }
}

/// `ioctl(fd, BLKPBSZGET)`—Returns the physical block size of a block device.
#[cfg(linux_kernel)]
#[inline]
#[doc(alias = "BLKPBSZGET")]
pub fn ioctl_blkpbszget<Fd: AsFd>(fd: Fd) -> io::Result<u32> {
    // SAFETY: BLKPBSZGET is a getter opcode that gets a u32.
    unsafe {
        let ctl = ioctl::Getter::<ioctl::BadOpcode<{ c::BLKPBSZGET }>, c::c_uint>::new();
        ioctl::ioctl(fd, ctl)
    }
}

/// `ioctl(fd, FICLONE, src_fd)`—Share data between open files.
///
/// This ioctl is not available on Sparc platforms
///
/// # References
///  - [Linux]
///
/// [Linux]: https://man7.org/linux/man-pages/man2/ioctl_ficlone.2.html
#[cfg(all(linux_kernel, not(any(target_arch = "sparc", target_arch = "sparc64"))))]
#[inline]
#[doc(alias = "FICLONE")]
pub fn ioctl_ficlone<Fd: AsFd, SrcFd: AsFd>(fd: Fd, src_fd: SrcFd) -> io::Result<()> {
    unsafe { ioctl::ioctl(fd, Ficlone(src_fd.as_fd())) }
}

/// `ioctl(fd, EXT4_IOC_RESIZE_FS, blocks)`—Resize ext4 filesystem on fd.
#[cfg(linux_kernel)]
#[inline]
#[doc(alias = "EXT4_IOC_RESIZE_FS")]
pub fn ext4_ioc_resize_fs<Fd: AsFd>(fd: Fd, blocks: u64) -> io::Result<()> {
    // SAFETY: EXT4_IOC_RESIZE_FS is a pointer setter opcode.
    unsafe {
        let ctl = ioctl::Setter::<ioctl::BadOpcode<{ backend::fs::EXT4_IOC_RESIZE_FS }>, u64>::new(
            blocks,
        );
        ioctl::ioctl(fd, ctl)
    }
}

#[cfg(all(linux_kernel, not(any(target_arch = "sparc", target_arch = "sparc64"))))]
struct Ficlone<'a>(BorrowedFd<'a>);

#[cfg(all(linux_kernel, not(any(target_arch = "sparc", target_arch = "sparc64"))))]
unsafe impl ioctl::Ioctl for Ficlone<'_> {
    type Output = ();

    const IS_MUTATING: bool = false;
    const OPCODE: ioctl::Opcode = ioctl::Opcode::old(c::FICLONE as ioctl::RawOpcode);

    fn as_ptr(&mut self) -> *mut c::c_void {
        self.0.as_raw_fd() as *mut c::c_void
    }

    unsafe fn output_from_ptr(
        _: ioctl::IoctlOutput,
        _: *mut c::c_void,
    ) -> io::Result<Self::Output> {
        Ok(())
    }
}
