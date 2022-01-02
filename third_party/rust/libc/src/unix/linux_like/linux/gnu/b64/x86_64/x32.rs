use pthread_mutex_t;

pub type c_long = i32;
pub type c_ulong = u32;

s! {
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
}

pub const __SIZEOF_PTHREAD_MUTEX_T: usize = 32;
pub const __SIZEOF_PTHREAD_RWLOCK_T: usize = 44;

align_const! {
    pub const PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP: ::pthread_mutex_t =
        pthread_mutex_t {
            size: [
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            ],
        };
    pub const PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP: ::pthread_mutex_t =
        pthread_mutex_t {
            size: [
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            ],
        };
    pub const PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP: ::pthread_mutex_t =
        pthread_mutex_t {
            size: [
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            ],
        };
}

// Syscall table

pub const __X32_SYSCALL_BIT: ::c_long = 0x40000000;

pub const SYS_read: ::c_long = __X32_SYSCALL_BIT + 0;
pub const SYS_write: ::c_long = __X32_SYSCALL_BIT + 1;
pub const SYS_open: ::c_long = __X32_SYSCALL_BIT + 2;
pub const SYS_close: ::c_long = __X32_SYSCALL_BIT + 3;
pub const SYS_stat: ::c_long = __X32_SYSCALL_BIT + 4;
pub const SYS_fstat: ::c_long = __X32_SYSCALL_BIT + 5;
pub const SYS_lstat: ::c_long = __X32_SYSCALL_BIT + 6;
pub const SYS_poll: ::c_long = __X32_SYSCALL_BIT + 7;
pub const SYS_lseek: ::c_long = __X32_SYSCALL_BIT + 8;
pub const SYS_mmap: ::c_long = __X32_SYSCALL_BIT + 9;
pub const SYS_mprotect: ::c_long = __X32_SYSCALL_BIT + 10;
pub const SYS_munmap: ::c_long = __X32_SYSCALL_BIT + 11;
pub const SYS_brk: ::c_long = __X32_SYSCALL_BIT + 12;
pub const SYS_rt_sigprocmask: ::c_long = __X32_SYSCALL_BIT + 14;
pub const SYS_pread64: ::c_long = __X32_SYSCALL_BIT + 17;
pub const SYS_pwrite64: ::c_long = __X32_SYSCALL_BIT + 18;
pub const SYS_access: ::c_long = __X32_SYSCALL_BIT + 21;
pub const SYS_pipe: ::c_long = __X32_SYSCALL_BIT + 22;
pub const SYS_select: ::c_long = __X32_SYSCALL_BIT + 23;
pub const SYS_sched_yield: ::c_long = __X32_SYSCALL_BIT + 24;
pub const SYS_mremap: ::c_long = __X32_SYSCALL_BIT + 25;
pub const SYS_msync: ::c_long = __X32_SYSCALL_BIT + 26;
pub const SYS_mincore: ::c_long = __X32_SYSCALL_BIT + 27;
pub const SYS_madvise: ::c_long = __X32_SYSCALL_BIT + 28;
pub const SYS_shmget: ::c_long = __X32_SYSCALL_BIT + 29;
pub const SYS_shmat: ::c_long = __X32_SYSCALL_BIT + 30;
pub const SYS_shmctl: ::c_long = __X32_SYSCALL_BIT + 31;
pub const SYS_dup: ::c_long = __X32_SYSCALL_BIT + 32;
pub const SYS_dup2: ::c_long = __X32_SYSCALL_BIT + 33;
pub const SYS_pause: ::c_long = __X32_SYSCALL_BIT + 34;
pub const SYS_nanosleep: ::c_long = __X32_SYSCALL_BIT + 35;
pub const SYS_getitimer: ::c_long = __X32_SYSCALL_BIT + 36;
pub const SYS_alarm: ::c_long = __X32_SYSCALL_BIT + 37;
pub const SYS_setitimer: ::c_long = __X32_SYSCALL_BIT + 38;
pub const SYS_getpid: ::c_long = __X32_SYSCALL_BIT + 39;
pub const SYS_sendfile: ::c_long = __X32_SYSCALL_BIT + 40;
pub const SYS_socket: ::c_long = __X32_SYSCALL_BIT + 41;
pub const SYS_connect: ::c_long = __X32_SYSCALL_BIT + 42;
pub const SYS_accept: ::c_long = __X32_SYSCALL_BIT + 43;
pub const SYS_sendto: ::c_long = __X32_SYSCALL_BIT + 44;
pub const SYS_shutdown: ::c_long = __X32_SYSCALL_BIT + 48;
pub const SYS_bind: ::c_long = __X32_SYSCALL_BIT + 49;
pub const SYS_listen: ::c_long = __X32_SYSCALL_BIT + 50;
pub const SYS_getsockname: ::c_long = __X32_SYSCALL_BIT + 51;
pub const SYS_getpeername: ::c_long = __X32_SYSCALL_BIT + 52;
pub const SYS_socketpair: ::c_long = __X32_SYSCALL_BIT + 53;
pub const SYS_clone: ::c_long = __X32_SYSCALL_BIT + 56;
pub const SYS_fork: ::c_long = __X32_SYSCALL_BIT + 57;
pub const SYS_vfork: ::c_long = __X32_SYSCALL_BIT + 58;
pub const SYS_exit: ::c_long = __X32_SYSCALL_BIT + 60;
pub const SYS_wait4: ::c_long = __X32_SYSCALL_BIT + 61;
pub const SYS_kill: ::c_long = __X32_SYSCALL_BIT + 62;
pub const SYS_uname: ::c_long = __X32_SYSCALL_BIT + 63;
pub const SYS_semget: ::c_long = __X32_SYSCALL_BIT + 64;
pub const SYS_semop: ::c_long = __X32_SYSCALL_BIT + 65;
pub const SYS_semctl: ::c_long = __X32_SYSCALL_BIT + 66;
pub const SYS_shmdt: ::c_long = __X32_SYSCALL_BIT + 67;
pub const SYS_msgget: ::c_long = __X32_SYSCALL_BIT + 68;
pub const SYS_msgsnd: ::c_long = __X32_SYSCALL_BIT + 69;
pub const SYS_msgrcv: ::c_long = __X32_SYSCALL_BIT + 70;
pub const SYS_msgctl: ::c_long = __X32_SYSCALL_BIT + 71;
pub const SYS_fcntl: ::c_long = __X32_SYSCALL_BIT + 72;
pub const SYS_flock: ::c_long = __X32_SYSCALL_BIT + 73;
pub const SYS_fsync: ::c_long = __X32_SYSCALL_BIT + 74;
pub const SYS_fdatasync: ::c_long = __X32_SYSCALL_BIT + 75;
pub const SYS_truncate: ::c_long = __X32_SYSCALL_BIT + 76;
pub const SYS_ftruncate: ::c_long = __X32_SYSCALL_BIT + 77;
pub const SYS_getdents: ::c_long = __X32_SYSCALL_BIT + 78;
pub const SYS_getcwd: ::c_long = __X32_SYSCALL_BIT + 79;
pub const SYS_chdir: ::c_long = __X32_SYSCALL_BIT + 80;
pub const SYS_fchdir: ::c_long = __X32_SYSCALL_BIT + 81;
pub const SYS_rename: ::c_long = __X32_SYSCALL_BIT + 82;
pub const SYS_mkdir: ::c_long = __X32_SYSCALL_BIT + 83;
pub const SYS_rmdir: ::c_long = __X32_SYSCALL_BIT + 84;
pub const SYS_creat: ::c_long = __X32_SYSCALL_BIT + 85;
pub const SYS_link: ::c_long = __X32_SYSCALL_BIT + 86;
pub const SYS_unlink: ::c_long = __X32_SYSCALL_BIT + 87;
pub const SYS_symlink: ::c_long = __X32_SYSCALL_BIT + 88;
pub const SYS_readlink: ::c_long = __X32_SYSCALL_BIT + 89;
pub const SYS_chmod: ::c_long = __X32_SYSCALL_BIT + 90;
pub const SYS_fchmod: ::c_long = __X32_SYSCALL_BIT + 91;
pub const SYS_chown: ::c_long = __X32_SYSCALL_BIT + 92;
pub const SYS_fchown: ::c_long = __X32_SYSCALL_BIT + 93;
pub const SYS_lchown: ::c_long = __X32_SYSCALL_BIT + 94;
pub const SYS_umask: ::c_long = __X32_SYSCALL_BIT + 95;
pub const SYS_gettimeofday: ::c_long = __X32_SYSCALL_BIT + 96;
pub const SYS_getrlimit: ::c_long = __X32_SYSCALL_BIT + 97;
pub const SYS_getrusage: ::c_long = __X32_SYSCALL_BIT + 98;
pub const SYS_sysinfo: ::c_long = __X32_SYSCALL_BIT + 99;
pub const SYS_times: ::c_long = __X32_SYSCALL_BIT + 100;
pub const SYS_getuid: ::c_long = __X32_SYSCALL_BIT + 102;
pub const SYS_syslog: ::c_long = __X32_SYSCALL_BIT + 103;
pub const SYS_getgid: ::c_long = __X32_SYSCALL_BIT + 104;
pub const SYS_setuid: ::c_long = __X32_SYSCALL_BIT + 105;
pub const SYS_setgid: ::c_long = __X32_SYSCALL_BIT + 106;
pub const SYS_geteuid: ::c_long = __X32_SYSCALL_BIT + 107;
pub const SYS_getegid: ::c_long = __X32_SYSCALL_BIT + 108;
pub const SYS_setpgid: ::c_long = __X32_SYSCALL_BIT + 109;
pub const SYS_getppid: ::c_long = __X32_SYSCALL_BIT + 110;
pub const SYS_getpgrp: ::c_long = __X32_SYSCALL_BIT + 111;
pub const SYS_setsid: ::c_long = __X32_SYSCALL_BIT + 112;
pub const SYS_setreuid: ::c_long = __X32_SYSCALL_BIT + 113;
pub const SYS_setregid: ::c_long = __X32_SYSCALL_BIT + 114;
pub const SYS_getgroups: ::c_long = __X32_SYSCALL_BIT + 115;
pub const SYS_setgroups: ::c_long = __X32_SYSCALL_BIT + 116;
pub const SYS_setresuid: ::c_long = __X32_SYSCALL_BIT + 117;
pub const SYS_getresuid: ::c_long = __X32_SYSCALL_BIT + 118;
pub const SYS_setresgid: ::c_long = __X32_SYSCALL_BIT + 119;
pub const SYS_getresgid: ::c_long = __X32_SYSCALL_BIT + 120;
pub const SYS_getpgid: ::c_long = __X32_SYSCALL_BIT + 121;
pub const SYS_setfsuid: ::c_long = __X32_SYSCALL_BIT + 122;
pub const SYS_setfsgid: ::c_long = __X32_SYSCALL_BIT + 123;
pub const SYS_getsid: ::c_long = __X32_SYSCALL_BIT + 124;
pub const SYS_capget: ::c_long = __X32_SYSCALL_BIT + 125;
pub const SYS_capset: ::c_long = __X32_SYSCALL_BIT + 126;
pub const SYS_rt_sigsuspend: ::c_long = __X32_SYSCALL_BIT + 130;
pub const SYS_utime: ::c_long = __X32_SYSCALL_BIT + 132;
pub const SYS_mknod: ::c_long = __X32_SYSCALL_BIT + 133;
pub const SYS_personality: ::c_long = __X32_SYSCALL_BIT + 135;
pub const SYS_ustat: ::c_long = __X32_SYSCALL_BIT + 136;
pub const SYS_statfs: ::c_long = __X32_SYSCALL_BIT + 137;
pub const SYS_fstatfs: ::c_long = __X32_SYSCALL_BIT + 138;
pub const SYS_sysfs: ::c_long = __X32_SYSCALL_BIT + 139;
pub const SYS_getpriority: ::c_long = __X32_SYSCALL_BIT + 140;
pub const SYS_setpriority: ::c_long = __X32_SYSCALL_BIT + 141;
pub const SYS_sched_setparam: ::c_long = __X32_SYSCALL_BIT + 142;
pub const SYS_sched_getparam: ::c_long = __X32_SYSCALL_BIT + 143;
pub const SYS_sched_setscheduler: ::c_long = __X32_SYSCALL_BIT + 144;
pub const SYS_sched_getscheduler: ::c_long = __X32_SYSCALL_BIT + 145;
pub const SYS_sched_get_priority_max: ::c_long = __X32_SYSCALL_BIT + 146;
pub const SYS_sched_get_priority_min: ::c_long = __X32_SYSCALL_BIT + 147;
pub const SYS_sched_rr_get_interval: ::c_long = __X32_SYSCALL_BIT + 148;
pub const SYS_mlock: ::c_long = __X32_SYSCALL_BIT + 149;
pub const SYS_munlock: ::c_long = __X32_SYSCALL_BIT + 150;
pub const SYS_mlockall: ::c_long = __X32_SYSCALL_BIT + 151;
pub const SYS_munlockall: ::c_long = __X32_SYSCALL_BIT + 152;
pub const SYS_vhangup: ::c_long = __X32_SYSCALL_BIT + 153;
pub const SYS_modify_ldt: ::c_long = __X32_SYSCALL_BIT + 154;
pub const SYS_pivot_root: ::c_long = __X32_SYSCALL_BIT + 155;
pub const SYS_prctl: ::c_long = __X32_SYSCALL_BIT + 157;
pub const SYS_arch_prctl: ::c_long = __X32_SYSCALL_BIT + 158;
pub const SYS_adjtimex: ::c_long = __X32_SYSCALL_BIT + 159;
pub const SYS_setrlimit: ::c_long = __X32_SYSCALL_BIT + 160;
pub const SYS_chroot: ::c_long = __X32_SYSCALL_BIT + 161;
pub const SYS_sync: ::c_long = __X32_SYSCALL_BIT + 162;
pub const SYS_acct: ::c_long = __X32_SYSCALL_BIT + 163;
pub const SYS_settimeofday: ::c_long = __X32_SYSCALL_BIT + 164;
pub const SYS_mount: ::c_long = __X32_SYSCALL_BIT + 165;
pub const SYS_umount2: ::c_long = __X32_SYSCALL_BIT + 166;
pub const SYS_swapon: ::c_long = __X32_SYSCALL_BIT + 167;
pub const SYS_swapoff: ::c_long = __X32_SYSCALL_BIT + 168;
pub const SYS_reboot: ::c_long = __X32_SYSCALL_BIT + 169;
pub const SYS_sethostname: ::c_long = __X32_SYSCALL_BIT + 170;
pub const SYS_setdomainname: ::c_long = __X32_SYSCALL_BIT + 171;
pub const SYS_iopl: ::c_long = __X32_SYSCALL_BIT + 172;
pub const SYS_ioperm: ::c_long = __X32_SYSCALL_BIT + 173;
pub const SYS_init_module: ::c_long = __X32_SYSCALL_BIT + 175;
pub const SYS_delete_module: ::c_long = __X32_SYSCALL_BIT + 176;
pub const SYS_quotactl: ::c_long = __X32_SYSCALL_BIT + 179;
pub const SYS_getpmsg: ::c_long = __X32_SYSCALL_BIT + 181;
pub const SYS_putpmsg: ::c_long = __X32_SYSCALL_BIT + 182;
pub const SYS_afs_syscall: ::c_long = __X32_SYSCALL_BIT + 183;
pub const SYS_tuxcall: ::c_long = __X32_SYSCALL_BIT + 184;
pub const SYS_security: ::c_long = __X32_SYSCALL_BIT + 185;
pub const SYS_gettid: ::c_long = __X32_SYSCALL_BIT + 186;
pub const SYS_readahead: ::c_long = __X32_SYSCALL_BIT + 187;
pub const SYS_setxattr: ::c_long = __X32_SYSCALL_BIT + 188;
pub const SYS_lsetxattr: ::c_long = __X32_SYSCALL_BIT + 189;
pub const SYS_fsetxattr: ::c_long = __X32_SYSCALL_BIT + 190;
pub const SYS_getxattr: ::c_long = __X32_SYSCALL_BIT + 191;
pub const SYS_lgetxattr: ::c_long = __X32_SYSCALL_BIT + 192;
pub const SYS_fgetxattr: ::c_long = __X32_SYSCALL_BIT + 193;
pub const SYS_listxattr: ::c_long = __X32_SYSCALL_BIT + 194;
pub const SYS_llistxattr: ::c_long = __X32_SYSCALL_BIT + 195;
pub const SYS_flistxattr: ::c_long = __X32_SYSCALL_BIT + 196;
pub const SYS_removexattr: ::c_long = __X32_SYSCALL_BIT + 197;
pub const SYS_lremovexattr: ::c_long = __X32_SYSCALL_BIT + 198;
pub const SYS_fremovexattr: ::c_long = __X32_SYSCALL_BIT + 199;
pub const SYS_tkill: ::c_long = __X32_SYSCALL_BIT + 200;
pub const SYS_time: ::c_long = __X32_SYSCALL_BIT + 201;
pub const SYS_futex: ::c_long = __X32_SYSCALL_BIT + 202;
pub const SYS_sched_setaffinity: ::c_long = __X32_SYSCALL_BIT + 203;
pub const SYS_sched_getaffinity: ::c_long = __X32_SYSCALL_BIT + 204;
pub const SYS_io_destroy: ::c_long = __X32_SYSCALL_BIT + 207;
pub const SYS_io_getevents: ::c_long = __X32_SYSCALL_BIT + 208;
pub const SYS_io_cancel: ::c_long = __X32_SYSCALL_BIT + 210;
pub const SYS_lookup_dcookie: ::c_long = __X32_SYSCALL_BIT + 212;
pub const SYS_epoll_create: ::c_long = __X32_SYSCALL_BIT + 213;
pub const SYS_remap_file_pages: ::c_long = __X32_SYSCALL_BIT + 216;
pub const SYS_getdents64: ::c_long = __X32_SYSCALL_BIT + 217;
pub const SYS_set_tid_address: ::c_long = __X32_SYSCALL_BIT + 218;
pub const SYS_restart_syscall: ::c_long = __X32_SYSCALL_BIT + 219;
pub const SYS_semtimedop: ::c_long = __X32_SYSCALL_BIT + 220;
pub const SYS_fadvise64: ::c_long = __X32_SYSCALL_BIT + 221;
pub const SYS_timer_settime: ::c_long = __X32_SYSCALL_BIT + 223;
pub const SYS_timer_gettime: ::c_long = __X32_SYSCALL_BIT + 224;
pub const SYS_timer_getoverrun: ::c_long = __X32_SYSCALL_BIT + 225;
pub const SYS_timer_delete: ::c_long = __X32_SYSCALL_BIT + 226;
pub const SYS_clock_settime: ::c_long = __X32_SYSCALL_BIT + 227;
pub const SYS_clock_gettime: ::c_long = __X32_SYSCALL_BIT + 228;
pub const SYS_clock_getres: ::c_long = __X32_SYSCALL_BIT + 229;
pub const SYS_clock_nanosleep: ::c_long = __X32_SYSCALL_BIT + 230;
pub const SYS_exit_group: ::c_long = __X32_SYSCALL_BIT + 231;
pub const SYS_epoll_wait: ::c_long = __X32_SYSCALL_BIT + 232;
pub const SYS_epoll_ctl: ::c_long = __X32_SYSCALL_BIT + 233;
pub const SYS_tgkill: ::c_long = __X32_SYSCALL_BIT + 234;
pub const SYS_utimes: ::c_long = __X32_SYSCALL_BIT + 235;
pub const SYS_mbind: ::c_long = __X32_SYSCALL_BIT + 237;
pub const SYS_set_mempolicy: ::c_long = __X32_SYSCALL_BIT + 238;
pub const SYS_get_mempolicy: ::c_long = __X32_SYSCALL_BIT + 239;
pub const SYS_mq_open: ::c_long = __X32_SYSCALL_BIT + 240;
pub const SYS_mq_unlink: ::c_long = __X32_SYSCALL_BIT + 241;
pub const SYS_mq_timedsend: ::c_long = __X32_SYSCALL_BIT + 242;
pub const SYS_mq_timedreceive: ::c_long = __X32_SYSCALL_BIT + 243;
pub const SYS_mq_getsetattr: ::c_long = __X32_SYSCALL_BIT + 245;
pub const SYS_add_key: ::c_long = __X32_SYSCALL_BIT + 248;
pub const SYS_request_key: ::c_long = __X32_SYSCALL_BIT + 249;
pub const SYS_keyctl: ::c_long = __X32_SYSCALL_BIT + 250;
pub const SYS_ioprio_set: ::c_long = __X32_SYSCALL_BIT + 251;
pub const SYS_ioprio_get: ::c_long = __X32_SYSCALL_BIT + 252;
pub const SYS_inotify_init: ::c_long = __X32_SYSCALL_BIT + 253;
pub const SYS_inotify_add_watch: ::c_long = __X32_SYSCALL_BIT + 254;
pub const SYS_inotify_rm_watch: ::c_long = __X32_SYSCALL_BIT + 255;
pub const SYS_migrate_pages: ::c_long = __X32_SYSCALL_BIT + 256;
pub const SYS_openat: ::c_long = __X32_SYSCALL_BIT + 257;
pub const SYS_mkdirat: ::c_long = __X32_SYSCALL_BIT + 258;
pub const SYS_mknodat: ::c_long = __X32_SYSCALL_BIT + 259;
pub const SYS_fchownat: ::c_long = __X32_SYSCALL_BIT + 260;
pub const SYS_futimesat: ::c_long = __X32_SYSCALL_BIT + 261;
pub const SYS_newfstatat: ::c_long = __X32_SYSCALL_BIT + 262;
pub const SYS_unlinkat: ::c_long = __X32_SYSCALL_BIT + 263;
pub const SYS_renameat: ::c_long = __X32_SYSCALL_BIT + 264;
pub const SYS_linkat: ::c_long = __X32_SYSCALL_BIT + 265;
pub const SYS_symlinkat: ::c_long = __X32_SYSCALL_BIT + 266;
pub const SYS_readlinkat: ::c_long = __X32_SYSCALL_BIT + 267;
pub const SYS_fchmodat: ::c_long = __X32_SYSCALL_BIT + 268;
pub const SYS_faccessat: ::c_long = __X32_SYSCALL_BIT + 269;
pub const SYS_pselect6: ::c_long = __X32_SYSCALL_BIT + 270;
pub const SYS_ppoll: ::c_long = __X32_SYSCALL_BIT + 271;
pub const SYS_unshare: ::c_long = __X32_SYSCALL_BIT + 272;
pub const SYS_splice: ::c_long = __X32_SYSCALL_BIT + 275;
pub const SYS_tee: ::c_long = __X32_SYSCALL_BIT + 276;
pub const SYS_sync_file_range: ::c_long = __X32_SYSCALL_BIT + 277;
pub const SYS_utimensat: ::c_long = __X32_SYSCALL_BIT + 280;
pub const SYS_epoll_pwait: ::c_long = __X32_SYSCALL_BIT + 281;
pub const SYS_signalfd: ::c_long = __X32_SYSCALL_BIT + 282;
pub const SYS_timerfd_create: ::c_long = __X32_SYSCALL_BIT + 283;
pub const SYS_eventfd: ::c_long = __X32_SYSCALL_BIT + 284;
pub const SYS_fallocate: ::c_long = __X32_SYSCALL_BIT + 285;
pub const SYS_timerfd_settime: ::c_long = __X32_SYSCALL_BIT + 286;
pub const SYS_timerfd_gettime: ::c_long = __X32_SYSCALL_BIT + 287;
pub const SYS_accept4: ::c_long = __X32_SYSCALL_BIT + 288;
pub const SYS_signalfd4: ::c_long = __X32_SYSCALL_BIT + 289;
pub const SYS_eventfd2: ::c_long = __X32_SYSCALL_BIT + 290;
pub const SYS_epoll_create1: ::c_long = __X32_SYSCALL_BIT + 291;
pub const SYS_dup3: ::c_long = __X32_SYSCALL_BIT + 292;
pub const SYS_pipe2: ::c_long = __X32_SYSCALL_BIT + 293;
pub const SYS_inotify_init1: ::c_long = __X32_SYSCALL_BIT + 294;
pub const SYS_perf_event_open: ::c_long = __X32_SYSCALL_BIT + 298;
pub const SYS_fanotify_init: ::c_long = __X32_SYSCALL_BIT + 300;
pub const SYS_fanotify_mark: ::c_long = __X32_SYSCALL_BIT + 301;
pub const SYS_prlimit64: ::c_long = __X32_SYSCALL_BIT + 302;
pub const SYS_name_to_handle_at: ::c_long = __X32_SYSCALL_BIT + 303;
pub const SYS_open_by_handle_at: ::c_long = __X32_SYSCALL_BIT + 304;
pub const SYS_clock_adjtime: ::c_long = __X32_SYSCALL_BIT + 305;
pub const SYS_syncfs: ::c_long = __X32_SYSCALL_BIT + 306;
pub const SYS_setns: ::c_long = __X32_SYSCALL_BIT + 308;
pub const SYS_getcpu: ::c_long = __X32_SYSCALL_BIT + 309;
pub const SYS_kcmp: ::c_long = __X32_SYSCALL_BIT + 312;
pub const SYS_finit_module: ::c_long = __X32_SYSCALL_BIT + 313;
pub const SYS_sched_setattr: ::c_long = __X32_SYSCALL_BIT + 314;
pub const SYS_sched_getattr: ::c_long = __X32_SYSCALL_BIT + 315;
pub const SYS_renameat2: ::c_long = __X32_SYSCALL_BIT + 316;
pub const SYS_seccomp: ::c_long = __X32_SYSCALL_BIT + 317;
pub const SYS_getrandom: ::c_long = __X32_SYSCALL_BIT + 318;
pub const SYS_memfd_create: ::c_long = __X32_SYSCALL_BIT + 319;
pub const SYS_kexec_file_load: ::c_long = __X32_SYSCALL_BIT + 320;
pub const SYS_bpf: ::c_long = __X32_SYSCALL_BIT + 321;
pub const SYS_userfaultfd: ::c_long = __X32_SYSCALL_BIT + 323;
pub const SYS_membarrier: ::c_long = __X32_SYSCALL_BIT + 324;
pub const SYS_mlock2: ::c_long = __X32_SYSCALL_BIT + 325;
pub const SYS_copy_file_range: ::c_long = __X32_SYSCALL_BIT + 326;
pub const SYS_pkey_mprotect: ::c_long = __X32_SYSCALL_BIT + 329;
pub const SYS_pkey_alloc: ::c_long = __X32_SYSCALL_BIT + 330;
pub const SYS_pkey_free: ::c_long = __X32_SYSCALL_BIT + 331;
pub const SYS_statx: ::c_long = __X32_SYSCALL_BIT + 332;
pub const SYS_pidfd_send_signal: ::c_long = __X32_SYSCALL_BIT + 424;
pub const SYS_io_uring_setup: ::c_long = __X32_SYSCALL_BIT + 425;
pub const SYS_io_uring_enter: ::c_long = __X32_SYSCALL_BIT + 426;
pub const SYS_io_uring_register: ::c_long = __X32_SYSCALL_BIT + 427;
pub const SYS_open_tree: ::c_long = __X32_SYSCALL_BIT + 428;
pub const SYS_move_mount: ::c_long = __X32_SYSCALL_BIT + 429;
pub const SYS_fsopen: ::c_long = __X32_SYSCALL_BIT + 430;
pub const SYS_fsconfig: ::c_long = __X32_SYSCALL_BIT + 431;
pub const SYS_fsmount: ::c_long = __X32_SYSCALL_BIT + 432;
pub const SYS_fspick: ::c_long = __X32_SYSCALL_BIT + 433;
pub const SYS_pidfd_open: ::c_long = __X32_SYSCALL_BIT + 434;
pub const SYS_clone3: ::c_long = __X32_SYSCALL_BIT + 435;
pub const SYS_close_range: ::c_long = __X32_SYSCALL_BIT + 436;
pub const SYS_openat2: ::c_long = __X32_SYSCALL_BIT + 437;
pub const SYS_pidfd_getfd: ::c_long = __X32_SYSCALL_BIT + 438;
pub const SYS_faccessat2: ::c_long = __X32_SYSCALL_BIT + 439;
pub const SYS_process_madvise: ::c_long = __X32_SYSCALL_BIT + 440;
pub const SYS_epoll_pwait2: ::c_long = __X32_SYSCALL_BIT + 441;
pub const SYS_mount_setattr: ::c_long = __X32_SYSCALL_BIT + 442;
pub const SYS_rt_sigaction: ::c_long = __X32_SYSCALL_BIT + 512;
pub const SYS_rt_sigreturn: ::c_long = __X32_SYSCALL_BIT + 513;
pub const SYS_ioctl: ::c_long = __X32_SYSCALL_BIT + 514;
pub const SYS_readv: ::c_long = __X32_SYSCALL_BIT + 515;
pub const SYS_writev: ::c_long = __X32_SYSCALL_BIT + 516;
pub const SYS_recvfrom: ::c_long = __X32_SYSCALL_BIT + 517;
pub const SYS_sendmsg: ::c_long = __X32_SYSCALL_BIT + 518;
pub const SYS_recvmsg: ::c_long = __X32_SYSCALL_BIT + 519;
pub const SYS_execve: ::c_long = __X32_SYSCALL_BIT + 520;
pub const SYS_ptrace: ::c_long = __X32_SYSCALL_BIT + 521;
pub const SYS_rt_sigpending: ::c_long = __X32_SYSCALL_BIT + 522;
pub const SYS_rt_sigtimedwait: ::c_long = __X32_SYSCALL_BIT + 523;
pub const SYS_rt_sigqueueinfo: ::c_long = __X32_SYSCALL_BIT + 524;
pub const SYS_sigaltstack: ::c_long = __X32_SYSCALL_BIT + 525;
pub const SYS_timer_create: ::c_long = __X32_SYSCALL_BIT + 526;
pub const SYS_mq_notify: ::c_long = __X32_SYSCALL_BIT + 527;
pub const SYS_kexec_load: ::c_long = __X32_SYSCALL_BIT + 528;
pub const SYS_waitid: ::c_long = __X32_SYSCALL_BIT + 529;
pub const SYS_set_robust_list: ::c_long = __X32_SYSCALL_BIT + 530;
pub const SYS_get_robust_list: ::c_long = __X32_SYSCALL_BIT + 531;
pub const SYS_vmsplice: ::c_long = __X32_SYSCALL_BIT + 532;
pub const SYS_move_pages: ::c_long = __X32_SYSCALL_BIT + 533;
pub const SYS_preadv: ::c_long = __X32_SYSCALL_BIT + 534;
pub const SYS_pwritev: ::c_long = __X32_SYSCALL_BIT + 535;
pub const SYS_rt_tgsigqueueinfo: ::c_long = __X32_SYSCALL_BIT + 536;
pub const SYS_recvmmsg: ::c_long = __X32_SYSCALL_BIT + 537;
pub const SYS_sendmmsg: ::c_long = __X32_SYSCALL_BIT + 538;
pub const SYS_process_vm_readv: ::c_long = __X32_SYSCALL_BIT + 539;
pub const SYS_process_vm_writev: ::c_long = __X32_SYSCALL_BIT + 540;
pub const SYS_setsockopt: ::c_long = __X32_SYSCALL_BIT + 541;
pub const SYS_getsockopt: ::c_long = __X32_SYSCALL_BIT + 542;
pub const SYS_io_setup: ::c_long = __X32_SYSCALL_BIT + 543;
pub const SYS_io_submit: ::c_long = __X32_SYSCALL_BIT + 544;
pub const SYS_execveat: ::c_long = __X32_SYSCALL_BIT + 545;
pub const SYS_preadv2: ::c_long = __X32_SYSCALL_BIT + 546;
pub const SYS_pwritev2: ::c_long = __X32_SYSCALL_BIT + 547;
