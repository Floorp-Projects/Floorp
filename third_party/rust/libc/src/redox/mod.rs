pub type c_char = i8;
pub type c_long = i64;
pub type c_ulong = u64;

pub type wchar_t = i16;

pub type off_t = c_long;
pub type mode_t = u16;
pub type time_t = i64;
pub type pid_t = usize;
pub type gid_t = usize;
pub type uid_t = usize;

pub type suseconds_t = i64;

s! {
    pub struct timeval {
        pub tv_sec: time_t,
        pub tv_usec: suseconds_t,
    }

    pub struct timespec {
        pub tv_sec: time_t,
        pub tv_nsec: c_long,
    }
}

pub const STDIN_FILENO: ::c_int = 0;
pub const STDOUT_FILENO: ::c_int = 1;
pub const STDERR_FILENO: ::c_int = 2;

pub const EXIT_FAILURE: ::c_int = 1;
pub const EXIT_SUCCESS: ::c_int = 0;

pub const S_ISUID: ::c_int = 0x800;
pub const S_ISGID: ::c_int = 0x400;
pub const S_ISVTX: ::c_int = 0x200;

pub const S_IFIFO: mode_t = 0x1000;
pub const S_IFCHR: mode_t = 0x2000;
pub const S_IFBLK: mode_t = 0x6000;
pub const S_IFDIR: mode_t = 0x4000;
pub const S_IFREG: mode_t = 0x8000;
pub const S_IFLNK: mode_t = 0xA000;
pub const S_IFSOCK: mode_t = 0xC000;
pub const S_IFMT: mode_t = 0xF000;
pub const S_IEXEC: mode_t = 0x40;
pub const S_IWRITE: mode_t = 0x80;
pub const S_IREAD: mode_t = 0x100;
pub const S_IRWXU: mode_t = 0x1C0;
pub const S_IXUSR: mode_t = 0x40;
pub const S_IWUSR: mode_t = 0x80;
pub const S_IRUSR: mode_t = 0x100;
pub const S_IRWXG: mode_t = 0x38;
pub const S_IXGRP: mode_t = 0x8;
pub const S_IWGRP: mode_t = 0x10;
pub const S_IRGRP: mode_t = 0x20;
pub const S_IRWXO: mode_t = 0x7;
pub const S_IXOTH: mode_t = 0x1;
pub const S_IWOTH: mode_t = 0x2;
pub const S_IROTH: mode_t = 0x4;

pub const F_DUPFD: ::c_int = 0;
pub const F_GETFD: ::c_int = 1;
pub const F_SETFD: ::c_int = 2;
pub const F_GETFL: ::c_int = 3;
pub const F_SETFL: ::c_int = 4;

pub const O_RDONLY: ::c_int =     0x0001_0000;
pub const O_WRONLY: ::c_int =     0x0002_0000;
pub const O_RDWR: ::c_int =       0x0003_0000;
pub const O_NONBLOCK: ::c_int =   0x0004_0000;
pub const O_APPEND: ::c_int =     0x0008_0000;
pub const O_SHLOCK: ::c_int =     0x0010_0000;
pub const O_EXLOCK: ::c_int =     0x0020_0000;
pub const O_ASYNC: ::c_int =      0x0040_0000;
pub const O_FSYNC: ::c_int =      0x0080_0000;
pub const O_CLOEXEC: ::c_int =    0x0100_0000;
pub const O_CREAT: ::c_int =      0x0200_0000;
pub const O_TRUNC: ::c_int =      0x0400_0000;
pub const O_EXCL: ::c_int =       0x0800_0000;
pub const O_DIRECTORY: ::c_int =  0x1000_0000;
pub const O_STAT: ::c_int =       0x2000_0000;
pub const O_SYMLINK: ::c_int =    0x4000_0000;
pub const O_NOFOLLOW: ::c_int =   0x8000_0000;
pub const O_ACCMODE: ::c_int =    O_RDONLY | O_WRONLY | O_RDWR;

extern {
    pub fn memalign(align: ::size_t, size: ::size_t) -> *mut ::c_void;
    pub fn read(fd: ::c_int, buf: *mut ::c_void, count: ::size_t)
                -> ::ssize_t;
    pub fn write(fd: ::c_int, buf: *const ::c_void, count: ::size_t)
                 -> ::ssize_t;
    pub fn fcntl(fd: ::c_int, cmd: ::c_int, ...) -> ::c_int;
    pub fn close(fd: ::c_int) -> ::c_int;
}

#[link(name = "c")]
#[link(name = "m")]
extern {}

pub use self::net::*;

mod net;
