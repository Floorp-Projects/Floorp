use std::mem;
use std::os::unix::io::RawFd;
use std::ptr::{null, null_mut};
use libc::{self, c_int};
use Result;
use errno::Errno;
use sys::signal::SigSet;
use sys::time::{TimeSpec, TimeVal};

pub use libc::FD_SETSIZE;

// FIXME: Change to repr(transparent) once it's stable
#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub struct FdSet(libc::fd_set);

impl FdSet {
    pub fn new() -> FdSet {
        let mut fdset = unsafe { mem::uninitialized() };
        unsafe { libc::FD_ZERO(&mut fdset) };
        FdSet(fdset)
    }

    pub fn insert(&mut self, fd: RawFd) {
        unsafe { libc::FD_SET(fd, &mut self.0) };
    }

    pub fn remove(&mut self, fd: RawFd) {
        unsafe { libc::FD_CLR(fd, &mut self.0) };
    }

    pub fn contains(&mut self, fd: RawFd) -> bool {
        unsafe { libc::FD_ISSET(fd, &mut self.0) }
    }

    pub fn clear(&mut self) {
        unsafe { libc::FD_ZERO(&mut self.0) };
    }

    /// Finds the highest file descriptor in the set.
    ///
    /// Returns `None` if the set is empty.
    ///
    /// This can be used to calculate the `nfds` parameter of the [`select`] function.
    ///
    /// # Example
    ///
    /// ```
    /// # extern crate nix;
    /// # use nix::sys::select::FdSet;
    /// # fn main() {
    /// let mut set = FdSet::new();
    /// set.insert(4);
    /// set.insert(9);
    /// assert_eq!(set.highest(), Some(9));
    /// # }
    /// ```
    ///
    /// [`select`]: fn.select.html
    pub fn highest(&mut self) -> Option<RawFd> {
        for i in (0..FD_SETSIZE).rev() {
            let i = i as RawFd;
            if unsafe { libc::FD_ISSET(i, self as *mut _ as *mut libc::fd_set) } {
                return Some(i)
            }
        }

        None
    }
}

/// Monitors file descriptors for readiness
///
/// Returns the total number of ready file descriptors in all sets. The sets are changed so that all
/// file descriptors that are ready for the given operation are set.
///
/// When this function returns, `timeout` has an implementation-defined value.
///
/// # Parameters
///
/// * `nfds`: The highest file descriptor set in any of the passed `FdSet`s, plus 1. If `None`, this
///   is calculated automatically by calling [`FdSet::highest`] on all descriptor sets and adding 1
///   to the maximum of that.
/// * `readfds`: File descriptors to check for being ready to read.
/// * `writefds`: File descriptors to check for being ready to write.
/// * `errorfds`: File descriptors to check for pending error conditions.
/// * `timeout`: Maximum time to wait for descriptors to become ready (`None` to block
///   indefinitely).
///
/// # References
///
/// [select(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/select.html)
///
/// [`FdSet::highest`]: struct.FdSet.html#method.highest
pub fn select<'a, N, R, W, E, T>(nfds: N,
                                 readfds: R,
                                 writefds: W,
                                 errorfds: E,
                                 timeout: T) -> Result<c_int>
where
    N: Into<Option<c_int>>,
    R: Into<Option<&'a mut FdSet>>,
    W: Into<Option<&'a mut FdSet>>,
    E: Into<Option<&'a mut FdSet>>,
    T: Into<Option<&'a mut TimeVal>>,
{
    let mut readfds = readfds.into();
    let mut writefds = writefds.into();
    let mut errorfds = errorfds.into();
    let timeout = timeout.into();

    let nfds = nfds.into().unwrap_or_else(|| {
        readfds.iter_mut()
            .chain(writefds.iter_mut())
            .chain(errorfds.iter_mut())
            .map(|set| set.highest().unwrap_or(-1))
            .max()
            .unwrap_or(-1) + 1
    });

    let readfds = readfds.map(|set| set as *mut _ as *mut libc::fd_set).unwrap_or(null_mut());
    let writefds = writefds.map(|set| set as *mut _ as *mut libc::fd_set).unwrap_or(null_mut());
    let errorfds = errorfds.map(|set| set as *mut _ as *mut libc::fd_set).unwrap_or(null_mut());
    let timeout = timeout.map(|tv| tv as *mut _ as *mut libc::timeval)
                         .unwrap_or(null_mut());

    let res = unsafe {
        libc::select(nfds, readfds, writefds, errorfds, timeout)
    };

    Errno::result(res)
}

/// Monitors file descriptors for readiness with an altered signal mask.
///
/// Returns the total number of ready file descriptors in all sets. The sets are changed so that all
/// file descriptors that are ready for the given operation are set.
///
/// When this function returns, the original signal mask is restored.
///
/// Unlike [`select`](#fn.select), `pselect` does not mutate the `timeout` value.
///
/// # Parameters
///
/// * `nfds`: The highest file descriptor set in any of the passed `FdSet`s, plus 1. If `None`, this
///   is calculated automatically by calling [`FdSet::highest`] on all descriptor sets and adding 1
///   to the maximum of that.
/// * `readfds`: File descriptors to check for read readiness
/// * `writefds`: File descriptors to check for write readiness
/// * `errorfds`: File descriptors to check for pending error conditions.
/// * `timeout`: Maximum time to wait for descriptors to become ready (`None` to block
///   indefinitely).
/// * `sigmask`: Signal mask to activate while waiting for file descriptors to turn
///    ready (`None` to set no alternative signal mask).
///
/// # References
///
/// [pselect(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/pselect.html)
///
/// [The new pselect() system call](https://lwn.net/Articles/176911/)
///
/// [`FdSet::highest`]: struct.FdSet.html#method.highest
pub fn pselect<'a, N, R, W, E, T, S>(nfds: N,
                                     readfds: R,
                                     writefds: W,
                                     errorfds: E,
                                     timeout: T,
                                     sigmask: S) -> Result<c_int>
where
    N: Into<Option<c_int>>,
    R: Into<Option<&'a mut FdSet>>,
    W: Into<Option<&'a mut FdSet>>,
    E: Into<Option<&'a mut FdSet>>,
    T: Into<Option<&'a TimeSpec>>,
    S: Into<Option<&'a SigSet>>,
{
    let mut readfds = readfds.into();
    let mut writefds = writefds.into();
    let mut errorfds = errorfds.into();
    let sigmask = sigmask.into();
    let timeout = timeout.into();

    let nfds = nfds.into().unwrap_or_else(|| {
        readfds.iter_mut()
            .chain(writefds.iter_mut())
            .chain(errorfds.iter_mut())
            .map(|set| set.highest().unwrap_or(-1))
            .max()
            .unwrap_or(-1) + 1
    });

    let readfds = readfds.map(|set| set as *mut _ as *mut libc::fd_set).unwrap_or(null_mut());
    let writefds = writefds.map(|set| set as *mut _ as *mut libc::fd_set).unwrap_or(null_mut());
    let errorfds = errorfds.map(|set| set as *mut _ as *mut libc::fd_set).unwrap_or(null_mut());
    let timeout = timeout.map(|ts| ts.as_ref() as *const libc::timespec).unwrap_or(null());
    let sigmask = sigmask.map(|sm| sm.as_ref() as *const libc::sigset_t).unwrap_or(null());

    let res = unsafe {
        libc::pselect(nfds, readfds, writefds, errorfds, timeout, sigmask)
    };

    Errno::result(res)
}


#[cfg(test)]
mod tests {
    use super::*;
    use std::os::unix::io::RawFd;
    use sys::time::{TimeVal, TimeValLike};
    use unistd::{write, pipe};

    #[test]
    fn fdset_insert() {
        let mut fd_set = FdSet::new();

        for i in 0..FD_SETSIZE {
            assert!(!fd_set.contains(i as RawFd));
        }

        fd_set.insert(7);

        assert!(fd_set.contains(7));
    }

    #[test]
    fn fdset_remove() {
        let mut fd_set = FdSet::new();

        for i in 0..FD_SETSIZE {
            assert!(!fd_set.contains(i as RawFd));
        }

        fd_set.insert(7);
        fd_set.remove(7);

        for i in 0..FD_SETSIZE {
            assert!(!fd_set.contains(i as RawFd));
        }
    }

    #[test]
    fn fdset_clear() {
        let mut fd_set = FdSet::new();
        fd_set.insert(1);
        fd_set.insert((FD_SETSIZE / 2) as RawFd);
        fd_set.insert((FD_SETSIZE - 1) as RawFd);

        fd_set.clear();

        for i in 0..FD_SETSIZE {
            assert!(!fd_set.contains(i as RawFd));
        }
    }

    #[test]
    fn fdset_highest() {
        let mut set = FdSet::new();
        assert_eq!(set.highest(), None);
        set.insert(0);
        assert_eq!(set.highest(), Some(0));
        set.insert(90);
        assert_eq!(set.highest(), Some(90));
        set.remove(0);
        assert_eq!(set.highest(), Some(90));
        set.remove(90);
        assert_eq!(set.highest(), None);

        set.insert(4);
        set.insert(5);
        set.insert(7);
        assert_eq!(set.highest(), Some(7));
    }

    #[test]
    fn test_select() {
        let (r1, w1) = pipe().unwrap();
        write(w1, b"hi!").unwrap();
        let (r2, _w2) = pipe().unwrap();

        let mut fd_set = FdSet::new();
        fd_set.insert(r1);
        fd_set.insert(r2);

        let mut timeout = TimeVal::seconds(10);
        assert_eq!(1, select(None,
                             &mut fd_set,
                             None,
                             None,
                             &mut timeout).unwrap());
        assert!(fd_set.contains(r1));
        assert!(!fd_set.contains(r2));
    }

    #[test]
    fn test_select_nfds() {
        let (r1, w1) = pipe().unwrap();
        write(w1, b"hi!").unwrap();
        let (r2, _w2) = pipe().unwrap();

        let mut fd_set = FdSet::new();
        fd_set.insert(r1);
        fd_set.insert(r2);

        let mut timeout = TimeVal::seconds(10);
        assert_eq!(1, select(Some(fd_set.highest().unwrap() + 1),
                             &mut fd_set,
                             None,
                             None,
                             &mut timeout).unwrap());
        assert!(fd_set.contains(r1));
        assert!(!fd_set.contains(r2));
    }

    #[test]
    fn test_select_nfds2() {
        let (r1, w1) = pipe().unwrap();
        write(w1, b"hi!").unwrap();
        let (r2, _w2) = pipe().unwrap();

        let mut fd_set = FdSet::new();
        fd_set.insert(r1);
        fd_set.insert(r2);

        let mut timeout = TimeVal::seconds(10);
        assert_eq!(1, select(::std::cmp::max(r1, r2) + 1,
                             &mut fd_set,
                             None,
                             None,
                             &mut timeout).unwrap());
        assert!(fd_set.contains(r1));
        assert!(!fd_set.contains(r2));
    }
}
