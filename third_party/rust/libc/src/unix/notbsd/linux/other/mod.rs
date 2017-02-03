pub type fsblkcnt_t = ::c_ulong;
pub type fsfilcnt_t = ::c_ulong;
pub type rlim_t = c_ulong;
pub type __priority_which_t = ::c_uint;

s! {
    pub struct aiocb {
        pub aio_fildes: ::c_int,
        pub aio_lio_opcode: ::c_int,
        pub aio_reqprio: ::c_int,
        pub aio_buf: *mut ::c_void,
        pub aio_nbytes: ::size_t,
        pub aio_sigevent: ::sigevent,
        __next_prio: *mut aiocb,
        __abs_prio: ::c_int,
        __policy: ::c_int,
        __error_code: ::c_int,
        __return_value: ::ssize_t,
        pub aio_offset: off_t,
        #[cfg(target_pointer_width = "32")]
        __unused1: [::c_char; 4],
        __glibc_reserved: [::c_char; 32]
    }

    pub struct __exit_status {
        pub e_termination: ::c_short,
        pub e_exit: ::c_short,
    }

    pub struct __timeval {
        pub tv_sec: ::int32_t,
        pub tv_usec: ::int32_t,
    }

    pub struct utmpx {
        pub ut_type: ::c_short,
        pub ut_pid: ::pid_t,
        pub ut_line: [::c_char; __UT_LINESIZE],
        pub ut_id: [::c_char; 4],

        pub ut_user: [::c_char; __UT_NAMESIZE],
        pub ut_host: [::c_char; __UT_HOSTSIZE],
        pub ut_exit: __exit_status,

        #[cfg(any(target_arch = "aarch64",
                  target_arch = "sparc64",
                  target_pointer_width = "32"))]
        pub ut_session: ::c_long,
        #[cfg(any(target_arch = "aarch64",
                  target_arch = "sparc64",
                  target_pointer_width = "32"))]
        pub ut_tv: ::timeval,

        #[cfg(not(any(target_arch = "aarch64",
                      target_arch = "sparc64",
                      target_pointer_width = "32")))]
        pub ut_session: ::int32_t,
        #[cfg(not(any(target_arch = "aarch64",
                      target_arch = "sparc64",
                      target_pointer_width = "32")))]
        pub ut_tv: __timeval,

        pub ut_addr_v6: [::int32_t; 4],
        __glibc_reserved: [::c_char; 20],
    }

    pub struct sigaction {
        pub sa_sigaction: ::sighandler_t,
        pub sa_mask: ::sigset_t,
        #[cfg(target_arch = "sparc64")]
        __reserved0: ::c_int,
        pub sa_flags: ::c_int,
        _restorer: *mut ::c_void,
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

    pub struct glob64_t {
        pub gl_pathc: ::size_t,
        pub gl_pathv: *mut *mut ::c_char,
        pub gl_offs: ::size_t,
        pub gl_flags: ::c_int,

        __unused1: *mut ::c_void,
        __unused2: *mut ::c_void,
        __unused3: *mut ::c_void,
        __unused4: *mut ::c_void,
        __unused5: *mut ::c_void,
    }

    pub struct ucred {
        pub pid: ::pid_t,
        pub uid: ::uid_t,
        pub gid: ::gid_t,
    }

    pub struct statfs {
        pub f_type: __fsword_t,
        pub f_bsize: __fsword_t,
        pub f_blocks: ::fsblkcnt_t,
        pub f_bfree: ::fsblkcnt_t,
        pub f_bavail: ::fsblkcnt_t,

        pub f_files: ::fsfilcnt_t,
        pub f_ffree: ::fsfilcnt_t,
        pub f_fsid: ::fsid_t,

        pub f_namelen: __fsword_t,
        pub f_frsize: __fsword_t,
        f_spare: [__fsword_t; 5],
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
        #[cfg(not(target_arch = "sparc64"))]
        pub c_ispeed: ::speed_t,
        #[cfg(not(target_arch = "sparc64"))]
        pub c_ospeed: ::speed_t,
    }

    pub struct flock {
        pub l_type: ::c_short,
        pub l_whence: ::c_short,
        pub l_start: ::off_t,
        pub l_len: ::off_t,
        pub l_pid: ::pid_t,
    }

    // FIXME this is actually a union
    pub struct sem_t {
        #[cfg(target_pointer_width = "32")]
        __size: [::c_char; 16],
        #[cfg(target_pointer_width = "64")]
        __size: [::c_char; 32],
        __align: [::c_long; 0],
    }
}

pub const __UT_LINESIZE: usize = 32;
pub const __UT_NAMESIZE: usize = 32;
pub const __UT_HOSTSIZE: usize = 256;
pub const EMPTY: ::c_short = 0;
pub const RUN_LVL: ::c_short = 1;
pub const BOOT_TIME: ::c_short = 2;
pub const NEW_TIME: ::c_short = 3;
pub const OLD_TIME: ::c_short = 4;
pub const INIT_PROCESS: ::c_short = 5;
pub const LOGIN_PROCESS: ::c_short = 6;
pub const USER_PROCESS: ::c_short = 7;
pub const DEAD_PROCESS: ::c_short = 8;
pub const ACCOUNTING: ::c_short = 9;

pub const RLIMIT_RSS: ::c_int = 5;
pub const RLIMIT_AS: ::c_int = 9;
pub const RLIMIT_MEMLOCK: ::c_int = 8;
pub const RLIM_INFINITY: ::rlim_t = !0;
pub const RLIMIT_RTTIME: ::c_int = 15;
pub const RLIMIT_NLIMITS: ::c_int = 16;

pub const SOCK_NONBLOCK: ::c_int = O_NONBLOCK;

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
pub const MAP_DENYWRITE: ::c_int = 0x0800;
pub const MAP_EXECUTABLE: ::c_int = 0x01000;
pub const MAP_POPULATE: ::c_int = 0x08000;
pub const MAP_NONBLOCK: ::c_int = 0x010000;
pub const MAP_STACK: ::c_int = 0x020000;

pub const ENOTSUP: ::c_int = EOPNOTSUPP;
pub const EUCLEAN: ::c_int = 117;
pub const ENOTNAM: ::c_int = 118;
pub const ENAVAIL: ::c_int = 119;
pub const EISNAM: ::c_int = 120;
pub const EREMOTEIO: ::c_int = 121;

pub const SOCK_STREAM: ::c_int = 1;
pub const SOCK_DGRAM: ::c_int = 2;
pub const SOCK_SEQPACKET: ::c_int = 5;

pub const TCP_COOKIE_TRANSACTIONS: ::c_int = 15;
pub const TCP_THIN_LINEAR_TIMEOUTS: ::c_int = 16;
pub const TCP_THIN_DUPACK: ::c_int = 17;
pub const TCP_USER_TIMEOUT: ::c_int = 18;
pub const TCP_REPAIR: ::c_int = 19;
pub const TCP_REPAIR_QUEUE: ::c_int = 20;
pub const TCP_QUEUE_SEQ: ::c_int = 21;
pub const TCP_REPAIR_OPTIONS: ::c_int = 22;
pub const TCP_FASTOPEN: ::c_int = 23;
pub const TCP_TIMESTAMP: ::c_int = 24;

pub const SIGTTIN: ::c_int = 21;
pub const SIGTTOU: ::c_int = 22;
pub const SIGXCPU: ::c_int = 24;
pub const SIGXFSZ: ::c_int = 25;
pub const SIGVTALRM: ::c_int = 26;
pub const SIGPROF: ::c_int = 27;
pub const SIGWINCH: ::c_int = 28;

pub const SIGEV_THREAD_ID: ::c_int = 4;

pub const POLLRDNORM: ::c_short = 0x040;
pub const POLLRDBAND: ::c_short = 0x080;

pub const FALLOC_FL_KEEP_SIZE: ::c_int = 0x01;
pub const FALLOC_FL_PUNCH_HOLE: ::c_int = 0x02;

pub const BUFSIZ: ::c_uint = 8192;
pub const TMP_MAX: ::c_uint = 238328;
pub const FOPEN_MAX: ::c_uint = 16;
pub const POSIX_FADV_DONTNEED: ::c_int = 4;
pub const POSIX_FADV_NOREUSE: ::c_int = 5;
pub const POSIX_MADV_DONTNEED: ::c_int = 4;
pub const _SC_2_C_VERSION: ::c_int = 96;
pub const O_ACCMODE: ::c_int = 3;
pub const ST_RELATIME: ::c_ulong = 4096;
pub const NI_MAXHOST: ::socklen_t = 1025;

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

pub const VEOF: usize = 4;
pub const IUTF8: ::tcflag_t = 0x00004000;

pub const CPU_SETSIZE: ::c_int = 0x400;

pub const QFMT_VFS_V1: ::c_int = 4;

pub const PTRACE_TRACEME: ::c_uint = 0;
pub const PTRACE_PEEKTEXT: ::c_uint = 1;
pub const PTRACE_PEEKDATA: ::c_uint = 2;
pub const PTRACE_PEEKUSER: ::c_uint = 3;
pub const PTRACE_POKETEXT: ::c_uint = 4;
pub const PTRACE_POKEDATA: ::c_uint = 5;
pub const PTRACE_POKEUSER: ::c_uint = 6;
pub const PTRACE_CONT: ::c_uint = 7;
pub const PTRACE_KILL: ::c_uint = 8;
pub const PTRACE_SINGLESTEP: ::c_uint = 9;
pub const PTRACE_ATTACH: ::c_uint = 16;
pub const PTRACE_SYSCALL: ::c_uint = 24;
pub const PTRACE_SETOPTIONS: ::c_uint = 0x4200;
pub const PTRACE_GETEVENTMSG: ::c_uint = 0x4201;
pub const PTRACE_GETSIGINFO: ::c_uint = 0x4202;
pub const PTRACE_SETSIGINFO: ::c_uint = 0x4203;
pub const PTRACE_GETREGSET: ::c_uint = 0x4204;
pub const PTRACE_SETREGSET: ::c_uint = 0x4205;
pub const PTRACE_SEIZE: ::c_uint = 0x4206;
pub const PTRACE_INTERRUPT: ::c_uint = 0x4207;
pub const PTRACE_LISTEN: ::c_uint = 0x4208;
pub const PTRACE_PEEKSIGINFO: ::c_uint = 0x4209;

pub const MADV_DODUMP: ::c_int = 17;
pub const MADV_DONTDUMP: ::c_int = 16;

pub const EPOLLWAKEUP: ::c_int = 0x20000000;

pub const MADV_HUGEPAGE: ::c_int = 14;
pub const MADV_NOHUGEPAGE: ::c_int = 15;
pub const MAP_HUGETLB: ::c_int = 0x040000;

pub const SEEK_DATA: ::c_int = 3;
pub const SEEK_HOLE: ::c_int = 4;

pub const TCSANOW: ::c_int = 0;
pub const TCSADRAIN: ::c_int = 1;
pub const TCSAFLUSH: ::c_int = 2;

pub const TIOCLINUX: ::c_ulong = 0x541C;
pub const TIOCGSERIAL: ::c_ulong = 0x541E;

pub const RTLD_DEEPBIND: ::c_int = 0x8;
pub const RTLD_GLOBAL: ::c_int = 0x100;
pub const RTLD_NOLOAD: ::c_int = 0x4;

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
pub const NLM_F_DUMP_FILTERED: ::c_int = 32;

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
pub const NETLINK_LISTEN_ALL_NSID: ::c_int = 8;
pub const NETLINK_LIST_MEMBERSHIPS: ::c_int = 9;
pub const NETLINK_CAP_ACK: ::c_int = 10;

pub const NLA_F_NESTED: ::c_int = 1 << 15;
pub const NLA_F_NET_BYTEORDER: ::c_int = 1 << 14;
pub const NLA_TYPE_MASK: ::c_int = !(NLA_F_NESTED | NLA_F_NET_BYTEORDER);

cfg_if! {
    if #[cfg(any(target_arch = "arm", target_arch = "x86",
                 target_arch = "x86_64"))] {
        pub const PTHREAD_STACK_MIN: ::size_t = 16384;
    } else if #[cfg(target_arch = "sparc64")] {
        pub const PTHREAD_STACK_MIN: ::size_t = 0x6000;
    } else {
        pub const PTHREAD_STACK_MIN: ::size_t = 131072;
    }
}

extern {
    pub fn utmpxname(file: *const ::c_char) -> ::c_int;
    pub fn getutxent() -> *mut utmpx;
    pub fn getutxid(ut: *const utmpx) -> *mut utmpx;
    pub fn getutxline(ut: *const utmpx) -> *mut utmpx;
    pub fn pututxline(ut: *const utmpx) -> *mut utmpx;
    pub fn setutxent();
    pub fn endutxent();
    pub fn getpt() -> ::c_int;
}

#[link(name = "util")]
extern {
    pub fn sysctl(name: *mut ::c_int,
                  namelen: ::c_int,
                  oldp: *mut ::c_void,
                  oldlenp: *mut ::size_t,
                  newp: *mut ::c_void,
                  newlen: ::size_t)
                  -> ::c_int;
    pub fn ioctl(fd: ::c_int, request: ::c_ulong, ...) -> ::c_int;
    pub fn backtrace(buf: *mut *mut ::c_void,
                     sz: ::c_int) -> ::c_int;
    pub fn glob64(pattern: *const ::c_char,
                  flags: ::c_int,
                  errfunc: ::dox::Option<extern fn(epath: *const ::c_char,
                                                   errno: ::c_int)
                                                   -> ::c_int>,
                  pglob: *mut glob64_t) -> ::c_int;
    pub fn globfree64(pglob: *mut glob64_t);
    pub fn ptrace(request: ::c_uint, ...) -> ::c_long;
    pub fn pthread_attr_getaffinity_np(attr: *const ::pthread_attr_t,
                                       cpusetsize: ::size_t,
                                       cpuset: *mut ::cpu_set_t) -> ::c_int;
    pub fn pthread_attr_setaffinity_np(attr: *mut ::pthread_attr_t,
                                       cpusetsize: ::size_t,
                                       cpuset: *const ::cpu_set_t) -> ::c_int;
    pub fn getpriority(which: ::__priority_which_t, who: ::id_t) -> ::c_int;
    pub fn setpriority(which: ::__priority_which_t, who: ::id_t,
                                       prio: ::c_int) -> ::c_int;
    pub fn pthread_getaffinity_np(thread: ::pthread_t,
                                  cpusetsize: ::size_t,
                                  cpuset: *mut ::cpu_set_t) -> ::c_int;
    pub fn pthread_setaffinity_np(thread: ::pthread_t,
                                  cpusetsize: ::size_t,
                                  cpuset: *const ::cpu_set_t) -> ::c_int;
    pub fn sched_getcpu() -> ::c_int;
}

cfg_if! {
    if #[cfg(any(target_arch = "x86",
                 target_arch = "arm",
                 target_arch = "powerpc"))] {
        mod b32;
        pub use self::b32::*;
    } else if #[cfg(any(target_arch = "x86_64",
                        target_arch = "aarch64",
                        target_arch = "powerpc64",
                        target_arch = "sparc64"))] {
        mod b64;
        pub use self::b64::*;
    } else {
        // Unknown target_arch
    }
}
