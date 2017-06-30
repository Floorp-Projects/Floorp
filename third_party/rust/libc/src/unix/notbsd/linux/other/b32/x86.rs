pub type c_char = i8;
pub type wchar_t = i32;
pub type greg_t = i32;

s! {
    pub struct _libc_fpreg {
        pub significand: [u16; 4],
        pub exponent: u16,
    }

    pub struct _libc_fpstate {
        pub cw: ::c_ulong,
        pub sw: ::c_ulong,
        pub tag: ::c_ulong,
        pub ipoff: ::c_ulong,
        pub cssel: ::c_ulong,
        pub dataoff: ::c_ulong,
        pub datasel: ::c_ulong,
        pub _st: [_libc_fpreg; 8],
        pub status: ::c_ulong,
    }

    pub struct user_fpregs_struct {
        pub cwd: ::c_long,
        pub swd: ::c_long,
        pub twd: ::c_long,
        pub fip: ::c_long,
        pub fcs: ::c_long,
        pub foo: ::c_long,
        pub fos: ::c_long,
        pub st_space: [::c_long; 20],
    }

    pub struct user_fpxregs_struct {
        pub cwd: ::c_ushort,
        pub swd: ::c_ushort,
        pub twd: ::c_ushort,
        pub fop: ::c_ushort,
        pub fip: ::c_long,
        pub fcs: ::c_long,
        pub foo: ::c_long,
        pub fos: ::c_long,
        pub mxcsr: ::c_long,
        __reserved: ::c_long,
        pub st_space: [::c_long; 32],
        pub xmm_space: [::c_long; 32],
        padding: [::c_long; 56],
    }

    pub struct user_regs_struct {
        pub ebx: ::c_long,
        pub ecx: ::c_long,
        pub edx: ::c_long,
        pub esi: ::c_long,
        pub edi: ::c_long,
        pub ebp: ::c_long,
        pub eax: ::c_long,
        pub xds: ::c_long,
        pub xes: ::c_long,
        pub xfs: ::c_long,
        pub xgs: ::c_long,
        pub orig_eax: ::c_long,
        pub eip: ::c_long,
        pub xcs: ::c_long,
        pub eflags: ::c_long,
        pub esp: ::c_long,
        pub xss: ::c_long,
    }

    pub struct user {
        pub regs: user_regs_struct,
        pub u_fpvalid: ::c_int,
        pub i387: user_fpregs_struct,
        pub u_tsize: ::c_ulong,
        pub u_dsize: ::c_ulong,
        pub u_ssize: ::c_ulong,
        pub start_code: ::c_ulong,
        pub start_stack: ::c_ulong,
        pub signal: ::c_long,
        __reserved: ::c_int,
        pub u_ar0: *mut user_regs_struct,
        pub u_fpstate: *mut user_fpregs_struct,
        pub magic: ::c_ulong,
        pub u_comm: [c_char; 32],
        pub u_debugreg: [::c_int; 8],
    }

    pub struct mcontext_t {
        pub gregs: [greg_t; 19],
        pub fpregs: *mut _libc_fpstate,
        pub oldmask: ::c_ulong,
        pub cr2: ::c_ulong,
    }

    pub struct ucontext_t {
        pub uc_flags: ::c_ulong,
        pub uc_link: *mut ucontext_t,
        pub uc_stack: ::stack_t,
        pub uc_mcontext: mcontext_t,
        pub uc_sigmask: ::sigset_t,
        __private: [u8; 112],
    }

    pub struct ipc_perm {
        pub __key: ::key_t,
        pub uid: ::uid_t,
        pub gid: ::gid_t,
        pub cuid: ::uid_t,
        pub cgid: ::gid_t,
        pub mode: ::c_ushort,
        __pad1: ::c_ushort,
        pub __seq: ::c_ushort,
        __pad2: ::c_ushort,
        __unused1: ::c_ulong,
        __unused2: ::c_ulong
    }

    pub struct stat64 {
        pub st_dev: ::dev_t,
        __pad1: ::c_uint,
        __st_ino: ::ino_t,
        pub st_mode: ::mode_t,
        pub st_nlink: ::nlink_t,
        pub st_uid: ::uid_t,
        pub st_gid: ::gid_t,
        pub st_rdev: ::dev_t,
        __pad2: ::c_uint,
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

    pub struct shmid_ds {
        pub shm_perm: ::ipc_perm,
        pub shm_segsz: ::size_t,
        pub shm_atime: ::time_t,
        __unused1: ::c_ulong,
        pub shm_dtime: ::time_t,
        __unused2: ::c_ulong,
        pub shm_ctime: ::time_t,
        __unused3: ::c_ulong,
        pub shm_cpid: ::pid_t,
        pub shm_lpid: ::pid_t,
        pub shm_nattch: ::shmatt_t,
        __unused4: ::c_ulong,
        __unused5: ::c_ulong
    }

    pub struct msqid_ds {
        pub msg_perm: ::ipc_perm,
        pub msg_stime: ::time_t,
        __glibc_reserved1: ::c_ulong,
        pub msg_rtime: ::time_t,
        __glibc_reserved2: ::c_ulong,
        pub msg_ctime: ::time_t,
        __glibc_reserved3: ::c_ulong,
        __msg_cbytes: ::c_ulong,
        pub msg_qnum: ::msgqnum_t,
        pub msg_qbytes: ::msglen_t,
        pub msg_lspid: ::pid_t,
        pub msg_lrpid: ::pid_t,
        __glibc_reserved4: ::c_ulong,
        __glibc_reserved5: ::c_ulong,
    }
}

pub const O_DIRECT: ::c_int = 0x4000;
pub const O_DIRECTORY: ::c_int = 0x10000;
pub const O_NOFOLLOW: ::c_int = 0x20000;

pub const MAP_LOCKED: ::c_int = 0x02000;
pub const MAP_NORESERVE: ::c_int = 0x04000;
pub const MAP_32BIT: ::c_int = 0x0040;

pub const EDEADLOCK: ::c_int = 35;

pub const SO_SNDBUFFORCE: ::c_int = 32;
pub const SO_RCVBUFFORCE: ::c_int = 33;
pub const SO_NO_CHECK: ::c_int = 11;
pub const SO_PRIORITY: ::c_int = 12;
pub const SO_BSDCOMPAT: ::c_int = 14;
pub const SO_PASSCRED: ::c_int = 16;
pub const SO_PEERCRED: ::c_int = 17;
pub const SO_RCVLOWAT: ::c_int = 18;
pub const SO_SNDLOWAT: ::c_int = 19;
pub const SO_RCVTIMEO: ::c_int = 20;
pub const SO_SNDTIMEO: ::c_int = 21;

pub const FIOCLEX: ::c_ulong = 0x5451;
pub const FIONBIO: ::c_ulong = 0x5421;

pub const SYS_gettid: ::c_long = 224;
pub const SYS_perf_event_open: ::c_long = 336;

pub const PTRACE_GETFPXREGS: ::c_uint = 18;
pub const PTRACE_SETFPXREGS: ::c_uint = 19;

pub const MCL_CURRENT: ::c_int = 0x0001;
pub const MCL_FUTURE: ::c_int = 0x0002;

pub const SIGSTKSZ: ::size_t = 8192;
pub const MINSIGSTKSZ: ::size_t = 2048;
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

pub const VEOL: usize = 11;
pub const VEOL2: usize = 16;
pub const VMIN: usize = 6;
pub const IEXTEN: ::tcflag_t = 0x00008000;
pub const TOSTOP: ::tcflag_t = 0x00000100;
pub const FLUSHO: ::tcflag_t = 0x00001000;
pub const EXTPROC: ::tcflag_t = 0x00010000;
pub const TCGETS: ::c_ulong = 0x5401;
pub const TCSETS: ::c_ulong = 0x5402;
pub const TCSETSW: ::c_ulong = 0x5403;
pub const TCSETSF: ::c_ulong = 0x5404;
pub const TCGETA: ::c_ulong = 0x5405;
pub const TCSETA: ::c_ulong = 0x5406;
pub const TCSETAW: ::c_ulong = 0x5407;
pub const TCSETAF: ::c_ulong = 0x5408;
pub const TCSBRK: ::c_ulong = 0x5409;
pub const TCXONC: ::c_ulong = 0x540A;
pub const TCFLSH: ::c_ulong = 0x540B;
pub const TIOCINQ: ::c_ulong = 0x541B;
pub const TIOCGPGRP: ::c_ulong = 0x540F;
pub const TIOCSPGRP: ::c_ulong = 0x5410;
pub const TIOCOUTQ: ::c_ulong = 0x5411;
pub const TIOCGWINSZ: ::c_ulong = 0x5413;
pub const TIOCSWINSZ: ::c_ulong = 0x5414;
pub const FIONREAD: ::c_ulong = 0x541B;

extern {
    pub fn getcontext(ucp: *mut ucontext_t) -> ::c_int;
    pub fn setcontext(ucp: *const ucontext_t) -> ::c_int;
    pub fn makecontext(ucp: *mut ucontext_t,
                       func:  extern fn (),
                       argc: ::c_int, ...);
    pub fn swapcontext(uocp: *mut ucontext_t,
                       ucp: *const ucontext_t) -> ::c_int;
}
