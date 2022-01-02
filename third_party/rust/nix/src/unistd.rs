//! Safe wrappers around functions found in libc "unistd.h" header

use errno::{self, Errno};
use {Error, Result, NixPath};
use fcntl::{AtFlags, at_rawfd, fcntl, FdFlag, OFlag};
use fcntl::FcntlArg::F_SETFD;
use libc::{self, c_char, c_void, c_int, c_long, c_uint, size_t, pid_t, off_t,
           uid_t, gid_t, mode_t};
use std::{fmt, mem, ptr};
use std::ffi::{CString, CStr, OsString, OsStr};
use std::os::unix::ffi::{OsStringExt, OsStrExt};
use std::os::unix::io::RawFd;
use std::path::PathBuf;
use void::Void;
use sys::stat::Mode;

#[cfg(any(target_os = "android", target_os = "linux"))]
pub use self::pivot_root::*;

#[cfg(any(target_os = "android", target_os = "freebsd",
          target_os = "linux", target_os = "openbsd"))]
pub use self::setres::*;

/// User identifier
///
/// Newtype pattern around `uid_t` (which is just alias). It prevents bugs caused by accidentally
/// passing wrong value.
#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub struct Uid(uid_t);

impl Uid {
    /// Creates `Uid` from raw `uid_t`.
    pub fn from_raw(uid: uid_t) -> Self {
        Uid(uid)
    }

    /// Returns Uid of calling process. This is practically a more Rusty alias for `getuid`.
    pub fn current() -> Self {
        getuid()
    }

    /// Returns effective Uid of calling process. This is practically a more Rusty alias for `geteuid`.
    pub fn effective() -> Self {
        geteuid()
    }

    /// Returns true if the `Uid` represents privileged user - root. (If it equals zero.)
    pub fn is_root(&self) -> bool {
        *self == ROOT
    }

    /// Get the raw `uid_t` wrapped by `self`.
    pub fn as_raw(&self) -> uid_t {
        self.0
    }
}

impl From<Uid> for uid_t {
    fn from(uid: Uid) -> Self {
        uid.0
    }
}

impl fmt::Display for Uid {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Display::fmt(&self.0, f)
    }
}

/// Constant for UID = 0
pub const ROOT: Uid = Uid(0);

/// Group identifier
///
/// Newtype pattern around `gid_t` (which is just alias). It prevents bugs caused by accidentally
/// passing wrong value.
#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub struct Gid(gid_t);

impl Gid {
    /// Creates `Gid` from raw `gid_t`.
    pub fn from_raw(gid: gid_t) -> Self {
        Gid(gid)
    }

    /// Returns Gid of calling process. This is practically a more Rusty alias for `getgid`.
    pub fn current() -> Self {
        getgid()
    }

    /// Returns effective Gid of calling process. This is practically a more Rusty alias for `getgid`.
    pub fn effective() -> Self {
        getegid()
    }

    /// Get the raw `gid_t` wrapped by `self`.
    pub fn as_raw(&self) -> gid_t {
        self.0
    }
}

impl From<Gid> for gid_t {
    fn from(gid: Gid) -> Self {
        gid.0
    }
}

impl fmt::Display for Gid {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Display::fmt(&self.0, f)
    }
}

/// Process identifier
///
/// Newtype pattern around `pid_t` (which is just alias). It prevents bugs caused by accidentally
/// passing wrong value.
#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub struct Pid(pid_t);

impl Pid {
    /// Creates `Pid` from raw `pid_t`.
    pub fn from_raw(pid: pid_t) -> Self {
        Pid(pid)
    }

    /// Returns PID of calling process
    pub fn this() -> Self {
        getpid()
    }

    /// Returns PID of parent of calling process
    pub fn parent() -> Self {
        getppid()
    }

    /// Get the raw `pid_t` wrapped by `self`.
    pub fn as_raw(&self) -> pid_t {
        self.0
    }
}

impl From<Pid> for pid_t {
    fn from(pid: Pid) -> Self {
        pid.0
    }
}

impl fmt::Display for Pid {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Display::fmt(&self.0, f)
    }
}


/// Represents the successful result of calling `fork`
///
/// When `fork` is called, the process continues execution in the parent process
/// and in the new child.  This return type can be examined to determine whether
/// you are now executing in the parent process or in the child.
#[derive(Clone, Copy, Debug)]
pub enum ForkResult {
    Parent { child: Pid },
    Child,
}

impl ForkResult {

    /// Return `true` if this is the child process of the `fork()`
    #[inline]
    pub fn is_child(&self) -> bool {
        match *self {
            ForkResult::Child => true,
            _ => false
        }
    }

    /// Returns `true` if this is the parent process of the `fork()`
    #[inline]
    pub fn is_parent(&self) -> bool {
        !self.is_child()
    }
}

/// Create a new child process duplicating the parent process ([see
/// fork(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/fork.html)).
///
/// After calling the fork system call (successfully) two processes will
/// be created that are identical with the exception of their pid and the
/// return value of this function.  As an example:
///
/// ```no_run
/// use nix::unistd::{fork, ForkResult};
///
/// match fork() {
///    Ok(ForkResult::Parent { child, .. }) => {
///        println!("Continuing execution in parent process, new child has pid: {}", child);
///    }
///    Ok(ForkResult::Child) => println!("I'm a new child process"),
///    Err(_) => println!("Fork failed"),
/// }
/// ```
///
/// This will print something like the following (order indeterministic).  The
/// thing to note is that you end up with two processes continuing execution
/// immediately after the fork call but with different match arms.
///
/// ```text
/// Continuing execution in parent process, new child has pid: 1234
/// I'm a new child process
/// ```
///
/// # Safety
///
/// In a multithreaded program, only [async-signal-safe] functions like `pause`
/// and `_exit` may be called by the child (the parent isn't restricted). Note
/// that memory allocation may **not** be async-signal-safe and thus must be
/// prevented.
///
/// Those functions are only a small subset of your operating system's API, so
/// special care must be taken to only invoke code you can control and audit.
///
/// [async-signal-safe]: http://man7.org/linux/man-pages/man7/signal-safety.7.html
#[inline]
pub fn fork() -> Result<ForkResult> {
    use self::ForkResult::*;
    let res = unsafe { libc::fork() };

    Errno::result(res).map(|res| match res {
        0 => Child,
        res => Parent { child: Pid(res) },
    })
}

/// Get the pid of this process (see
/// [getpid(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/getpid.html)).
///
/// Since you are running code, there is always a pid to return, so there
/// is no error case that needs to be handled.
#[inline]
pub fn getpid() -> Pid {
    Pid(unsafe { libc::getpid() })
}

/// Get the pid of this processes' parent (see
/// [getpid(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/getppid.html)).
///
/// There is always a parent pid to return, so there is no error case that needs
/// to be handled.
#[inline]
pub fn getppid() -> Pid {
    Pid(unsafe { libc::getppid() }) // no error handling, according to man page: "These functions are always successful."
}

/// Set a process group ID (see
/// [setpgid(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/setpgid.html)).
///
/// Set the process group id (PGID) of a particular process.  If a pid of zero
/// is specified, then the pid of the calling process is used.  Process groups
/// may be used to group together a set of processes in order for the OS to
/// apply some operations across the group.
///
/// `setsid()` may be used to create a new process group.
#[inline]
pub fn setpgid(pid: Pid, pgid: Pid) -> Result<()> {
    let res = unsafe { libc::setpgid(pid.into(), pgid.into()) };
    Errno::result(res).map(drop)
}
#[inline]
pub fn getpgid(pid: Option<Pid>) -> Result<Pid> {
    let res = unsafe { libc::getpgid(pid.unwrap_or(Pid(0)).into()) };
    Errno::result(res).map(Pid)
}

/// Create new session and set process group id (see
/// [setsid(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/setsid.html)).
#[inline]
pub fn setsid() -> Result<Pid> {
    Errno::result(unsafe { libc::setsid() }).map(Pid)
}

/// Get the process group ID of a session leader
/// [getsid(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/getsid.html).
///
/// Obtain the process group ID of the process that is the session leader of the process specified
/// by pid. If pid is zero, it specifies the calling process.
#[inline]
pub fn getsid(pid: Option<Pid>) -> Result<Pid> {
    let res = unsafe { libc::getsid(pid.unwrap_or(Pid(0)).into()) };
    Errno::result(res).map(Pid)
}


/// Get the terminal foreground process group (see
/// [tcgetpgrp(3)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/tcgetpgrp.html)).
///
/// Get the group process id (GPID) of the foreground process group on the
/// terminal associated to file descriptor (FD).
#[inline]
pub fn tcgetpgrp(fd: c_int) -> Result<Pid> {
    let res = unsafe { libc::tcgetpgrp(fd) };
    Errno::result(res).map(Pid)
}
/// Set the terminal foreground process group (see
/// [tcgetpgrp(3)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/tcsetpgrp.html)).
///
/// Get the group process id (PGID) to the foreground process group on the
/// terminal associated to file descriptor (FD).
#[inline]
pub fn tcsetpgrp(fd: c_int, pgrp: Pid) -> Result<()> {
    let res = unsafe { libc::tcsetpgrp(fd, pgrp.into()) };
    Errno::result(res).map(drop)
}


/// Get the group id of the calling process (see
///[getpgrp(3)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/getpgrp.html)).
///
/// Get the process group id (PGID) of the calling process.
/// According to the man page it is always successful.
#[inline]
pub fn getpgrp() -> Pid {
    Pid(unsafe { libc::getpgrp() })
}

/// Get the caller's thread ID (see
/// [gettid(2)](http://man7.org/linux/man-pages/man2/gettid.2.html).
///
/// This function is only available on Linux based systems.  In a single
/// threaded process, the main thread will have the same ID as the process.  In
/// a multithreaded process, each thread will have a unique thread id but the
/// same process ID.
///
/// No error handling is required as a thread id should always exist for any
/// process, even if threads are not being used.
#[cfg(any(target_os = "linux", target_os = "android"))]
#[inline]
pub fn gettid() -> Pid {
    Pid(unsafe { libc::syscall(libc::SYS_gettid) as pid_t })
}

/// Create a copy of the specified file descriptor (see
/// [dup(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/dup.html)).
///
/// The new file descriptor will be have a new index but refer to the same
/// resource as the old file descriptor and the old and new file descriptors may
/// be used interchangeably.  The new and old file descriptor share the same
/// underlying resource, offset, and file status flags.  The actual index used
/// for the file descriptor will be the lowest fd index that is available.
///
/// The two file descriptors do not share file descriptor flags (e.g. `OFlag::FD_CLOEXEC`).
#[inline]
pub fn dup(oldfd: RawFd) -> Result<RawFd> {
    let res = unsafe { libc::dup(oldfd) };

    Errno::result(res)
}

/// Create a copy of the specified file descriptor using the specified fd (see
/// [dup(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/dup.html)).
///
/// This function behaves similar to `dup()` except that it will try to use the
/// specified fd instead of allocating a new one.  See the man pages for more
/// detail on the exact behavior of this function.
#[inline]
pub fn dup2(oldfd: RawFd, newfd: RawFd) -> Result<RawFd> {
    let res = unsafe { libc::dup2(oldfd, newfd) };

    Errno::result(res)
}

/// Create a new copy of the specified file descriptor using the specified fd
/// and flags (see [dup(2)](http://man7.org/linux/man-pages/man2/dup.2.html)).
///
/// This function behaves similar to `dup2()` but allows for flags to be
/// specified.
pub fn dup3(oldfd: RawFd, newfd: RawFd, flags: OFlag) -> Result<RawFd> {
    dup3_polyfill(oldfd, newfd, flags)
}

#[inline]
fn dup3_polyfill(oldfd: RawFd, newfd: RawFd, flags: OFlag) -> Result<RawFd> {
    if oldfd == newfd {
        return Err(Error::Sys(Errno::EINVAL));
    }

    let fd = dup2(oldfd, newfd)?;

    if flags.contains(OFlag::O_CLOEXEC) {
        if let Err(e) = fcntl(fd, F_SETFD(FdFlag::FD_CLOEXEC)) {
            let _ = close(fd);
            return Err(e);
        }
    }

    Ok(fd)
}

/// Change the current working directory of the calling process (see
/// [chdir(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/chdir.html)).
///
/// This function may fail in a number of different scenarios.  See the man
/// pages for additional details on possible failure cases.
#[inline]
pub fn chdir<P: ?Sized + NixPath>(path: &P) -> Result<()> {
    let res = path.with_nix_path(|cstr| {
        unsafe { libc::chdir(cstr.as_ptr()) }
    })?;

    Errno::result(res).map(drop)
}

/// Change the current working directory of the process to the one
/// given as an open file descriptor (see
/// [fchdir(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/fchdir.html)).
///
/// This function may fail in a number of different scenarios.  See the man
/// pages for additional details on possible failure cases.
#[inline]
pub fn fchdir(dirfd: RawFd) -> Result<()> {
    let res = unsafe { libc::fchdir(dirfd) };

    Errno::result(res).map(drop)
}

/// Creates new directory `path` with access rights `mode`.  (see [mkdir(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/mkdir.html))
///
/// # Errors
///
/// There are several situations where mkdir might fail:
///
/// - current user has insufficient rights in the parent directory
/// - the path already exists
/// - the path name is too long (longer than `PATH_MAX`, usually 4096 on linux, 1024 on OS X)
///
/// # Example
///
/// ```rust
/// extern crate tempfile;
/// extern crate nix;
///
/// use nix::unistd;
/// use nix::sys::stat;
/// use tempfile::tempdir;
///
/// fn main() {
///     let tmp_dir1 = tempdir().unwrap();
///     let tmp_dir2 = tmp_dir1.path().join("new_dir");
///
///     // create new directory and give read, write and execute rights to the owner
///     match unistd::mkdir(&tmp_dir2, stat::Mode::S_IRWXU) {
///        Ok(_) => println!("created {:?}", tmp_dir2),
///        Err(err) => println!("Error creating directory: {}", err),
///     }
/// }
/// ```
#[inline]
pub fn mkdir<P: ?Sized + NixPath>(path: &P, mode: Mode) -> Result<()> {
    let res = path.with_nix_path(|cstr| {
        unsafe { libc::mkdir(cstr.as_ptr(), mode.bits() as mode_t) }
    })?;

    Errno::result(res).map(drop)
}

/// Creates new fifo special file (named pipe) with path `path` and access rights `mode`.
///
/// # Errors
///
/// There are several situations where mkfifo might fail:
///
/// - current user has insufficient rights in the parent directory
/// - the path already exists
/// - the path name is too long (longer than `PATH_MAX`, usually 4096 on linux, 1024 on OS X)
///
/// For a full list consult
/// [posix specification](http://pubs.opengroup.org/onlinepubs/9699919799/functions/mkfifo.html)
///
/// # Example
///
/// ```rust
/// extern crate tempfile;
/// extern crate nix;
///
/// use nix::unistd;
/// use nix::sys::stat;
/// use tempfile::tempdir;
///
/// fn main() {
///     let tmp_dir = tempdir().unwrap();
///     let fifo_path = tmp_dir.path().join("foo.pipe");
///
///     // create new fifo and give read, write and execute rights to the owner
///     match unistd::mkfifo(&fifo_path, stat::Mode::S_IRWXU) {
///        Ok(_) => println!("created {:?}", fifo_path),
///        Err(err) => println!("Error creating fifo: {}", err),
///     }
/// }
/// ```
#[inline]
pub fn mkfifo<P: ?Sized + NixPath>(path: &P, mode: Mode) -> Result<()> {
    let res = path.with_nix_path(|cstr| {
        unsafe { libc::mkfifo(cstr.as_ptr(), mode.bits() as mode_t) }
    })?;

    Errno::result(res).map(drop)
}

/// Creates a symbolic link at `path2` which points to `path1`.
///
/// If `dirfd` has a value, then `path2` is relative to directory associated
/// with the file descriptor.
///
/// If `dirfd` is `None`, then `path2` is relative to the current working
/// directory. This is identical to `libc::symlink(path1, path2)`.
///
/// See also [symlinkat(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/symlinkat.html).
pub fn symlinkat<P1: ?Sized + NixPath, P2: ?Sized + NixPath>(
    path1: &P1,
    dirfd: Option<RawFd>,
    path2: &P2) -> Result<()> {
    let res =
        path1.with_nix_path(|path1| {
            path2.with_nix_path(|path2| {
                unsafe {
                    libc::symlinkat(
                        path1.as_ptr(),
                        dirfd.unwrap_or(libc::AT_FDCWD),
                        path2.as_ptr()
                    )
                }
            })
        })??;
    Errno::result(res).map(drop)
}

/// Returns the current directory as a `PathBuf`
///
/// Err is returned if the current user doesn't have the permission to read or search a component
/// of the current path.
///
/// # Example
///
/// ```rust
/// extern crate nix;
///
/// use nix::unistd;
///
/// fn main() {
///     // assume that we are allowed to get current directory
///     let dir = unistd::getcwd().unwrap();
///     println!("The current directory is {:?}", dir);
/// }
/// ```
#[inline]
pub fn getcwd() -> Result<PathBuf> {
    let mut buf = Vec::with_capacity(512);
    loop {
        unsafe {
            let ptr = buf.as_mut_ptr() as *mut c_char;

            // The buffer must be large enough to store the absolute pathname plus
            // a terminating null byte, or else null is returned.
            // To safely handle this we start with a reasonable size (512 bytes)
            // and double the buffer size upon every error
            if !libc::getcwd(ptr, buf.capacity()).is_null() {
                let len = CStr::from_ptr(buf.as_ptr() as *const c_char).to_bytes().len();
                buf.set_len(len);
                buf.shrink_to_fit();
                return Ok(PathBuf::from(OsString::from_vec(buf)));
            } else {
                let error = Errno::last();
                // ERANGE means buffer was too small to store directory name
                if error != Errno::ERANGE {
                    return Err(Error::Sys(error));
                }
            }

            // Trigger the internal buffer resizing logic of `Vec` by requiring
            // more space than the current capacity.
            let cap = buf.capacity();
            buf.set_len(cap);
            buf.reserve(1);
        }
    }
}

/// Computes the raw UID and GID values to pass to a `*chown` call.
fn chown_raw_ids(owner: Option<Uid>, group: Option<Gid>) -> (libc::uid_t, libc::gid_t) {
    // According to the POSIX specification, -1 is used to indicate that owner and group
    // are not to be changed.  Since uid_t and gid_t are unsigned types, we have to wrap
    // around to get -1.
    let uid = owner.map(Into::into).unwrap_or((0 as uid_t).wrapping_sub(1));
    let gid = group.map(Into::into).unwrap_or((0 as gid_t).wrapping_sub(1));
    (uid, gid)
}

/// Change the ownership of the file at `path` to be owned by the specified
/// `owner` (user) and `group` (see
/// [chown(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/chown.html)).
///
/// The owner/group for the provided path name will not be modified if `None` is
/// provided for that argument.  Ownership change will be attempted for the path
/// only if `Some` owner/group is provided.
#[inline]
pub fn chown<P: ?Sized + NixPath>(path: &P, owner: Option<Uid>, group: Option<Gid>) -> Result<()> {
    let res = path.with_nix_path(|cstr| {
        let (uid, gid) = chown_raw_ids(owner, group);
        unsafe { libc::chown(cstr.as_ptr(), uid, gid) }
    })?;

    Errno::result(res).map(drop)
}

/// Flags for `fchownat` function.
#[derive(Clone, Copy, Debug)]
pub enum FchownatFlags {
    FollowSymlink,
    NoFollowSymlink,
}

/// Change the ownership of the file at `path` to be owned by the specified
/// `owner` (user) and `group`.
///
/// The owner/group for the provided path name will not be modified if `None` is
/// provided for that argument.  Ownership change will be attempted for the path
/// only if `Some` owner/group is provided.
///
/// The file to be changed is determined relative to the directory associated
/// with the file descriptor `dirfd` or the current working directory
/// if `dirfd` is `None`.
///
/// If `flag` is `FchownatFlags::NoFollowSymlink` and `path` names a symbolic link,
/// then the mode of the symbolic link is changed.
///
/// `fchownat(None, path, mode, FchownatFlags::NoFollowSymlink)` is identical to
/// a call `libc::lchown(path, mode)`.  That's why `lchmod` is unimplemented in
/// the `nix` crate.
///
/// # References
///
/// [fchownat(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/fchownat.html).
pub fn fchownat<P: ?Sized + NixPath>(
    dirfd: Option<RawFd>,
    path: &P,
    owner: Option<Uid>,
    group: Option<Gid>,
    flag: FchownatFlags,
) -> Result<()> {
    let atflag =
        match flag {
            FchownatFlags::FollowSymlink => AtFlags::empty(),
            FchownatFlags::NoFollowSymlink => AtFlags::AT_SYMLINK_NOFOLLOW,
        };
    let res = path.with_nix_path(|cstr| unsafe {
        let (uid, gid) = chown_raw_ids(owner, group);
        libc::fchownat(at_rawfd(dirfd), cstr.as_ptr(), uid, gid,
                       atflag.bits() as libc::c_int)
    })?;

    Errno::result(res).map(drop)
}

fn to_exec_array(args: &[CString]) -> Vec<*const c_char> {
    let mut args_p: Vec<*const c_char> = args.iter().map(|s| s.as_ptr()).collect();
    args_p.push(ptr::null());
    args_p
}

/// Replace the current process image with a new one (see
/// [exec(3)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/exec.html)).
///
/// See the `::nix::unistd::execve` system call for additional details.  `execv`
/// performs the same action but does not allow for customization of the
/// environment for the new process.
#[inline]
pub fn execv(path: &CString, argv: &[CString]) -> Result<Void> {
    let args_p = to_exec_array(argv);

    unsafe {
        libc::execv(path.as_ptr(), args_p.as_ptr())
    };

    Err(Error::Sys(Errno::last()))
}


/// Replace the current process image with a new one (see
/// [execve(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/exec.html)).
///
/// The execve system call allows for another process to be "called" which will
/// replace the current process image.  That is, this process becomes the new
/// command that is run. On success, this function will not return. Instead,
/// the new program will run until it exits.
///
/// `::nix::unistd::execv` and `::nix::unistd::execve` take as arguments a slice
/// of `::std::ffi::CString`s for `args` and `env` (for `execve`). Each element
/// in the `args` list is an argument to the new process. Each element in the
/// `env` list should be a string in the form "key=value".
#[inline]
pub fn execve(path: &CString, args: &[CString], env: &[CString]) -> Result<Void> {
    let args_p = to_exec_array(args);
    let env_p = to_exec_array(env);

    unsafe {
        libc::execve(path.as_ptr(), args_p.as_ptr(), env_p.as_ptr())
    };

    Err(Error::Sys(Errno::last()))
}

/// Replace the current process image with a new one and replicate shell `PATH`
/// searching behavior (see
/// [exec(3)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/exec.html)).
///
/// See `::nix::unistd::execve` for additional details.  `execvp` behaves the
/// same as execv except that it will examine the `PATH` environment variables
/// for file names not specified with a leading slash.  For example, `execv`
/// would not work if "bash" was specified for the path argument, but `execvp`
/// would assuming that a bash executable was on the system `PATH`.
#[inline]
pub fn execvp(filename: &CString, args: &[CString]) -> Result<Void> {
    let args_p = to_exec_array(args);

    unsafe {
        libc::execvp(filename.as_ptr(), args_p.as_ptr())
    };

    Err(Error::Sys(Errno::last()))
}

/// Replace the current process image with a new one and replicate shell `PATH`
/// searching behavior (see
/// [`execvpe(3)`](http://man7.org/linux/man-pages/man3/exec.3.html)).
///
/// This functions like a combination of `execvp(2)` and `execve(2)` to pass an
/// environment and have a search path. See these two for additional
/// information.
#[cfg(any(target_os = "haiku",
          target_os = "linux",
          target_os = "openbsd"))]
pub fn execvpe(filename: &CString, args: &[CString], env: &[CString]) -> Result<Void> {
    let args_p = to_exec_array(args);
    let env_p = to_exec_array(env);

    unsafe {
        libc::execvpe(filename.as_ptr(), args_p.as_ptr(), env_p.as_ptr())
    };

    Err(Error::Sys(Errno::last()))
}

/// Replace the current process image with a new one (see
/// [fexecve(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/fexecve.html)).
///
/// The `fexecve` function allows for another process to be "called" which will
/// replace the current process image.  That is, this process becomes the new
/// command that is run. On success, this function will not return. Instead,
/// the new program will run until it exits.
///
/// This function is similar to `execve`, except that the program to be executed
/// is referenced as a file descriptor instead of a path.
// Note for NetBSD and OpenBSD: although rust-lang/libc includes it (under
// unix/bsd/netbsdlike/) fexecve is not currently implemented on NetBSD nor on
// OpenBSD.
#[cfg(any(target_os = "android",
          target_os = "linux",
          target_os = "freebsd"))]
#[inline]
pub fn fexecve(fd: RawFd, args: &[CString], env: &[CString]) -> Result<Void> {
    let args_p = to_exec_array(args);
    let env_p = to_exec_array(env);

    unsafe {
        libc::fexecve(fd, args_p.as_ptr(), env_p.as_ptr())
    };

    Err(Error::Sys(Errno::last()))
}

/// Execute program relative to a directory file descriptor (see
/// [execveat(2)](http://man7.org/linux/man-pages/man2/execveat.2.html)).
///
/// The `execveat` function allows for another process to be "called" which will
/// replace the current process image.  That is, this process becomes the new
/// command that is run. On success, this function will not return. Instead,
/// the new program will run until it exits.
///
/// This function is similar to `execve`, except that the program to be executed
/// is referenced as a file descriptor to the base directory plus a path.
#[cfg(any(target_os = "android", target_os = "linux"))]
#[inline]
pub fn execveat(dirfd: RawFd, pathname: &CString, args: &[CString],
                env: &[CString], flags: super::fcntl::AtFlags) -> Result<Void> {
    let args_p = to_exec_array(args);
    let env_p = to_exec_array(env);

    unsafe {
        libc::syscall(libc::SYS_execveat, dirfd, pathname.as_ptr(),
                      args_p.as_ptr(), env_p.as_ptr(), flags);
    };

    Err(Error::Sys(Errno::last()))
}

/// Daemonize this process by detaching from the controlling terminal (see
/// [daemon(3)](http://man7.org/linux/man-pages/man3/daemon.3.html)).
///
/// When a process is launched it is typically associated with a parent and it,
/// in turn, by its controlling terminal/process.  In order for a process to run
/// in the "background" it must daemonize itself by detaching itself.  Under
/// posix, this is done by doing the following:
///
/// 1. Parent process (this one) forks
/// 2. Parent process exits
/// 3. Child process continues to run.
///
/// `nochdir`:
///
/// * `nochdir = true`: The current working directory after daemonizing will
///    be the current working directory.
/// *  `nochdir = false`: The current working directory after daemonizing will
///    be the root direcory, `/`.
///
/// `noclose`:
///
/// * `noclose = true`: The process' current stdin, stdout, and stderr file
///   descriptors will remain identical after daemonizing.
/// * `noclose = false`: The process' stdin, stdout, and stderr will point to
///   `/dev/null` after daemonizing.
#[cfg_attr(any(target_os = "macos", target_os = "ios"), deprecated(
    since="0.14.0",
    note="Deprecated in MacOSX 10.5"
))]
#[cfg_attr(any(target_os = "macos", target_os = "ios"), allow(deprecated))]
pub fn daemon(nochdir: bool, noclose: bool) -> Result<()> {
    let res = unsafe { libc::daemon(nochdir as c_int, noclose as c_int) };
    Errno::result(res).map(drop)
}

/// Set the system host name (see
/// [sethostname(2)](http://man7.org/linux/man-pages/man2/gethostname.2.html)).
///
/// Given a name, attempt to update the system host name to the given string.
/// On some systems, the host name is limited to as few as 64 bytes.  An error
/// will be return if the name is not valid or the current process does not have
/// permissions to update the host name.
pub fn sethostname<S: AsRef<OsStr>>(name: S) -> Result<()> {
    // Handle some differences in type of the len arg across platforms.
    cfg_if! {
        if #[cfg(any(target_os = "dragonfly",
                     target_os = "freebsd",
                     target_os = "ios",
                     target_os = "macos", ))] {
            type sethostname_len_t = c_int;
        } else {
            type sethostname_len_t = size_t;
        }
    }
    let ptr = name.as_ref().as_bytes().as_ptr() as *const c_char;
    let len = name.as_ref().len() as sethostname_len_t;

    let res = unsafe { libc::sethostname(ptr, len) };
    Errno::result(res).map(drop)
}

/// Get the host name and store it in the provided buffer, returning a pointer
/// the `CStr` in that buffer on success (see
/// [gethostname(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/gethostname.html)).
///
/// This function call attempts to get the host name for the running system and
/// store it in a provided buffer.  The buffer will be populated with bytes up
/// to the length of the provided slice including a NUL terminating byte.  If
/// the hostname is longer than the length provided, no error will be provided.
/// The posix specification does not specify whether implementations will
/// null-terminate in this case, but the nix implementation will ensure that the
/// buffer is null terminated in this case.
///
/// ```no_run
/// use nix::unistd;
///
/// let mut buf = [0u8; 64];
/// let hostname_cstr = unistd::gethostname(&mut buf).expect("Failed getting hostname");
/// let hostname = hostname_cstr.to_str().expect("Hostname wasn't valid UTF-8");
/// println!("Hostname: {}", hostname);
/// ```
pub fn gethostname(buffer: &mut [u8]) -> Result<&CStr> {
    let ptr = buffer.as_mut_ptr() as *mut c_char;
    let len = buffer.len() as size_t;

    let res = unsafe { libc::gethostname(ptr, len) };
    Errno::result(res).map(|_| {
        buffer[len - 1] = 0; // ensure always null-terminated
        unsafe { CStr::from_ptr(buffer.as_ptr() as *const c_char) }
    })
}

/// Close a raw file descriptor
///
/// Be aware that many Rust types implicitly close-on-drop, including
/// `std::fs::File`.  Explicitly closing them with this method too can result in
/// a double-close condition, which can cause confusing `EBADF` errors in
/// seemingly unrelated code.  Caveat programmer.  See also
/// [close(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/close.html).
///
/// # Examples
///
/// ```no_run
/// extern crate tempfile;
/// extern crate nix;
///
/// use std::os::unix::io::AsRawFd;
/// use nix::unistd::close;
///
/// fn main() {
///     let f = tempfile::tempfile().unwrap();
///     close(f.as_raw_fd()).unwrap();   // Bad!  f will also close on drop!
/// }
/// ```
///
/// ```rust
/// extern crate tempfile;
/// extern crate nix;
///
/// use std::os::unix::io::IntoRawFd;
/// use nix::unistd::close;
///
/// fn main() {
///     let f = tempfile::tempfile().unwrap();
///     close(f.into_raw_fd()).unwrap(); // Good.  into_raw_fd consumes f
/// }
/// ```
pub fn close(fd: RawFd) -> Result<()> {
    let res = unsafe { libc::close(fd) };
    Errno::result(res).map(drop)
}

/// Read from a raw file descriptor.
///
/// See also [read(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/read.html)
pub fn read(fd: RawFd, buf: &mut [u8]) -> Result<usize> {
    let res = unsafe { libc::read(fd, buf.as_mut_ptr() as *mut c_void, buf.len() as size_t) };

    Errno::result(res).map(|r| r as usize)
}

/// Write to a raw file descriptor.
///
/// See also [write(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/write.html)
pub fn write(fd: RawFd, buf: &[u8]) -> Result<usize> {
    let res = unsafe { libc::write(fd, buf.as_ptr() as *const c_void, buf.len() as size_t) };

    Errno::result(res).map(|r| r as usize)
}

/// Directive that tells [`lseek`] and [`lseek64`] what the offset is relative to.
///
/// [`lseek`]: ./fn.lseek.html
/// [`lseek64`]: ./fn.lseek64.html
#[repr(i32)]
#[derive(Clone, Copy, Debug)]
pub enum Whence {
    /// Specify an offset relative to the start of the file.
    SeekSet = libc::SEEK_SET,
    /// Specify an offset relative to the current file location.
    SeekCur = libc::SEEK_CUR,
    /// Specify an offset relative to the end of the file.
    SeekEnd = libc::SEEK_END,
    /// Specify an offset relative to the next location in the file greater than or
    /// equal to offset that contains some data. If offset points to
    /// some data, then the file offset is set to offset.
    #[cfg(any(target_os = "dragonfly", target_os = "freebsd",
          all(target_os = "linux", not(any(target_env = "musl",
                                           target_arch = "mips",
                                           target_arch = "mips64")))))]
    SeekData = libc::SEEK_DATA,
    /// Specify an offset relative to the next hole in the file greater than
    /// or equal to offset. If offset points into the middle of a hole, then
    /// the file offset should be set to offset. If there is no hole past offset,
    /// then the file offset should be adjusted to the end of the file (i.e., there
    /// is an implicit hole at the end of any file).
    #[cfg(any(target_os = "dragonfly", target_os = "freebsd",
          all(target_os = "linux", not(any(target_env = "musl",
                                           target_arch = "mips",
                                           target_arch = "mips64")))))]
    SeekHole = libc::SEEK_HOLE
}

/// Move the read/write file offset.
///
/// See also [lseek(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/lseek.html)
pub fn lseek(fd: RawFd, offset: off_t, whence: Whence) -> Result<off_t> {
    let res = unsafe { libc::lseek(fd, offset, whence as i32) };

    Errno::result(res).map(|r| r as off_t)
}

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn lseek64(fd: RawFd, offset: libc::off64_t, whence: Whence) -> Result<libc::off64_t> {
    let res = unsafe { libc::lseek64(fd, offset, whence as i32) };

    Errno::result(res).map(|r| r as libc::off64_t)
}

/// Create an interprocess channel.
///
/// See also [pipe(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/pipe.html)
pub fn pipe() -> Result<(RawFd, RawFd)> {
    unsafe {
        let mut fds: [c_int; 2] = mem::uninitialized();

        let res = libc::pipe(fds.as_mut_ptr());

        Errno::result(res)?;

        Ok((fds[0], fds[1]))
    }
}

/// Like `pipe`, but allows setting certain file descriptor flags.
///
/// The following flags are supported, and will be set atomically as the pipe is
/// created:
///
/// `O_CLOEXEC`:    Set the close-on-exec flag for the new file descriptors.
/// `O_NONBLOCK`:   Set the non-blocking flag for the ends of the pipe.
///
/// See also [pipe(2)](http://man7.org/linux/man-pages/man2/pipe.2.html)
#[cfg(any(target_os = "android",
          target_os = "dragonfly",
          target_os = "emscripten",
          target_os = "freebsd",
          target_os = "linux",
          target_os = "netbsd",
          target_os = "openbsd"))]
pub fn pipe2(flags: OFlag) -> Result<(RawFd, RawFd)> {
    let mut fds: [c_int; 2] = unsafe { mem::uninitialized() };

    let res = unsafe { libc::pipe2(fds.as_mut_ptr(), flags.bits()) };

    Errno::result(res)?;

    Ok((fds[0], fds[1]))
}

/// Like `pipe`, but allows setting certain file descriptor flags.
///
/// The following flags are supported, and will be set after the pipe is
/// created:
///
/// `O_CLOEXEC`:    Set the close-on-exec flag for the new file descriptors.
/// `O_NONBLOCK`:   Set the non-blocking flag for the ends of the pipe.
#[cfg(any(target_os = "ios", target_os = "macos"))]
#[deprecated(
    since="0.10.0",
    note="pipe2(2) is not actually atomic on these platforms.  Use pipe(2) and fcntl(2) instead"
)]
pub fn pipe2(flags: OFlag) -> Result<(RawFd, RawFd)> {
    let mut fds: [c_int; 2] = unsafe { mem::uninitialized() };

    let res = unsafe { libc::pipe(fds.as_mut_ptr()) };

    Errno::result(res)?;

    pipe2_setflags(fds[0], fds[1], flags)?;

    Ok((fds[0], fds[1]))
}

#[cfg(any(target_os = "ios", target_os = "macos"))]
fn pipe2_setflags(fd1: RawFd, fd2: RawFd, flags: OFlag) -> Result<()> {
    use fcntl::FcntlArg::F_SETFL;

    let mut res = Ok(0);

    if flags.contains(OFlag::O_CLOEXEC) {
        res = res
            .and_then(|_| fcntl(fd1, F_SETFD(FdFlag::FD_CLOEXEC)))
            .and_then(|_| fcntl(fd2, F_SETFD(FdFlag::FD_CLOEXEC)));
    }

    if flags.contains(OFlag::O_NONBLOCK) {
        res = res
            .and_then(|_| fcntl(fd1, F_SETFL(OFlag::O_NONBLOCK)))
            .and_then(|_| fcntl(fd2, F_SETFL(OFlag::O_NONBLOCK)));
    }

    match res {
        Ok(_) => Ok(()),
        Err(e) => {
            let _ = close(fd1);
            let _ = close(fd2);
            Err(e)
        }
    }
}

/// Truncate a file to a specified length
///
/// See also
/// [truncate(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/truncate.html)
pub fn truncate<P: ?Sized + NixPath>(path: &P, len: off_t) -> Result<()> {
    let res = path.with_nix_path(|cstr| {
        unsafe {
            libc::truncate(cstr.as_ptr(), len)
        }
    })?;

    Errno::result(res).map(drop)
}

/// Truncate a file to a specified length
///
/// See also
/// [ftruncate(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/ftruncate.html)
pub fn ftruncate(fd: RawFd, len: off_t) -> Result<()> {
    Errno::result(unsafe { libc::ftruncate(fd, len) }).map(drop)
}

pub fn isatty(fd: RawFd) -> Result<bool> {
    unsafe {
        // ENOTTY means `fd` is a valid file descriptor, but not a TTY, so
        // we return `Ok(false)`
        if libc::isatty(fd) == 1 {
            Ok(true)
        } else {
            match Errno::last() {
                Errno::ENOTTY => Ok(false),
                err => Err(Error::Sys(err)),
            }
        }
    }
}

/// Remove a directory entry
///
/// See also [unlink(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/unlink.html)
pub fn unlink<P: ?Sized + NixPath>(path: &P) -> Result<()> {
    let res = path.with_nix_path(|cstr| {
        unsafe {
            libc::unlink(cstr.as_ptr())
        }
    })?;
    Errno::result(res).map(drop)
}

/// Flags for `unlinkat` function.
#[derive(Clone, Copy, Debug)]
pub enum UnlinkatFlags {
    RemoveDir,
    NoRemoveDir,
}

/// Remove a directory entry
///
/// In the case of a relative path, the directory entry to be removed is determined relative to
/// the directory associated with the file descriptor `dirfd` or the current working directory
/// if `dirfd` is `None`. In the case of an absolute `path` `dirfd` is ignored. If `flag` is
/// `UnlinkatFlags::RemoveDir` then removal of the directory entry specified by `dirfd` and `path`
/// is performed.
///
/// # References
/// See also [unlinkat(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/unlinkat.html)
pub fn unlinkat<P: ?Sized + NixPath>(
    dirfd: Option<RawFd>,
    path: &P,
    flag: UnlinkatFlags,
) -> Result<()> {
    let atflag =
        match flag {
            UnlinkatFlags::RemoveDir => AtFlags::AT_REMOVEDIR,
            UnlinkatFlags::NoRemoveDir => AtFlags::empty(),
        };
    let res = path.with_nix_path(|cstr| {
        unsafe {
            libc::unlinkat(at_rawfd(dirfd), cstr.as_ptr(), atflag.bits() as libc::c_int)
        }
    })?;
    Errno::result(res).map(drop)
}


#[inline]
pub fn chroot<P: ?Sized + NixPath>(path: &P) -> Result<()> {
    let res = path.with_nix_path(|cstr| {
        unsafe { libc::chroot(cstr.as_ptr()) }
    })?;

    Errno::result(res).map(drop)
}

/// Commit filesystem caches to disk
///
/// See also [sync(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/sync.html)
#[cfg(any(
    target_os = "dragonfly",
    target_os = "freebsd",
    target_os = "linux",
    target_os = "netbsd",
    target_os = "openbsd"
))]
pub fn sync() -> () {
    unsafe { libc::sync() };
}

/// Synchronize changes to a file
///
/// See also [fsync(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/fsync.html)
#[inline]
pub fn fsync(fd: RawFd) -> Result<()> {
    let res = unsafe { libc::fsync(fd) };

    Errno::result(res).map(drop)
}

/// Synchronize the data of a file
///
/// See also
/// [fdatasync(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/fdatasync.html)
// `fdatasync(2) is in POSIX, but in libc it is only defined in `libc::notbsd`.
// TODO: exclude only Apple systems after https://github.com/rust-lang/libc/pull/211
#[cfg(any(target_os = "linux",
          target_os = "android",
          target_os = "emscripten"))]
#[inline]
pub fn fdatasync(fd: RawFd) -> Result<()> {
    let res = unsafe { libc::fdatasync(fd) };

    Errno::result(res).map(drop)
}

/// Get a real user ID
///
/// See also [getuid(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/getuid.html)
// POSIX requires that getuid is always successful, so no need to check return
// value or errno.
#[inline]
pub fn getuid() -> Uid {
    Uid(unsafe { libc::getuid() })
}

/// Get the effective user ID
///
/// See also [geteuid(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/geteuid.html)
// POSIX requires that geteuid is always successful, so no need to check return
// value or errno.
#[inline]
pub fn geteuid() -> Uid {
    Uid(unsafe { libc::geteuid() })
}

/// Get the real group ID
///
/// See also [getgid(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/getgid.html)
// POSIX requires that getgid is always successful, so no need to check return
// value or errno.
#[inline]
pub fn getgid() -> Gid {
    Gid(unsafe { libc::getgid() })
}

/// Get the effective group ID
///
/// See also [getegid(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/getegid.html)
// POSIX requires that getegid is always successful, so no need to check return
// value or errno.
#[inline]
pub fn getegid() -> Gid {
    Gid(unsafe { libc::getegid() })
}

/// Set the effective user ID
///
/// See also [seteuid(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/seteuid.html)
#[inline]
pub fn seteuid(euid: Uid) -> Result<()> {
    let res = unsafe { libc::seteuid(euid.into()) };

    Errno::result(res).map(drop)
}

/// Set the effective group ID
///
/// See also [setegid(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/setegid.html)
#[inline]
pub fn setegid(egid: Gid) -> Result<()> {
    let res = unsafe { libc::setegid(egid.into()) };

    Errno::result(res).map(drop)
}

/// Set the user ID
///
/// See also [setuid(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/setuid.html)
#[inline]
pub fn setuid(uid: Uid) -> Result<()> {
    let res = unsafe { libc::setuid(uid.into()) };

    Errno::result(res).map(drop)
}

/// Set the group ID
///
/// See also [setgid(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/setgid.html)
#[inline]
pub fn setgid(gid: Gid) -> Result<()> {
    let res = unsafe { libc::setgid(gid.into()) };

    Errno::result(res).map(drop)
}

/// Get the list of supplementary group IDs of the calling process.
///
/// [Further reading](http://pubs.opengroup.org/onlinepubs/009695399/functions/getgroups.html)
///
/// **Note:** This function is not available for Apple platforms. On those
/// platforms, checking group membership should be achieved via communication
/// with the `opendirectoryd` service.
#[cfg(not(any(target_os = "ios", target_os = "macos")))]
pub fn getgroups() -> Result<Vec<Gid>> {
    // First get the number of groups so we can size our Vec
    let ret = unsafe { libc::getgroups(0, ptr::null_mut()) };

    // Now actually get the groups. We try multiple times in case the number of
    // groups has changed since the first call to getgroups() and the buffer is
    // now too small.
    let mut groups = Vec::<Gid>::with_capacity(Errno::result(ret)? as usize);
    loop {
        // FIXME: On the platforms we currently support, the `Gid` struct has
        // the same representation in memory as a bare `gid_t`. This is not
        // necessarily the case on all Rust platforms, though. See RFC 1785.
        let ret = unsafe {
            libc::getgroups(groups.capacity() as c_int, groups.as_mut_ptr() as *mut gid_t)
        };

        match Errno::result(ret) {
            Ok(s) => {
                unsafe { groups.set_len(s as usize) };
                return Ok(groups);
            },
            Err(Error::Sys(Errno::EINVAL)) => {
                // EINVAL indicates that the buffer size was too small. Trigger
                // the internal buffer resizing logic of `Vec` by requiring
                // more space than the current capacity.
                let cap = groups.capacity();
                unsafe { groups.set_len(cap) };
                groups.reserve(1);
            },
            Err(e) => return Err(e)
        }
    }
}

/// Set the list of supplementary group IDs for the calling process.
///
/// [Further reading](http://man7.org/linux/man-pages/man2/getgroups.2.html)
///
/// **Note:** This function is not available for Apple platforms. On those
/// platforms, group membership management should be achieved via communication
/// with the `opendirectoryd` service.
///
/// # Examples
///
/// `setgroups` can be used when dropping privileges from the root user to a
/// specific user and group. For example, given the user `www-data` with UID
/// `33` and the group `backup` with the GID `34`, one could switch the user as
/// follows:
///
/// ```rust,no_run
/// # use std::error::Error;
/// # use nix::unistd::*;
/// #
/// # fn try_main() -> Result<(), Box<Error>> {
/// let uid = Uid::from_raw(33);
/// let gid = Gid::from_raw(34);
/// setgroups(&[gid])?;
/// setgid(gid)?;
/// setuid(uid)?;
/// #
/// #     Ok(())
/// # }
/// #
/// # fn main() {
/// #     try_main().unwrap();
/// # }
/// ```
#[cfg(not(any(target_os = "ios", target_os = "macos")))]
pub fn setgroups(groups: &[Gid]) -> Result<()> {
    cfg_if! {
        if #[cfg(any(target_os = "dragonfly",
                     target_os = "freebsd",
                     target_os = "ios",
                     target_os = "macos",
                     target_os = "netbsd",
                     target_os = "openbsd"))] {
            type setgroups_ngroups_t = c_int;
        } else {
            type setgroups_ngroups_t = size_t;
        }
    }
    // FIXME: On the platforms we currently support, the `Gid` struct has the
    // same representation in memory as a bare `gid_t`. This is not necessarily
    // the case on all Rust platforms, though. See RFC 1785.
    let res = unsafe {
        libc::setgroups(groups.len() as setgroups_ngroups_t, groups.as_ptr() as *const gid_t)
    };

    Errno::result(res).map(drop)
}

/// Calculate the supplementary group access list.
///
/// Gets the group IDs of all groups that `user` is a member of. The additional
/// group `group` is also added to the list.
///
/// [Further reading](http://man7.org/linux/man-pages/man3/getgrouplist.3.html)
///
/// **Note:** This function is not available for Apple platforms. On those
/// platforms, checking group membership should be achieved via communication
/// with the `opendirectoryd` service.
///
/// # Errors
///
/// Although the `getgrouplist()` call does not return any specific
/// errors on any known platforms, this implementation will return a system
/// error of `EINVAL` if the number of groups to be fetched exceeds the
/// `NGROUPS_MAX` sysconf value. This mimics the behaviour of `getgroups()`
/// and `setgroups()`. Additionally, while some implementations will return a
/// partial list of groups when `NGROUPS_MAX` is exceeded, this implementation
/// will only ever return the complete list or else an error.
#[cfg(not(any(target_os = "ios", target_os = "macos")))]
pub fn getgrouplist(user: &CStr, group: Gid) -> Result<Vec<Gid>> {
    let ngroups_max = match sysconf(SysconfVar::NGROUPS_MAX) {
        Ok(Some(n)) => n as c_int,
        Ok(None) | Err(_) => <c_int>::max_value(),
    };
    use std::cmp::min;
    let mut ngroups = min(ngroups_max, 8);
    let mut groups = Vec::<Gid>::with_capacity(ngroups as usize);
    cfg_if! {
        if #[cfg(any(target_os = "ios", target_os = "macos"))] {
            type getgrouplist_group_t = c_int;
        } else {
            type getgrouplist_group_t = gid_t;
        }
    }
    let gid: gid_t = group.into();
    loop {
        let ret = unsafe {
            libc::getgrouplist(user.as_ptr(),
                               gid as getgrouplist_group_t,
                               groups.as_mut_ptr() as *mut getgrouplist_group_t,
                               &mut ngroups)
        };

        // BSD systems only return 0 or -1, Linux returns ngroups on success.
        if ret >= 0 {
            unsafe { groups.set_len(ngroups as usize) };
            return Ok(groups);
        } else if ret == -1 {
            // Returns -1 if ngroups is too small, but does not set errno.
            // BSD systems will still fill the groups buffer with as many
            // groups as possible, but Linux manpages do not mention this
            // behavior.

            let cap = groups.capacity();
            if cap >= ngroups_max as usize {
                // We already have the largest capacity we can, give up
                return Err(Error::invalid_argument());
            }

            // Reserve space for at least ngroups
            groups.reserve(ngroups as usize);

            // Even if the buffer gets resized to bigger than ngroups_max,
            // don't ever ask for more than ngroups_max groups
            ngroups = min(ngroups_max, groups.capacity() as c_int);
        }
    }
}

/// Initialize the supplementary group access list.
///
/// Sets the supplementary group IDs for the calling process using all groups
/// that `user` is a member of. The additional group `group` is also added to
/// the list.
///
/// [Further reading](http://man7.org/linux/man-pages/man3/initgroups.3.html)
///
/// **Note:** This function is not available for Apple platforms. On those
/// platforms, group membership management should be achieved via communication
/// with the `opendirectoryd` service.
///
/// # Examples
///
/// `initgroups` can be used when dropping privileges from the root user to
/// another user. For example, given the user `www-data`, we could look up the
/// UID and GID for the user in the system's password database (usually found
/// in `/etc/passwd`). If the `www-data` user's UID and GID were `33` and `33`,
/// respectively, one could switch the user as follows:
///
/// ```rust,no_run
/// # use std::error::Error;
/// # use std::ffi::CString;
/// # use nix::unistd::*;
/// #
/// # fn try_main() -> Result<(), Box<Error>> {
/// let user = CString::new("www-data").unwrap();
/// let uid = Uid::from_raw(33);
/// let gid = Gid::from_raw(33);
/// initgroups(&user, gid)?;
/// setgid(gid)?;
/// setuid(uid)?;
/// #
/// #     Ok(())
/// # }
/// #
/// # fn main() {
/// #     try_main().unwrap();
/// # }
/// ```
#[cfg(not(any(target_os = "ios", target_os = "macos")))]
pub fn initgroups(user: &CStr, group: Gid) -> Result<()> {
    cfg_if! {
        if #[cfg(any(target_os = "ios", target_os = "macos"))] {
            type initgroups_group_t = c_int;
        } else {
            type initgroups_group_t = gid_t;
        }
    }
    let gid: gid_t = group.into();
    let res = unsafe { libc::initgroups(user.as_ptr(), gid as initgroups_group_t) };

    Errno::result(res).map(drop)
}

/// Suspend the thread until a signal is received.
///
/// See also [pause(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/pause.html).
#[inline]
pub fn pause() {
    unsafe { libc::pause() };
}

pub mod alarm {
    //! Alarm signal scheduling.
    //!
    //! Scheduling an alarm will trigger a `SIGALRM` signal when the time has
    //! elapsed, which has to be caught, because the default action for the
    //! signal is to terminate the program. This signal also can't be ignored
    //! because the system calls like `pause` will not be interrupted, see the
    //! second example below.
    //!
    //! # Examples
    //!
    //! Canceling an alarm:
    //!
    //! ```
    //! use nix::unistd::alarm;
    //!
    //! // Set an alarm for 60 seconds from now.
    //! alarm::set(60);
    //!
    //! // Cancel the above set alarm, which returns the number of seconds left
    //! // of the previously set alarm.
    //! assert_eq!(alarm::cancel(), Some(60));
    //! ```
    //!
    //! Scheduling an alarm and waiting for the signal:
    //!
    //! ```
    //! use std::time::{Duration, Instant};
    //!
    //! use nix::unistd::{alarm, pause};
    //! use nix::sys::signal::*;
    //!
    //! // We need to setup an empty signal handler to catch the alarm signal,
    //! // otherwise the program will be terminated once the signal is delivered.
    //! extern fn signal_handler(_: nix::libc::c_int) { }
    //! unsafe { sigaction(Signal::SIGALRM, &SigAction::new(SigHandler::Handler(signal_handler), SaFlags::empty(), SigSet::empty())); }
    //!
    //! // Set an alarm for 1 second from now.
    //! alarm::set(1);
    //!
    //! let start = Instant::now();
    //! // Pause the process until the alarm signal is received.
    //! pause();
    //!
    //! assert!(start.elapsed() >= Duration::from_secs(1));
    //! ```
    //!
    //! # References
    //!
    //! See also [alarm(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/alarm.html).

    use libc;

    /// Schedule an alarm signal.
    ///
    /// This will cause the system to generate a `SIGALRM` signal for the
    /// process after the specified number of seconds have elapsed.
    ///
    /// Returns the leftover time of a previously set alarm if there was one.
    pub fn set(secs: libc::c_uint) -> Option<libc::c_uint> {
        assert!(secs != 0, "passing 0 to `alarm::set` is not allowed, to cancel an alarm use `alarm::cancel`");
        alarm(secs)
    }

    /// Cancel an previously set alarm signal.
    ///
    /// Returns the leftover time of a previously set alarm if there was one.
    pub fn cancel() -> Option<libc::c_uint> {
        alarm(0)
    }

    fn alarm(secs: libc::c_uint) -> Option<libc::c_uint> {
        match unsafe { libc::alarm(secs) } {
            0 => None,
            secs => Some(secs),
        }
    }
}

/// Suspend execution for an interval of time
///
/// See also [sleep(2)](http://pubs.opengroup.org/onlinepubs/009695399/functions/sleep.html#tag_03_705_05)
// Per POSIX, does not fail
#[inline]
pub fn sleep(seconds: c_uint) -> c_uint {
    unsafe { libc::sleep(seconds) }
}

pub mod acct {
    use libc;
    use {Result, NixPath};
    use errno::Errno;
    use std::ptr;

    /// Enable process accounting
    ///
    /// See also [acct(2)](https://linux.die.net/man/2/acct)
    pub fn enable<P: ?Sized + NixPath>(filename: &P) -> Result<()> {
        let res = filename.with_nix_path(|cstr| {
            unsafe { libc::acct(cstr.as_ptr()) }
        })?;

        Errno::result(res).map(drop)
    }

    /// Disable process accounting
    pub fn disable() -> Result<()> {
        let res = unsafe { libc::acct(ptr::null()) };

        Errno::result(res).map(drop)
    }
}

/// Creates a regular file which persists even after process termination
///
/// * `template`: a path whose 6 rightmost characters must be X, e.g. `/tmp/tmpfile_XXXXXX`
/// * returns: tuple of file descriptor and filename
///
/// Err is returned either if no temporary filename could be created or the template doesn't
/// end with XXXXXX
///
/// See also [mkstemp(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/mkstemp.html)
///
/// # Example
///
/// ```rust
/// use nix::unistd;
///
/// let _ = match unistd::mkstemp("/tmp/tempfile_XXXXXX") {
///     Ok((fd, path)) => {
///         unistd::unlink(path.as_path()).unwrap(); // flag file to be deleted at app termination
///         fd
///     }
///     Err(e) => panic!("mkstemp failed: {}", e)
/// };
/// // do something with fd
/// ```
#[inline]
pub fn mkstemp<P: ?Sized + NixPath>(template: &P) -> Result<(RawFd, PathBuf)> {
    let mut path = template.with_nix_path(|path| {path.to_bytes_with_nul().to_owned()})?;
    let p = path.as_mut_ptr() as *mut _;
    let fd = unsafe { libc::mkstemp(p) };
    let last = path.pop(); // drop the trailing nul
    debug_assert!(last == Some(b'\0'));
    let pathname = OsString::from_vec(path);
    Errno::result(fd)?;
    Ok((fd, PathBuf::from(pathname)))
}

/// Variable names for `pathconf`
///
/// Nix uses the same naming convention for these variables as the
/// [getconf(1)](http://pubs.opengroup.org/onlinepubs/9699919799/utilities/getconf.html) utility.
/// That is, `PathconfVar` variables have the same name as the abstract
/// variables  shown in the `pathconf(2)` man page.  Usually, it's the same as
/// the C variable name without the leading `_PC_`.
///
/// POSIX 1003.1-2008 standardizes all of these variables, but some OSes choose
/// not to implement variables that cannot change at runtime.
///
/// # References
///
/// - [pathconf(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/pathconf.html)
/// - [limits.h](http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/limits.h.html)
/// - [unistd.h](http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/unistd.h.html)
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
#[repr(i32)]
pub enum PathconfVar {
    #[cfg(any(target_os = "dragonfly", target_os = "freebsd", target_os = "linux",
              target_os = "netbsd", target_os = "openbsd"))]
    /// Minimum number of bits needed to represent, as a signed integer value,
    /// the maximum size of a regular file allowed in the specified directory.
    FILESIZEBITS = libc::_PC_FILESIZEBITS,
    /// Maximum number of links to a single file.
    LINK_MAX = libc::_PC_LINK_MAX,
    /// Maximum number of bytes in a terminal canonical input line.
    MAX_CANON = libc::_PC_MAX_CANON,
    /// Minimum number of bytes for which space is available in a terminal input
    /// queue; therefore, the maximum number of bytes a conforming application
    /// may require to be typed as input before reading them.
    MAX_INPUT = libc::_PC_MAX_INPUT,
    /// Maximum number of bytes in a filename (not including the terminating
    /// null of a filename string).
    NAME_MAX = libc::_PC_NAME_MAX,
    /// Maximum number of bytes the implementation will store as a pathname in a
    /// user-supplied buffer of unspecified size, including the terminating null
    /// character. Minimum number the implementation will accept as the maximum
    /// number of bytes in a pathname.
    PATH_MAX = libc::_PC_PATH_MAX,
    /// Maximum number of bytes that is guaranteed to be atomic when writing to
    /// a pipe.
    PIPE_BUF = libc::_PC_PIPE_BUF,
    #[cfg(any(target_os = "android", target_os = "dragonfly", target_os = "linux",
              target_os = "netbsd", target_os = "openbsd"))]
    /// Symbolic links can be created.
    POSIX2_SYMLINKS = libc::_PC_2_SYMLINKS,
    #[cfg(any(target_os = "android", target_os = "dragonfly", target_os = "freebsd",
              target_os = "linux", target_os = "openbsd"))]
    /// Minimum number of bytes of storage actually allocated for any portion of
    /// a file.
    POSIX_ALLOC_SIZE_MIN = libc::_PC_ALLOC_SIZE_MIN,
    #[cfg(any(target_os = "android", target_os = "dragonfly", target_os = "freebsd",
              target_os = "linux", target_os = "openbsd"))]
    /// Recommended increment for file transfer sizes between the
    /// `POSIX_REC_MIN_XFER_SIZE` and `POSIX_REC_MAX_XFER_SIZE` values.
    POSIX_REC_INCR_XFER_SIZE = libc::_PC_REC_INCR_XFER_SIZE,
    #[cfg(any(target_os = "android", target_os = "dragonfly", target_os = "freebsd",
              target_os = "linux", target_os = "openbsd"))]
    /// Maximum recommended file transfer size.
    POSIX_REC_MAX_XFER_SIZE = libc::_PC_REC_MAX_XFER_SIZE,
    #[cfg(any(target_os = "android", target_os = "dragonfly", target_os = "freebsd",
              target_os = "linux", target_os = "openbsd"))]
    /// Minimum recommended file transfer size.
    POSIX_REC_MIN_XFER_SIZE = libc::_PC_REC_MIN_XFER_SIZE,
    #[cfg(any(target_os = "android", target_os = "dragonfly", target_os = "freebsd",
              target_os = "linux", target_os = "openbsd"))]
    ///  Recommended file transfer buffer alignment.
    POSIX_REC_XFER_ALIGN = libc::_PC_REC_XFER_ALIGN,
    #[cfg(any(target_os = "android", target_os = "dragonfly", target_os = "freebsd",
              target_os = "linux", target_os = "netbsd", target_os = "openbsd"))]
    /// Maximum number of bytes in a symbolic link.
    SYMLINK_MAX = libc::_PC_SYMLINK_MAX,
    /// The use of `chown` and `fchown` is restricted to a process with
    /// appropriate privileges, and to changing the group ID of a file only to
    /// the effective group ID of the process or to one of its supplementary
    /// group IDs.
    _POSIX_CHOWN_RESTRICTED = libc::_PC_CHOWN_RESTRICTED,
    /// Pathname components longer than {NAME_MAX} generate an error.
    _POSIX_NO_TRUNC = libc::_PC_NO_TRUNC,
    /// This symbol shall be defined to be the value of a character that shall
    /// disable terminal special character handling.
    _POSIX_VDISABLE = libc::_PC_VDISABLE,
    #[cfg(any(target_os = "android", target_os = "dragonfly", target_os = "freebsd",
              target_os = "linux", target_os = "openbsd"))]
    /// Asynchronous input or output operations may be performed for the
    /// associated file.
    _POSIX_ASYNC_IO = libc::_PC_ASYNC_IO,
    #[cfg(any(target_os = "android", target_os = "dragonfly", target_os = "freebsd",
              target_os = "linux", target_os = "openbsd"))]
    /// Prioritized input or output operations may be performed for the
    /// associated file.
    _POSIX_PRIO_IO = libc::_PC_PRIO_IO,
    #[cfg(any(target_os = "android", target_os = "dragonfly", target_os = "freebsd",
              target_os = "linux", target_os = "netbsd", target_os = "openbsd"))]
    /// Synchronized input or output operations may be performed for the
    /// associated file.
    _POSIX_SYNC_IO = libc::_PC_SYNC_IO,
    #[cfg(any(target_os = "dragonfly", target_os = "openbsd"))]
    /// The resolution in nanoseconds for all file timestamps.
    _POSIX_TIMESTAMP_RESOLUTION = libc::_PC_TIMESTAMP_RESOLUTION
}

/// Like `pathconf`, but works with file descriptors instead of paths (see
/// [fpathconf(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/pathconf.html))
///
/// # Parameters
///
/// - `fd`:   The file descriptor whose variable should be interrogated
/// - `var`:  The pathconf variable to lookup
///
/// # Returns
///
/// - `Ok(Some(x))`: the variable's limit (for limit variables) or its
///     implementation level (for option variables).  Implementation levels are
///     usually a decimal-coded date, such as 200112 for POSIX 2001.12
/// - `Ok(None)`: the variable has no limit (for limit variables) or is
///     unsupported (for option variables)
/// - `Err(x)`: an error occurred
pub fn fpathconf(fd: RawFd, var: PathconfVar) -> Result<Option<c_long>> {
    let raw = unsafe {
        Errno::clear();
        libc::fpathconf(fd, var as c_int)
    };
    if raw == -1 {
        if errno::errno() == 0 {
            Ok(None)
        } else {
            Err(Error::Sys(Errno::last()))
        }
    } else {
        Ok(Some(raw))
    }
}

/// Get path-dependent configurable system variables (see
/// [pathconf(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/pathconf.html))
///
/// Returns the value of a path-dependent configurable system variable.  Most
/// supported variables also have associated compile-time constants, but POSIX
/// allows their values to change at runtime.  There are generally two types of
/// `pathconf` variables: options and limits.  See [pathconf(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/pathconf.html) for more details.
///
/// # Parameters
///
/// - `path`: Lookup the value of `var` for this file or directory
/// - `var`:  The `pathconf` variable to lookup
///
/// # Returns
///
/// - `Ok(Some(x))`: the variable's limit (for limit variables) or its
///     implementation level (for option variables).  Implementation levels are
///     usually a decimal-coded date, such as 200112 for POSIX 2001.12
/// - `Ok(None)`: the variable has no limit (for limit variables) or is
///     unsupported (for option variables)
/// - `Err(x)`: an error occurred
pub fn pathconf<P: ?Sized + NixPath>(path: &P, var: PathconfVar) -> Result<Option<c_long>> {
    let raw = path.with_nix_path(|cstr| {
        unsafe {
            Errno::clear();
            libc::pathconf(cstr.as_ptr(), var as c_int)
        }
    })?;
    if raw == -1 {
        if errno::errno() == 0 {
            Ok(None)
        } else {
            Err(Error::Sys(Errno::last()))
        }
    } else {
        Ok(Some(raw))
    }
}

/// Variable names for `sysconf`
///
/// Nix uses the same naming convention for these variables as the
/// [getconf(1)](http://pubs.opengroup.org/onlinepubs/9699919799/utilities/getconf.html) utility.
/// That is, `SysconfVar` variables have the same name as the abstract variables
/// shown in the `sysconf(3)` man page.  Usually, it's the same as the C
/// variable name without the leading `_SC_`.
///
/// All of these symbols are standardized by POSIX 1003.1-2008, but haven't been
/// implemented by all platforms.
///
/// # References
///
/// - [sysconf(3)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/sysconf.html)
/// - [unistd.h](http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/unistd.h.html)
/// - [limits.h](http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/limits.h.html)
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
#[repr(i32)]
pub enum SysconfVar {
    /// Maximum number of I/O operations in a single list I/O call supported by
    /// the implementation.
    AIO_LISTIO_MAX = libc::_SC_AIO_LISTIO_MAX,
    /// Maximum number of outstanding asynchronous I/O operations supported by
    /// the implementation.
    AIO_MAX = libc::_SC_AIO_MAX,
    #[cfg(any(target_os="android", target_os="dragonfly", target_os="freebsd",
              target_os = "ios", target_os="linux", target_os = "macos",
              target_os="openbsd"))]
    /// The maximum amount by which a process can decrease its asynchronous I/O
    /// priority level from its own scheduling priority.
    AIO_PRIO_DELTA_MAX = libc::_SC_AIO_PRIO_DELTA_MAX,
    /// Maximum length of argument to the exec functions including environment data.
    ARG_MAX = libc::_SC_ARG_MAX,
    /// Maximum number of functions that may be registered with `atexit`.
    ATEXIT_MAX = libc::_SC_ATEXIT_MAX,
    /// Maximum obase values allowed by the bc utility.
    BC_BASE_MAX = libc::_SC_BC_BASE_MAX,
    /// Maximum number of elements permitted in an array by the bc utility.
    BC_DIM_MAX = libc::_SC_BC_DIM_MAX,
    /// Maximum scale value allowed by the bc utility.
    BC_SCALE_MAX = libc::_SC_BC_SCALE_MAX,
    /// Maximum length of a string constant accepted by the bc utility.
    BC_STRING_MAX = libc::_SC_BC_STRING_MAX,
    /// Maximum number of simultaneous processes per real user ID.
    CHILD_MAX = libc::_SC_CHILD_MAX,
    // _SC_CLK_TCK is obsolete
    /// Maximum number of weights that can be assigned to an entry of the
    /// LC_COLLATE order keyword in the locale definition file
    COLL_WEIGHTS_MAX = libc::_SC_COLL_WEIGHTS_MAX,
    /// Maximum number of timer expiration overruns.
    DELAYTIMER_MAX = libc::_SC_DELAYTIMER_MAX,
    /// Maximum number of expressions that can be nested within parentheses by
    /// the expr utility.
    EXPR_NEST_MAX = libc::_SC_EXPR_NEST_MAX,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="netbsd",
              target_os="openbsd"))]
    /// Maximum length of a host name (not including the terminating null) as
    /// returned from the `gethostname` function
    HOST_NAME_MAX = libc::_SC_HOST_NAME_MAX,
    /// Maximum number of iovec structures that one process has available for
    /// use with `readv` or `writev`.
    IOV_MAX = libc::_SC_IOV_MAX,
    /// Unless otherwise noted, the maximum length, in bytes, of a utility's
    /// input line (either standard input or another file), when the utility is
    /// described as processing text files. The length includes room for the
    /// trailing <newline>.
    LINE_MAX = libc::_SC_LINE_MAX,
    /// Maximum length of a login name.
    LOGIN_NAME_MAX = libc::_SC_LOGIN_NAME_MAX,
    /// Maximum number of simultaneous supplementary group IDs per process.
    NGROUPS_MAX = libc::_SC_NGROUPS_MAX,
    /// Initial size of `getgrgid_r` and `getgrnam_r` data buffers
    GETGR_R_SIZE_MAX = libc::_SC_GETGR_R_SIZE_MAX,
    /// Initial size of `getpwuid_r` and `getpwnam_r` data buffers
    GETPW_R_SIZE_MAX = libc::_SC_GETPW_R_SIZE_MAX,
    /// The maximum number of open message queue descriptors a process may hold.
    MQ_OPEN_MAX = libc::_SC_MQ_OPEN_MAX,
    /// The maximum number of message priorities supported by the implementation.
    MQ_PRIO_MAX = libc::_SC_MQ_PRIO_MAX,
    /// A value one greater than the maximum value that the system may assign to
    /// a newly-created file descriptor.
    OPEN_MAX = libc::_SC_OPEN_MAX,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="openbsd"))]
    /// The implementation supports the Advisory Information option.
    _POSIX_ADVISORY_INFO = libc::_SC_ADVISORY_INFO,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="netbsd",
              target_os="openbsd"))]
    /// The implementation supports barriers.
    _POSIX_BARRIERS = libc::_SC_BARRIERS,
    /// The implementation supports asynchronous input and output.
    _POSIX_ASYNCHRONOUS_IO = libc::_SC_ASYNCHRONOUS_IO,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="netbsd",
              target_os="openbsd"))]
    /// The implementation supports clock selection.
    _POSIX_CLOCK_SELECTION = libc::_SC_CLOCK_SELECTION,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="netbsd",
              target_os="openbsd"))]
    /// The implementation supports the Process CPU-Time Clocks option.
    _POSIX_CPUTIME = libc::_SC_CPUTIME,
    /// The implementation supports the File Synchronization option.
    _POSIX_FSYNC = libc::_SC_FSYNC,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="openbsd"))]
    /// The implementation supports the IPv6 option.
    _POSIX_IPV6 = libc::_SC_IPV6,
    /// The implementation supports job control.
    _POSIX_JOB_CONTROL = libc::_SC_JOB_CONTROL,
    /// The implementation supports memory mapped Files.
    _POSIX_MAPPED_FILES = libc::_SC_MAPPED_FILES,
    /// The implementation supports the Process Memory Locking option.
    _POSIX_MEMLOCK = libc::_SC_MEMLOCK,
    /// The implementation supports the Range Memory Locking option.
    _POSIX_MEMLOCK_RANGE = libc::_SC_MEMLOCK_RANGE,
    /// The implementation supports memory protection.
    _POSIX_MEMORY_PROTECTION = libc::_SC_MEMORY_PROTECTION,
    /// The implementation supports the Message Passing option.
    _POSIX_MESSAGE_PASSING = libc::_SC_MESSAGE_PASSING,
    /// The implementation supports the Monotonic Clock option.
    _POSIX_MONOTONIC_CLOCK = libc::_SC_MONOTONIC_CLOCK,
    #[cfg(any(target_os="android", target_os="dragonfly", target_os="freebsd",
              target_os = "ios", target_os="linux", target_os = "macos",
              target_os="openbsd"))]
    /// The implementation supports the Prioritized Input and Output option.
    _POSIX_PRIORITIZED_IO = libc::_SC_PRIORITIZED_IO,
    /// The implementation supports the Process Scheduling option.
    _POSIX_PRIORITY_SCHEDULING = libc::_SC_PRIORITY_SCHEDULING,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="openbsd"))]
    /// The implementation supports the Raw Sockets option.
    _POSIX_RAW_SOCKETS = libc::_SC_RAW_SOCKETS,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="netbsd",
              target_os="openbsd"))]
    /// The implementation supports read-write locks.
    _POSIX_READER_WRITER_LOCKS = libc::_SC_READER_WRITER_LOCKS,
    #[cfg(any(target_os = "android", target_os="dragonfly", target_os="freebsd",
              target_os = "ios", target_os="linux", target_os = "macos",
              target_os = "openbsd"))]
    /// The implementation supports realtime signals.
    _POSIX_REALTIME_SIGNALS = libc::_SC_REALTIME_SIGNALS,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="netbsd",
              target_os="openbsd"))]
    /// The implementation supports the Regular Expression Handling option.
    _POSIX_REGEXP = libc::_SC_REGEXP,
    /// Each process has a saved set-user-ID and a saved set-group-ID.
    _POSIX_SAVED_IDS = libc::_SC_SAVED_IDS,
    /// The implementation supports semaphores.
    _POSIX_SEMAPHORES = libc::_SC_SEMAPHORES,
    /// The implementation supports the Shared Memory Objects option.
    _POSIX_SHARED_MEMORY_OBJECTS = libc::_SC_SHARED_MEMORY_OBJECTS,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="netbsd",
              target_os="openbsd"))]
    /// The implementation supports the POSIX shell.
    _POSIX_SHELL = libc::_SC_SHELL,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="netbsd",
              target_os="openbsd"))]
    /// The implementation supports the Spawn option.
    _POSIX_SPAWN = libc::_SC_SPAWN,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="netbsd",
              target_os="openbsd"))]
    /// The implementation supports spin locks.
    _POSIX_SPIN_LOCKS = libc::_SC_SPIN_LOCKS,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="openbsd"))]
    /// The implementation supports the Process Sporadic Server option.
    _POSIX_SPORADIC_SERVER = libc::_SC_SPORADIC_SERVER,
    #[cfg(any(target_os = "ios", target_os="linux", target_os = "macos",
              target_os="openbsd"))]
    _POSIX_SS_REPL_MAX = libc::_SC_SS_REPL_MAX,
    /// The implementation supports the Synchronized Input and Output option.
    _POSIX_SYNCHRONIZED_IO = libc::_SC_SYNCHRONIZED_IO,
    /// The implementation supports the Thread Stack Address Attribute option.
    _POSIX_THREAD_ATTR_STACKADDR = libc::_SC_THREAD_ATTR_STACKADDR,
    /// The implementation supports the Thread Stack Size Attribute option.
    _POSIX_THREAD_ATTR_STACKSIZE = libc::_SC_THREAD_ATTR_STACKSIZE,
    #[cfg(any(target_os = "ios", target_os="linux", target_os = "macos",
              target_os="netbsd", target_os="openbsd"))]
    /// The implementation supports the Thread CPU-Time Clocks option.
    _POSIX_THREAD_CPUTIME = libc::_SC_THREAD_CPUTIME,
    /// The implementation supports the Non-Robust Mutex Priority Inheritance
    /// option.
    _POSIX_THREAD_PRIO_INHERIT = libc::_SC_THREAD_PRIO_INHERIT,
    /// The implementation supports the Non-Robust Mutex Priority Protection option.
    _POSIX_THREAD_PRIO_PROTECT = libc::_SC_THREAD_PRIO_PROTECT,
    /// The implementation supports the Thread Execution Scheduling option.
    _POSIX_THREAD_PRIORITY_SCHEDULING = libc::_SC_THREAD_PRIORITY_SCHEDULING,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="netbsd",
              target_os="openbsd"))]
    /// The implementation supports the Thread Process-Shared Synchronization
    /// option.
    _POSIX_THREAD_PROCESS_SHARED = libc::_SC_THREAD_PROCESS_SHARED,
    #[cfg(any(target_os="dragonfly", target_os="linux", target_os="openbsd"))]
    /// The implementation supports the Robust Mutex Priority Inheritance option.
    _POSIX_THREAD_ROBUST_PRIO_INHERIT = libc::_SC_THREAD_ROBUST_PRIO_INHERIT,
    #[cfg(any(target_os="dragonfly", target_os="linux", target_os="openbsd"))]
    /// The implementation supports the Robust Mutex Priority Protection option.
    _POSIX_THREAD_ROBUST_PRIO_PROTECT = libc::_SC_THREAD_ROBUST_PRIO_PROTECT,
    /// The implementation supports thread-safe functions.
    _POSIX_THREAD_SAFE_FUNCTIONS = libc::_SC_THREAD_SAFE_FUNCTIONS,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="openbsd"))]
    /// The implementation supports the Thread Sporadic Server option.
    _POSIX_THREAD_SPORADIC_SERVER = libc::_SC_THREAD_SPORADIC_SERVER,
    /// The implementation supports threads.
    _POSIX_THREADS = libc::_SC_THREADS,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="openbsd"))]
    /// The implementation supports timeouts.
    _POSIX_TIMEOUTS = libc::_SC_TIMEOUTS,
    /// The implementation supports timers.
    _POSIX_TIMERS = libc::_SC_TIMERS,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="openbsd"))]
    /// The implementation supports the Trace option.
    _POSIX_TRACE = libc::_SC_TRACE,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="openbsd"))]
    /// The implementation supports the Trace Event Filter option.
    _POSIX_TRACE_EVENT_FILTER = libc::_SC_TRACE_EVENT_FILTER,
    #[cfg(any(target_os = "ios", target_os="linux", target_os = "macos",
              target_os="openbsd"))]
    _POSIX_TRACE_EVENT_NAME_MAX = libc::_SC_TRACE_EVENT_NAME_MAX,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="openbsd"))]
    /// The implementation supports the Trace Inherit option.
    _POSIX_TRACE_INHERIT = libc::_SC_TRACE_INHERIT,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="openbsd"))]
    /// The implementation supports the Trace Log option.
    _POSIX_TRACE_LOG = libc::_SC_TRACE_LOG,
    #[cfg(any(target_os = "ios", target_os="linux", target_os = "macos",
              target_os="openbsd"))]
    _POSIX_TRACE_NAME_MAX = libc::_SC_TRACE_NAME_MAX,
    #[cfg(any(target_os = "ios", target_os="linux", target_os = "macos",
              target_os="openbsd"))]
    _POSIX_TRACE_SYS_MAX = libc::_SC_TRACE_SYS_MAX,
    #[cfg(any(target_os = "ios", target_os="linux", target_os = "macos",
              target_os="openbsd"))]
    _POSIX_TRACE_USER_EVENT_MAX = libc::_SC_TRACE_USER_EVENT_MAX,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="openbsd"))]
    /// The implementation supports the Typed Memory Objects option.
    _POSIX_TYPED_MEMORY_OBJECTS = libc::_SC_TYPED_MEMORY_OBJECTS,
    /// Integer value indicating version of this standard (C-language binding)
    /// to which the implementation conforms. For implementations conforming to
    /// POSIX.1-2008, the value shall be 200809L.
    _POSIX_VERSION = libc::_SC_VERSION,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="netbsd",
              target_os="openbsd"))]
    /// The implementation provides a C-language compilation environment with
    /// 32-bit `int`, `long`, `pointer`, and `off_t` types.
    _POSIX_V6_ILP32_OFF32 = libc::_SC_V6_ILP32_OFF32,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="netbsd",
              target_os="openbsd"))]
    /// The implementation provides a C-language compilation environment with
    /// 32-bit `int`, `long`, and pointer types and an `off_t` type using at
    /// least 64 bits.
    _POSIX_V6_ILP32_OFFBIG = libc::_SC_V6_ILP32_OFFBIG,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="netbsd",
              target_os="openbsd"))]
    /// The implementation provides a C-language compilation environment with
    /// 32-bit `int` and 64-bit `long`, `pointer`, and `off_t` types.
    _POSIX_V6_LP64_OFF64 = libc::_SC_V6_LP64_OFF64,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="netbsd",
              target_os="openbsd"))]
    /// The implementation provides a C-language compilation environment with an
    /// `int` type using at least 32 bits and `long`, pointer, and `off_t` types
    /// using at least 64 bits.
    _POSIX_V6_LPBIG_OFFBIG = libc::_SC_V6_LPBIG_OFFBIG,
    /// The implementation supports the C-Language Binding option.
    _POSIX2_C_BIND = libc::_SC_2_C_BIND,
    /// The implementation supports the C-Language Development Utilities option.
    _POSIX2_C_DEV = libc::_SC_2_C_DEV,
    /// The implementation supports the Terminal Characteristics option.
    _POSIX2_CHAR_TERM = libc::_SC_2_CHAR_TERM,
    /// The implementation supports the FORTRAN Development Utilities option.
    _POSIX2_FORT_DEV = libc::_SC_2_FORT_DEV,
    /// The implementation supports the FORTRAN Runtime Utilities option.
    _POSIX2_FORT_RUN = libc::_SC_2_FORT_RUN,
    /// The implementation supports the creation of locales by the localedef
    /// utility.
    _POSIX2_LOCALEDEF = libc::_SC_2_LOCALEDEF,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="netbsd",
              target_os="openbsd"))]
    /// The implementation supports the Batch Environment Services and Utilities
    /// option.
    _POSIX2_PBS = libc::_SC_2_PBS,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="netbsd",
              target_os="openbsd"))]
    /// The implementation supports the Batch Accounting option.
    _POSIX2_PBS_ACCOUNTING = libc::_SC_2_PBS_ACCOUNTING,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="netbsd",
              target_os="openbsd"))]
    /// The implementation supports the Batch Checkpoint/Restart option.
    _POSIX2_PBS_CHECKPOINT = libc::_SC_2_PBS_CHECKPOINT,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="netbsd",
              target_os="openbsd"))]
    /// The implementation supports the Locate Batch Job Request option.
    _POSIX2_PBS_LOCATE = libc::_SC_2_PBS_LOCATE,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="netbsd",
              target_os="openbsd"))]
    /// The implementation supports the Batch Job Message Request option.
    _POSIX2_PBS_MESSAGE = libc::_SC_2_PBS_MESSAGE,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="netbsd",
              target_os="openbsd"))]
    /// The implementation supports the Track Batch Job Request option.
    _POSIX2_PBS_TRACK = libc::_SC_2_PBS_TRACK,
    /// The implementation supports the Software Development Utilities option.
    _POSIX2_SW_DEV = libc::_SC_2_SW_DEV,
    /// The implementation supports the User Portability Utilities option.
    _POSIX2_UPE = libc::_SC_2_UPE,
    /// Integer value indicating version of the Shell and Utilities volume of
    /// POSIX.1 to which the implementation conforms.
    _POSIX2_VERSION = libc::_SC_2_VERSION,
    /// The size of a system page in bytes.
    ///
    /// POSIX also defines an alias named `PAGESIZE`, but Rust does not allow two
    /// enum constants to have the same value, so nix omits `PAGESIZE`.
    PAGE_SIZE = libc::_SC_PAGE_SIZE,
    PTHREAD_DESTRUCTOR_ITERATIONS = libc::_SC_THREAD_DESTRUCTOR_ITERATIONS,
    PTHREAD_KEYS_MAX = libc::_SC_THREAD_KEYS_MAX,
    PTHREAD_STACK_MIN = libc::_SC_THREAD_STACK_MIN,
    PTHREAD_THREADS_MAX = libc::_SC_THREAD_THREADS_MAX,
    RE_DUP_MAX = libc::_SC_RE_DUP_MAX,
    #[cfg(any(target_os="android", target_os="dragonfly", target_os="freebsd",
              target_os = "ios", target_os="linux", target_os = "macos",
              target_os="openbsd"))]
    RTSIG_MAX = libc::_SC_RTSIG_MAX,
    SEM_NSEMS_MAX = libc::_SC_SEM_NSEMS_MAX,
    #[cfg(any(target_os="android", target_os="dragonfly", target_os="freebsd",
              target_os = "ios", target_os="linux", target_os = "macos",
              target_os="openbsd"))]
    SEM_VALUE_MAX = libc::_SC_SEM_VALUE_MAX,
    #[cfg(any(target_os = "android", target_os="dragonfly", target_os="freebsd",
              target_os = "ios", target_os="linux", target_os = "macos",
              target_os = "openbsd"))]
    SIGQUEUE_MAX = libc::_SC_SIGQUEUE_MAX,
    STREAM_MAX = libc::_SC_STREAM_MAX,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="netbsd",
              target_os="openbsd"))]
    SYMLOOP_MAX = libc::_SC_SYMLOOP_MAX,
    TIMER_MAX = libc::_SC_TIMER_MAX,
    TTY_NAME_MAX = libc::_SC_TTY_NAME_MAX,
    TZNAME_MAX = libc::_SC_TZNAME_MAX,
    #[cfg(any(target_os="android", target_os="dragonfly", target_os="freebsd",
              target_os = "ios", target_os="linux", target_os = "macos",
              target_os="openbsd"))]
    /// The implementation supports the X/Open Encryption Option Group.
    _XOPEN_CRYPT = libc::_SC_XOPEN_CRYPT,
    #[cfg(any(target_os="android", target_os="dragonfly", target_os="freebsd",
              target_os = "ios", target_os="linux", target_os = "macos",
              target_os="openbsd"))]
    /// The implementation supports the Issue 4, Version 2 Enhanced
    /// Internationalization Option Group.
    _XOPEN_ENH_I18N = libc::_SC_XOPEN_ENH_I18N,
    #[cfg(any(target_os="android", target_os="dragonfly", target_os="freebsd",
              target_os = "ios", target_os="linux", target_os = "macos",
              target_os="openbsd"))]
    _XOPEN_LEGACY = libc::_SC_XOPEN_LEGACY,
    #[cfg(any(target_os="android", target_os="dragonfly", target_os="freebsd",
              target_os = "ios", target_os="linux", target_os = "macos",
              target_os="openbsd"))]
    /// The implementation supports the X/Open Realtime Option Group.
    _XOPEN_REALTIME = libc::_SC_XOPEN_REALTIME,
    #[cfg(any(target_os="android", target_os="dragonfly", target_os="freebsd",
              target_os = "ios", target_os="linux", target_os = "macos",
              target_os="openbsd"))]
    /// The implementation supports the X/Open Realtime Threads Option Group.
    _XOPEN_REALTIME_THREADS = libc::_SC_XOPEN_REALTIME_THREADS,
    /// The implementation supports the Issue 4, Version 2 Shared Memory Option
    /// Group.
    _XOPEN_SHM = libc::_SC_XOPEN_SHM,
    #[cfg(any(target_os="dragonfly", target_os="freebsd", target_os = "ios",
              target_os="linux", target_os = "macos", target_os="openbsd"))]
    /// The implementation supports the XSI STREAMS Option Group.
    _XOPEN_STREAMS = libc::_SC_XOPEN_STREAMS,
    #[cfg(any(target_os="android", target_os="dragonfly", target_os="freebsd",
              target_os = "ios", target_os="linux", target_os = "macos",
              target_os="openbsd"))]
    /// The implementation supports the XSI option
    _XOPEN_UNIX = libc::_SC_XOPEN_UNIX,
    #[cfg(any(target_os="android", target_os="dragonfly", target_os="freebsd",
              target_os = "ios", target_os="linux", target_os = "macos",
              target_os="openbsd"))]
    /// Integer value indicating version of the X/Open Portability Guide to
    /// which the implementation conforms.
    _XOPEN_VERSION = libc::_SC_XOPEN_VERSION,
}

/// Get configurable system variables (see
/// [sysconf(3)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/sysconf.html))
///
/// Returns the value of a configurable system variable.  Most supported
/// variables also have associated compile-time constants, but POSIX
/// allows their values to change at runtime.  There are generally two types of
/// sysconf variables: options and limits.  See sysconf(3) for more details.
///
/// # Returns
///
/// - `Ok(Some(x))`: the variable's limit (for limit variables) or its
///     implementation level (for option variables).  Implementation levels are
///     usually a decimal-coded date, such as 200112 for POSIX 2001.12
/// - `Ok(None)`: the variable has no limit (for limit variables) or is
///     unsupported (for option variables)
/// - `Err(x)`: an error occurred
pub fn sysconf(var: SysconfVar) -> Result<Option<c_long>> {
    let raw = unsafe {
        Errno::clear();
        libc::sysconf(var as c_int)
    };
    if raw == -1 {
        if errno::errno() == 0 {
            Ok(None)
        } else {
            Err(Error::Sys(Errno::last()))
        }
    } else {
        Ok(Some(raw))
    }
}

#[cfg(any(target_os = "android", target_os = "linux"))]
mod pivot_root {
    use libc;
    use {Result, NixPath};
    use errno::Errno;

    pub fn pivot_root<P1: ?Sized + NixPath, P2: ?Sized + NixPath>(
            new_root: &P1, put_old: &P2) -> Result<()> {
        let res = new_root.with_nix_path(|new_root| {
            put_old.with_nix_path(|put_old| {
                unsafe {
                    libc::syscall(libc::SYS_pivot_root, new_root.as_ptr(), put_old.as_ptr())
                }
            })
        })??;

        Errno::result(res).map(drop)
    }
}

#[cfg(any(target_os = "android", target_os = "freebsd",
          target_os = "linux", target_os = "openbsd"))]
mod setres {
    use libc;
    use Result;
    use errno::Errno;
    use super::{Uid, Gid};

    /// Sets the real, effective, and saved uid.
    /// ([see setresuid(2)](http://man7.org/linux/man-pages/man2/setresuid.2.html))
    ///
    /// * `ruid`: real user id
    /// * `euid`: effective user id
    /// * `suid`: saved user id
    /// * returns: Ok or libc error code.
    ///
    /// Err is returned if the user doesn't have permission to set this UID.
    #[inline]
    pub fn setresuid(ruid: Uid, euid: Uid, suid: Uid) -> Result<()> {
        let res = unsafe { libc::setresuid(ruid.into(), euid.into(), suid.into()) };

        Errno::result(res).map(drop)
    }

    /// Sets the real, effective, and saved gid.
    /// ([see setresuid(2)](http://man7.org/linux/man-pages/man2/setresuid.2.html))
    ///
    /// * `rgid`: real group id
    /// * `egid`: effective group id
    /// * `sgid`: saved group id
    /// * returns: Ok or libc error code.
    ///
    /// Err is returned if the user doesn't have permission to set this GID.
    #[inline]
    pub fn setresgid(rgid: Gid, egid: Gid, sgid: Gid) -> Result<()> {
        let res = unsafe { libc::setresgid(rgid.into(), egid.into(), sgid.into()) };

        Errno::result(res).map(drop)
    }
}

libc_bitflags!{
    /// Options for access()
    pub struct AccessFlags : c_int {
        /// Test for existence of file.
        F_OK;
        /// Test for read permission.
        R_OK;
        /// Test for write permission.
        W_OK;
        /// Test for execute (search) permission.
        X_OK;
    }
}

/// Checks the file named by `path` for accessibility according to the flags given by `amode`
/// See [access(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/access.html)
pub fn access<P: ?Sized + NixPath>(path: &P, amode: AccessFlags) -> Result<()> {
    let res = path.with_nix_path(|cstr| {
        unsafe {
            libc::access(cstr.as_ptr(), amode.bits)
        }
    })?;
    Errno::result(res).map(drop)
}
