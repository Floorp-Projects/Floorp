// Portions of this file are Copyright 2014 The Rust Project Developers.
// See http://rust-lang.org/COPYRIGHT.

///! Operating system signals.

use libc;
use {Error, Result};
use errno::Errno;
use std::mem;
use std::fmt;
use std::str::FromStr;
#[cfg(any(target_os = "dragonfly", target_os = "freebsd"))]
use std::os::unix::io::RawFd;
use std::ptr;

#[cfg(not(target_os = "openbsd"))]
pub use self::sigevent::*;

libc_enum!{
    // Currently there is only one definition of c_int in libc, as well as only one
    // type for signal constants.
    // We would prefer to use the libc::c_int alias in the repr attribute. Unfortunately
    // this is not (yet) possible.
    #[repr(i32)]
    pub enum Signal {
        SIGHUP,
        SIGINT,
        SIGQUIT,
        SIGILL,
        SIGTRAP,
        SIGABRT,
        SIGBUS,
        SIGFPE,
        SIGKILL,
        SIGUSR1,
        SIGSEGV,
        SIGUSR2,
        SIGPIPE,
        SIGALRM,
        SIGTERM,
        #[cfg(all(any(target_os = "android", target_os = "emscripten", target_os = "linux"),
                  not(any(target_arch = "mips", target_arch = "mips64", target_arch = "sparc64"))))]
        SIGSTKFLT,
        SIGCHLD,
        SIGCONT,
        SIGSTOP,
        SIGTSTP,
        SIGTTIN,
        SIGTTOU,
        SIGURG,
        SIGXCPU,
        SIGXFSZ,
        SIGVTALRM,
        SIGPROF,
        SIGWINCH,
        SIGIO,
        #[cfg(any(target_os = "android", target_os = "emscripten", target_os = "linux"))]
        SIGPWR,
        SIGSYS,
        #[cfg(not(any(target_os = "android", target_os = "emscripten", target_os = "linux")))]
        SIGEMT,
        #[cfg(not(any(target_os = "android", target_os = "emscripten", target_os = "linux")))]
        SIGINFO,
    }
}

impl FromStr for Signal {
    type Err = Error;
    fn from_str(s: &str) -> Result<Signal> {
        Ok(match s {
            "SIGHUP" => Signal::SIGHUP,
            "SIGINT" => Signal::SIGINT,
            "SIGQUIT" => Signal::SIGQUIT,
            "SIGILL" => Signal::SIGILL,
            "SIGTRAP" => Signal::SIGTRAP,
            "SIGABRT" => Signal::SIGABRT,
            "SIGBUS" => Signal::SIGBUS,
            "SIGFPE" => Signal::SIGFPE,
            "SIGKILL" => Signal::SIGKILL,
            "SIGUSR1" => Signal::SIGUSR1,
            "SIGSEGV" => Signal::SIGSEGV,
            "SIGUSR2" => Signal::SIGUSR2,
            "SIGPIPE" => Signal::SIGPIPE,
            "SIGALRM" => Signal::SIGALRM,
            "SIGTERM" => Signal::SIGTERM,
            #[cfg(all(any(target_os = "android", target_os = "emscripten", target_os = "linux"),
                      not(any(target_arch = "mips", target_arch = "mips64", target_arch = "sparc64"))))]
            "SIGSTKFLT" => Signal::SIGSTKFLT,
            "SIGCHLD" => Signal::SIGCHLD,
            "SIGCONT" => Signal::SIGCONT,
            "SIGSTOP" => Signal::SIGSTOP,
            "SIGTSTP" => Signal::SIGTSTP,
            "SIGTTIN" => Signal::SIGTTIN,
            "SIGTTOU" => Signal::SIGTTOU,
            "SIGURG" => Signal::SIGURG,
            "SIGXCPU" => Signal::SIGXCPU,
            "SIGXFSZ" => Signal::SIGXFSZ,
            "SIGVTALRM" => Signal::SIGVTALRM,
            "SIGPROF" => Signal::SIGPROF,
            "SIGWINCH" => Signal::SIGWINCH,
            "SIGIO" => Signal::SIGIO,
            #[cfg(any(target_os = "android", target_os = "emscripten", target_os = "linux"))]
            "SIGPWR" => Signal::SIGPWR,
            "SIGSYS" => Signal::SIGSYS,
            #[cfg(not(any(target_os = "android", target_os = "emscripten", target_os = "linux")))]
            "SIGEMT" => Signal::SIGEMT,
            #[cfg(not(any(target_os = "android", target_os = "emscripten", target_os = "linux")))]
            "SIGINFO" => Signal::SIGINFO,
            _ => return Err(Error::invalid_argument()),
        })
    }
}

impl AsRef<str> for Signal {
    fn as_ref(&self) -> &str {
        match *self {
            Signal::SIGHUP => "SIGHUP",
            Signal::SIGINT => "SIGINT",
            Signal::SIGQUIT => "SIGQUIT",
            Signal::SIGILL => "SIGILL",
            Signal::SIGTRAP => "SIGTRAP",
            Signal::SIGABRT => "SIGABRT",
            Signal::SIGBUS => "SIGBUS",
            Signal::SIGFPE => "SIGFPE",
            Signal::SIGKILL => "SIGKILL",
            Signal::SIGUSR1 => "SIGUSR1",
            Signal::SIGSEGV => "SIGSEGV",
            Signal::SIGUSR2 => "SIGUSR2",
            Signal::SIGPIPE => "SIGPIPE",
            Signal::SIGALRM => "SIGALRM",
            Signal::SIGTERM => "SIGTERM",
            #[cfg(all(any(target_os = "android", target_os = "emscripten", target_os = "linux"),
                      not(any(target_arch = "mips", target_arch = "mips64", target_arch = "sparc64"))))]
            Signal::SIGSTKFLT => "SIGSTKFLT",
            Signal::SIGCHLD => "SIGCHLD",
            Signal::SIGCONT => "SIGCONT",
            Signal::SIGSTOP => "SIGSTOP",
            Signal::SIGTSTP => "SIGTSTP",
            Signal::SIGTTIN => "SIGTTIN",
            Signal::SIGTTOU => "SIGTTOU",
            Signal::SIGURG => "SIGURG",
            Signal::SIGXCPU => "SIGXCPU",
            Signal::SIGXFSZ => "SIGXFSZ",
            Signal::SIGVTALRM => "SIGVTALRM",
            Signal::SIGPROF => "SIGPROF",
            Signal::SIGWINCH => "SIGWINCH",
            Signal::SIGIO => "SIGIO",
            #[cfg(any(target_os = "android", target_os = "emscripten", target_os = "linux"))]
            Signal::SIGPWR => "SIGPWR",
            Signal::SIGSYS => "SIGSYS",
            #[cfg(not(any(target_os = "android", target_os = "emscripten", target_os = "linux")))]
            Signal::SIGEMT => "SIGEMT",
            #[cfg(not(any(target_os = "android", target_os = "emscripten", target_os = "linux")))]
            Signal::SIGINFO => "SIGINFO",
        }
    }
}

impl fmt::Display for Signal {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(self.as_ref())
    }
}

pub use self::Signal::*;

#[cfg(all(any(target_os = "linux", target_os = "android", target_os = "emscripten"), not(any(target_arch = "mips", target_arch = "mips64", target_arch = "sparc64"))))]
const SIGNALS: [Signal; 31] = [
    SIGHUP,
    SIGINT,
    SIGQUIT,
    SIGILL,
    SIGTRAP,
    SIGABRT,
    SIGBUS,
    SIGFPE,
    SIGKILL,
    SIGUSR1,
    SIGSEGV,
    SIGUSR2,
    SIGPIPE,
    SIGALRM,
    SIGTERM,
    SIGSTKFLT,
    SIGCHLD,
    SIGCONT,
    SIGSTOP,
    SIGTSTP,
    SIGTTIN,
    SIGTTOU,
    SIGURG,
    SIGXCPU,
    SIGXFSZ,
    SIGVTALRM,
    SIGPROF,
    SIGWINCH,
    SIGIO,
    SIGPWR,
    SIGSYS];
#[cfg(all(any(target_os = "linux", target_os = "android", target_os = "emscripten"), any(target_arch = "mips", target_arch = "mips64", target_arch = "sparc64")))]
const SIGNALS: [Signal; 30] = [
    SIGHUP,
    SIGINT,
    SIGQUIT,
    SIGILL,
    SIGTRAP,
    SIGABRT,
    SIGBUS,
    SIGFPE,
    SIGKILL,
    SIGUSR1,
    SIGSEGV,
    SIGUSR2,
    SIGPIPE,
    SIGALRM,
    SIGTERM,
    SIGCHLD,
    SIGCONT,
    SIGSTOP,
    SIGTSTP,
    SIGTTIN,
    SIGTTOU,
    SIGURG,
    SIGXCPU,
    SIGXFSZ,
    SIGVTALRM,
    SIGPROF,
    SIGWINCH,
    SIGIO,
    SIGPWR,
    SIGSYS];
#[cfg(not(any(target_os = "linux", target_os = "android", target_os = "emscripten")))]
const SIGNALS: [Signal; 31] = [
    SIGHUP,
    SIGINT,
    SIGQUIT,
    SIGILL,
    SIGTRAP,
    SIGABRT,
    SIGBUS,
    SIGFPE,
    SIGKILL,
    SIGUSR1,
    SIGSEGV,
    SIGUSR2,
    SIGPIPE,
    SIGALRM,
    SIGTERM,
    SIGCHLD,
    SIGCONT,
    SIGSTOP,
    SIGTSTP,
    SIGTTIN,
    SIGTTOU,
    SIGURG,
    SIGXCPU,
    SIGXFSZ,
    SIGVTALRM,
    SIGPROF,
    SIGWINCH,
    SIGIO,
    SIGSYS,
    SIGEMT,
    SIGINFO];

pub const NSIG: libc::c_int = 32;

#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub struct SignalIterator {
    next: usize,
}

impl Iterator for SignalIterator {
    type Item = Signal;

    fn next(&mut self) -> Option<Signal> {
        if self.next < SIGNALS.len() {
            let next_signal = SIGNALS[self.next];
            self.next += 1;
            Some(next_signal)
        } else {
            None
        }
    }
}

impl Signal {
    pub fn iterator() -> SignalIterator {
        SignalIterator{next: 0}
    }

    // We do not implement the From trait, because it is supposed to be infallible.
    // With Rust RFC 1542 comes the appropriate trait TryFrom. Once it is
    // implemented, we'll replace this function.
    #[inline]
    pub fn from_c_int(signum: libc::c_int) -> Result<Signal> {
        if 0 < signum && signum < NSIG {
            Ok(unsafe { mem::transmute(signum) })
        } else {
            Err(Error::invalid_argument())
        }
    }
}

pub const SIGIOT : Signal = SIGABRT;
pub const SIGPOLL : Signal = SIGIO;
pub const SIGUNUSED : Signal = SIGSYS;

libc_bitflags!{
    pub struct SaFlags: libc::c_int {
        SA_NOCLDSTOP;
        SA_NOCLDWAIT;
        SA_NODEFER;
        SA_ONSTACK;
        SA_RESETHAND;
        SA_RESTART;
        SA_SIGINFO;
    }
}

libc_enum! {
    #[repr(i32)]
    pub enum SigmaskHow {
        SIG_BLOCK,
        SIG_UNBLOCK,
        SIG_SETMASK,
    }
}

#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub struct SigSet {
    sigset: libc::sigset_t
}


impl SigSet {
    pub fn all() -> SigSet {
        let mut sigset: libc::sigset_t = unsafe { mem::uninitialized() };
        let _ = unsafe { libc::sigfillset(&mut sigset as *mut libc::sigset_t) };

        SigSet { sigset: sigset }
    }

    pub fn empty() -> SigSet {
        let mut sigset: libc::sigset_t = unsafe { mem::uninitialized() };
        let _ = unsafe { libc::sigemptyset(&mut sigset as *mut libc::sigset_t) };

        SigSet { sigset: sigset }
    }

    pub fn add(&mut self, signal: Signal) {
        unsafe { libc::sigaddset(&mut self.sigset as *mut libc::sigset_t, signal as libc::c_int) };
    }

    pub fn clear(&mut self) {
        unsafe { libc::sigemptyset(&mut self.sigset as *mut libc::sigset_t) };
    }

    pub fn remove(&mut self, signal: Signal) {
        unsafe { libc::sigdelset(&mut self.sigset as *mut libc::sigset_t, signal as libc::c_int) };
    }

    pub fn contains(&self, signal: Signal) -> bool {
        let res = unsafe { libc::sigismember(&self.sigset as *const libc::sigset_t, signal as libc::c_int) };

        match res {
            1 => true,
            0 => false,
            _ => unreachable!("unexpected value from sigismember"),
        }
    }

    pub fn extend(&mut self, other: &SigSet) {
        for signal in Signal::iterator() {
            if other.contains(signal) {
                self.add(signal);
            }
        }
    }

    /// Gets the currently blocked (masked) set of signals for the calling thread.
    pub fn thread_get_mask() -> Result<SigSet> {
        let mut oldmask: SigSet = unsafe { mem::uninitialized() };
        pthread_sigmask(SigmaskHow::SIG_SETMASK, None, Some(&mut oldmask))?;
        Ok(oldmask)
    }

    /// Sets the set of signals as the signal mask for the calling thread.
    pub fn thread_set_mask(&self) -> Result<()> {
        pthread_sigmask(SigmaskHow::SIG_SETMASK, Some(self), None)
    }

    /// Adds the set of signals to the signal mask for the calling thread.
    pub fn thread_block(&self) -> Result<()> {
        pthread_sigmask(SigmaskHow::SIG_BLOCK, Some(self), None)
    }

    /// Removes the set of signals from the signal mask for the calling thread.
    pub fn thread_unblock(&self) -> Result<()> {
        pthread_sigmask(SigmaskHow::SIG_UNBLOCK, Some(self), None)
    }

    /// Sets the set of signals as the signal mask, and returns the old mask.
    pub fn thread_swap_mask(&self, how: SigmaskHow) -> Result<SigSet> {
        let mut oldmask: SigSet = unsafe { mem::uninitialized() };
        pthread_sigmask(how, Some(self), Some(&mut oldmask))?;
        Ok(oldmask)
    }

    /// Suspends execution of the calling thread until one of the signals in the
    /// signal mask becomes pending, and returns the accepted signal.
    pub fn wait(&self) -> Result<Signal> {
        let mut signum: libc::c_int = unsafe { mem::uninitialized() };
        let res = unsafe { libc::sigwait(&self.sigset as *const libc::sigset_t, &mut signum) };

        Errno::result(res).map(|_| Signal::from_c_int(signum).unwrap())
    }
}

impl AsRef<libc::sigset_t> for SigSet {
    fn as_ref(&self) -> &libc::sigset_t {
        &self.sigset
    }
}

/// A signal handler.
#[allow(unknown_lints)]
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub enum SigHandler {
    /// Default signal handling.
    SigDfl,
    /// Request that the signal be ignored.
    SigIgn,
    /// Use the given signal-catching function, which takes in the signal.
    Handler(extern fn(libc::c_int)),
    /// Use the given signal-catching function, which takes in the signal, information about how
    /// the signal was generated, and a pointer to the threads `ucontext_t`.
    SigAction(extern fn(libc::c_int, *mut libc::siginfo_t, *mut libc::c_void))
}

/// Action to take on receipt of a signal. Corresponds to `sigaction`.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub struct SigAction {
    sigaction: libc::sigaction
}

impl SigAction {
    /// Creates a new action.
    ///
    /// The `SA_SIGINFO` bit in the `flags` argument is ignored (it will be set only if `handler`
    /// is the `SigAction` variant). `mask` specifies other signals to block during execution of
    /// the signal-catching function.
    pub fn new(handler: SigHandler, flags: SaFlags, mask: SigSet) -> SigAction {
        let mut s = unsafe { mem::uninitialized::<libc::sigaction>() };
        s.sa_sigaction = match handler {
            SigHandler::SigDfl => libc::SIG_DFL,
            SigHandler::SigIgn => libc::SIG_IGN,
            SigHandler::Handler(f) => f as *const extern fn(libc::c_int) as usize,
            SigHandler::SigAction(f) => f as *const extern fn(libc::c_int, *mut libc::siginfo_t, *mut libc::c_void) as usize,
        };
        s.sa_flags = match handler {
            SigHandler::SigAction(_) => (flags | SaFlags::SA_SIGINFO).bits(),
            _ => (flags - SaFlags::SA_SIGINFO).bits(),
        };
        s.sa_mask = mask.sigset;

        SigAction { sigaction: s }
    }

    /// Returns the flags set on the action.
    pub fn flags(&self) -> SaFlags {
        SaFlags::from_bits_truncate(self.sigaction.sa_flags)
    }

    /// Returns the set of signals that are blocked during execution of the action's
    /// signal-catching function.
    pub fn mask(&self) -> SigSet {
        SigSet { sigset: self.sigaction.sa_mask }
    }

    /// Returns the action's handler.
    pub fn handler(&self) -> SigHandler {
        match self.sigaction.sa_sigaction {
            libc::SIG_DFL => SigHandler::SigDfl,
            libc::SIG_IGN => SigHandler::SigIgn,
            f if self.flags().contains(SaFlags::SA_SIGINFO) =>
                SigHandler::SigAction( unsafe { mem::transmute(f) } ),
            f => SigHandler::Handler( unsafe { mem::transmute(f) } ),
        }
    }
}

/// Changes the action taken by a process on receipt of a specific signal.
///
/// `signal` can be any signal except `SIGKILL` or `SIGSTOP`. On success, it returns the previous
/// action for the given signal. If `sigaction` fails, no new signal handler is installed.
///
/// # Safety
///
/// Signal handlers may be called at any point during execution, which limits what is safe to do in
/// the body of the signal-catching function. Be certain to only make syscalls that are explicitly
/// marked safe for signal handlers and only share global data using atomics.
pub unsafe fn sigaction(signal: Signal, sigaction: &SigAction) -> Result<SigAction> {
    let mut oldact = mem::uninitialized::<libc::sigaction>();

    let res =
        libc::sigaction(signal as libc::c_int, &sigaction.sigaction as *const libc::sigaction, &mut oldact as *mut libc::sigaction);

    Errno::result(res).map(|_| SigAction { sigaction: oldact })
}

/// Signal management (see [signal(3p)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/signal.html))
///
/// Installs `handler` for the given `signal`, returning the previous signal
/// handler. `signal` should only be used following another call to `signal` or
/// if the current handler is the default. The return value of `signal` is
/// undefined after setting the handler with [`sigaction`][SigActionFn].
///
/// # Safety
///
/// If the pointer to the previous signal handler is invalid, undefined
/// behavior could be invoked when casting it back to a [`SigAction`][SigActionStruct].
///
/// # Examples
///
/// Ignore `SIGINT`:
///
/// ```no_run
/// # use nix::sys::signal::{self, Signal, SigHandler};
/// unsafe { signal::signal(Signal::SIGINT, SigHandler::SigIgn) }.unwrap();
/// ```
///
/// Use a signal handler to set a flag variable:
///
/// ```no_run
/// # #[macro_use] extern crate lazy_static;
/// # extern crate libc;
/// # extern crate nix;
/// # use std::sync::atomic::{AtomicBool, Ordering};
/// # use nix::sys::signal::{self, Signal, SigHandler};
/// lazy_static! {
///    static ref SIGNALED: AtomicBool = AtomicBool::new(false);
/// }
///
/// extern fn handle_sigint(signal: libc::c_int) {
///     let signal = Signal::from_c_int(signal).unwrap();
///     SIGNALED.store(signal == Signal::SIGINT, Ordering::Relaxed);
/// }
///
/// fn main() {
///     let handler = SigHandler::Handler(handle_sigint);
///     unsafe { signal::signal(Signal::SIGINT, handler) }.unwrap();
/// }
/// ```
///
/// # Errors
///
/// Returns [`Error::UnsupportedOperation`] if `handler` is
/// [`SigAction`][SigActionStruct]. Use [`sigaction`][SigActionFn] instead.
///
/// `signal` also returns any error from `libc::signal`, such as when an attempt
/// is made to catch a signal that cannot be caught or to ignore a signal that
/// cannot be ignored.
///
/// [`Error::UnsupportedOperation`]: ../../enum.Error.html#variant.UnsupportedOperation
/// [SigActionStruct]: struct.SigAction.html
/// [sigactionFn]: fn.sigaction.html
pub unsafe fn signal(signal: Signal, handler: SigHandler) -> Result<SigHandler> {
    let signal = signal as libc::c_int;
    let res = match handler {
        SigHandler::SigDfl => libc::signal(signal, libc::SIG_DFL),
        SigHandler::SigIgn => libc::signal(signal, libc::SIG_IGN),
        SigHandler::Handler(handler) => libc::signal(signal, handler as libc::sighandler_t),
        SigHandler::SigAction(_) => return Err(Error::UnsupportedOperation),
    };
    Errno::result(res).map(|oldhandler| {
        match oldhandler {
            libc::SIG_DFL => SigHandler::SigDfl,
            libc::SIG_IGN => SigHandler::SigIgn,
            f => SigHandler::Handler(mem::transmute(f)),
        }
    })
}

/// Manages the signal mask (set of blocked signals) for the calling thread.
///
/// If the `set` parameter is `Some(..)`, then the signal mask will be updated with the signal set.
/// The `how` flag decides the type of update. If `set` is `None`, `how` will be ignored,
/// and no modification will take place.
///
/// If the 'oldset' parameter is `Some(..)` then the current signal mask will be written into it.
///
/// If both `set` and `oldset` is `Some(..)`, the current signal mask will be written into oldset,
/// and then it will be updated with `set`.
///
/// If both `set` and `oldset` is None, this function is a no-op.
///
/// For more information, visit the [`pthread_sigmask`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/pthread_sigmask.html),
/// or [`sigprocmask`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/sigprocmask.html) man pages.
pub fn pthread_sigmask(how: SigmaskHow,
                       set: Option<&SigSet>,
                       oldset: Option<&mut SigSet>) -> Result<()> {
    if set.is_none() && oldset.is_none() {
        return Ok(())
    }

    let res = unsafe {
        // if set or oldset is None, pass in null pointers instead
        libc::pthread_sigmask(how as libc::c_int,
                             set.map_or_else(ptr::null::<libc::sigset_t>,
                                             |s| &s.sigset as *const libc::sigset_t),
                             oldset.map_or_else(ptr::null_mut::<libc::sigset_t>,
                                                |os| &mut os.sigset as *mut libc::sigset_t))
    };

    Errno::result(res).map(drop)
}

/// Examine and change blocked signals.
///
/// For more informations see the [`sigprocmask` man
/// pages](http://pubs.opengroup.org/onlinepubs/9699919799/functions/sigprocmask.html).
pub fn sigprocmask(how: SigmaskHow, set: Option<&SigSet>, oldset: Option<&mut SigSet>) -> Result<()> {
    if set.is_none() && oldset.is_none() {
        return Ok(())
    }

    let res = unsafe {
        // if set or oldset is None, pass in null pointers instead
        libc::sigprocmask(how as libc::c_int,
                          set.map_or_else(ptr::null::<libc::sigset_t>,
                                          |s| &s.sigset as *const libc::sigset_t),
                          oldset.map_or_else(ptr::null_mut::<libc::sigset_t>,
                                             |os| &mut os.sigset as *mut libc::sigset_t))
    };

    Errno::result(res).map(drop)
}

pub fn kill<T: Into<Option<Signal>>>(pid: ::unistd::Pid, signal: T) -> Result<()> {
    let res = unsafe { libc::kill(pid.into(),
                                  match signal.into() {
                                      Some(s) => s as libc::c_int,
                                      None => 0,
                                  }) };

    Errno::result(res).map(drop)
}

/// Send a signal to a process group [(see
/// killpg(3))](http://pubs.opengroup.org/onlinepubs/9699919799/functions/killpg.html).
///
/// If `pgrp` less then or equal 1, the behavior is platform-specific.
/// If `signal` is `None`, `killpg` will only preform error checking and won't
/// send any signal.
pub fn killpg<T: Into<Option<Signal>>>(pgrp: ::unistd::Pid, signal: T) -> Result<()> {
    let res = unsafe { libc::killpg(pgrp.into(),
                                  match signal.into() {
                                      Some(s) => s as libc::c_int,
                                      None => 0,
                                  }) };

    Errno::result(res).map(drop)
}

pub fn raise(signal: Signal) -> Result<()> {
    let res = unsafe { libc::raise(signal as libc::c_int) };

    Errno::result(res).map(drop)
}


#[cfg(target_os = "freebsd")]
pub type type_of_thread_id = libc::lwpid_t;
#[cfg(target_os = "linux")]
pub type type_of_thread_id = libc::pid_t;

/// Used to request asynchronous notification of certain events, for example,
/// with POSIX AIO, POSIX message queues, and POSIX timers.
// sigval is actually a union of a int and a void*.  But it's never really used
// as a pointer, because neither libc nor the kernel ever dereference it.  nix
// therefore presents it as an intptr_t, which is how kevent uses it.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub enum SigevNotify {
    /// No notification will be delivered
    SigevNone,
    /// The signal given by `signal` will be delivered to the process.  The
    /// value in `si_value` will be present in the `si_value` field of the
    /// `siginfo_t` structure of the queued signal.
    SigevSignal { signal: Signal, si_value: libc::intptr_t },
    // Note: SIGEV_THREAD is not implemented because libc::sigevent does not
    // expose a way to set the union members needed by SIGEV_THREAD.
    /// A new `kevent` is posted to the kqueue `kq`.  The `kevent`'s `udata`
    /// field will contain the value in `udata`.
    #[cfg(any(target_os = "dragonfly", target_os = "freebsd"))]
    SigevKevent { kq: RawFd, udata: libc::intptr_t },
    /// The signal `signal` is queued to the thread whose LWP ID is given in
    /// `thread_id`.  The value stored in `si_value` will be present in the
    /// `si_value` of the `siginfo_t` structure of the queued signal.
    #[cfg(any(target_os = "freebsd", target_os = "linux"))]
    SigevThreadId { signal: Signal, thread_id: type_of_thread_id,
                    si_value: libc::intptr_t },
}

#[cfg(not(target_os = "openbsd"))]
mod sigevent {
    use libc;
    use std::mem;
    use std::ptr;
    use super::SigevNotify;
    #[cfg(any(target_os = "freebsd", target_os = "linux"))]
    use super::type_of_thread_id;

    /// Used to request asynchronous notification of the completion of certain
    /// events, such as POSIX AIO and timers.
    #[repr(C)]
    #[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
    pub struct SigEvent {
        sigevent: libc::sigevent
    }

    impl SigEvent {
        /// **Note:** this constructor does not allow the user to set the
        /// `sigev_notify_kevent_flags` field.  That's considered ok because on FreeBSD
        /// at least those flags don't do anything useful.  That field is part of a
        /// union that shares space with the more genuinely useful fields.
        ///
        /// **Note:** This constructor also doesn't allow the caller to set the
        /// `sigev_notify_function` or `sigev_notify_attributes` fields, which are
        /// required for `SIGEV_THREAD`.  That's considered ok because on no operating
        /// system is `SIGEV_THREAD` the most efficient way to deliver AIO
        /// notification.  FreeBSD and DragonFly BSD programs should prefer `SIGEV_KEVENT`.
        /// Linux, Solaris, and portable programs should prefer `SIGEV_THREAD_ID` or
        /// `SIGEV_SIGNAL`.  That field is part of a union that shares space with the
        /// more genuinely useful `sigev_notify_thread_id`
        pub fn new(sigev_notify: SigevNotify) -> SigEvent {
            let mut sev = unsafe { mem::zeroed::<libc::sigevent>()};
            sev.sigev_notify = match sigev_notify {
                SigevNotify::SigevNone => libc::SIGEV_NONE,
                SigevNotify::SigevSignal{..} => libc::SIGEV_SIGNAL,
                #[cfg(any(target_os = "dragonfly", target_os = "freebsd"))]
                SigevNotify::SigevKevent{..} => libc::SIGEV_KEVENT,
                #[cfg(target_os = "freebsd")]
                SigevNotify::SigevThreadId{..} => libc::SIGEV_THREAD_ID,
                #[cfg(all(target_os = "linux", target_env = "gnu", not(target_arch = "mips")))]
                SigevNotify::SigevThreadId{..} => libc::SIGEV_THREAD_ID,
                #[cfg(any(all(target_os = "linux", target_env = "musl"), target_arch = "mips"))]
                SigevNotify::SigevThreadId{..} => 4  // No SIGEV_THREAD_ID defined
            };
            sev.sigev_signo = match sigev_notify {
                SigevNotify::SigevSignal{ signal, .. } => signal as libc::c_int,
                #[cfg(any(target_os = "dragonfly", target_os = "freebsd"))]
                SigevNotify::SigevKevent{ kq, ..} => kq,
                #[cfg(any(target_os = "linux", target_os = "freebsd"))]
                SigevNotify::SigevThreadId{ signal, .. } => signal as libc::c_int,
                _ => 0
            };
            sev.sigev_value.sival_ptr = match sigev_notify {
                SigevNotify::SigevNone => ptr::null_mut::<libc::c_void>(),
                SigevNotify::SigevSignal{ si_value, .. } => si_value as *mut libc::c_void,
                #[cfg(any(target_os = "dragonfly", target_os = "freebsd"))]
                SigevNotify::SigevKevent{ udata, .. } => udata as *mut libc::c_void,
                #[cfg(any(target_os = "freebsd", target_os = "linux"))]
                SigevNotify::SigevThreadId{ si_value, .. } => si_value as *mut libc::c_void,
            };
            SigEvent::set_tid(&mut sev, &sigev_notify);
            SigEvent{sigevent: sev}
        }

        #[cfg(any(target_os = "freebsd", target_os = "linux"))]
        fn set_tid(sev: &mut libc::sigevent, sigev_notify: &SigevNotify) {
            sev.sigev_notify_thread_id = match *sigev_notify {
                SigevNotify::SigevThreadId { thread_id, .. } => thread_id,
                _ => 0 as type_of_thread_id
            };
        }

        #[cfg(not(any(target_os = "freebsd", target_os = "linux")))]
        fn set_tid(_sev: &mut libc::sigevent, _sigev_notify: &SigevNotify) {
        }

        pub fn sigevent(&self) -> libc::sigevent {
            self.sigevent
        }
    }

    impl<'a> From<&'a libc::sigevent> for SigEvent {
        fn from(sigevent: &libc::sigevent) -> Self {
            SigEvent{ sigevent: *sigevent }
        }
    }
}

#[cfg(test)]
mod tests {
    use std::thread;
    use super::*;

    #[test]
    fn test_contains() {
        let mut mask = SigSet::empty();
        mask.add(SIGUSR1);

        assert!(mask.contains(SIGUSR1));
        assert!(!mask.contains(SIGUSR2));

        let all = SigSet::all();
        assert!(all.contains(SIGUSR1));
        assert!(all.contains(SIGUSR2));
    }

    #[test]
    fn test_clear() {
        let mut set = SigSet::all();
        set.clear();
        for signal in Signal::iterator() {
            assert!(!set.contains(signal));
        }
    }

    #[test]
    fn test_from_str_round_trips() {
        for signal in Signal::iterator() {
            assert_eq!(signal.as_ref().parse::<Signal>().unwrap(), signal);
            assert_eq!(signal.to_string().parse::<Signal>().unwrap(), signal);
        }
    }

    #[test]
    fn test_from_str_invalid_value() {
        let errval = Err(Error::Sys(Errno::EINVAL));
        assert_eq!("NOSIGNAL".parse::<Signal>(), errval);
        assert_eq!("kill".parse::<Signal>(), errval);
        assert_eq!("9".parse::<Signal>(), errval);
    }

    #[test]
    fn test_extend() {
        let mut one_signal = SigSet::empty();
        one_signal.add(SIGUSR1);

        let mut two_signals = SigSet::empty();
        two_signals.add(SIGUSR2);
        two_signals.extend(&one_signal);

        assert!(two_signals.contains(SIGUSR1));
        assert!(two_signals.contains(SIGUSR2));
    }

    #[test]
    fn test_thread_signal_set_mask() {
        thread::spawn(|| {
            let prev_mask = SigSet::thread_get_mask()
                .expect("Failed to get existing signal mask!");

            let mut test_mask = prev_mask;
            test_mask.add(SIGUSR1);

            assert!(test_mask.thread_set_mask().is_ok());
            let new_mask = SigSet::thread_get_mask()
                .expect("Failed to get new mask!");

            assert!(new_mask.contains(SIGUSR1));
            assert!(!new_mask.contains(SIGUSR2));

            prev_mask.thread_set_mask().expect("Failed to revert signal mask!");
        }).join().unwrap();
    }

    #[test]
    fn test_thread_signal_block() {
        thread::spawn(|| {
            let mut mask = SigSet::empty();
            mask.add(SIGUSR1);

            assert!(mask.thread_block().is_ok());

            assert!(SigSet::thread_get_mask().unwrap().contains(SIGUSR1));
        }).join().unwrap();
    }

    #[test]
    fn test_thread_signal_unblock() {
        thread::spawn(|| {
            let mut mask = SigSet::empty();
            mask.add(SIGUSR1);

            assert!(mask.thread_unblock().is_ok());

            assert!(!SigSet::thread_get_mask().unwrap().contains(SIGUSR1));
        }).join().unwrap();
    }

    #[test]
    fn test_thread_signal_swap() {
        thread::spawn(|| {
            let mut mask = SigSet::empty();
            mask.add(SIGUSR1);
            mask.thread_block().unwrap();

            assert!(SigSet::thread_get_mask().unwrap().contains(SIGUSR1));

            let mut mask2 = SigSet::empty();
            mask2.add(SIGUSR2);

            let oldmask = mask2.thread_swap_mask(SigmaskHow::SIG_SETMASK)
                .unwrap();

            assert!(oldmask.contains(SIGUSR1));
            assert!(!oldmask.contains(SIGUSR2));

            assert!(SigSet::thread_get_mask().unwrap().contains(SIGUSR2));
        }).join().unwrap();
    }

    #[test]
    fn test_sigaction() {
        use libc;
        thread::spawn(|| {
            extern fn test_sigaction_handler(_: libc::c_int) {}
            extern fn test_sigaction_action(_: libc::c_int,
                _: *mut libc::siginfo_t, _: *mut libc::c_void) {}

            let handler_sig = SigHandler::Handler(test_sigaction_handler);

            let flags = SaFlags::SA_ONSTACK | SaFlags::SA_RESTART |
                        SaFlags::SA_SIGINFO;

            let mut mask = SigSet::empty();
            mask.add(SIGUSR1);

            let action_sig = SigAction::new(handler_sig, flags, mask);

            assert_eq!(action_sig.flags(),
                       SaFlags::SA_ONSTACK | SaFlags::SA_RESTART);
            assert_eq!(action_sig.handler(), handler_sig);

            mask = action_sig.mask();
            assert!(mask.contains(SIGUSR1));
            assert!(!mask.contains(SIGUSR2));

            let handler_act = SigHandler::SigAction(test_sigaction_action);
            let action_act = SigAction::new(handler_act, flags, mask);
            assert_eq!(action_act.handler(), handler_act);

            let action_dfl = SigAction::new(SigHandler::SigDfl, flags, mask);
            assert_eq!(action_dfl.handler(), SigHandler::SigDfl);

            let action_ign = SigAction::new(SigHandler::SigIgn, flags, mask);
            assert_eq!(action_ign.handler(), SigHandler::SigIgn);
        }).join().unwrap();
    }

    #[test]
    fn test_sigwait() {
        thread::spawn(|| {
            let mut mask = SigSet::empty();
            mask.add(SIGUSR1);
            mask.add(SIGUSR2);
            mask.thread_block().unwrap();

            raise(SIGUSR1).unwrap();
            assert_eq!(mask.wait().unwrap(), SIGUSR1);
        }).join().unwrap();
    }
}
