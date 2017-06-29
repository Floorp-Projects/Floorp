pub type c_char = i8;
pub type wchar_t = i32;

s! {
    pub struct stat {
        pub st_dev: ::dev_t,
        pub st_ino: ::ino_t,
        pub st_nlink: ::c_ulong,
        pub st_mode: ::c_uint,
        pub st_uid: ::uid_t,
        pub st_gid: ::gid_t,
        pub st_rdev: ::dev_t,
        pub st_size: ::off64_t,
        pub st_blksize: ::c_long,
        pub st_blocks: ::c_long,
        pub st_atime: ::c_ulong,
        pub st_atime_nsec: ::c_ulong,
        pub st_mtime: ::c_ulong,
        pub st_mtime_nsec: ::c_ulong,
        pub st_ctime: ::c_ulong,
        pub st_ctime_nsec: ::c_ulong,
        __unused: [::c_long; 3],
    }

    pub struct stat64 {
        pub st_dev: ::dev_t,
        pub st_ino: ::ino_t,
        pub st_nlink: ::c_ulong,
        pub st_mode: ::c_uint,
        pub st_uid: ::uid_t,
        pub st_gid: ::gid_t,
        pub st_rdev: ::dev_t,
        pub st_size: ::off64_t,
        pub st_blksize: ::c_long,
        pub st_blocks: ::c_long,
        pub st_atime: ::c_ulong,
        pub st_atime_nsec: ::c_ulong,
        pub st_mtime: ::c_ulong,
        pub st_mtime_nsec: ::c_ulong,
        pub st_ctime: ::c_ulong,
        pub st_ctime_nsec: ::c_ulong,
        __unused: [::c_long; 3],
    }
}

pub const O_DIRECT: ::c_int = 0x4000;
pub const O_DIRECTORY: ::c_int = 0x10000;
pub const O_NOFOLLOW: ::c_int = 0x20000;

pub const SYS_gettid: ::c_long = 186;
