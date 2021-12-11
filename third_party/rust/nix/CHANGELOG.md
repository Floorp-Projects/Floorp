# Change Log

All notable changes to this project will be documented in this file.
This project adheres to [Semantic Versioning](http://semver.org/).

## [Unreleased] - ReleaseDate
### Added
### Changed
### Fixed
### Removed

## [0.15.0] - 10 August 2019
### Added
- Added `MSG_WAITALL` to `MsgFlags` in `sys::socket`.
  ([#1079](https://github.com/nix-rust/nix/pull/1079))
- Implemented `Clone`, `Copy`, `Debug`, `Eq`, `Hash`, and `PartialEq` for most
  types that support them. ([#1035](https://github.com/nix-rust/nix/pull/1035))
- Added `copy_file_range` wrapper
  ([#1069](https://github.com/nix-rust/nix/pull/1069))
- Add `mkdirat`.
  ([#1084](https://github.com/nix-rust/nix/pull/1084))
- Add `posix_fadvise`.
  ([#1089](https://github.com/nix-rust/nix/pull/1089))
- Added `AF_VSOCK` to `AddressFamily`.
  ([#1091](https://github.com/nix-rust/nix/pull/1091))
- Add `unlinkat`
  ([#1058](https://github.com/nix-rust/nix/pull/1058))
- Add `renameat`.
  ([#1097](https://github.com/nix-rust/nix/pull/1097))

### Changed
- Support for `ifaddrs` now present when building for Android.
  ([#1077](https://github.com/nix-rust/nix/pull/1077))
- Minimum supported Rust version is now 1.31.0
  ([#1035](https://github.com/nix-rust/nix/pull/1035))
  ([#1095](https://github.com/nix-rust/nix/pull/1095))
- Now functions `statfs()` and `fstatfs()` return result with `Statfs` wrapper
  ([#928](https://github.com/nix-rust/nix/pull/928))

### Fixed
- Enabled `sched_yield` for all nix hosts.
  ([#1090](https://github.com/nix-rust/nix/pull/1090))

### Removed

## [0.14.1] - 2019-06-06
### Added
- Macros exported by `nix` may now be imported via `use` on the Rust 2018
  edition without importing helper macros on Linux targets.
  ([#1066](https://github.com/nix-rust/nix/pull/1066))

  For example, in Rust 2018, the `ioctl_read_bad!` macro can now be imported
  without importing the `convert_ioctl_res!` macro.

  ```rust
  use nix::ioctl_read_bad;

  ioctl_read_bad!(tcgets, libc::TCGETS, libc::termios);
  ```

### Changed
- Changed some public types from reexports of libc types like `uint32_t` to the
  native equivalents like `u32.`
  ([#1072](https://github.com/nix-rust/nix/pull/1072/commits))

### Fixed
- Fix the build on Android and Linux/mips with recent versions of libc.
  ([#1072](https://github.com/nix-rust/nix/pull/1072/commits))

### Removed

## [0.14.0] - 2019-05-21
### Added
- Add IP_RECVIF & IP_RECVDSTADDR. Enable IP_PKTINFO and IP6_PKTINFO on netbsd/openbsd.
  ([#1002](https://github.com/nix-rust/nix/pull/1002))
- Added `inotify_init1`, `inotify_add_watch` and `inotify_rm_watch` wrappers for
  Android and Linux. ([#1016](https://github.com/nix-rust/nix/pull/1016))
- Add `ALG_SET_IV`, `ALG_SET_OP` and `ALG_SET_AEAD_ASSOCLEN` control messages and `AF_ALG`
  socket types on Linux and Android ([#1031](https://github.com/nix-rust/nix/pull/1031))
- Add killpg
  ([#1034](https://github.com/nix-rust/nix/pull/1034))
- Added ENOTSUP errno support for Linux and Android.
  ([#969](https://github.com/nix-rust/nix/pull/969))
- Add several errno constants from OpenBSD 6.2
  ([#1036](https://github.com/nix-rust/nix/pull/1036))
- Added `from_std` and `to_std` methods for `sys::socket::IpAddr`
  ([#1043](https://github.com/nix-rust/nix/pull/1043))
- Added `nix::unistd:seteuid` and `nix::unistd::setegid` for those platforms that do
  not support `setresuid` nor `setresgid` respectively.
  ([#1044](https://github.com/nix-rust/nix/pull/1044))
- Added a `access` wrapper
  ([#1045](https://github.com/nix-rust/nix/pull/1045))
- Add `forkpty`
  ([#1042](https://github.com/nix-rust/nix/pull/1042))
- Add `sched_yield`
  ([#1050](https://github.com/nix-rust/nix/pull/1050))

### Changed
- `PollFd` event flags renamed to `PollFlags` ([#1024](https://github.com/nix-rust/nix/pull/1024/))
- `recvmsg` now returns an Iterator over `ControlMessageOwned` objects rather
  than `ControlMessage` objects.  This is sadly not backwards-compatible.  Fix
  code like this:
  ```rust
  if let ControlMessage::ScmRights(&fds) = cmsg {
  ```

  By replacing it with code like this:
  ```rust
  if let ControlMessageOwned::ScmRights(fds) = cmsg {
  ```
  ([#1020](https://github.com/nix-rust/nix/pull/1020))
- Replaced `CmsgSpace` with the `cmsg_space` macro.
  ([#1020](https://github.com/nix-rust/nix/pull/1020))

### Fixed
- Fixed multiple bugs when using `sendmsg` and `recvmsg` with ancillary control messages
  ([#1020](https://github.com/nix-rust/nix/pull/1020))
- Macros exported by `nix` may now be imported via `use` on the Rust 2018
  edition without importing helper macros for BSD targets.
  ([#1041](https://github.com/nix-rust/nix/pull/1041))

  For example, in Rust 2018, the `ioctl_read_bad!` macro can now be imported
  without importing the `convert_ioctl_res!` macro.

  ```rust
  use nix::ioctl_read_bad;

  ioctl_read_bad!(tcgets, libc::TCGETS, libc::termios);
  ```

### Removed
- `Daemon`, `NOTE_REAP`, and `NOTE_EXIT_REPARENTED` are now deprecated on OSX
  and iOS.
  ([#1033](https://github.com/nix-rust/nix/pull/1033))
- `PTRACE_GETREGS`, `PTRACE_SETREGS`, `PTRACE_GETFPREGS`, and
  `PTRACE_SETFPREGS` have been removed from some platforms where they never
  should've been defined in the first place.
  ([#1055](https://github.com/nix-rust/nix/pull/1055))

## [0.13.0] - 2019-01-15
### Added
- Added PKTINFO(V4) & V6PKTINFO cmsg support - Android/FreeBSD/iOS/Linux/MacOS.
  ([#990](https://github.com/nix-rust/nix/pull/990))
- Added support of CString type in `setsockopt`.
  ([#972](https://github.com/nix-rust/nix/pull/972))
- Added option `TCP_CONGESTION` in `setsockopt`.
  ([#972](https://github.com/nix-rust/nix/pull/972))
- Added `symlinkat` wrapper.
  ([#997](https://github.com/nix-rust/nix/pull/997))
- Added `ptrace::{getregs, setregs}`.
  ([#1010](https://github.com/nix-rust/nix/pull/1010))
- Added `nix::sys::signal::signal`.
  ([#817](https://github.com/nix-rust/nix/pull/817))
- Added an `mprotect` wrapper.
  ([#991](https://github.com/nix-rust/nix/pull/991))

### Changed
### Fixed
- `lutimes` never worked on OpenBSD as it is not implemented on OpenBSD. It has
  been removed. ([#1000](https://github.com/nix-rust/nix/pull/1000))
- `fexecve` never worked on NetBSD or on OpenBSD as it is not implemented on
  either OS. It has been removed. ([#1000](https://github.com/nix-rust/nix/pull/1000))

### Removed

## [0.12.0] 2018-11-28

### Added
- Added `FromStr` and `Display` impls for `nix::sys::Signal`
  ([#884](https://github.com/nix-rust/nix/pull/884))
- Added a `sync` wrapper.
  ([#961](https://github.com/nix-rust/nix/pull/961))
- Added a `sysinfo` wrapper.
  ([#922](https://github.com/nix-rust/nix/pull/922))
- Support the `SO_PEERCRED` socket option and the `UnixCredentials` type on all Linux and Android targets.
  ([#921](https://github.com/nix-rust/nix/pull/921))
- Added support for `SCM_CREDENTIALS`, allowing to send process credentials over Unix sockets.
  ([#923](https://github.com/nix-rust/nix/pull/923))
- Added a `dir` module for reading directories (wraps `fdopendir`, `readdir`, and `rewinddir`).
  ([#916](https://github.com/nix-rust/nix/pull/916))
- Added `kmod` module that allows loading and unloading kernel modules on Linux.
  ([#930](https://github.com/nix-rust/nix/pull/930))
- Added `futimens` and `utimesat` wrappers ([#944](https://github.com/nix-rust/nix/pull/944)),
  an `lutimes` wrapper ([#967](https://github.com/nix-rust/nix/pull/967)),
  and a `utimes` wrapper ([#946](https://github.com/nix-rust/nix/pull/946)).
- Added `AF_UNSPEC` wrapper to `AddressFamily` ([#948](https://github.com/nix-rust/nix/pull/948))
- Added the `mode_t` public alias within `sys::stat`.
  ([#954](https://github.com/nix-rust/nix/pull/954))
- Added a `truncate` wrapper.
  ([#956](https://github.com/nix-rust/nix/pull/956))
- Added a `fchownat` wrapper.
  ([#955](https://github.com/nix-rust/nix/pull/955))
- Added support for `ptrace` on BSD operating systems ([#949](https://github.com/nix-rust/nix/pull/949))
- Added `ptrace` functions for reads and writes to tracee memory and ptrace kill
  ([#949](https://github.com/nix-rust/nix/pull/949)) ([#958](https://github.com/nix-rust/nix/pull/958))
- Added a `acct` wrapper module for enabling and disabling process accounting
  ([#952](https://github.com/nix-rust/nix/pull/952))
- Added the `time_t` and `suseconds_t` public aliases within `sys::time`.
  ([#968](https://github.com/nix-rust/nix/pull/968))
- Added `unistd::execvpe` for Haiku, Linux and OpenBSD
  ([#975](https://github.com/nix-rust/nix/pull/975))
- Added `Error::as_errno`.
  ([#977](https://github.com/nix-rust/nix/pull/977))

### Changed
- Increased required Rust version to 1.24.1
  ([#900](https://github.com/nix-rust/nix/pull/900))
  ([#966](https://github.com/nix-rust/nix/pull/966))

### Fixed
- Made `preadv` take immutable slice of IoVec.
  ([#914](https://github.com/nix-rust/nix/pull/914))
- Fixed passing multiple file descriptors over Unix Sockets.
  ([#918](https://github.com/nix-rust/nix/pull/918))

### Removed

## [0.11.0] 2018-06-01

### Added
- Added `sendfile` on FreeBSD and Darwin.
  ([#901](https://github.com/nix-rust/nix/pull/901))
- Added `pselect`
  ([#894](https://github.com/nix-rust/nix/pull/894))
- Exposed `preadv` and `pwritev` on the BSDs.
  ([#883](https://github.com/nix-rust/nix/pull/883))
- Added `mlockall` and `munlockall`
  ([#876](https://github.com/nix-rust/nix/pull/876))
- Added `SO_MARK` on Linux.
  ([#873](https://github.com/nix-rust/nix/pull/873))
- Added safe support for nearly any buffer type in the `sys::aio` module.
  ([#872](https://github.com/nix-rust/nix/pull/872))
- Added `sys::aio::LioCb` as a wrapper for `libc::lio_listio`.
  ([#872](https://github.com/nix-rust/nix/pull/872))
- Added `unistd::getsid`
  ([#850](https://github.com/nix-rust/nix/pull/850))
- Added `alarm`. ([#830](https://github.com/nix-rust/nix/pull/830))
- Added interface flags `IFF_NO_PI, IFF_TUN, IFF_TAP` on linux-like systems.
  ([#853](https://github.com/nix-rust/nix/pull/853))
- Added `statvfs` module to all MacOS and Linux architectures.
  ([#832](https://github.com/nix-rust/nix/pull/832))
- Added `EVFILT_EMPTY`, `EVFILT_PROCDESC`, and `EVFILT_SENDFILE` on FreeBSD.
  ([#825](https://github.com/nix-rust/nix/pull/825))
- Exposed `termios::cfmakesane` on FreeBSD.
  ([#825](https://github.com/nix-rust/nix/pull/825))
- Exposed `MSG_CMSG_CLOEXEC` on *BSD.
  ([#825](https://github.com/nix-rust/nix/pull/825))
- Added `fchmod`, `fchmodat`.
  ([#857](https://github.com/nix-rust/nix/pull/857))
- Added `request_code_write_int!` on FreeBSD/DragonFlyBSD
  ([#833](https://github.com/nix-rust/nix/pull/833))

### Changed
- `Display` and `Debug` for `SysControlAddr` now includes all fields.
  ([#837](https://github.com/nix-rust/nix/pull/837))
- `ioctl!` has been replaced with a family of `ioctl_*!` macros.
  ([#833](https://github.com/nix-rust/nix/pull/833))
- `io!`, `ior!`, `iow!`, and `iorw!` has been renamed to `request_code_none!`, `request_code_read!`,
  `request_code_write!`, and `request_code_readwrite!` respectively. These have also now been exposed
  in the documentation.
  ([#833](https://github.com/nix-rust/nix/pull/833))
- Enabled more `ptrace::Request` definitions for uncommon Linux platforms
  ([#892](https://github.com/nix-rust/nix/pull/892))
- Emulation of `FD_CLOEXEC` and `O_NONBLOCK` was removed from `socket()`, `accept4()`, and
  `socketpair()`.
  ([#907](https://github.com/nix-rust/nix/pull/907))

### Fixed
- Fixed possible panics when using `SigAction::flags` on Linux
  ([#869](https://github.com/nix-rust/nix/pull/869))
- Properly exposed 460800 and 921600 baud rates on NetBSD
  ([#837](https://github.com/nix-rust/nix/pull/837))
- Fixed `ioctl_write_int!` on FreeBSD/DragonFlyBSD
  ([#833](https://github.com/nix-rust/nix/pull/833))
- `ioctl_write_int!` now properly supports passing a `c_ulong` as the parameter on Linux non-musl targets
  ([#833](https://github.com/nix-rust/nix/pull/833))

### Removed
- Removed explicit support for the `bytes` crate from the `sys::aio` module.
  See `sys::aio::AioCb::from_boxed_slice` examples for alternatives.
  ([#872](https://github.com/nix-rust/nix/pull/872))
- Removed `sys::aio::lio_listio`.  Use `sys::aio::LioCb::listio` instead.
  ([#872](https://github.com/nix-rust/nix/pull/872))
- Removed emulated `accept4()` from macos, ios, and netbsd targets
  ([#907](https://github.com/nix-rust/nix/pull/907))
- Removed `IFF_NOTRAILERS` on OpenBSD, as it has been removed in OpenBSD 6.3
  ([#893](https://github.com/nix-rust/nix/pull/893))

## [0.10.0] 2018-01-26

### Added
- Added specialized wrapper: `sys::ptrace::step`
  ([#852](https://github.com/nix-rust/nix/pull/852))
- Added `AioCb::from_ptr` and `AioCb::from_mut_ptr`
  ([#820](https://github.com/nix-rust/nix/pull/820))
- Added specialized wrappers: `sys::ptrace::{traceme, syscall, cont, attach}`. Using the matching routines
  with `sys::ptrace::ptrace` is now deprecated.
- Added `nix::poll` module for all platforms
  ([#672](https://github.com/nix-rust/nix/pull/672))
- Added `nix::ppoll` function for FreeBSD and DragonFly
  ([#672](https://github.com/nix-rust/nix/pull/672))
- Added protocol families in `AddressFamily` enum.
  ([#647](https://github.com/nix-rust/nix/pull/647))
- Added the `pid()` method to `WaitStatus` for extracting the PID.
  ([#722](https://github.com/nix-rust/nix/pull/722))
- Added `nix::unistd:fexecve`.
  ([#727](https://github.com/nix-rust/nix/pull/727))
- Expose `uname()` on all platforms.
  ([#739](https://github.com/nix-rust/nix/pull/739))
- Expose `signalfd` module on Android as well.
  ([#739](https://github.com/nix-rust/nix/pull/739))
- Added `nix::sys::ptrace::detach`.
  ([#749](https://github.com/nix-rust/nix/pull/749))
- Added timestamp socket control message variant:
  `nix::sys::socket::ControlMessage::ScmTimestamp`
  ([#663](https://github.com/nix-rust/nix/pull/663))
- Added socket option variant that enables the timestamp socket
  control message: `nix::sys::socket::sockopt::ReceiveTimestamp`
  ([#663](https://github.com/nix-rust/nix/pull/663))
- Added more accessor methods for `AioCb`
  ([#773](https://github.com/nix-rust/nix/pull/773))
- Add `nix::sys::fallocate`
  ([#768](https:://github.com/nix-rust/nix/pull/768))
- Added `nix::unistd::mkfifo`.
  ([#602](https://github.com/nix-rust/nix/pull/774))
- Added `ptrace::Options::PTRACE_O_EXITKILL` on Linux and Android.
  ([#771](https://github.com/nix-rust/nix/pull/771))
- Added `nix::sys::uio::{process_vm_readv, process_vm_writev}` on Linux
  ([#568](https://github.com/nix-rust/nix/pull/568))
- Added `nix::unistd::{getgroups, setgroups, getgrouplist, initgroups}`. ([#733](https://github.com/nix-rust/nix/pull/733))
- Added `nix::sys::socket::UnixAddr::as_abstract` on Linux and Android.
  ([#785](https://github.com/nix-rust/nix/pull/785))
- Added `nix::unistd::execveat` on Linux and Android.
  ([#800](https://github.com/nix-rust/nix/pull/800))
- Added the `from_raw()` method to `WaitStatus` for converting raw status values
  to `WaitStatus` independent of syscalls.
  ([#741](https://github.com/nix-rust/nix/pull/741))
- Added more standard trait implementations for various types.
  ([#814](https://github.com/nix-rust/nix/pull/814))
- Added `sigprocmask` to the signal module.
  ([#826](https://github.com/nix-rust/nix/pull/826))
- Added `nix::sys::socket::LinkAddr` on Linux and all bsdlike system.
  ([#813](https://github.com/nix-rust/nix/pull/813))
- Add socket options for `IP_TRANSPARENT` / `BIND_ANY`.
  ([#835](https://github.com/nix-rust/nix/pull/835))

### Changed
- Exposed the `mqueue` module for all supported operating systems.
  ([#834](https://github.com/nix-rust/nix/pull/834))
- Use native `pipe2` on all BSD targets.  Users should notice no difference.
  ([#777](https://github.com/nix-rust/nix/pull/777))
- Renamed existing `ptrace` wrappers to encourage namespacing ([#692](https://github.com/nix-rust/nix/pull/692))
- Marked `sys::ptrace::ptrace` as `unsafe`.
- Changed function signature of `socket()` and `socketpair()`. The `protocol` argument
  has changed type from `c_int` to `SockProtocol`.
  It accepts a `None` value for default protocol that was specified with zero using `c_int`.
  ([#647](https://github.com/nix-rust/nix/pull/647))
- Made `select` easier to use, adding the ability to automatically calculate the `nfds` parameter using the new
  `FdSet::highest` ([#701](https://github.com/nix-rust/nix/pull/701))
- Exposed `unistd::setresuid` and `unistd::setresgid` on FreeBSD and OpenBSD
  ([#721](https://github.com/nix-rust/nix/pull/721))
- Refactored the `statvfs` module removing extraneous API functions and the
  `statvfs::vfs` module. Additionally  `(f)statvfs()` now return the struct
  directly. And the returned `Statvfs` struct now exposes its data through
  accessor methods. ([#729](https://github.com/nix-rust/nix/pull/729))
- The `addr` argument to `madvise` and `msync` is now `*mut` to better match the
  libc API. ([#731](https://github.com/nix-rust/nix/pull/731))
- `shm_open` and `shm_unlink` are no longer exposed on Android targets, where
  they are not officially supported. ([#731](https://github.com/nix-rust/nix/pull/731))
- `MapFlags`, `MmapAdvise`, and `MsFlags` expose some more variants and only
  officially-supported variants are provided for each target.
  ([#731](https://github.com/nix-rust/nix/pull/731))
- Marked `pty::ptsname` function as `unsafe`
  ([#744](https://github.com/nix-rust/nix/pull/744))
- Moved constants ptrace request, event and options to enums and updated ptrace functions and argument types accordingly.
  ([#749](https://github.com/nix-rust/nix/pull/749))
- `AioCb::Drop` will now panic if the `AioCb` is still in-progress ([#715](https://github.com/nix-rust/nix/pull/715))
- Restricted `nix::sys::socket::UnixAddr::new_abstract` to Linux and Android only.
  ([#785](https://github.com/nix-rust/nix/pull/785))
- The `ucred` struct has been removed in favor of a `UserCredentials` struct that
  contains only getters for its fields.
  ([#814](https://github.com/nix-rust/nix/pull/814))
- Both `ip_mreq` and `ipv6_mreq` have been replaced with `IpMembershipRequest` and
  `Ipv6MembershipRequest`.
  ([#814](https://github.com/nix-rust/nix/pull/814))
- Removed return type from `pause`.
  ([#829](https://github.com/nix-rust/nix/pull/829))
- Changed the termios APIs to allow for using a `u32` instead of the `BaudRate`
  enum on BSD platforms to support arbitrary baud rates. See the module docs for
  `nix::sys::termios` for more details.
  ([#843](https://github.com/nix-rust/nix/pull/843))

### Fixed
- Fix compilation and tests for OpenBSD targets
  ([#688](https://github.com/nix-rust/nix/pull/688))
- Fixed error handling in `AioCb::fsync`, `AioCb::read`, and `AioCb::write`.
  It is no longer an error to drop an `AioCb` that failed to enqueue in the OS.
  ([#715](https://github.com/nix-rust/nix/pull/715))
- Fix potential memory corruption on non-Linux platforms when using
  `sendmsg`/`recvmsg`, caused by mismatched `msghdr` definition.
  ([#648](https://github.com/nix-rust/nix/pull/648))

### Removed
- `AioCb::from_boxed_slice` has been removed.  It was never actually safe.  Use
  `from_bytes` or `from_bytes_mut` instead.
  ([#820](https://github.com/nix-rust/nix/pull/820))
- The syscall module has been removed. This only exposed enough functionality for
  `memfd_create()` and `pivot_root()`, which are still exposed as separate functions.
  ([#747](https://github.com/nix-rust/nix/pull/747))
- The `Errno` variants are no longer reexported from the `errno` module. `Errno` itself is no longer reexported from the
  crate root and instead must be accessed using the `errno` module. ([#696](https://github.com/nix-rust/nix/pull/696))
- Removed `MS_VERBOSE`, `MS_NOSEC`, and `MS_BORN` from `MsFlags`. These
  are internal kernel flags and should never have been exposed.
  ([#814](https://github.com/nix-rust/nix/pull/814))


## [0.9.0] 2017-07-23

### Added
- Added `sysconf`, `pathconf`, and `fpathconf`
  ([#630](https://github.com/nix-rust/nix/pull/630)
- Added `sys::signal::SigAction::{ flags, mask, handler}`
  ([#611](https://github.com/nix-rust/nix/pull/609)
- Added `nix::sys::pthread::pthread_self`
  ([#591](https://github.com/nix-rust/nix/pull/591)
- Added `AioCb::from_boxed_slice`
  ([#582](https://github.com/nix-rust/nix/pull/582)
- Added `nix::unistd::{openat, fstatat, readlink, readlinkat}`
  ([#551](https://github.com/nix-rust/nix/pull/551))
- Added `nix::pty::{grantpt, posix_openpt, ptsname/ptsname_r, unlockpt}`
  ([#556](https://github.com/nix-rust/nix/pull/556)
- Added `nix::ptr::openpty`
  ([#456](https://github.com/nix-rust/nix/pull/456))
- Added `nix::ptrace::{ptrace_get_data, ptrace_getsiginfo, ptrace_setsiginfo
  and nix::Error::UnsupportedOperation}`
  ([#614](https://github.com/nix-rust/nix/pull/614))
- Added `cfmakeraw`, `cfsetspeed`, and `tcgetsid`. ([#527](https://github.com/nix-rust/nix/pull/527))
- Added "bad none", "bad write_ptr", "bad write_int", and "bad readwrite" variants to the `ioctl!`
  macro. ([#670](https://github.com/nix-rust/nix/pull/670))
- On Linux and Android, added support for receiving `PTRACE_O_TRACESYSGOOD`
  events from `wait` and `waitpid` using `WaitStatus::PtraceSyscall`
  ([#566](https://github.com/nix-rust/nix/pull/566)).

### Changed
- The `ioctl!` macro and its variants now allow the generated functions to have
  doccomments. ([#661](https://github.com/nix-rust/nix/pull/661))
- Changed `ioctl!(write ...)` into `ioctl!(write_ptr ...)` and `ioctl!(write_int ..)` variants
  to more clearly separate those use cases. ([#670](https://github.com/nix-rust/nix/pull/670))
- Marked `sys::mman::{ mmap, munmap, madvise, munlock, msync }` as unsafe.
  ([#559](https://github.com/nix-rust/nix/pull/559))
- Minimum supported Rust version is now 1.13.
- Removed `revents` argument from `PollFd::new()` as it's an output argument and
  will be overwritten regardless of value.
  ([#542](https://github.com/nix-rust/nix/pull/542))
- Changed type signature of `sys::select::FdSet::contains` to make `self`
  immutable ([#564](https://github.com/nix-rust/nix/pull/564))
- Introduced wrapper types for `gid_t`, `pid_t`, and `uid_t` as `Gid`, `Pid`, and `Uid`
  respectively. Various functions have been changed to use these new types as
  arguments. ([#629](https://github.com/nix-rust/nix/pull/629))
- Fixed compilation on all Android and iOS targets ([#527](https://github.com/nix-rust/nix/pull/527))
  and promoted them to Tier 2 support.
- `nix::sys::statfs::{statfs,fstatfs}` uses statfs definition from `libc::statfs` instead of own linux specific type `nix::sys::Statfs`.
  Also file system type constants like `nix::sys::statfs::ADFS_SUPER_MAGIC` were removed in favor of the libc equivalent.
  ([#561](https://github.com/nix-rust/nix/pull/561))
- Revised the termios API including additional tests and documentation and exposed it on iOS. ([#527](https://github.com/nix-rust/nix/pull/527))
- `eventfd`, `signalfd`, and `pwritev`/`preadv` functionality is now included by default for all
  supported platforms. ([#681](https://github.com/nix-rust/nix/pull/561))
- The `ioctl!` macro's plain variants has been replaced with "bad read" to be consistent with
  other variants. The generated functions also have more strict types for their arguments. The
  "*_buf" variants also now calculate total array size and take slice references for improved type
  safety. The documentation has also been dramatically improved.
  ([#670](https://github.com/nix-rust/nix/pull/670))

### Removed
- Removed `io::Error` from `nix::Error` and the conversion from `nix::Error` to `Errno`
  ([#614](https://github.com/nix-rust/nix/pull/614))
- All feature flags have been removed in favor of conditional compilation on supported platforms.
  `execvpe` is no longer supported, but this was already broken and will be added back in the next
  release. ([#681](https://github.com/nix-rust/nix/pull/561))
- Removed `ioc_*` functions and many helper constants and macros within the `ioctl` module. These
  should always have been private and only the `ioctl!` should be used in public code.
  ([#670](https://github.com/nix-rust/nix/pull/670))

### Fixed
- Fixed multiple issues compiling under different archetectures and OSes.
  Now compiles on Linux/MIPS ([#538](https://github.com/nix-rust/nix/pull/538)),
  `Linux/PPC` ([#553](https://github.com/nix-rust/nix/pull/553)),
  `MacOS/x86_64,i686` ([#553](https://github.com/nix-rust/nix/pull/553)),
  `NetBSD/x64_64` ([#538](https://github.com/nix-rust/nix/pull/538)),
  `FreeBSD/x86_64,i686` ([#536](https://github.com/nix-rust/nix/pull/536)), and
  `Android` ([#631](https://github.com/nix-rust/nix/pull/631)).
- `bind` and `errno_location` now work correctly on `Android`
  ([#631](https://github.com/nix-rust/nix/pull/631))
- Added `nix::ptrace` on all Linux-kernel-based platforms
  [#624](https://github.com/nix-rust/nix/pull/624). Previously it was
  only available on x86, x86-64, and ARM, and also not on Android.
- Fixed `sys::socket::sendmsg` with zero entry `cmsgs` parameter.
  ([#623](https://github.com/nix-rust/nix/pull/623))
- Multiple constants related to the termios API have now been properly defined for
  all supported platforms. ([#527](https://github.com/nix-rust/nix/pull/527))
- `ioctl!` macro now supports working with non-int datatypes and properly supports all platforms.
  ([#670](https://github.com/nix-rust/nix/pull/670))

## [0.8.1] 2017-04-16

### Fixed
- Fixed build on FreeBSD. (Cherry-picked
  [a859ee3c](https://github.com/nix-rust/nix/commit/a859ee3c9396dfdb118fcc2c8ecc697e2d303467))

## [0.8.0] 2017-03-02

### Added
- Added `::nix::sys::termios::BaudRate` enum to provide portable baudrate
  values. ([#518](https://github.com/nix-rust/nix/pull/518))
- Added a new `WaitStatus::PtraceEvent` to support ptrace events on Linux
  and Android ([#438](https://github.com/nix-rust/nix/pull/438))
- Added support for POSIX AIO
  ([#483](https://github.com/nix-rust/nix/pull/483))
  ([#506](https://github.com/nix-rust/nix/pull/506))
- Added support for XNU system control sockets
  ([#478](https://github.com/nix-rust/nix/pull/478))
- Added support for `ioctl` calls on BSD platforms
  ([#478](https://github.com/nix-rust/nix/pull/478))
- Added struct `TimeSpec`
  ([#475](https://github.com/nix-rust/nix/pull/475))
  ([#483](https://github.com/nix-rust/nix/pull/483))
- Added complete definitions for all kqueue-related constants on all supported
  OSes
  ([#415](https://github.com/nix-rust/nix/pull/415))
- Added function `epoll_create1` and bitflags `EpollCreateFlags` in
  `::nix::sys::epoll` in order to support `::libc::epoll_create1`.
  ([#410](https://github.com/nix-rust/nix/pull/410))
- Added `setresuid` and `setresgid` for Linux in `::nix::unistd`
  ([#448](https://github.com/nix-rust/nix/pull/448))
- Added `getpgid` in `::nix::unistd`
  ([#433](https://github.com/nix-rust/nix/pull/433))
- Added `tcgetpgrp` and `tcsetpgrp` in `::nix::unistd`
  ([#451](https://github.com/nix-rust/nix/pull/451))
- Added `CLONE_NEWCGROUP` in `::nix::sched`
  ([#457](https://github.com/nix-rust/nix/pull/457))
- Added `getpgrp` in `::nix::unistd`
  ([#491](https://github.com/nix-rust/nix/pull/491))
- Added `fchdir` in `::nix::unistd`
  ([#497](https://github.com/nix-rust/nix/pull/497))
- Added `major` and `minor` in `::nix::sys::stat` for decomposing `dev_t`
  ([#508](https://github.com/nix-rust/nix/pull/508))
- Fixed the style of many bitflags and use `libc` in more places.
  ([#503](https://github.com/nix-rust/nix/pull/503))
- Added `ppoll` in `::nix::poll`
  ([#520](https://github.com/nix-rust/nix/pull/520))
- Added support for getting and setting pipe size with fcntl(2) on Linux
  ([#540](https://github.com/nix-rust/nix/pull/540))

### Changed
- `::nix::sys::termios::{cfgetispeed, cfsetispeed, cfgetospeed, cfsetospeed}`
  switched  to use `BaudRate` enum from `speed_t`.
  ([#518](https://github.com/nix-rust/nix/pull/518))
- `epoll_ctl` now could accept None as argument `event`
  when op is `EpollOp::EpollCtlDel`.
  ([#480](https://github.com/nix-rust/nix/pull/480))
- Removed the `bad` keyword from the `ioctl!` macro
  ([#478](https://github.com/nix-rust/nix/pull/478))
- Changed `TimeVal` into an opaque Newtype
  ([#475](https://github.com/nix-rust/nix/pull/475))
- `kill`'s signature, defined in `::nix::sys::signal`, changed, so that the
  signal parameter has type `T: Into<Option<Signal>>`. `None` as an argument
  for that parameter will result in a 0 passed to libc's `kill`, while a
  `Some`-argument will result in the previous behavior for the contained
  `Signal`.
  ([#445](https://github.com/nix-rust/nix/pull/445))
- The minimum supported version of rustc is now 1.7.0.
  ([#444](https://github.com/nix-rust/nix/pull/444))
- Changed `KEvent` to an opaque structure that may only be modified by its
  constructor and the `ev_set` method.
  ([#415](https://github.com/nix-rust/nix/pull/415))
  ([#442](https://github.com/nix-rust/nix/pull/442))
  ([#463](https://github.com/nix-rust/nix/pull/463))
- `pipe2` now calls `libc::pipe2` where available. Previously it was emulated
  using `pipe`, which meant that setting `O_CLOEXEC` was not atomic.
  ([#427](https://github.com/nix-rust/nix/pull/427))
- Renamed `EpollEventKind` to `EpollFlags` in `::nix::sys::epoll` in order for
  it to conform with our conventions.
  ([#410](https://github.com/nix-rust/nix/pull/410))
- `EpollEvent` in `::nix::sys::epoll` is now an opaque proxy for
  `::libc::epoll_event`. The formerly public field `events` is now be read-only
  accessible with the new method `events()` of `EpollEvent`. Instances of
  `EpollEvent` can be constructed using the new method `new()` of EpollEvent.
  ([#410](https://github.com/nix-rust/nix/pull/410))
- `SigFlags` in `::nix::sys::signal` has be renamed to `SigmaskHow` and its type
  has changed from `bitflags` to `enum` in order to conform to our conventions.
  ([#460](https://github.com/nix-rust/nix/pull/460))
- `sethostname` now takes a `&str` instead of a `&[u8]` as this provides an API
  that makes more sense in normal, correct usage of the API.
- `gethostname` previously did not expose the actual length of the hostname
  written from the underlying system call at all.  This has been updated to
  return a `&CStr` within the provided buffer that is always properly
  NUL-terminated (this is not guaranteed by the call with all platforms/libc
  implementations).
- Exposed all fcntl(2) operations at the module level, so they can be
  imported direclty instead of via `FcntlArg` enum.
  ([#541](https://github.com/nix-rust/nix/pull/541))

### Fixed
- Fixed multiple issues with Unix domain sockets on non-Linux OSes
  ([#474](https://github.com/nix-rust/nix/pull/415))
- Fixed using kqueue with `EVFILT_USER` on FreeBSD
  ([#415](https://github.com/nix-rust/nix/pull/415))
- Fixed the build on FreeBSD, and fixed the getsockopt, sendmsg, and recvmsg
  functions on that same OS.
  ([#397](https://github.com/nix-rust/nix/pull/397))
- Fixed an off-by-one bug in `UnixAddr::new_abstract` in `::nix::sys::socket`.
  ([#429](https://github.com/nix-rust/nix/pull/429))
- Fixed clone passing a potentially unaligned stack.
  ([#490](https://github.com/nix-rust/nix/pull/490))
- Fixed mkdev not creating a `dev_t` the same way as libc.
  ([#508](https://github.com/nix-rust/nix/pull/508))

## [0.7.0] 2016-09-09

### Added
- Added `lseek` and `lseek64` in `::nix::unistd`
  ([#377](https://github.com/nix-rust/nix/pull/377))
- Added `mkdir` and `getcwd` in `::nix::unistd`
  ([#416](https://github.com/nix-rust/nix/pull/416))
- Added accessors `sigmask_mut` and `sigmask` to `UContext` in
  `::nix::ucontext`.
  ([#370](https://github.com/nix-rust/nix/pull/370))
- Added `WUNTRACED` to `WaitPidFlag` in `::nix::sys::wait` for non-_linux_
  targets.
  ([#379](https://github.com/nix-rust/nix/pull/379))
- Added new module `::nix::sys::reboot` with enumeration `RebootMode` and
  functions `reboot` and `set_cad_enabled`. Currently for _linux_ only.
  ([#386](https://github.com/nix-rust/nix/pull/386))
- `FdSet` in `::nix::sys::select` now also implements `Clone`.
  ([#405](https://github.com/nix-rust/nix/pull/405))
- Added `F_FULLFSYNC` to `FcntlArg` in `::nix::fcntl` for _apple_ targets.
  ([#407](https://github.com/nix-rust/nix/pull/407))
- Added `CpuSet::unset` in `::nix::sched`.
  ([#402](https://github.com/nix-rust/nix/pull/402))
- Added constructor method `new()` to `PollFd` in `::nix::poll`, in order to
  allow creation of objects, after removing public access to members.
  ([#399](https://github.com/nix-rust/nix/pull/399))
- Added method `revents()` to `PollFd` in `::nix::poll`, in order to provide
  read access to formerly public member `revents`.
  ([#399](https://github.com/nix-rust/nix/pull/399))
- Added `MSG_CMSG_CLOEXEC` to `MsgFlags` in `::nix::sys::socket` for _linux_ only.
  ([#422](https://github.com/nix-rust/nix/pull/422))

### Changed
- Replaced the reexported integer constants for signals by the enumeration
  `Signal` in `::nix::sys::signal`.
  ([#362](https://github.com/nix-rust/nix/pull/362))
- Renamed `EventFdFlag` to `EfdFlags` in `::nix::sys::eventfd`.
  ([#383](https://github.com/nix-rust/nix/pull/383))
- Changed the result types of `CpuSet::is_set` and `CpuSet::set` in
  `::nix::sched` to `Result<bool>` and `Result<()>`, respectively. They now
  return `EINVAL`, if an invalid argument for the `field` parameter is passed.
  ([#402](https://github.com/nix-rust/nix/pull/402))
- `MqAttr` in `::nix::mqueue` is now an opaque proxy for `::libc::mq_attr`,
  which has the same structure as the old `MqAttr`. The field `mq_flags` of
  `::libc::mq_attr` is readable using the new method `flags()` of `MqAttr`.
  `MqAttr` also no longer implements `Debug`.
  ([#392](https://github.com/nix-rust/nix/pull/392))
- The parameter `msq_prio` of `mq_receive` with type `u32` in `::nix::mqueue`
  was replaced by a parameter named `msg_prio` with type `&mut u32`, so that
  the message priority can be obtained by the caller.
  ([#392](https://github.com/nix-rust/nix/pull/392))
- The type alias `MQd` in `::nix::queue` was replaced by the type alias
  `libc::mqd_t`, both of which are aliases for the same type.
  ([#392](https://github.com/nix-rust/nix/pull/392))

### Removed
- Type alias `SigNum` from `::nix::sys::signal`.
  ([#362](https://github.com/nix-rust/nix/pull/362))
- Type alias `CpuMask` from `::nix::shed`.
  ([#402](https://github.com/nix-rust/nix/pull/402))
- Removed public fields from `PollFd` in `::nix::poll`. (See also added method
  `revents()`.
  ([#399](https://github.com/nix-rust/nix/pull/399))

### Fixed
- Fixed the build problem for NetBSD (Note, that we currently do not support
  it, so it might already be broken again).
  ([#389](https://github.com/nix-rust/nix/pull/389))
- Fixed the build on FreeBSD, and fixed the getsockopt, sendmsg, and recvmsg
  functions on that same OS.
  ([#397](https://github.com/nix-rust/nix/pull/397))

## [0.6.0] 2016-06-10

### Added
- Added `gettid` in `::nix::unistd` for _linux_ and _android_.
  ([#293](https://github.com/nix-rust/nix/pull/293))
- Some _mips_ support in `::nix::sched` and `::nix::sys::syscall`.
  ([#301](https://github.com/nix-rust/nix/pull/301))
- Added `SIGNALFD_SIGINFO_SIZE` in `::nix::sys::signalfd`.
  ([#309](https://github.com/nix-rust/nix/pull/309))
- Added new module `::nix::ucontext` with struct `UContext`. Currently for
  _linux_ only.
  ([#311](https://github.com/nix-rust/nix/pull/311))
- Added `EPOLLEXCLUSIVE` to `EpollEventKind` in `::nix::sys::epoll`.
  ([#330](https://github.com/nix-rust/nix/pull/330))
- Added `pause` to `::nix::unistd`.
  ([#336](https://github.com/nix-rust/nix/pull/336))
- Added `sleep` to `::nix::unistd`.
  ([#351](https://github.com/nix-rust/nix/pull/351))
- Added `S_IFDIR`, `S_IFLNK`, `S_IFMT` to `SFlag` in `::nix::sys::stat`.
  ([#359](https://github.com/nix-rust/nix/pull/359))
- Added `clear` and `extend` functions to `SigSet`'s implementation in
  `::nix::sys::signal`.
  ([#347](https://github.com/nix-rust/nix/pull/347))
- `sockaddr_storage_to_addr` in `::nix::sys::socket` now supports `sockaddr_nl`
  on _linux_ and _android_.
  ([#366](https://github.com/nix-rust/nix/pull/366))
- Added support for `SO_ORIGINAL_DST` in `::nix::sys::socket` on _linux_.
  ([#367](https://github.com/nix-rust/nix/pull/367))
- Added `SIGINFO` in `::nix::sys::signal` for the _macos_ target as well as
  `SIGPWR` and `SIGSTKFLT` in `::nix::sys::signal` for non-_macos_ targets.
  ([#361](https://github.com/nix-rust/nix/pull/361))

### Changed
- Changed the structure `IoVec` in `::nix::sys::uio`.
  ([#304](https://github.com/nix-rust/nix/pull/304))
- Replaced `CREATE_NEW_FD` by `SIGNALFD_NEW` in `::nix::sys::signalfd`.
  ([#309](https://github.com/nix-rust/nix/pull/309))
- Renamed `SaFlag` to `SaFlags` and `SigFlag` to `SigFlags` in
  `::nix::sys::signal`.
  ([#314](https://github.com/nix-rust/nix/pull/314))
- Renamed `Fork` to `ForkResult` and changed its fields in `::nix::unistd`.
  ([#332](https://github.com/nix-rust/nix/pull/332))
- Added the `signal` parameter to `clone`'s signature in `::nix::sched`.
  ([#344](https://github.com/nix-rust/nix/pull/344))
- `execv`, `execve`, and `execvp` now return `Result<Void>` instead of
  `Result<()>` in `::nix::unistd`.
  ([#357](https://github.com/nix-rust/nix/pull/357))

### Fixed
- Improved the conversion from `std::net::SocketAddr` to `InetAddr` in
  `::nix::sys::socket::addr`.
  ([#335](https://github.com/nix-rust/nix/pull/335))

## [0.5.0] 2016-03-01
