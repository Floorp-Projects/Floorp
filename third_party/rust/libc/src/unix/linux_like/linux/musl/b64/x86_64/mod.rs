pub type c_char = i8;
pub type wchar_t = i32;
pub type nlink_t = u64;
pub type blksize_t = ::c_long;
pub type __u64 = ::c_ulonglong;
pub type greg_t = i64;

s! {
    pub struct stat {
        pub st_dev: ::dev_t,
        pub st_ino: ::ino_t,
        pub st_nlink: ::nlink_t,
        pub st_mode: ::mode_t,
        pub st_uid: ::uid_t,
        pub st_gid: ::gid_t,
        __pad0: ::c_int,
        pub st_rdev: ::dev_t,
        pub st_size: ::off_t,
        pub st_blksize: ::blksize_t,
        pub st_blocks: ::blkcnt_t,
        pub st_atime: ::time_t,
        pub st_atime_nsec: ::c_long,
        pub st_mtime: ::time_t,
        pub st_mtime_nsec: ::c_long,
        pub st_ctime: ::time_t,
        pub st_ctime_nsec: ::c_long,
        __unused: [::c_long; 3],
    }

    pub struct stat64 {
        pub st_dev: ::dev_t,
        pub st_ino: ::ino64_t,
        pub st_nlink: ::nlink_t,
        pub st_mode: ::mode_t,
        pub st_uid: ::uid_t,
        pub st_gid: ::gid_t,
        __pad0: ::c_int,
        pub st_rdev: ::dev_t,
        pub st_size: ::off_t,
        pub st_blksize: ::blksize_t,
        pub st_blocks: ::blkcnt64_t,
        pub st_atime: ::time_t,
        pub st_atime_nsec: ::c_long,
        pub st_mtime: ::time_t,
        pub st_mtime_nsec: ::c_long,
        pub st_ctime: ::time_t,
        pub st_ctime_nsec: ::c_long,
        __reserved: [::c_long; 3],
    }

    pub struct user_regs_struct {
        pub r15: ::c_ulong,
        pub r14: ::c_ulong,
        pub r13: ::c_ulong,
        pub r12: ::c_ulong,
        pub rbp: ::c_ulong,
        pub rbx: ::c_ulong,
        pub r11: ::c_ulong,
        pub r10: ::c_ulong,
        pub r9: ::c_ulong,
        pub r8: ::c_ulong,
        pub rax: ::c_ulong,
        pub rcx: ::c_ulong,
        pub rdx: ::c_ulong,
        pub rsi: ::c_ulong,
        pub rdi: ::c_ulong,
        pub orig_rax: ::c_ulong,
        pub rip: ::c_ulong,
        pub cs: ::c_ulong,
        pub eflags: ::c_ulong,
        pub rsp: ::c_ulong,
        pub ss: ::c_ulong,
        pub fs_base: ::c_ulong,
        pub gs_base: ::c_ulong,
        pub ds: ::c_ulong,
        pub es: ::c_ulong,
        pub fs: ::c_ulong,
        pub gs: ::c_ulong,
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
        #[cfg(target_pointer_width = "32")]
        __pad1: u32,
        pub u_ar0: *mut user_regs_struct,
        #[cfg(target_pointer_width = "32")]
        __pad2: u32,
        pub u_fpstate: *mut user_fpregs_struct,
        pub magic: ::c_ulong,
        pub u_comm: [::c_char; 32],
        pub u_debugreg: [::c_ulong; 8],
    }

    // GitHub repo: ifduyue/musl/
    // commit: b4b1e10364c8737a632be61582e05a8d3acf5690
    // file: arch/x86_64/bits/signal.h#L80-L84
    pub struct mcontext_t {
        pub gregs: [greg_t; 23],
        __private: [u64; 9],
    }

    pub struct ipc_perm {
        pub __ipc_perm_key: ::key_t,
        pub uid: ::uid_t,
        pub gid: ::gid_t,
        pub cuid: ::uid_t,
        pub cgid: ::gid_t,
        pub mode: ::mode_t,
        pub __seq: ::c_int,
        __unused1: ::c_long,
        __unused2: ::c_long
    }
}

s_no_extra_traits! {
    pub struct user_fpregs_struct {
        pub cwd: ::c_ushort,
        pub swd: ::c_ushort,
        pub ftw: ::c_ushort,
        pub fop: ::c_ushort,
        pub rip: ::c_ulong,
        pub rdp: ::c_ulong,
        pub mxcsr: ::c_uint,
        pub mxcr_mask: ::c_uint,
        pub st_space: [::c_uint; 32],
        pub xmm_space: [::c_uint; 64],
        padding: [::c_uint; 24],
    }

    pub struct ucontext_t {
        pub uc_flags: ::c_ulong,
        pub uc_link: *mut ucontext_t,
        pub uc_stack: ::stack_t,
        pub uc_mcontext: mcontext_t,
        pub uc_sigmask: ::sigset_t,
        __private: [u8; 512],
    }
}

cfg_if! {
    if #[cfg(feature = "extra_traits")] {
        impl PartialEq for user_fpregs_struct {
            fn eq(&self, other: &user_fpregs_struct) -> bool {
                self.cwd == other.cwd
                    && self.swd == other.swd
                    && self.ftw == other.ftw
                    && self.fop == other.fop
                    && self.rip == other.rip
                    && self.rdp == other.rdp
                    && self.mxcsr == other.mxcsr
                    && self.mxcr_mask == other.mxcr_mask
                    && self.st_space == other.st_space
                    && self
                    .xmm_space
                    .iter()
                    .zip(other.xmm_space.iter())
                    .all(|(a,b)| a == b)
                // Ignore padding field
            }
        }

        impl Eq for user_fpregs_struct {}

        impl ::fmt::Debug for user_fpregs_struct {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("user_fpregs_struct")
                    .field("cwd", &self.cwd)
                    .field("ftw", &self.ftw)
                    .field("fop", &self.fop)
                    .field("rip", &self.rip)
                    .field("rdp", &self.rdp)
                    .field("mxcsr", &self.mxcsr)
                    .field("mxcr_mask", &self.mxcr_mask)
                    .field("st_space", &self.st_space)
                // FIXME: .field("xmm_space", &self.xmm_space)
                // Ignore padding field
                    .finish()
            }
        }

        impl ::hash::Hash for user_fpregs_struct {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.cwd.hash(state);
                self.ftw.hash(state);
                self.fop.hash(state);
                self.rip.hash(state);
                self.rdp.hash(state);
                self.mxcsr.hash(state);
                self.mxcr_mask.hash(state);
                self.st_space.hash(state);
                self.xmm_space.hash(state);
                // Ignore padding field
            }
        }

        impl PartialEq for ucontext_t {
            fn eq(&self, other: &ucontext_t) -> bool {
                self.uc_flags == other.uc_flags
                    && self.uc_link == other.uc_link
                    && self.uc_stack == other.uc_stack
                    && self.uc_mcontext == other.uc_mcontext
                    && self.uc_sigmask == other.uc_sigmask
                    && self
                    .__private
                    .iter()
                    .zip(other.__private.iter())
                    .all(|(a,b)| a == b)
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
                    .field("uc_sigmask", &self.uc_sigmask)
                // Ignore __private field
                    .finish()
            }
        }

        impl ::hash::Hash for ucontext_t {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.uc_flags.hash(state);
                self.uc_link.hash(state);
                self.uc_stack.hash(state);
                self.uc_mcontext.hash(state);
                self.uc_sigmask.hash(state);
                self.__private.hash(state);
            }
        }
    }
}

// Syscall table

pub const SYS_read: ::c_long = 0;
pub const SYS_write: ::c_long = 1;
pub const SYS_open: ::c_long = 2;
pub const SYS_close: ::c_long = 3;
pub const SYS_stat: ::c_long = 4;
pub const SYS_fstat: ::c_long = 5;
pub const SYS_lstat: ::c_long = 6;
pub const SYS_poll: ::c_long = 7;
pub const SYS_lseek: ::c_long = 8;
pub const SYS_mmap: ::c_long = 9;
pub const SYS_mprotect: ::c_long = 10;
pub const SYS_munmap: ::c_long = 11;
pub const SYS_brk: ::c_long = 12;
pub const SYS_rt_sigaction: ::c_long = 13;
pub const SYS_rt_sigprocmask: ::c_long = 14;
pub const SYS_rt_sigreturn: ::c_long = 15;
pub const SYS_ioctl: ::c_long = 16;
pub const SYS_pread64: ::c_long = 17;
pub const SYS_pwrite64: ::c_long = 18;
pub const SYS_readv: ::c_long = 19;
pub const SYS_writev: ::c_long = 20;
pub const SYS_access: ::c_long = 21;
pub const SYS_pipe: ::c_long = 22;
pub const SYS_select: ::c_long = 23;
pub const SYS_sched_yield: ::c_long = 24;
pub const SYS_mremap: ::c_long = 25;
pub const SYS_msync: ::c_long = 26;
pub const SYS_mincore: ::c_long = 27;
pub const SYS_madvise: ::c_long = 28;
pub const SYS_shmget: ::c_long = 29;
pub const SYS_shmat: ::c_long = 30;
pub const SYS_shmctl: ::c_long = 31;
pub const SYS_dup: ::c_long = 32;
pub const SYS_dup2: ::c_long = 33;
pub const SYS_pause: ::c_long = 34;
pub const SYS_nanosleep: ::c_long = 35;
pub const SYS_getitimer: ::c_long = 36;
pub const SYS_alarm: ::c_long = 37;
pub const SYS_setitimer: ::c_long = 38;
pub const SYS_getpid: ::c_long = 39;
pub const SYS_sendfile: ::c_long = 40;
pub const SYS_socket: ::c_long = 41;
pub const SYS_connect: ::c_long = 42;
pub const SYS_accept: ::c_long = 43;
pub const SYS_sendto: ::c_long = 44;
pub const SYS_recvfrom: ::c_long = 45;
pub const SYS_sendmsg: ::c_long = 46;
pub const SYS_recvmsg: ::c_long = 47;
pub const SYS_shutdown: ::c_long = 48;
pub const SYS_bind: ::c_long = 49;
pub const SYS_listen: ::c_long = 50;
pub const SYS_getsockname: ::c_long = 51;
pub const SYS_getpeername: ::c_long = 52;
pub const SYS_socketpair: ::c_long = 53;
pub const SYS_setsockopt: ::c_long = 54;
pub const SYS_getsockopt: ::c_long = 55;
pub const SYS_clone: ::c_long = 56;
pub const SYS_fork: ::c_long = 57;
pub const SYS_vfork: ::c_long = 58;
pub const SYS_execve: ::c_long = 59;
pub const SYS_exit: ::c_long = 60;
pub const SYS_wait4: ::c_long = 61;
pub const SYS_kill: ::c_long = 62;
pub const SYS_uname: ::c_long = 63;
pub const SYS_semget: ::c_long = 64;
pub const SYS_semop: ::c_long = 65;
pub const SYS_semctl: ::c_long = 66;
pub const SYS_shmdt: ::c_long = 67;
pub const SYS_msgget: ::c_long = 68;
pub const SYS_msgsnd: ::c_long = 69;
pub const SYS_msgrcv: ::c_long = 70;
pub const SYS_msgctl: ::c_long = 71;
pub const SYS_fcntl: ::c_long = 72;
pub const SYS_flock: ::c_long = 73;
pub const SYS_fsync: ::c_long = 74;
pub const SYS_fdatasync: ::c_long = 75;
pub const SYS_truncate: ::c_long = 76;
pub const SYS_ftruncate: ::c_long = 77;
pub const SYS_getdents: ::c_long = 78;
pub const SYS_getcwd: ::c_long = 79;
pub const SYS_chdir: ::c_long = 80;
pub const SYS_fchdir: ::c_long = 81;
pub const SYS_rename: ::c_long = 82;
pub const SYS_mkdir: ::c_long = 83;
pub const SYS_rmdir: ::c_long = 84;
pub const SYS_creat: ::c_long = 85;
pub const SYS_link: ::c_long = 86;
pub const SYS_unlink: ::c_long = 87;
pub const SYS_symlink: ::c_long = 88;
pub const SYS_readlink: ::c_long = 89;
pub const SYS_chmod: ::c_long = 90;
pub const SYS_fchmod: ::c_long = 91;
pub const SYS_chown: ::c_long = 92;
pub const SYS_fchown: ::c_long = 93;
pub const SYS_lchown: ::c_long = 94;
pub const SYS_umask: ::c_long = 95;
pub const SYS_gettimeofday: ::c_long = 96;
pub const SYS_getrlimit: ::c_long = 97;
pub const SYS_getrusage: ::c_long = 98;
pub const SYS_sysinfo: ::c_long = 99;
pub const SYS_times: ::c_long = 100;
pub const SYS_ptrace: ::c_long = 101;
pub const SYS_getuid: ::c_long = 102;
pub const SYS_syslog: ::c_long = 103;
pub const SYS_getgid: ::c_long = 104;
pub const SYS_setuid: ::c_long = 105;
pub const SYS_setgid: ::c_long = 106;
pub const SYS_geteuid: ::c_long = 107;
pub const SYS_getegid: ::c_long = 108;
pub const SYS_setpgid: ::c_long = 109;
pub const SYS_getppid: ::c_long = 110;
pub const SYS_getpgrp: ::c_long = 111;
pub const SYS_setsid: ::c_long = 112;
pub const SYS_setreuid: ::c_long = 113;
pub const SYS_setregid: ::c_long = 114;
pub const SYS_getgroups: ::c_long = 115;
pub const SYS_setgroups: ::c_long = 116;
pub const SYS_setresuid: ::c_long = 117;
pub const SYS_getresuid: ::c_long = 118;
pub const SYS_setresgid: ::c_long = 119;
pub const SYS_getresgid: ::c_long = 120;
pub const SYS_getpgid: ::c_long = 121;
pub const SYS_setfsuid: ::c_long = 122;
pub const SYS_setfsgid: ::c_long = 123;
pub const SYS_getsid: ::c_long = 124;
pub const SYS_capget: ::c_long = 125;
pub const SYS_capset: ::c_long = 126;
pub const SYS_rt_sigpending: ::c_long = 127;
pub const SYS_rt_sigtimedwait: ::c_long = 128;
pub const SYS_rt_sigqueueinfo: ::c_long = 129;
pub const SYS_rt_sigsuspend: ::c_long = 130;
pub const SYS_sigaltstack: ::c_long = 131;
pub const SYS_utime: ::c_long = 132;
pub const SYS_mknod: ::c_long = 133;
pub const SYS_uselib: ::c_long = 134;
pub const SYS_personality: ::c_long = 135;
pub const SYS_ustat: ::c_long = 136;
pub const SYS_statfs: ::c_long = 137;
pub const SYS_fstatfs: ::c_long = 138;
pub const SYS_sysfs: ::c_long = 139;
pub const SYS_getpriority: ::c_long = 140;
pub const SYS_setpriority: ::c_long = 141;
pub const SYS_sched_setparam: ::c_long = 142;
pub const SYS_sched_getparam: ::c_long = 143;
pub const SYS_sched_setscheduler: ::c_long = 144;
pub const SYS_sched_getscheduler: ::c_long = 145;
pub const SYS_sched_get_priority_max: ::c_long = 146;
pub const SYS_sched_get_priority_min: ::c_long = 147;
pub const SYS_sched_rr_get_interval: ::c_long = 148;
pub const SYS_mlock: ::c_long = 149;
pub const SYS_munlock: ::c_long = 150;
pub const SYS_mlockall: ::c_long = 151;
pub const SYS_munlockall: ::c_long = 152;
pub const SYS_vhangup: ::c_long = 153;
pub const SYS_modify_ldt: ::c_long = 154;
pub const SYS_pivot_root: ::c_long = 155;
pub const SYS__sysctl: ::c_long = 156;
pub const SYS_prctl: ::c_long = 157;
pub const SYS_arch_prctl: ::c_long = 158;
pub const SYS_adjtimex: ::c_long = 159;
pub const SYS_setrlimit: ::c_long = 160;
pub const SYS_chroot: ::c_long = 161;
pub const SYS_sync: ::c_long = 162;
pub const SYS_acct: ::c_long = 163;
pub const SYS_settimeofday: ::c_long = 164;
pub const SYS_mount: ::c_long = 165;
pub const SYS_umount2: ::c_long = 166;
pub const SYS_swapon: ::c_long = 167;
pub const SYS_swapoff: ::c_long = 168;
pub const SYS_reboot: ::c_long = 169;
pub const SYS_sethostname: ::c_long = 170;
pub const SYS_setdomainname: ::c_long = 171;
pub const SYS_iopl: ::c_long = 172;
pub const SYS_ioperm: ::c_long = 173;
pub const SYS_create_module: ::c_long = 174;
pub const SYS_init_module: ::c_long = 175;
pub const SYS_delete_module: ::c_long = 176;
pub const SYS_get_kernel_syms: ::c_long = 177;
pub const SYS_query_module: ::c_long = 178;
pub const SYS_quotactl: ::c_long = 179;
pub const SYS_nfsservctl: ::c_long = 180;
pub const SYS_getpmsg: ::c_long = 181;
pub const SYS_putpmsg: ::c_long = 182;
pub const SYS_afs_syscall: ::c_long = 183;
pub const SYS_tuxcall: ::c_long = 184;
pub const SYS_security: ::c_long = 185;
pub const SYS_gettid: ::c_long = 186;
pub const SYS_readahead: ::c_long = 187;
pub const SYS_setxattr: ::c_long = 188;
pub const SYS_lsetxattr: ::c_long = 189;
pub const SYS_fsetxattr: ::c_long = 190;
pub const SYS_getxattr: ::c_long = 191;
pub const SYS_lgetxattr: ::c_long = 192;
pub const SYS_fgetxattr: ::c_long = 193;
pub const SYS_listxattr: ::c_long = 194;
pub const SYS_llistxattr: ::c_long = 195;
pub const SYS_flistxattr: ::c_long = 196;
pub const SYS_removexattr: ::c_long = 197;
pub const SYS_lremovexattr: ::c_long = 198;
pub const SYS_fremovexattr: ::c_long = 199;
pub const SYS_tkill: ::c_long = 200;
pub const SYS_time: ::c_long = 201;
pub const SYS_futex: ::c_long = 202;
pub const SYS_sched_setaffinity: ::c_long = 203;
pub const SYS_sched_getaffinity: ::c_long = 204;
pub const SYS_set_thread_area: ::c_long = 205;
pub const SYS_io_setup: ::c_long = 206;
pub const SYS_io_destroy: ::c_long = 207;
pub const SYS_io_getevents: ::c_long = 208;
pub const SYS_io_submit: ::c_long = 209;
pub const SYS_io_cancel: ::c_long = 210;
pub const SYS_get_thread_area: ::c_long = 211;
pub const SYS_lookup_dcookie: ::c_long = 212;
pub const SYS_epoll_create: ::c_long = 213;
pub const SYS_epoll_ctl_old: ::c_long = 214;
pub const SYS_epoll_wait_old: ::c_long = 215;
pub const SYS_remap_file_pages: ::c_long = 216;
pub const SYS_getdents64: ::c_long = 217;
pub const SYS_set_tid_address: ::c_long = 218;
pub const SYS_restart_syscall: ::c_long = 219;
pub const SYS_semtimedop: ::c_long = 220;
pub const SYS_fadvise64: ::c_long = 221;
pub const SYS_timer_create: ::c_long = 222;
pub const SYS_timer_settime: ::c_long = 223;
pub const SYS_timer_gettime: ::c_long = 224;
pub const SYS_timer_getoverrun: ::c_long = 225;
pub const SYS_timer_delete: ::c_long = 226;
pub const SYS_clock_settime: ::c_long = 227;
pub const SYS_clock_gettime: ::c_long = 228;
pub const SYS_clock_getres: ::c_long = 229;
pub const SYS_clock_nanosleep: ::c_long = 230;
pub const SYS_exit_group: ::c_long = 231;
pub const SYS_epoll_wait: ::c_long = 232;
pub const SYS_epoll_ctl: ::c_long = 233;
pub const SYS_tgkill: ::c_long = 234;
pub const SYS_utimes: ::c_long = 235;
pub const SYS_vserver: ::c_long = 236;
pub const SYS_mbind: ::c_long = 237;
pub const SYS_set_mempolicy: ::c_long = 238;
pub const SYS_get_mempolicy: ::c_long = 239;
pub const SYS_mq_open: ::c_long = 240;
pub const SYS_mq_unlink: ::c_long = 241;
pub const SYS_mq_timedsend: ::c_long = 242;
pub const SYS_mq_timedreceive: ::c_long = 243;
pub const SYS_mq_notify: ::c_long = 244;
pub const SYS_mq_getsetattr: ::c_long = 245;
pub const SYS_kexec_load: ::c_long = 246;
pub const SYS_waitid: ::c_long = 247;
pub const SYS_add_key: ::c_long = 248;
pub const SYS_request_key: ::c_long = 249;
pub const SYS_keyctl: ::c_long = 250;
pub const SYS_ioprio_set: ::c_long = 251;
pub const SYS_ioprio_get: ::c_long = 252;
pub const SYS_inotify_init: ::c_long = 253;
pub const SYS_inotify_add_watch: ::c_long = 254;
pub const SYS_inotify_rm_watch: ::c_long = 255;
pub const SYS_migrate_pages: ::c_long = 256;
pub const SYS_openat: ::c_long = 257;
pub const SYS_mkdirat: ::c_long = 258;
pub const SYS_mknodat: ::c_long = 259;
pub const SYS_fchownat: ::c_long = 260;
pub const SYS_futimesat: ::c_long = 261;
pub const SYS_newfstatat: ::c_long = 262;
pub const SYS_unlinkat: ::c_long = 263;
pub const SYS_renameat: ::c_long = 264;
pub const SYS_linkat: ::c_long = 265;
pub const SYS_symlinkat: ::c_long = 266;
pub const SYS_readlinkat: ::c_long = 267;
pub const SYS_fchmodat: ::c_long = 268;
pub const SYS_faccessat: ::c_long = 269;
pub const SYS_pselect6: ::c_long = 270;
pub const SYS_ppoll: ::c_long = 271;
pub const SYS_unshare: ::c_long = 272;
pub const SYS_set_robust_list: ::c_long = 273;
pub const SYS_get_robust_list: ::c_long = 274;
pub const SYS_splice: ::c_long = 275;
pub const SYS_tee: ::c_long = 276;
pub const SYS_sync_file_range: ::c_long = 277;
pub const SYS_vmsplice: ::c_long = 278;
pub const SYS_move_pages: ::c_long = 279;
pub const SYS_utimensat: ::c_long = 280;
pub const SYS_epoll_pwait: ::c_long = 281;
pub const SYS_signalfd: ::c_long = 282;
pub const SYS_timerfd_create: ::c_long = 283;
pub const SYS_eventfd: ::c_long = 284;
pub const SYS_fallocate: ::c_long = 285;
pub const SYS_timerfd_settime: ::c_long = 286;
pub const SYS_timerfd_gettime: ::c_long = 287;
pub const SYS_accept4: ::c_long = 288;
pub const SYS_signalfd4: ::c_long = 289;
pub const SYS_eventfd2: ::c_long = 290;
pub const SYS_epoll_create1: ::c_long = 291;
pub const SYS_dup3: ::c_long = 292;
pub const SYS_pipe2: ::c_long = 293;
pub const SYS_inotify_init1: ::c_long = 294;
pub const SYS_preadv: ::c_long = 295;
pub const SYS_pwritev: ::c_long = 296;
pub const SYS_rt_tgsigqueueinfo: ::c_long = 297;
pub const SYS_perf_event_open: ::c_long = 298;
pub const SYS_recvmmsg: ::c_long = 299;
pub const SYS_fanotify_init: ::c_long = 300;
pub const SYS_fanotify_mark: ::c_long = 301;
pub const SYS_prlimit64: ::c_long = 302;
pub const SYS_name_to_handle_at: ::c_long = 303;
pub const SYS_open_by_handle_at: ::c_long = 304;
pub const SYS_clock_adjtime: ::c_long = 305;
pub const SYS_syncfs: ::c_long = 306;
pub const SYS_sendmmsg: ::c_long = 307;
pub const SYS_setns: ::c_long = 308;
pub const SYS_getcpu: ::c_long = 309;
pub const SYS_process_vm_readv: ::c_long = 310;
pub const SYS_process_vm_writev: ::c_long = 311;
pub const SYS_kcmp: ::c_long = 312;
pub const SYS_finit_module: ::c_long = 313;
pub const SYS_sched_setattr: ::c_long = 314;
pub const SYS_sched_getattr: ::c_long = 315;
pub const SYS_renameat2: ::c_long = 316;
pub const SYS_seccomp: ::c_long = 317;
pub const SYS_getrandom: ::c_long = 318;
pub const SYS_memfd_create: ::c_long = 319;
pub const SYS_kexec_file_load: ::c_long = 320;
pub const SYS_bpf: ::c_long = 321;
pub const SYS_execveat: ::c_long = 322;
pub const SYS_userfaultfd: ::c_long = 323;
pub const SYS_membarrier: ::c_long = 324;
pub const SYS_mlock2: ::c_long = 325;
pub const SYS_copy_file_range: ::c_long = 326;
pub const SYS_preadv2: ::c_long = 327;
pub const SYS_pwritev2: ::c_long = 328;
pub const SYS_pkey_mprotect: ::c_long = 329;
pub const SYS_pkey_alloc: ::c_long = 330;
pub const SYS_pkey_free: ::c_long = 331;
pub const SYS_statx: ::c_long = 332;
pub const SYS_pidfd_send_signal: ::c_long = 424;
pub const SYS_io_uring_setup: ::c_long = 425;
pub const SYS_io_uring_enter: ::c_long = 426;
pub const SYS_io_uring_register: ::c_long = 427;
pub const SYS_open_tree: ::c_long = 428;
pub const SYS_move_mount: ::c_long = 429;
pub const SYS_fsopen: ::c_long = 430;
pub const SYS_fsconfig: ::c_long = 431;
pub const SYS_fsmount: ::c_long = 432;
pub const SYS_fspick: ::c_long = 433;
pub const SYS_pidfd_open: ::c_long = 434;
pub const SYS_clone3: ::c_long = 435;
pub const SYS_close_range: ::c_long = 436;
pub const SYS_openat2: ::c_long = 437;
pub const SYS_pidfd_getfd: ::c_long = 438;
pub const SYS_faccessat2: ::c_long = 439;
pub const SYS_process_madvise: ::c_long = 440;
pub const SYS_epoll_pwait2: ::c_long = 441;
pub const SYS_mount_setattr: ::c_long = 442;

// offsets in user_regs_structs, from sys/reg.h
pub const R15: ::c_int = 0;
pub const R14: ::c_int = 1;
pub const R13: ::c_int = 2;
pub const R12: ::c_int = 3;
pub const RBP: ::c_int = 4;
pub const RBX: ::c_int = 5;
pub const R11: ::c_int = 6;
pub const R10: ::c_int = 7;
pub const R9: ::c_int = 8;
pub const R8: ::c_int = 9;
pub const RAX: ::c_int = 10;
pub const RCX: ::c_int = 11;
pub const RDX: ::c_int = 12;
pub const RSI: ::c_int = 13;
pub const RDI: ::c_int = 14;
pub const ORIG_RAX: ::c_int = 15;
pub const RIP: ::c_int = 16;
pub const CS: ::c_int = 17;
pub const EFLAGS: ::c_int = 18;
pub const RSP: ::c_int = 19;
pub const SS: ::c_int = 20;
pub const FS_BASE: ::c_int = 21;
pub const GS_BASE: ::c_int = 22;
pub const DS: ::c_int = 23;
pub const ES: ::c_int = 24;
pub const FS: ::c_int = 25;
pub const GS: ::c_int = 26;

// offsets in mcontext_t.gregs from bits/signal.h
// GitHub repo: ifduyue/musl/
// commit: b4b1e10364c8737a632be61582e05a8d3acf5690
// file: arch/x86_64/bits/signal.h#L9-L56
pub const REG_R8: ::c_int = 0;
pub const REG_R9: ::c_int = 1;
pub const REG_R10: ::c_int = 2;
pub const REG_R11: ::c_int = 3;
pub const REG_R12: ::c_int = 4;
pub const REG_R13: ::c_int = 5;
pub const REG_R14: ::c_int = 6;
pub const REG_R15: ::c_int = 7;
pub const REG_RDI: ::c_int = 8;
pub const REG_RSI: ::c_int = 9;
pub const REG_RBP: ::c_int = 10;
pub const REG_RBX: ::c_int = 11;
pub const REG_RDX: ::c_int = 12;
pub const REG_RAX: ::c_int = 13;
pub const REG_RCX: ::c_int = 14;
pub const REG_RSP: ::c_int = 15;
pub const REG_RIP: ::c_int = 16;
pub const REG_EFL: ::c_int = 17;
pub const REG_CSGSFS: ::c_int = 18;
pub const REG_ERR: ::c_int = 19;
pub const REG_TRAPNO: ::c_int = 20;
pub const REG_OLDMASK: ::c_int = 21;
pub const REG_CR2: ::c_int = 22;

pub const MADV_SOFT_OFFLINE: ::c_int = 101;
pub const MAP_32BIT: ::c_int = 0x0040;
pub const O_APPEND: ::c_int = 1024;
pub const O_DIRECT: ::c_int = 0x4000;
pub const O_DIRECTORY: ::c_int = 0x10000;
pub const O_LARGEFILE: ::c_int = 0;
pub const O_NOFOLLOW: ::c_int = 0x20000;
pub const O_CREAT: ::c_int = 64;
pub const O_EXCL: ::c_int = 128;
pub const O_NOCTTY: ::c_int = 256;
pub const O_NONBLOCK: ::c_int = 2048;
pub const O_SYNC: ::c_int = 1052672;
pub const O_RSYNC: ::c_int = 1052672;
pub const O_DSYNC: ::c_int = 4096;
pub const O_ASYNC: ::c_int = 0x2000;

pub const TIOCGRS485: ::c_int = 0x542E;
pub const TIOCSRS485: ::c_int = 0x542F;

pub const SIGSTKSZ: ::size_t = 8192;
pub const MINSIGSTKSZ: ::size_t = 2048;

pub const ENAMETOOLONG: ::c_int = 36;
pub const ENOLCK: ::c_int = 37;
pub const ENOSYS: ::c_int = 38;
pub const ENOTEMPTY: ::c_int = 39;
pub const ELOOP: ::c_int = 40;
pub const ENOMSG: ::c_int = 42;
pub const EIDRM: ::c_int = 43;
pub const ECHRNG: ::c_int = 44;
pub const EL2NSYNC: ::c_int = 45;
pub const EL3HLT: ::c_int = 46;
pub const EL3RST: ::c_int = 47;
pub const ELNRNG: ::c_int = 48;
pub const EUNATCH: ::c_int = 49;
pub const ENOCSI: ::c_int = 50;
pub const EL2HLT: ::c_int = 51;
pub const EBADE: ::c_int = 52;
pub const EBADR: ::c_int = 53;
pub const EXFULL: ::c_int = 54;
pub const ENOANO: ::c_int = 55;
pub const EBADRQC: ::c_int = 56;
pub const EBADSLT: ::c_int = 57;
pub const EMULTIHOP: ::c_int = 72;
pub const EBADMSG: ::c_int = 74;
pub const EOVERFLOW: ::c_int = 75;
pub const ENOTUNIQ: ::c_int = 76;
pub const EBADFD: ::c_int = 77;
pub const EREMCHG: ::c_int = 78;
pub const ELIBACC: ::c_int = 79;
pub const ELIBBAD: ::c_int = 80;
pub const ELIBSCN: ::c_int = 81;
pub const ELIBMAX: ::c_int = 82;
pub const ELIBEXEC: ::c_int = 83;
pub const EILSEQ: ::c_int = 84;
pub const ERESTART: ::c_int = 85;
pub const ESTRPIPE: ::c_int = 86;
pub const EUSERS: ::c_int = 87;
pub const ENOTSOCK: ::c_int = 88;
pub const EDESTADDRREQ: ::c_int = 89;
pub const EMSGSIZE: ::c_int = 90;
pub const EPROTOTYPE: ::c_int = 91;
pub const ENOPROTOOPT: ::c_int = 92;
pub const EPROTONOSUPPORT: ::c_int = 93;
pub const ESOCKTNOSUPPORT: ::c_int = 94;
pub const EOPNOTSUPP: ::c_int = 95;
pub const ENOTSUP: ::c_int = EOPNOTSUPP;
pub const EPFNOSUPPORT: ::c_int = 96;
pub const EAFNOSUPPORT: ::c_int = 97;
pub const EADDRINUSE: ::c_int = 98;
pub const EADDRNOTAVAIL: ::c_int = 99;
pub const ENETDOWN: ::c_int = 100;
pub const ENETUNREACH: ::c_int = 101;
pub const ENETRESET: ::c_int = 102;
pub const ECONNABORTED: ::c_int = 103;
pub const ECONNRESET: ::c_int = 104;
pub const ENOBUFS: ::c_int = 105;
pub const EISCONN: ::c_int = 106;
pub const ENOTCONN: ::c_int = 107;
pub const ESHUTDOWN: ::c_int = 108;
pub const ETOOMANYREFS: ::c_int = 109;
pub const ETIMEDOUT: ::c_int = 110;
pub const ECONNREFUSED: ::c_int = 111;
pub const EHOSTDOWN: ::c_int = 112;
pub const EHOSTUNREACH: ::c_int = 113;
pub const EALREADY: ::c_int = 114;
pub const EINPROGRESS: ::c_int = 115;
pub const ESTALE: ::c_int = 116;
pub const EUCLEAN: ::c_int = 117;
pub const ENOTNAM: ::c_int = 118;
pub const ENAVAIL: ::c_int = 119;
pub const EISNAM: ::c_int = 120;
pub const EREMOTEIO: ::c_int = 121;
pub const EDQUOT: ::c_int = 122;
pub const ENOMEDIUM: ::c_int = 123;
pub const EMEDIUMTYPE: ::c_int = 124;
pub const ECANCELED: ::c_int = 125;
pub const ENOKEY: ::c_int = 126;
pub const EKEYEXPIRED: ::c_int = 127;
pub const EKEYREVOKED: ::c_int = 128;
pub const EKEYREJECTED: ::c_int = 129;
pub const EOWNERDEAD: ::c_int = 130;
pub const ENOTRECOVERABLE: ::c_int = 131;
pub const ERFKILL: ::c_int = 132;
pub const EHWPOISON: ::c_int = 133;

pub const SA_ONSTACK: ::c_int = 0x08000000;
pub const SA_SIGINFO: ::c_int = 0x00000004;
pub const SA_NOCLDWAIT: ::c_int = 0x00000002;

pub const SIGCHLD: ::c_int = 17;
pub const SIGBUS: ::c_int = 7;
pub const SIGTTIN: ::c_int = 21;
pub const SIGTTOU: ::c_int = 22;
pub const SIGXCPU: ::c_int = 24;
pub const SIGXFSZ: ::c_int = 25;
pub const SIGVTALRM: ::c_int = 26;
pub const SIGPROF: ::c_int = 27;
pub const SIGWINCH: ::c_int = 28;
pub const SIGUSR1: ::c_int = 10;
pub const SIGUSR2: ::c_int = 12;
pub const SIGCONT: ::c_int = 18;
pub const SIGSTOP: ::c_int = 19;
pub const SIGTSTP: ::c_int = 20;
pub const SIGURG: ::c_int = 23;
pub const SIGIO: ::c_int = 29;
pub const SIGSYS: ::c_int = 31;
pub const SIGSTKFLT: ::c_int = 16;
pub const SIGPOLL: ::c_int = 29;
pub const SIGPWR: ::c_int = 30;
pub const SIG_SETMASK: ::c_int = 2;
pub const SIG_BLOCK: ::c_int = 0x000000;
pub const SIG_UNBLOCK: ::c_int = 0x01;

pub const F_GETLK: ::c_int = 5;
pub const F_GETOWN: ::c_int = 9;
pub const F_SETLK: ::c_int = 6;
pub const F_SETLKW: ::c_int = 7;
pub const F_SETOWN: ::c_int = 8;
pub const F_OFD_GETLK: ::c_int = 36;
pub const F_OFD_SETLK: ::c_int = 37;
pub const F_OFD_SETLKW: ::c_int = 38;

pub const VEOF: usize = 4;

pub const POLLWRNORM: ::c_short = 0x100;
pub const POLLWRBAND: ::c_short = 0x200;

pub const SOCK_STREAM: ::c_int = 1;
pub const SOCK_DGRAM: ::c_int = 2;

pub const MAP_ANON: ::c_int = 0x0020;
pub const MAP_GROWSDOWN: ::c_int = 0x0100;
pub const MAP_DENYWRITE: ::c_int = 0x0800;
pub const MAP_EXECUTABLE: ::c_int = 0x01000;
pub const MAP_LOCKED: ::c_int = 0x02000;
pub const MAP_NORESERVE: ::c_int = 0x04000;
pub const MAP_POPULATE: ::c_int = 0x08000;
pub const MAP_NONBLOCK: ::c_int = 0x010000;
pub const MAP_STACK: ::c_int = 0x020000;
pub const MAP_HUGETLB: ::c_int = 0x040000;
pub const MAP_SYNC: ::c_int = 0x080000;

pub const RLIMIT_NLIMITS: ::c_int = 15;
pub const RLIM_NLIMITS: ::c_int = RLIMIT_NLIMITS;
pub const TIOCINQ: ::c_int = ::FIONREAD;
pub const MCL_CURRENT: ::c_int = 0x0001;
pub const MCL_FUTURE: ::c_int = 0x0002;
pub const CBAUD: ::tcflag_t = 0o0010017;
pub const TAB1: ::c_int = 0x00000800;
pub const TAB2: ::c_int = 0x00001000;
pub const TAB3: ::c_int = 0x00001800;
pub const CR1: ::c_int = 0x00000200;
pub const CR2: ::c_int = 0x00000400;
pub const CR3: ::c_int = 0x00000600;
pub const FF1: ::c_int = 0x00008000;
pub const BS1: ::c_int = 0x00002000;
pub const VT1: ::c_int = 0x00004000;
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
pub const CIBAUD: ::tcflag_t = 0o02003600000;
pub const CBAUDEX: ::tcflag_t = 0o010000;
pub const VSWTC: usize = 7;
pub const OLCUC: ::tcflag_t = 0o000002;
pub const NLDLY: ::tcflag_t = 0o000400;
pub const CRDLY: ::tcflag_t = 0o003000;
pub const TABDLY: ::tcflag_t = 0o014000;
pub const BSDLY: ::tcflag_t = 0o020000;
pub const FFDLY: ::tcflag_t = 0o100000;
pub const VTDLY: ::tcflag_t = 0o040000;
pub const XTABS: ::tcflag_t = 0o014000;
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

pub const FIOCLEX: ::c_int = 0x5451;
pub const FIONCLEX: ::c_int = 0x5450;
pub const FIONBIO: ::c_int = 0x5421;
pub const EDEADLK: ::c_int = 35;
pub const EDEADLOCK: ::c_int = EDEADLK;

pub const EXTPROC: ::tcflag_t = 0x00010000;
pub const VEOL: usize = 11;
pub const VEOL2: usize = 16;
pub const VMIN: usize = 6;
pub const IEXTEN: ::tcflag_t = 0x00008000;
pub const TOSTOP: ::tcflag_t = 0x00000100;
pub const FLUSHO: ::tcflag_t = 0x00001000;
pub const TCGETS: ::c_int = 0x5401;
pub const TCSETS: ::c_int = 0x5402;
pub const TCSETSW: ::c_int = 0x5403;
pub const TCSETSF: ::c_int = 0x5404;
pub const TCGETA: ::c_int = 0x5405;
pub const TCSETA: ::c_int = 0x5406;
pub const TCSETAW: ::c_int = 0x5407;
pub const TCSETAF: ::c_int = 0x5408;
pub const TCSBRK: ::c_int = 0x5409;
pub const TCXONC: ::c_int = 0x540A;
pub const TCFLSH: ::c_int = 0x540B;
pub const TIOCGSOFTCAR: ::c_int = 0x5419;
pub const TIOCSSOFTCAR: ::c_int = 0x541A;
pub const TIOCLINUX: ::c_int = 0x541C;
pub const TIOCGSERIAL: ::c_int = 0x541E;
pub const TIOCEXCL: ::c_int = 0x540C;
pub const TIOCNXCL: ::c_int = 0x540D;
pub const TIOCSCTTY: ::c_int = 0x540E;
pub const TIOCGPGRP: ::c_int = 0x540F;
pub const TIOCSPGRP: ::c_int = 0x5410;
pub const TIOCOUTQ: ::c_int = 0x5411;
pub const TIOCSTI: ::c_int = 0x5412;
pub const TIOCGWINSZ: ::c_int = 0x5413;
pub const TIOCSWINSZ: ::c_int = 0x5414;
pub const TIOCMGET: ::c_int = 0x5415;
pub const TIOCMBIS: ::c_int = 0x5416;
pub const TIOCMBIC: ::c_int = 0x5417;
pub const TIOCMSET: ::c_int = 0x5418;
pub const FIONREAD: ::c_int = 0x541B;
pub const TIOCCONS: ::c_int = 0x541D;

pub const TIOCM_LE: ::c_int = 0x001;
pub const TIOCM_DTR: ::c_int = 0x002;
pub const TIOCM_RTS: ::c_int = 0x004;
pub const TIOCM_ST: ::c_int = 0x008;
pub const TIOCM_SR: ::c_int = 0x010;
pub const TIOCM_CTS: ::c_int = 0x020;
pub const TIOCM_CAR: ::c_int = 0x040;
pub const TIOCM_RNG: ::c_int = 0x080;
pub const TIOCM_DSR: ::c_int = 0x100;
pub const TIOCM_CD: ::c_int = TIOCM_CAR;
pub const TIOCM_RI: ::c_int = TIOCM_RNG;

extern "C" {
    pub fn ioctl(fd: ::c_int, request: ::c_int, ...) -> ::c_int;
}

cfg_if! {
    if #[cfg(libc_align)] {
        mod align;
        pub use self::align::*;
    }
}
