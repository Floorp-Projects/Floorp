//! SPARC64-specific definitions for 64-bit linux-like values

pub type c_char = i8;
pub type wchar_t = i32;
pub type nlink_t = u32;
pub type blksize_t = i64;
pub type suseconds_t = i32;

s! {
    pub struct stat {
        pub st_dev: ::dev_t,
        __pad0: u64,
        pub st_ino: ::ino_t,
        pub st_mode: ::mode_t,
        pub st_nlink: ::nlink_t,
        pub st_uid: ::uid_t,
        pub st_gid: ::gid_t,
        pub st_rdev: ::dev_t,
        __pad1: u64,
        pub st_size: ::off_t,
        pub st_blksize: ::blksize_t,
        pub st_blocks: ::blkcnt_t,
        pub st_atime: ::time_t,
        pub st_atime_nsec: ::c_long,
        pub st_mtime: ::time_t,
        pub st_mtime_nsec: ::c_long,
        pub st_ctime: ::time_t,
        pub st_ctime_nsec: ::c_long,
        __unused: [::c_long; 2],
    }

    pub struct stat64 {
        pub st_dev: ::dev_t,
        __pad0: u64,
        pub st_ino: ::ino64_t,
        pub st_mode: ::mode_t,
        pub st_nlink: ::nlink_t,
        pub st_uid: ::uid_t,
        pub st_gid: ::gid_t,
        pub st_rdev: ::dev_t,
        __pad2: ::c_int,
        pub st_size: ::off64_t,
        pub st_blksize: ::blksize_t,
        pub st_blocks: ::blkcnt64_t,
        pub st_atime: ::time_t,
        pub st_atime_nsec: ::c_long,
        pub st_mtime: ::time_t,
        pub st_mtime_nsec: ::c_long,
        pub st_ctime: ::time_t,
        pub st_ctime_nsec: ::c_long,
        __reserved: [::c_long; 2],
    }

    pub struct pthread_attr_t {
        __size: [u64; 7]
    }

    pub struct ipc_perm {
        pub __key: ::key_t,
        pub uid: ::uid_t,
        pub gid: ::gid_t,
        pub cuid: ::uid_t,
        pub cgid: ::gid_t,
        pub mode: ::mode_t,
        __pad0: u16,
        pub __seq: ::c_ushort,
        __unused1: ::c_ulonglong,
        __unused2: ::c_ulonglong,
    }

    pub struct shmid_ds {
        pub shm_perm: ::ipc_perm,
        pub shm_atime: ::time_t,
        pub shm_dtime: ::time_t,
        pub shm_ctime: ::time_t,
        pub shm_segsz: ::size_t,
        pub shm_cpid: ::pid_t,
        pub shm_lpid: ::pid_t,
        pub shm_nattch: ::shmatt_t,
        __reserved1: ::c_ulong,
        __reserved2: ::c_ulong
    }
}

pub const TIOCGSOFTCAR: ::c_ulong = 0x40047464;
pub const TIOCSSOFTCAR: ::c_ulong = 0x80047465;

pub const RLIMIT_NOFILE: ::c_int = 6;
pub const RLIMIT_NPROC: ::c_int = 7;

pub const O_APPEND: ::c_int = 0x8;
pub const O_CREAT: ::c_int = 0x200;
pub const O_EXCL: ::c_int = 0x800;
pub const O_NOCTTY: ::c_int = 0x8000;
pub const O_NONBLOCK: ::c_int = 0x4000;
pub const O_SYNC: ::c_int = 0x802000;
pub const O_RSYNC: ::c_int = 0x802000;
pub const O_DSYNC: ::c_int = 0x2000;
pub const O_FSYNC: ::c_int = 0x802000;

pub const MAP_GROWSDOWN: ::c_int = 0x0200;

pub const EDEADLK: ::c_int = 78;
pub const ENAMETOOLONG: ::c_int = 63;
pub const ENOLCK: ::c_int = 79;
pub const ENOSYS: ::c_int = 90;
pub const ENOTEMPTY: ::c_int = 66;
pub const ELOOP: ::c_int = 62;
pub const ENOMSG: ::c_int = 75;
pub const EIDRM: ::c_int = 77;
pub const ECHRNG: ::c_int = 94;
pub const EL2NSYNC: ::c_int = 95;
pub const EL3HLT: ::c_int = 96;
pub const EL3RST: ::c_int = 97;
pub const ELNRNG: ::c_int = 98;
pub const EUNATCH: ::c_int = 99;
pub const ENOCSI: ::c_int = 100;
pub const EL2HLT: ::c_int = 101;
pub const EBADE: ::c_int = 102;
pub const EBADR: ::c_int = 103;
pub const EXFULL: ::c_int = 104;
pub const ENOANO: ::c_int = 105;
pub const EBADRQC: ::c_int = 106;
pub const EBADSLT: ::c_int = 107;
pub const EMULTIHOP: ::c_int = 87;
pub const EOVERFLOW: ::c_int = 92;
pub const ENOTUNIQ: ::c_int = 115;
pub const EBADFD: ::c_int = 93;
pub const EBADMSG: ::c_int = 76;
pub const EREMCHG: ::c_int = 89;
pub const ELIBACC: ::c_int = 114;
pub const ELIBBAD: ::c_int = 112;
pub const ELIBSCN: ::c_int = 124;
pub const ELIBMAX: ::c_int = 123;
pub const ELIBEXEC: ::c_int = 110;
pub const EILSEQ: ::c_int = 122;
pub const ERESTART: ::c_int = 116;
pub const ESTRPIPE: ::c_int = 91;
pub const EUSERS: ::c_int = 68;
pub const ENOTSOCK: ::c_int = 38;
pub const EDESTADDRREQ: ::c_int = 39;
pub const EMSGSIZE: ::c_int = 40;
pub const EPROTOTYPE: ::c_int = 41;
pub const ENOPROTOOPT: ::c_int = 42;
pub const EPROTONOSUPPORT: ::c_int = 43;
pub const ESOCKTNOSUPPORT: ::c_int = 44;
pub const EOPNOTSUPP: ::c_int = 45;
pub const EPFNOSUPPORT: ::c_int = 46;
pub const EAFNOSUPPORT: ::c_int = 47;
pub const EADDRINUSE: ::c_int = 48;
pub const EADDRNOTAVAIL: ::c_int = 49;
pub const ENETDOWN: ::c_int = 50;
pub const ENETUNREACH: ::c_int = 51;
pub const ENETRESET: ::c_int = 52;
pub const ECONNABORTED: ::c_int = 53;
pub const ECONNRESET: ::c_int = 54;
pub const ENOBUFS: ::c_int = 55;
pub const EISCONN: ::c_int = 56;
pub const ENOTCONN: ::c_int = 57;
pub const ESHUTDOWN: ::c_int = 58;
pub const ETOOMANYREFS: ::c_int = 59;
pub const ETIMEDOUT: ::c_int = 60;
pub const ECONNREFUSED: ::c_int = 61;
pub const EHOSTDOWN: ::c_int = 64;
pub const EHOSTUNREACH: ::c_int = 65;
pub const EALREADY: ::c_int = 37;
pub const EINPROGRESS: ::c_int = 36;
pub const ESTALE: ::c_int = 70;
pub const EDQUOT: ::c_int = 69;
pub const ENOMEDIUM: ::c_int = 125;
pub const EMEDIUMTYPE: ::c_int = 126;
pub const ECANCELED: ::c_int = 127;
pub const ENOKEY: ::c_int = 128;
pub const EKEYEXPIRED: ::c_int = 129;
pub const EKEYREVOKED: ::c_int = 130;
pub const EKEYREJECTED: ::c_int = 131;
pub const EOWNERDEAD: ::c_int = 132;
pub const ENOTRECOVERABLE: ::c_int = 133;
pub const EHWPOISON: ::c_int = 135;
pub const ERFKILL: ::c_int = 134;

pub const SOL_SOCKET: ::c_int = 0xffff;

pub const SO_REUSEADDR: ::c_int = 4;
pub const SO_TYPE: ::c_int = 0x1008;
pub const SO_ERROR: ::c_int = 0x1007;
pub const SO_DONTROUTE: ::c_int = 16;
pub const SO_BROADCAST: ::c_int = 32;
pub const SO_SNDBUF: ::c_int = 0x1001;
pub const SO_RCVBUF: ::c_int = 0x1002;
pub const SO_KEEPALIVE: ::c_int = 8;
pub const SO_OOBINLINE: ::c_int = 0x100;
pub const SO_LINGER: ::c_int = 128;
pub const SO_REUSEPORT: ::c_int = 0x200;
pub const SO_ACCEPTCONN: ::c_int = 0x8000;

pub const SA_ONSTACK: ::c_int = 1;
pub const SA_SIGINFO: ::c_int = 0x200;
pub const SA_NOCLDWAIT: ::c_int = 0x100;

pub const SIGCHLD: ::c_int = 20;
pub const SIGBUS: ::c_int = 10;
pub const SIGUSR1: ::c_int = 30;
pub const SIGUSR2: ::c_int = 31;
pub const SIGCONT: ::c_int = 19;
pub const SIGSTOP: ::c_int = 17;
pub const SIGTSTP: ::c_int = 18;
pub const SIGURG: ::c_int = 16;
pub const SIGIO: ::c_int = 23;
pub const SIGSYS: ::c_int = 12;
pub const SIGPOLL: ::c_int = 23;
pub const SIGPWR: ::c_int = 29;
pub const SIG_SETMASK: ::c_int = 4;
pub const SIG_BLOCK: ::c_int = 1;
pub const SIG_UNBLOCK: ::c_int = 2;

pub const POLLWRNORM: ::c_short = 4;
pub const POLLWRBAND: ::c_short = 0x100;

pub const O_ASYNC: ::c_int = 0x40;
pub const O_NDELAY: ::c_int = 0x4004;

pub const PTRACE_DETACH: ::c_uint = 11;

pub const EFD_NONBLOCK: ::c_int = 0x4000;

pub const F_GETLK: ::c_int = 7;
pub const F_GETOWN: ::c_int = 5;
pub const F_SETOWN: ::c_int = 6;
pub const F_SETLK: ::c_int = 8;
pub const F_SETLKW: ::c_int = 9;

pub const SFD_NONBLOCK: ::c_int = 0x4000;

pub const TIOCEXCL: ::c_ulong = 0x2000740d;
pub const TIOCNXCL: ::c_ulong = 0x2000740e;
pub const TIOCSCTTY: ::c_ulong = 0x20007484;
pub const TIOCSTI: ::c_ulong = 0x80017472;
pub const TIOCMGET: ::c_ulong = 0x4004746a;
pub const TIOCMBIS: ::c_ulong = 0x8004746c;
pub const TIOCMBIC: ::c_ulong = 0x8004746b;
pub const TIOCMSET: ::c_ulong = 0x8004746d;
pub const TIOCCONS: ::c_ulong = 0x20007424;

pub const SFD_CLOEXEC: ::c_int = 0x400000;

pub const NCCS: usize = 17;
pub const O_TRUNC: ::c_int = 0x400;

pub const O_CLOEXEC: ::c_int = 0x400000;

pub const EBFONT: ::c_int = 109;
pub const ENOSTR: ::c_int = 72;
pub const ENODATA: ::c_int = 111;
pub const ETIME: ::c_int = 73;
pub const ENOSR: ::c_int = 74;
pub const ENONET: ::c_int = 80;
pub const ENOPKG: ::c_int = 113;
pub const EREMOTE: ::c_int = 71;
pub const ENOLINK: ::c_int = 82;
pub const EADV: ::c_int = 83;
pub const ESRMNT: ::c_int = 84;
pub const ECOMM: ::c_int = 85;
pub const EPROTO: ::c_int = 86;
pub const EDOTDOT: ::c_int = 88;

pub const SA_NODEFER: ::c_int = 0x20;
pub const SA_RESETHAND: ::c_int = 0x4;
pub const SA_RESTART: ::c_int = 0x2;
pub const SA_NOCLDSTOP: ::c_int = 0x00000008;

pub const EPOLL_CLOEXEC: ::c_int = 0x400000;

pub const EFD_CLOEXEC: ::c_int = 0x400000;
pub const __SIZEOF_PTHREAD_CONDATTR_T: usize = 4;
pub const __SIZEOF_PTHREAD_MUTEX_T: usize = 40;
pub const __SIZEOF_PTHREAD_MUTEXATTR_T: usize = 4;

pub const O_DIRECTORY: ::c_int = 0o200000;
pub const O_NOFOLLOW: ::c_int = 0o400000;
pub const O_DIRECT: ::c_int = 0x100000;

pub const MAP_LOCKED: ::c_int = 0x0100;
pub const MAP_NORESERVE: ::c_int = 0x00040;

pub const EDEADLOCK: ::c_int = 108;

pub const SO_PEERCRED: ::c_int = 0x40;
pub const SO_RCVLOWAT: ::c_int = 0x800;
pub const SO_SNDLOWAT: ::c_int = 0x1000;
pub const SO_RCVTIMEO: ::c_int = 0x2000;
pub const SO_SNDTIMEO: ::c_int = 0x4000;

pub const FIOCLEX: ::c_ulong = 0x20006601;
pub const FIONBIO: ::c_ulong = 0x8004667e;

pub const SYS_gettid: ::c_long = 143;
pub const SYS_perf_event_open: ::c_long = 327;

pub const MCL_CURRENT: ::c_int = 0x2000;
pub const MCL_FUTURE: ::c_int = 0x4000;

pub const SIGSTKSZ: ::size_t = 16384;
pub const CBAUD: ::tcflag_t = 0x0000100f;
pub const TAB1: ::c_int = 0x800;
pub const TAB2: ::c_int = 0x1000;
pub const TAB3: ::c_int = 0x1800;
pub const CR1: ::c_int  = 0x200;
pub const CR2: ::c_int  = 0x400;
pub const CR3: ::c_int  = 0x600;
pub const FF1: ::c_int  = 0x8000;
pub const BS1: ::c_int  = 0x2000;
pub const VT1: ::c_int  = 0x4000;
pub const VWERASE: usize = 0xe;
pub const VREPRINT: usize = 0xc;
pub const VSUSP: usize = 0xa;
pub const VSTART: usize = 0x8;
pub const VSTOP: usize = 0x9;
pub const VDISCARD: usize = 0xd;
pub const VTIME: usize = 0x5;
pub const IXON: ::tcflag_t = 0x400;
pub const IXOFF: ::tcflag_t = 0x1000;
pub const ONLCR: ::tcflag_t = 0x4;
pub const CSIZE: ::tcflag_t = 0x30;
pub const CS6: ::tcflag_t = 0x10;
pub const CS7: ::tcflag_t = 0x20;
pub const CS8: ::tcflag_t = 0x30;
pub const CSTOPB: ::tcflag_t = 0x40;
pub const CREAD: ::tcflag_t = 0x80;
pub const PARENB: ::tcflag_t = 0x100;
pub const PARODD: ::tcflag_t = 0x200;
pub const HUPCL: ::tcflag_t = 0x400;
pub const CLOCAL: ::tcflag_t = 0x800;
pub const ECHOKE: ::tcflag_t = 0x800;
pub const ECHOE: ::tcflag_t = 0x10;
pub const ECHOK: ::tcflag_t = 0x20;
pub const ECHONL: ::tcflag_t = 0x40;
pub const ECHOPRT: ::tcflag_t = 0x400;
pub const ECHOCTL: ::tcflag_t = 0x200;
pub const ISIG: ::tcflag_t = 0x1;
pub const ICANON: ::tcflag_t = 0x2;
pub const PENDIN: ::tcflag_t = 0x4000;
pub const NOFLSH: ::tcflag_t = 0x80;

pub const VEOL: usize = 5;
pub const VEOL2: usize = 6;
pub const VMIN: usize = 4;
pub const IEXTEN: ::tcflag_t = 0x8000;
pub const TOSTOP: ::tcflag_t = 0x100;
pub const FLUSHO: ::tcflag_t = 0x2000;
pub const EXTPROC: ::tcflag_t = 0x10000;
pub const TCGETS: ::c_ulong = 0x40245408;
pub const TCSETS: ::c_ulong = 0x80245409;
pub const TCSETSW: ::c_ulong = 0x8024540a;
pub const TCSETSF: ::c_ulong = 0x8024540b;
pub const TCGETA: ::c_ulong = 0x40125401;
pub const TCSETA: ::c_ulong = 0x80125402;
pub const TCSETAW: ::c_ulong = 0x80125403;
pub const TCSETAF: ::c_ulong = 0x80125404;
pub const TCSBRK: ::c_ulong = 0x20005405;
pub const TCXONC: ::c_ulong = 0x20005406;
pub const TCFLSH: ::c_ulong = 0x20005407;
pub const TIOCINQ: ::c_ulong = 0x4004667f;
pub const TIOCGPGRP: ::c_ulong = 0x40047483;
pub const TIOCSPGRP: ::c_ulong = 0x80047482;
pub const TIOCOUTQ: ::c_ulong = 0x40047473;
pub const TIOCGWINSZ: ::c_ulong = 0x40087468;
pub const TIOCSWINSZ: ::c_ulong = 0x80087467;
pub const FIONREAD: ::c_ulong = 0x4004667f;
