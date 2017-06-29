pub type fflags_t = u32;
pub type clock_t = i32;
pub type ino_t = u32;
pub type lwpid_t = i32;
pub type nlink_t = u16;
pub type blksize_t = u32;
pub type clockid_t = ::c_int;
pub type sem_t = _sem;

pub type fsblkcnt_t = ::uint64_t;
pub type fsfilcnt_t = ::uint64_t;
pub type idtype_t = ::c_uint;

pub type key_t = ::c_long;
pub type msglen_t = ::c_ulong;
pub type msgqnum_t = ::c_ulong;

s! {
    pub struct utmpx {
        pub ut_type: ::c_short,
        pub ut_tv: ::timeval,
        pub ut_id: [::c_char; 8],
        pub ut_pid: ::pid_t,
        pub ut_user: [::c_char; 32],
        pub ut_line: [::c_char; 16],
        pub ut_host: [::c_char; 128],
        pub __ut_spare: [::c_char; 64],
    }

    pub struct aiocb {
        pub aio_fildes: ::c_int,
        pub aio_offset: ::off_t,
        pub aio_buf: *mut ::c_void,
        pub aio_nbytes: ::size_t,
        __unused1: [::c_int; 2],
        __unused2: *mut ::c_void,
        pub aio_lio_opcode: ::c_int,
        pub aio_reqprio: ::c_int,
        // unused 3 through 5 are the __aiocb_private structure
        __unused3: ::c_long,
        __unused4: ::c_long,
        __unused5: *mut ::c_void,
        pub aio_sigevent: sigevent
    }

    pub struct dirent {
        pub d_fileno: u32,
        pub d_reclen: u16,
        pub d_type: u8,
        pub d_namlen: u8,
        pub d_name: [::c_char; 256],
    }

    pub struct jail {
        pub version: u32,
        pub path: *mut ::c_char,
        pub hostname: *mut ::c_char,
        pub jailname: *mut ::c_char,
        pub ip4s: ::c_uint,
        pub ip6s: ::c_uint,
        pub ip4: *mut ::in_addr,
        pub ip6: *mut ::in6_addr,
    }

    pub struct sigevent {
        pub sigev_notify: ::c_int,
        pub sigev_signo: ::c_int,
        pub sigev_value: ::sigval,
        //The rest of the structure is actually a union.  We expose only
        //sigev_notify_thread_id because it's the most useful union member.
        pub sigev_notify_thread_id: ::lwpid_t,
        #[cfg(target_pointer_width = "64")]
        __unused1: ::c_int,
        __unused2: [::c_long; 7]
    }

    pub struct statvfs {
        pub f_bavail: ::fsblkcnt_t,
        pub f_bfree: ::fsblkcnt_t,
        pub f_blocks: ::fsblkcnt_t,
        pub f_favail: ::fsfilcnt_t,
        pub f_ffree: ::fsfilcnt_t,
        pub f_files: ::fsfilcnt_t,
        pub f_bsize: ::c_ulong,
        pub f_flag: ::c_ulong,
        pub f_frsize: ::c_ulong,
        pub f_fsid: ::c_ulong,
        pub f_namemax: ::c_ulong,
    }

    // internal structure has changed over time
    pub struct _sem {
        data: [u32; 4],
    }

    pub struct ipc_perm {
        pub cuid: ::uid_t,
        pub cgid: ::gid_t,
        pub uid: ::uid_t,
        pub gid: ::gid_t,
        pub mode: ::mode_t,
        pub seq: ::c_ushort,
        pub key: ::key_t,
    }

    pub struct msqid_ds {
        pub msg_perm: ::ipc_perm,
        __unused1: *mut ::c_void,
        __unused2: *mut ::c_void,
        pub msg_cbytes: ::msglen_t,
        pub msg_qnum: ::msgqnum_t,
        pub msg_qbytes: ::msglen_t,
        pub msg_lspid: ::pid_t,
        pub msg_lrpid: ::pid_t,
        pub msg_stime: ::time_t,
        pub msg_rtime: ::time_t,
        pub msg_ctime: ::time_t,
    }

    pub struct shmid_ds {
        pub shm_perm: ::ipc_perm,
        pub shm_segsz: ::size_t,
        pub shm_lpid: ::pid_t,
        pub shm_cpid: ::pid_t,
        pub shm_nattch: ::c_int,
        pub shm_atime: ::time_t,
        pub shm_dtime: ::time_t,
        pub shm_ctime: ::time_t,
    }
}

pub const SIGEV_THREAD_ID: ::c_int = 4;

pub const RAND_MAX: ::c_int = 0x7fff_fffd;
pub const PTHREAD_STACK_MIN: ::size_t = 2048;
pub const PTHREAD_MUTEX_ADAPTIVE_NP: ::c_int = 4;
pub const SIGSTKSZ: ::size_t = 34816;
pub const SF_NODISKIO: ::c_int = 0x00000001;
pub const SF_MNOWAIT: ::c_int = 0x00000002;
pub const SF_SYNC: ::c_int = 0x00000004;
pub const O_CLOEXEC: ::c_int = 0x00100000;
pub const F_GETLK: ::c_int = 11;
pub const F_SETLK: ::c_int = 12;
pub const F_SETLKW: ::c_int = 13;
pub const ELAST: ::c_int = 96;
pub const RLIMIT_NPTS: ::c_int = 11;
pub const RLIMIT_SWAP: ::c_int = 12;
pub const RLIM_NLIMITS: ::rlim_t = 13;

pub const Q_GETQUOTA: ::c_int = 0x700;
pub const Q_SETQUOTA: ::c_int = 0x800;

pub const POSIX_FADV_NORMAL: ::c_int = 0;
pub const POSIX_FADV_RANDOM: ::c_int = 1;
pub const POSIX_FADV_SEQUENTIAL: ::c_int = 2;
pub const POSIX_FADV_WILLNEED: ::c_int = 3;
pub const POSIX_FADV_DONTNEED: ::c_int = 4;
pub const POSIX_FADV_NOREUSE: ::c_int = 5;

pub const EVFILT_READ: ::int16_t = -1;
pub const EVFILT_WRITE: ::int16_t = -2;
pub const EVFILT_AIO: ::int16_t = -3;
pub const EVFILT_VNODE: ::int16_t = -4;
pub const EVFILT_PROC: ::int16_t = -5;
pub const EVFILT_SIGNAL: ::int16_t = -6;
pub const EVFILT_TIMER: ::int16_t = -7;
pub const EVFILT_FS: ::int16_t = -9;
pub const EVFILT_LIO: ::int16_t = -10;
pub const EVFILT_USER: ::int16_t = -11;

pub const EV_ADD: ::uint16_t = 0x1;
pub const EV_DELETE: ::uint16_t = 0x2;
pub const EV_ENABLE: ::uint16_t = 0x4;
pub const EV_DISABLE: ::uint16_t = 0x8;
pub const EV_ONESHOT: ::uint16_t = 0x10;
pub const EV_CLEAR: ::uint16_t = 0x20;
pub const EV_RECEIPT: ::uint16_t = 0x40;
pub const EV_DISPATCH: ::uint16_t = 0x80;
pub const EV_DROP: ::uint16_t = 0x1000;
pub const EV_FLAG1: ::uint16_t = 0x2000;
pub const EV_ERROR: ::uint16_t = 0x4000;
pub const EV_EOF: ::uint16_t = 0x8000;
pub const EV_SYSFLAGS: ::uint16_t = 0xf000;

pub const NOTE_TRIGGER: ::uint32_t = 0x01000000;
pub const NOTE_FFNOP: ::uint32_t = 0x00000000;
pub const NOTE_FFAND: ::uint32_t = 0x40000000;
pub const NOTE_FFOR: ::uint32_t = 0x80000000;
pub const NOTE_FFCOPY: ::uint32_t = 0xc0000000;
pub const NOTE_FFCTRLMASK: ::uint32_t = 0xc0000000;
pub const NOTE_FFLAGSMASK: ::uint32_t = 0x00ffffff;
pub const NOTE_LOWAT: ::uint32_t = 0x00000001;
pub const NOTE_DELETE: ::uint32_t = 0x00000001;
pub const NOTE_WRITE: ::uint32_t = 0x00000002;
pub const NOTE_EXTEND: ::uint32_t = 0x00000004;
pub const NOTE_ATTRIB: ::uint32_t = 0x00000008;
pub const NOTE_LINK: ::uint32_t = 0x00000010;
pub const NOTE_RENAME: ::uint32_t = 0x00000020;
pub const NOTE_REVOKE: ::uint32_t = 0x00000040;
pub const NOTE_EXIT: ::uint32_t = 0x80000000;
pub const NOTE_FORK: ::uint32_t = 0x40000000;
pub const NOTE_EXEC: ::uint32_t = 0x20000000;
pub const NOTE_PDATAMASK: ::uint32_t = 0x000fffff;
pub const NOTE_PCTRLMASK: ::uint32_t = 0xf0000000;
pub const NOTE_TRACK: ::uint32_t = 0x00000001;
pub const NOTE_TRACKERR: ::uint32_t = 0x00000002;
pub const NOTE_CHILD: ::uint32_t = 0x00000004;
pub const NOTE_SECONDS: ::uint32_t = 0x00000001;
pub const NOTE_MSECONDS: ::uint32_t = 0x00000002;
pub const NOTE_USECONDS: ::uint32_t = 0x00000004;
pub const NOTE_NSECONDS: ::uint32_t = 0x00000008;

pub const MADV_PROTECT: ::c_int = 10;
pub const RUSAGE_THREAD: ::c_int = 1;

pub const CLOCK_REALTIME: clockid_t = 0;
pub const CLOCK_VIRTUAL: clockid_t = 1;
pub const CLOCK_PROF: clockid_t = 2;
pub const CLOCK_MONOTONIC: clockid_t = 4;
pub const CLOCK_UPTIME: clockid_t = 5;
pub const CLOCK_UPTIME_PRECISE: clockid_t = 7;
pub const CLOCK_UPTIME_FAST: clockid_t = 8;
pub const CLOCK_REALTIME_PRECISE: clockid_t = 9;
pub const CLOCK_REALTIME_FAST: clockid_t = 10;
pub const CLOCK_MONOTONIC_PRECISE: clockid_t = 11;
pub const CLOCK_MONOTONIC_FAST: clockid_t = 12;
pub const CLOCK_SECOND: clockid_t = 13;
pub const CLOCK_THREAD_CPUTIME_ID: clockid_t = 14;
pub const CLOCK_PROCESS_CPUTIME_ID: clockid_t = 15;

pub const CTL_UNSPEC: ::c_int = 0;
pub const CTL_KERN: ::c_int = 1;
pub const CTL_VM: ::c_int = 2;
pub const CTL_VFS: ::c_int = 3;
pub const CTL_NET: ::c_int = 4;
pub const CTL_DEBUG: ::c_int = 5;
pub const CTL_HW: ::c_int = 6;
pub const CTL_MACHDEP: ::c_int = 7;
pub const CTL_USER: ::c_int = 8;
pub const CTL_P1003_1B: ::c_int = 9;
pub const KERN_OSTYPE: ::c_int = 1;
pub const KERN_OSRELEASE: ::c_int = 2;
pub const KERN_OSREV: ::c_int = 3;
pub const KERN_VERSION: ::c_int = 4;
pub const KERN_MAXVNODES: ::c_int = 5;
pub const KERN_MAXPROC: ::c_int = 6;
pub const KERN_MAXFILES: ::c_int = 7;
pub const KERN_ARGMAX: ::c_int = 8;
pub const KERN_SECURELVL: ::c_int = 9;
pub const KERN_HOSTNAME: ::c_int = 10;
pub const KERN_HOSTID: ::c_int = 11;
pub const KERN_CLOCKRATE: ::c_int = 12;
pub const KERN_VNODE: ::c_int = 13;
pub const KERN_PROC: ::c_int = 14;
pub const KERN_FILE: ::c_int = 15;
pub const KERN_PROF: ::c_int = 16;
pub const KERN_POSIX1: ::c_int = 17;
pub const KERN_NGROUPS: ::c_int = 18;
pub const KERN_JOB_CONTROL: ::c_int = 19;
pub const KERN_SAVED_IDS: ::c_int = 20;
pub const KERN_BOOTTIME: ::c_int = 21;
pub const KERN_NISDOMAINNAME: ::c_int = 22;
pub const KERN_UPDATEINTERVAL: ::c_int = 23;
pub const KERN_OSRELDATE: ::c_int = 24;
pub const KERN_NTP_PLL: ::c_int = 25;
pub const KERN_BOOTFILE: ::c_int = 26;
pub const KERN_MAXFILESPERPROC: ::c_int = 27;
pub const KERN_MAXPROCPERUID: ::c_int = 28;
pub const KERN_DUMPDEV: ::c_int = 29;
pub const KERN_IPC: ::c_int = 30;
pub const KERN_DUMMY: ::c_int = 31;
pub const KERN_PS_STRINGS: ::c_int = 32;
pub const KERN_USRSTACK: ::c_int = 33;
pub const KERN_LOGSIGEXIT: ::c_int = 34;
pub const KERN_IOV_MAX: ::c_int = 35;
pub const KERN_HOSTUUID: ::c_int = 36;
pub const KERN_ARND: ::c_int = 37;
pub const KERN_PROC_ALL: ::c_int = 0;
pub const KERN_PROC_PID: ::c_int = 1;
pub const KERN_PROC_PGRP: ::c_int = 2;
pub const KERN_PROC_SESSION: ::c_int = 3;
pub const KERN_PROC_TTY: ::c_int = 4;
pub const KERN_PROC_UID: ::c_int = 5;
pub const KERN_PROC_RUID: ::c_int = 6;
pub const KERN_PROC_ARGS: ::c_int = 7;
pub const KERN_PROC_PROC: ::c_int = 8;
pub const KERN_PROC_SV_NAME: ::c_int = 9;
pub const KERN_PROC_RGID: ::c_int = 10;
pub const KERN_PROC_GID: ::c_int = 11;
pub const KERN_PROC_PATHNAME: ::c_int = 12;
pub const KERN_PROC_OVMMAP: ::c_int = 13;
pub const KERN_PROC_OFILEDESC: ::c_int = 14;
pub const KERN_PROC_KSTACK: ::c_int = 15;
pub const KERN_PROC_INC_THREAD: ::c_int = 0x10;
pub const KERN_PROC_VMMAP: ::c_int = 32;
pub const KERN_PROC_FILEDESC: ::c_int = 33;
pub const KERN_PROC_GROUPS: ::c_int = 34;
pub const KERN_PROC_ENV: ::c_int = 35;
pub const KERN_PROC_AUXV: ::c_int = 36;
pub const KERN_PROC_RLIMIT: ::c_int = 37;
pub const KERN_PROC_PS_STRINGS: ::c_int = 38;
pub const KERN_PROC_UMASK: ::c_int = 39;
pub const KERN_PROC_OSREL: ::c_int = 40;
pub const KERN_PROC_SIGTRAMP: ::c_int = 41;
pub const KIPC_MAXSOCKBUF: ::c_int = 1;
pub const KIPC_SOCKBUF_WASTE: ::c_int = 2;
pub const KIPC_SOMAXCONN: ::c_int = 3;
pub const KIPC_MAX_LINKHDR: ::c_int = 4;
pub const KIPC_MAX_PROTOHDR: ::c_int = 5;
pub const KIPC_MAX_HDR: ::c_int = 6;
pub const KIPC_MAX_DATALEN: ::c_int = 7;
pub const HW_MACHINE: ::c_int = 1;
pub const HW_MODEL: ::c_int = 2;
pub const HW_NCPU: ::c_int = 3;
pub const HW_BYTEORDER: ::c_int = 4;
pub const HW_PHYSMEM: ::c_int = 5;
pub const HW_USERMEM: ::c_int = 6;
pub const HW_PAGESIZE: ::c_int = 7;
pub const HW_DISKNAMES: ::c_int = 8;
pub const HW_DISKSTATS: ::c_int = 9;
pub const HW_FLOATINGPT: ::c_int = 10;
pub const HW_MACHINE_ARCH: ::c_int = 11;
pub const HW_REALMEM: ::c_int = 12;
pub const USER_CS_PATH: ::c_int = 1;
pub const USER_BC_BASE_MAX: ::c_int = 2;
pub const USER_BC_DIM_MAX: ::c_int = 3;
pub const USER_BC_SCALE_MAX: ::c_int = 4;
pub const USER_BC_STRING_MAX: ::c_int = 5;
pub const USER_COLL_WEIGHTS_MAX: ::c_int = 6;
pub const USER_EXPR_NEST_MAX: ::c_int = 7;
pub const USER_LINE_MAX: ::c_int = 8;
pub const USER_RE_DUP_MAX: ::c_int = 9;
pub const USER_POSIX2_VERSION: ::c_int = 10;
pub const USER_POSIX2_C_BIND: ::c_int = 11;
pub const USER_POSIX2_C_DEV: ::c_int = 12;
pub const USER_POSIX2_CHAR_TERM: ::c_int = 13;
pub const USER_POSIX2_FORT_DEV: ::c_int = 14;
pub const USER_POSIX2_FORT_RUN: ::c_int = 15;
pub const USER_POSIX2_LOCALEDEF: ::c_int = 16;
pub const USER_POSIX2_SW_DEV: ::c_int = 17;
pub const USER_POSIX2_UPE: ::c_int = 18;
pub const USER_STREAM_MAX: ::c_int = 19;
pub const USER_TZNAME_MAX: ::c_int = 20;
pub const CTL_P1003_1B_ASYNCHRONOUS_IO: ::c_int = 1;
pub const CTL_P1003_1B_MAPPED_FILES: ::c_int = 2;
pub const CTL_P1003_1B_MEMLOCK: ::c_int = 3;
pub const CTL_P1003_1B_MEMLOCK_RANGE: ::c_int = 4;
pub const CTL_P1003_1B_MEMORY_PROTECTION: ::c_int = 5;
pub const CTL_P1003_1B_MESSAGE_PASSING: ::c_int = 6;
pub const CTL_P1003_1B_PRIORITIZED_IO: ::c_int = 7;
pub const CTL_P1003_1B_PRIORITY_SCHEDULING: ::c_int = 8;
pub const CTL_P1003_1B_REALTIME_SIGNALS: ::c_int = 9;
pub const CTL_P1003_1B_SEMAPHORES: ::c_int = 10;
pub const CTL_P1003_1B_FSYNC: ::c_int = 11;
pub const CTL_P1003_1B_SHARED_MEMORY_OBJECTS: ::c_int = 12;
pub const CTL_P1003_1B_SYNCHRONIZED_IO: ::c_int = 13;
pub const CTL_P1003_1B_TIMERS: ::c_int = 14;
pub const CTL_P1003_1B_AIO_LISTIO_MAX: ::c_int = 15;
pub const CTL_P1003_1B_AIO_MAX: ::c_int = 16;
pub const CTL_P1003_1B_AIO_PRIO_DELTA_MAX: ::c_int = 17;
pub const CTL_P1003_1B_DELAYTIMER_MAX: ::c_int = 18;
pub const CTL_P1003_1B_MQ_OPEN_MAX: ::c_int = 19;
pub const CTL_P1003_1B_PAGESIZE: ::c_int = 20;
pub const CTL_P1003_1B_RTSIG_MAX: ::c_int = 21;
pub const CTL_P1003_1B_SEM_NSEMS_MAX: ::c_int = 22;
pub const CTL_P1003_1B_SEM_VALUE_MAX: ::c_int = 23;
pub const CTL_P1003_1B_SIGQUEUE_MAX: ::c_int = 24;
pub const CTL_P1003_1B_TIMER_MAX: ::c_int = 25;
pub const TIOCGPTN: ::c_uint = 0x4004740f;
pub const TIOCPTMASTER: ::c_uint = 0x2000741c;
pub const TIOCSIG: ::c_uint = 0x2004745f;
pub const TIOCM_DCD: ::c_int = 0x40;
pub const H4DISC: ::c_int = 0x7;

pub const JAIL_API_VERSION: u32 = 2;
pub const JAIL_CREATE: ::c_int = 0x01;
pub const JAIL_UPDATE: ::c_int = 0x02;
pub const JAIL_ATTACH: ::c_int = 0x04;
pub const JAIL_DYING: ::c_int = 0x08;
pub const JAIL_SET_MASK: ::c_int = 0x0f;
pub const JAIL_GET_MASK: ::c_int = 0x08;
pub const JAIL_SYS_DISABLE: ::c_int = 0;
pub const JAIL_SYS_NEW: ::c_int = 1;
pub const JAIL_SYS_INHERIT: ::c_int = 2;

pub const SO_BINTIME: ::c_int = 0x2000;
pub const SO_NO_OFFLOAD: ::c_int = 0x4000;
pub const SO_NO_DDP: ::c_int = 0x8000;
pub const SO_LABEL: ::c_int = 0x1009;
pub const SO_PEERLABEL: ::c_int = 0x1010;
pub const SO_LISTENQLIMIT: ::c_int = 0x1011;
pub const SO_LISTENQLEN: ::c_int = 0x1012;
pub const SO_LISTENINCQLEN: ::c_int = 0x1013;
pub const SO_SETFIB: ::c_int = 0x1014;
pub const SO_USER_COOKIE: ::c_int = 0x1015;
pub const SO_PROTOCOL: ::c_int = 0x1016;
pub const SO_PROTOTYPE: ::c_int = SO_PROTOCOL;
pub const SO_VENDOR: ::c_int = 0x80000000;

pub const AF_SLOW: ::c_int = 33;
pub const AF_SCLUSTER: ::c_int = 34;
pub const AF_ARP: ::c_int = 35;
pub const AF_BLUETOOTH: ::c_int = 36;
pub const AF_IEEE80211: ::c_int = 37;
pub const AF_INET_SDP: ::c_int = 40;
pub const AF_INET6_SDP: ::c_int = 42;
#[doc(hidden)]
pub const AF_MAX: ::c_int = 42;

pub const IPPROTO_DIVERT: ::c_int = 258;

pub const PF_SLOW: ::c_int = AF_SLOW;
pub const PF_SCLUSTER: ::c_int = AF_SCLUSTER;
pub const PF_ARP: ::c_int = AF_ARP;
pub const PF_BLUETOOTH: ::c_int = AF_BLUETOOTH;
pub const PF_IEEE80211: ::c_int = AF_IEEE80211;
pub const PF_INET_SDP: ::c_int = AF_INET_SDP;
pub const PF_INET6_SDP: ::c_int = AF_INET6_SDP;
#[doc(hidden)]
pub const PF_MAX: ::c_int = AF_MAX;

pub const NET_RT_DUMP: ::c_int = 1;
pub const NET_RT_FLAGS: ::c_int = 2;
pub const NET_RT_IFLIST: ::c_int = 3;
pub const NET_RT_IFMALIST: ::c_int = 4;
pub const NET_RT_IFLISTL: ::c_int = 5;

// System V IPC
pub const IPC_PRIVATE: ::key_t = 0;
pub const IPC_CREAT: ::c_int = 0o1000;
pub const IPC_EXCL: ::c_int = 0o2000;
pub const IPC_NOWAIT: ::c_int = 0o4000;
pub const IPC_RMID: ::c_int = 0;
pub const IPC_SET: ::c_int = 1;
pub const IPC_STAT: ::c_int = 2;
pub const IPC_INFO: ::c_int = 3;
pub const IPC_R : ::c_int = 0o400;
pub const IPC_W : ::c_int = 0o200;
pub const IPC_M : ::c_int = 0o10000;
pub const MSG_NOERROR: ::c_int = 0o10000;
pub const SHM_RDONLY: ::c_int = 0o10000;
pub const SHM_RND: ::c_int = 0o20000;
pub const SHM_R: ::c_int = 0o400;
pub const SHM_W: ::c_int = 0o200;
pub const SHM_LOCK: ::c_int = 11;
pub const SHM_UNLOCK: ::c_int = 12;
pub const SHM_STAT: ::c_int = 13;
pub const SHM_INFO: ::c_int = 14;

// The *_MAXID constants never should've been used outside of the
// FreeBSD base system.  And with the exception of CTL_P1003_1B_MAXID,
// they were all removed in svn r262489.  They remain here for backwards
// compatibility only, and are scheduled to be removed in libc 1.0.0.
#[doc(hidden)]
pub const NET_MAXID: ::c_int = AF_MAX;
#[doc(hidden)]
pub const CTL_MAXID: ::c_int = 10;
#[doc(hidden)]
pub const KERN_MAXID: ::c_int = 38;
#[doc(hidden)]
pub const HW_MAXID: ::c_int = 13;
#[doc(hidden)]
pub const USER_MAXID: ::c_int = 21;
#[doc(hidden)]
pub const CTL_P1003_1B_MAXID: ::c_int = 26;

pub const MSG_NOTIFICATION: ::c_int = 0x00002000;
pub const MSG_NBIO: ::c_int = 0x00004000;
pub const MSG_COMPAT: ::c_int = 0x00008000;
pub const MSG_CMSG_CLOEXEC: ::c_int = 0x00040000;
pub const MSG_NOSIGNAL: ::c_int = 0x20000;

pub const EMPTY: ::c_short = 0;
pub const BOOT_TIME: ::c_short = 1;
pub const OLD_TIME: ::c_short = 2;
pub const NEW_TIME: ::c_short = 3;
pub const USER_PROCESS: ::c_short = 4;
pub const INIT_PROCESS: ::c_short = 5;
pub const LOGIN_PROCESS: ::c_short = 6;
pub const DEAD_PROCESS: ::c_short = 7;
pub const SHUTDOWN_TIME: ::c_short = 8;

pub const LC_COLLATE_MASK: ::c_int = (1 << 0);
pub const LC_CTYPE_MASK: ::c_int = (1 << 1);
pub const LC_MESSAGES_MASK: ::c_int = (1 << 2);
pub const LC_MONETARY_MASK: ::c_int = (1 << 3);
pub const LC_NUMERIC_MASK: ::c_int = (1 << 4);
pub const LC_TIME_MASK: ::c_int = (1 << 5);
pub const LC_ALL_MASK: ::c_int = LC_COLLATE_MASK
                               | LC_CTYPE_MASK
                               | LC_MESSAGES_MASK
                               | LC_MONETARY_MASK
                               | LC_NUMERIC_MASK
                               | LC_TIME_MASK;

pub const WSTOPPED: ::c_int = 2; // same as WUNTRACED
pub const WCONTINUED: ::c_int = 4;
pub const WNOWAIT: ::c_int = 8;
pub const WEXITED: ::c_int = 16;
pub const WTRAPPED: ::c_int = 32;

// FreeBSD defines a great many more of these, we only expose the
// standardized ones.
pub const P_PID: idtype_t = 0;
pub const P_PGID: idtype_t = 2;
pub const P_ALL: idtype_t = 7;

pub const B460800: ::speed_t = 460800;
pub const B921600: ::speed_t = 921600;

extern {
    pub fn __error() -> *mut ::c_int;

    pub fn mprotect(addr: *const ::c_void, len: ::size_t, prot: ::c_int)
                    -> ::c_int;

    pub fn clock_getres(clk_id: clockid_t, tp: *mut ::timespec) -> ::c_int;
    pub fn clock_gettime(clk_id: clockid_t, tp: *mut ::timespec) -> ::c_int;
    pub fn clock_settime(clk_id: clockid_t, tp: *const ::timespec) -> ::c_int;

    pub fn jail(jail: *mut ::jail) -> ::c_int;
    pub fn jail_attach(jid: ::c_int) -> ::c_int;
    pub fn jail_remove(jid: ::c_int) -> ::c_int;
    pub fn jail_get(iov: *mut ::iovec, niov: ::c_uint, flags: ::c_int)
                    -> ::c_int;
    pub fn jail_set(iov: *mut ::iovec, niov: ::c_uint, flags: ::c_int)
                    -> ::c_int;

    pub fn posix_fallocate(fd: ::c_int, offset: ::off_t,
                           len: ::off_t) -> ::c_int;
    pub fn posix_fadvise(fd: ::c_int, offset: ::off_t, len: ::off_t,
                         advise: ::c_int) -> ::c_int;
    pub fn mkostemp(template: *mut ::c_char, flags: ::c_int) -> ::c_int;
    pub fn mkostemps(template: *mut ::c_char,
                     suffixlen: ::c_int,
                     flags: ::c_int) -> ::c_int;

    pub fn getutxuser(user: *const ::c_char) -> *mut utmpx;
    pub fn setutxdb(_type: ::c_int, file: *const ::c_char) -> ::c_int;

    pub fn aio_waitcomplete(iocbp: *mut *mut aiocb,
                            timeout: *mut ::timespec) -> ::ssize_t;

    pub fn freelocale(loc: ::locale_t) -> ::c_int;
    pub fn waitid(idtype: idtype_t, id: ::id_t, infop: *mut ::siginfo_t,
                  options: ::c_int) -> ::c_int;

    pub fn ftok(pathname: *const ::c_char, proj_id: ::c_int) -> ::key_t;
    pub fn shmget(key: ::key_t, size: ::size_t, shmflg: ::c_int) -> ::c_int;
    pub fn shmat(shmid: ::c_int, shmaddr: *const ::c_void,
        shmflg: ::c_int) -> *mut ::c_void;
    pub fn shmdt(shmaddr: *const ::c_void) -> ::c_int;
    pub fn shmctl(shmid: ::c_int, cmd: ::c_int,
        buf: *mut ::shmid_ds) -> ::c_int;
    pub fn msgctl(msqid: ::c_int, cmd: ::c_int,
        buf: *mut ::msqid_ds) -> ::c_int;
    pub fn msgget(key: ::key_t, msgflg: ::c_int) -> ::c_int;
    pub fn msgrcv(msqid: ::c_int, msgp: *mut ::c_void, msgsz: ::size_t,
        msgtyp: ::c_long, msgflg: ::c_int) -> ::c_int;
    pub fn msgsnd(msqid: ::c_int, msgp: *const ::c_void, msgsz: ::size_t,
        msgflg: ::c_int) -> ::c_int;
}

cfg_if! {
    if #[cfg(target_arch = "x86")] {
        mod x86;
        pub use self::x86::*;
    } else if #[cfg(target_arch = "x86_64")] {
        mod x86_64;
        pub use self::x86_64::*;
    } else if #[cfg(target_arch = "aarch64")] {
        mod aarch64;
        pub use self::aarch64::*;
    } else {
        // Unknown target_arch
    }
}
