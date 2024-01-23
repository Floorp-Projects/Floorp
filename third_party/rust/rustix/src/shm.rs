//! POSIX shared memory

use crate::fd::OwnedFd;
use crate::{backend, io, path};

pub use crate::backend::fs::types::Mode;
pub use crate::backend::shm::types::ShmOFlags;

/// `shm_open(name, oflags, mode)`—Opens a shared memory object.
///
/// For portability, `name` should begin with a slash, contain no other
/// slashes, and be no longer than an implementation-defined limit (255 on
/// Linux).
///
/// Exactly one of [`ShmOFlags::RDONLY`] and [`ShmOFlags::RDWR`] should be
/// passed. The file descriptor will be opened with `FD_CLOEXEC` set.
///
/// # References
///  - [POSIX]
///  - [Linux]
///
/// [POSIX]: https://pubs.opengroup.org/onlinepubs/9699919799/functions/shm_open.html
/// [Linux]: https://man7.org/linux/man-pages/man3/shm_open.3.html
#[inline]
pub fn shm_open<P: path::Arg>(name: P, flags: ShmOFlags, mode: Mode) -> io::Result<OwnedFd> {
    name.into_with_c_str(|name| backend::shm::syscalls::shm_open(name, flags, mode))
}

/// `shm_unlink(name)`—Unlinks a shared memory object.
///
/// # References
///  - [POSIX]
///  - [Linux]
///
/// [POSIX]: https://pubs.opengroup.org/onlinepubs/9699919799/functions/shm_unlink.html
/// [Linux]: https://man7.org/linux/man-pages/man3/shm_unlink.3.html
#[inline]
pub fn shm_unlink<P: path::Arg>(name: P) -> io::Result<()> {
    name.into_with_c_str(backend::shm::syscalls::shm_unlink)
}
