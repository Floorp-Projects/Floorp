pub type c_char = u8;
pub type wchar_t = u32;
pub type greg_t = i32;
pub type mcontext_t = sigcontext;

s! {
    pub struct sigcontext {
        pub trap_no: ::c_ulong,
        pub error_code: ::c_ulong,
        pub oldmask: ::c_ulong,
        pub arm_r0: ::c_ulong,
        pub arm_r1: ::c_ulong,
        pub arm_r2: ::c_ulong,
        pub arm_r3: ::c_ulong,
        pub arm_r4: ::c_ulong,
        pub arm_r5: ::c_ulong,
        pub arm_r6: ::c_ulong,
        pub arm_r7: ::c_ulong,
        pub arm_r8: ::c_ulong,
        pub arm_r9: ::c_ulong,
        pub arm_r10: ::c_ulong,
        pub arm_fp: ::c_ulong,
        pub arm_ip: ::c_ulong,
        pub arm_sp: ::c_ulong,
        pub arm_lr: ::c_ulong,
        pub arm_pc: ::c_ulong,
        pub arm_cpsr: ::c_ulong,
        pub fault_address: ::c_ulong,
    }
}

cfg_if! {
    if #[cfg(libc_union)] {
        s_no_extra_traits! {
            pub struct __c_anonymous_uc_sigmask_with_padding {
                pub uc_sigmask: ::sigset_t,
                /* Android has a wrong (smaller) sigset_t on x86. */
                __padding_rt_sigset: u32,
            }

            pub union __c_anonymous_uc_sigmask {
                uc_sigmask: __c_anonymous_uc_sigmask_with_padding,
                uc_sigmask64: ::sigset64_t,
            }

            pub struct ucontext_t {
                pub uc_flags: ::c_ulong,
                pub uc_link: *mut ucontext_t,
                pub uc_stack: ::stack_t,
                pub uc_mcontext: mcontext_t,
                pub uc_sigmask__c_anonymous_union: __c_anonymous_uc_sigmask,
                /* The kernel adds extra padding after uc_sigmask to match
                 * glibc sigset_t on ARM. */
                __padding: [c_char; 120],
                __align: [::c_longlong; 0],
                uc_regspace: [::c_ulong; 128],
            }
        }

        cfg_if! {
            if #[cfg(feature = "extra_traits")] {
                impl PartialEq for __c_anonymous_uc_sigmask_with_padding {
                    fn eq(
                        &self, other: &__c_anonymous_uc_sigmask_with_padding
                    ) -> bool {
                        self.uc_sigmask == other.uc_sigmask
                            // Ignore padding
                    }
                }
                impl Eq for __c_anonymous_uc_sigmask_with_padding {}
                impl ::fmt::Debug for __c_anonymous_uc_sigmask_with_padding {
                    fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                        f.debug_struct("uc_sigmask_with_padding")
                            .field("uc_sigmask_with_padding", &self.uc_sigmask)
                            // Ignore padding
                            .finish()
                    }
                }
                impl ::hash::Hash for __c_anonymous_uc_sigmask_with_padding {
                    fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                        self.uc_sigmask.hash(state)
                            // Ignore padding
                    }
                }

                impl PartialEq for __c_anonymous_uc_sigmask {
                    fn eq(&self, other: &__c_anonymous_uc_sigmask) -> bool {
                        unsafe { self.uc_sigmask == other.uc_sigmask }
                    }
                }
                impl Eq for __c_anonymous_uc_sigmask {}
                impl ::fmt::Debug for __c_anonymous_uc_sigmask {
                    fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                        f.debug_struct("uc_sigmask")
                            .field("uc_sigmask", unsafe { &self.uc_sigmask })
                            .finish()
                    }
                }
                impl ::hash::Hash for __c_anonymous_uc_sigmask {
                    fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                        unsafe { self.uc_sigmask.hash(state) }
                    }
                }

                impl PartialEq for ucontext_t {
                    fn eq(&self, other: &Self) -> bool {
                        self.uc_flags == other.uc_flags
                            && self.uc_link == other.uc_link
                            && self.uc_stack == other.uc_stack
                            && self.uc_mcontext == other.uc_mcontext
                            && self.uc_sigmask__c_anonymous_union
                                == other.uc_sigmask__c_anonymous_union
                            && &self.uc_regspace[..] == &other.uc_regspace[..]
                            // Ignore padding field
                    }
                }
                impl Eq for ucontext_t {}
                impl ::fmt::Debug for ucontext_t {
                    fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                        f.debug_struct("ucontext_t")
                            .field("uc_flags", &self.uc_flags)
                            .field("uc_link", &self.uc_link)
                            .field("uc_stack", &self.uc_stack)
                            .field("uc_mcontext", &self.uc_mcontext)
                            .field(
                                "uc_sigmask__c_anonymous_union",
                                &self.uc_sigmask__c_anonymous_union
                            )
                            .field("uc_regspace", &&self.uc_regspace[..])
                            // Ignore padding field
                            .finish()
                    }
                }
                impl ::hash::Hash for ucontext_t {
                    fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                        self.uc_flags.hash(state);
                        self.uc_link.hash(state);
                        self.uc_stack.hash(state);
                        self.uc_mcontext.hash(state);
                        self.uc_sigmask__c_anonymous_union.hash(state);
                        self.uc_regspace[..].hash(state);
                        // Ignore padding field
                    }
                }
            }
        }
    }
}

pub const O_DIRECT: ::c_int = 0x10000;
pub const O_DIRECTORY: ::c_int = 0x4000;
pub const O_NOFOLLOW: ::c_int = 0x8000;
pub const O_LARGEFILE: ::c_int = 0o400000;

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
pub const SYS_getdents: ::c_long = 141;
pub const SYS_flock: ::c_long = 143;
pub const SYS_msync: ::c_long = 144;
pub const SYS_readv: ::c_long = 145;
pub const SYS_writev: ::c_long = 146;
pub const SYS_getsid: ::c_long = 147;
pub const SYS_fdatasync: ::c_long = 148;
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
pub const SYS_arm_fadvise64_64: ::c_long = 270;
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
pub const SYS_arm_sync_file_range: ::c_long = 341;
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

// offsets in mcontext_t.gregs from sys/ucontext.h
pub const REG_R0: ::c_int = 0;
pub const REG_R1: ::c_int = 1;
pub const REG_R2: ::c_int = 2;
pub const REG_R3: ::c_int = 3;
pub const REG_R4: ::c_int = 4;
pub const REG_R5: ::c_int = 5;
pub const REG_R6: ::c_int = 6;
pub const REG_R7: ::c_int = 7;
pub const REG_R8: ::c_int = 8;
pub const REG_R9: ::c_int = 9;
pub const REG_R10: ::c_int = 10;
pub const REG_R11: ::c_int = 11;
pub const REG_R12: ::c_int = 12;
pub const REG_R13: ::c_int = 13;
pub const REG_R14: ::c_int = 14;
pub const REG_R15: ::c_int = 15;

pub const NGREG: ::c_int = 18;

f! {
    // Sadly, Android before 5.0 (API level 21), the accept4 syscall is not
    // exposed by the libc. As work-around, we implement it through `syscall`
    // directly. This workaround can be removed if the minimum version of
    // Android is bumped. When the workaround is removed, `accept4` can be
    // moved back to `linux_like/mod.rs`
    pub fn accept4(
        fd: ::c_int,
        addr: *mut ::sockaddr,
        len: *mut ::socklen_t,
        flg: ::c_int
    ) -> ::c_int {
        ::syscall(SYS_accept4, fd, addr, len, flg) as ::c_int
    }
}
