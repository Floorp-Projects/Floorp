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
        pub st_atim: ::timespec,
        pub st_mtim: ::timespec,
        pub st_ctim: ::timespec,
        pub __unused4: ::c_ulong,
        pub __unused5: ::c_ulong,
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
        pub st_atim: ::timespec,
        pub st_mtim: ::timespec,
        pub st_ctim: ::timespec,
        pub st_ino: ::ino64_t,
    }

    pub struct flock {
        pub l_type: ::c_short,
        pub l_whence: ::c_short,
        pub l_start: ::off_t,
        pub l_len: ::off_t,
        pub l_pid: ::pid_t,
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
        pub f_spare: [::c_int; 5],
    }

    pub struct sigset_t {
        __val: [::c_ulong; 2],
    }

    pub struct sigaction {
        pub sa_sigaction: ::sighandler_t,
        // uClibc defines sa_flags as `unsigned long int`,
        // but nix crate expects `int`
        pub sa_flags: ::c_int,
        pub sa_restorer: *mut ::c_void,
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
        ss_flags: ::c_int,
        ss_size: ::size_t,
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
        pub __unused1: ::c_ulong,
        pub __unused2: ::c_ulong,
    }

    pub struct msqid_ds {
        pub msg_perm: ::ipc_perm,
        pub msg_stime: ::time_t,
        pub __unused1: ::c_ulong,
        pub msg_rtime: ::time_t,
        pub __unused2: ::c_ulong,
        pub msg_ctime: ::time_t,
        pub __unused3: ::c_ulong,
        pub __msg_cbytes: ::c_ulong,
        pub msg_qnum: ::msgqnum_t,
        pub msg_qbytes: ::msglen_t,
        pub msg_lspid: ::pid_t,
        pub msg_lrpid: ::pid_t,
        pub __unused4: ::c_ulong,
        pub __unused5: ::c_ulong,
    }

    pub struct shmid_ds {
        pub shm_perm: ::ipc_perm,
        pub shm_segsz: ::size_t,
        pub shm_atime: ::time_t,
        pub __unused1: ::c_ulong,
        pub shm_dtime: ::time_t,
        pub __unused2: ::c_ulong,
        pub shm_ctime: ::time_t,
        pub __unused3: ::c_ulong,
        pub shm_cpid: ::pid_t,
        pub shm_lpid: ::pid_t,
        pub shm_nattch: ::shmatt_t,
        pub __unused4: ::c_ulong,
        pub __unused5: ::c_ulong,
    }

    pub struct ucred {
        pub pid: ::pid_t,
        pub uid: ::uid_t,
        pub gid: ::gid_t,
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
pub const AIO_ALLDONE: ::c_int = 2; // from linux/mod.rs
pub const AIO_CANCELED: ::c_int = 0; // from linux/mod.rs
pub const AIO_NOTCANCELED: ::c_int = 1; // from linux/mod.rs
pub const CLONE_NEWCGROUP: ::c_int = 0x02000000; // from linux/mod.rs
pub const EPOLLEXCLUSIVE: ::c_int = 0x10000000; // from linux/mod.rs
pub const EPOLLWAKEUP: ::c_int = 0x20000000; // from linux/other/mod.rs
pub const EXTPROC: ::tcflag_t = 0o200000; // from asm-generic/termbits.h
pub const F_GETPIPE_SZ: ::c_int = 1032; // from notbsd/mod.rs
pub const F_SETPIPE_SZ: ::c_int = 1031; // from notbsd/mod.rs
pub const LIO_NOP: ::c_int = 2; // from linux/mod.rs
pub const LIO_NOWAIT: ::c_int = 1; // from linux/mod.rs
pub const LIO_READ: ::c_int = 0; // from linux/mod.rs
pub const LIO_WAIT: ::c_int = 0; // from linux/mod.rs
pub const LIO_WRITE: ::c_int = 1; // from linux/mod.rs
pub const MAP_HUGETLB: ::c_int = 0x040000; // from linux/other/mod.rs
pub const O_TMPFILE: ::c_int = 0o20000000 | O_DIRECTORY;
pub const RB_KEXEC: ::c_int = 0x45584543u32 as i32; // from linux/mod.rs
pub const RB_SW_SUSPEND: ::c_int = 0xd000fce2u32 as i32; // from linux/mod.rs
pub const SO_BUSY_POLL: ::c_int = 46; // from src/unix/notbsd/mod.rs
pub const SO_PEEK_OFF: ::c_int = 42; // from src/unix/notbsd/mod.rs
pub const SO_REUSEPORT: ::c_int = 15; // from src/unix/notbsd/mod.rs
pub const SOL_NETLINK: ::c_int = 270; // from src/unix/notbsd/mod.rs
pub const _POSIX_VDISABLE: ::cc_t = 0; // from linux/mod.rs
pub const AT_EMPTY_PATH: ::c_int = 0x1000; // from notbsd/mod.rs

// autogenerated constants with hand tuned types
pub const AT_NO_AUTOMOUNT: ::c_int = 0x800;
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
pub const CMSPAR: ::tcflag_t = 0x40000000;
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
pub const FLUSHO: ::tcflag_t = 0x1000;
pub const F_GETLK: ::c_int = 0x5;
pub const F_SETLK: ::c_int = 0x6;
pub const F_SETLKW: ::c_int = 0x7;
pub const HUPCL: ::tcflag_t = 0x400;
pub const ICANON: ::tcflag_t = 0x2;
pub const IEXTEN: ::tcflag_t = 0x8000;
pub const IPV6_MULTICAST_HOPS: ::c_int = 0x12;
pub const IPV6_MULTICAST_IF: ::c_int = 0x11;
pub const IPV6_UNICAST_HOPS: ::c_int = 0x10;
pub const IP_MULTICAST_IF: ::c_int = 0x20;
pub const ISIG: ::tcflag_t = 0x1;
pub const IUTF8: ::tcflag_t = 0x4000;
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
pub const MS_ACTIVE: u32 = 0x40000000;
pub const MS_DIRSYNC: u32 = 0x80;
pub const MS_I_VERSION: u32 = 0x800000;
pub const MS_KERNMOUNT: u32 = 0x400000;
pub const MS_MOVE: u32 = 0x2000;
pub const MS_POSIXACL: u32 = 0x10000;
pub const MS_PRIVATE: u32 = 0x40000;
pub const MS_REC: u32 = 0x4000;
pub const MS_RELATIME: u32 = 0x200000;
pub const MS_SHARED: u32 = 0x100000;
pub const MS_SILENT: u32 = 0x8000;
pub const MS_SLAVE: u32 = 0x80000;
pub const MS_STRICTATIME: u32 = 0x1000000;
pub const MS_UNBINDABLE: u32 = 0x20000;
pub const NLDLY: ::tcflag_t = 0x100;
pub const NOFLSH: ::tcflag_t = 0x80;
pub const OCRNL: ::c_int = 0x8;
pub const OFDEL: ::c_int = 0x80;
pub const OFILL: ::c_int = 0x40;
pub const OLCUC: ::tcflag_t = 0x2;
pub const ONLCR: ::tcflag_t = 0x4;
pub const ONLRET: ::tcflag_t = 0x20;
pub const ONOCR: ::tcflag_t = 0x10;
pub const O_ACCMODE: ::c_int = 0x3;
pub const O_APPEND: ::c_int = 0x400;
pub const O_CREAT: ::c_int = 0x40;
pub const O_DIRECT: ::c_int = 0x10000;
pub const O_DIRECTORY: ::c_int = 0x4000;
pub const O_DSYNC: ::c_int = 0x1000;
pub const O_EXCL: ::c_int = 0x80;
pub const O_NDELAY: ::c_int = 0x800;
pub const O_NOCTTY: ::c_int = 0x100;
pub const O_NOFOLLOW: ::c_int = 0x8000;
pub const O_NONBLOCK: ::c_int = 0x800;
pub const O_SYNC: ::c_int = 0o10000;
pub const O_TRUNC: ::c_int = 0x200;
pub const PARENB: ::tcflag_t = 0x100;
pub const PARODD: ::tcflag_t = 0x200;
pub const PENDIN: ::tcflag_t = 0x4000;
pub const POLLRDBAND: ::c_short = 0x80;
pub const POLLRDNORM: ::c_short = 0x40;
pub const POLLWRBAND: ::c_short = 0x200;
pub const POLLWRNORM: ::c_short = 0x100;
pub const QIF_ALL: ::uint32_t = 0x3f;
pub const QIF_BLIMITS: ::uint32_t = 0x1;
pub const QIF_BTIME: ::uint32_t = 0x10;
pub const QIF_ILIMITS: ::uint32_t = 0x4;
pub const QIF_INODES: ::uint32_t = 0x8;
pub const QIF_ITIME: ::uint32_t = 0x20;
pub const QIF_LIMITS: ::uint32_t = 0x5;
pub const QIF_SPACE: ::uint32_t = 0x2;
pub const QIF_TIMES: ::uint32_t = 0x30;
pub const QIF_USAGE: ::uint32_t = 0xa;
pub const SA_NOCLDSTOP: ::c_int = 0x1;
pub const SA_NOCLDWAIT: ::c_int = 0x2;
pub const SA_NODEFER: ::c_int = 0x40000000;
pub const SA_ONSTACK: ::c_int = 0x8000000;
pub const SA_RESETHAND: ::c_int = 0x80000000;
pub const SA_RESTART: ::c_int = 0x10000000;
pub const SA_SIGINFO: ::c_int = 0x4;
pub const SFD_CLOEXEC: ::c_int = 0x80000;
pub const SFD_NONBLOCK: ::c_int = 0x800;
pub const SIGBUS: ::c_int = 0x7;
pub const SIGCHLD: ::c_int = 0x11;
pub const SIGCONT: ::c_int = 0x12;
pub const SIGIO: ::c_int = 0x1d;
pub const SIGPROF: ::c_int = 0x1b;
pub const SIGPWR: ::c_int = 0x1e;
pub const SIGSTKFLT: ::c_int = 0x10;
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
pub const SOL_SOCKET: ::c_int = 0x1;
pub const SO_ACCEPTCONN: ::c_int = 0x1e;
pub const SO_BINDTODEVICE: ::c_int = 0x19;
pub const SO_BROADCAST: ::c_int = 0x6;
pub const SO_BSDCOMPAT: ::c_int = 0xe;
pub const SO_DOMAIN: ::c_int = 0x27;
pub const SO_DONTROUTE: ::c_int = 0x5;
pub const SO_ERROR: ::c_int = 0x4;
pub const SO_KEEPALIVE: ::c_int = 0x9;
pub const SO_LINGER: ::c_int = 0xd;
pub const SO_MARK: ::c_int = 0x24;
pub const SO_OOBINLINE: ::c_int = 0xa;
pub const SO_PASSCRED: ::c_int = 0x10;
pub const SO_PEERCRED: ::c_int = 0x11;
pub const SO_PRIORITY: ::c_int = 0xc;
pub const SO_PROTOCOL: ::c_int = 0x26;
pub const SO_RCVBUF: ::c_int = 0x8;
pub const SO_RCVLOWAT: ::c_int = 0x12;
pub const SO_RCVTIMEO: ::c_int = 0x14;
pub const SO_REUSEADDR: ::c_int = 0x2;
pub const SO_RXQ_OVFL: ::c_int = 0x28;
pub const SO_SNDBUF: ::c_int = 0x7;
pub const SO_SNDBUFFORCE: ::c_int = 0x20;
pub const SO_SNDLOWAT: ::c_int = 0x13;
pub const SO_SNDTIMEO: ::c_int = 0x15;
pub const SO_TIMESTAMP: ::c_int = 0x1d;
pub const SO_TYPE: ::c_int = 0x3;
pub const SYS_gettid: ::c_int = 0xe0;
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
pub const _PC_2_SYMLINKS: ::c_int = 0x14;
pub const _PC_ALLOC_SIZE_MIN: ::c_int = 0x12;
pub const _PC_ASYNC_IO: ::c_int = 0xa;
pub const _PC_FILESIZEBITS: ::c_int = 0xd;
pub const _PC_PRIO_IO: ::c_int = 0xb;
pub const _PC_REC_INCR_XFER_SIZE: ::c_int = 0xe;
pub const _PC_REC_MAX_XFER_SIZE: ::c_int = 0xf;
pub const _PC_REC_MIN_XFER_SIZE: ::c_int = 0x10;
pub const _PC_REC_XFER_ALIGN: ::c_int = 0x11;
pub const _PC_SYMLINK_MAX: ::c_int = 0x13;
pub const _PC_SYNC_IO: ::c_int = 0x9;
pub const _SC_2_PBS: ::c_int = 0xa8;
pub const _SC_2_PBS_ACCOUNTING: ::c_int = 0xa9;
pub const _SC_2_PBS_CHECKPOINT: ::c_int = 0xaf;
pub const _SC_2_PBS_LOCATE: ::c_int = 0xaa;
pub const _SC_2_PBS_MESSAGE: ::c_int = 0xab;
pub const _SC_2_PBS_TRACK: ::c_int = 0xac;
pub const _SC_ADVISORY_INFO: ::c_int = 0x84;
pub const _SC_BARRIERS: ::c_int = 0x85;
pub const _SC_CLOCK_SELECTION: ::c_int = 0x89;
pub const _SC_CPUTIME: ::c_int = 0x8a;
pub const _SC_IPV6: ::c_int = 0xeb;
pub const _SC_MONOTONIC_CLOCK: ::c_int = 0x95;
pub const _SC_RAW_SOCKETS: ::c_int = 0xec;
pub const _SC_READER_WRITER_LOCKS: ::c_int = 0x99;
pub const _SC_REGEXP: ::c_int = 0x9b;
pub const _SC_SHELL: ::c_int = 0x9d;
pub const _SC_SPAWN: ::c_int = 0x9f;
pub const _SC_SPIN_LOCKS: ::c_int = 0x9a;
pub const _SC_SPORADIC_SERVER: ::c_int = 0xa0;
pub const _SC_SS_REPL_MAX: ::c_int = 0xf1;
pub const _SC_SYMLOOP_MAX: ::c_int = 0xad;
pub const _SC_THREAD_CPUTIME: ::c_int = 0x8b;
pub const _SC_THREAD_PROCESS_SHARED: ::c_int = 0x52;
pub const _SC_THREAD_ROBUST_PRIO_INHERIT: ::c_int = 0xf7;
pub const _SC_THREAD_ROBUST_PRIO_PROTECT: ::c_int = 0xf8;
pub const _SC_THREAD_SPORADIC_SERVER: ::c_int = 0xa1;
pub const _SC_TIMEOUTS: ::c_int = 0xa4;
pub const _SC_TRACE: ::c_int = 0xb5;
pub const _SC_TRACE_EVENT_FILTER: ::c_int = 0xb6;
pub const _SC_TRACE_EVENT_NAME_MAX: ::c_int = 0xf2;
pub const _SC_TRACE_INHERIT: ::c_int = 0xb7;
pub const _SC_TRACE_LOG: ::c_int = 0xb8;
pub const _SC_TRACE_NAME_MAX: ::c_int = 0xf3;
pub const _SC_TRACE_SYS_MAX: ::c_int = 0xf4;
pub const _SC_TRACE_USER_EVENT_MAX: ::c_int = 0xf5;
pub const _SC_TYPED_MEMORY_OBJECTS: ::c_int = 0xa5;
pub const _SC_V6_ILP32_OFF32: ::c_int = 0xb0;
pub const _SC_V6_ILP32_OFFBIG: ::c_int = 0xb1;
pub const _SC_V6_LP64_OFF64: ::c_int = 0xb2;
pub const _SC_V6_LPBIG_OFFBIG: ::c_int = 0xb3;
pub const _SC_XOPEN_STREAMS: ::c_int = 0xf6;

fn CMSG_ALIGN(len: usize) -> usize {
    len + ::mem::size_of::<usize>() - 1 & !(::mem::size_of::<usize>() - 1)
}

f! {
    pub fn CMSG_FIRSTHDR(mhdr: *const msghdr) -> *mut cmsghdr {
        if (*mhdr).msg_controllen as usize >= ::mem::size_of::<cmsghdr>() {
            (*mhdr).msg_control as *mut cmsghdr
        } else {
            0 as *mut cmsghdr
        }
    }

    pub fn CMSG_DATA(cmsg: *const cmsghdr) -> *mut ::c_uchar {
        cmsg.offset(1) as *mut ::c_uchar
    }

    pub fn CMSG_SPACE(length: ::c_uint) -> ::c_uint {
        (CMSG_ALIGN(length as usize) + CMSG_ALIGN(::mem::size_of::<cmsghdr>()))
            as ::c_uint
    }

    pub fn CMSG_LEN(length: ::c_uint) -> ::c_uint {
        CMSG_ALIGN(::mem::size_of::<cmsghdr>()) as ::c_uint + length
    }

    pub fn CMSG_NXTHDR(mhdr: *const msghdr,
                       cmsg: *const cmsghdr) -> *mut cmsghdr {
        if ((*cmsg).cmsg_len as usize) < ::mem::size_of::<cmsghdr>() {
            return 0 as *mut cmsghdr;
        };
        let next = (cmsg as usize +
                    CMSG_ALIGN((*cmsg).cmsg_len as usize))
            as *mut cmsghdr;
        let max = (*mhdr).msg_control as usize
            + (*mhdr).msg_controllen as usize;
        if (next.offset(1)) as usize > max ||
            next as usize + CMSG_ALIGN((*next).cmsg_len as usize) > max
        {
            0 as *mut cmsghdr
        } else {
            next as *mut cmsghdr
        }
    }

}

extern {
    pub fn ioctl(fd: ::c_int, request: ::c_ulong, ...) -> ::c_int;
    pub fn openpty(amaster: *mut ::c_int,
                aslave: *mut ::c_int,
                name: *mut ::c_char,
                termp: *mut termios,
                winp: *mut ::winsize) -> ::c_int;
    pub fn setns(fd: ::c_int, nstype: ::c_int) -> ::c_int;
    pub fn pwritev(fd: ::c_int,
                   iov: *const ::iovec,
                   iovcnt: ::c_int,
                   offset: ::off_t) -> ::ssize_t;
    pub fn preadv(fd: ::c_int,
                  iov: *const ::iovec,
                  iovcnt: ::c_int,
                  offset: ::off_t) -> ::ssize_t;
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
