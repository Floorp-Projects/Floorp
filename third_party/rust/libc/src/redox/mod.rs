pub type int8_t = i8;
pub type int16_t = i16;
pub type int32_t = i32;
pub type int64_t = i64;
pub type uint8_t = u8;
pub type uint16_t = u16;
pub type uint32_t = u32;
pub type uint64_t = u64;

pub type c_schar = i8;
pub type c_uchar = u8;
pub type c_short = i16;
pub type c_ushort = u16;
pub type c_int = i32;
pub type c_uint = u32;
pub type c_float = f32;
pub type c_double = f64;
pub type c_longlong = i64;
pub type c_ulonglong = u64;
pub type intmax_t = i64;
pub type uintmax_t = u64;

pub type size_t = usize;
pub type ptrdiff_t = isize;
pub type intptr_t = isize;
pub type uintptr_t = usize;
pub type ssize_t = isize;

pub type c_char = i8;
pub type c_long = i64;
pub type c_ulong = u64;

pub type wchar_t = i32;
pub type wint_t = u32;
pub type wctype_t = i64;

pub type regoff_t = size_t;
pub type off_t = c_long;
pub type mode_t = c_int;
pub type time_t = c_long;
pub type pid_t = c_int;
pub type id_t = c_uint;
pub type gid_t = c_int;
pub type uid_t = c_int;
pub type dev_t = c_long;
pub type ino_t = c_ulong;
pub type nlink_t = c_ulong;
pub type blksize_t = c_long;
pub type blkcnt_t = c_ulong;

pub type fsblkcnt_t = c_ulong;
pub type fsfilcnt_t = c_ulong;

pub type useconds_t = c_uint;
pub type suseconds_t = c_int;

pub type clock_t = c_long;
pub type clockid_t = c_int;
pub type timer_t = *mut c_void;

pub type nfds_t = c_ulong;

s! {
    pub struct fd_set {
        fds_bits: [::c_ulong; FD_SETSIZE / ULONG_SIZE],
    }

    pub struct pollfd {
        pub fd: ::c_int,
        pub events: ::c_short,
        pub revents: ::c_short,
    }

    pub struct stat {
        pub st_dev: ::dev_t,
        pub st_ino: ::ino_t,
        pub st_nlink: ::nlink_t,
        pub st_mode: ::mode_t,
        pub st_uid: ::uid_t,
        pub st_gid: ::gid_t,
        pub st_rdev: ::dev_t,
        pub st_size: ::off_t,
        pub st_blksize: ::blksize_t,
        pub st_blocks: ::blkcnt_t,

        pub st_atime: ::timespec,
        pub st_mtime: ::timespec,
        pub st_ctime: ::timespec,

        _pad: [c_char; 24],
    }

    pub struct timeval {
        pub tv_sec: time_t,
        pub tv_usec: suseconds_t,
    }

    pub struct timespec {
        pub tv_sec: time_t,
        pub tv_nsec: c_long,
    }
}

pub const INT_MIN: c_int = -2147483648;
pub const INT_MAX: c_int = 2147483647;

pub const STDIN_FILENO: ::c_int = 0;
pub const STDOUT_FILENO: ::c_int = 1;
pub const STDERR_FILENO: ::c_int = 2;

pub const EXIT_FAILURE: ::c_int = 1;
pub const EXIT_SUCCESS: ::c_int = 0;

pub const FD_SETSIZE: usize = 1024;

pub const MAP_SHARED: ::c_int = 1;
pub const MAP_PRIVATE: ::c_int = 2;
pub const MAP_ANONYMOUS: ::c_int = 4;
pub const MAP_ANON: ::c_int = MAP_ANONYMOUS;

pub const MAP_FAILED: *mut ::c_void = !0 as *mut ::c_void;

pub const POLLIN: ::c_short = 0x001;
pub const POLLPRI: ::c_short = 0x002;
pub const POLLOUT: ::c_short = 0x004;
pub const POLLERR: ::c_short = 0x008;
pub const POLLHUP: ::c_short = 0x010;
pub const POLLNVAL: ::c_short = 0x020;

pub const PROT_NONE: ::c_int = 0;
pub const PROT_EXEC: ::c_int = 1;
pub const PROT_WRITE: ::c_int = 2;
pub const PROT_READ: ::c_int = 4;

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

pub const FD_CLOEXEC: ::c_int =   0x0100_0000;

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

pub const SIGHUP:    ::c_int = 1;
pub const SIGINT:    ::c_int = 2;
pub const SIGQUIT:   ::c_int = 3;
pub const SIGILL:    ::c_int = 4;
pub const SIGTRAP:   ::c_int = 5;
pub const SIGABRT:   ::c_int = 6;
pub const SIGBUS:    ::c_int = 7;
pub const SIGFPE:    ::c_int = 8;
pub const SIGKILL:   ::c_int = 9;
pub const SIGUSR1:   ::c_int = 10;
pub const SIGSEGV:   ::c_int = 11;
pub const SIGUSR2:   ::c_int = 12;
pub const SIGPIPE:   ::c_int = 13;
pub const SIGALRM:   ::c_int = 14;
pub const SIGTERM:   ::c_int = 15;
pub const SIGSTKFLT: ::c_int = 16;
pub const SIGCHLD:   ::c_int = 17;
pub const SIGCONT:   ::c_int = 18;
pub const SIGSTOP:   ::c_int = 19;
pub const SIGTSTP:   ::c_int = 20;
pub const SIGTTIN:   ::c_int = 21;
pub const SIGTTOU:   ::c_int = 22;
pub const SIGURG:    ::c_int = 23;
pub const SIGXCPU:   ::c_int = 24;
pub const SIGXFSZ:   ::c_int = 25;
pub const SIGVTALRM: ::c_int = 26;
pub const SIGPROF:   ::c_int = 27;
pub const SIGWINCH:  ::c_int = 28;
pub const SIGIO:     ::c_int = 29;
pub const SIGPWR:    ::c_int = 30;
pub const SIGSYS:    ::c_int = 31;

#[cfg_attr(feature = "extra_traits", derive(Debug))]
pub enum FILE {}
impl ::Copy for FILE {}
impl ::Clone for FILE {
    fn clone(&self) -> FILE { *self }
}
#[cfg_attr(feature = "extra_traits", derive(Debug))]
pub enum fpos_t {} // TODO: fill this out with a struct
impl ::Copy for fpos_t {}
impl ::Clone for fpos_t {
    fn clone(&self) -> fpos_t { *self }
}

// intentionally not public, only used for fd_set
cfg_if! {
    if #[cfg(target_pointer_width = "32")] {
        const ULONG_SIZE: usize = 32;
    } else if #[cfg(target_pointer_width = "64")] {
        const ULONG_SIZE: usize = 64;
    } else {
        // Unknown target_pointer_width
    }
}

extern {
    pub fn isalnum(c: c_int) -> c_int;
    pub fn isalpha(c: c_int) -> c_int;
    pub fn iscntrl(c: c_int) -> c_int;
    pub fn isdigit(c: c_int) -> c_int;
    pub fn isgraph(c: c_int) -> c_int;
    pub fn islower(c: c_int) -> c_int;
    pub fn isprint(c: c_int) -> c_int;
    pub fn ispunct(c: c_int) -> c_int;
    pub fn isspace(c: c_int) -> c_int;
    pub fn isupper(c: c_int) -> c_int;
    pub fn isxdigit(c: c_int) -> c_int;
    pub fn tolower(c: c_int) -> c_int;
    pub fn toupper(c: c_int) -> c_int;
    pub fn fopen(filename: *const c_char, mode: *const c_char) -> *mut FILE;
    pub fn freopen(filename: *const c_char, mode: *const c_char,
                   file: *mut FILE) -> *mut FILE;
    pub fn fflush(file: *mut FILE) -> c_int;
    pub fn fclose(file: *mut FILE) -> c_int;
    pub fn remove(filename: *const c_char) -> c_int;
    pub fn rename(oldname: *const c_char, newname: *const c_char) -> c_int;
    pub fn tmpfile() -> *mut FILE;
    pub fn setvbuf(stream: *mut FILE, buffer: *mut c_char, mode: c_int,
                   size: size_t) -> c_int;
    pub fn setbuf(stream: *mut FILE, buf: *mut c_char);
    pub fn getchar() -> c_int;
    pub fn putchar(c: c_int) -> c_int;
    pub fn fgetc(stream: *mut FILE) -> c_int;
    pub fn fgets(buf: *mut c_char, n: c_int, stream: *mut FILE) -> *mut c_char;
    pub fn fputc(c: c_int, stream: *mut FILE) -> c_int;
    pub fn fputs(s: *const c_char, stream: *mut FILE) -> c_int;
    pub fn puts(s: *const c_char) -> c_int;
    pub fn ungetc(c: c_int, stream: *mut FILE) -> c_int;
    pub fn fread(ptr: *mut c_void, size: size_t, nobj: size_t,
                 stream: *mut FILE) -> size_t;
    pub fn fwrite(ptr: *const c_void, size: size_t, nobj: size_t,
                  stream: *mut FILE) -> size_t;
    pub fn fseek(stream: *mut FILE, offset: c_long, whence: c_int) -> c_int;
    pub fn ftell(stream: *mut FILE) -> c_long;
    pub fn rewind(stream: *mut FILE);
    pub fn fgetpos(stream: *mut FILE, ptr: *mut fpos_t) -> c_int;
    pub fn fsetpos(stream: *mut FILE, ptr: *const fpos_t) -> c_int;
    pub fn feof(stream: *mut FILE) -> c_int;
    pub fn ferror(stream: *mut FILE) -> c_int;
    pub fn perror(s: *const c_char);
    pub fn atoi(s: *const c_char) -> c_int;
    pub fn strtod(s: *const c_char, endp: *mut *mut c_char) -> c_double;
    pub fn strtol(s: *const c_char, endp: *mut *mut c_char,
                  base: c_int) -> c_long;
    pub fn strtoul(s: *const c_char, endp: *mut *mut c_char,
                   base: c_int) -> c_ulong;
    pub fn calloc(nobj: size_t, size: size_t) -> *mut c_void;
    pub fn malloc(size: size_t) -> *mut c_void;
    pub fn realloc(p: *mut c_void, size: size_t) -> *mut c_void;
    pub fn free(p: *mut c_void);
    pub fn abort() -> !;
    pub fn exit(status: c_int) -> !;
    pub fn _exit(status: c_int) -> !;
    pub fn atexit(cb: extern fn()) -> c_int;
    pub fn system(s: *const c_char) -> c_int;
    pub fn getenv(s: *const c_char) -> *mut c_char;

    pub fn strcpy(dst: *mut c_char, src: *const c_char) -> *mut c_char;
    pub fn strncpy(dst: *mut c_char, src: *const c_char,
                   n: size_t) -> *mut c_char;
    pub fn strcat(s: *mut c_char, ct: *const c_char) -> *mut c_char;
    pub fn strncat(s: *mut c_char, ct: *const c_char,
                   n: size_t) -> *mut c_char;
    pub fn strcmp(cs: *const c_char, ct: *const c_char) -> c_int;
    pub fn strncmp(cs: *const c_char, ct: *const c_char,
                   n: size_t) -> c_int;
    pub fn strcoll(cs: *const c_char, ct: *const c_char) -> c_int;
    pub fn strchr(cs: *const c_char, c: c_int) -> *mut c_char;
    pub fn strrchr(cs: *const c_char, c: c_int) -> *mut c_char;
    pub fn strspn(cs: *const c_char, ct: *const c_char) -> size_t;
    pub fn strcspn(cs: *const c_char, ct: *const c_char) -> size_t;
    pub fn strdup(cs: *const c_char) -> *mut c_char;
    pub fn strpbrk(cs: *const c_char, ct: *const c_char) -> *mut c_char;
    pub fn strstr(cs: *const c_char, ct: *const c_char) -> *mut c_char;
    pub fn strcasestr(cs: *const c_char, ct: *const c_char) -> *mut c_char;
    pub fn strcasecmp(s1: *const c_char, s2: *const c_char) -> c_int;
    pub fn strncasecmp(s1: *const c_char, s2: *const c_char,
                       n: size_t) -> c_int;
    pub fn strlen(cs: *const c_char) -> size_t;
    pub fn strnlen(cs: *const c_char, maxlen: size_t) -> size_t;
    pub fn strerror(n: c_int) -> *mut c_char;
    pub fn strtok(s: *mut c_char, t: *const c_char) -> *mut c_char;
    pub fn strxfrm(s: *mut c_char, ct: *const c_char, n: size_t) -> size_t;
    pub fn wcslen(buf: *const wchar_t) -> size_t;
    pub fn wcstombs(dest: *mut c_char, src: *const wchar_t,
                    n: size_t) -> ::size_t;

    pub fn memchr(cx: *const c_void, c: c_int, n: size_t) -> *mut c_void;
    pub fn memcmp(cx: *const c_void, ct: *const c_void, n: size_t) -> c_int;
    pub fn memcpy(dest: *mut c_void, src: *const c_void,
                  n: size_t) -> *mut c_void;
    pub fn memmove(dest: *mut c_void, src: *const c_void,
                   n: size_t) -> *mut c_void;
    pub fn memset(dest: *mut c_void, c: c_int, n: size_t) -> *mut c_void;

    pub fn abs(i: c_int) -> c_int;
    pub fn atof(s: *const c_char) -> c_double;
    pub fn labs(i: c_long) -> c_long;
    pub fn rand() -> c_int;
    pub fn srand(seed: c_uint);

    pub fn chown(path: *const c_char, uid: uid_t, gid: gid_t) -> ::c_int;
    pub fn close(fd: ::c_int) -> ::c_int;
    pub fn fchown(fd: ::c_int, uid: ::uid_t, gid: ::gid_t) -> ::c_int;
    pub fn fcntl(fd: ::c_int, cmd: ::c_int, ...) -> ::c_int;
    pub fn fstat(fd: ::c_int, buf: *mut stat) -> ::c_int;
    pub fn fsync(fd: ::c_int) -> ::c_int;
    pub fn gethostname(name: *mut ::c_char, len: ::size_t) -> ::c_int;
    pub fn getpid() -> pid_t;
    pub fn memalign(align: ::size_t, size: ::size_t) -> *mut ::c_void;
    pub fn mmap(addr: *mut ::c_void,
         len: ::size_t,
         prot: ::c_int,
         flags: ::c_int,
         fd: ::c_int,
         offset: off_t)
         -> *mut ::c_void;
    pub fn mprotect(addr: *mut ::c_void, len: ::size_t, prot: ::c_int)
                    -> ::c_int;
    pub fn munmap(addr: *mut ::c_void, len: ::size_t) -> ::c_int;
    pub fn poll(fds: *mut pollfd, nfds: nfds_t, timeout: ::c_int) -> ::c_int;
    pub fn read(fd: ::c_int, buf: *mut ::c_void, count: ::size_t) -> ::ssize_t;
    pub fn setenv(name: *const c_char, val: *const c_char, overwrite: ::c_int)
                  -> ::c_int;
    pub fn unsetenv(name: *const c_char) -> ::c_int;
    pub fn write(fd: ::c_int, buf: *const ::c_void, count: ::size_t)
                 -> ::ssize_t;
}

#[link(name = "c")]
#[link(name = "m")]
extern {}

pub use self::net::*;

mod net;

cfg_if! {
    if #[cfg(libc_core_cvoid)] {
        pub use ::ffi::c_void;
    } else {
        // Use repr(u8) as LLVM expects `void*` to be the same as `i8*` to help
        // enable more optimization opportunities around it recognizing things
        // like malloc/free.
        #[repr(u8)]
        #[allow(missing_copy_implementations)]
        #[allow(missing_debug_implementations)]
        pub enum c_void {
            // Two dummy variants so the #[repr] attribute can be used.
            #[doc(hidden)]
            __variant1,
            #[doc(hidden)]
            __variant2,
        }
    }
}

cfg_if! {
    if #[cfg(libc_align)] {
        mod align;
        pub use self::align::*;
    } else {
        mod no_align;
        pub use self::no_align::*;
    }
}
