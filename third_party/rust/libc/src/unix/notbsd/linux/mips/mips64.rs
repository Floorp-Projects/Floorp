use pthread_mutex_t;

pub type blkcnt_t = i64;
pub type blksize_t = i64;
pub type c_char = i8;
pub type c_long = i64;
pub type c_ulong = u64;
pub type ino_t = u64;
pub type nlink_t = u64;
pub type off_t = i64;
pub type suseconds_t = i64;
pub type time_t = i64;
pub type wchar_t = i32;
pub type clock_t = i64;
pub type __u64 = ::c_ulong;

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
        __glibc_reserved: [::c_char; 32]
    }

    pub struct stat {
        pub st_dev: ::c_ulong,
        st_pad1: [::c_long; 2],
        pub st_ino: ::ino_t,
        pub st_mode: ::mode_t,
        pub st_nlink: ::nlink_t,
        pub st_uid: ::uid_t,
        pub st_gid: ::gid_t,
        pub st_rdev: ::c_ulong,
        st_pad2: [::c_ulong; 1],
        pub st_size: ::off_t,
        st_pad3: ::c_long,
        pub st_atime: ::time_t,
        pub st_atime_nsec: ::c_long,
        pub st_mtime: ::time_t,
        pub st_mtime_nsec: ::c_long,
        pub st_ctime: ::time_t,
        pub st_ctime_nsec: ::c_long,
        pub st_blksize: ::blksize_t,
        st_pad4: ::c_long,
        pub st_blocks: ::blkcnt_t,
        st_pad5: [::c_long; 7],
    }

    pub struct stat64 {
        pub st_dev: ::c_ulong,
        st_pad1: [::c_long; 2],
        pub st_ino: ::ino64_t,
        pub st_mode: ::mode_t,
        pub st_nlink: ::nlink_t,
        pub st_uid: ::uid_t,
        pub st_gid: ::gid_t,
        pub st_rdev: ::c_ulong,
        st_pad2: [::c_long; 2],
        pub st_size: ::off64_t,
        pub st_atime: ::time_t,
        pub st_atime_nsec: ::c_long,
        pub st_mtime: ::time_t,
        pub st_mtime_nsec: ::c_long,
        pub st_ctime: ::time_t,
        pub st_ctime_nsec: ::c_long,
        pub st_blksize: ::blksize_t,
        st_pad3: ::c_long,
        pub st_blocks: ::blkcnt64_t,
        st_pad5: [::c_long; 7],
    }

    pub struct statfs64 {
        pub f_type: ::c_long,
        pub f_bsize: ::c_long,
        pub f_frsize: ::c_long,
        pub f_blocks: u64,
        pub f_bfree: u64,
        pub f_files: u64,
        pub f_ffree: u64,
        pub f_bavail: u64,
        pub f_fsid: ::fsid_t,
        pub f_namelen: ::c_long,
        pub f_flags: ::c_long,
        pub f_spare: [::c_long; 5],
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
        __f_spare: [::c_int; 6],
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
        pub f_flag: ::c_ulong,
        pub f_namemax: ::c_ulong,
        __f_spare: [::c_int; 6],
    }

    pub struct pthread_attr_t {
        __size: [::c_ulong; 7]
    }

    pub struct sigaction {
        pub sa_flags: ::c_int,
        pub sa_sigaction: ::sighandler_t,
        pub sa_mask: sigset_t,
        pub sa_restorer: ::Option<extern fn()>,
    }

    pub struct stack_t {
        pub ss_sp: *mut ::c_void,
        pub ss_size: ::size_t,
        pub ss_flags: ::c_int,
    }

    pub struct sigset_t {
        __size: [::c_ulong; 16],
    }

    pub struct siginfo_t {
        pub si_signo: ::c_int,
        pub si_code: ::c_int,
        pub si_errno: ::c_int,
        _pad: ::c_int,
        _pad2: [::c_long; 14],
    }

    pub struct ipc_perm {
        pub __key: ::key_t,
        pub uid: ::uid_t,
        pub gid: ::gid_t,
        pub cuid: ::uid_t,
        pub cgid: ::gid_t,
        pub mode: ::c_uint,
        pub __seq: ::c_ushort,
        __pad1: ::c_ushort,
        __unused1: ::c_ulong,
        __unused2: ::c_ulong
    }

    pub struct shmid_ds {
        pub shm_perm: ::ipc_perm,
        pub shm_segsz: ::size_t,
        pub shm_atime: ::time_t,
        pub shm_dtime: ::time_t,
        pub shm_ctime: ::time_t,
        pub shm_cpid: ::pid_t,
        pub shm_lpid: ::pid_t,
        pub shm_nattch: ::shmatt_t,
        __unused4: ::c_ulong,
        __unused5: ::c_ulong
    }

    pub struct msqid_ds {
        pub msg_perm: ::ipc_perm,
        pub msg_stime: ::time_t,
        pub msg_rtime: ::time_t,
        pub msg_ctime: ::time_t,
        __msg_cbytes: ::c_ulong,
        pub msg_qnum: ::msgqnum_t,
        pub msg_qbytes: ::msglen_t,
        pub msg_lspid: ::pid_t,
        pub msg_lrpid: ::pid_t,
        __glibc_reserved4: ::c_ulong,
        __glibc_reserved5: ::c_ulong,
    }

    pub struct statfs {
        pub f_type: ::c_long,
        pub f_bsize: ::c_long,
        pub f_frsize: ::c_long,
        pub f_blocks: ::fsblkcnt_t,
        pub f_bfree: ::fsblkcnt_t,
        pub f_files: ::fsblkcnt_t,
        pub f_ffree: ::fsblkcnt_t,
        pub f_bavail: ::fsblkcnt_t,
        pub f_fsid: ::fsid_t,

        pub f_namelen: ::c_long,
        f_spare: [::c_long; 6],
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
        pub _f: [::c_char; 0],
    }
}

pub const __SIZEOF_PTHREAD_CONDATTR_T: usize = 4;
pub const __SIZEOF_PTHREAD_MUTEXATTR_T: usize = 4;
pub const __SIZEOF_PTHREAD_MUTEX_T: usize = 40;
pub const __SIZEOF_PTHREAD_RWLOCK_T: usize = 56;
pub const __SIZEOF_PTHREAD_RWLOCKATTR_T: usize = 8;

align_const! {
    #[cfg(target_endian = "little")]
    pub const PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP: ::pthread_mutex_t =
        pthread_mutex_t {
            size: [
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            ],
        };
    #[cfg(target_endian = "little")]
    pub const PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP: ::pthread_mutex_t =
        pthread_mutex_t {
            size: [
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            ],
        };
    #[cfg(target_endian = "little")]
    pub const PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP: ::pthread_mutex_t =
        pthread_mutex_t {
            size: [
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            ],
        };
    #[cfg(target_endian = "big")]
    pub const PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP: ::pthread_mutex_t =
        pthread_mutex_t {
            size: [
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            ],
        };
    #[cfg(target_endian = "big")]
    pub const PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP: ::pthread_mutex_t =
        pthread_mutex_t {
            size: [
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            ],
        };
    #[cfg(target_endian = "big")]
    pub const PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP: ::pthread_mutex_t =
        pthread_mutex_t {
            size: [
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            ],
        };
}

pub const O_LARGEFILE: ::c_int = 0;

pub const RLIM_INFINITY: ::rlim_t = 0xffff_ffff_ffff_ffff;

pub const SYS_read: ::c_long = 5000 + 0;
pub const SYS_write: ::c_long = 5000 + 1;
pub const SYS_open: ::c_long = 5000 + 2;
pub const SYS_close: ::c_long = 5000 + 3;
pub const SYS_stat: ::c_long = 5000 + 4;
pub const SYS_fstat: ::c_long = 5000 + 5;
pub const SYS_lstat: ::c_long = 5000 + 6;
pub const SYS_poll: ::c_long = 5000 + 7;
pub const SYS_lseek: ::c_long = 5000 + 8;
pub const SYS_mmap: ::c_long = 5000 + 9;
pub const SYS_mprotect: ::c_long = 5000 +  10;
pub const SYS_munmap: ::c_long = 5000 +  11;
pub const SYS_brk: ::c_long = 5000 +  12;
pub const SYS_rt_sigaction: ::c_long = 5000 +  13;
pub const SYS_rt_sigprocmask: ::c_long = 5000 +  14;
pub const SYS_ioctl: ::c_long = 5000 +  15;
pub const SYS_pread64: ::c_long = 5000 +  16;
pub const SYS_pwrite64: ::c_long = 5000 +  17;
pub const SYS_readv: ::c_long = 5000 +  18;
pub const SYS_writev: ::c_long = 5000 +  19;
pub const SYS_access: ::c_long = 5000 +  20;
pub const SYS_pipe: ::c_long = 5000 +  21;
pub const SYS__newselect: ::c_long = 5000 +  22;
pub const SYS_sched_yield: ::c_long = 5000 +  23;
pub const SYS_mremap: ::c_long = 5000 +  24;
pub const SYS_msync: ::c_long = 5000 +  25;
pub const SYS_mincore: ::c_long = 5000 +  26;
pub const SYS_madvise: ::c_long = 5000 +  27;
pub const SYS_shmget: ::c_long = 5000 +  28;
pub const SYS_shmat: ::c_long = 5000 +  29;
pub const SYS_shmctl: ::c_long = 5000 +  30;
pub const SYS_dup: ::c_long = 5000 +  31;
pub const SYS_dup2: ::c_long = 5000 +  32;
pub const SYS_pause: ::c_long = 5000 +  33;
pub const SYS_nanosleep: ::c_long = 5000 +  34;
pub const SYS_getitimer: ::c_long = 5000 +  35;
pub const SYS_setitimer: ::c_long = 5000 +  36;
pub const SYS_alarm: ::c_long = 5000 +  37;
pub const SYS_getpid: ::c_long = 5000 +  38;
pub const SYS_sendfile: ::c_long = 5000 +  39;
pub const SYS_socket: ::c_long = 5000 +  40;
pub const SYS_connect: ::c_long = 5000 +  41;
pub const SYS_accept: ::c_long = 5000 +  42;
pub const SYS_sendto: ::c_long = 5000 +  43;
pub const SYS_recvfrom: ::c_long = 5000 +  44;
pub const SYS_sendmsg: ::c_long = 5000 +  45;
pub const SYS_recvmsg: ::c_long = 5000 +  46;
pub const SYS_shutdown: ::c_long = 5000 +  47;
pub const SYS_bind: ::c_long = 5000 +  48;
pub const SYS_listen: ::c_long = 5000 +  49;
pub const SYS_getsockname: ::c_long = 5000 +  50;
pub const SYS_getpeername: ::c_long = 5000 +  51;
pub const SYS_socketpair: ::c_long = 5000 +  52;
pub const SYS_setsockopt: ::c_long = 5000 +  53;
pub const SYS_getsockopt: ::c_long = 5000 +  54;
pub const SYS_clone: ::c_long = 5000 +  55;
pub const SYS_fork: ::c_long = 5000 +  56;
pub const SYS_execve: ::c_long = 5000 +  57;
pub const SYS_exit: ::c_long = 5000 +  58;
pub const SYS_wait4: ::c_long = 5000 +  59;
pub const SYS_kill: ::c_long = 5000 +  60;
pub const SYS_uname: ::c_long = 5000 +  61;
pub const SYS_semget: ::c_long = 5000 +  62;
pub const SYS_semop: ::c_long = 5000 +  63;
pub const SYS_semctl: ::c_long = 5000 +  64;
pub const SYS_shmdt: ::c_long = 5000 +  65;
pub const SYS_msgget: ::c_long = 5000 +  66;
pub const SYS_msgsnd: ::c_long = 5000 +  67;
pub const SYS_msgrcv: ::c_long = 5000 +  68;
pub const SYS_msgctl: ::c_long = 5000 +  69;
pub const SYS_fcntl: ::c_long = 5000 +  70;
pub const SYS_flock: ::c_long = 5000 +  71;
pub const SYS_fsync: ::c_long = 5000 +  72;
pub const SYS_fdatasync: ::c_long = 5000 +  73;
pub const SYS_truncate: ::c_long = 5000 +  74;
pub const SYS_ftruncate: ::c_long = 5000 +  75;
pub const SYS_getdents: ::c_long = 5000 +  76;
pub const SYS_getcwd: ::c_long = 5000 +  77;
pub const SYS_chdir: ::c_long = 5000 +  78;
pub const SYS_fchdir: ::c_long = 5000 +  79;
pub const SYS_rename: ::c_long = 5000 +  80;
pub const SYS_mkdir: ::c_long = 5000 +  81;
pub const SYS_rmdir: ::c_long = 5000 +  82;
pub const SYS_creat: ::c_long = 5000 +  83;
pub const SYS_link: ::c_long = 5000 +  84;
pub const SYS_unlink: ::c_long = 5000 +  85;
pub const SYS_symlink: ::c_long = 5000 +  86;
pub const SYS_readlink: ::c_long = 5000 +  87;
pub const SYS_chmod: ::c_long = 5000 +  88;
pub const SYS_fchmod: ::c_long = 5000 +  89;
pub const SYS_chown: ::c_long = 5000 +  90;
pub const SYS_fchown: ::c_long = 5000 +  91;
pub const SYS_lchown: ::c_long = 5000 +  92;
pub const SYS_umask: ::c_long = 5000 +  93;
pub const SYS_gettimeofday: ::c_long = 5000 +  94;
pub const SYS_getrlimit: ::c_long = 5000 +  95;
pub const SYS_getrusage: ::c_long = 5000 +  96;
pub const SYS_sysinfo: ::c_long = 5000 +  97;
pub const SYS_times: ::c_long = 5000 +  98;
pub const SYS_ptrace: ::c_long = 5000 +  99;
pub const SYS_getuid: ::c_long = 5000 + 100;
pub const SYS_syslog: ::c_long = 5000 + 101;
pub const SYS_getgid: ::c_long = 5000 + 102;
pub const SYS_setuid: ::c_long = 5000 + 103;
pub const SYS_setgid: ::c_long = 5000 + 104;
pub const SYS_geteuid: ::c_long = 5000 + 105;
pub const SYS_getegid: ::c_long = 5000 + 106;
pub const SYS_setpgid: ::c_long = 5000 + 107;
pub const SYS_getppid: ::c_long = 5000 + 108;
pub const SYS_getpgrp: ::c_long = 5000 + 109;
pub const SYS_setsid: ::c_long = 5000 + 110;
pub const SYS_setreuid: ::c_long = 5000 + 111;
pub const SYS_setregid: ::c_long = 5000 + 112;
pub const SYS_getgroups: ::c_long = 5000 + 113;
pub const SYS_setgroups: ::c_long = 5000 + 114;
pub const SYS_setresuid: ::c_long = 5000 + 115;
pub const SYS_getresuid: ::c_long = 5000 + 116;
pub const SYS_setresgid: ::c_long = 5000 + 117;
pub const SYS_getresgid: ::c_long = 5000 + 118;
pub const SYS_getpgid: ::c_long = 5000 + 119;
pub const SYS_setfsuid: ::c_long = 5000 + 120;
pub const SYS_setfsgid: ::c_long = 5000 + 121;
pub const SYS_getsid: ::c_long = 5000 + 122;
pub const SYS_capget: ::c_long = 5000 + 123;
pub const SYS_capset: ::c_long = 5000 + 124;
pub const SYS_rt_sigpending: ::c_long = 5000 + 125;
pub const SYS_rt_sigtimedwait: ::c_long = 5000 + 126;
pub const SYS_rt_sigqueueinfo: ::c_long = 5000 + 127;
pub const SYS_rt_sigsuspend: ::c_long = 5000 + 128;
pub const SYS_sigaltstack: ::c_long = 5000 + 129;
pub const SYS_utime: ::c_long = 5000 + 130;
pub const SYS_mknod: ::c_long = 5000 + 131;
pub const SYS_personality: ::c_long = 5000 + 132;
pub const SYS_ustat: ::c_long = 5000 + 133;
pub const SYS_statfs: ::c_long = 5000 + 134;
pub const SYS_fstatfs: ::c_long = 5000 + 135;
pub const SYS_sysfs: ::c_long = 5000 + 136;
pub const SYS_getpriority: ::c_long = 5000 + 137;
pub const SYS_setpriority: ::c_long = 5000 + 138;
pub const SYS_sched_setparam: ::c_long = 5000 + 139;
pub const SYS_sched_getparam: ::c_long = 5000 + 140;
pub const SYS_sched_setscheduler: ::c_long = 5000 + 141;
pub const SYS_sched_getscheduler: ::c_long = 5000 + 142;
pub const SYS_sched_get_priority_max: ::c_long = 5000 + 143;
pub const SYS_sched_get_priority_min: ::c_long = 5000 + 144;
pub const SYS_sched_rr_get_interval: ::c_long = 5000 + 145;
pub const SYS_mlock: ::c_long = 5000 + 146;
pub const SYS_munlock: ::c_long = 5000 + 147;
pub const SYS_mlockall: ::c_long = 5000 + 148;
pub const SYS_munlockall: ::c_long = 5000 + 149;
pub const SYS_vhangup: ::c_long = 5000 + 150;
pub const SYS_pivot_root: ::c_long = 5000 + 151;
pub const SYS__sysctl: ::c_long = 5000 + 152;
pub const SYS_prctl: ::c_long = 5000 + 153;
pub const SYS_adjtimex: ::c_long = 5000 + 154;
pub const SYS_setrlimit: ::c_long = 5000 + 155;
pub const SYS_chroot: ::c_long = 5000 + 156;
pub const SYS_sync: ::c_long = 5000 + 157;
pub const SYS_acct: ::c_long = 5000 + 158;
pub const SYS_settimeofday: ::c_long = 5000 + 159;
pub const SYS_mount: ::c_long = 5000 + 160;
pub const SYS_umount2: ::c_long = 5000 + 161;
pub const SYS_swapon: ::c_long = 5000 + 162;
pub const SYS_swapoff: ::c_long = 5000 + 163;
pub const SYS_reboot: ::c_long = 5000 + 164;
pub const SYS_sethostname: ::c_long = 5000 + 165;
pub const SYS_setdomainname: ::c_long = 5000 + 166;
pub const SYS_create_module: ::c_long = 5000 + 167;
pub const SYS_init_module: ::c_long = 5000 + 168;
pub const SYS_delete_module: ::c_long = 5000 + 169;
pub const SYS_get_kernel_syms: ::c_long = 5000 + 170;
pub const SYS_query_module: ::c_long = 5000 + 171;
pub const SYS_quotactl: ::c_long = 5000 + 172;
pub const SYS_nfsservctl: ::c_long = 5000 + 173;
pub const SYS_getpmsg: ::c_long = 5000 + 174;
pub const SYS_putpmsg: ::c_long = 5000 + 175;
pub const SYS_afs_syscall: ::c_long = 5000 + 176;
pub const SYS_reserved177: ::c_long = 5000 + 177;
pub const SYS_gettid: ::c_long = 5000 + 178;
pub const SYS_readahead: ::c_long = 5000 + 179;
pub const SYS_setxattr: ::c_long = 5000 + 180;
pub const SYS_lsetxattr: ::c_long = 5000 + 181;
pub const SYS_fsetxattr: ::c_long = 5000 + 182;
pub const SYS_getxattr: ::c_long = 5000 + 183;
pub const SYS_lgetxattr: ::c_long = 5000 + 184;
pub const SYS_fgetxattr: ::c_long = 5000 + 185;
pub const SYS_listxattr: ::c_long = 5000 + 186;
pub const SYS_llistxattr: ::c_long = 5000 + 187;
pub const SYS_flistxattr: ::c_long = 5000 + 188;
pub const SYS_removexattr: ::c_long = 5000 + 189;
pub const SYS_lremovexattr: ::c_long = 5000 + 190;
pub const SYS_fremovexattr: ::c_long = 5000 + 191;
pub const SYS_tkill: ::c_long = 5000 + 192;
pub const SYS_reserved193: ::c_long = 5000 + 193;
pub const SYS_futex: ::c_long = 5000 + 194;
pub const SYS_sched_setaffinity: ::c_long = 5000 + 195;
pub const SYS_sched_getaffinity: ::c_long = 5000 + 196;
pub const SYS_cacheflush: ::c_long = 5000 + 197;
pub const SYS_cachectl: ::c_long = 5000 + 198;
pub const SYS_sysmips: ::c_long = 5000 + 199;
pub const SYS_io_setup: ::c_long = 5000 + 200;
pub const SYS_io_destroy: ::c_long = 5000 + 201;
pub const SYS_io_getevents: ::c_long = 5000 + 202;
pub const SYS_io_submit: ::c_long = 5000 + 203;
pub const SYS_io_cancel: ::c_long = 5000 + 204;
pub const SYS_exit_group: ::c_long = 5000 + 205;
pub const SYS_lookup_dcookie: ::c_long = 5000 + 206;
pub const SYS_epoll_create: ::c_long = 5000 + 207;
pub const SYS_epoll_ctl: ::c_long = 5000 + 208;
pub const SYS_epoll_wait: ::c_long = 5000 + 209;
pub const SYS_remap_file_pages: ::c_long = 5000 + 210;
pub const SYS_rt_sigreturn: ::c_long = 5000 + 211;
pub const SYS_set_tid_address: ::c_long = 5000 + 212;
pub const SYS_restart_syscall: ::c_long = 5000 + 213;
pub const SYS_semtimedop: ::c_long = 5000 + 214;
pub const SYS_fadvise64: ::c_long = 5000 + 215;
pub const SYS_timer_create: ::c_long = 5000 + 216;
pub const SYS_timer_settime: ::c_long = 5000 + 217;
pub const SYS_timer_gettime: ::c_long = 5000 + 218;
pub const SYS_timer_getoverrun: ::c_long = 5000 + 219;
pub const SYS_timer_delete: ::c_long = 5000 + 220;
pub const SYS_clock_settime: ::c_long = 5000 + 221;
pub const SYS_clock_gettime: ::c_long = 5000 + 222;
pub const SYS_clock_getres: ::c_long = 5000 + 223;
pub const SYS_clock_nanosleep: ::c_long = 5000 + 224;
pub const SYS_tgkill: ::c_long = 5000 + 225;
pub const SYS_utimes: ::c_long = 5000 + 226;
pub const SYS_mbind: ::c_long = 5000 + 227;
pub const SYS_get_mempolicy: ::c_long = 5000 + 228;
pub const SYS_set_mempolicy: ::c_long = 5000 + 229;
pub const SYS_mq_open: ::c_long = 5000 + 230;
pub const SYS_mq_unlink: ::c_long = 5000 + 231;
pub const SYS_mq_timedsend: ::c_long = 5000 + 232;
pub const SYS_mq_timedreceive: ::c_long = 5000 + 233;
pub const SYS_mq_notify: ::c_long = 5000 + 234;
pub const SYS_mq_getsetattr: ::c_long = 5000 + 235;
pub const SYS_vserver: ::c_long = 5000 + 236;
pub const SYS_waitid: ::c_long = 5000 + 237;
/* pub const SYS_sys_setaltroot: ::c_long = 5000 + 238; */
pub const SYS_add_key: ::c_long = 5000 + 239;
pub const SYS_request_key: ::c_long = 5000 + 240;
pub const SYS_keyctl: ::c_long = 5000 + 241;
pub const SYS_set_thread_area: ::c_long = 5000 + 242;
pub const SYS_inotify_init: ::c_long = 5000 + 243;
pub const SYS_inotify_add_watch: ::c_long = 5000 + 244;
pub const SYS_inotify_rm_watch: ::c_long = 5000 + 245;
pub const SYS_migrate_pages: ::c_long = 5000 + 246;
pub const SYS_openat: ::c_long = 5000 + 247;
pub const SYS_mkdirat: ::c_long = 5000 + 248;
pub const SYS_mknodat: ::c_long = 5000 + 249;
pub const SYS_fchownat: ::c_long = 5000 + 250;
pub const SYS_futimesat: ::c_long = 5000 + 251;
pub const SYS_newfstatat: ::c_long = 5000 + 252;
pub const SYS_unlinkat: ::c_long = 5000 + 253;
pub const SYS_renameat: ::c_long = 5000 + 254;
pub const SYS_linkat: ::c_long = 5000 + 255;
pub const SYS_symlinkat: ::c_long = 5000 + 256;
pub const SYS_readlinkat: ::c_long = 5000 + 257;
pub const SYS_fchmodat: ::c_long = 5000 + 258;
pub const SYS_faccessat: ::c_long = 5000 + 259;
pub const SYS_pselect6: ::c_long = 5000 + 260;
pub const SYS_ppoll: ::c_long = 5000 + 261;
pub const SYS_unshare: ::c_long = 5000 + 262;
pub const SYS_splice: ::c_long = 5000 + 263;
pub const SYS_sync_file_range: ::c_long = 5000 + 264;
pub const SYS_tee: ::c_long = 5000 + 265;
pub const SYS_vmsplice: ::c_long = 5000 + 266;
pub const SYS_move_pages: ::c_long = 5000 + 267;
pub const SYS_set_robust_list: ::c_long = 5000 + 268;
pub const SYS_get_robust_list: ::c_long = 5000 + 269;
pub const SYS_kexec_load: ::c_long = 5000 + 270;
pub const SYS_getcpu: ::c_long = 5000 + 271;
pub const SYS_epoll_pwait: ::c_long = 5000 + 272;
pub const SYS_ioprio_set: ::c_long = 5000 + 273;
pub const SYS_ioprio_get: ::c_long = 5000 + 274;
pub const SYS_utimensat: ::c_long = 5000 + 275;
pub const SYS_signalfd: ::c_long = 5000 + 276;
pub const SYS_timerfd: ::c_long = 5000 + 277;
pub const SYS_eventfd: ::c_long = 5000 + 278;
pub const SYS_fallocate: ::c_long = 5000 + 279;
pub const SYS_timerfd_create: ::c_long = 5000 + 280;
pub const SYS_timerfd_gettime: ::c_long = 5000 + 281;
pub const SYS_timerfd_settime: ::c_long = 5000 + 282;
pub const SYS_signalfd4: ::c_long = 5000 + 283;
pub const SYS_eventfd2: ::c_long = 5000 + 284;
pub const SYS_epoll_create1: ::c_long = 5000 + 285;
pub const SYS_dup3: ::c_long = 5000 + 286;
pub const SYS_pipe2: ::c_long = 5000 + 287;
pub const SYS_inotify_init1: ::c_long = 5000 + 288;
pub const SYS_preadv: ::c_long = 5000 + 289;
pub const SYS_pwritev: ::c_long = 5000 + 290;
pub const SYS_rt_tgsigqueueinfo: ::c_long = 5000 + 291;
pub const SYS_perf_event_open: ::c_long = 5000 + 292;
pub const SYS_accept4: ::c_long = 5000 + 293;
pub const SYS_recvmmsg: ::c_long = 5000 + 294;
pub const SYS_fanotify_init: ::c_long = 5000 + 295;
pub const SYS_fanotify_mark: ::c_long = 5000 + 296;
pub const SYS_prlimit64: ::c_long = 5000 + 297;
pub const SYS_name_to_handle_at: ::c_long = 5000 + 298;
pub const SYS_open_by_handle_at: ::c_long = 5000 + 299;
pub const SYS_clock_adjtime: ::c_long = 5000 + 300;
pub const SYS_syncfs: ::c_long = 5000 + 301;
pub const SYS_sendmmsg: ::c_long = 5000 + 302;
pub const SYS_setns: ::c_long = 5000 + 303;
pub const SYS_process_vm_readv: ::c_long = 5000 + 304;
pub const SYS_process_vm_writev: ::c_long = 5000 + 305;
pub const SYS_kcmp: ::c_long = 5000 + 306;
pub const SYS_finit_module: ::c_long = 5000 + 307;
pub const SYS_getdents64: ::c_long = 5000 + 308;
pub const SYS_sched_setattr: ::c_long = 5000 + 309;
pub const SYS_sched_getattr: ::c_long = 5000 + 310;
pub const SYS_renameat2: ::c_long = 5000 + 311;
pub const SYS_seccomp: ::c_long = 5000 + 312;
pub const SYS_getrandom: ::c_long = 5000 + 313;
pub const SYS_memfd_create: ::c_long = 5000 + 314;
pub const SYS_bpf: ::c_long = 5000 + 315;
pub const SYS_execveat: ::c_long = 5000 + 316;
pub const SYS_userfaultfd: ::c_long = 5000 + 317;
pub const SYS_membarrier: ::c_long = 5000 + 318;
pub const SYS_mlock2: ::c_long = 5000 + 319;
pub const SYS_copy_file_range: ::c_long = 5000 + 320;
pub const SYS_preadv2: ::c_long = 5000 + 321;
pub const SYS_pwritev2: ::c_long = 5000 + 322;
pub const SYS_pkey_mprotect: ::c_long = 5000 + 323;
pub const SYS_pkey_alloc: ::c_long = 5000 + 324;
pub const SYS_pkey_free: ::c_long = 5000 + 325;
