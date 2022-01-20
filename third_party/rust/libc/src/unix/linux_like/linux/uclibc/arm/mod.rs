pub type c_char = u8;
pub type wchar_t = ::c_uint;
pub type c_long = i32;
pub type c_ulong = u32;
pub type time_t = ::c_long;

pub type clock_t = ::c_long;
pub type fsblkcnt_t = ::c_ulong;
pub type fsfilcnt_t = ::c_ulong;
pub type ino_t = ::c_ulong;
pub type off_t = ::c_long;
pub type pthread_t = ::c_ulong;
pub type rlim_t = ::c_ulong;
pub type suseconds_t = ::c_long;

pub type nlink_t = ::c_uint;
pub type blksize_t = ::c_long;
pub type blkcnt_t = ::c_long;

pub type fsblkcnt64_t = u64;
pub type fsfilcnt64_t = u64;
pub type __u64 = ::c_ulonglong;

s! {
    pub struct cmsghdr {
        pub cmsg_len: ::size_t,
        pub cmsg_level: ::c_int,
        pub cmsg_type: ::c_int,
    }

    pub struct msghdr {
        pub msg_name: *mut ::c_void,
        pub msg_namelen: ::socklen_t,
        pub msg_iov: *mut ::iovec,
        pub msg_iovlen: ::c_int,
        pub msg_control: *mut ::c_void,
        pub msg_controllen: ::socklen_t,
        pub msg_flags: ::c_int,
    }

    pub struct pthread_attr_t {
        __size: [::c_long; 9],
    }

    pub struct stat {
        pub st_dev: ::c_ulonglong,
        pub __pad1: ::c_ushort,
        pub st_ino: ::ino_t,
        pub st_mode: ::mode_t,
        pub st_nlink: ::nlink_t,
        pub st_uid: ::uid_t,
        pub st_gid: ::gid_t,
        pub st_rdev: ::c_ulonglong,
        pub __pad2: ::c_ushort,
        pub st_size: ::off_t,
        pub st_blksize: ::blksize_t,
        pub st_blocks: ::blkcnt_t,
        pub st_atime: ::time_t,
        pub st_atime_nsec: ::c_long,
        pub st_mtime: ::time_t,
        pub st_mtime_nsec: ::c_long,
        pub st_ctime: ::time_t,
        pub st_ctime_nsec: ::c_long,
        pub __uclibc_unused4: ::c_ulong,
        pub __uclibc_unused5: ::c_ulong,
    }

    pub struct stat64
    {
        pub st_dev: ::c_ulonglong,
        pub __pad1: ::c_uint,
        pub __st_ino: ::ino_t,
        pub st_mode: ::mode_t,
        pub st_nlink: ::nlink_t,
        pub st_uid: ::uid_t,
        pub st_gid: ::gid_t,
        pub st_rdev: ::c_ulonglong,
        pub __pad2: ::c_uint,
        pub st_size: ::off64_t,
        pub st_blksize: ::blksize_t,
        pub st_blocks: ::blkcnt64_t,
        pub st_atime: ::time_t,
        pub st_atime_nsec: ::c_long,
        pub st_mtime: ::time_t,
        pub st_mtime_nsec: ::c_long,
        pub st_ctime: ::time_t,
        pub st_ctime_nsec: ::c_long,
        pub st_ino: ::ino64_t,
    }

    pub struct flock {
        pub l_type: ::c_short,
        pub l_whence: ::c_short,
        pub l_start: ::off_t,
        pub l_len: ::off_t,
        pub l_pid: ::pid_t,
    }

    pub struct sysinfo {
        pub uptime: ::c_long,
        pub loads: [::c_ulong; 3],
        pub totalram: ::c_ulong,
        pub freeram: ::c_ulong,
        pub sharedram: ::c_ulong,
        pub bufferram: ::c_ulong,
        pub totalswap: ::c_ulong,
        pub freeswap: ::c_ulong,
        pub procs: ::c_ushort,
        pub pad: ::c_ushort,
        pub totalhigh: ::c_ulong,
        pub freehigh: ::c_ulong,
        pub mem_unit: ::c_uint,
        pub _f: [::c_char; 8],
    }

    pub struct statfs {
        pub f_type: ::c_int,
        pub f_bsize: ::c_int,
        pub f_blocks: ::fsblkcnt_t,
        pub f_bfree: ::fsblkcnt_t,
        pub f_bavail: ::fsblkcnt_t,
        pub f_files: ::fsfilcnt_t,
        pub f_ffree: ::fsfilcnt_t,

        pub f_fsid: ::fsid_t,
        pub f_namelen: ::c_int,
        pub f_frsize: ::c_int,
        pub f_flags: ::c_int,
        pub f_spare: [::c_int; 4],
    }

    pub struct statfs64 {
        pub f_type: ::c_int,
        pub f_bsize: ::c_int,
        pub f_blocks: ::fsblkcnt64_t,
        pub f_bfree: ::fsblkcnt64_t,
        pub f_bavail: ::fsblkcnt64_t,
        pub f_files: ::fsfilcnt64_t,
        pub f_ffree: ::fsfilcnt64_t,
        pub f_fsid: ::fsid_t,
        pub f_namelen: ::c_int,
        pub f_frsize: ::c_int,
        pub f_flags: ::c_int,
        pub f_spare: [::c_int; 4],
    }

    pub struct statvfs64 {
        pub f_bsize: ::c_ulong,
        pub f_frsize: ::c_ulong,
        pub f_blocks: u64,
        pub f_bfree: u64,
        pub f_bavail: u64,
        pub f_files: u64,
        pub f_ffree: u64,
        pub f_favail: u64,
        pub f_fsid: ::c_ulong,
        __f_unused: ::c_int,
        pub f_flag: ::c_ulong,
        pub f_namemax: ::c_ulong,
        __f_spare: [::c_int; 6],
    }

    pub struct sigset_t {
        __val: [::c_ulong; 2],
    }

    pub struct sigaction {
        pub sa_sigaction: ::sighandler_t,
        pub sa_flags: ::c_ulong,
        pub sa_restorer: ::Option<extern fn()>,
        pub sa_mask: sigset_t,
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

    pub struct siginfo_t {
        pub si_signo: ::c_int,
        pub si_errno: ::c_int,
        pub si_code: ::c_int,
        pub _pad: [::c_int; 29],
    }

    pub struct stack_t {
        pub ss_sp: *mut ::c_void,
        pub ss_flags: ::c_int,
        pub ss_size: ::size_t,
    }

    pub struct ipc_perm {
        pub __key: ::key_t,
        pub uid: ::uid_t,
        pub gid: ::gid_t,
        pub cuid: ::uid_t,
        pub cgid: ::gid_t,
        pub mode: ::c_ushort,
        pub __pad1: ::c_ushort,
        pub __seq: ::c_ushort,
        pub __pad2: ::c_ushort,
        pub __uclibc_unused1: ::c_ulong,
        pub __uclibc_unused2: ::c_ulong,
    }

    pub struct msqid_ds {
        pub msg_perm: ::ipc_perm,
        pub msg_stime: ::time_t,
        pub __uclibc_unused1: ::c_ulong,
        pub msg_rtime: ::time_t,
        pub __uclibc_unused2: ::c_ulong,
        pub msg_ctime: ::time_t,
        pub __uclibc_unused3: ::c_ulong,
        pub __msg_cbytes: ::c_ulong,
        pub msg_qnum: ::msgqnum_t,
        pub msg_qbytes: ::msglen_t,
        pub msg_lspid: ::pid_t,
        pub msg_lrpid: ::pid_t,
        pub __uclibc_unused4: ::c_ulong,
        pub __uclibc_unused5: ::c_ulong,
    }

    pub struct shmid_ds {
        pub shm_perm: ::ipc_perm,
        pub shm_segsz: ::size_t,
        pub shm_atime: ::time_t,
        pub __uclibc_unused1: ::c_ulong,
        pub shm_dtime: ::time_t,
        pub __uclibc_unused2: ::c_ulong,
        pub shm_ctime: ::time_t,
        pub __uclibc_unused3: ::c_ulong,
        pub shm_cpid: ::pid_t,
        pub shm_lpid: ::pid_t,
        pub shm_nattch: ::shmatt_t,
        pub __uclibc_unused4: ::c_ulong,
        pub __uclibc_unused5: ::c_ulong,
    }
}

pub const O_CLOEXEC: ::c_int = 0o2000000;
pub const RLIM_INFINITY: rlim_t = !0;
pub const __SIZEOF_PTHREAD_ATTR_T: usize = 36;
pub const __SIZEOF_PTHREAD_MUTEX_T: usize = 24;
pub const __SIZEOF_PTHREAD_MUTEXATTR_T: usize = 4;
pub const __SIZEOF_PTHREAD_COND_COMPAT_T: usize = 12;
pub const __SIZEOF_PTHREAD_CONDATTR_T: usize = 4;
pub const __SIZEOF_PTHREAD_RWLOCK_T: usize = 32;
pub const __SIZEOF_PTHREAD_RWLOCKATTR_T: usize = 8;
pub const __SIZEOF_PTHREAD_BARRIER_T: usize = 20;
pub const __SIZEOF_PTHREAD_BARRIERATTR_T: usize = 4;
pub const NCCS: usize = 32;

// I wasn't able to find those constants
// in uclibc build environment for armv7
pub const MAP_HUGETLB: ::c_int = 0x040000; // from linux/other/mod.rs

// autogenerated constants with hand tuned types
pub const B0: ::speed_t = 0;
pub const B1000000: ::speed_t = 0x1008;
pub const B110: ::speed_t = 0x3;
pub const B115200: ::speed_t = 0x1002;
pub const B1152000: ::speed_t = 0x1009;
pub const B1200: ::speed_t = 0x9;
pub const B134: ::speed_t = 0x4;
pub const B150: ::speed_t = 0x5;
pub const B1500000: ::speed_t = 0x100a;
pub const B1800: ::speed_t = 0xa;
pub const B19200: ::speed_t = 0xe;
pub const B200: ::speed_t = 0x6;
pub const B2000000: ::speed_t = 0x100b;
pub const B230400: ::speed_t = 0x1003;
pub const B2400: ::speed_t = 0xb;
pub const B2500000: ::speed_t = 0x100c;
pub const B300: ::speed_t = 0x7;
pub const B3000000: ::speed_t = 0x100d;
pub const B3500000: ::speed_t = 0x100e;
pub const B38400: ::speed_t = 0xf;
pub const B4000000: ::speed_t = 0x100f;
pub const B460800: ::speed_t = 0x1004;
pub const B4800: ::speed_t = 0xc;
pub const B50: ::speed_t = 0x1;
pub const B500000: ::speed_t = 0x1005;
pub const B57600: ::speed_t = 0x1001;
pub const B576000: ::speed_t = 0x1006;
pub const B600: ::speed_t = 0x8;
pub const B75: ::speed_t = 0x2;
pub const B921600: ::speed_t = 0x1007;
pub const B9600: ::speed_t = 0xd;
pub const BS1: ::c_int = 0x2000;
pub const BSDLY: ::c_int = 0x2000;
pub const CBAUD: ::tcflag_t = 0x100f;
pub const CBAUDEX: ::tcflag_t = 0x1000;
pub const CIBAUD: ::tcflag_t = 0x100f0000;
pub const CLOCAL: ::tcflag_t = 0x800;
pub const CPU_SETSIZE: ::c_int = 0x400;
pub const CR1: ::c_int = 0x200;
pub const CR2: ::c_int = 0x400;
pub const CR3: ::c_int = 0x600;
pub const CRDLY: ::c_int = 0x600;
pub const CREAD: ::tcflag_t = 0x80;
pub const CS6: ::tcflag_t = 0x10;
pub const CS7: ::tcflag_t = 0x20;
pub const CS8: ::tcflag_t = 0x30;
pub const CSIZE: ::tcflag_t = 0x30;
pub const CSTOPB: ::tcflag_t = 0x40;
pub const EADDRINUSE: ::c_int = 0x62;
pub const EADDRNOTAVAIL: ::c_int = 0x63;
pub const EADV: ::c_int = 0x44;
pub const EAFNOSUPPORT: ::c_int = 0x61;
pub const EALREADY: ::c_int = 0x72;
pub const EBADE: ::c_int = 0x34;
pub const EBADFD: ::c_int = 0x4d;
pub const EBADMSG: ::c_int = 0x4a;
pub const EBADR: ::c_int = 0x35;
pub const EBADRQC: ::c_int = 0x38;
pub const EBADSLT: ::c_int = 0x39;
pub const EBFONT: ::c_int = 0x3b;
pub const ECANCELED: ::c_int = 0x7d;
pub const ECHOCTL: ::tcflag_t = 0x200;
pub const ECHOE: ::tcflag_t = 0x10;
pub const ECHOK: ::tcflag_t = 0x20;
pub const ECHOKE: ::tcflag_t = 0x800;
pub const ECHONL: ::tcflag_t = 0x40;
pub const ECHOPRT: ::tcflag_t = 0x400;
pub const ECHRNG: ::c_int = 0x2c;
pub const ECOMM: ::c_int = 0x46;
pub const ECONNABORTED: ::c_int = 0x67;
pub const ECONNREFUSED: ::c_int = 0x6f;
pub const ECONNRESET: ::c_int = 0x68;
pub const EDEADLK: ::c_int = 0x23;
pub const EDESTADDRREQ: ::c_int = 0x59;
pub const EDOTDOT: ::c_int = 0x49;
pub const EDQUOT: ::c_int = 0x7a;
pub const EFD_CLOEXEC: ::c_int = 0x80000;
pub const EFD_NONBLOCK: ::c_int = 0x800;
pub const EHOSTDOWN: ::c_int = 0x70;
pub const EHOSTUNREACH: ::c_int = 0x71;
pub const EHWPOISON: ::c_int = 0x85;
pub const EIDRM: ::c_int = 0x2b;
pub const EILSEQ: ::c_int = 0x54;
pub const EINPROGRESS: ::c_int = 0x73;
pub const EISCONN: ::c_int = 0x6a;
pub const EISNAM: ::c_int = 0x78;
pub const EKEYEXPIRED: ::c_int = 0x7f;
pub const EKEYREJECTED: ::c_int = 0x81;
pub const EKEYREVOKED: ::c_int = 0x80;
pub const EL2HLT: ::c_int = 0x33;
pub const EL2NSYNC: ::c_int = 0x2d;
pub const EL3HLT: ::c_int = 0x2e;
pub const EL3RST: ::c_int = 0x2f;
pub const ELIBACC: ::c_int = 0x4f;
pub const ELIBBAD: ::c_int = 0x50;
pub const ELIBEXEC: ::c_int = 0x53;
pub const ELIBMAX: ::c_int = 0x52;
pub const ELIBSCN: ::c_int = 0x51;
pub const ELNRNG: ::c_int = 0x30;
pub const ELOOP: ::c_int = 0x28;
pub const EMEDIUMTYPE: ::c_int = 0x7c;
pub const EMSGSIZE: ::c_int = 0x5a;
pub const EMULTIHOP: ::c_int = 0x48;
pub const ENAMETOOLONG: ::c_int = 0x24;
pub const ENAVAIL: ::c_int = 0x77;
pub const ENETDOWN: ::c_int = 0x64;
pub const ENETRESET: ::c_int = 0x66;
pub const ENETUNREACH: ::c_int = 0x65;
pub const ENOANO: ::c_int = 0x37;
pub const ENOBUFS: ::c_int = 0x69;
pub const ENOCSI: ::c_int = 0x32;
pub const ENODATA: ::c_int = 0x3d;
pub const ENOKEY: ::c_int = 0x7e;
pub const ENOLCK: ::c_int = 0x25;
pub const ENOLINK: ::c_int = 0x43;
pub const ENOMEDIUM: ::c_int = 0x7b;
pub const ENOMSG: ::c_int = 0x2a;
pub const ENONET: ::c_int = 0x40;
pub const ENOPKG: ::c_int = 0x41;
pub const ENOPROTOOPT: ::c_int = 0x5c;
pub const ENOSR: ::c_int = 0x3f;
pub const ENOSTR: ::c_int = 0x3c;
pub const ENOSYS: ::c_int = 0x26;
pub const ENOTCONN: ::c_int = 0x6b;
pub const ENOTEMPTY: ::c_int = 0x27;
pub const ENOTNAM: ::c_int = 0x76;
pub const ENOTRECOVERABLE: ::c_int = 0x83;
pub const ENOTSOCK: ::c_int = 0x58;
pub const ENOTUNIQ: ::c_int = 0x4c;
pub const EOPNOTSUPP: ::c_int = 0x5f;
pub const EOVERFLOW: ::c_int = 0x4b;
pub const EOWNERDEAD: ::c_int = 0x82;
pub const EPFNOSUPPORT: ::c_int = 0x60;
pub const EPOLL_CLOEXEC: ::c_int = 0x80000;
pub const EPROTO: ::c_int = 0x47;
pub const EPROTONOSUPPORT: ::c_int = 0x5d;
pub const EPROTOTYPE: ::c_int = 0x5b;
pub const EREMCHG: ::c_int = 0x4e;
pub const EREMOTE: ::c_int = 0x42;
pub const EREMOTEIO: ::c_int = 0x79;
pub const ERESTART: ::c_int = 0x55;
pub const ERFKILL: ::c_int = 0x84;
pub const ESHUTDOWN: ::c_int = 0x6c;
pub const ESOCKTNOSUPPORT: ::c_int = 0x5e;
pub const ESRMNT: ::c_int = 0x45;
pub const ESTALE: ::c_int = 0x74;
pub const ESTRPIPE: ::c_int = 0x56;
pub const ETIME: ::c_int = 0x3e;
pub const ETIMEDOUT: ::c_int = 0x6e;
pub const ETOOMANYREFS: ::c_int = 0x6d;
pub const EUCLEAN: ::c_int = 0x75;
pub const EUNATCH: ::c_int = 0x31;
pub const EUSERS: ::c_int = 0x57;
pub const EXFULL: ::c_int = 0x36;
pub const FF1: ::c_int = 0x8000;
pub const FFDLY: ::c_int = 0x8000;
pub const FIONBIO: ::c_ulong = 0x5421;
pub const FIOCLEX: ::c_ulong = 0x5451;
pub const FIONCLEX: ::c_ulong = 0x5450;
pub const FLUSHO: ::tcflag_t = 0x1000;
pub const F_GETLK: ::c_int = 0x5;
pub const F_SETLK: ::c_int = 0x6;
pub const F_SETLKW: ::c_int = 0x7;
pub const HUPCL: ::tcflag_t = 0x400;
pub const ICANON: ::tcflag_t = 0x2;
pub const IEXTEN: ::tcflag_t = 0x8000;
pub const ISIG: ::tcflag_t = 0x1;
pub const IXOFF: ::tcflag_t = 0x1000;
pub const IXON: ::tcflag_t = 0x400;
pub const MAP_ANON: ::c_int = 0x20;
pub const MAP_ANONYMOUS: ::c_int = 0x20;
pub const MAP_DENYWRITE: ::c_int = 0x800;
pub const MAP_EXECUTABLE: ::c_int = 0x1000;
pub const MAP_GROWSDOWN: ::c_int = 0x100;
pub const MAP_LOCKED: ::c_int = 0x2000;
pub const MAP_NONBLOCK: ::c_int = 0x10000;
pub const MAP_NORESERVE: ::c_int = 0x4000;
pub const MAP_POPULATE: ::c_int = 0x8000;
pub const MAP_STACK: ::c_int = 0x20000;
pub const NLDLY: ::tcflag_t = 0x100;
pub const NOFLSH: ::tcflag_t = 0x80;
pub const OLCUC: ::tcflag_t = 0x2;
pub const ONLCR: ::tcflag_t = 0x4;
pub const O_ACCMODE: ::c_int = 0x3;
pub const O_APPEND: ::c_int = 0x400;
pub const O_ASYNC: ::c_int = 0o20000;
pub const O_CREAT: ::c_int = 0x40;
pub const O_DIRECT: ::c_int = 0x10000;
pub const O_DIRECTORY: ::c_int = 0x4000;
pub const O_DSYNC: ::c_int = O_SYNC;
pub const O_EXCL: ::c_int = 0x80;
pub const O_FSYNC: ::c_int = O_SYNC;
pub const O_LARGEFILE: ::c_int = 0o400000;
pub const O_NDELAY: ::c_int = O_NONBLOCK;
pub const O_NOATIME: ::c_int = 0o1000000;
pub const O_NOCTTY: ::c_int = 0x100;
pub const O_NOFOLLOW: ::c_int = 0x8000;
pub const O_NONBLOCK: ::c_int = 0x800;
pub const O_PATH: ::c_int = 0o10000000;
pub const O_RSYNC: ::c_int = O_SYNC;
pub const O_SYNC: ::c_int = 0o10000;
pub const O_TRUNC: ::c_int = 0x200;
pub const PARENB: ::tcflag_t = 0x100;
pub const PARODD: ::tcflag_t = 0x200;
pub const PENDIN: ::tcflag_t = 0x4000;
pub const POLLWRBAND: ::c_short = 0x200;
pub const POLLWRNORM: ::c_short = 0x100;
pub const PTHREAD_STACK_MIN: ::size_t = 16384;

// These are typed unsigned to match sigaction
pub const SA_NOCLDSTOP: ::c_ulong = 0x1;
pub const SA_NOCLDWAIT: ::c_ulong = 0x2;
pub const SA_SIGINFO: ::c_ulong = 0x4;
pub const SA_NODEFER: ::c_ulong = 0x40000000;
pub const SA_ONSTACK: ::c_ulong = 0x8000000;
pub const SA_RESETHAND: ::c_ulong = 0x80000000;
pub const SA_RESTART: ::c_ulong = 0x10000000;

pub const SFD_CLOEXEC: ::c_int = 0x80000;
pub const SFD_NONBLOCK: ::c_int = 0x800;
pub const SIGBUS: ::c_int = 0x7;
pub const SIGCHLD: ::c_int = 0x11;
pub const SIGCONT: ::c_int = 0x12;
pub const SIGIO: ::c_int = 0x1d;
pub const SIGPROF: ::c_int = 0x1b;
pub const SIGPWR: ::c_int = 0x1e;
pub const SIGSTKFLT: ::c_int = 0x10;
pub const SIGSTKSZ: ::size_t = 8192;
pub const SIGSTOP: ::c_int = 0x13;
pub const SIGSYS: ::c_int = 0x1f;
pub const SIGTSTP: ::c_int = 0x14;
pub const SIGTTIN: ::c_int = 0x15;
pub const SIGTTOU: ::c_int = 0x16;
pub const SIGURG: ::c_int = 0x17;
pub const SIGUSR1: ::c_int = 0xa;
pub const SIGUSR2: ::c_int = 0xc;
pub const SIGVTALRM: ::c_int = 0x1a;
pub const SIGWINCH: ::c_int = 0x1c;
pub const SIGXCPU: ::c_int = 0x18;
pub const SIGXFSZ: ::c_int = 0x19;
pub const SIG_BLOCK: ::c_int = 0;
pub const SIG_SETMASK: ::c_int = 0x2;
pub const SIG_UNBLOCK: ::c_int = 0x1;
pub const SOCK_DGRAM: ::c_int = 0x2;
pub const SOCK_NONBLOCK: ::c_int = 0o0004000;
pub const SOCK_SEQPACKET: ::c_int = 0x5;
pub const SOCK_STREAM: ::c_int = 0x1;

pub const TAB1: ::c_int = 0x800;
pub const TAB2: ::c_int = 0x1000;
pub const TAB3: ::c_int = 0x1800;
pub const TABDLY: ::c_int = 0x1800;
pub const TCSADRAIN: ::c_int = 0x1;
pub const TCSAFLUSH: ::c_int = 0x2;
pub const TCSANOW: ::c_int = 0;
pub const TOSTOP: ::tcflag_t = 0x100;
pub const VDISCARD: usize = 0xd;
pub const VEOF: usize = 0x4;
pub const VEOL: usize = 0xb;
pub const VEOL2: usize = 0x10;
pub const VMIN: usize = 0x6;
pub const VREPRINT: usize = 0xc;
pub const VSTART: usize = 0x8;
pub const VSTOP: usize = 0x9;
pub const VSUSP: usize = 0xa;
pub const VSWTC: usize = 0x7;
pub const VT1: ::c_int = 0x4000;
pub const VTDLY: ::c_int = 0x4000;
pub const VTIME: usize = 0x5;
pub const VWERASE: usize = 0xe;
pub const XTABS: ::tcflag_t = 0x1800;

pub const MADV_SOFT_OFFLINE: ::c_int = 101;

// Syscall table is copied from src/unix/notbsd/linux/musl/b32/arm.rs
pub const SYS_restart_syscall: ::c_long = 0;
pub const SYS_exit: ::c_long = 1;
pub const SYS_fork: ::c_long = 2;
pub const SYS_read: ::c_long = 3;
pub const SYS_write: ::c_long = 4;
pub const SYS_open: ::c_long = 5;
pub const SYS_close: ::c_long = 6;
pub const SYS_creat: ::c_long = 8;
pub const SYS_link: ::c_long = 9;
pub const SYS_unlink: ::c_long = 10;
pub const SYS_execve: ::c_long = 11;
pub const SYS_chdir: ::c_long = 12;
pub const SYS_mknod: ::c_long = 14;
pub const SYS_chmod: ::c_long = 15;
pub const SYS_lchown: ::c_long = 16;
pub const SYS_lseek: ::c_long = 19;
pub const SYS_getpid: ::c_long = 20;
pub const SYS_mount: ::c_long = 21;
pub const SYS_setuid: ::c_long = 23;
pub const SYS_getuid: ::c_long = 24;
pub const SYS_ptrace: ::c_long = 26;
pub const SYS_pause: ::c_long = 29;
pub const SYS_access: ::c_long = 33;
pub const SYS_nice: ::c_long = 34;
pub const SYS_sync: ::c_long = 36;
pub const SYS_kill: ::c_long = 37;
pub const SYS_rename: ::c_long = 38;
pub const SYS_mkdir: ::c_long = 39;
pub const SYS_rmdir: ::c_long = 40;
pub const SYS_dup: ::c_long = 41;
pub const SYS_pipe: ::c_long = 42;
pub const SYS_times: ::c_long = 43;
pub const SYS_brk: ::c_long = 45;
pub const SYS_setgid: ::c_long = 46;
pub const SYS_getgid: ::c_long = 47;
pub const SYS_geteuid: ::c_long = 49;
pub const SYS_getegid: ::c_long = 50;
pub const SYS_acct: ::c_long = 51;
pub const SYS_umount2: ::c_long = 52;
pub const SYS_ioctl: ::c_long = 54;
pub const SYS_fcntl: ::c_long = 55;
pub const SYS_setpgid: ::c_long = 57;
pub const SYS_umask: ::c_long = 60;
pub const SYS_chroot: ::c_long = 61;
pub const SYS_ustat: ::c_long = 62;
pub const SYS_dup2: ::c_long = 63;
pub const SYS_getppid: ::c_long = 64;
pub const SYS_getpgrp: ::c_long = 65;
pub const SYS_setsid: ::c_long = 66;
pub const SYS_sigaction: ::c_long = 67;
pub const SYS_setreuid: ::c_long = 70;
pub const SYS_setregid: ::c_long = 71;
pub const SYS_sigsuspend: ::c_long = 72;
pub const SYS_sigpending: ::c_long = 73;
pub const SYS_sethostname: ::c_long = 74;
pub const SYS_setrlimit: ::c_long = 75;
pub const SYS_getrusage: ::c_long = 77;
pub const SYS_gettimeofday: ::c_long = 78;
pub const SYS_settimeofday: ::c_long = 79;
pub const SYS_getgroups: ::c_long = 80;
pub const SYS_setgroups: ::c_long = 81;
pub const SYS_symlink: ::c_long = 83;
pub const SYS_readlink: ::c_long = 85;
pub const SYS_uselib: ::c_long = 86;
pub const SYS_swapon: ::c_long = 87;
pub const SYS_reboot: ::c_long = 88;
pub const SYS_munmap: ::c_long = 91;
pub const SYS_truncate: ::c_long = 92;
pub const SYS_ftruncate: ::c_long = 93;
pub const SYS_fchmod: ::c_long = 94;
pub const SYS_fchown: ::c_long = 95;
pub const SYS_getpriority: ::c_long = 96;
pub const SYS_setpriority: ::c_long = 97;
pub const SYS_statfs: ::c_long = 99;
pub const SYS_fstatfs: ::c_long = 100;
pub const SYS_syslog: ::c_long = 103;
pub const SYS_setitimer: ::c_long = 104;
pub const SYS_getitimer: ::c_long = 105;
pub const SYS_stat: ::c_long = 106;
pub const SYS_lstat: ::c_long = 107;
pub const SYS_fstat: ::c_long = 108;
pub const SYS_vhangup: ::c_long = 111;
pub const SYS_wait4: ::c_long = 114;
pub const SYS_swapoff: ::c_long = 115;
pub const SYS_sysinfo: ::c_long = 116;
pub const SYS_fsync: ::c_long = 118;
pub const SYS_sigreturn: ::c_long = 119;
pub const SYS_clone: ::c_long = 120;
pub const SYS_setdomainname: ::c_long = 121;
pub const SYS_uname: ::c_long = 122;
pub const SYS_adjtimex: ::c_long = 124;
pub const SYS_mprotect: ::c_long = 125;
pub const SYS_sigprocmask: ::c_long = 126;
pub const SYS_init_module: ::c_long = 128;
pub const SYS_delete_module: ::c_long = 129;
pub const SYS_quotactl: ::c_long = 131;
pub const SYS_getpgid: ::c_long = 132;
pub const SYS_fchdir: ::c_long = 133;
pub const SYS_bdflush: ::c_long = 134;
pub const SYS_sysfs: ::c_long = 135;
pub const SYS_personality: ::c_long = 136;
pub const SYS_setfsuid: ::c_long = 138;
pub const SYS_setfsgid: ::c_long = 139;
pub const SYS__llseek: ::c_long = 140;
pub const SYS_getdents: ::c_long = 141;
pub const SYS__newselect: ::c_long = 142;
pub const SYS_flock: ::c_long = 143;
pub const SYS_msync: ::c_long = 144;
pub const SYS_readv: ::c_long = 145;
pub const SYS_writev: ::c_long = 146;
pub const SYS_getsid: ::c_long = 147;
pub const SYS_fdatasync: ::c_long = 148;
pub const SYS__sysctl: ::c_long = 149;
pub const SYS_mlock: ::c_long = 150;
pub const SYS_munlock: ::c_long = 151;
pub const SYS_mlockall: ::c_long = 152;
pub const SYS_munlockall: ::c_long = 153;
pub const SYS_sched_setparam: ::c_long = 154;
pub const SYS_sched_getparam: ::c_long = 155;
pub const SYS_sched_setscheduler: ::c_long = 156;
pub const SYS_sched_getscheduler: ::c_long = 157;
pub const SYS_sched_yield: ::c_long = 158;
pub const SYS_sched_get_priority_max: ::c_long = 159;
pub const SYS_sched_get_priority_min: ::c_long = 160;
pub const SYS_sched_rr_get_interval: ::c_long = 161;
pub const SYS_nanosleep: ::c_long = 162;
pub const SYS_mremap: ::c_long = 163;
pub const SYS_setresuid: ::c_long = 164;
pub const SYS_getresuid: ::c_long = 165;
pub const SYS_poll: ::c_long = 168;
pub const SYS_nfsservctl: ::c_long = 169;
pub const SYS_setresgid: ::c_long = 170;
pub const SYS_getresgid: ::c_long = 171;
pub const SYS_prctl: ::c_long = 172;
pub const SYS_rt_sigreturn: ::c_long = 173;
pub const SYS_rt_sigaction: ::c_long = 174;
pub const SYS_rt_sigprocmask: ::c_long = 175;
pub const SYS_rt_sigpending: ::c_long = 176;
pub const SYS_rt_sigtimedwait: ::c_long = 177;
pub const SYS_rt_sigqueueinfo: ::c_long = 178;
pub const SYS_rt_sigsuspend: ::c_long = 179;
pub const SYS_pread64: ::c_long = 180;
pub const SYS_pwrite64: ::c_long = 181;
pub const SYS_chown: ::c_long = 182;
pub const SYS_getcwd: ::c_long = 183;
pub const SYS_capget: ::c_long = 184;
pub const SYS_capset: ::c_long = 185;
pub const SYS_sigaltstack: ::c_long = 186;
pub const SYS_sendfile: ::c_long = 187;
pub const SYS_vfork: ::c_long = 190;
pub const SYS_ugetrlimit: ::c_long = 191;
pub const SYS_mmap2: ::c_long = 192;
pub const SYS_truncate64: ::c_long = 193;
pub const SYS_ftruncate64: ::c_long = 194;
pub const SYS_stat64: ::c_long = 195;
pub const SYS_lstat64: ::c_long = 196;
pub const SYS_fstat64: ::c_long = 197;
pub const SYS_lchown32: ::c_long = 198;
pub const SYS_getuid32: ::c_long = 199;
pub const SYS_getgid32: ::c_long = 200;
pub const SYS_geteuid32: ::c_long = 201;
pub const SYS_getegid32: ::c_long = 202;
pub const SYS_setreuid32: ::c_long = 203;
pub const SYS_setregid32: ::c_long = 204;
pub const SYS_getgroups32: ::c_long = 205;
pub const SYS_setgroups32: ::c_long = 206;
pub const SYS_fchown32: ::c_long = 207;
pub const SYS_setresuid32: ::c_long = 208;
pub const SYS_getresuid32: ::c_long = 209;
pub const SYS_setresgid32: ::c_long = 210;
pub const SYS_getresgid32: ::c_long = 211;
pub const SYS_chown32: ::c_long = 212;
pub const SYS_setuid32: ::c_long = 213;
pub const SYS_setgid32: ::c_long = 214;
pub const SYS_setfsuid32: ::c_long = 215;
pub const SYS_setfsgid32: ::c_long = 216;
pub const SYS_getdents64: ::c_long = 217;
pub const SYS_pivot_root: ::c_long = 218;
pub const SYS_mincore: ::c_long = 219;
pub const SYS_madvise: ::c_long = 220;
pub const SYS_fcntl64: ::c_long = 221;
pub const SYS_gettid: ::c_long = 224;
pub const SYS_readahead: ::c_long = 225;
pub const SYS_setxattr: ::c_long = 226;
pub const SYS_lsetxattr: ::c_long = 227;
pub const SYS_fsetxattr: ::c_long = 228;
pub const SYS_getxattr: ::c_long = 229;
pub const SYS_lgetxattr: ::c_long = 230;
pub const SYS_fgetxattr: ::c_long = 231;
pub const SYS_listxattr: ::c_long = 232;
pub const SYS_llistxattr: ::c_long = 233;
pub const SYS_flistxattr: ::c_long = 234;
pub const SYS_removexattr: ::c_long = 235;
pub const SYS_lremovexattr: ::c_long = 236;
pub const SYS_fremovexattr: ::c_long = 237;
pub const SYS_tkill: ::c_long = 238;
pub const SYS_sendfile64: ::c_long = 239;
pub const SYS_futex: ::c_long = 240;
pub const SYS_sched_setaffinity: ::c_long = 241;
pub const SYS_sched_getaffinity: ::c_long = 242;
pub const SYS_io_setup: ::c_long = 243;
pub const SYS_io_destroy: ::c_long = 244;
pub const SYS_io_getevents: ::c_long = 245;
pub const SYS_io_submit: ::c_long = 246;
pub const SYS_io_cancel: ::c_long = 247;
pub const SYS_exit_group: ::c_long = 248;
pub const SYS_lookup_dcookie: ::c_long = 249;
pub const SYS_epoll_create: ::c_long = 250;
pub const SYS_epoll_ctl: ::c_long = 251;
pub const SYS_epoll_wait: ::c_long = 252;
pub const SYS_remap_file_pages: ::c_long = 253;
pub const SYS_set_tid_address: ::c_long = 256;
pub const SYS_timer_create: ::c_long = 257;
pub const SYS_timer_settime: ::c_long = 258;
pub const SYS_timer_gettime: ::c_long = 259;
pub const SYS_timer_getoverrun: ::c_long = 260;
pub const SYS_timer_delete: ::c_long = 261;
pub const SYS_clock_settime: ::c_long = 262;
pub const SYS_clock_gettime: ::c_long = 263;
pub const SYS_clock_getres: ::c_long = 264;
pub const SYS_clock_nanosleep: ::c_long = 265;
pub const SYS_statfs64: ::c_long = 266;
pub const SYS_fstatfs64: ::c_long = 267;
pub const SYS_tgkill: ::c_long = 268;
pub const SYS_utimes: ::c_long = 269;
pub const SYS_pciconfig_iobase: ::c_long = 271;
pub const SYS_pciconfig_read: ::c_long = 272;
pub const SYS_pciconfig_write: ::c_long = 273;
pub const SYS_mq_open: ::c_long = 274;
pub const SYS_mq_unlink: ::c_long = 275;
pub const SYS_mq_timedsend: ::c_long = 276;
pub const SYS_mq_timedreceive: ::c_long = 277;
pub const SYS_mq_notify: ::c_long = 278;
pub const SYS_mq_getsetattr: ::c_long = 279;
pub const SYS_waitid: ::c_long = 280;
pub const SYS_socket: ::c_long = 281;
pub const SYS_bind: ::c_long = 282;
pub const SYS_connect: ::c_long = 283;
pub const SYS_listen: ::c_long = 284;
pub const SYS_accept: ::c_long = 285;
pub const SYS_getsockname: ::c_long = 286;
pub const SYS_getpeername: ::c_long = 287;
pub const SYS_socketpair: ::c_long = 288;
pub const SYS_send: ::c_long = 289;
pub const SYS_sendto: ::c_long = 290;
pub const SYS_recv: ::c_long = 291;
pub const SYS_recvfrom: ::c_long = 292;
pub const SYS_shutdown: ::c_long = 293;
pub const SYS_setsockopt: ::c_long = 294;
pub const SYS_getsockopt: ::c_long = 295;
pub const SYS_sendmsg: ::c_long = 296;
pub const SYS_recvmsg: ::c_long = 297;
pub const SYS_semop: ::c_long = 298;
pub const SYS_semget: ::c_long = 299;
pub const SYS_semctl: ::c_long = 300;
pub const SYS_msgsnd: ::c_long = 301;
pub const SYS_msgrcv: ::c_long = 302;
pub const SYS_msgget: ::c_long = 303;
pub const SYS_msgctl: ::c_long = 304;
pub const SYS_shmat: ::c_long = 305;
pub const SYS_shmdt: ::c_long = 306;
pub const SYS_shmget: ::c_long = 307;
pub const SYS_shmctl: ::c_long = 308;
pub const SYS_add_key: ::c_long = 309;
pub const SYS_request_key: ::c_long = 310;
pub const SYS_keyctl: ::c_long = 311;
pub const SYS_semtimedop: ::c_long = 312;
pub const SYS_vserver: ::c_long = 313;
pub const SYS_ioprio_set: ::c_long = 314;
pub const SYS_ioprio_get: ::c_long = 315;
pub const SYS_inotify_init: ::c_long = 316;
pub const SYS_inotify_add_watch: ::c_long = 317;
pub const SYS_inotify_rm_watch: ::c_long = 318;
pub const SYS_mbind: ::c_long = 319;
pub const SYS_get_mempolicy: ::c_long = 320;
pub const SYS_set_mempolicy: ::c_long = 321;
pub const SYS_openat: ::c_long = 322;
pub const SYS_mkdirat: ::c_long = 323;
pub const SYS_mknodat: ::c_long = 324;
pub const SYS_fchownat: ::c_long = 325;
pub const SYS_futimesat: ::c_long = 326;
pub const SYS_fstatat64: ::c_long = 327;
pub const SYS_unlinkat: ::c_long = 328;
pub const SYS_renameat: ::c_long = 329;
pub const SYS_linkat: ::c_long = 330;
pub const SYS_symlinkat: ::c_long = 331;
pub const SYS_readlinkat: ::c_long = 332;
pub const SYS_fchmodat: ::c_long = 333;
pub const SYS_faccessat: ::c_long = 334;
pub const SYS_pselect6: ::c_long = 335;
pub const SYS_ppoll: ::c_long = 336;
pub const SYS_unshare: ::c_long = 337;
pub const SYS_set_robust_list: ::c_long = 338;
pub const SYS_get_robust_list: ::c_long = 339;
pub const SYS_splice: ::c_long = 340;
pub const SYS_tee: ::c_long = 342;
pub const SYS_vmsplice: ::c_long = 343;
pub const SYS_move_pages: ::c_long = 344;
pub const SYS_getcpu: ::c_long = 345;
pub const SYS_epoll_pwait: ::c_long = 346;
pub const SYS_kexec_load: ::c_long = 347;
pub const SYS_utimensat: ::c_long = 348;
pub const SYS_signalfd: ::c_long = 349;
pub const SYS_timerfd_create: ::c_long = 350;
pub const SYS_eventfd: ::c_long = 351;
pub const SYS_fallocate: ::c_long = 352;
pub const SYS_timerfd_settime: ::c_long = 353;
pub const SYS_timerfd_gettime: ::c_long = 354;
pub const SYS_signalfd4: ::c_long = 355;
pub const SYS_eventfd2: ::c_long = 356;
pub const SYS_epoll_create1: ::c_long = 357;
pub const SYS_dup3: ::c_long = 358;
pub const SYS_pipe2: ::c_long = 359;
pub const SYS_inotify_init1: ::c_long = 360;
pub const SYS_preadv: ::c_long = 361;
pub const SYS_pwritev: ::c_long = 362;
pub const SYS_rt_tgsigqueueinfo: ::c_long = 363;
pub const SYS_perf_event_open: ::c_long = 364;
pub const SYS_recvmmsg: ::c_long = 365;
pub const SYS_accept4: ::c_long = 366;
pub const SYS_fanotify_init: ::c_long = 367;
pub const SYS_fanotify_mark: ::c_long = 368;
pub const SYS_prlimit64: ::c_long = 369;
pub const SYS_name_to_handle_at: ::c_long = 370;
pub const SYS_open_by_handle_at: ::c_long = 371;
pub const SYS_clock_adjtime: ::c_long = 372;
pub const SYS_syncfs: ::c_long = 373;
pub const SYS_sendmmsg: ::c_long = 374;
pub const SYS_setns: ::c_long = 375;
pub const SYS_process_vm_readv: ::c_long = 376;
pub const SYS_process_vm_writev: ::c_long = 377;
pub const SYS_kcmp: ::c_long = 378;
pub const SYS_finit_module: ::c_long = 379;
pub const SYS_sched_setattr: ::c_long = 380;
pub const SYS_sched_getattr: ::c_long = 381;
pub const SYS_renameat2: ::c_long = 382;
pub const SYS_seccomp: ::c_long = 383;
pub const SYS_getrandom: ::c_long = 384;
pub const SYS_memfd_create: ::c_long = 385;
pub const SYS_bpf: ::c_long = 386;
pub const SYS_execveat: ::c_long = 387;
pub const SYS_userfaultfd: ::c_long = 388;
pub const SYS_membarrier: ::c_long = 389;
pub const SYS_mlock2: ::c_long = 390;
pub const SYS_copy_file_range: ::c_long = 391;
pub const SYS_preadv2: ::c_long = 392;
pub const SYS_pwritev2: ::c_long = 393;
pub const SYS_pkey_mprotect: ::c_long = 394;
pub const SYS_pkey_alloc: ::c_long = 395;
pub const SYS_pkey_free: ::c_long = 396;

extern "C" {
    pub fn ioctl(fd: ::c_int, request: ::c_ulong, ...) -> ::c_int;
}

cfg_if! {
    if #[cfg(libc_align)] {
        mod align;
        pub use self::align::*;
    } else {
        mod no_align;
        pub use self::no_align::*;
    }
}
