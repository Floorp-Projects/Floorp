pub type c_char = i8;
pub type c_long = i64;
pub type c_ulong = u64;
pub type wchar_t = i32;

pub type blkcnt_t = ::c_ulong;
pub type blksize_t = ::c_long;
pub type clock_t = ::c_long;
pub type clockid_t = ::c_int;
pub type dev_t = ::c_long;
pub type fsblkcnt_t = ::c_ulong;
pub type fsfilcnt_t = ::c_ulong;
pub type ino_t = ::c_ulong;
pub type mode_t = ::c_int;
pub type nfds_t = ::c_ulong;
pub type nlink_t = ::c_ulong;
pub type off_t = ::c_long;
pub type pthread_t = *mut ::c_void;
pub type pthread_attr_t = *mut ::c_void;
pub type pthread_cond_t = *mut ::c_void;
pub type pthread_condattr_t = *mut ::c_void;
// Must be usize due to libstd/sys_common/thread_local.rs,
// should technically be *mut ::c_void
pub type pthread_key_t = usize;
pub type pthread_mutex_t = *mut ::c_void;
pub type pthread_mutexattr_t = *mut ::c_void;
pub type pthread_rwlock_t = *mut ::c_void;
pub type pthread_rwlockattr_t = *mut ::c_void;
pub type rlim_t = ::c_ulonglong;
pub type sa_family_t = u16;
pub type sem_t = *mut ::c_void;
pub type sigset_t = ::c_ulong;
pub type socklen_t = u32;
pub type speed_t = u32;
pub type suseconds_t = ::c_int;
pub type tcflag_t = u32;
pub type time_t = ::c_long;

s! {
    pub struct addrinfo {
        pub ai_flags: ::c_int,
        pub ai_family: ::c_int,
        pub ai_socktype: ::c_int,
        pub ai_protocol: ::c_int,
        pub ai_addrlen: ::size_t,
        pub ai_canonname: *mut ::c_char,
        pub ai_addr: *mut ::sockaddr,
        pub ai_next: *mut ::addrinfo,
    }

    pub struct dirent {
        pub d_ino: ::ino_t,
        pub d_off: ::off_t,
        pub d_reclen: ::c_ushort,
        pub d_type: ::c_uchar,
        pub d_name: [::c_char; 256],
    }

    pub struct Dl_info {
        pub dli_fname: *const ::c_char,
        pub dli_fbase: *mut ::c_void,
        pub dli_sname: *const ::c_char,
        pub dli_saddr: *mut ::c_void,
    }

    pub struct epoll_event {
        pub events: u32,
        pub u64: u64,
        pub _pad: u64,
    }

    pub struct fd_set {
        fds_bits: [::c_ulong; ::FD_SETSIZE / ULONG_SIZE],
    }

    pub struct in_addr {
        pub s_addr: ::in_addr_t,
    }

    pub struct ip_mreq {
        pub imr_multiaddr: ::in_addr,
        pub imr_interface: ::in_addr,
    }

    pub struct lconv {
        pub currency_symbol: *const ::c_char,
        pub decimal_point: *const ::c_char,
        pub frac_digits: ::c_char,
        pub grouping: *const ::c_char,
        pub int_curr_symbol: *const ::c_char,
        pub int_frac_digits: ::c_char,
        pub mon_decimal_point: *const ::c_char,
        pub mon_grouping: *const ::c_char,
        pub mon_thousands_sep: *const ::c_char,
        pub negative_sign: *const ::c_char,
        pub n_cs_precedes: ::c_char,
        pub n_sep_by_space: ::c_char,
        pub n_sign_posn: ::c_char,
        pub positive_sign: *const ::c_char,
        pub p_cs_precedes: ::c_char,
        pub p_sep_by_space: ::c_char,
        pub p_sign_posn: ::c_char,
        pub thousands_sep: *const ::c_char,
    }

    pub struct passwd {
        pub pw_name: *mut ::c_char,
        pub pw_passwd: *mut ::c_char,
        pub pw_uid: ::uid_t,
        pub pw_gid: ::gid_t,
        pub pw_gecos: *mut ::c_char,
        pub pw_dir: *mut ::c_char,
        pub pw_shell: *mut ::c_char,
    }

    pub struct sigaction {
        pub sa_handler: ::sighandler_t,
        pub sa_flags: ::c_ulong,
        pub sa_restorer: ::Option<extern fn()>,
        pub sa_mask: ::sigset_t,
    }

    pub struct sockaddr {
        pub sa_family: ::sa_family_t,
        pub sa_data: [::c_char; 14],
    }

    pub struct sockaddr_in {
        pub sin_family: ::sa_family_t,
        pub sin_port: ::in_port_t,
        pub sin_addr: ::in_addr,
        pub sin_zero: [::c_char; 8],
    }

    pub struct sockaddr_in6 {
        pub sin6_family: ::sa_family_t,
        pub sin6_port: ::in_port_t,
        pub sin6_flowinfo: u32,
        pub sin6_addr: ::in6_addr,
        pub sin6_scope_id: u32,
    }

    pub struct sockaddr_storage {
        pub ss_family: ::sa_family_t,
        __ss_padding: [
            u8;
            128 -
            ::core::mem::size_of::<sa_family_t>() -
            ::core::mem::size_of::<c_ulong>()
        ],
        __ss_align: ::c_ulong,
    }

    pub struct sockaddr_un {
        pub sun_family: ::sa_family_t,
        pub sun_path: [::c_char; 108]
    }

    pub struct stat {
        pub st_dev: ::dev_t,
        pub st_ino: ::ino_t,
        pub st_nlink: ::nlink_t,
        pub st_mode: ::mode_t,
        pub st_uid: ::uid_t,
        pub st_gid: ::gid_t,
        pub st_rdev: ::dev_t,
        pub st_size: ::off_t,
        pub st_blksize: ::blksize_t,
        pub st_blocks: ::blkcnt_t,
        pub st_atime: ::time_t,
        pub st_atime_nsec: ::c_long,
        pub st_mtime: ::time_t,
        pub st_mtime_nsec: ::c_long,
        pub st_ctime: ::time_t,
        pub st_ctime_nsec: ::c_long,
        _pad: [::c_char; 24],
    }

    pub struct statvfs {
        pub f_bsize: ::c_ulong,
        pub f_frsize: ::c_ulong,
        pub f_blocks: ::fsblkcnt_t,
        pub f_bfree: ::fsblkcnt_t,
        pub f_bavail: ::fsblkcnt_t,
        pub f_files: ::fsfilcnt_t,
        pub f_ffree: ::fsfilcnt_t,
        pub f_favail: ::fsfilcnt_t,
        pub f_fsid: ::c_ulong,
        pub f_flag: ::c_ulong,
        pub f_namemax: ::c_ulong,
    }

    pub struct termios {
        pub c_iflag: ::tcflag_t,
        pub c_oflag: ::tcflag_t,
        pub c_cflag: ::tcflag_t,
        pub c_lflag: ::tcflag_t,
        pub c_line: ::cc_t,
        pub c_cc: [::cc_t; ::NCCS],
        pub c_ispeed: ::speed_t,
        pub c_ospeed: ::speed_t,
    }

    pub struct tm {
        pub tm_sec: ::c_int,
        pub tm_min: ::c_int,
        pub tm_hour: ::c_int,
        pub tm_mday: ::c_int,
        pub tm_mon: ::c_int,
        pub tm_year: ::c_int,
        pub tm_wday: ::c_int,
        pub tm_yday: ::c_int,
        pub tm_isdst: ::c_int,
        pub tm_gmtoff: ::c_long,
        pub tm_zone: *const ::c_char,
    }
}

// TODO: relibc {
    pub const RTLD_DEFAULT: *mut ::c_void = 0i64 as *mut ::c_void;
// }

// dlfcn.h

pub const RTLD_LAZY: ::c_int = 0x0001;
pub const RTLD_NOW: ::c_int = 0x0002;
pub const RTLD_GLOBAL: ::c_int = 0x0100;
pub const RTLD_LOCAL: ::c_int = 0x0000;

// errno.h
pub const EPERM: ::c_int = 1;
pub const ENOENT: ::c_int = 2;
pub const ESRCH: ::c_int = 3;
pub const EINTR: ::c_int = 4;
pub const EIO: ::c_int = 5;
pub const ENXIO: ::c_int = 6;
pub const E2BIG: ::c_int = 7;
pub const ENOEXEC: ::c_int = 8;
pub const EBADF: ::c_int = 9;
pub const ECHILD: ::c_int = 10;
pub const EAGAIN: ::c_int = 11;
pub const ENOMEM: ::c_int = 12;
pub const EACCES: ::c_int = 13;
pub const EFAULT: ::c_int = 14;
pub const ENOTBLK: ::c_int = 15;
pub const EBUSY: ::c_int = 16;
pub const EEXIST: ::c_int = 17;
pub const EXDEV: ::c_int = 18;
pub const ENODEV: ::c_int = 19;
pub const ENOTDIR: ::c_int = 20;
pub const EISDIR: ::c_int = 21;
pub const EINVAL: ::c_int = 22;
pub const ENFILE: ::c_int = 23;
pub const EMFILE: ::c_int = 24;
pub const ENOTTY: ::c_int = 25;
pub const ETXTBSY: ::c_int = 26;
pub const EFBIG: ::c_int = 27;
pub const ENOSPC: ::c_int = 28;
pub const ESPIPE: ::c_int = 29;
pub const EROFS: ::c_int = 30;
pub const EMLINK: ::c_int = 31;
pub const EPIPE: ::c_int = 32;
pub const EDOM: ::c_int = 33;
pub const ERANGE: ::c_int = 34;
pub const EDEADLK: ::c_int = 35;
pub const ENOSYS: ::c_int = 38;
pub const EWOULDBLOCK: ::c_int = 41;
pub const EADDRINUSE: ::c_int = 98;
pub const EADDRNOTAVAIL: ::c_int = 99;
pub const ECONNABORTED: ::c_int = 103;
pub const ECONNRESET: ::c_int = 104;
pub const ENOTCONN: ::c_int = 107;
pub const ETIMEDOUT: ::c_int = 110;
pub const ECONNREFUSED: ::c_int = 111;
pub const EINPROGRESS: ::c_int = 115;

// fcntl.h
pub const F_DUPFD: ::c_int = 0;
pub const F_GETFD: ::c_int = 1;
pub const F_SETFD: ::c_int = 2;
pub const F_GETFL: ::c_int = 3;
pub const F_SETFL: ::c_int = 4;
// TODO: relibc {
    pub const F_DUPFD_CLOEXEC: ::c_int = ::F_DUPFD;
// }
pub const FD_CLOEXEC: ::c_int = 0x0100_0000;
pub const O_RDONLY: ::c_int = 0x0001_0000;
pub const O_WRONLY: ::c_int = 0x0002_0000;
pub const O_RDWR: ::c_int = 0x0003_0000;
pub const O_ACCMODE: ::c_int = 0x0003_0000;
pub const O_NONBLOCK: ::c_int = 0x0004_0000;
pub const O_APPEND: ::c_int = 0x0008_0000;
pub const O_SHLOCK: ::c_int = 0x0010_0000;
pub const O_EXLOCK: ::c_int = 0x0020_0000;
pub const O_ASYNC: ::c_int = 0x0040_0000;
pub const O_FSYNC: ::c_int = 0x0080_0000;
pub const O_CLOEXEC: ::c_int = 0x0100_0000;
pub const O_CREAT: ::c_int = 0x0200_0000;
pub const O_TRUNC: ::c_int = 0x0400_0000;
pub const O_EXCL: ::c_int = 0x0800_0000;
pub const O_DIRECTORY: ::c_int = 0x1000_0000;
pub const O_PATH: ::c_int = 0x2000_0000;
pub const O_SYMLINK: ::c_int = 0x4000_0000;
// Negative to allow it to be used as int
// TODO: Fix negative values missing from includes
pub const O_NOFOLLOW: ::c_int = -0x8000_0000;

// netdb.h
pub const EAI_SYSTEM: ::c_int = -11;

// netinet/in.h
// TODO: relibc {
    pub const IP_TTL: ::c_int = 2;
    pub const IPV6_UNICAST_HOPS: ::c_int = 16;
    pub const IPV6_MULTICAST_IF: ::c_int = 17;
    pub const IPV6_MULTICAST_HOPS: ::c_int = 18;
    pub const IPV6_MULTICAST_LOOP: ::c_int = 19;
    pub const IPV6_ADD_MEMBERSHIP: ::c_int = 20;
    pub const IPV6_DROP_MEMBERSHIP: ::c_int = 21;
    pub const IPV6_V6ONLY: ::c_int = 26;
    pub const IP_MULTICAST_IF: ::c_int = 32;
    pub const IP_MULTICAST_TTL: ::c_int = 33;
    pub const IP_MULTICAST_LOOP: ::c_int = 34;
    pub const IP_ADD_MEMBERSHIP: ::c_int = 35;
    pub const IP_DROP_MEMBERSHIP: ::c_int = 36;
// }

// netinet/tcp.h
pub const TCP_NODELAY: ::c_int = 1;
// TODO: relibc {
    pub const TCP_KEEPIDLE: ::c_int = 1;
// }

// poll.h
pub const POLLIN: ::c_short = 0x001;
pub const POLLPRI: ::c_short = 0x002;
pub const POLLOUT: ::c_short = 0x004;
pub const POLLERR: ::c_short = 0x008;
pub const POLLHUP: ::c_short = 0x010;
pub const POLLNVAL: ::c_short = 0x020;

// pthread.h
pub const PTHREAD_MUTEX_NORMAL: ::c_int = 0;
pub const PTHREAD_MUTEX_RECURSIVE: ::c_int = 1;
pub const PTHREAD_MUTEX_INITIALIZER: ::pthread_mutex_t = -1isize as *mut _;
pub const PTHREAD_COND_INITIALIZER: ::pthread_cond_t = -1isize as *mut _;
pub const PTHREAD_RWLOCK_INITIALIZER: ::pthread_rwlock_t = -1isize as *mut _;
pub const PTHREAD_STACK_MIN : ::size_t = 4096;

// signal.h
pub const SIG_BLOCK: ::c_int = 0;
pub const SIG_UNBLOCK: ::c_int = 1;
pub const SIG_SETMASK: ::c_int = 2;
pub const SIGHUP: ::c_int = 1;
pub const SIGINT: ::c_int = 2;
pub const SIGQUIT: ::c_int = 3;
pub const SIGILL: ::c_int = 4;
pub const SIGTRAP: ::c_int = 5;
pub const SIGABRT: ::c_int = 6;
pub const SIGBUS: ::c_int = 7;
pub const SIGFPE: ::c_int = 8;
pub const SIGKILL: ::c_int = 9;
pub const SIGUSR1: ::c_int = 10;
pub const SIGSEGV: ::c_int = 11;
pub const SIGUSR2: ::c_int = 12;
pub const SIGPIPE: ::c_int = 13;
pub const SIGALRM: ::c_int = 14;
pub const SIGTERM: ::c_int = 15;
pub const SIGSTKFLT: ::c_int = 16;
pub const SIGCHLD: ::c_int = 17;
pub const SIGCONT: ::c_int = 18;
pub const SIGSTOP: ::c_int = 19;
pub const SIGTSTP: ::c_int = 20;
pub const SIGTTIN: ::c_int = 21;
pub const SIGTTOU: ::c_int = 22;
pub const SIGURG: ::c_int = 23;
pub const SIGXCPU: ::c_int = 24;
pub const SIGXFSZ: ::c_int = 25;
pub const SIGVTALRM: ::c_int = 26;
pub const SIGPROF: ::c_int = 27;
pub const SIGWINCH: ::c_int = 28;
pub const SIGIO: ::c_int = 29;
pub const SIGPWR: ::c_int = 30;
pub const SIGSYS: ::c_int = 31;
pub const NSIG: ::c_int = 32;

// sys/epoll.h
pub const EPOLL_CLOEXEC: ::c_int = 0x0100_0000;
pub const EPOLL_CTL_ADD: ::c_int = 1;
pub const EPOLL_CTL_DEL: ::c_int = 2;
pub const EPOLL_CTL_MOD: ::c_int = 3;
pub const EPOLLIN: ::c_int = 1;
pub const EPOLLPRI: ::c_int = 0;
pub const EPOLLOUT: ::c_int = 2;
pub const EPOLLRDNORM: ::c_int = 0;
pub const EPOLLNVAL: ::c_int = 0;
pub const EPOLLRDBAND: ::c_int = 0;
pub const EPOLLWRNORM: ::c_int = 0;
pub const EPOLLWRBAND: ::c_int = 0;
pub const EPOLLMSG: ::c_int = 0;
pub const EPOLLERR: ::c_int = 0;
pub const EPOLLHUP: ::c_int = 0;
pub const EPOLLRDHUP: ::c_int = 0;
pub const EPOLLEXCLUSIVE: ::c_int = 0;
pub const EPOLLWAKEUP: ::c_int = 0;
pub const EPOLLONESHOT: ::c_int = 0;
pub const EPOLLET: ::c_int = 0;

// sys/stat.h
pub const S_IFMT: ::c_int = 0o0_170_000;
pub const S_IFDIR: ::c_int = 0o040_000;
pub const S_IFCHR: ::c_int = 0o020_000;
pub const S_IFBLK: ::c_int = 0o060_000;
pub const S_IFREG: ::c_int = 0o100_000;
pub const S_IFIFO: ::c_int = 0o010_000;
pub const S_IFLNK: ::c_int = 0o120_000;
pub const S_IFSOCK: ::c_int = 0o140_000;
pub const S_IRWXU: ::c_int = 0o0_700;
pub const S_IRUSR: ::c_int = 0o0_400;
pub const S_IWUSR: ::c_int = 0o0_200;
pub const S_IXUSR: ::c_int = 0o0_100;
pub const S_IRWXG: ::c_int = 0o0_070;
pub const S_IRGRP: ::c_int = 0o0_040;
pub const S_IWGRP: ::c_int = 0o0_020;
pub const S_IXGRP: ::c_int = 0o0_010;
pub const S_IRWXO: ::c_int = 0o0_007;
pub const S_IROTH: ::c_int = 0o0_004;
pub const S_IWOTH: ::c_int = 0o0_002;
pub const S_IXOTH: ::c_int = 0o0_001;

// stdlib.h
pub const EXIT_SUCCESS: ::c_int = 0;
pub const EXIT_FAILURE: ::c_int = 1;

// sys/ioctl.h
// TODO: relibc {
    pub const FIONBIO: ::c_ulong = 0x5421;
    pub const FIOCLEX: ::c_ulong = 0x5451;
// }
pub const TCGETS: ::c_ulong = 0x5401;
pub const TCSETS: ::c_ulong = 0x5402;
pub const TCFLSH: ::c_ulong = 0x540B;
pub const TIOCGPGRP: ::c_ulong = 0x540F;
pub const TIOCSPGRP: ::c_ulong = 0x5410;
pub const TIOCGWINSZ: ::c_ulong = 0x5413;
pub const TIOCSWINSZ: ::c_ulong = 0x5414;

// sys/select.h
pub const FD_SETSIZE: usize = 1024;

// sys/socket.h
pub const AF_UNIX: ::c_int = 1;
pub const AF_INET: ::c_int = 2;
pub const AF_INET6: ::c_int = 10;
pub const MSG_PEEK: ::c_int = 2;
pub const SHUT_RD: ::c_int = 0;
pub const SHUT_WR: ::c_int = 1;
pub const SHUT_RDWR: ::c_int = 2;
pub const SO_REUSEADDR: ::c_int = 2;
pub const SO_ERROR: ::c_int = 4;
pub const SO_BROADCAST: ::c_int = 6;
pub const SO_SNDBUF: ::c_int = 7;
pub const SO_RCVBUF: ::c_int = 8;
pub const SO_KEEPALIVE: ::c_int = 9;
pub const SO_LINGER: ::c_int = 13;
pub const SO_REUSEPORT: ::c_int = 15;
pub const SO_RCVTIMEO: ::c_int = 20;
pub const SO_SNDTIMEO: ::c_int = 21;
pub const SOCK_STREAM: ::c_int = 1;
pub const SOCK_DGRAM: ::c_int = 2;
pub const SOL_SOCKET: ::c_int = 1;

// sys/wait.h
pub const WNOHANG: ::c_int = 1;

// termios.h
pub const NCCS: usize = 32;

// time.h
pub const CLOCK_REALTIME: ::c_int = 1;
pub const CLOCK_MONOTONIC: ::c_int = 4;

// unistd.h
pub const _SC_PAGESIZE: ::c_int = 30;
pub const SEEK_SET: ::c_int = 0;
pub const SEEK_CUR: ::c_int = 1;
pub const SEEK_END: ::c_int = 2;
pub const STDIN_FILENO: ::c_int = 0;
pub const STDOUT_FILENO: ::c_int = 1;
pub const STDERR_FILENO: ::c_int = 2;

// wait.h
pub fn WIFSTOPPED(status: ::c_int) -> bool {
    (status & 0xff) == 0x7f
}

pub fn WSTOPSIG(status: ::c_int) -> ::c_int {
    (status >> 8) & 0xff
}

pub fn WIFCONTINUED(status: ::c_int) -> bool {
    status == 0xffff
}

pub fn WIFSIGNALED(status: ::c_int) -> bool {
    ((status & 0x7f) + 1) as i8 >= 2
}

pub fn WTERMSIG(status: ::c_int) -> ::c_int {
    status & 0x7f
}

pub fn WIFEXITED(status: ::c_int) -> bool {
    (status & 0x7f) == 0
}

pub fn WEXITSTATUS(status: ::c_int) -> ::c_int {
    (status >> 8) & 0xff
}

pub fn WCOREDUMP(status: ::c_int) -> bool {
    (status & 0x80) != 0
}

// intentionally not public, only used for fd_set
cfg_if! {
    if #[cfg(target_pointer_width = "32")] {
        const ULONG_SIZE: usize = 32;
    } else if #[cfg(target_pointer_width = "64")] {
        const ULONG_SIZE: usize = 64;
    } else {
        // Unknown target_pointer_width
    }
}

extern {
    pub fn getrlimit(resource: ::c_int, rlim: *mut ::rlimit) -> ::c_int;
    pub fn setrlimit(resource: ::c_int, rlim: *const ::rlimit) -> ::c_int;

    pub fn strerror_r(errnum: ::c_int, buf: *mut c_char,
                      buflen: ::size_t) -> ::c_int;
    pub fn __errno_location() -> *mut ::c_int;

    // malloc.h
    pub fn memalign(align: ::size_t, size: ::size_t) -> *mut ::c_void;

    // pthread.h
    pub fn pthread_atfork(prepare: ::Option<unsafe extern fn()>,
                          parent: ::Option<unsafe extern fn()>,
                          child: ::Option<unsafe extern fn()>) -> ::c_int;
    pub fn pthread_create(tid: *mut ::pthread_t,
                          attr: *const ::pthread_attr_t,
                          start: extern fn(*mut ::c_void) -> *mut ::c_void,
                          arg: *mut ::c_void) -> ::c_int;
    pub fn pthread_condattr_setclock(attr: *mut pthread_condattr_t,
                                     clock_id: ::clockid_t) -> ::c_int;

    // signal.h
    pub fn pthread_sigmask(how: ::c_int,
                           set: *const ::sigset_t,
                           oldset: *mut ::sigset_t) -> ::c_int;

    // sys/epoll.h
    pub fn epoll_create(size: ::c_int) -> ::c_int;
    pub fn epoll_create1(flags: ::c_int) -> ::c_int;
    pub fn epoll_wait(epfd: ::c_int,
                      events: *mut ::epoll_event,
                      maxevents: ::c_int,
                      timeout: ::c_int) -> ::c_int;
    pub fn epoll_ctl(epfd: ::c_int,
                     op: ::c_int,
                     fd: ::c_int,
                     event: *mut ::epoll_event) -> ::c_int;

    // sys/ioctl.h
    pub fn ioctl(fd: ::c_int, request: ::c_ulong, ...) -> ::c_int;

    // sys/socket.h
    pub fn bind(socket: ::c_int, address: *const ::sockaddr,
                address_len: ::socklen_t) -> ::c_int;
    pub fn recvfrom(socket: ::c_int, buf: *mut ::c_void, len: ::size_t,
                    flags: ::c_int, addr: *mut ::sockaddr,
                    addrlen: *mut ::socklen_t) -> ::ssize_t;

    // sys/uio.h
    pub fn readv(fd: ::c_int,
                 iov: *const ::iovec,
                 iovcnt: ::c_int) -> ::ssize_t;
    pub fn writev(fd: ::c_int,
                  iov: *const ::iovec,
                  iovcnt: ::c_int) -> ::ssize_t;

    // time.h
    pub fn gettimeofday(tp: *mut ::timeval,
                        tz: *mut ::timezone) -> ::c_int;
    pub fn clock_gettime(clk_id: ::clockid_t, tp: *mut ::timespec) -> ::c_int;
}
