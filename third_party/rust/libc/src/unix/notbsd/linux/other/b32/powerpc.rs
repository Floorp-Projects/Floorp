pub type c_char = u8;
pub type wchar_t = i32;

s! {
    pub struct ipc_perm {
        __key: ::key_t,
        pub uid: ::uid_t,
        pub gid: ::gid_t,
        pub cuid: ::uid_t,
        pub cgid: ::gid_t,
        pub mode: ::mode_t,
        __seq: ::uint32_t,
        __pad1: ::uint32_t,
        __glibc_reserved1: ::uint64_t,
        __glibc_reserved2: ::uint64_t,
    }

    pub struct stat64 {
        pub st_dev: ::dev_t,
        pub st_ino: ::ino64_t,
        pub st_mode: ::mode_t,
        pub st_nlink: ::nlink_t,
        pub st_uid: ::uid_t,
        pub st_gid: ::gid_t,
        pub st_rdev: ::dev_t,
        __pad2: ::c_ushort,
        pub st_size: ::off64_t,
        pub st_blksize: ::blksize_t,
        pub st_blocks: ::blkcnt64_t,
        pub st_atime: ::time_t,
        pub st_atime_nsec: ::c_long,
        pub st_mtime: ::time_t,
        pub st_mtime_nsec: ::c_long,
        pub st_ctime: ::time_t,
        pub st_ctime_nsec: ::c_long,
        __glibc_reserved4: ::c_ulong,
        __glibc_reserved5: ::c_ulong,
    }

    pub struct shmid_ds {
        pub shm_perm: ::ipc_perm,
        __glibc_reserved1: ::c_uint,
        pub shm_atime: ::time_t,
        __glibc_reserved2: ::c_uint,
        pub shm_dtime: ::time_t,
        __glibc_reserved3: ::c_uint,
        pub shm_ctime: ::time_t,
        __glibc_reserved4: ::c_uint,
        pub shm_segsz: ::size_t,
        pub shm_cpid: ::pid_t,
        pub shm_lpid: ::pid_t,
        pub shm_nattch: ::shmatt_t,
        __glibc_reserved5: ::c_ulong,
        __glibc_reserved6: ::c_ulong,
    }

    pub struct msqid_ds {
        pub msg_perm: ::ipc_perm,
        __glibc_reserved1: ::c_uint,
        pub msg_stime: ::time_t,
        __glibc_reserved2: ::c_uint,
        pub msg_rtime: ::time_t,
        __glibc_reserved3: ::c_uint,
        pub msg_ctime: ::time_t,
        __msg_cbytes: ::c_ulong,
        pub msg_qnum: ::msgqnum_t,
        pub msg_qbytes: ::msglen_t,
        pub msg_lspid: ::pid_t,
        pub msg_lrpid: ::pid_t,
        __glibc_reserved4: ::c_ulong,
        __glibc_reserved5: ::c_ulong,
    }
}

pub const O_DIRECT: ::c_int = 0x20000;
pub const O_DIRECTORY: ::c_int = 0x4000;
pub const O_NOFOLLOW: ::c_int = 0x8000;

pub const MAP_LOCKED: ::c_int = 0x00080;
pub const MAP_NORESERVE: ::c_int = 0x00040;

pub const EDEADLOCK: ::c_int = 58;

pub const SO_SNDBUFFORCE: ::c_int = 32;
pub const SO_RCVBUFFORCE: ::c_int = 33;
pub const SO_NO_CHECK: ::c_int = 11;
pub const SO_PRIORITY: ::c_int = 12;
pub const SO_BSDCOMPAT: ::c_int = 14;
pub const SO_RCVLOWAT: ::c_int = 16;
pub const SO_SNDLOWAT: ::c_int = 17;
pub const SO_RCVTIMEO: ::c_int = 18;
pub const SO_SNDTIMEO: ::c_int = 19;
pub const SO_PASSCRED: ::c_int = 20;
pub const SO_PEERCRED: ::c_int = 21;

pub const FIOCLEX: ::c_ulong = 0x20006601;
pub const FIONBIO: ::c_ulong = 0x8004667e;

pub const SYS_gettid: ::c_long = 207;
pub const SYS_perf_event_open: ::c_long = 319;

pub const MCL_CURRENT: ::c_int = 0x2000;
pub const MCL_FUTURE: ::c_int = 0x4000;

pub const SIGSTKSZ: ::size_t = 0x4000;
pub const MINSIGSTKSZ: ::size_t = 4096;
pub const CBAUD: ::tcflag_t = 0xff;
pub const TAB1: ::c_int = 0x400;
pub const TAB2: ::c_int = 0x800;
pub const TAB3: ::c_int = 0xc00;
pub const CR1: ::c_int  = 0x1000;
pub const CR2: ::c_int  = 0x2000;
pub const CR3: ::c_int  = 0x3000;
pub const FF1: ::c_int  = 0x4000;
pub const BS1: ::c_int  = 0x8000;
pub const VT1: ::c_int  = 0x10000;
pub const VWERASE: usize = 0xa;
pub const VREPRINT: usize = 0xb;
pub const VSUSP: usize = 0xc;
pub const VSTART: usize = 0xd;
pub const VSTOP: usize = 0xe;
pub const VDISCARD: usize = 0x10;
pub const VTIME: usize = 0x7;
pub const IXON: ::tcflag_t = 0x200;
pub const IXOFF: ::tcflag_t = 0x400;
pub const ONLCR: ::tcflag_t = 0x2;
pub const CSIZE: ::tcflag_t = 0x300;
pub const CS6: ::tcflag_t = 0x100;
pub const CS7: ::tcflag_t = 0x200;
pub const CS8: ::tcflag_t = 0x300;
pub const CSTOPB: ::tcflag_t = 0x400;
pub const CREAD: ::tcflag_t = 0x800;
pub const PARENB: ::tcflag_t = 0x1000;
pub const PARODD: ::tcflag_t = 0x2000;
pub const HUPCL: ::tcflag_t = 0x4000;
pub const CLOCAL: ::tcflag_t = 0x8000;
pub const ECHOKE: ::tcflag_t = 0x1;
pub const ECHOE: ::tcflag_t = 0x2;
pub const ECHOK: ::tcflag_t = 0x4;
pub const ECHONL: ::tcflag_t = 0x10;
pub const ECHOPRT: ::tcflag_t = 0x20;
pub const ECHOCTL: ::tcflag_t = 0x40;
pub const ISIG: ::tcflag_t = 0x80;
pub const ICANON: ::tcflag_t = 0x100;
pub const PENDIN: ::tcflag_t = 0x20000000;
pub const NOFLSH: ::tcflag_t = 0x80000000;

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
pub const CBAUDEX: ::speed_t = 0o000020;
pub const B57600: ::speed_t = 0o0020;
pub const B115200: ::speed_t = 0o0021;
pub const B230400: ::speed_t = 0o0022;
pub const B460800: ::speed_t = 0o0023;
pub const B500000: ::speed_t = 0o0024;
pub const B576000: ::speed_t = 0o0025;
pub const B921600: ::speed_t = 0o0026;
pub const B1000000: ::speed_t = 0o0027;
pub const B1152000: ::speed_t = 0o0030;
pub const B1500000: ::speed_t = 0o0031;
pub const B2000000: ::speed_t = 0o0032;
pub const B2500000: ::speed_t = 0o0033;
pub const B3000000: ::speed_t = 0o0034;
pub const B3500000: ::speed_t = 0o0035;
pub const B4000000: ::speed_t = 0o0036;

pub const VEOL: usize = 6;
pub const VEOL2: usize = 8;
pub const VMIN: usize = 5;
pub const IEXTEN: ::tcflag_t = 0x400;
pub const TOSTOP: ::tcflag_t = 0x400000;
pub const FLUSHO: ::tcflag_t = 0x800000;
pub const EXTPROC: ::tcflag_t = 0x10000000;
pub const TCGETS: ::c_ulong = 0x403c7413;
pub const TCSETS: ::c_ulong = 0x803c7414;
pub const TCSETSW: ::c_ulong = 0x803c7415;
pub const TCSETSF: ::c_ulong = 0x803c7416;
pub const TCGETA: ::c_ulong = 0x40147417;
pub const TCSETA: ::c_ulong = 0x80147418;
pub const TCSETAW: ::c_ulong = 0x80147419;
pub const TCSETAF: ::c_ulong = 0x8014741c;
pub const TCSBRK: ::c_ulong = 0x2000741d;
pub const TCXONC: ::c_ulong = 0x2000741e;
pub const TCFLSH: ::c_ulong = 0x2000741f;
pub const TIOCINQ: ::c_ulong = 0x4004667f;
pub const TIOCGPGRP: ::c_ulong = 0x40047477;
pub const TIOCSPGRP: ::c_ulong = 0x80047476;
pub const TIOCOUTQ: ::c_ulong = 0x40047473;
pub const TIOCGWINSZ: ::c_ulong = 0x40087468;
pub const TIOCSWINSZ: ::c_ulong = 0x80087467;
pub const FIONREAD: ::c_ulong = 0x4004667f;
