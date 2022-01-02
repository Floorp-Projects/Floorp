use errno::Errno;
use libc::{self, c_int};
use std::ptr;
use sys::signal::Signal;
use unistd::Pid;
use Result;

pub type RequestType = c_int;

cfg_if! {
    if #[cfg(any(target_os = "dragonfly", 
                 target_os = "freebsd", 
                 target_os = "macos",
                 target_os = "openbsd"))] {
        #[doc(hidden)]
        pub type AddressType = *mut ::libc::c_char;
    } else {
        #[doc(hidden)]
        pub type AddressType = *mut ::libc::c_void;
    }
}

libc_enum! {
    #[repr(i32)]
    /// Ptrace Request enum defining the action to be taken.
    pub enum Request {
        PT_TRACE_ME,
        PT_READ_I,
        PT_READ_D,
        #[cfg(target_os = "macos")]
        PT_READ_U,
        PT_WRITE_I,
        PT_WRITE_D,
        #[cfg(target_os = "macos")]
        PT_WRITE_U,
        PT_CONTINUE,
        PT_KILL,
        #[cfg(any(any(target_os = "dragonfly",
                  target_os = "freebsd",
                  target_os = "macos"),
                  all(target_os = "openbsd", target_arch = "x86_64"),
                  all(target_os = "netbsd", any(target_arch = "x86_64",
                                                target_arch = "powerpc"))))]
        PT_STEP,
        PT_ATTACH,
        PT_DETACH,
        #[cfg(target_os = "macos")]
        PT_SIGEXC,
        #[cfg(target_os = "macos")]
        PT_THUPDATE,
        #[cfg(target_os = "macos")]
        PT_ATTACHEXC
    }
}

unsafe fn ptrace_other(
    request: Request,
    pid: Pid,
    addr: AddressType,
    data: c_int,
) -> Result<c_int> {
    Errno::result(libc::ptrace(
        request as RequestType,
        libc::pid_t::from(pid),
        addr,
        data,
    )).map(|_| 0)
}

/// Sets the process as traceable, as with `ptrace(PT_TRACEME, ...)`
///
/// Indicates that this process is to be traced by its parent.
/// This is the only ptrace request to be issued by the tracee.
pub fn traceme() -> Result<()> {
    unsafe { ptrace_other(Request::PT_TRACE_ME, Pid::from_raw(0), ptr::null_mut(), 0).map(drop) }
}

/// Attach to a running process, as with `ptrace(PT_ATTACH, ...)`
///
/// Attaches to the process specified in pid, making it a tracee of the calling process.
pub fn attach(pid: Pid) -> Result<()> {
    unsafe { ptrace_other(Request::PT_ATTACH, pid, ptr::null_mut(), 0).map(drop) }
}

/// Detaches the current running process, as with `ptrace(PT_DETACH, ...)`
///
/// Detaches from the process specified in pid allowing it to run freely
pub fn detach(pid: Pid) -> Result<()> {
    unsafe { ptrace_other(Request::PT_DETACH, pid, ptr::null_mut(), 0).map(drop) }
}

/// Restart the stopped tracee process, as with `ptrace(PTRACE_CONT, ...)`
///
/// Continues the execution of the process with PID `pid`, optionally
/// delivering a signal specified by `sig`.
pub fn cont<T: Into<Option<Signal>>>(pid: Pid, sig: T) -> Result<()> {
    let data = match sig.into() {
        Some(s) => s as c_int,
        None => 0,
    };
    unsafe {
        // Ignore the useless return value
        ptrace_other(Request::PT_CONTINUE, pid, 1 as AddressType, data).map(drop)
    }
}

/// Issues a kill request as with `ptrace(PT_KILL, ...)`
///
/// This request is equivalent to `ptrace(PT_CONTINUE, ..., SIGKILL);` 
pub fn kill(pid: Pid) -> Result<()> {
    unsafe {
        ptrace_other(Request::PT_KILL, pid, 0 as AddressType, 0).map(drop)
    }
}

/// Move the stopped tracee process forward by a single step as with
/// `ptrace(PT_STEP, ...)`
///
/// Advances the execution of the process with PID `pid` by a single step optionally delivering a
/// signal specified by `sig`.
///
/// # Example
/// ```rust
/// extern crate nix;
/// use nix::sys::ptrace::step;
/// use nix::unistd::Pid;
/// use nix::sys::signal::Signal;
/// use nix::sys::wait::*;
/// fn main() {
///     // If a process changes state to the stopped state because of a SIGUSR1
///     // signal, this will step the process forward and forward the user
///     // signal to the stopped process
///     match waitpid(Pid::from_raw(-1), None) {
///         Ok(WaitStatus::Stopped(pid, Signal::SIGUSR1)) => {
///             let _ = step(pid, Signal::SIGUSR1);
///         }
///         _ => {},
///     }
/// }
/// ```
#[cfg(
    any(
        any(target_os = "dragonfly", target_os = "freebsd", target_os = "macos"),
        all(target_os = "openbsd", target_arch = "x86_64"),
        all(target_os = "netbsd",
            any(target_arch = "x86_64", target_arch = "powerpc")
        )
    )
)]
pub fn step<T: Into<Option<Signal>>>(pid: Pid, sig: T) -> Result<()> {
    let data = match sig.into() {
        Some(s) => s as c_int,
        None => 0,
    };
    unsafe { ptrace_other(Request::PT_STEP, pid, ptr::null_mut(), data).map(drop) }
}

/// Reads a word from a processes memory at the given address
pub fn read(pid: Pid, addr: AddressType) -> Result<c_int> {
    unsafe {
        // Traditionally there was a difference between reading data or
        // instruction memory but not in modern systems.
        ptrace_other(Request::PT_READ_D, pid, addr, 0)
    }
}

/// Writes a word into the processes memory at the given address
pub fn write(pid: Pid, addr: AddressType, data: c_int) -> Result<()> {
    unsafe { ptrace_other(Request::PT_WRITE_D, pid, addr, data).map(drop) }
}
