//! Definitions for l4re-uclibc on 64bit systems

pub type blkcnt_t = i64;
pub type blksize_t = i64;
pub type c_char = u8;
pub type c_long = i64;
pub type c_ulong = u64;
pub type fsblkcnt_t = ::c_ulong;
pub type fsfilcnt_t = ::c_ulong;
pub type fsword_t = ::c_long;
pub type ino_t = ::c_ulong;
pub type nlink_t = ::c_uint;
pub type off_t = ::c_long;
pub type rlim_t = c_ulong;
pub type rlim64_t = u64;
pub type suseconds_t = ::c_long;
pub type time_t = ::c_int;
pub type wchar_t = ::c_int;

// ToDo, used?
//pub type d_ino = ::c_ulong;
pub type nfds_t = ::c_ulong;

s! {
    // ------------------------------------------------------------
    // networking
    pub struct in_addr {
        pub s_addr: in_addr_t,
    }

    pub struct in6_addr {
        pub s6_addr: [u8; 16],
        __align: [u32; 0],
    }

    pub struct sockaddr {
        pub sa_family: sa_family_t,
        pub sa_data: [::c_char; 14],
    }

    pub struct sockaddr_in {
        pub sin_family: sa_family_t,
        pub sin_port: ::in_port_t,
        pub sin_addr: ::in_addr,
        pub sin_zero: [u8; 8],
    }

    pub struct sockaddr_in6 {
        pub sin6_family: sa_family_t,
        pub sin6_port: in_port_t,
        pub sin6_flowinfo: u32,
        pub sin6_addr: ::in6_addr,
        pub sin6_scope_id: u32,
    }

    // ------------------------------------------------------------
    // definitions below are *unverified* and might **break** the software
    pub struct stat { // ToDo
        pub st_dev: ::c_ulong,
        st_pad1: [::c_long; 2],
        pub st_ino: ::ino_t,
        pub st_mode: ::mode_t,
        pub st_nlink: ::nlink_t,
        pub st_uid: ::uid_t,
        pub st_gid: ::gid_t,
        pub st_rdev: u64,
        pub st_pad2: [u64; 1],
        pub st_size: off_t,
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

    pub struct statvfs { // ToDo: broken
        pub f_bsize: ::c_ulong,
        pub f_frsize: ::c_ulong,
        pub f_blocks: ::fsblkcnt_t,
        pub f_bfree: ::fsblkcnt_t,
        pub f_bavail: ::fsblkcnt_t,
        pub f_files: ::fsfilcnt_t,
        pub f_ffree: ::fsfilcnt_t,
        pub f_favail: ::fsfilcnt_t,
        #[cfg(target_endian = "little")]
        pub f_fsid: ::c_ulong,
        #[cfg(target_pointer_width = "32")]
        __f_unused: ::c_int,
        #[cfg(target_endian = "big")]
        pub f_fsid: ::c_ulong,
        pub f_flag: ::c_ulong,
        pub f_namemax: ::c_ulong,
        __f_spare: [::c_int; 6],
    }

    pub struct dirent { // Todo
        pub d_ino: ino_64_t,
        pub d_off: off64_t,
        d_reclen: u16,
        pub d_type: u8,
        pub d_name: [i8; 256],
    }

    pub struct dirent64 { //
        pub d_ino: ino64_t,
        pub d_off: off64_t,
        pub d_reclen: u16,
        pub d_type: u8,
        pub d_name: [i8; 256],
    }

    pub struct pthread_attr_t { // ToDo
        __size: [u64; 7]
    }

    pub struct sigaction { // TODO!!
        pub sa_sigaction: ::sighandler_t,
        pub sa_mask: ::sigset_t,
        #[cfg(target_arch = "sparc64")]
        __reserved0: ::c_int,
        pub sa_flags: ::c_int,
        _restorer: *mut ::c_void,
    }

    pub struct stack_t { // ToDo
        pub ss_sp: *mut ::c_void,
        pub ss_flags: ::c_int,
        pub ss_size: ::size_t
    }

    pub struct statfs { // ToDo
        pub f_type: fsword_t,
        pub f_bsize: fsword_t,
        pub f_blocks: ::fsblkcnt_t,
        pub f_bfree: ::fsblkcnt_t,
        pub f_bavail: ::fsblkcnt_t,
        pub f_files: ::fsfilcnt_t,
        pub f_ffree: ::fsfilcnt_t,
        pub f_fsid: ::fsid_t,
        pub f_namelen: fsword_t,
        pub f_frsize: fsword_t,
        f_spare: [fsword_t; 5],
    }

    pub struct msghdr { // ToDo
        pub msg_name: *mut ::c_void,
        pub msg_namelen: ::socklen_t,
        pub msg_iov: *mut ::iovec,
        pub msg_iovlen: ::size_t,
        pub msg_control: *mut ::c_void,
        pub msg_controllen: ::size_t,
        pub msg_flags: ::c_int,
    }

    pub struct termios { // ToDo
        pub c_iflag: ::tcflag_t,
        pub c_oflag: ::tcflag_t,
        pub c_cflag: ::tcflag_t,
        pub c_lflag: ::tcflag_t,
        pub c_line: ::cc_t,
        pub c_cc: [::cc_t; ::NCCS],
    }

    pub struct sem_t { // ToDo
        #[cfg(target_pointer_width = "32")]
        __size: [::c_char; 16],
        #[cfg(target_pointer_width = "64")]
        __size: [::c_char; 32],
        __align: [::c_long; 0],
    }

    pub struct pthread_mutex_t { // ToDo
        #[cfg(any(target_arch = "mips", target_arch = "arm",
                  target_arch = "powerpc"))]
        __align: [::c_long; 0],
        #[cfg(not(any(target_arch = "mips", target_arch = "arm",
                      target_arch = "powerpc")))]
        __align: [::c_longlong; 0],
        size: [u8; __SIZEOF_PTHREAD_MUTEX_T],
    }

    pub struct pthread_mutexattr_t { // ToDo
        #[cfg(any(target_arch = "x86_64", target_arch = "powerpc64",
                  target_arch = "mips64", target_arch = "s390x",
                  target_arch = "sparc64"))]
        __align: [::c_int; 0],
        #[cfg(not(any(target_arch = "x86_64", target_arch = "powerpc64",
                      target_arch = "mips64", target_arch = "s390x",
                      target_arch = "sparc64")))]
        __align: [::c_long; 0],
        size: [u8; __SIZEOF_PTHREAD_MUTEXATTR_T],
    }

    pub struct pthread_cond_t { // ToDo
        __align: [::c_longlong; 0],
        size: [u8; __SIZEOF_PTHREAD_COND_T],
    }

    pub struct pthread_condattr_t { // ToDo
        __align: [::c_int; 0],
        size: [u8; __SIZEOF_PTHREAD_CONDATTR_T],
    }

    pub struct pthread_rwlock_t { // ToDo
        #[cfg(any(target_arch = "mips", target_arch = "arm",
                  target_arch = "powerpc"))]
        __align: [::c_long; 0],
        #[cfg(not(any(target_arch = "mips", target_arch = "arm",
                      target_arch = "powerpc")))]
        __align: [::c_longlong; 0],
        size: [u8; __SIZEOF_PTHREAD_RWLOCK_T],
    }

    pub struct sigset_t { // ToDo
        __val: [::c_ulong; 16],
    }

    pub struct sysinfo { // ToDo
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

    pub struct glob_t { // ToDo
        pub gl_pathc: ::size_t,
        pub gl_pathv: *mut *mut c_char,
        pub gl_offs: ::size_t,
        pub gl_flags: ::c_int,
        __unused1: *mut ::c_void,
        __unused2: *mut ::c_void,
        __unused3: *mut ::c_void,
        __unused4: *mut ::c_void,
        __unused5: *mut ::c_void,
    }

    pub struct stat64 { // ToDo
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

    pub struct rlimit64 { // ToDo
        pub rlim_cur: rlim64_t,
        pub rlim_max: rlim64_t,
    }

    pub struct cpu_set_t { // ToDo
        #[cfg(target_pointer_width = "32")]
        bits: [u32; 32],
        #[cfg(target_pointer_width = "64")]
        bits: [u64; 16],
    }

    pub struct timespec { // ToDo
        tv_sec: time_t, // seconds
        tv_nsec: ::c_ulong, // nanoseconds
    }

    pub struct fsid_t { // ToDo
        __val: [::c_int; 2],
    }
}

// constants
pub const O_CLOEXEC: ::c_int = 0x80000;
pub const O_DIRECTORY: ::c_int = 0200000;
pub const NCCS: usize = 32;
pub const __SIZEOF_PTHREAD_MUTEX_T: usize = 40;
pub const __SIZEOF_PTHREAD_MUTEXATTR_T: usize = 4;
pub const PTHREAD_MUTEX_NORMAL: ::c_int = 0;
pub const PTHREAD_MUTEX_RECURSIVE: ::c_int = 1;
pub const PTHREAD_MUTEX_ERRORCHECK: ::c_int = 2;
pub const PTHREAD_MUTEX_DEFAULT: ::c_int = PTHREAD_MUTEX_NORMAL;
pub const __SIZEOF_PTHREAD_COND_T: usize = 48;
pub const __SIZEOF_PTHREAD_CONDATTR_T: usize = 4;
pub const __SIZEOF_PTHREAD_RWLOCK_T: usize = 56;

extern {
    pub fn memalign(align: ::size_t, size: ::size_t) -> *mut ::c_void;
}
