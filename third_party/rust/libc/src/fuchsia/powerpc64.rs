pub type c_char = u8;
pub type wchar_t = i32;
pub type __u64 = ::c_ulong;
pub type nlink_t = u64;
pub type blksize_t = ::c_long;

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

pub const SYS_pivot_root: ::c_long = 203;
pub const SYS_gettid: ::c_long = 207;
pub const SYS_perf_event_open: ::c_long = 319;
pub const SYS_memfd_create: ::c_long = 360;

pub const MAP_32BIT: ::c_int = 0x0040;

pub const SIGSTKSZ: ::size_t = 8192;
pub const MINSIGSTKSZ: ::size_t = 2048;

#[doc(hidden)]
pub const AF_MAX: ::c_int = 42;
#[doc(hidden)]
pub const PF_MAX: ::c_int = AF_MAX;

// Syscall table
pub const SYS_renameat2: ::c_long = 357;
