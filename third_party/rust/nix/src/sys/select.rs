//! Portably monitor a group of file descriptors for readiness.
use crate::errno::Errno;
use crate::sys::time::{TimeSpec, TimeVal};
use crate::Result;
use libc::{self, c_int};
use std::convert::TryFrom;
use std::iter::FusedIterator;
use std::mem;
use std::ops::Range;
use std::os::unix::io::{AsFd, AsRawFd, BorrowedFd, RawFd};
use std::ptr::{null, null_mut};

pub use libc::FD_SETSIZE;

/// Contains a set of file descriptors used by [`select`]
#[repr(transparent)]
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub struct FdSet<'fd> {
    set: libc::fd_set,
    _fd: std::marker::PhantomData<BorrowedFd<'fd>>,
}

fn assert_fd_valid(fd: RawFd) {
    assert!(
        usize::try_from(fd).map_or(false, |fd| fd < FD_SETSIZE),
        "fd must be in the range 0..FD_SETSIZE",
    );
}

impl<'fd> FdSet<'fd> {
    /// Create an empty `FdSet`
    pub fn new() -> FdSet<'fd> {
        let mut fdset = mem::MaybeUninit::uninit();
        unsafe {
            libc::FD_ZERO(fdset.as_mut_ptr());
            Self {
                set: fdset.assume_init(),
                _fd: std::marker::PhantomData,
            }
        }
    }

    /// Add a file descriptor to an `FdSet`
    pub fn insert<Fd: AsFd>(&mut self, fd: &'fd Fd) {
        assert_fd_valid(fd.as_fd().as_raw_fd());
        unsafe { libc::FD_SET(fd.as_fd().as_raw_fd(), &mut self.set) };
    }

    /// Remove a file descriptor from an `FdSet`
    pub fn remove<Fd: AsFd>(&mut self, fd: &'fd Fd) {
        assert_fd_valid(fd.as_fd().as_raw_fd());
        unsafe { libc::FD_CLR(fd.as_fd().as_raw_fd(), &mut self.set) };
    }

    /// Test an `FdSet` for the presence of a certain file descriptor.
    pub fn contains<Fd: AsFd>(&self, fd: &'fd Fd) -> bool {
        assert_fd_valid(fd.as_fd().as_raw_fd());
        unsafe { libc::FD_ISSET(fd.as_fd().as_raw_fd(), &self.set) }
    }

    /// Remove all file descriptors from this `FdSet`.
    pub fn clear(&mut self) {
        unsafe { libc::FD_ZERO(&mut self.set) };
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
    /// # use std::os::unix::io::{AsRawFd, BorrowedFd};
    /// # use nix::sys::select::FdSet;
    /// let fd_four = unsafe {BorrowedFd::borrow_raw(4)};
    /// let fd_nine = unsafe {BorrowedFd::borrow_raw(9)};
    /// let mut set = FdSet::new();
    /// set.insert(&fd_four);
    /// set.insert(&fd_nine);
    /// assert_eq!(set.highest().map(|borrowed_fd|borrowed_fd.as_raw_fd()), Some(9));
    /// ```
    ///
    /// [`select`]: fn.select.html
    pub fn highest(&self) -> Option<BorrowedFd<'_>> {
        self.fds(None).next_back()
    }

    /// Returns an iterator over the file descriptors in the set.
    ///
    /// For performance, it takes an optional higher bound: the iterator will
    /// not return any elements of the set greater than the given file
    /// descriptor.
    ///
    /// # Examples
    ///
    /// ```
    /// # use nix::sys::select::FdSet;
    /// # use std::os::unix::io::{AsRawFd, BorrowedFd, RawFd};
    /// let mut set = FdSet::new();
    /// let fd_four = unsafe {BorrowedFd::borrow_raw(4)};
    /// let fd_nine = unsafe {BorrowedFd::borrow_raw(9)};
    /// set.insert(&fd_four);
    /// set.insert(&fd_nine);
    /// let fds: Vec<RawFd> = set.fds(None).map(|borrowed_fd|borrowed_fd.as_raw_fd()).collect();
    /// assert_eq!(fds, vec![4, 9]);
    /// ```
    #[inline]
    pub fn fds(&self, highest: Option<RawFd>) -> Fds {
        Fds {
            set: self,
            range: 0..highest.map(|h| h as usize + 1).unwrap_or(FD_SETSIZE),
        }
    }
}

impl<'fd> Default for FdSet<'fd> {
    fn default() -> Self {
        Self::new()
    }
}

/// Iterator over `FdSet`.
#[derive(Debug)]
pub struct Fds<'a, 'fd> {
    set: &'a FdSet<'fd>,
    range: Range<usize>,
}

impl<'a, 'fd> Iterator for Fds<'a, 'fd> {
    type Item = BorrowedFd<'fd>;

    fn next(&mut self) -> Option<Self::Item> {
        for i in &mut self.range {
            let borrowed_i = unsafe { BorrowedFd::borrow_raw(i as RawFd) };
            if self.set.contains(&borrowed_i) {
                return Some(borrowed_i);
            }
        }
        None
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        let (_, upper) = self.range.size_hint();
        (0, upper)
    }
}

impl<'a, 'fd> DoubleEndedIterator for Fds<'a, 'fd> {
    #[inline]
    fn next_back(&mut self) -> Option<BorrowedFd<'fd>> {
        while let Some(i) = self.range.next_back() {
            let borrowed_i = unsafe { BorrowedFd::borrow_raw(i as RawFd) };
            if self.set.contains(&borrowed_i) {
                return Some(borrowed_i);
            }
        }
        None
    }
}

impl<'a, 'fd> FusedIterator for Fds<'a, 'fd> {}

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
/// [select(2)](https://pubs.opengroup.org/onlinepubs/9699919799/functions/select.html)
///
/// [`FdSet::highest`]: struct.FdSet.html#method.highest
pub fn select<'a, 'fd, N, R, W, E, T>(
    nfds: N,
    readfds: R,
    writefds: W,
    errorfds: E,
    timeout: T,
) -> Result<c_int>
where
    'fd: 'a,
    N: Into<Option<c_int>>,
    R: Into<Option<&'a mut FdSet<'fd>>>,
    W: Into<Option<&'a mut FdSet<'fd>>>,
    E: Into<Option<&'a mut FdSet<'fd>>>,
    T: Into<Option<&'a mut TimeVal>>,
{
    let mut readfds = readfds.into();
    let mut writefds = writefds.into();
    let mut errorfds = errorfds.into();
    let timeout = timeout.into();

    let nfds = nfds.into().unwrap_or_else(|| {
        readfds
            .iter_mut()
            .chain(writefds.iter_mut())
            .chain(errorfds.iter_mut())
            .map(|set| {
                set.highest()
                    .map(|borrowed_fd| borrowed_fd.as_raw_fd())
                    .unwrap_or(-1)
            })
            .max()
            .unwrap_or(-1)
            + 1
    });

    let readfds = readfds
        .map(|set| set as *mut _ as *mut libc::fd_set)
        .unwrap_or(null_mut());
    let writefds = writefds
        .map(|set| set as *mut _ as *mut libc::fd_set)
        .unwrap_or(null_mut());
    let errorfds = errorfds
        .map(|set| set as *mut _ as *mut libc::fd_set)
        .unwrap_or(null_mut());
    let timeout = timeout
        .map(|tv| tv as *mut _ as *mut libc::timeval)
        .unwrap_or(null_mut());

    let res =
        unsafe { libc::select(nfds, readfds, writefds, errorfds, timeout) };

    Errno::result(res)
}

feature! {
#![feature = "signal"]

use crate::sys::signal::SigSet;

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
/// [pselect(2)](https://pubs.opengroup.org/onlinepubs/9699919799/functions/pselect.html)
///
/// [The new pselect() system call](https://lwn.net/Articles/176911/)
///
/// [`FdSet::highest`]: struct.FdSet.html#method.highest
pub fn pselect<'a, 'fd, N, R, W, E, T, S>(nfds: N,
    readfds: R,
    writefds: W,
    errorfds: E,
    timeout: T,
                                     sigmask: S) -> Result<c_int>
where
    'fd: 'a,
    N: Into<Option<c_int>>,
    R: Into<Option<&'a mut FdSet<'fd>>>,
    W: Into<Option<&'a mut FdSet<'fd>>>,
    E: Into<Option<&'a mut FdSet<'fd>>>,
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
            .map(|set| set.highest().map(|borrowed_fd|borrowed_fd.as_raw_fd()).unwrap_or(-1))
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
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::sys::time::{TimeVal, TimeValLike};
    use crate::unistd::{close, pipe, write};
    use std::os::unix::io::{FromRawFd, OwnedFd, RawFd};

    #[test]
    fn fdset_insert() {
        let mut fd_set = FdSet::new();

        for i in 0..FD_SETSIZE {
            let borrowed_i = unsafe { BorrowedFd::borrow_raw(i as RawFd) };
            assert!(!fd_set.contains(&borrowed_i));
        }

        let fd_seven = unsafe { BorrowedFd::borrow_raw(7) };
        fd_set.insert(&fd_seven);

        assert!(fd_set.contains(&fd_seven));
    }

    #[test]
    fn fdset_remove() {
        let mut fd_set = FdSet::new();

        for i in 0..FD_SETSIZE {
            let borrowed_i = unsafe { BorrowedFd::borrow_raw(i as RawFd) };
            assert!(!fd_set.contains(&borrowed_i));
        }

        let fd_seven = unsafe { BorrowedFd::borrow_raw(7) };
        fd_set.insert(&fd_seven);
        fd_set.remove(&fd_seven);

        for i in 0..FD_SETSIZE {
            let borrowed_i = unsafe { BorrowedFd::borrow_raw(i as RawFd) };
            assert!(!fd_set.contains(&borrowed_i));
        }
    }

    #[test]
    #[allow(non_snake_case)]
    fn fdset_clear() {
        let mut fd_set = FdSet::new();
        let fd_one = unsafe { BorrowedFd::borrow_raw(1) };
        let fd_FD_SETSIZE_devided_by_two =
            unsafe { BorrowedFd::borrow_raw((FD_SETSIZE / 2) as RawFd) };
        let fd_FD_SETSIZE_minus_one =
            unsafe { BorrowedFd::borrow_raw((FD_SETSIZE - 1) as RawFd) };
        fd_set.insert(&fd_one);
        fd_set.insert(&fd_FD_SETSIZE_devided_by_two);
        fd_set.insert(&fd_FD_SETSIZE_minus_one);

        fd_set.clear();

        for i in 0..FD_SETSIZE {
            let borrowed_i = unsafe { BorrowedFd::borrow_raw(i as RawFd) };
            assert!(!fd_set.contains(&borrowed_i));
        }
    }

    #[test]
    fn fdset_highest() {
        let mut set = FdSet::new();
        assert_eq!(
            set.highest().map(|borrowed_fd| borrowed_fd.as_raw_fd()),
            None
        );
        let fd_zero = unsafe { BorrowedFd::borrow_raw(0) };
        let fd_ninety = unsafe { BorrowedFd::borrow_raw(90) };
        set.insert(&fd_zero);
        assert_eq!(
            set.highest().map(|borrowed_fd| borrowed_fd.as_raw_fd()),
            Some(0)
        );
        set.insert(&fd_ninety);
        assert_eq!(
            set.highest().map(|borrowed_fd| borrowed_fd.as_raw_fd()),
            Some(90)
        );
        set.remove(&fd_zero);
        assert_eq!(
            set.highest().map(|borrowed_fd| borrowed_fd.as_raw_fd()),
            Some(90)
        );
        set.remove(&fd_ninety);
        assert_eq!(
            set.highest().map(|borrowed_fd| borrowed_fd.as_raw_fd()),
            None
        );

        let fd_four = unsafe { BorrowedFd::borrow_raw(4) };
        let fd_five = unsafe { BorrowedFd::borrow_raw(5) };
        let fd_seven = unsafe { BorrowedFd::borrow_raw(7) };
        set.insert(&fd_four);
        set.insert(&fd_five);
        set.insert(&fd_seven);
        assert_eq!(
            set.highest().map(|borrowed_fd| borrowed_fd.as_raw_fd()),
            Some(7)
        );
    }

    #[test]
    fn fdset_fds() {
        let mut set = FdSet::new();
        let fd_zero = unsafe { BorrowedFd::borrow_raw(0) };
        let fd_ninety = unsafe { BorrowedFd::borrow_raw(90) };
        assert_eq!(
            set.fds(None)
                .map(|borrowed_fd| borrowed_fd.as_raw_fd())
                .collect::<Vec<_>>(),
            vec![]
        );
        set.insert(&fd_zero);
        assert_eq!(
            set.fds(None)
                .map(|borrowed_fd| borrowed_fd.as_raw_fd())
                .collect::<Vec<_>>(),
            vec![0]
        );
        set.insert(&fd_ninety);
        assert_eq!(
            set.fds(None)
                .map(|borrowed_fd| borrowed_fd.as_raw_fd())
                .collect::<Vec<_>>(),
            vec![0, 90]
        );

        // highest limit
        assert_eq!(
            set.fds(Some(89))
                .map(|borrowed_fd| borrowed_fd.as_raw_fd())
                .collect::<Vec<_>>(),
            vec![0]
        );
        assert_eq!(
            set.fds(Some(90))
                .map(|borrowed_fd| borrowed_fd.as_raw_fd())
                .collect::<Vec<_>>(),
            vec![0, 90]
        );
    }

    #[test]
    fn test_select() {
        let (r1, w1) = pipe().unwrap();
        let r1 = unsafe { OwnedFd::from_raw_fd(r1) };
        let w1 = unsafe { OwnedFd::from_raw_fd(w1) };
        let (r2, _w2) = pipe().unwrap();
        let r2 = unsafe { OwnedFd::from_raw_fd(r2) };

        write(w1.as_raw_fd(), b"hi!").unwrap();
        let mut fd_set = FdSet::new();
        fd_set.insert(&r1);
        fd_set.insert(&r2);

        let mut timeout = TimeVal::seconds(10);
        assert_eq!(
            1,
            select(None, &mut fd_set, None, None, &mut timeout).unwrap()
        );
        assert!(fd_set.contains(&r1));
        assert!(!fd_set.contains(&r2));
        close(_w2).unwrap();
    }

    #[test]
    fn test_select_nfds() {
        let (r1, w1) = pipe().unwrap();
        let (r2, _w2) = pipe().unwrap();
        let r1 = unsafe { OwnedFd::from_raw_fd(r1) };
        let w1 = unsafe { OwnedFd::from_raw_fd(w1) };
        let r2 = unsafe { OwnedFd::from_raw_fd(r2) };

        write(w1.as_raw_fd(), b"hi!").unwrap();
        let mut fd_set = FdSet::new();
        fd_set.insert(&r1);
        fd_set.insert(&r2);

        let mut timeout = TimeVal::seconds(10);
        {
            assert_eq!(
                1,
                select(
                    Some(
                        fd_set
                            .highest()
                            .map(|borrowed_fd| borrowed_fd.as_raw_fd())
                            .unwrap()
                            + 1
                    ),
                    &mut fd_set,
                    None,
                    None,
                    &mut timeout
                )
                .unwrap()
            );
        }
        assert!(fd_set.contains(&r1));
        assert!(!fd_set.contains(&r2));
        close(_w2).unwrap();
    }

    #[test]
    fn test_select_nfds2() {
        let (r1, w1) = pipe().unwrap();
        write(w1, b"hi!").unwrap();
        let (r2, _w2) = pipe().unwrap();
        let r1 = unsafe { OwnedFd::from_raw_fd(r1) };
        let r2 = unsafe { OwnedFd::from_raw_fd(r2) };
        let mut fd_set = FdSet::new();
        fd_set.insert(&r1);
        fd_set.insert(&r2);

        let mut timeout = TimeVal::seconds(10);
        assert_eq!(
            1,
            select(
                std::cmp::max(r1.as_raw_fd(), r2.as_raw_fd()) + 1,
                &mut fd_set,
                None,
                None,
                &mut timeout
            )
            .unwrap()
        );
        assert!(fd_set.contains(&r1));
        assert!(!fd_set.contains(&r2));
        close(_w2).unwrap();
    }
}
