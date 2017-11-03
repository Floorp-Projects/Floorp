//! Android-specific definitions for linux-like values

use dox::{mem, Option};

pub type clock_t = ::c_long;
pub type time_t = ::c_long;
pub type suseconds_t = ::c_long;
pub type off_t = ::c_long;
pub type blkcnt_t = ::c_ulong;
pub type blksize_t = ::c_ulong;
pub type nlink_t = u32;
pub type useconds_t = u32;
pub type pthread_t = ::c_long;
pub type pthread_mutexattr_t = ::c_long;
pub type pthread_rwlockattr_t = ::c_long;
pub type pthread_condattr_t = ::c_long;
pub type fsfilcnt_t = ::c_ulong;
pub type fsblkcnt_t = ::c_ulong;
pub type nfds_t = ::c_uint;
pub type rlim_t = ::c_ulong;
pub type dev_t = ::c_ulong;
pub type ino_t = ::c_ulong;
pub type __CPU_BITTYPE = ::c_ulong;
pub type idtype_t = ::c_int;
pub type loff_t = ::c_longlong;

s! {
    pub struct dirent {
        pub d_ino: u64,
        pub d_off: i64,
        pub d_reclen: ::c_ushort,
        pub d_type: ::c_uchar,
        pub d_name: [::c_char; 256],
    }

    pub struct dirent64 {
        pub d_ino: u64,
        pub d_off: i64,
        pub d_reclen: ::c_ushort,
        pub d_type: ::c_uchar,
        pub d_name: [::c_char; 256],
    }

    pub struct stack_t {
        pub ss_sp: *mut ::c_void,
        pub ss_flags: ::c_int,
        pub ss_size: ::size_t
    }

    pub struct siginfo_t {
        pub si_signo: ::c_int,
        pub si_errno: ::c_int,
        pub si_code: ::c_int,
        pub _pad: [::c_int; 29],
        _align: [usize; 0],
    }

    pub struct __fsid_t {
        __val: [::c_int; 2],
    }

    pub struct msghdr {
        pub msg_name: *mut ::c_void,
        pub msg_namelen: ::socklen_t,
        pub msg_iov: *mut ::iovec,
        pub msg_iovlen: ::size_t,
        pub msg_control: *mut ::c_void,
        pub msg_controllen: ::size_t,
        pub msg_flags: ::c_int,
    }

    pub struct cmsghdr {
        pub cmsg_len: ::size_t,
        pub cmsg_level: ::c_int,
        pub cmsg_type: ::c_int,
    }

    pub struct termios {
        pub c_iflag: ::tcflag_t,
        pub c_oflag: ::tcflag_t,
        pub c_cflag: ::tcflag_t,
        pub c_lflag: ::tcflag_t,
        pub c_line: ::cc_t,
        pub c_cc: [::cc_t; ::NCCS],
    }

    pub struct flock {
        pub l_type: ::c_short,
        pub l_whence: ::c_short,
        pub l_start: ::off_t,
        pub l_len: ::off_t,
        pub l_pid: ::pid_t,
    }

    pub struct cpu_set_t {
        #[cfg(target_pointer_width = "64")]
        __bits: [__CPU_BITTYPE; 16],
        #[cfg(target_pointer_width = "32")]
        __bits: [__CPU_BITTYPE; 1],
    }

    pub struct sem_t {
        count: ::c_uint,
        #[cfg(target_pointer_width = "64")]
        __reserved: [::c_int; 3],
    }

    pub struct lastlog {
        ll_time: ::time_t,
        ll_line: [::c_char; UT_LINESIZE],
        ll_host: [::c_char; UT_HOSTSIZE],
    }

    pub struct exit_status {
        pub e_termination: ::c_short,
        pub e_exit: ::c_short,
    }

    pub struct utmp {
        pub ut_type: ::c_short,
        pub ut_pid: ::pid_t,
        pub ut_line: [::c_char; UT_LINESIZE],
        pub ut_id: [::c_char; 4],

        pub ut_user: [::c_char; UT_NAMESIZE],
        pub ut_host: [::c_char; UT_HOSTSIZE],
        pub ut_exit: exit_status,
        pub ut_session: ::c_long,
        pub ut_tv: ::timeval,

        pub ut_addr_v6: [::int32_t; 4],
        unused: [::c_char; 20],
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
        #[cfg(target_pointer_width = "64")]
        __f_reserved: [u32; 6],
    }

    pub struct signalfd_siginfo {
        pub ssi_signo: ::uint32_t,
        pub ssi_errno: ::int32_t,
        pub ssi_code: ::int32_t,
        pub ssi_pid: ::uint32_t,
        pub ssi_uid: ::uint32_t,
        pub ssi_fd: ::int32_t,
        pub ssi_tid: ::uint32_t,
        pub ssi_band: ::uint32_t,
        pub ssi_overrun: ::uint32_t,
        pub ssi_trapno: ::uint32_t,
        pub ssi_status: ::int32_t,
        pub ssi_int: ::int32_t,
        pub ssi_ptr: ::c_ulonglong,
        pub ssi_utime: ::c_ulonglong,
        pub ssi_stime: ::c_ulonglong,
        pub ssi_addr: ::c_ulonglong,
        pub ssi_addr_lsb: ::uint16_t,
        _pad: [::uint8_t; 46],
    }
}

pub const O_TRUNC: ::c_int = 512;
pub const O_CLOEXEC: ::c_int = 0x80000;
pub const O_PATH: ::c_int = 0o10000000;
pub const O_NOATIME: ::c_int = 0o1000000;

pub const EBFONT: ::c_int = 59;
pub const ENOSTR: ::c_int = 60;
pub const ENODATA: ::c_int = 61;
pub const ETIME: ::c_int = 62;
pub const ENOSR: ::c_int = 63;
pub const ENONET: ::c_int = 64;
pub const ENOPKG: ::c_int = 65;
pub const EREMOTE: ::c_int = 66;
pub const ENOLINK: ::c_int = 67;
pub const EADV: ::c_int = 68;
pub const ESRMNT: ::c_int = 69;
pub const ECOMM: ::c_int = 70;
pub const EPROTO: ::c_int = 71;
pub const EDOTDOT: ::c_int = 73;

pub const SA_NODEFER: ::c_int = 0x40000000;
pub const SA_RESETHAND: ::c_int = 0x80000000;
pub const SA_RESTART: ::c_int = 0x10000000;
pub const SA_NOCLDSTOP: ::c_int = 0x00000001;

pub const EPOLL_CLOEXEC: ::c_int = 0x80000;
pub const EPOLLONESHOT: ::c_int = 0x40000000;
pub const EPOLLRDHUP: ::c_int = 0x00002000;
pub const EPOLLWAKEUP: ::c_int = 0x20000000;

pub const EFD_CLOEXEC: ::c_int = 0x80000;

pub const USER_PROCESS: ::c_short = 7;

pub const BUFSIZ: ::c_uint = 1024;
pub const FILENAME_MAX: ::c_uint = 1024;
pub const FOPEN_MAX: ::c_uint = 20;
pub const POSIX_FADV_DONTNEED: ::c_int = 4;
pub const POSIX_FADV_NOREUSE: ::c_int = 5;
pub const L_tmpnam: ::c_uint = 1024;
pub const TMP_MAX: ::c_uint = 308915776;
pub const _PC_LINK_MAX: ::c_int = 1;
pub const _PC_MAX_CANON: ::c_int = 2;
pub const _PC_MAX_INPUT: ::c_int = 3;
pub const _PC_NAME_MAX: ::c_int = 4;
pub const _PC_PATH_MAX: ::c_int = 5;
pub const _PC_PIPE_BUF: ::c_int = 6;
pub const _PC_2_SYMLINKS: ::c_int = 7;
pub const _PC_ALLOC_SIZE_MIN: ::c_int = 8;
pub const _PC_REC_INCR_XFER_SIZE: ::c_int = 9;
pub const _PC_REC_MAX_XFER_SIZE: ::c_int = 10;
pub const _PC_REC_MIN_XFER_SIZE: ::c_int = 11;
pub const _PC_REC_XFER_ALIGN: ::c_int = 12;
pub const _PC_SYMLINK_MAX: ::c_int = 13;
pub const _PC_CHOWN_RESTRICTED: ::c_int = 14;
pub const _PC_NO_TRUNC: ::c_int = 15;
pub const _PC_VDISABLE: ::c_int = 16;
pub const _PC_ASYNC_IO: ::c_int = 17;
pub const _PC_PRIO_IO: ::c_int = 18;
pub const _PC_SYNC_IO: ::c_int = 19;

pub const FIONBIO: ::c_int = 0x5421;

pub const _SC_ARG_MAX: ::c_int = 0;
pub const _SC_BC_BASE_MAX: ::c_int = 1;
pub const _SC_BC_DIM_MAX: ::c_int = 2;
pub const _SC_BC_SCALE_MAX: ::c_int = 3;
pub const _SC_BC_STRING_MAX: ::c_int = 4;
pub const _SC_CHILD_MAX: ::c_int = 5;
pub const _SC_CLK_TCK: ::c_int = 6;
pub const _SC_COLL_WEIGHTS_MAX: ::c_int = 7;
pub const _SC_EXPR_NEST_MAX: ::c_int = 8;
pub const _SC_LINE_MAX: ::c_int = 9;
pub const _SC_NGROUPS_MAX: ::c_int = 10;
pub const _SC_OPEN_MAX: ::c_int = 11;
pub const _SC_PASS_MAX: ::c_int = 12;
pub const _SC_2_C_BIND: ::c_int = 13;
pub const _SC_2_C_DEV: ::c_int = 14;
pub const _SC_2_C_VERSION: ::c_int = 15;
pub const _SC_2_CHAR_TERM: ::c_int = 16;
pub const _SC_2_FORT_DEV: ::c_int = 17;
pub const _SC_2_FORT_RUN: ::c_int = 18;
pub const _SC_2_LOCALEDEF: ::c_int = 19;
pub const _SC_2_SW_DEV: ::c_int = 20;
pub const _SC_2_UPE: ::c_int = 21;
pub const _SC_2_VERSION: ::c_int = 22;
pub const _SC_JOB_CONTROL: ::c_int = 23;
pub const _SC_SAVED_IDS: ::c_int = 24;
pub const _SC_VERSION: ::c_int = 25;
pub const _SC_RE_DUP_MAX: ::c_int = 26;
pub const _SC_STREAM_MAX: ::c_int = 27;
pub const _SC_TZNAME_MAX: ::c_int = 28;
pub const _SC_XOPEN_CRYPT: ::c_int = 29;
pub const _SC_XOPEN_ENH_I18N: ::c_int = 30;
pub const _SC_XOPEN_SHM: ::c_int = 31;
pub const _SC_XOPEN_VERSION: ::c_int = 32;
pub const _SC_XOPEN_XCU_VERSION: ::c_int = 33;
pub const _SC_XOPEN_REALTIME: ::c_int = 34;
pub const _SC_XOPEN_REALTIME_THREADS: ::c_int = 35;
pub const _SC_XOPEN_LEGACY: ::c_int = 36;
pub const _SC_ATEXIT_MAX: ::c_int = 37;
pub const _SC_IOV_MAX: ::c_int = 38;
pub const _SC_PAGESIZE: ::c_int = 39;
pub const _SC_PAGE_SIZE: ::c_int = 40;
pub const _SC_XOPEN_UNIX: ::c_int = 41;
pub const _SC_XBS5_ILP32_OFF32: ::c_int = 42;
pub const _SC_XBS5_ILP32_OFFBIG: ::c_int = 43;
pub const _SC_XBS5_LP64_OFF64: ::c_int = 44;
pub const _SC_XBS5_LPBIG_OFFBIG: ::c_int = 45;
pub const _SC_AIO_LISTIO_MAX: ::c_int = 46;
pub const _SC_AIO_MAX: ::c_int = 47;
pub const _SC_AIO_PRIO_DELTA_MAX: ::c_int = 48;
pub const _SC_DELAYTIMER_MAX: ::c_int = 49;
pub const _SC_MQ_OPEN_MAX: ::c_int = 50;
pub const _SC_MQ_PRIO_MAX: ::c_int = 51;
pub const _SC_RTSIG_MAX: ::c_int = 52;
pub const _SC_SEM_NSEMS_MAX: ::c_int = 53;
pub const _SC_SEM_VALUE_MAX: ::c_int = 54;
pub const _SC_SIGQUEUE_MAX: ::c_int = 55;
pub const _SC_TIMER_MAX: ::c_int = 56;
pub const _SC_ASYNCHRONOUS_IO: ::c_int = 57;
pub const _SC_FSYNC: ::c_int = 58;
pub const _SC_MAPPED_FILES: ::c_int = 59;
pub const _SC_MEMLOCK: ::c_int = 60;
pub const _SC_MEMLOCK_RANGE: ::c_int = 61;
pub const _SC_MEMORY_PROTECTION: ::c_int = 62;
pub const _SC_MESSAGE_PASSING: ::c_int = 63;
pub const _SC_PRIORITIZED_IO: ::c_int = 64;
pub const _SC_PRIORITY_SCHEDULING: ::c_int = 65;
pub const _SC_REALTIME_SIGNALS: ::c_int = 66;
pub const _SC_SEMAPHORES: ::c_int = 67;
pub const _SC_SHARED_MEMORY_OBJECTS: ::c_int = 68;
pub const _SC_SYNCHRONIZED_IO: ::c_int = 69;
pub const _SC_TIMERS: ::c_int = 70;
pub const _SC_GETGR_R_SIZE_MAX: ::c_int = 71;
pub const _SC_GETPW_R_SIZE_MAX: ::c_int = 72;
pub const _SC_LOGIN_NAME_MAX: ::c_int = 73;
pub const _SC_THREAD_DESTRUCTOR_ITERATIONS: ::c_int = 74;
pub const _SC_THREAD_KEYS_MAX: ::c_int = 75;
pub const _SC_THREAD_STACK_MIN: ::c_int = 76;
pub const _SC_THREAD_THREADS_MAX: ::c_int = 77;
pub const _SC_TTY_NAME_MAX: ::c_int = 78;
pub const _SC_THREADS: ::c_int = 79;
pub const _SC_THREAD_ATTR_STACKADDR: ::c_int = 80;
pub const _SC_THREAD_ATTR_STACKSIZE: ::c_int = 81;
pub const _SC_THREAD_PRIORITY_SCHEDULING: ::c_int = 82;
pub const _SC_THREAD_PRIO_INHERIT: ::c_int = 83;
pub const _SC_THREAD_PRIO_PROTECT: ::c_int = 84;
pub const _SC_THREAD_SAFE_FUNCTIONS: ::c_int = 85;
pub const _SC_NPROCESSORS_CONF: ::c_int = 96;
pub const _SC_NPROCESSORS_ONLN: ::c_int = 97;
pub const _SC_PHYS_PAGES: ::c_int = 98;
pub const _SC_AVPHYS_PAGES: ::c_int = 99;
pub const _SC_MONOTONIC_CLOCK: ::c_int = 100;

pub const PTHREAD_MUTEX_NORMAL: ::c_int = 0;
pub const PTHREAD_MUTEX_RECURSIVE: ::c_int = 1;
pub const PTHREAD_MUTEX_ERRORCHECK: ::c_int = 2;
pub const PTHREAD_MUTEX_DEFAULT: ::c_int = PTHREAD_MUTEX_NORMAL;

pub const FIOCLEX: ::c_int = 0x5451;

pub const SA_ONSTACK: ::c_ulong = 0x08000000;
pub const SA_SIGINFO: ::c_ulong = 0x00000004;
pub const SA_NOCLDWAIT: ::c_ulong = 0x00000002;
pub const SIGCHLD: ::c_int = 17;
pub const SIGBUS: ::c_int = 7;
pub const SIGUSR1: ::c_int = 10;
pub const SIGUSR2: ::c_int = 12;
pub const SIGCONT: ::c_int = 18;
pub const SIGSTOP: ::c_int = 19;
pub const SIGTSTP: ::c_int = 20;
pub const SIGURG: ::c_int = 23;
pub const SIGIO: ::c_int = 29;
pub const SIGSYS: ::c_int = 31;
pub const SIGSTKFLT: ::c_int = 16;
pub const SIGUNUSED: ::c_int = 31;
pub const SIGTTIN: ::c_int = 21;
pub const SIGTTOU: ::c_int = 22;
pub const SIGXCPU: ::c_int = 24;
pub const SIGXFSZ: ::c_int = 25;
pub const SIGVTALRM: ::c_int = 26;
pub const SIGPROF: ::c_int = 27;
pub const SIGWINCH: ::c_int = 28;
pub const SIGPOLL: ::c_int = 29;
pub const SIGPWR: ::c_int = 30;
pub const SIG_SETMASK: ::c_int = 2;
pub const SIG_BLOCK: ::c_int = 0x000000;
pub const SIG_UNBLOCK: ::c_int = 0x01;

pub const RUSAGE_CHILDREN: ::c_int = -1;

pub const LC_PAPER: ::c_int = 7;
pub const LC_NAME: ::c_int = 8;
pub const LC_ADDRESS: ::c_int = 9;
pub const LC_TELEPHONE: ::c_int = 10;
pub const LC_MEASUREMENT: ::c_int = 11;
pub const LC_IDENTIFICATION: ::c_int = 12;
pub const LC_PAPER_MASK: ::c_int = (1 << LC_PAPER);
pub const LC_NAME_MASK: ::c_int = (1 << LC_NAME);
pub const LC_ADDRESS_MASK: ::c_int = (1 << LC_ADDRESS);
pub const LC_TELEPHONE_MASK: ::c_int = (1 << LC_TELEPHONE);
pub const LC_MEASUREMENT_MASK: ::c_int = (1 << LC_MEASUREMENT);
pub const LC_IDENTIFICATION_MASK: ::c_int = (1 << LC_IDENTIFICATION);
pub const LC_ALL_MASK: ::c_int = ::LC_CTYPE_MASK
                               | ::LC_NUMERIC_MASK
                               | ::LC_TIME_MASK
                               | ::LC_COLLATE_MASK
                               | ::LC_MONETARY_MASK
                               | ::LC_MESSAGES_MASK
                               | LC_PAPER_MASK
                               | LC_NAME_MASK
                               | LC_ADDRESS_MASK
                               | LC_TELEPHONE_MASK
                               | LC_MEASUREMENT_MASK
                               | LC_IDENTIFICATION_MASK;

pub const MAP_ANON: ::c_int = 0x0020;
pub const MAP_ANONYMOUS: ::c_int = 0x0020;
pub const MAP_GROWSDOWN: ::c_int = 0x0100;
pub const MAP_DENYWRITE: ::c_int = 0x0800;
pub const MAP_EXECUTABLE: ::c_int = 0x01000;
pub const MAP_LOCKED: ::c_int = 0x02000;
pub const MAP_NORESERVE: ::c_int = 0x04000;
pub const MAP_POPULATE: ::c_int = 0x08000;
pub const MAP_NONBLOCK: ::c_int = 0x010000;
pub const MAP_STACK: ::c_int = 0x020000;

pub const EDEADLK: ::c_int = 35;
pub const ENAMETOOLONG: ::c_int = 36;
pub const ENOLCK: ::c_int = 37;
pub const ENOSYS: ::c_int = 38;
pub const ENOTEMPTY: ::c_int = 39;
pub const ELOOP: ::c_int = 40;
pub const ENOMSG: ::c_int = 42;
pub const EIDRM: ::c_int = 43;
pub const ECHRNG: ::c_int = 44;
pub const EL2NSYNC: ::c_int = 45;
pub const EL3HLT: ::c_int = 46;
pub const EL3RST: ::c_int = 47;
pub const ELNRNG: ::c_int = 48;
pub const EUNATCH: ::c_int = 49;
pub const ENOCSI: ::c_int = 50;
pub const EL2HLT: ::c_int = 51;
pub const EBADE: ::c_int = 52;
pub const EBADR: ::c_int = 53;
pub const EXFULL: ::c_int = 54;
pub const ENOANO: ::c_int = 55;
pub const EBADRQC: ::c_int = 56;
pub const EBADSLT: ::c_int = 57;

pub const EMULTIHOP: ::c_int = 72;
pub const EBADMSG: ::c_int = 74;
pub const EOVERFLOW: ::c_int = 75;
pub const ENOTUNIQ: ::c_int = 76;
pub const EBADFD: ::c_int = 77;
pub const EREMCHG: ::c_int = 78;
pub const ELIBACC: ::c_int = 79;
pub const ELIBBAD: ::c_int = 80;
pub const ELIBSCN: ::c_int = 81;
pub const ELIBMAX: ::c_int = 82;
pub const ELIBEXEC: ::c_int = 83;
pub const EILSEQ: ::c_int = 84;
pub const ERESTART: ::c_int = 85;
pub const ESTRPIPE: ::c_int = 86;
pub const EUSERS: ::c_int = 87;
pub const ENOTSOCK: ::c_int = 88;
pub const EDESTADDRREQ: ::c_int = 89;
pub const EMSGSIZE: ::c_int = 90;
pub const EPROTOTYPE: ::c_int = 91;
pub const ENOPROTOOPT: ::c_int = 92;
pub const EPROTONOSUPPORT: ::c_int = 93;
pub const ESOCKTNOSUPPORT: ::c_int = 94;
pub const EOPNOTSUPP: ::c_int = 95;
pub const ENOTSUP: ::c_int = EOPNOTSUPP;
pub const EPFNOSUPPORT: ::c_int = 96;
pub const EAFNOSUPPORT: ::c_int = 97;
pub const EADDRINUSE: ::c_int = 98;
pub const EADDRNOTAVAIL: ::c_int = 99;
pub const ENETDOWN: ::c_int = 100;
pub const ENETUNREACH: ::c_int = 101;
pub const ENETRESET: ::c_int = 102;
pub const ECONNABORTED: ::c_int = 103;
pub const ECONNRESET: ::c_int = 104;
pub const ENOBUFS: ::c_int = 105;
pub const EISCONN: ::c_int = 106;
pub const ENOTCONN: ::c_int = 107;
pub const ESHUTDOWN: ::c_int = 108;
pub const ETOOMANYREFS: ::c_int = 109;
pub const ETIMEDOUT: ::c_int = 110;
pub const ECONNREFUSED: ::c_int = 111;
pub const EHOSTDOWN: ::c_int = 112;
pub const EHOSTUNREACH: ::c_int = 113;
pub const EALREADY: ::c_int = 114;
pub const EINPROGRESS: ::c_int = 115;
pub const ESTALE: ::c_int = 116;
pub const EUCLEAN: ::c_int = 117;
pub const ENOTNAM: ::c_int = 118;
pub const ENAVAIL: ::c_int = 119;
pub const EISNAM: ::c_int = 120;
pub const EREMOTEIO: ::c_int = 121;
pub const EDQUOT: ::c_int = 122;
pub const ENOMEDIUM: ::c_int = 123;
pub const EMEDIUMTYPE: ::c_int = 124;
pub const ECANCELED: ::c_int = 125;
pub const ENOKEY: ::c_int = 126;
pub const EKEYEXPIRED: ::c_int = 127;
pub const EKEYREVOKED: ::c_int = 128;
pub const EKEYREJECTED: ::c_int = 129;
pub const EOWNERDEAD: ::c_int = 130;
pub const ENOTRECOVERABLE: ::c_int = 131;

pub const SOCK_STREAM: ::c_int = 1;
pub const SOCK_DGRAM: ::c_int = 2;
pub const SOCK_SEQPACKET: ::c_int = 5;

pub const SOL_SOCKET: ::c_int = 1;
pub const SOL_SCTP: ::c_int = 132;
pub const SOL_IPX: ::c_int = 256;
pub const SOL_AX25: ::c_int = 257;
pub const SOL_ATALK: ::c_int = 258;
pub const SOL_NETROM: ::c_int = 259;
pub const SOL_ROSE: ::c_int = 260;

#[doc(hidden)]
pub const AF_MAX: ::c_int = 43;
#[doc(hidden)]
pub const PF_MAX: ::c_int = AF_MAX;

pub const SO_REUSEADDR: ::c_int = 2;
pub const SO_TYPE: ::c_int = 3;
pub const SO_ERROR: ::c_int = 4;
pub const SO_DONTROUTE: ::c_int = 5;
pub const SO_BROADCAST: ::c_int = 6;
pub const SO_SNDBUF: ::c_int = 7;
pub const SO_RCVBUF: ::c_int = 8;
pub const SO_KEEPALIVE: ::c_int = 9;
pub const SO_OOBINLINE: ::c_int = 10;
pub const SO_PRIORITY: ::c_int = 12;
pub const SO_LINGER: ::c_int = 13;
pub const SO_BSDCOMPAT: ::c_int = 14;
pub const SO_REUSEPORT: ::c_int = 15;
pub const SO_PASSCRED: ::c_int = 16;
pub const SO_PEERCRED: ::c_int = 17;
pub const SO_RCVLOWAT: ::c_int = 18;
pub const SO_SNDLOWAT: ::c_int = 19;
pub const SO_RCVTIMEO: ::c_int = 20;
pub const SO_SNDTIMEO: ::c_int = 21;
pub const SO_BINDTODEVICE: ::c_int = 25;
pub const SO_TIMESTAMP: ::c_int = 29;
pub const SO_ACCEPTCONN: ::c_int = 30;
pub const SO_SNDBUFFORCE: ::c_int = 32;
pub const SO_RCVBUFFORCE: ::c_int = 33;
pub const SO_MARK: ::c_int = 36;
pub const SO_PROTOCOL: ::c_int = 38;
pub const SO_DOMAIN: ::c_int = 39;
pub const SO_RXQ_OVFL: ::c_int = 40;
pub const SO_PEEK_OFF: ::c_int = 42;
pub const SO_BUSY_POLL: ::c_int = 46;

pub const O_ACCMODE: ::c_int = 3;
pub const O_APPEND: ::c_int = 1024;
pub const O_CREAT: ::c_int = 64;
pub const O_EXCL: ::c_int = 128;
pub const O_NOCTTY: ::c_int = 256;
pub const O_NONBLOCK: ::c_int = 2048;
pub const O_SYNC: ::c_int = 0x101000;
pub const O_ASYNC: ::c_int = 0x2000;
pub const O_NDELAY: ::c_int = 0x800;
pub const O_DSYNC: ::c_int = 4096;

pub const NI_MAXHOST: ::size_t = 1025;

pub const NCCS: usize = 19;
pub const TCSBRKP: ::c_int = 0x5425;
pub const TCSANOW: ::c_int = 0;
pub const TCSADRAIN: ::c_int = 0x1;
pub const TCSAFLUSH: ::c_int = 0x2;
pub const VEOF: usize = 4;
pub const VEOL: usize = 11;
pub const VEOL2: usize = 16;
pub const VMIN: usize = 6;
pub const IEXTEN: ::tcflag_t = 0x00008000;
pub const TOSTOP: ::tcflag_t = 0x00000100;
pub const FLUSHO: ::tcflag_t = 0x00001000;
pub const EXTPROC: ::tcflag_t = 0o200000;

pub const ADFS_SUPER_MAGIC: ::c_long = 0x0000adf5;
pub const AFFS_SUPER_MAGIC: ::c_long = 0x0000adff;
pub const CODA_SUPER_MAGIC: ::c_long = 0x73757245;
pub const CRAMFS_MAGIC: ::c_long = 0x28cd3d45;
pub const EFS_SUPER_MAGIC: ::c_long = 0x00414a53;
pub const EXT2_SUPER_MAGIC: ::c_long = 0x0000ef53;
pub const EXT3_SUPER_MAGIC: ::c_long = 0x0000ef53;
pub const EXT4_SUPER_MAGIC: ::c_long = 0x0000ef53;
pub const HPFS_SUPER_MAGIC: ::c_long = 0xf995e849;
pub const HUGETLBFS_MAGIC: ::c_long = 0x958458f6;
pub const ISOFS_SUPER_MAGIC: ::c_long = 0x00009660;
pub const JFFS2_SUPER_MAGIC: ::c_long = 0x000072b6;
pub const MINIX_SUPER_MAGIC: ::c_long = 0x0000137f;
pub const MINIX_SUPER_MAGIC2: ::c_long = 0x0000138f;
pub const MINIX2_SUPER_MAGIC: ::c_long = 0x00002468;
pub const MINIX2_SUPER_MAGIC2: ::c_long = 0x00002478;
pub const MSDOS_SUPER_MAGIC: ::c_long = 0x00004d44;
pub const NCP_SUPER_MAGIC: ::c_long = 0x0000564c;
pub const NFS_SUPER_MAGIC: ::c_long = 0x00006969;
pub const OPENPROM_SUPER_MAGIC: ::c_long = 0x00009fa1;
pub const PROC_SUPER_MAGIC: ::c_long = 0x00009fa0;
pub const QNX4_SUPER_MAGIC: ::c_long = 0x0000002f;
pub const REISERFS_SUPER_MAGIC: ::c_long = 0x52654973;
pub const SMB_SUPER_MAGIC: ::c_long = 0x0000517b;
pub const TMPFS_MAGIC: ::c_long = 0x01021994;
pub const USBDEVICE_SUPER_MAGIC: ::c_long = 0x00009fa2;

pub const MAP_HUGETLB: ::c_int = 0x040000;

pub const PTRACE_TRACEME: ::c_int = 0;
pub const PTRACE_PEEKTEXT: ::c_int = 1;
pub const PTRACE_PEEKDATA: ::c_int = 2;
pub const PTRACE_PEEKUSER: ::c_int = 3;
pub const PTRACE_POKETEXT: ::c_int = 4;
pub const PTRACE_POKEDATA: ::c_int = 5;
pub const PTRACE_POKEUSER: ::c_int = 6;
pub const PTRACE_CONT: ::c_int = 7;
pub const PTRACE_KILL: ::c_int = 8;
pub const PTRACE_SINGLESTEP: ::c_int = 9;
pub const PTRACE_ATTACH: ::c_int = 16;
pub const PTRACE_DETACH: ::c_int = 17;
pub const PTRACE_SYSCALL: ::c_int = 24;
pub const PTRACE_SETOPTIONS: ::c_int = 0x4200;
pub const PTRACE_GETEVENTMSG: ::c_int = 0x4201;
pub const PTRACE_GETSIGINFO: ::c_int = 0x4202;
pub const PTRACE_SETSIGINFO: ::c_int = 0x4203;

pub const EFD_NONBLOCK: ::c_int = 0x800;

pub const F_GETLK: ::c_int = 5;
pub const F_GETOWN: ::c_int = 9;
pub const F_SETOWN: ::c_int = 8;
pub const F_SETLK: ::c_int = 6;
pub const F_SETLKW: ::c_int = 7;

pub const TCGETS: ::c_int = 0x5401;
pub const TCSETS: ::c_int = 0x5402;
pub const TCSETSW: ::c_int = 0x5403;
pub const TCSETSF: ::c_int = 0x5404;
pub const TCGETA: ::c_int = 0x5405;
pub const TCSETA: ::c_int = 0x5406;
pub const TCSETAW: ::c_int = 0x5407;
pub const TCSETAF: ::c_int = 0x5408;
pub const TCSBRK: ::c_int = 0x5409;
pub const TCXONC: ::c_int = 0x540A;
pub const TCFLSH: ::c_int = 0x540B;
pub const TIOCGSOFTCAR: ::c_int = 0x5419;
pub const TIOCSSOFTCAR: ::c_int = 0x541A;
pub const TIOCINQ: ::c_int = 0x541B;
pub const TIOCLINUX: ::c_int = 0x541C;
pub const TIOCGSERIAL: ::c_int = 0x541E;
pub const TIOCEXCL: ::c_int = 0x540C;
pub const TIOCNXCL: ::c_int = 0x540D;
pub const TIOCSCTTY: ::c_int = 0x540E;
pub const TIOCGPGRP: ::c_int = 0x540F;
pub const TIOCSPGRP: ::c_int = 0x5410;
pub const TIOCOUTQ: ::c_int = 0x5411;
pub const TIOCSTI: ::c_int = 0x5412;
pub const TIOCGWINSZ: ::c_int = 0x5413;
pub const TIOCSWINSZ: ::c_int = 0x5414;
pub const TIOCMGET: ::c_int = 0x5415;
pub const TIOCMBIS: ::c_int = 0x5416;
pub const TIOCMBIC: ::c_int = 0x5417;
pub const TIOCMSET: ::c_int = 0x5418;
pub const FIONREAD: ::c_int = 0x541B;
pub const TIOCCONS: ::c_int = 0x541D;

pub const RTLD_NOLOAD: ::c_int = 0x4;

pub const SEM_FAILED: *mut sem_t = 0 as *mut sem_t;

pub const LINUX_REBOOT_MAGIC1: ::c_int = 0xfee1dead;
pub const LINUX_REBOOT_MAGIC2: ::c_int = 672274793;
pub const LINUX_REBOOT_MAGIC2A: ::c_int = 85072278;
pub const LINUX_REBOOT_MAGIC2B: ::c_int = 369367448;
pub const LINUX_REBOOT_MAGIC2C: ::c_int = 537993216;

pub const LINUX_REBOOT_CMD_RESTART: ::c_int = 0x01234567;
pub const LINUX_REBOOT_CMD_HALT: ::c_int = 0xCDEF0123;
pub const LINUX_REBOOT_CMD_CAD_ON: ::c_int = 0x89ABCDEF;
pub const LINUX_REBOOT_CMD_CAD_OFF: ::c_int = 0x00000000;
pub const LINUX_REBOOT_CMD_POWER_OFF: ::c_int = 0x4321FEDC;
pub const LINUX_REBOOT_CMD_RESTART2: ::c_int = 0xA1B2C3D4;
pub const LINUX_REBOOT_CMD_SW_SUSPEND: ::c_int = 0xD000FCE2;
pub const LINUX_REBOOT_CMD_KEXEC: ::c_int = 0x45584543;

pub const MCL_CURRENT: ::c_int = 0x0001;
pub const MCL_FUTURE: ::c_int = 0x0002;

pub const CBAUD: ::tcflag_t = 0o0010017;
pub const TAB1: ::c_int = 0x00000800;
pub const TAB2: ::c_int = 0x00001000;
pub const TAB3: ::c_int = 0x00001800;
pub const CR1: ::c_int  = 0x00000200;
pub const CR2: ::c_int  = 0x00000400;
pub const CR3: ::c_int  = 0x00000600;
pub const FF1: ::c_int  = 0x00008000;
pub const BS1: ::c_int  = 0x00002000;
pub const VT1: ::c_int  = 0x00004000;
pub const VWERASE: usize = 14;
pub const VREPRINT: usize = 12;
pub const VSUSP: usize = 10;
pub const VSTART: usize = 8;
pub const VSTOP: usize = 9;
pub const VDISCARD: usize = 13;
pub const VTIME: usize = 5;
pub const IXON: ::tcflag_t = 0x00000400;
pub const IXOFF: ::tcflag_t = 0x00001000;
pub const ONLCR: ::tcflag_t = 0x4;
pub const CSIZE: ::tcflag_t = 0x00000030;
pub const CS6: ::tcflag_t = 0x00000010;
pub const CS7: ::tcflag_t = 0x00000020;
pub const CS8: ::tcflag_t = 0x00000030;
pub const CSTOPB: ::tcflag_t = 0x00000040;
pub const CREAD: ::tcflag_t = 0x00000080;
pub const PARENB: ::tcflag_t = 0x00000100;
pub const PARODD: ::tcflag_t = 0x00000200;
pub const HUPCL: ::tcflag_t = 0x00000400;
pub const CLOCAL: ::tcflag_t = 0x00000800;
pub const ECHOKE: ::tcflag_t = 0x00000800;
pub const ECHOE: ::tcflag_t = 0x00000010;
pub const ECHOK: ::tcflag_t = 0x00000020;
pub const ECHONL: ::tcflag_t = 0x00000040;
pub const ECHOPRT: ::tcflag_t = 0x00000400;
pub const ECHOCTL: ::tcflag_t = 0x00000200;
pub const ISIG: ::tcflag_t = 0x00000001;
pub const ICANON: ::tcflag_t = 0x00000002;
pub const PENDIN: ::tcflag_t = 0x00004000;
pub const NOFLSH: ::tcflag_t = 0x00000080;
pub const VSWTC: usize = 7;
pub const OLCUC:  ::tcflag_t = 0o000002;
pub const NLDLY:  ::tcflag_t = 0o000400;
pub const CRDLY:  ::tcflag_t = 0o003000;
pub const TABDLY: ::tcflag_t = 0o014000;
pub const BSDLY:  ::tcflag_t = 0o020000;
pub const FFDLY:  ::tcflag_t = 0o100000;
pub const VTDLY:  ::tcflag_t = 0o040000;
pub const XTABS:  ::tcflag_t = 0o014000;

pub const B0: ::speed_t = 0o000000;
pub const B50: ::speed_t = 0o000001;
pub const B75: ::speed_t = 0o000002;
pub const B110: ::speed_t = 0o000003;
pub const B134: ::speed_t = 0o000004;
pub const B150: ::speed_t = 0o000005;
pub const B200: ::speed_t = 0o000006;
pub const B300: ::speed_t = 0o000007;
pub const B600: ::speed_t = 0o000010;
pub const B1200: ::speed_t = 0o000011;
pub const B1800: ::speed_t = 0o000012;
pub const B2400: ::speed_t = 0o000013;
pub const B4800: ::speed_t = 0o000014;
pub const B9600: ::speed_t = 0o000015;
pub const B19200: ::speed_t = 0o000016;
pub const B38400: ::speed_t = 0o000017;
pub const EXTA: ::speed_t = B19200;
pub const EXTB: ::speed_t = B38400;
pub const B57600: ::speed_t = 0o010001;
pub const B115200: ::speed_t = 0o010002;
pub const B230400: ::speed_t = 0o010003;
pub const B460800: ::speed_t = 0o010004;
pub const B500000: ::speed_t = 0o010005;
pub const B576000: ::speed_t = 0o010006;
pub const B921600: ::speed_t = 0o010007;
pub const B1000000: ::speed_t = 0o010010;
pub const B1152000: ::speed_t = 0o010011;
pub const B1500000: ::speed_t = 0o010012;
pub const B2000000: ::speed_t = 0o010013;
pub const B2500000: ::speed_t = 0o010014;
pub const B3000000: ::speed_t = 0o010015;
pub const B3500000: ::speed_t = 0o010016;
pub const B4000000: ::speed_t = 0o010017;

pub const EAI_SYSTEM: ::c_int = 11;

pub const NETLINK_ROUTE: ::c_int = 0;
pub const NETLINK_UNUSED: ::c_int = 1;
pub const NETLINK_USERSOCK: ::c_int = 2;
pub const NETLINK_FIREWALL: ::c_int = 3;
pub const NETLINK_SOCK_DIAG: ::c_int = 4;
pub const NETLINK_NFLOG: ::c_int = 5;
pub const NETLINK_XFRM: ::c_int = 6;
pub const NETLINK_SELINUX: ::c_int = 7;
pub const NETLINK_ISCSI: ::c_int = 8;
pub const NETLINK_AUDIT: ::c_int = 9;
pub const NETLINK_FIB_LOOKUP: ::c_int = 10;
pub const NETLINK_CONNECTOR: ::c_int = 11;
pub const NETLINK_NETFILTER: ::c_int = 12;
pub const NETLINK_IP6_FW: ::c_int = 13;
pub const NETLINK_DNRTMSG: ::c_int = 14;
pub const NETLINK_KOBJECT_UEVENT: ::c_int = 15;
pub const NETLINK_GENERIC: ::c_int = 16;
pub const NETLINK_SCSITRANSPORT: ::c_int = 18;
pub const NETLINK_ECRYPTFS: ::c_int = 19;
pub const NETLINK_RDMA: ::c_int = 20;
pub const NETLINK_CRYPTO: ::c_int = 21;
pub const NETLINK_INET_DIAG: ::c_int = NETLINK_SOCK_DIAG;

pub const MAX_LINKS: ::c_int = 32;

pub const NLM_F_REQUEST: ::c_int = 1;
pub const NLM_F_MULTI: ::c_int = 2;
pub const NLM_F_ACK: ::c_int = 4;
pub const NLM_F_ECHO: ::c_int = 8;
pub const NLM_F_DUMP_INTR: ::c_int = 16;

pub const NLM_F_ROOT: ::c_int = 0x100;
pub const NLM_F_MATCH: ::c_int = 0x200;
pub const NLM_F_ATOMIC: ::c_int = 0x400;
pub const NLM_F_DUMP: ::c_int = NLM_F_ROOT | NLM_F_MATCH;

pub const NLM_F_REPLACE: ::c_int = 0x100;
pub const NLM_F_EXCL: ::c_int = 0x200;
pub const NLM_F_CREATE: ::c_int = 0x400;
pub const NLM_F_APPEND: ::c_int = 0x800;

pub const NLMSG_NOOP: ::c_int = 0x1;
pub const NLMSG_ERROR: ::c_int = 0x2;
pub const NLMSG_DONE: ::c_int = 0x3;
pub const NLMSG_OVERRUN: ::c_int = 0x4;
pub const NLMSG_MIN_TYPE: ::c_int = 0x10;

pub const NETLINK_ADD_MEMBERSHIP: ::c_int = 1;
pub const NETLINK_DROP_MEMBERSHIP: ::c_int = 2;
pub const NETLINK_PKTINFO: ::c_int = 3;
pub const NETLINK_BROADCAST_ERROR: ::c_int = 4;
pub const NETLINK_NO_ENOBUFS: ::c_int = 5;
pub const NETLINK_RX_RING: ::c_int = 6;
pub const NETLINK_TX_RING: ::c_int = 7;

pub const NLA_F_NESTED: ::c_int = 1 << 15;
pub const NLA_F_NET_BYTEORDER: ::c_int = 1 << 14;
pub const NLA_TYPE_MASK: ::c_int = !(NLA_F_NESTED | NLA_F_NET_BYTEORDER);

pub const SIGEV_THREAD_ID: ::c_int = 4;

pub const CIBAUD: ::tcflag_t = 0o02003600000;
pub const CBAUDEX: ::tcflag_t = 0o010000;

pub const TIOCM_LE: ::c_int = 0x001;
pub const TIOCM_DTR: ::c_int = 0x002;
pub const TIOCM_RTS: ::c_int = 0x004;
pub const TIOCM_ST: ::c_int = 0x008;
pub const TIOCM_SR: ::c_int = 0x010;
pub const TIOCM_CTS: ::c_int = 0x020;
pub const TIOCM_CAR: ::c_int = 0x040;
pub const TIOCM_RNG: ::c_int = 0x080;
pub const TIOCM_DSR: ::c_int = 0x100;
pub const TIOCM_CD: ::c_int = TIOCM_CAR;
pub const TIOCM_RI: ::c_int = TIOCM_RNG;

pub const POLLWRNORM: ::c_short = 0x100;
pub const POLLWRBAND: ::c_short = 0x200;

pub const SFD_CLOEXEC: ::c_int = O_CLOEXEC;
pub const SFD_NONBLOCK: ::c_int = O_NONBLOCK;

pub const SOCK_NONBLOCK: ::c_int = O_NONBLOCK;

pub const SO_ORIGINAL_DST: ::c_int = 80;
pub const IUTF8: ::tcflag_t = 0x00004000;
pub const CMSPAR: ::tcflag_t = 0o10000000000;
pub const O_TMPFILE: ::c_int = 0o20000000 | O_DIRECTORY;

f! {
    pub fn CPU_ZERO(cpuset: &mut cpu_set_t) -> () {
        for slot in cpuset.__bits.iter_mut() {
            *slot = 0;
        }
    }

    pub fn CPU_SET(cpu: usize, cpuset: &mut cpu_set_t) -> () {
        let size_in___bits = 8 * mem::size_of_val(&cpuset.__bits[0]);
        let (idx, offset) = (cpu / size_in___bits, cpu % size_in___bits);
        cpuset.__bits[idx] |= 1 << offset;
        ()
    }

    pub fn CPU_CLR(cpu: usize, cpuset: &mut cpu_set_t) -> () {
        let size_in___bits = 8 * mem::size_of_val(&cpuset.__bits[0]);
        let (idx, offset) = (cpu / size_in___bits, cpu % size_in___bits);
        cpuset.__bits[idx] &= !(1 << offset);
        ()
    }

    pub fn CPU_ISSET(cpu: usize, cpuset: &cpu_set_t) -> bool {
        let size_in___bits = 8 * mem::size_of_val(&cpuset.__bits[0]);
        let (idx, offset) = (cpu / size_in___bits, cpu % size_in___bits);
        0 != (cpuset.__bits[idx] & (1 << offset))
    }

    pub fn CPU_EQUAL(set1: &cpu_set_t, set2: &cpu_set_t) -> bool {
        set1.__bits == set2.__bits
    }
    pub fn major(dev: ::dev_t) -> ::c_int {
        ((dev >> 8) & 0xfff) as ::c_int
    }
    pub fn minor(dev: ::dev_t) -> ::c_int {
        ((dev & 0xff) | ((dev >> 12) & 0xfff00)) as ::c_int
    }
    pub fn makedev(ma: ::c_int, mi: ::c_int) -> ::dev_t {
        let ma = ma as ::dev_t;
        let mi = mi as ::dev_t;
        ((ma & 0xfff) << 8) | (mi & 0xff) | ((mi & 0xfff00) << 12)
    }
}

extern {
    static mut __progname: *mut ::c_char;
}

extern {
    pub fn madvise(addr: *const ::c_void, len: ::size_t, advice: ::c_int)
                   -> ::c_int;
    pub fn ioctl(fd: ::c_int, request: ::c_int, ...) -> ::c_int;
    pub fn msync(addr: *const ::c_void, len: ::size_t,
                 flags: ::c_int) -> ::c_int;
    pub fn mprotect(addr: *const ::c_void, len: ::size_t, prot: ::c_int)
                    -> ::c_int;
    pub fn recvfrom(socket: ::c_int, buf: *mut ::c_void, len: ::size_t,
                    flags: ::c_int, addr: *const ::sockaddr,
                    addrlen: *mut ::socklen_t) -> ::ssize_t;
    pub fn getnameinfo(sa: *const ::sockaddr,
                       salen: ::socklen_t,
                       host: *mut ::c_char,
                       hostlen: ::size_t,
                       serv: *mut ::c_char,
                       sevlen: ::size_t,
                       flags: ::c_int) -> ::c_int;
    pub fn ptrace(request: ::c_int, ...) -> ::c_long;
    pub fn getpriority(which: ::c_int, who: ::c_int) -> ::c_int;
    pub fn setpriority(which: ::c_int, who: ::c_int, prio: ::c_int) -> ::c_int;
    pub fn __sched_cpualloc(count: ::size_t) -> *mut ::cpu_set_t;
    pub fn __sched_cpufree(set: *mut ::cpu_set_t);
    pub fn __sched_cpucount(setsize: ::size_t, set: *mut cpu_set_t) -> ::c_int;
    pub fn sched_getcpu() -> ::c_int;

    pub fn utmpname(name: *const ::c_char) -> ::c_int;
    pub fn setutent();
    pub fn getutent() -> *mut utmp;

    pub fn posix_fallocate(fd: ::c_int, offset: ::off_t,
                           len: ::off_t) -> ::c_int;
    pub fn signalfd(fd: ::c_int, mask: *const ::sigset_t, flags: ::c_int)
                    -> ::c_int;
    pub fn syscall(num: ::c_long, ...) -> ::c_long;
    pub fn sched_getaffinity(pid: ::pid_t,
                             cpusetsize: ::size_t,
                             cpuset: *mut cpu_set_t) -> ::c_int;
    pub fn sched_setaffinity(pid: ::pid_t,
                             cpusetsize: ::size_t,
                             cpuset: *const cpu_set_t) -> ::c_int;
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
    pub fn pthread_getschedparam(native: ::pthread_t,
                                 policy: *mut ::c_int,
                                 param: *mut ::sched_param) -> ::c_int;
    pub fn unshare(flags: ::c_int) -> ::c_int;
    pub fn umount(target: *const ::c_char) -> ::c_int;
    pub fn sched_get_priority_max(policy: ::c_int) -> ::c_int;
    pub fn tee(fd_in: ::c_int,
               fd_out: ::c_int,
               len: ::size_t,
               flags: ::c_uint) -> ::ssize_t;
    pub fn settimeofday(tv: *const ::timeval, tz: *const ::timezone) -> ::c_int;
    pub fn splice(fd_in: ::c_int,
                  off_in: *mut ::loff_t,
                  fd_out: ::c_int,
                  off_out: *mut ::loff_t,
                  len: ::size_t,
                  flags: ::c_uint) -> ::ssize_t;
    pub fn eventfd(init: ::c_uint, flags: ::c_int) -> ::c_int;
    pub fn sched_rr_get_interval(pid: ::pid_t, tp: *mut ::timespec) -> ::c_int;
    pub fn sem_timedwait(sem: *mut sem_t,
                         abstime: *const ::timespec) -> ::c_int;
    pub fn sched_setparam(pid: ::pid_t, param: *const ::sched_param) -> ::c_int;
    pub fn setns(fd: ::c_int, nstype: ::c_int) -> ::c_int;
    pub fn swapoff(puath: *const ::c_char) -> ::c_int;
    pub fn vmsplice(fd: ::c_int,
                    iov: *const ::iovec,
                    nr_segs: ::size_t,
                    flags: ::c_uint) -> ::ssize_t;
    pub fn mount(src: *const ::c_char,
                 target: *const ::c_char,
                 fstype: *const ::c_char,
                 flags: ::c_ulong,
                 data: *const ::c_void) -> ::c_int;
    pub fn personality(persona: ::c_ulong) -> ::c_int;
    pub fn prctl(option: ::c_int, ...) -> ::c_int;
    pub fn sched_getparam(pid: ::pid_t, param: *mut ::sched_param) -> ::c_int;
    pub fn ppoll(fds: *mut ::pollfd,
                 nfds: nfds_t,
                 timeout: *const ::timespec,
                 sigmask: *const sigset_t) -> ::c_int;
    pub fn pthread_mutex_timedlock(lock: *mut pthread_mutex_t,
                                   abstime: *const ::timespec) -> ::c_int;
    pub fn clone(cb: extern fn(*mut ::c_void) -> ::c_int,
                 child_stack: *mut ::c_void,
                 flags: ::c_int,
                 arg: *mut ::c_void, ...) -> ::c_int;
    pub fn sched_getscheduler(pid: ::pid_t) -> ::c_int;
    pub fn clock_nanosleep(clk_id: ::clockid_t,
                           flags: ::c_int,
                           rqtp: *const ::timespec,
                           rmtp:  *mut ::timespec) -> ::c_int;
    pub fn pthread_attr_getguardsize(attr: *const ::pthread_attr_t,
                                     guardsize: *mut ::size_t) -> ::c_int;
    pub fn sethostname(name: *const ::c_char, len: ::size_t) -> ::c_int;
    pub fn sched_get_priority_min(policy: ::c_int) -> ::c_int;
    pub fn pthread_condattr_getpshared(attr: *const pthread_condattr_t,
                                       pshared: *mut ::c_int) -> ::c_int;
    pub fn sysinfo(info: *mut ::sysinfo) -> ::c_int;
    pub fn umount2(target: *const ::c_char, flags: ::c_int) -> ::c_int;
    pub fn pthread_setschedparam(native: ::pthread_t,
                                 policy: ::c_int,
                                 param: *const ::sched_param) -> ::c_int;
    pub fn swapon(path: *const ::c_char, swapflags: ::c_int) -> ::c_int;
    pub fn sched_setscheduler(pid: ::pid_t,
                              policy: ::c_int,
                              param: *const ::sched_param) -> ::c_int;
    pub fn sendfile(out_fd: ::c_int,
                    in_fd: ::c_int,
                    offset: *mut off_t,
                    count: ::size_t) -> ::ssize_t;
    pub fn sigsuspend(mask: *const ::sigset_t) -> ::c_int;
    #[cfg_attr(target_os = "solaris", link_name = "__posix_getgrgid_r")]
    pub fn getgrgid_r(uid: ::uid_t,
                      grp: *mut ::group,
                      buf: *mut ::c_char,
                      buflen: ::size_t,
                      result: *mut *mut ::group) -> ::c_int;
    #[cfg_attr(all(target_os = "macos", target_arch = "x86"),
               link_name = "sigaltstack$UNIX2003")]
    #[cfg_attr(target_os = "netbsd", link_name = "__sigaltstack14")]
    pub fn sigaltstack(ss: *const stack_t,
                       oss: *mut stack_t) -> ::c_int;
    pub fn sem_close(sem: *mut sem_t) -> ::c_int;
    pub fn getdtablesize() -> ::c_int;
    #[cfg_attr(target_os = "solaris", link_name = "__posix_getgrnam_r")]
    pub fn getgrnam_r(name: *const ::c_char,
                      grp: *mut ::group,
                      buf: *mut ::c_char,
                      buflen: ::size_t,
                      result: *mut *mut ::group) -> ::c_int;
    #[cfg_attr(all(target_os = "macos", target_arch = "x86"),
               link_name = "pthread_sigmask$UNIX2003")]
    pub fn pthread_sigmask(how: ::c_int, set: *const sigset_t,
                           oldset: *mut sigset_t) -> ::c_int;
    pub fn sem_open(name: *const ::c_char, oflag: ::c_int, ...) -> *mut sem_t;
    pub fn getgrnam(name: *const ::c_char) -> *mut ::group;
    pub fn pthread_kill(thread: ::pthread_t, sig: ::c_int) -> ::c_int;
    pub fn sem_unlink(name: *const ::c_char) -> ::c_int;
    pub fn daemon(nochdir: ::c_int, noclose: ::c_int) -> ::c_int;
    #[cfg_attr(target_os = "netbsd", link_name = "__getpwnam_r50")]
    #[cfg_attr(target_os = "solaris", link_name = "__posix_getpwnam_r")]
    pub fn getpwnam_r(name: *const ::c_char,
                      pwd: *mut passwd,
                      buf: *mut ::c_char,
                      buflen: ::size_t,
                      result: *mut *mut passwd) -> ::c_int;
    #[cfg_attr(target_os = "netbsd", link_name = "__getpwuid_r50")]
    #[cfg_attr(target_os = "solaris", link_name = "__posix_getpwuid_r")]
    pub fn getpwuid_r(uid: ::uid_t,
                      pwd: *mut passwd,
                      buf: *mut ::c_char,
                      buflen: ::size_t,
                      result: *mut *mut passwd) -> ::c_int;
    #[cfg_attr(all(target_os = "macos", target_arch ="x86"),
               link_name = "sigwait$UNIX2003")]
    #[cfg_attr(target_os = "solaris", link_name = "__posix_sigwait")]
    pub fn sigwait(set: *const sigset_t,
                   sig: *mut ::c_int) -> ::c_int;
    pub fn pthread_atfork(prepare: Option<unsafe extern fn()>,
                          parent: Option<unsafe extern fn()>,
                          child: Option<unsafe extern fn()>) -> ::c_int;
    pub fn getgrgid(gid: ::gid_t) -> *mut ::group;
    pub fn getgrouplist(user: *const ::c_char,
                        group: ::gid_t,
                        groups: *mut ::gid_t,
                        ngroups: *mut ::c_int) -> ::c_int;
    pub fn pthread_mutexattr_getpshared(attr: *const pthread_mutexattr_t,
                                        pshared: *mut ::c_int) -> ::c_int;
    #[cfg_attr(all(target_os = "macos", target_arch = "x86"),
               link_name = "popen$UNIX2003")]
    pub fn popen(command: *const c_char,
                 mode: *const c_char) -> *mut ::FILE;
    pub fn faccessat(dirfd: ::c_int, pathname: *const ::c_char,
                     mode: ::c_int, flags: ::c_int) -> ::c_int;
    pub fn pthread_create(native: *mut ::pthread_t,
                          attr: *const ::pthread_attr_t,
                          f: extern fn(*mut ::c_void) -> *mut ::c_void,
                          value: *mut ::c_void) -> ::c_int;
}

cfg_if! {
    if #[cfg(target_pointer_width = "32")] {
        mod b32;
        pub use self::b32::*;
    } else if #[cfg(target_pointer_width = "64")] {
        mod b64;
        pub use self::b64::*;
    } else {
        // Unknown target_pointer_width
    }
}
