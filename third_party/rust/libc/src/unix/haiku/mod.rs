pub type rlim_t = ::uintptr_t;
pub type sa_family_t = u8;
pub type pthread_key_t = ::c_int;
pub type nfds_t = ::c_ulong;
pub type tcflag_t = ::c_uint;
pub type speed_t = ::c_uchar;
pub type c_char = i8;
pub type clock_t = i32;
pub type clockid_t = i32;
pub type suseconds_t = i32;
pub type wchar_t = i32;
pub type off_t = i64;
pub type ino_t = i64;
pub type blkcnt_t = i64;
pub type blksize_t = i32;
pub type dev_t = i32;
pub type mode_t = u32;
pub type nlink_t = i32;
pub type useconds_t = u32;
pub type socklen_t = u32;
pub type pthread_t = ::uintptr_t;
pub type pthread_condattr_t = ::uintptr_t;
pub type pthread_mutexattr_t = ::uintptr_t;
pub type pthread_rwlockattr_t = ::uintptr_t;
pub type sigset_t = u64;
pub type fsblkcnt_t = i64;
pub type fsfilcnt_t = i64;
pub type pthread_attr_t = *mut ::c_void;
pub type nl_item = ::c_int;
pub type id_t = i32;
pub type idtype_t = ::c_uint;
pub type fd_mask = u32;

#[cfg_attr(feature = "extra_traits", derive(Debug))]
pub enum timezone {}
impl ::Copy for timezone {}
impl ::Clone for timezone {
    fn clone(&self) -> timezone {
        *self
    }
}

impl siginfo_t {
    pub unsafe fn si_addr(&self) -> *mut ::c_void {
        self.si_addr
    }

    pub unsafe fn si_pid(&self) -> ::pid_t {
        self.si_pid
    }

    pub unsafe fn si_uid(&self) -> ::uid_t {
        self.si_uid
    }

    pub unsafe fn si_status(&self) -> ::c_int {
        self.si_status
    }
}

s! {
    pub struct in_addr {
        pub s_addr: ::in_addr_t,
    }

    pub struct ip_mreq {
        pub imr_multiaddr: in_addr,
        pub imr_interface: in_addr,
    }

    pub struct sockaddr {
        pub sa_len: u8,
        pub sa_family: sa_family_t,
        pub sa_data: [u8; 30],
    }

    pub struct sockaddr_in {
        pub sin_len: u8,
        pub sin_family: sa_family_t,
        pub sin_port: ::in_port_t,
        pub sin_addr: ::in_addr,
        pub sin_zero: [i8; 24],
    }

    pub struct sockaddr_in6 {
        pub sin6_len: u8,
        pub sin6_family: u8,
        pub sin6_port: u16,
        pub sin6_flowinfo: u32,
        pub sin6_addr: ::in6_addr,
        pub sin6_scope_id: u32,
    }

    pub struct addrinfo {
        pub ai_flags: ::c_int,
        pub ai_family: ::c_int,
        pub ai_socktype: ::c_int,
        pub ai_protocol: ::c_int,
        pub ai_addrlen: socklen_t,
        pub ai_canonname: *mut c_char,
        pub ai_addr: *mut ::sockaddr,
        pub ai_next: *mut addrinfo,
    }

    pub struct fd_set {
        // size for 1024 bits, and a fd_mask with size u32
        fds_bits: [fd_mask; 32],
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
        pub tm_gmtoff: ::c_int,
        pub tm_zone: *mut ::c_char,
    }

    pub struct utsname {
        pub sysname: [::c_char; 32],
        pub nodename: [::c_char; 32],
        pub release: [::c_char; 32],
        pub version: [::c_char; 32],
        pub machine: [::c_char; 32],
    }

    pub struct lconv {
        pub decimal_point: *mut ::c_char,
        pub thousands_sep: *mut ::c_char,
        pub grouping: *mut ::c_char,
        pub int_curr_symbol: *mut ::c_char,
        pub currency_symbol: *mut ::c_char,
        pub mon_decimal_point: *mut ::c_char,
        pub mon_thousands_sep: *mut ::c_char,
        pub mon_grouping: *mut ::c_char,
        pub positive_sign: *mut ::c_char,
        pub negative_sign: *mut ::c_char,
        pub int_frac_digits: ::c_char,
        pub frac_digits: ::c_char,
        pub p_cs_precedes: ::c_char,
        pub p_sep_by_space: ::c_char,
        pub n_cs_precedes: ::c_char,
        pub n_sep_by_space: ::c_char,
        pub p_sign_posn: ::c_char,
        pub n_sign_posn: ::c_char,
        pub int_p_cs_precedes: ::c_char,
        pub int_p_sep_by_space: ::c_char,
        pub int_n_cs_precedes: ::c_char,
        pub int_n_sep_by_space: ::c_char,
        pub int_p_sign_posn: ::c_char,
        pub int_n_sign_posn: ::c_char,
    }

    pub struct msghdr {
        pub msg_name: *mut ::c_void,
        pub msg_namelen: socklen_t,
        pub msg_iov: *mut ::iovec,
        pub msg_iovlen: ::c_int,
        pub msg_control: *mut ::c_void,
        pub msg_controllen: socklen_t,
        pub msg_flags: ::c_int,
    }

    pub struct cmsghdr {
        pub cmsg_len: ::socklen_t,
        pub cmsg_level: ::c_int,
        pub cmsg_type: ::c_int,
    }

    pub struct Dl_info {
        pub dli_fname: *const ::c_char,
        pub dli_fbase: *mut ::c_void,
        pub dli_sname: *const ::c_char,
        pub dli_saddr: *mut ::c_void,
    }

    pub struct termios {
        pub c_iflag: ::tcflag_t,
        pub c_oflag: ::tcflag_t,
        pub c_cflag: ::tcflag_t,
        pub c_lflag: ::tcflag_t,
        pub c_line:  ::c_char,
        pub c_ispeed: ::speed_t,
        pub c_ospeed: ::speed_t,
        pub c_cc: [::cc_t; ::NCCS],
    }

    pub struct flock {
        pub l_type: ::c_short,
        pub l_whence: ::c_short,
        pub l_start: ::off_t,
        pub l_len: ::off_t,
        pub l_pid: ::pid_t,
    }

    pub struct stat {
        pub st_dev: dev_t,
        pub st_ino: ino_t,
        pub st_mode: mode_t,
        pub st_nlink: nlink_t,
        pub st_uid: ::uid_t,
        pub st_gid: ::gid_t,
        pub st_size: off_t,
        pub st_rdev: dev_t,
        pub st_blksize: blksize_t,
        pub st_atime: time_t,
        pub st_atime_nsec: c_long,
        pub st_mtime: time_t,
        pub st_mtime_nsec: c_long,
        pub st_ctime: time_t,
        pub st_ctime_nsec: c_long,
        pub st_crtime: time_t,
        pub st_crtime_nsec: c_long,
        pub st_type: u32,
        pub st_blocks: blkcnt_t,
    }

    pub struct glob_t {
        pub gl_pathc: ::size_t,
        __unused1: ::size_t,
        pub gl_offs: ::size_t,
        __unused2: ::size_t,
        pub gl_pathv: *mut *mut c_char,

        __unused3: *mut ::c_void,
        __unused4: *mut ::c_void,
        __unused5: *mut ::c_void,
        __unused6: *mut ::c_void,
        __unused7: *mut ::c_void,
        __unused8: *mut ::c_void,
    }

    pub struct pthread_mutex_t {
        flags: u32,
        lock: i32,
        unused: i32,
        owner: i32,
        owner_count: i32,
    }

    pub struct pthread_cond_t {
        flags: u32,
        unused: i32,
        mutex: *mut ::c_void,
        waiter_count: i32,
        lock: i32,
    }

    pub struct pthread_rwlock_t {
        flags: u32,
        owner: i32,
        lock_sem: i32,      // this is actually a union
        lock_count: i32,
        reader_count: i32,
        writer_count: i32,
        waiters: [*mut ::c_void; 2],
    }

    pub struct passwd {
        pub pw_name: *mut ::c_char,
        pub pw_passwd: *mut ::c_char,
        pub pw_uid: ::uid_t,
        pub pw_gid: ::gid_t,
        pub pw_dir: *mut ::c_char,
        pub pw_shell: *mut ::c_char,
        pub pw_gecos: *mut ::c_char,
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

    pub struct stack_t {
        pub ss_sp: *mut ::c_void,
        pub ss_size: ::size_t,
        pub ss_flags: ::c_int,
    }

    pub struct siginfo_t {
        pub si_signo: ::c_int,
        pub si_code: ::c_int,
        pub si_errno: ::c_int,
        pub si_pid: ::pid_t,
        pub si_uid: ::uid_t,
        pub si_addr: *mut ::c_void,
        pub si_status: ::c_int,
        pub si_band: c_long,
        pub sigval: *mut ::c_void,
    }

    pub struct sigaction {
        pub sa_sigaction: ::sighandler_t, //actually a union with sa_handler
        pub sa_mask: ::sigset_t,
        pub sa_flags: ::c_int,
        sa_userdata: *mut ::c_void,
    }

    pub struct sem_t {
        pub type_: i32,
        pub named_sem_id: i32, // actually a union with unnamed_sem (i32)
        pub padding: [i32; 2],
    }
}

s_no_extra_traits! {
    pub struct sockaddr_un {
        pub sun_len: u8,
        pub sun_family: sa_family_t,
        pub sun_path: [::c_char; 126]
    }
    pub struct sockaddr_storage {
        pub ss_len: u8,
        pub ss_family: sa_family_t,
        __ss_pad1: [u8; 6],
        __ss_pad2: u64,
        __ss_pad3: [u8; 112],
    }
    pub struct dirent {
        pub d_dev: dev_t,
        pub d_pdev: dev_t,
        pub d_ino: ino_t,
        pub d_pino: i64,
        pub d_reclen: ::c_ushort,
        pub d_name: [::c_char; 1024], // Max length is _POSIX_PATH_MAX
    }

    pub struct sigevent {
        pub sigev_notify: ::c_int,
        pub sigev_signo: ::c_int,
        pub sigev_value: ::sigval,
        __unused1: *mut ::c_void, // actually a function pointer
        pub sigev_notify_attributes: *mut ::pthread_attr_t,
    }
}

cfg_if! {
    if #[cfg(feature = "extra_traits")] {
        impl PartialEq for sockaddr_un {
            fn eq(&self, other: &sockaddr_un) -> bool {
                self.sun_len == other.sun_len
                    && self.sun_family == other.sun_family
                    && self
                    .sun_path
                    .iter()
                    .zip(other.sun_path.iter())
                    .all(|(a,b)| a == b)
            }
        }
        impl Eq for sockaddr_un {}
        impl ::fmt::Debug for sockaddr_un {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("sockaddr_un")
                    .field("sun_len", &self.sun_len)
                    .field("sun_family", &self.sun_family)
                    // FIXME: .field("sun_path", &self.sun_path)
                    .finish()
            }
        }
        impl ::hash::Hash for sockaddr_un {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.sun_len.hash(state);
                self.sun_family.hash(state);
                self.sun_path.hash(state);
            }
        }

        impl PartialEq for sockaddr_storage {
            fn eq(&self, other: &sockaddr_storage) -> bool {
                self.ss_len == other.ss_len
                    && self.ss_family == other.ss_family
                    && self
                    .__ss_pad1
                    .iter()
                    .zip(other.__ss_pad1.iter())
                    .all(|(a, b)| a == b)
                    && self.__ss_pad2 == other.__ss_pad2
                    && self
                    .__ss_pad3
                    .iter()
                    .zip(other.__ss_pad3.iter())
                    .all(|(a, b)| a == b)
            }
        }
        impl Eq for sockaddr_storage {}
        impl ::fmt::Debug for sockaddr_storage {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("sockaddr_storage")
                    .field("ss_len", &self.ss_len)
                    .field("ss_family", &self.ss_family)
                    .field("__ss_pad1", &self.__ss_pad1)
                    .field("__ss_pad2", &self.__ss_pad2)
                    // FIXME: .field("__ss_pad3", &self.__ss_pad3)
                    .finish()
            }
        }
        impl ::hash::Hash for sockaddr_storage {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.ss_len.hash(state);
                self.ss_family.hash(state);
                self.__ss_pad1.hash(state);
                self.__ss_pad2.hash(state);
                self.__ss_pad3.hash(state);
            }
        }

        impl PartialEq for dirent {
            fn eq(&self, other: &dirent) -> bool {
                self.d_dev == other.d_dev
                    && self.d_pdev == other.d_pdev
                    && self.d_ino == other.d_ino
                    && self.d_pino == other.d_pino
                    && self.d_reclen == other.d_reclen
                    && self
                    .d_name
                    .iter()
                    .zip(other.d_name.iter())
                    .all(|(a,b)| a == b)
            }
        }
        impl Eq for dirent {}
        impl ::fmt::Debug for dirent {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("dirent")
                    .field("d_dev", &self.d_dev)
                    .field("d_pdev", &self.d_pdev)
                    .field("d_ino", &self.d_ino)
                    .field("d_pino", &self.d_pino)
                    .field("d_reclen", &self.d_reclen)
                    // FIXME: .field("d_name", &self.d_name)
                    .finish()
            }
        }
        impl ::hash::Hash for dirent {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.d_dev.hash(state);
                self.d_pdev.hash(state);
                self.d_ino.hash(state);
                self.d_pino.hash(state);
                self.d_reclen.hash(state);
                self.d_name.hash(state);
            }
        }

        impl PartialEq for sigevent {
            fn eq(&self, other: &sigevent) -> bool {
                self.sigev_notify == other.sigev_notify
                    && self.sigev_signo == other.sigev_signo
                    && self.sigev_value == other.sigev_value
                    && self.sigev_notify_attributes
                        == other.sigev_notify_attributes
            }
        }
        impl Eq for sigevent {}
        impl ::fmt::Debug for sigevent {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("sigevent")
                    .field("sigev_notify", &self.sigev_notify)
                    .field("sigev_signo", &self.sigev_signo)
                    .field("sigev_value", &self.sigev_value)
                    .field("sigev_notify_attributes",
                           &self.sigev_notify_attributes)
                    .finish()
            }
        }
        impl ::hash::Hash for sigevent {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.sigev_notify.hash(state);
                self.sigev_signo.hash(state);
                self.sigev_value.hash(state);
                self.sigev_notify_attributes.hash(state);
            }
        }
    }
}

pub const EXIT_FAILURE: ::c_int = 1;
pub const EXIT_SUCCESS: ::c_int = 0;
pub const RAND_MAX: ::c_int = 2147483647;
pub const EOF: ::c_int = -1;
pub const SEEK_SET: ::c_int = 0;
pub const SEEK_CUR: ::c_int = 1;
pub const SEEK_END: ::c_int = 2;
pub const _IOFBF: ::c_int = 0;
pub const _IONBF: ::c_int = 2;
pub const _IOLBF: ::c_int = 1;

pub const F_DUPFD: ::c_int = 0x0001;
pub const F_GETFD: ::c_int = 0x0002;
pub const F_SETFD: ::c_int = 0x0004;
pub const F_GETFL: ::c_int = 0x0008;
pub const F_SETFL: ::c_int = 0x0010;
pub const F_GETLK: ::c_int = 0x0020;
pub const F_SETLK: ::c_int = 0x0080;
pub const F_SETLKW: ::c_int = 0x0100;
pub const F_DUPFD_CLOEXEC: ::c_int = 0x0200;

pub const F_RDLCK: ::c_int = 0x0040;
pub const F_UNLCK: ::c_int = 0x0200;
pub const F_WRLCK: ::c_int = 0x0400;

pub const AT_FDCWD: ::c_int = -1;
pub const AT_SYMLINK_NOFOLLOW: ::c_int = 0x01;
pub const AT_SYMLINK_FOLLOW: ::c_int = 0x02;
pub const AT_REMOVEDIR: ::c_int = 0x04;
pub const AT_EACCESS: ::c_int = 0x08;

pub const POLLIN: ::c_short = 0x0001;
pub const POLLOUT: ::c_short = 0x0002;
pub const POLLRDNORM: ::c_short = POLLIN;
pub const POLLWRNORM: ::c_short = POLLOUT;
pub const POLLRDBAND: ::c_short = 0x0008;
pub const POLLWRBAND: ::c_short = 0x0010;
pub const POLLPRI: ::c_short = 0x0020;
pub const POLLERR: ::c_short = 0x0004;
pub const POLLHUP: ::c_short = 0x0080;
pub const POLLNVAL: ::c_short = 0x1000;

pub const PTHREAD_CREATE_JOINABLE: ::c_int = 0;
pub const PTHREAD_CREATE_DETACHED: ::c_int = 1;

pub const CLOCK_REALTIME: ::c_int = -1;
pub const CLOCK_MONOTONIC: ::c_int = 0;

pub const RLIMIT_CORE: ::c_int = 0;
pub const RLIMIT_CPU: ::c_int = 1;
pub const RLIMIT_DATA: ::c_int = 2;
pub const RLIMIT_FSIZE: ::c_int = 3;
pub const RLIMIT_NOFILE: ::c_int = 4;
pub const RLIMIT_STACK: ::c_int = 5;
pub const RLIMIT_AS: ::c_int = 6;
// Haiku specific
pub const RLIMIT_NOVMON: ::c_int = 7;
pub const RLIM_NLIMITS: ::c_int = 8;

pub const RUSAGE_SELF: ::c_int = 0;

pub const RTLD_LAZY: ::c_int = 0;

pub const NCCS: usize = 11;

pub const O_RDONLY: ::c_int = 0x0000;
pub const O_WRONLY: ::c_int = 0x0001;
pub const O_RDWR: ::c_int = 0x0002;
pub const O_ACCMODE: ::c_int = 0x0003;

pub const O_EXCL: ::c_int = 0x0100;
pub const O_CREAT: ::c_int = 0x0200;
pub const O_TRUNC: ::c_int = 0x0400;
pub const O_NOCTTY: ::c_int = 0x1000;
pub const O_NOTRAVERSE: ::c_int = 0x2000;

pub const O_CLOEXEC: ::c_int = 0x00000040;
pub const O_NONBLOCK: ::c_int = 0x00000080;
pub const O_APPEND: ::c_int = 0x00000800;
pub const O_SYNC: ::c_int = 0x00010000;
pub const O_RSYNC: ::c_int = 0x00020000;
pub const O_DSYNC: ::c_int = 0x00040000;
pub const O_NOFOLLOW: ::c_int = 0x00080000;
pub const O_NOCACHE: ::c_int = 0x00100000;
pub const O_DIRECTORY: ::c_int = 0x00200000;

pub const S_IFIFO: ::mode_t = 4096;
pub const S_IFCHR: ::mode_t = 8192;
pub const S_IFBLK: ::mode_t = 24576;
pub const S_IFDIR: ::mode_t = 16384;
pub const S_IFREG: ::mode_t = 32768;
pub const S_IFLNK: ::mode_t = 40960;
pub const S_IFSOCK: ::mode_t = 49152;
pub const S_IFMT: ::mode_t = 61440;

pub const S_IRWXU: ::mode_t = 0o00700;
pub const S_IRUSR: ::mode_t = 0o00400;
pub const S_IWUSR: ::mode_t = 0o00200;
pub const S_IXUSR: ::mode_t = 0o00100;
pub const S_IRWXG: ::mode_t = 0o00070;
pub const S_IRGRP: ::mode_t = 0o00040;
pub const S_IWGRP: ::mode_t = 0o00020;
pub const S_IXGRP: ::mode_t = 0o00010;
pub const S_IRWXO: ::mode_t = 0o00007;
pub const S_IROTH: ::mode_t = 0o00004;
pub const S_IWOTH: ::mode_t = 0o00002;
pub const S_IXOTH: ::mode_t = 0o00001;

pub const F_OK: ::c_int = 0;
pub const R_OK: ::c_int = 4;
pub const W_OK: ::c_int = 2;
pub const X_OK: ::c_int = 1;
pub const STDIN_FILENO: ::c_int = 0;
pub const STDOUT_FILENO: ::c_int = 1;
pub const STDERR_FILENO: ::c_int = 2;

pub const SIGHUP: ::c_int = 1;
pub const SIGINT: ::c_int = 2;
pub const SIGQUIT: ::c_int = 3;
pub const SIGILL: ::c_int = 4;
pub const SIGCHLD: ::c_int = 5;
pub const SIGABRT: ::c_int = 6;
pub const SIGPIPE: ::c_int = 7;
pub const SIGFPE: ::c_int = 8;
pub const SIGKILL: ::c_int = 9;
pub const SIGSTOP: ::c_int = 10;
pub const SIGSEGV: ::c_int = 11;
pub const SIGCONT: ::c_int = 12;
pub const SIGTSTP: ::c_int = 13;
pub const SIGALRM: ::c_int = 14;
pub const SIGTERM: ::c_int = 15;
pub const SIGTTIN: ::c_int = 16;
pub const SIGTTOU: ::c_int = 17;
pub const SIGUSR1: ::c_int = 18;
pub const SIGUSR2: ::c_int = 19;
pub const SIGWINCH: ::c_int = 20;
pub const SIGKILLTHR: ::c_int = 21;
pub const SIGTRAP: ::c_int = 22;
pub const SIGPOLL: ::c_int = 23;
pub const SIGPROF: ::c_int = 24;
pub const SIGSYS: ::c_int = 25;
pub const SIGURG: ::c_int = 26;
pub const SIGVTALRM: ::c_int = 27;
pub const SIGXCPU: ::c_int = 28;
pub const SIGXFSZ: ::c_int = 29;
pub const SIGBUS: ::c_int = 30;

pub const SIG_BLOCK: ::c_int = 1;
pub const SIG_UNBLOCK: ::c_int = 2;
pub const SIG_SETMASK: ::c_int = 3;

pub const SIGEV_NONE: ::c_int = 0;
pub const SIGEV_SIGNAL: ::c_int = 1;
pub const SIGEV_THREAD: ::c_int = 2;

pub const EAI_AGAIN: ::c_int = 2;
pub const EAI_BADFLAGS: ::c_int = 3;
pub const EAI_FAIL: ::c_int = 4;
pub const EAI_FAMILY: ::c_int = 5;
pub const EAI_MEMORY: ::c_int = 6;
pub const EAI_NODATA: ::c_int = 7;
pub const EAI_NONAME: ::c_int = 8;
pub const EAI_SERVICE: ::c_int = 9;
pub const EAI_SOCKTYPE: ::c_int = 10;
pub const EAI_SYSTEM: ::c_int = 11;
pub const EAI_OVERFLOW: ::c_int = 14;

pub const PROT_NONE: ::c_int = 0;
pub const PROT_READ: ::c_int = 1;
pub const PROT_WRITE: ::c_int = 2;
pub const PROT_EXEC: ::c_int = 4;

pub const LC_ALL: ::c_int = 0;
pub const LC_COLLATE: ::c_int = 1;
pub const LC_CTYPE: ::c_int = 2;
pub const LC_MONETARY: ::c_int = 3;
pub const LC_NUMERIC: ::c_int = 4;
pub const LC_TIME: ::c_int = 5;
pub const LC_MESSAGES: ::c_int = 6;

// FIXME: Haiku does not have MAP_FILE, but libstd/os.rs requires it
pub const MAP_FILE: ::c_int = 0x00;
pub const MAP_SHARED: ::c_int = 0x01;
pub const MAP_PRIVATE: ::c_int = 0x02;
pub const MAP_FIXED: ::c_int = 0x04;
pub const MAP_ANONYMOUS: ::c_int = 0x08;
pub const MAP_ANON: ::c_int = MAP_ANONYMOUS;

pub const MAP_FAILED: *mut ::c_void = !0 as *mut ::c_void;

pub const MS_ASYNC: ::c_int = 0x01;
pub const MS_INVALIDATE: ::c_int = 0x04;
pub const MS_SYNC: ::c_int = 0x02;

pub const E2BIG: ::c_int = -2147454975;
pub const ECHILD: ::c_int = -2147454974;
pub const EDEADLK: ::c_int = -2147454973;
pub const EFBIG: ::c_int = -2147454972;
pub const EMLINK: ::c_int = -2147454971;
pub const ENFILE: ::c_int = -2147454970;
pub const ENODEV: ::c_int = -2147454969;
pub const ENOLCK: ::c_int = -2147454968;
pub const ENOSYS: ::c_int = -2147454967;
pub const ENOTTY: ::c_int = -2147454966;
pub const ENXIO: ::c_int = -2147454965;
pub const ESPIPE: ::c_int = -2147454964;
pub const ESRCH: ::c_int = -2147454963;
pub const EFPOS: ::c_int = -2147454962;
pub const ESIGPARM: ::c_int = -2147454961;
pub const EDOM: ::c_int = -2147454960;
pub const ERANGE: ::c_int = -2147454959;
pub const EPROTOTYPE: ::c_int = -2147454958;
pub const EPROTONOSUPPORT: ::c_int = -2147454957;
pub const EPFNOSUPPORT: ::c_int = -2147454956;
pub const EAFNOSUPPORT: ::c_int = -2147454955;
pub const EADDRINUSE: ::c_int = -2147454954;
pub const EADDRNOTAVAIL: ::c_int = -2147454953;
pub const ENETDOWN: ::c_int = -2147454952;
pub const ENETUNREACH: ::c_int = -2147454951;
pub const ENETRESET: ::c_int = -2147454950;
pub const ECONNABORTED: ::c_int = -2147454949;
pub const ECONNRESET: ::c_int = -2147454948;
pub const EISCONN: ::c_int = -2147454947;
pub const ENOTCONN: ::c_int = -2147454946;
pub const ESHUTDOWN: ::c_int = -2147454945;
pub const ECONNREFUSED: ::c_int = -2147454944;
pub const EHOSTUNREACH: ::c_int = -2147454943;
pub const ENOPROTOOPT: ::c_int = -2147454942;
pub const ENOBUFS: ::c_int = -2147454941;
pub const EINPROGRESS: ::c_int = -2147454940;
pub const EALREADY: ::c_int = -2147454939;
pub const EILSEQ: ::c_int = -2147454938;
pub const ENOMSG: ::c_int = -2147454937;
pub const ESTALE: ::c_int = -2147454936;
pub const EOVERFLOW: ::c_int = -2147454935;
pub const EMSGSIZE: ::c_int = -2147454934;
pub const EOPNOTSUPP: ::c_int = -2147454933;
pub const ENOTSOCK: ::c_int = -2147454932;
pub const EHOSTDOWN: ::c_int = -2147454931;
pub const EBADMSG: ::c_int = -2147454930;
pub const ECANCELED: ::c_int = -2147454929;
pub const EDESTADDRREQ: ::c_int = -2147454928;
pub const EDQUOT: ::c_int = -2147454927;
pub const EIDRM: ::c_int = -2147454926;
pub const EMULTIHOP: ::c_int = -2147454925;
pub const ENODATA: ::c_int = -2147454924;
pub const ENOLINK: ::c_int = -2147454923;
pub const ENOSR: ::c_int = -2147454922;
pub const ENOSTR: ::c_int = -2147454921;
pub const ENOTSUP: ::c_int = -2147454920;
pub const EPROTO: ::c_int = -2147454919;
pub const ETIME: ::c_int = -2147454918;
pub const ETXTBSY: ::c_int = -2147454917;
pub const ENOATTR: ::c_int = -2147454916;

// INT_MIN
pub const ENOMEM: ::c_int = -2147483648;

// POSIX errors that can be mapped to BeOS error codes
pub const EACCES: ::c_int = -2147483646;
pub const EINTR: ::c_int = -2147483638;
pub const EIO: ::c_int = -2147483647;
pub const EBUSY: ::c_int = -2147483634;
pub const EFAULT: ::c_int = -2147478783;
pub const ETIMEDOUT: ::c_int = -2147483639;
pub const EAGAIN: ::c_int = -2147483637;
pub const EWOULDBLOCK: ::c_int = -2147483637;
pub const EBADF: ::c_int = -2147459072;
pub const EEXIST: ::c_int = -2147459070;
pub const EINVAL: ::c_int = -2147483643;
pub const ENAMETOOLONG: ::c_int = -2147459068;
pub const ENOENT: ::c_int = -2147459069;
pub const EPERM: ::c_int = -2147483633;
pub const ENOTDIR: ::c_int = -2147459067;
pub const EISDIR: ::c_int = -2147459063;
pub const ENOTEMPTY: ::c_int = -2147459066;
pub const ENOSPC: ::c_int = -2147459065;
pub const EROFS: ::c_int = -2147459064;
pub const EMFILE: ::c_int = -2147459062;
pub const EXDEV: ::c_int = -2147459061;
pub const ELOOP: ::c_int = -2147459060;
pub const ENOEXEC: ::c_int = -2147478782;
pub const EPIPE: ::c_int = -2147459059;

pub const IPPROTO_RAW: ::c_int = 255;

// These are prefixed with POSIX_ on Haiku
pub const MADV_NORMAL: ::c_int = 1;
pub const MADV_SEQUENTIAL: ::c_int = 2;
pub const MADV_RANDOM: ::c_int = 3;
pub const MADV_WILLNEED: ::c_int = 4;
pub const MADV_DONTNEED: ::c_int = 5;

// https://github.com/haiku/haiku/blob/master/headers/posix/net/if.h#L80
pub const IFF_UP: ::c_int = 0x0001;
pub const IFF_BROADCAST: ::c_int = 0x0002; // valid broadcast address
pub const IFF_LOOPBACK: ::c_int = 0x0008;
pub const IFF_POINTOPOINT: ::c_int = 0x0010; // point-to-point link
pub const IFF_NOARP: ::c_int = 0x0040; // no address resolution
pub const IFF_AUTOUP: ::c_int = 0x0080; // auto dial
pub const IFF_PROMISC: ::c_int = 0x0100; // receive all packets
pub const IFF_ALLMULTI: ::c_int = 0x0200; // receive all multicast packets
pub const IFF_SIMPLEX: ::c_int = 0x0800; // doesn't receive own transmissions
pub const IFF_LINK: ::c_int = 0x1000; // has link
pub const IFF_AUTO_CONFIGURED: ::c_int = 0x2000;
pub const IFF_CONFIGURING: ::c_int = 0x4000;
pub const IFF_MULTICAST: ::c_int = 0x8000; // supports multicast

pub const AF_UNSPEC: ::c_int = 0;
pub const AF_INET: ::c_int = 1;
pub const AF_APPLETALK: ::c_int = 2;
pub const AF_ROUTE: ::c_int = 3;
pub const AF_LINK: ::c_int = 4;
pub const AF_INET6: ::c_int = 5;
pub const AF_DLI: ::c_int = 6;
pub const AF_IPX: ::c_int = 7;
pub const AF_NOTIFY: ::c_int = 8;
pub const AF_LOCAL: ::c_int = 9;
pub const AF_UNIX: ::c_int = AF_LOCAL;
pub const AF_BLUETOOTH: ::c_int = 10;

pub const IP_OPTIONS: ::c_int = 1;
pub const IP_HDRINCL: ::c_int = 2;
pub const IP_TOS: ::c_int = 3;
pub const IP_TTL: ::c_int = 4;
pub const IP_RECVOPTS: ::c_int = 5;
pub const IP_RECVRETOPTS: ::c_int = 6;
pub const IP_RECVDSTADDR: ::c_int = 7;
pub const IP_RETOPTS: ::c_int = 8;
pub const IP_MULTICAST_IF: ::c_int = 9;
pub const IP_MULTICAST_TTL: ::c_int = 10;
pub const IP_MULTICAST_LOOP: ::c_int = 11;
pub const IP_ADD_MEMBERSHIP: ::c_int = 12;
pub const IP_DROP_MEMBERSHIP: ::c_int = 13;
pub const IP_BLOCK_SOURCE: ::c_int = 14;
pub const IP_UNBLOCK_SOURCE: ::c_int = 15;
pub const IP_ADD_SOURCE_MEMBERSHIP: ::c_int = 16;
pub const IP_DROP_SOURCE_MEMBERSHIP: ::c_int = 17;

pub const TCP_NODELAY: ::c_int = 0x01;
pub const TCP_MAXSEG: ::c_int = 0x02;
pub const TCP_NOPUSH: ::c_int = 0x04;
pub const TCP_NOOPT: ::c_int = 0x08;

pub const IF_NAMESIZE: ::size_t = 32;
pub const IFNAMSIZ: ::size_t = IF_NAMESIZE;

pub const IPV6_MULTICAST_IF: ::c_int = 24;
pub const IPV6_MULTICAST_HOPS: ::c_int = 25;
pub const IPV6_MULTICAST_LOOP: ::c_int = 26;
pub const IPV6_UNICAST_HOPS: ::c_int = 27;
pub const IPV6_JOIN_GROUP: ::c_int = 28;
pub const IPV6_LEAVE_GROUP: ::c_int = 29;
pub const IPV6_V6ONLY: ::c_int = 30;
pub const IPV6_PKTINFO: ::c_int = 31;
pub const IPV6_RECVPKTINFO: ::c_int = 32;
pub const IPV6_HOPLIMIT: ::c_int = 33;
pub const IPV6_RECVHOPLIMIT: ::c_int = 34;
pub const IPV6_HOPOPTS: ::c_int = 35;
pub const IPV6_DSTOPTS: ::c_int = 36;
pub const IPV6_RTHDR: ::c_int = 37;

pub const MSG_OOB: ::c_int = 0x0001;
pub const MSG_PEEK: ::c_int = 0x0002;
pub const MSG_DONTROUTE: ::c_int = 0x0004;
pub const MSG_EOR: ::c_int = 0x0008;
pub const MSG_TRUNC: ::c_int = 0x0010;
pub const MSG_CTRUNC: ::c_int = 0x0020;
pub const MSG_WAITALL: ::c_int = 0x0040;
pub const MSG_DONTWAIT: ::c_int = 0x0080;
pub const MSG_BCAST: ::c_int = 0x0100;
pub const MSG_MCAST: ::c_int = 0x0200;
pub const MSG_EOF: ::c_int = 0x0400;
pub const MSG_NOSIGNAL: ::c_int = 0x0800;

pub const SHUT_RD: ::c_int = 0;
pub const SHUT_WR: ::c_int = 1;
pub const SHUT_RDWR: ::c_int = 2;

pub const LOCK_SH: ::c_int = 0x01;
pub const LOCK_EX: ::c_int = 0x02;
pub const LOCK_NB: ::c_int = 0x04;
pub const LOCK_UN: ::c_int = 0x08;

pub const SIGSTKSZ: ::size_t = 16384;

pub const IOV_MAX: ::c_int = 1024;
pub const PATH_MAX: ::c_int = 1024;

pub const SA_NOCLDSTOP: ::c_int = 0x01;
pub const SA_NOCLDWAIT: ::c_int = 0x02;
pub const SA_RESETHAND: ::c_int = 0x04;
pub const SA_NODEFER: ::c_int = 0x08;
pub const SA_RESTART: ::c_int = 0x10;
pub const SA_ONSTACK: ::c_int = 0x20;
pub const SA_SIGINFO: ::c_int = 0x40;
pub const SA_NOMASK: ::c_int = SA_NODEFER;
pub const SA_STACK: ::c_int = SA_ONSTACK;
pub const SA_ONESHOT: ::c_int = SA_RESETHAND;

pub const FD_SETSIZE: usize = 1024;

pub const RTLD_LOCAL: ::c_int = 0x0;
pub const RTLD_NOW: ::c_int = 0x1;
pub const RTLD_GLOBAL: ::c_int = 0x2;
pub const RTLD_DEFAULT: *mut ::c_void = 0isize as *mut ::c_void;

pub const BUFSIZ: ::c_uint = 8192;
pub const FILENAME_MAX: ::c_uint = 256;
pub const FOPEN_MAX: ::c_uint = 128;
pub const L_tmpnam: ::c_uint = 512;
pub const TMP_MAX: ::c_uint = 32768;

pub const _PC_CHOWN_RESTRICTED: ::c_int = 1;
pub const _PC_MAX_CANON: ::c_int = 2;
pub const _PC_MAX_INPUT: ::c_int = 3;
pub const _PC_NAME_MAX: ::c_int = 4;
pub const _PC_NO_TRUNC: ::c_int = 5;
pub const _PC_PATH_MAX: ::c_int = 6;
pub const _PC_PIPE_BUF: ::c_int = 7;
pub const _PC_VDISABLE: ::c_int = 8;
pub const _PC_LINK_MAX: ::c_int = 25;
pub const _PC_SYNC_IO: ::c_int = 26;
pub const _PC_ASYNC_IO: ::c_int = 27;
pub const _PC_PRIO_IO: ::c_int = 28;
pub const _PC_SOCK_MAXBUF: ::c_int = 29;
pub const _PC_FILESIZEBITS: ::c_int = 30;
pub const _PC_REC_INCR_XFER_SIZE: ::c_int = 31;
pub const _PC_REC_MAX_XFER_SIZE: ::c_int = 32;
pub const _PC_REC_MIN_XFER_SIZE: ::c_int = 33;
pub const _PC_REC_XFER_ALIGN: ::c_int = 34;
pub const _PC_ALLOC_SIZE_MIN: ::c_int = 35;
pub const _PC_SYMLINK_MAX: ::c_int = 36;
pub const _PC_2_SYMLINKS: ::c_int = 37;
pub const _PC_XATTR_EXISTS: ::c_int = 38;
pub const _PC_XATTR_ENABLED: ::c_int = 39;

pub const FIONBIO: ::c_ulong = 0xbe000000;
pub const FIONREAD: ::c_ulong = 0xbe000001;
pub const FIOSEEKDATA: ::c_ulong = 0xbe000002;
pub const FIOSEEKHOLE: ::c_ulong = 0xbe000003;

pub const _SC_ARG_MAX: ::c_int = 15;
pub const _SC_CHILD_MAX: ::c_int = 16;
pub const _SC_CLK_TCK: ::c_int = 17;
pub const _SC_JOB_CONTROL: ::c_int = 18;
pub const _SC_NGROUPS_MAX: ::c_int = 19;
pub const _SC_OPEN_MAX: ::c_int = 20;
pub const _SC_SAVED_IDS: ::c_int = 21;
pub const _SC_STREAM_MAX: ::c_int = 22;
pub const _SC_TZNAME_MAX: ::c_int = 23;
pub const _SC_VERSION: ::c_int = 24;
pub const _SC_GETGR_R_SIZE_MAX: ::c_int = 25;
pub const _SC_GETPW_R_SIZE_MAX: ::c_int = 26;
pub const _SC_PAGESIZE: ::c_int = 27;
pub const _SC_PAGE_SIZE: ::c_int = 27;
pub const _SC_SEM_NSEMS_MAX: ::c_int = 28;
pub const _SC_SEM_VALUE_MAX: ::c_int = 29;
pub const _SC_SEMAPHORES: ::c_int = 30;
pub const _SC_THREADS: ::c_int = 31;
pub const _SC_IOV_MAX: ::c_int = 32;
pub const _SC_UIO_MAXIOV: ::c_int = 32;
pub const _SC_NPROCESSORS_CONF: ::c_int = 34;
pub const _SC_NPROCESSORS_ONLN: ::c_int = 35;
pub const _SC_ATEXIT_MAX: ::c_int = 37;
pub const _SC_PASS_MAX: ::c_int = 39;
pub const _SC_PHYS_PAGES: ::c_int = 40;
pub const _SC_AVPHYS_PAGES: ::c_int = 41;
pub const _SC_PIPE: ::c_int = 42;
pub const _SC_SELECT: ::c_int = 43;
pub const _SC_POLL: ::c_int = 44;
pub const _SC_MAPPED_FILES: ::c_int = 45;
pub const _SC_THREAD_PROCESS_SHARED: ::c_int = 46;
pub const _SC_THREAD_STACK_MIN: ::c_int = 47;
pub const _SC_THREAD_ATTR_STACKADDR: ::c_int = 48;
pub const _SC_THREAD_ATTR_STACKSIZE: ::c_int = 49;
pub const _SC_THREAD_PRIORITY_SCHEDULING: ::c_int = 50;
pub const _SC_REALTIME_SIGNALS: ::c_int = 51;
pub const _SC_MEMORY_PROTECTION: ::c_int = 52;
pub const _SC_SIGQUEUE_MAX: ::c_int = 53;
pub const _SC_RTSIG_MAX: ::c_int = 54;
pub const _SC_MONOTONIC_CLOCK: ::c_int = 55;
pub const _SC_DELAYTIMER_MAX: ::c_int = 56;
pub const _SC_TIMER_MAX: ::c_int = 57;
pub const _SC_TIMERS: ::c_int = 58;
pub const _SC_CPUTIME: ::c_int = 59;
pub const _SC_THREAD_CPUTIME: ::c_int = 60;
pub const _SC_HOST_NAME_MAX: ::c_int = 61;
pub const _SC_REGEXP: ::c_int = 62;
pub const _SC_SYMLOOP_MAX: ::c_int = 63;
pub const _SC_SHELL: ::c_int = 64;

pub const PTHREAD_STACK_MIN: ::size_t = 8192;

pub const PTHREAD_MUTEX_INITIALIZER: pthread_mutex_t = pthread_mutex_t {
    flags: 0,
    lock: 0,
    unused: -42,
    owner: -1,
    owner_count: 0,
};
pub const PTHREAD_COND_INITIALIZER: pthread_cond_t = pthread_cond_t {
    flags: 0,
    unused: -42,
    mutex: 0 as *mut _,
    waiter_count: 0,
    lock: 0,
};
pub const PTHREAD_RWLOCK_INITIALIZER: pthread_rwlock_t = pthread_rwlock_t {
    flags: 0,
    owner: -1,
    lock_sem: 0,
    lock_count: 0,
    reader_count: 0,
    writer_count: 0,
    waiters: [0 as *mut _; 2],
};

pub const PTHREAD_MUTEX_DEFAULT: ::c_int = 0;
pub const PTHREAD_MUTEX_NORMAL: ::c_int = 1;
pub const PTHREAD_MUTEX_ERRORCHECK: ::c_int = 2;
pub const PTHREAD_MUTEX_RECURSIVE: ::c_int = 3;

pub const FIOCLEX: c_ulong = 0; // FIXME: does not exist on Haiku!

pub const RUSAGE_CHILDREN: ::c_int = -1;

pub const SOCK_STREAM: ::c_int = 1;
pub const SOCK_DGRAM: ::c_int = 2;
pub const SOCK_RAW: ::c_int = 3;
pub const SOCK_SEQPACKET: ::c_int = 5;

pub const SOL_SOCKET: ::c_int = -1;
pub const SO_ACCEPTCONN: ::c_int = 0x00000001;
pub const SO_BROADCAST: ::c_int = 0x00000002;
pub const SO_DEBUG: ::c_int = 0x00000004;
pub const SO_DONTROUTE: ::c_int = 0x00000008;
pub const SO_KEEPALIVE: ::c_int = 0x00000010;
pub const SO_OOBINLINE: ::c_int = 0x00000020;
pub const SO_REUSEADDR: ::c_int = 0x00000040;
pub const SO_REUSEPORT: ::c_int = 0x00000080;
pub const SO_USELOOPBACK: ::c_int = 0x00000100;
pub const SO_LINGER: ::c_int = 0x00000200;
pub const SO_SNDBUF: ::c_int = 0x40000001;
pub const SO_SNDLOWAT: ::c_int = 0x40000002;
pub const SO_SNDTIMEO: ::c_int = 0x40000003;
pub const SO_RCVBUF: ::c_int = 0x40000004;
pub const SO_RCVLOWAT: ::c_int = 0x40000005;
pub const SO_RCVTIMEO: ::c_int = 0x40000006;
pub const SO_ERROR: ::c_int = 0x40000007;
pub const SO_TYPE: ::c_int = 0x40000008;
pub const SO_NONBLOCK: ::c_int = 0x40000009;
pub const SO_BINDTODEVICE: ::c_int = 0x4000000a;
pub const SO_PEERCRED: ::c_int = 0x4000000b;

pub const SCM_RIGHTS: ::c_int = 0x01;

pub const NI_MAXHOST: ::size_t = 1025;

pub const WNOHANG: ::c_int = 0x01;
pub const WUNTRACED: ::c_int = 0x02;
pub const WCONTINUED: ::c_int = 0x04;
pub const WEXITED: ::c_int = 0x08;
pub const WSTOPPED: ::c_int = 0x10;
pub const WNOWAIT: ::c_int = 0x20;

pub const CLD_EXITED: ::c_int = 60;
pub const CLD_KILLED: ::c_int = 61;
pub const CLD_DUMPED: ::c_int = 62;
pub const CLD_TRAPPED: ::c_int = 63;
pub const CLD_STOPPED: ::c_int = 64;
pub const CLD_CONTINUED: ::c_int = 65;

pub const P_ALL: idtype_t = 0;
pub const P_PID: idtype_t = 1;
pub const P_PGID: idtype_t = 2;

pub const UTIME_OMIT: c_long = 1000000001;
pub const UTIME_NOW: c_long = 1000000000;

pub const VINTR: usize = 0;
pub const VQUIT: usize = 1;
pub const VERASE: usize = 2;
pub const VKILL: usize = 3;
pub const VEOF: usize = 4;
pub const VEOL: usize = 5;
pub const VMIN: usize = 4;
pub const VTIME: usize = 5;
pub const VEOL2: usize = 6;
pub const VSWTCH: usize = 7;
pub const VSTART: usize = 8;
pub const VSTOP: usize = 9;
pub const VSUSP: usize = 10;

pub const IGNBRK: ::tcflag_t = 0x01;
pub const BRKINT: ::tcflag_t = 0x02;
pub const IGNPAR: ::tcflag_t = 0x04;
pub const PARMRK: ::tcflag_t = 0x08;
pub const INPCK: ::tcflag_t = 0x10;
pub const ISTRIP: ::tcflag_t = 0x20;
pub const INLCR: ::tcflag_t = 0x40;
pub const IGNCR: ::tcflag_t = 0x80;
pub const ICRNL: ::tcflag_t = 0x100;
pub const IUCLC: ::tcflag_t = 0x200;
pub const IXON: ::tcflag_t = 0x400;
pub const IXANY: ::tcflag_t = 0x800;
pub const IXOFF: ::tcflag_t = 0x1000;

pub const OPOST: ::tcflag_t = 0x00000001;
pub const OLCUC: ::tcflag_t = 0x00000002;
pub const ONLCR: ::tcflag_t = 0x00000004;
pub const OCRNL: ::tcflag_t = 0x00000008;
pub const ONOCR: ::tcflag_t = 0x00000010;
pub const ONLRET: ::tcflag_t = 0x00000020;
pub const OFILL: ::tcflag_t = 0x00000040;
pub const OFDEL: ::tcflag_t = 0x00000080;
pub const NLDLY: ::tcflag_t = 0x00000100;
pub const NL0: ::tcflag_t = 0x00000000;
pub const NL1: ::tcflag_t = 0x00000100;
pub const CRDLY: ::tcflag_t = 0x00000600;
pub const CR0: ::tcflag_t = 0x00000000;
pub const CR1: ::tcflag_t = 0x00000200;
pub const CR2: ::tcflag_t = 0x00000400;
pub const CR3: ::tcflag_t = 0x00000600;
pub const TABDLY: ::tcflag_t = 0x00001800;
pub const TAB0: ::tcflag_t = 0x00000000;
pub const TAB1: ::tcflag_t = 0x00000800;
pub const TAB2: ::tcflag_t = 0x00001000;
pub const TAB3: ::tcflag_t = 0x00001800;
pub const BSDLY: ::tcflag_t = 0x00002000;
pub const BS0: ::tcflag_t = 0x00000000;
pub const BS1: ::tcflag_t = 0x00002000;
pub const VTDLY: ::tcflag_t = 0x00004000;
pub const VT0: ::tcflag_t = 0x00000000;
pub const VT1: ::tcflag_t = 0x00004000;
pub const FFDLY: ::tcflag_t = 0x00008000;
pub const FF0: ::tcflag_t = 0x00000000;
pub const FF1: ::tcflag_t = 0x00008000;

pub const CSIZE: ::tcflag_t = 0x00000020;
pub const CS5: ::tcflag_t = 0x00000000;
pub const CS6: ::tcflag_t = 0x00000000;
pub const CS7: ::tcflag_t = 0x00000000;
pub const CS8: ::tcflag_t = 0x00000020;
pub const CSTOPB: ::tcflag_t = 0x00000040;
pub const CREAD: ::tcflag_t = 0x00000080;
pub const PARENB: ::tcflag_t = 0x00000100;
pub const PARODD: ::tcflag_t = 0x00000200;
pub const HUPCL: ::tcflag_t = 0x00000400;
pub const CLOCAL: ::tcflag_t = 0x00000800;
pub const XLOBLK: ::tcflag_t = 0x00001000;
pub const CTSFLOW: ::tcflag_t = 0x00002000;
pub const RTSFLOW: ::tcflag_t = 0x00004000;
pub const CRTSCTS: ::tcflag_t = RTSFLOW | CTSFLOW;

pub const ISIG: ::tcflag_t = 0x00000001;
pub const ICANON: ::tcflag_t = 0x00000002;
pub const XCASE: ::tcflag_t = 0x00000004;
pub const ECHO: ::tcflag_t = 0x00000008;
pub const ECHOE: ::tcflag_t = 0x00000010;
pub const ECHOK: ::tcflag_t = 0x00000020;
pub const ECHONL: ::tcflag_t = 0x00000040;
pub const NOFLSH: ::tcflag_t = 0x00000080;
pub const TOSTOP: ::tcflag_t = 0x00000100;
pub const IEXTEN: ::tcflag_t = 0x00000200;
pub const ECHOCTL: ::tcflag_t = 0x00000400;
pub const ECHOPRT: ::tcflag_t = 0x00000800;
pub const ECHOKE: ::tcflag_t = 0x00001000;
pub const FLUSHO: ::tcflag_t = 0x00002000;
pub const PENDIN: ::tcflag_t = 0x00004000;

pub const TCGB_CTS: ::c_int = 0x01;
pub const TCGB_DSR: ::c_int = 0x02;
pub const TCGB_RI: ::c_int = 0x04;
pub const TCGB_DCD: ::c_int = 0x08;
pub const TIOCM_CTS: ::c_int = TCGB_CTS;
pub const TIOCM_CD: ::c_int = TCGB_DCD;
pub const TIOCM_CAR: ::c_int = TIOCM_CD;
pub const TIOCM_RI: ::c_int = TCGB_RI;
pub const TIOCM_DSR: ::c_int = TCGB_DSR;
pub const TIOCM_DTR: ::c_int = 0x10;
pub const TIOCM_RTS: ::c_int = 0x20;

pub const B0: speed_t = 0x00;
pub const B50: speed_t = 0x01;
pub const B75: speed_t = 0x02;
pub const B110: speed_t = 0x03;
pub const B134: speed_t = 0x04;
pub const B150: speed_t = 0x05;
pub const B200: speed_t = 0x06;
pub const B300: speed_t = 0x07;
pub const B600: speed_t = 0x08;
pub const B1200: speed_t = 0x09;
pub const B1800: speed_t = 0x0A;
pub const B2400: speed_t = 0x0B;
pub const B4800: speed_t = 0x0C;
pub const B9600: speed_t = 0x0D;
pub const B19200: speed_t = 0x0E;
pub const B38400: speed_t = 0x0F;
pub const B57600: speed_t = 0x10;
pub const B115200: speed_t = 0x11;
pub const B230400: speed_t = 0x12;
pub const B31250: speed_t = 0x13;

pub const TCSANOW: ::c_int = 0x01;
pub const TCSADRAIN: ::c_int = 0x02;
pub const TCSAFLUSH: ::c_int = 0x04;

pub const TCOOFF: ::c_int = 0x01;
pub const TCOON: ::c_int = 0x02;
pub const TCIOFF: ::c_int = 0x04;
pub const TCION: ::c_int = 0x08;

pub const TCIFLUSH: ::c_int = 0x01;
pub const TCOFLUSH: ::c_int = 0x02;
pub const TCIOFLUSH: ::c_int = 0x03;

pub const TCGETA: ::c_ulong = 0x8000;
pub const TCSETA: ::c_ulong = TCGETA + 1;
pub const TCSETAF: ::c_ulong = TCGETA + 2;
pub const TCSETAW: ::c_ulong = TCGETA + 3;
pub const TCWAITEVENT: ::c_ulong = TCGETA + 4;
pub const TCSBRK: ::c_ulong = TCGETA + 5;
pub const TCFLSH: ::c_ulong = TCGETA + 6;
pub const TCXONC: ::c_ulong = TCGETA + 7;
pub const TCQUERYCONNECTED: ::c_ulong = TCGETA + 8;
pub const TCGETBITS: ::c_ulong = TCGETA + 9;
pub const TCSETDTR: ::c_ulong = TCGETA + 10;
pub const TCSETRTS: ::c_ulong = TCGETA + 11;
pub const TIOCGWINSZ: ::c_ulong = TCGETA + 12;
pub const TIOCSWINSZ: ::c_ulong = TCGETA + 13;
pub const TCVTIME: ::c_ulong = TCGETA + 14;
pub const TIOCGPGRP: ::c_ulong = TCGETA + 15;
pub const TIOCSPGRP: ::c_ulong = TCGETA + 16;
pub const TIOCSCTTY: ::c_ulong = TCGETA + 17;
pub const TIOCMGET: ::c_ulong = TCGETA + 18;
pub const TIOCMSET: ::c_ulong = TCGETA + 19;
pub const TIOCSBRK: ::c_ulong = TCGETA + 20;
pub const TIOCCBRK: ::c_ulong = TCGETA + 21;
pub const TIOCMBIS: ::c_ulong = TCGETA + 22;
pub const TIOCMBIC: ::c_ulong = TCGETA + 23;

pub const PRIO_PROCESS: ::c_int = 0;
pub const PRIO_PGRP: ::c_int = 1;
pub const PRIO_USER: ::c_int = 2;

pub const LOG_PID: ::c_int = 1 << 12;
pub const LOG_CONS: ::c_int = 2 << 12;
pub const LOG_ODELAY: ::c_int = 4 << 12;
pub const LOG_NDELAY: ::c_int = 8 << 12;
pub const LOG_SERIAL: ::c_int = 16 << 12;
pub const LOG_PERROR: ::c_int = 32 << 12;
pub const LOG_NOWAIT: ::c_int = 64 << 12;

const_fn! {
    {const} fn CMSG_ALIGN(len: usize) -> usize {
        len + ::mem::size_of::<usize>() - 1 & !(::mem::size_of::<usize>() - 1)
    }
}

f! {
    pub fn CMSG_FIRSTHDR(mhdr: *const msghdr) -> *mut cmsghdr {
        if (*mhdr).msg_controllen as usize >= ::mem::size_of::<cmsghdr>() {
            (*mhdr).msg_control as *mut cmsghdr
        } else {
            0 as *mut cmsghdr
        }
    }

    pub fn CMSG_DATA(cmsg: *const ::cmsghdr) -> *mut ::c_uchar {
        (cmsg as *mut ::c_uchar)
            .offset(CMSG_ALIGN(::mem::size_of::<::cmsghdr>()) as isize)
    }

    pub {const} fn CMSG_SPACE(length: ::c_uint) -> ::c_uint {
        (CMSG_ALIGN(length as usize) + CMSG_ALIGN(::mem::size_of::<cmsghdr>()))
            as ::c_uint
    }

    pub fn CMSG_LEN(length: ::c_uint) -> ::c_uint {
        CMSG_ALIGN(::mem::size_of::<cmsghdr>()) as ::c_uint + length
    }

    pub fn CMSG_NXTHDR(mhdr: *const msghdr,
                       cmsg: *const cmsghdr) -> *mut cmsghdr {
        if cmsg.is_null() {
            return ::CMSG_FIRSTHDR(mhdr);
        };
        let next = cmsg as usize + CMSG_ALIGN((*cmsg).cmsg_len as usize)
            + CMSG_ALIGN(::mem::size_of::<::cmsghdr>());
        let max = (*mhdr).msg_control as usize
            + (*mhdr).msg_controllen as usize;
        if next > max {
            0 as *mut ::cmsghdr
        } else {
            (cmsg as usize + CMSG_ALIGN((*cmsg).cmsg_len as usize))
                as *mut ::cmsghdr
        }
    }

    pub fn FD_CLR(fd: ::c_int, set: *mut fd_set) -> () {
        let fd = fd as usize;
        let size = ::mem::size_of_val(&(*set).fds_bits[0]) * 8;
        (*set).fds_bits[fd / size] &= !(1 << (fd % size));
        return
    }

    pub fn FD_ISSET(fd: ::c_int, set: *mut fd_set) -> bool {
        let fd = fd as usize;
        let size = ::mem::size_of_val(&(*set).fds_bits[0]) * 8;
        return ((*set).fds_bits[fd / size] & (1 << (fd % size))) != 0
    }

    pub fn FD_SET(fd: ::c_int, set: *mut fd_set) -> () {
        let fd = fd as usize;
        let size = ::mem::size_of_val(&(*set).fds_bits[0]) * 8;
        (*set).fds_bits[fd / size] |= 1 << (fd % size);
        return
    }

    pub fn FD_ZERO(set: *mut fd_set) -> () {
        for slot in (*set).fds_bits.iter_mut() {
            *slot = 0;
        }
    }
}

safe_f! {
    pub {const} fn WIFEXITED(status: ::c_int) -> bool {
        (status & !0xff) == 0
    }

    pub {const} fn WEXITSTATUS(status: ::c_int) -> ::c_int {
        status & 0xff
    }

    pub {const} fn WIFSIGNALED(status: ::c_int) -> bool {
        ((status >> 8) & 0xff) != 0
    }

    pub {const} fn WTERMSIG(status: ::c_int) -> ::c_int {
        (status >> 8) & 0xff
    }

    pub {const} fn WIFSTOPPED(status: ::c_int) -> bool {
        ((status >> 16) & 0xff) != 0
    }

    pub {const} fn WSTOPSIG(status: ::c_int) -> ::c_int {
        (status >> 16) & 0xff
    }

    // actually WIFCORED, but this is used everywhere else
    pub {const} fn WCOREDUMP(status: ::c_int) -> bool {
        (status & 0x10000) != 0
    }

    pub {const} fn WIFCONTINUED(status: ::c_int) -> bool {
        (status & 0x20000) != 0
    }
}

extern "C" {
    pub fn getrlimit(resource: ::c_int, rlim: *mut ::rlimit) -> ::c_int;
    pub fn setrlimit(resource: ::c_int, rlim: *const ::rlimit) -> ::c_int;
    pub fn getpriority(which: ::c_int, who: id_t) -> ::c_int;
    pub fn setpriority(which: ::c_int, who: id_t, priority: ::c_int) -> ::c_int;

    pub fn utimensat(
        fd: ::c_int,
        path: *const ::c_char,
        times: *const ::timespec,
        flag: ::c_int,
    ) -> ::c_int;
    pub fn futimens(fd: ::c_int, times: *const ::timespec) -> ::c_int;
    pub fn strerror_r(errnum: ::c_int, buf: *mut c_char, buflen: ::size_t) -> ::c_int;
    pub fn _errnop() -> *mut ::c_int;

    pub fn abs(i: ::c_int) -> ::c_int;
    pub fn atof(s: *const ::c_char) -> ::c_double;
    pub fn labs(i: ::c_long) -> ::c_long;
    pub fn rand() -> ::c_int;
    pub fn srand(seed: ::c_uint);
}

#[link(name = "bsd")]
extern "C" {
    pub fn sem_destroy(sem: *mut sem_t) -> ::c_int;
    pub fn sem_init(sem: *mut sem_t, pshared: ::c_int, value: ::c_uint) -> ::c_int;

    pub fn clock_gettime(clk_id: ::c_int, tp: *mut ::timespec) -> ::c_int;
    pub fn clock_settime(clk_id: ::c_int, tp: *const ::timespec) -> ::c_int;
    pub fn pthread_create(
        thread: *mut ::pthread_t,
        attr: *const ::pthread_attr_t,
        f: extern "C" fn(*mut ::c_void) -> *mut ::c_void,
        value: *mut ::c_void,
    ) -> ::c_int;
    pub fn pthread_attr_getguardsize(
        attr: *const ::pthread_attr_t,
        guardsize: *mut ::size_t,
    ) -> ::c_int;
    pub fn pthread_attr_getstack(
        attr: *const ::pthread_attr_t,
        stackaddr: *mut *mut ::c_void,
        stacksize: *mut ::size_t,
    ) -> ::c_int;
    pub fn pthread_condattr_getclock(
        attr: *const pthread_condattr_t,
        clock_id: *mut clockid_t,
    ) -> ::c_int;
    pub fn pthread_condattr_setclock(
        attr: *mut pthread_condattr_t,
        clock_id: ::clockid_t,
    ) -> ::c_int;
    pub fn memalign(align: ::size_t, size: ::size_t) -> *mut ::c_void;
    pub fn setgroups(ngroups: ::c_int, ptr: *const ::gid_t) -> ::c_int;
    pub fn ioctl(fd: ::c_int, request: ::c_ulong, ...) -> ::c_int;
    pub fn mprotect(addr: *mut ::c_void, len: ::size_t, prot: ::c_int) -> ::c_int;
    pub fn dirfd(dirp: *mut ::DIR) -> ::c_int;
    pub fn getnameinfo(
        sa: *const ::sockaddr,
        salen: ::socklen_t,
        host: *mut ::c_char,
        hostlen: ::socklen_t,
        serv: *mut ::c_char,
        sevlen: ::socklen_t,
        flags: ::c_int,
    ) -> ::c_int;
    pub fn pthread_mutex_timedlock(
        lock: *mut pthread_mutex_t,
        abstime: *const ::timespec,
    ) -> ::c_int;
    pub fn waitid(idtype: idtype_t, id: id_t, infop: *mut ::siginfo_t, options: ::c_int)
        -> ::c_int;

    pub fn glob(
        pattern: *const ::c_char,
        flags: ::c_int,
        errfunc: ::Option<extern "C" fn(epath: *const ::c_char, errno: ::c_int) -> ::c_int>,
        pglob: *mut ::glob_t,
    ) -> ::c_int;
    pub fn globfree(pglob: *mut ::glob_t);
    pub fn gettimeofday(tp: *mut ::timeval, tz: *mut ::c_void) -> ::c_int;
    pub fn posix_madvise(addr: *mut ::c_void, len: ::size_t, advice: ::c_int) -> ::c_int;

    pub fn shm_open(name: *const ::c_char, oflag: ::c_int, mode: ::mode_t) -> ::c_int;
    pub fn shm_unlink(name: *const ::c_char) -> ::c_int;

    pub fn seekdir(dirp: *mut ::DIR, loc: ::c_long);

    pub fn telldir(dirp: *mut ::DIR) -> ::c_long;
    pub fn madvise(addr: *mut ::c_void, len: ::size_t, advice: ::c_int) -> ::c_int;

    pub fn msync(addr: *mut ::c_void, len: ::size_t, flags: ::c_int) -> ::c_int;

    pub fn recvfrom(
        socket: ::c_int,
        buf: *mut ::c_void,
        len: ::size_t,
        flags: ::c_int,
        addr: *mut ::sockaddr,
        addrlen: *mut ::socklen_t,
    ) -> ::ssize_t;
    pub fn mkstemps(template: *mut ::c_char, suffixlen: ::c_int) -> ::c_int;
    pub fn lutimes(file: *const ::c_char, times: *const ::timeval) -> ::c_int;
    pub fn nl_langinfo(item: ::nl_item) -> *mut ::c_char;

    pub fn bind(socket: ::c_int, address: *const ::sockaddr, address_len: ::socklen_t) -> ::c_int;

    pub fn writev(fd: ::c_int, iov: *const ::iovec, count: ::c_int) -> ::ssize_t;
    pub fn readv(fd: ::c_int, iov: *const ::iovec, count: ::c_int) -> ::ssize_t;

    pub fn sendmsg(fd: ::c_int, msg: *const ::msghdr, flags: ::c_int) -> ::ssize_t;
    pub fn recvmsg(fd: ::c_int, msg: *mut ::msghdr, flags: ::c_int) -> ::ssize_t;
    pub fn execvpe(
        file: *const ::c_char,
        argv: *const *const ::c_char,
        environment: *const *const ::c_char,
    ) -> ::c_int;
    pub fn getgrgid_r(
        gid: ::gid_t,
        grp: *mut ::group,
        buf: *mut ::c_char,
        buflen: ::size_t,
        result: *mut *mut ::group,
    ) -> ::c_int;
    pub fn sigaltstack(ss: *const stack_t, oss: *mut stack_t) -> ::c_int;
    pub fn sem_close(sem: *mut sem_t) -> ::c_int;
    pub fn getdtablesize() -> ::c_int;
    pub fn getgrnam_r(
        name: *const ::c_char,
        grp: *mut ::group,
        buf: *mut ::c_char,
        buflen: ::size_t,
        result: *mut *mut ::group,
    ) -> ::c_int;
    pub fn pthread_sigmask(how: ::c_int, set: *const sigset_t, oldset: *mut sigset_t) -> ::c_int;
    pub fn sem_open(name: *const ::c_char, oflag: ::c_int, ...) -> *mut sem_t;
    pub fn getgrnam(name: *const ::c_char) -> *mut ::group;
    pub fn pthread_kill(thread: ::pthread_t, sig: ::c_int) -> ::c_int;
    pub fn sem_unlink(name: *const ::c_char) -> ::c_int;
    pub fn daemon(nochdir: ::c_int, noclose: ::c_int) -> ::c_int;
    pub fn getpwnam_r(
        name: *const ::c_char,
        pwd: *mut passwd,
        buf: *mut ::c_char,
        buflen: ::size_t,
        result: *mut *mut passwd,
    ) -> ::c_int;
    pub fn getpwuid_r(
        uid: ::uid_t,
        pwd: *mut passwd,
        buf: *mut ::c_char,
        buflen: ::size_t,
        result: *mut *mut passwd,
    ) -> ::c_int;
    pub fn sigwait(set: *const sigset_t, sig: *mut ::c_int) -> ::c_int;
    pub fn pthread_atfork(
        prepare: ::Option<unsafe extern "C" fn()>,
        parent: ::Option<unsafe extern "C" fn()>,
        child: ::Option<unsafe extern "C" fn()>,
    ) -> ::c_int;
    pub fn getgrgid(gid: ::gid_t) -> *mut ::group;
    pub fn popen(command: *const c_char, mode: *const c_char) -> *mut ::FILE;
    pub fn openpty(
        amaster: *mut ::c_int,
        aslave: *mut ::c_int,
        name: *mut ::c_char,
        termp: *mut termios,
        winp: *mut ::winsize,
    ) -> ::c_int;
    pub fn forkpty(
        amaster: *mut ::c_int,
        name: *mut ::c_char,
        termp: *mut termios,
        winp: *mut ::winsize,
    ) -> ::pid_t;
    pub fn sethostname(name: *const ::c_char, len: ::size_t) -> ::c_int;
    pub fn uname(buf: *mut ::utsname) -> ::c_int;
}

cfg_if! {
    if #[cfg(target_pointer_width = "64")] {
        mod b64;
        pub use self::b64::*;
    } else {
        mod b32;
        pub use self::b32::*;
    }
}

mod native;
pub use self::native::*;
