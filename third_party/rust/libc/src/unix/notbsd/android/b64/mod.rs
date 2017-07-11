// The following definitions are correct for aarch64 and x86_64,
// but may be wrong for mips64

pub type c_long = i64;
pub type c_ulong = u64;
pub type mode_t = u32;
pub type off64_t = i64;
pub type socklen_t = u32;

s! {
    pub struct sigset_t {
        __val: [::c_ulong; 1],
    }

    pub struct sigaction {
        pub sa_flags: ::c_uint,
        pub sa_sigaction: ::sighandler_t,
        pub sa_mask: ::sigset_t,
        pub sa_restorer: ::dox::Option<extern fn()>,
    }

    pub struct rlimit64 {
        pub rlim_cur: ::c_ulonglong,
        pub rlim_max: ::c_ulonglong,
    }

    pub struct pthread_attr_t {
        pub flags: ::uint32_t,
        pub stack_base: *mut ::c_void,
        pub stack_size: ::size_t,
        pub guard_size: ::size_t,
        pub sched_policy: ::int32_t,
        pub sched_priority: ::int32_t,
        __reserved: [::c_char; 16],
    }

    pub struct pthread_mutex_t {
        value: ::c_int,
        __reserved: [::c_char; 36],
    }

    pub struct pthread_cond_t {
        value: ::c_int,
        __reserved: [::c_char; 44],
    }

    pub struct pthread_rwlock_t {
        numLocks: ::c_int,
        writerThreadId: ::c_int,
        pendingReaders: ::c_int,
        pendingWriters: ::c_int,
        attr: i32,
        __reserved: [::c_char; 36],
    }

    pub struct passwd {
        pub pw_name: *mut ::c_char,
        pub pw_passwd: *mut ::c_char,
        pub pw_uid: ::uid_t,
        pub pw_gid: ::gid_t,
        pub pw_gecos: *mut ::c_char,
        pub pw_dir: *mut ::c_char,
        pub pw_shell: *mut ::c_char,
    }

    pub struct statfs {
        pub f_type: ::uint64_t,
        pub f_bsize: ::uint64_t,
        pub f_blocks: ::uint64_t,
        pub f_bfree: ::uint64_t,
        pub f_bavail: ::uint64_t,
        pub f_files: ::uint64_t,
        pub f_ffree: ::uint64_t,
        pub f_fsid: ::__fsid_t,
        pub f_namelen: ::uint64_t,
        pub f_frsize: ::uint64_t,
        pub f_flags: ::uint64_t,
        pub f_spare: [::uint64_t; 4],
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

pub const RTLD_GLOBAL: ::c_int = 0x00100;
pub const RTLD_NOW: ::c_int = 2;
pub const RTLD_DEFAULT: *mut ::c_void = 0i64 as *mut ::c_void;

pub const PTHREAD_MUTEX_INITIALIZER: pthread_mutex_t = pthread_mutex_t {
    value: 0,
    __reserved: [0; 36],
};
pub const PTHREAD_COND_INITIALIZER: pthread_cond_t = pthread_cond_t {
    value: 0,
    __reserved: [0; 44],
};
pub const PTHREAD_RWLOCK_INITIALIZER: pthread_rwlock_t = pthread_rwlock_t {
    numLocks: 0,
    writerThreadId: 0,
    pendingReaders: 0,
    pendingWriters: 0,
    attr: 0,
    __reserved: [0; 36],
};
pub const PTHREAD_STACK_MIN: ::size_t = 4096 * 4;
pub const CPU_SETSIZE: ::size_t = 1024;
pub const __CPU_BITS: ::size_t = 64;

pub const UT_LINESIZE: usize = 32;
pub const UT_NAMESIZE: usize = 32;
pub const UT_HOSTSIZE: usize = 256;

// Some weirdness in Android
extern {
    // address_len should be socklen_t, but it is c_int!
    pub fn bind(socket: ::c_int, address: *const ::sockaddr,
                address_len: ::c_int) -> ::c_int;

    // the return type should be ::ssize_t, but it is c_int!
    pub fn writev(fd: ::c_int,
                  iov: *const ::iovec,
                  iovcnt: ::c_int) -> ::c_int;

    // the return type should be ::ssize_t, but it is c_int!
    pub fn readv(fd: ::c_int,
                 iov: *const ::iovec,
                 iovcnt: ::c_int) -> ::c_int;

    // the return type should be ::ssize_t, but it is c_int!
    pub fn sendmsg(fd: ::c_int,
                   msg: *const ::msghdr,
                   flags: ::c_int) -> ::c_int;

    // the return type should be ::ssize_t, but it is c_int!
    pub fn recvmsg(fd: ::c_int, msg: *mut ::msghdr, flags: ::c_int) -> ::c_int;
}

cfg_if! {
    if #[cfg(target_arch = "x86_64")] {
        mod x86_64;
        pub use self::x86_64::*;
    } else if #[cfg(target_arch = "aarch64")] {
        mod aarch64;
        pub use self::aarch64::*;
    } else {
        // Unknown target_arch
    }
}
