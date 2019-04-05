pub use ffi::c_void;

pub type c_char = i8;
pub type c_uchar = u8;
pub type c_int = i32;
pub type c_uint = u32;
pub type c_short = i16;
pub type c_ushort = u16;
pub type c_long = i32;
pub type c_ulong = u32;
pub type c_longlong = i64;
pub type c_ulonglong = u64;
pub type size_t = usize;
pub type ssize_t = isize;
pub type off_t = i64;
pub type pid_t = i32;
pub type int8_t = i8;
pub type uint8_t = u8;
pub type int16_t = i16;
pub type uint16_t = u16;
pub type int32_t = i32;
pub type uint32_t = u32;
pub type int64_t = i64;
pub type uint64_t = u64;
pub type clock_t = c_longlong;
pub type time_t = c_longlong;
pub type c_double = f64;
pub type c_float = f32;

pub type __wasi_advice_t = u8;
pub type __wasi_clockid_t = u32;
pub type __wasi_device_t = u64;
pub type __wasi_dircookie_t = u64;
pub type __wasi_errno_t = u16;
pub type __wasi_eventrwflags_t = u16;
pub type __wasi_eventtype_t = u8;
pub type __wasi_exitcode_t = u32;
pub type __wasi_fd_t = u32;
pub type __wasi_fdflags_t = u16;
pub type __wasi_filedelta_t = i64;
pub type __wasi_filesize_t = u64;
pub type __wasi_filetype_t = u8;
pub type __wasi_fstflags_t = u16;
pub type __wasi_inode_t = u64;
pub type __wasi_linkcount_t = u32;
pub type __wasi_lookupflags_t = u32;
pub type __wasi_oflags_t = u16;
pub type __wasi_riflags_t = u16;
pub type __wasi_rights_t = u64;
pub type __wasi_roflags_t = u16;
pub type __wasi_sdflags_t = u8;
pub type __wasi_siflags_t = u16;
pub type __wasi_signal_t = u8;
pub type __wasi_subclockflags_t = u16;
pub type __wasi_timestamp_t = u64;
pub type __wasi_userdata_t = u64;
pub type __wasi_whence_t = u8;
pub type __wasi_preopentype_t = u8;

#[cfg_attr(feature = "extra_traits", derive(Debug))]
pub enum FILE {}
impl ::Copy for FILE {}
impl ::Clone for FILE {
    fn clone(&self) -> FILE {
        *self
    }
}

s! {
    #[repr(align(8))]
    pub struct fpos_t {
        data: [u8; 16],
    }

    pub struct tm {
        pub tm_sec: c_int,
        pub tm_min: c_int,
        pub tm_hour: c_int,
        pub tm_mday: c_int,
        pub tm_mon: c_int,
        pub tm_year: c_int,
        pub tm_wday: c_int,
        pub tm_yday: c_int,
        pub tm_isdst: c_int,
        pub __tm_gmtoff: c_int,
        pub __tm_zone: *const c_char,
        pub __tm_nsec: c_int,
    }

    pub struct timespec {
        pub tv_sec: time_t,
        pub tv_nsec: c_long,
    }

    pub struct itimerspec {
        pub it_interval: timespec,
        pub it_value: timespec,
    }

    pub struct __wasi_dirent_t {
        pub d_next: __wasi_dircookie_t,
        pub d_ino: __wasi_inode_t,
        pub d_namlen: u32,
        pub d_type: __wasi_filetype_t,
    }

    pub struct __wasi_event_u_fd_readwrite_t {
        pub nbytes: __wasi_filesize_t,
        pub flags: __wasi_eventrwflags_t,
    }

    pub struct __wasi_fdstat_t {
        pub fs_filetype: __wasi_filetype_t,
        pub fs_flags: __wasi_fdflags_t,
        pub fs_rights_base: __wasi_rights_t,
        pub fs_rights_inheriting: __wasi_rights_t,
    }

    pub struct __wasi_filestat_t {
        pub st_dev: __wasi_device_t,
        pub st_ino: __wasi_inode_t,
        pub st_filetype: __wasi_filetype_t,
        pub st_nlink: __wasi_linkcount_t,
        pub st_size: __wasi_filesize_t,
        pub st_atim: __wasi_timestamp_t,
        pub st_mtim: __wasi_timestamp_t,
        pub st_ctim: __wasi_timestamp_t,
    }

    pub struct __wasi_ciovec_t {
        pub buf: *const ::c_void,
        pub buf_len: size_t,
    }

    pub struct __wasi_iovec_t {
        pub buf: *mut ::c_void,
        pub buf_len: size_t,
    }

    pub struct __wasi_subscription_u_clock_t {
        pub identifier: __wasi_userdata_t,
        pub clock_id: __wasi_clockid_t,
        pub timeout: __wasi_timestamp_t,
        pub precision: __wasi_timestamp_t,
        pub flags: __wasi_subclockflags_t,
    }

    pub struct __wasi_subscription_u_fd_readwrite_t {
        pub fd: __wasi_fd_t,
    }

    pub struct __wasi_prestat_u_dir_t {
        pub pr_name_len: size_t,
    }
}

s_no_extra_traits! {
    #[allow(missing_debug_implementations)]
    pub struct __wasi_subscription_t {
        pub userdata: __wasi_userdata_t,
        pub type_: __wasi_eventtype_t,
        pub u: __wasi_subscription_u,
    }

    #[allow(missing_debug_implementations)]
    pub struct __wasi_event_t {
        pub userdata: __wasi_userdata_t,
        pub error: __wasi_errno_t,
        pub type_: __wasi_eventtype_t,
        pub u: __wasi_event_u,
    }

    #[allow(missing_debug_implementations)]
    pub union __wasi_event_u {
        pub fd_readwrite: __wasi_event_u_fd_readwrite_t,
        _bindgen_union_align: [u64; 2],
    }

    #[allow(missing_debug_implementations)]
    pub union __wasi_subscription_u {
        pub clock: __wasi_subscription_u_clock_t,
        pub fd_readwrite:
            __wasi_subscription_u_fd_readwrite_t,
        _bindgen_union_align: [u64; 5],
    }

    #[allow(missing_debug_implementations)]
    pub struct __wasi_prestat_t {
        pub pr_type: __wasi_preopentype_t,
        pub u: __wasi_prestat_u,
    }

    #[allow(missing_debug_implementations)]
    pub union __wasi_prestat_u {
        pub dir: __wasi_prestat_u_dir_t,
    }

}

pub const STDIN_FILENO: c_int = 0;
pub const STDOUT_FILENO: c_int = 1;
pub const STDERR_FILENO: c_int = 2;
pub const SEEK_SET: c_int = 2;
pub const SEEK_CUR: c_int = 0;
pub const SEEK_END: c_int = 1;

pub const __WASI_ADVICE_NORMAL: u8 = 0;
pub const __WASI_ADVICE_SEQUENTIAL: u8 = 1;
pub const __WASI_ADVICE_RANDOM: u8 = 2;
pub const __WASI_ADVICE_WILLNEED: u8 = 3;
pub const __WASI_ADVICE_DONTNEED: u8 = 4;
pub const __WASI_ADVICE_NOREUSE: u8 = 5;
pub const __WASI_CLOCK_REALTIME: u32 = 0;
pub const __WASI_CLOCK_MONOTONIC: u32 = 1;
pub const __WASI_CLOCK_PROCESS_CPUTIME_ID: u32 = 2;
pub const __WASI_CLOCK_THREAD_CPUTIME_ID: u32 = 3;
pub const __WASI_DIRCOOKIE_START: u64 = 0;
pub const __WASI_ESUCCESS: u16 = 0;
pub const __WASI_E2BIG: u16 = 1;
pub const __WASI_EACCES: u16 = 2;
pub const __WASI_EADDRINUSE: u16 = 3;
pub const __WASI_EADDRNOTAVAIL: u16 = 4;
pub const __WASI_EAFNOSUPPORT: u16 = 5;
pub const __WASI_EAGAIN: u16 = 6;
pub const __WASI_EALREADY: u16 = 7;
pub const __WASI_EBADF: u16 = 8;
pub const __WASI_EBADMSG: u16 = 9;
pub const __WASI_EBUSY: u16 = 10;
pub const __WASI_ECANCELED: u16 = 11;
pub const __WASI_ECHILD: u16 = 12;
pub const __WASI_ECONNABORTED: u16 = 13;
pub const __WASI_ECONNREFUSED: u16 = 14;
pub const __WASI_ECONNRESET: u16 = 15;
pub const __WASI_EDEADLK: u16 = 16;
pub const __WASI_EDESTADDRREQ: u16 = 17;
pub const __WASI_EDOM: u16 = 18;
pub const __WASI_EDQUOT: u16 = 19;
pub const __WASI_EEXIST: u16 = 20;
pub const __WASI_EFAULT: u16 = 21;
pub const __WASI_EFBIG: u16 = 22;
pub const __WASI_EHOSTUNREACH: u16 = 23;
pub const __WASI_EIDRM: u16 = 24;
pub const __WASI_EILSEQ: u16 = 25;
pub const __WASI_EINPROGRESS: u16 = 26;
pub const __WASI_EINTR: u16 = 27;
pub const __WASI_EINVAL: u16 = 28;
pub const __WASI_EIO: u16 = 29;
pub const __WASI_EISCONN: u16 = 30;
pub const __WASI_EISDIR: u16 = 31;
pub const __WASI_ELOOP: u16 = 32;
pub const __WASI_EMFILE: u16 = 33;
pub const __WASI_EMLINK: u16 = 34;
pub const __WASI_EMSGSIZE: u16 = 35;
pub const __WASI_EMULTIHOP: u16 = 36;
pub const __WASI_ENAMETOOLONG: u16 = 37;
pub const __WASI_ENETDOWN: u16 = 38;
pub const __WASI_ENETRESET: u16 = 39;
pub const __WASI_ENETUNREACH: u16 = 40;
pub const __WASI_ENFILE: u16 = 41;
pub const __WASI_ENOBUFS: u16 = 42;
pub const __WASI_ENODEV: u16 = 43;
pub const __WASI_ENOENT: u16 = 44;
pub const __WASI_ENOEXEC: u16 = 45;
pub const __WASI_ENOLCK: u16 = 46;
pub const __WASI_ENOLINK: u16 = 47;
pub const __WASI_ENOMEM: u16 = 48;
pub const __WASI_ENOMSG: u16 = 49;
pub const __WASI_ENOPROTOOPT: u16 = 50;
pub const __WASI_ENOSPC: u16 = 51;
pub const __WASI_ENOSYS: u16 = 52;
pub const __WASI_ENOTCONN: u16 = 53;
pub const __WASI_ENOTDIR: u16 = 54;
pub const __WASI_ENOTEMPTY: u16 = 55;
pub const __WASI_ENOTRECOVERABLE: u16 = 56;
pub const __WASI_ENOTSOCK: u16 = 57;
pub const __WASI_ENOTSUP: u16 = 58;
pub const __WASI_ENOTTY: u16 = 59;
pub const __WASI_ENXIO: u16 = 60;
pub const __WASI_EOVERFLOW: u16 = 61;
pub const __WASI_EOWNERDEAD: u16 = 62;
pub const __WASI_EPERM: u16 = 63;
pub const __WASI_EPIPE: u16 = 64;
pub const __WASI_EPROTO: u16 = 65;
pub const __WASI_EPROTONOSUPPORT: u16 = 66;
pub const __WASI_EPROTOTYPE: u16 = 67;
pub const __WASI_ERANGE: u16 = 68;
pub const __WASI_EROFS: u16 = 69;
pub const __WASI_ESPIPE: u16 = 70;
pub const __WASI_ESRCH: u16 = 71;
pub const __WASI_ESTALE: u16 = 72;
pub const __WASI_ETIMEDOUT: u16 = 73;
pub const __WASI_ETXTBSY: u16 = 74;
pub const __WASI_EXDEV: u16 = 75;
pub const __WASI_ENOTCAPABLE: u16 = 76;
pub const __WASI_EVENT_FD_READWRITE_HANGUP: u16 = 0x0001;
pub const __WASI_EVENTTYPE_CLOCK: u8 = 0;
pub const __WASI_EVENTTYPE_FD_READ: u8 = 1;
pub const __WASI_EVENTTYPE_FD_WRITE: u8 = 2;
pub const __WASI_FDFLAG_APPEND: u16 = 0x0001;
pub const __WASI_FDFLAG_DSYNC: u16 = 0x0002;
pub const __WASI_FDFLAG_NONBLOCK: u16 = 0x0004;
pub const __WASI_FDFLAG_RSYNC: u16 = 0x0008;
pub const __WASI_FDFLAG_SYNC: u16 = 0x0010;
pub const __WASI_FILETYPE_UNKNOWN: u8 = 0;
pub const __WASI_FILETYPE_BLOCK_DEVICE: u8 = 1;
pub const __WASI_FILETYPE_CHARACTER_DEVICE: u8 = 2;
pub const __WASI_FILETYPE_DIRECTORY: u8 = 3;
pub const __WASI_FILETYPE_REGULAR_FILE: u8 = 4;
pub const __WASI_FILETYPE_SOCKET_DGRAM: u8 = 5;
pub const __WASI_FILETYPE_SOCKET_STREAM: u8 = 6;
pub const __WASI_FILETYPE_SYMBOLIC_LINK: u8 = 7;
pub const __WASI_FILESTAT_SET_ATIM: u16 = 0x0001;
pub const __WASI_FILESTAT_SET_ATIM_NOW: u16 = 0x0002;
pub const __WASI_FILESTAT_SET_MTIM: u16 = 0x0004;
pub const __WASI_FILESTAT_SET_MTIM_NOW: u16 = 0x0008;
pub const __WASI_LOOKUP_SYMLINK_FOLLOW: u32 = 0x00000001;
pub const __WASI_O_CREAT: u16 = 0x0001;
pub const __WASI_O_DIRECTORY: u16 = 0x0002;
pub const __WASI_O_EXCL: u16 = 0x0004;
pub const __WASI_O_TRUNC: u16 = 0x0008;
pub const __WASI_PREOPENTYPE_DIR: u8 = 0;
pub const __WASI_SOCK_RECV_PEEK: u16 = 0x0001;
pub const __WASI_SOCK_RECV_WAITALL: u16 = 0x0002;
pub const __WASI_RIGHT_FD_DATASYNC: u64 = 0x0000000000000001;
pub const __WASI_RIGHT_FD_READ: u64 = 0x0000000000000002;
pub const __WASI_RIGHT_FD_SEEK: u64 = 0x0000000000000004;
pub const __WASI_RIGHT_FD_FDSTAT_SET_FLAGS: u64 = 0x0000000000000008;
pub const __WASI_RIGHT_FD_SYNC: u64 = 0x0000000000000010;
pub const __WASI_RIGHT_FD_TELL: u64 = 0x0000000000000020;
pub const __WASI_RIGHT_FD_WRITE: u64 = 0x0000000000000040;
pub const __WASI_RIGHT_FD_ADVISE: u64 = 0x0000000000000080;
pub const __WASI_RIGHT_FD_ALLOCATE: u64 = 0x0000000000000100;
pub const __WASI_RIGHT_PATH_CREATE_DIRECTORY: u64 = 0x0000000000000200;
pub const __WASI_RIGHT_PATH_CREATE_FILE: u64 = 0x0000000000000400;
pub const __WASI_RIGHT_PATH_LINK_SOURCE: u64 = 0x0000000000000800;
pub const __WASI_RIGHT_PATH_LINK_TARGET: u64 = 0x0000000000001000;
pub const __WASI_RIGHT_PATH_OPEN: u64 = 0x0000000000002000;
pub const __WASI_RIGHT_FD_READDIR: u64 = 0x0000000000004000;
pub const __WASI_RIGHT_PATH_READLINK: u64 = 0x0000000000008000;
pub const __WASI_RIGHT_PATH_RENAME_SOURCE: u64 = 0x0000000000010000;
pub const __WASI_RIGHT_PATH_RENAME_TARGET: u64 = 0x0000000000020000;
pub const __WASI_RIGHT_PATH_FILESTAT_GET: u64 = 0x0000000000040000;
pub const __WASI_RIGHT_PATH_FILESTAT_SET_SIZE: u64 = 0x0000000000080000;
pub const __WASI_RIGHT_PATH_FILESTAT_SET_TIMES: u64 = 0x0000000000100000;
pub const __WASI_RIGHT_FD_FILESTAT_GET: u64 = 0x0000000000200000;
pub const __WASI_RIGHT_FD_FILESTAT_SET_SIZE: u64 = 0x0000000000400000;
pub const __WASI_RIGHT_FD_FILESTAT_SET_TIMES: u64 = 0x0000000000800000;
pub const __WASI_RIGHT_PATH_SYMLINK: u64 = 0x0000000001000000;
pub const __WASI_RIGHT_PATH_REMOVE_DIRECTORY: u64 = 0x0000000002000000;
pub const __WASI_RIGHT_PATH_UNLINK_FILE: u64 = 0x0000000004000000;
pub const __WASI_RIGHT_POLL_FD_READWRITE: u64 = 0x0000000008000000;
pub const __WASI_RIGHT_SOCK_SHUTDOWN: u64 = 0x0000000010000000;
pub const __WASI_SOCK_RECV_DATA_TRUNCATED: u16 = 0x0001;
pub const __WASI_SHUT_RD: u8 = 0x01;
pub const __WASI_SHUT_WR: u8 = 0x02;
pub const __WASI_SIGHUP: u8 = 1;
pub const __WASI_SIGINT: u8 = 2;
pub const __WASI_SIGQUIT: u8 = 3;
pub const __WASI_SIGILL: u8 = 4;
pub const __WASI_SIGTRAP: u8 = 5;
pub const __WASI_SIGABRT: u8 = 6;
pub const __WASI_SIGBUS: u8 = 7;
pub const __WASI_SIGFPE: u8 = 8;
pub const __WASI_SIGKILL: u8 = 9;
pub const __WASI_SIGUSR1: u8 = 10;
pub const __WASI_SIGSEGV: u8 = 11;
pub const __WASI_SIGUSR2: u8 = 12;
pub const __WASI_SIGPIPE: u8 = 13;
pub const __WASI_SIGALRM: u8 = 14;
pub const __WASI_SIGTERM: u8 = 15;
pub const __WASI_SIGCHLD: u8 = 16;
pub const __WASI_SIGCONT: u8 = 17;
pub const __WASI_SIGSTOP: u8 = 18;
pub const __WASI_SIGTSTP: u8 = 19;
pub const __WASI_SIGTTIN: u8 = 20;
pub const __WASI_SIGTTOU: u8 = 21;
pub const __WASI_SIGURG: u8 = 22;
pub const __WASI_SIGXCPU: u8 = 23;
pub const __WASI_SIGXFSZ: u8 = 24;
pub const __WASI_SIGVTALRM: u8 = 25;
pub const __WASI_SIGPROF: u8 = 26;
pub const __WASI_SIGWINCH: u8 = 27;
pub const __WASI_SIGPOLL: u8 = 28;
pub const __WASI_SIGPWR: u8 = 29;
pub const __WASI_SIGSYS: u8 = 30;
pub const __WASI_SUBSCRIPTION_CLOCK_ABSTIME: u16 = 0x0001;
pub const __WASI_WHENCE_CUR: u8 = 0;
pub const __WASI_WHENCE_END: u8 = 1;
pub const __WASI_WHENCE_SET: u8 = 2;

#[cfg_attr(feature = "rustc-dep-of-std",
           link(name = "c", kind = "static",
                cfg(target_feature = "crt-static")))]
#[cfg_attr(feature = "rustc-dep-of-std",
           link(name = "c", cfg(not(target_feature = "crt-static"))))]
extern {
    pub fn _Exit(code: c_int) -> !;
    pub fn _exit(code: c_int) -> !;
    pub fn abort() -> !;
    pub fn aligned_alloc(a: size_t, b: size_t) -> *mut c_void;
    pub fn calloc(amt: size_t, amt2: size_t) -> *mut c_void;
    pub fn exit(code: c_int) -> !;
    pub fn free(ptr: *mut c_void);
    pub fn getenv(s: *const c_char) -> *mut c_char;
    pub fn malloc(amt: size_t) -> *mut c_void;
    pub fn malloc_usable_size(ptr: *mut c_void) -> size_t;
    pub fn rand() -> c_int;
    pub fn read(fd: c_int, ptr: *mut c_void, size: size_t) -> ssize_t;
    pub fn realloc(ptr: *mut c_void, amt: size_t) -> *mut c_void;
    pub fn setenv(k: *const c_char, v: *const c_char, a: c_int) -> c_int;
    pub fn unsetenv(k: *const c_char) -> c_int;
    pub fn write(fd: c_int, ptr: *const c_void, size: size_t) -> ssize_t;
    pub static mut environ: *mut *mut c_char;
    pub fn fopen(a: *const c_char, b: *const c_char) -> *mut FILE;
    pub fn freopen(
        a: *const c_char,
        b: *const c_char,
        f: *mut FILE,
    ) -> *mut FILE;
    pub fn fclose(f: *mut FILE) -> c_int;
    pub fn remove(a: *const c_char) -> c_int;
    pub fn rename(a: *const c_char, b: *const c_char) -> c_int;
    pub fn feof(f: *mut FILE) -> c_int;
    pub fn ferror(f: *mut FILE) -> c_int;
    pub fn fflush(f: *mut FILE) -> c_int;
    pub fn clearerr(f: *mut FILE);
    pub fn fseek(f: *mut FILE, b: c_long, c: c_int) -> c_int;
    pub fn ftell(f: *mut FILE) -> c_long;
    pub fn rewind(f: *mut FILE);
    pub fn fgetpos(f: *mut FILE, pos: *mut fpos_t) -> c_int;
    pub fn fsetpos(f: *mut FILE, pos: *const fpos_t) -> c_int;
    pub fn fread(
        buf: *mut c_void,
        a: size_t,
        b: size_t,
        f: *mut FILE,
    ) -> size_t;
    pub fn fwrite(
        buf: *const c_void,
        a: size_t,
        b: size_t,
        f: *mut FILE,
    ) -> size_t;
    pub fn fgetc(f: *mut FILE) -> c_int;
    pub fn getc(f: *mut FILE) -> c_int;
    pub fn getchar() -> c_int;
    pub fn ungetc(a: c_int, f: *mut FILE) -> c_int;
    pub fn fputc(a: c_int, f: *mut FILE) -> c_int;
    pub fn putc(a: c_int, f: *mut FILE) -> c_int;
    pub fn putchar(a: c_int) -> c_int;
    pub fn fputs(a: *const c_char, f: *mut FILE) -> c_int;
    pub fn puts(a: *const c_char) -> c_int;
    pub fn perror(a: *const c_char);
    pub fn srand(a: c_uint);
    pub fn atexit(a: extern fn()) -> c_int;
    pub fn at_quick_exit(a: extern fn()) -> c_int;
    pub fn quick_exit(a: c_int) -> !;
    pub fn posix_memalign(a: *mut *mut c_void, b: size_t, c: size_t) -> c_int;
    pub fn rand_r(a: *mut c_uint) -> c_int;
    pub fn random() -> c_long;
    pub fn srandom(a: c_uint);
    pub fn putenv(a: *mut c_char) -> c_int;
    pub fn clock() -> clock_t;
    pub fn time(a: *mut time_t) -> time_t;
    pub fn difftime(a: time_t, b: time_t) -> c_double;
    pub fn mktime(a: *mut tm) -> time_t;
    pub fn strftime(
        a: *mut c_char,
        b: size_t,
        c: *const c_char,
        d: *const tm,
    ) -> size_t;
    pub fn gmtime(a: *const time_t) -> *mut tm;
    pub fn gmtime_r(a: *const time_t, b: *mut tm) -> *mut tm;
    pub fn localtime_r(a: *const time_t, b: *mut tm) -> *mut tm;
    pub fn asctime_r(a: *const tm, b: *mut c_char) -> *mut c_char;
    pub fn ctime_r(a: *const time_t, b: *mut c_char) -> *mut c_char;

    pub fn nanosleep(a: *const timespec, b: *mut timespec) -> c_int;
    // pub fn clock_getres(a: clockid_t, b: *mut timespec) -> c_int;
    // pub fn clock_gettime(a: clockid_t, b: *mut timespec) -> c_int;
    // pub fn clock_nanosleep(
    //     a: clockid_t,
    //     a2: c_int,
    //     b: *const timespec,
    //     c: *mut timespec,
    // ) -> c_int;

    pub fn __wasilibc_register_preopened_fd(
        fd: c_int,
        path: *const c_char,
    ) -> c_int;
    pub fn __wasilibc_fd_renumber(fd: c_int, newfd: c_int) -> c_int;
    pub fn __wasilibc_rmfileat(fd: c_int, path: *const c_char) -> c_int;
    pub fn __wasilibc_rmdirat(fd: c_int, path: *const c_char) -> c_int;
    pub fn __wasilibc_init_preopen();

    pub fn arc4random() -> u32;
    pub fn arc4random_buf(a: *mut c_void, b: size_t);
    pub fn arc4random_uniform(a: u32) -> u32;
}

#[link(wasm_import_module = "wasi_unstable")]
extern {
    #[link_name = "clock_res_get"]
    pub fn __wasi_clock_res_get(
        clock_id: __wasi_clockid_t,
        resolution: *mut __wasi_timestamp_t,
    ) -> __wasi_errno_t;
    #[link_name = "clock_time_get"]
    pub fn __wasi_clock_time_get(
        clock_id: __wasi_clockid_t,
        precision: __wasi_timestamp_t,
        time: *mut __wasi_timestamp_t,
    ) -> __wasi_errno_t;
    #[link_name = "fd_close"]
    pub fn __wasi_fd_close(fd: __wasi_fd_t) -> __wasi_errno_t;
    #[link_name = "fd_datasync"]
    pub fn __wasi_fd_datasync(fd: __wasi_fd_t) -> __wasi_errno_t;
    #[link_name = "fd_pread"]
    pub fn __wasi_fd_pread(
        fd: __wasi_fd_t,
        iovs: *const __wasi_iovec_t,
        iovs_len: size_t,
        offset: __wasi_filesize_t,
        nread: *mut size_t,
    ) -> __wasi_errno_t;
    #[link_name = "fd_pwrite"]
    pub fn __wasi_fd_pwrite(
        fd: __wasi_fd_t,
        iovs: *const __wasi_ciovec_t,
        iovs_len: size_t,
        offset: __wasi_filesize_t,
        nwritten: *mut size_t,
    ) -> __wasi_errno_t;
    #[link_name = "fd_read"]
    pub fn __wasi_fd_read(
        fd: __wasi_fd_t,
        iovs: *const __wasi_iovec_t,
        iovs_len: size_t,
        nread: *mut size_t,
    ) -> __wasi_errno_t;
    #[link_name = "fd_renumber"]
    pub fn __wasi_fd_renumber(
        from: __wasi_fd_t,
        to: __wasi_fd_t,
    ) -> __wasi_errno_t;
    #[link_name = "fd_seek"]
    pub fn __wasi_fd_seek(
        fd: __wasi_fd_t,
        offset: __wasi_filedelta_t,
        whence: __wasi_whence_t,
        newoffset: *mut __wasi_filesize_t,
    ) -> __wasi_errno_t;
    #[link_name = "fd_tell"]
    pub fn __wasi_fd_tell(
        fd: __wasi_fd_t,
        newoffset: *mut __wasi_filesize_t,
    ) -> __wasi_errno_t;
    #[link_name = "fd_fdstat_get"]
    pub fn __wasi_fd_fdstat_get(
        fd: __wasi_fd_t,
        buf: *mut __wasi_fdstat_t,
    ) -> __wasi_errno_t;
    #[link_name = "fd_fdstat_set_flags"]
    pub fn __wasi_fd_fdstat_set_flags(
        fd: __wasi_fd_t,
        flags: __wasi_fdflags_t,
    ) -> __wasi_errno_t;
    #[link_name = "fd_fdstat_set_rights"]
    pub fn __wasi_fd_fdstat_set_rights(
        fd: __wasi_fd_t,
        fs_rights_base: __wasi_rights_t,
        fs_rights_inheriting: __wasi_rights_t,
    ) -> __wasi_errno_t;
    #[link_name = "fd_sync"]
    pub fn __wasi_fd_sync(fd: __wasi_fd_t) -> __wasi_errno_t;
    #[link_name = "fd_write"]
    pub fn __wasi_fd_write(
        fd: __wasi_fd_t,
        iovs: *const __wasi_ciovec_t,
        iovs_len: size_t,
        nwritten: *mut size_t,
    ) -> __wasi_errno_t;
    #[link_name = "fd_advise"]
    pub fn __wasi_fd_advise(
        fd: __wasi_fd_t,
        offset: __wasi_filesize_t,
        len: __wasi_filesize_t,
        advice: __wasi_advice_t,
    ) -> __wasi_errno_t;
    #[link_name = "fd_allocate"]
    pub fn __wasi_fd_allocate(
        fd: __wasi_fd_t,
        offset: __wasi_filesize_t,
        len: __wasi_filesize_t,
    ) -> __wasi_errno_t;
    #[link_name = "path_create_directory"]
    pub fn __wasi_path_create_directory(
        fd: __wasi_fd_t,
        path: *const ::c_char,
        path_len: size_t,
    ) -> __wasi_errno_t;
    #[link_name = "path_link"]
    pub fn __wasi_path_link(
        old_fd: __wasi_fd_t,
        old_flags: __wasi_lookupflags_t,
        old_path: *const ::c_char,
        old_path_len: size_t,
        new_fd: __wasi_fd_t,
        new_path: *const ::c_char,
        new_path_len: size_t,
    ) -> __wasi_errno_t;
    #[link_name = "path_open"]
    pub fn __wasi_path_open(
        dirfd: __wasi_fd_t,
        dirflags: __wasi_lookupflags_t,
        path: *const ::c_char,
        path_len: size_t,
        oflags: __wasi_oflags_t,
        fs_rights_base: __wasi_rights_t,
        fs_rights_inheriting: __wasi_rights_t,
        fs_flags: __wasi_fdflags_t,
        fd: *mut __wasi_fd_t,
    ) -> __wasi_errno_t;
    #[link_name = "fd_readdir"]
    pub fn __wasi_fd_readdir(
        fd: __wasi_fd_t,
        buf: *mut ::c_void,
        buf_len: size_t,
        cookie: __wasi_dircookie_t,
        bufused: *mut size_t,
    ) -> __wasi_errno_t;
    #[link_name = "path_readlink"]
    pub fn __wasi_path_readlink(
        fd: __wasi_fd_t,
        path: *const ::c_char,
        path_len: size_t,
        buf: *mut ::c_char,
        buf_len: size_t,
        bufused: *mut size_t,
    ) -> __wasi_errno_t;
    #[link_name = "path_rename"]
    pub fn __wasi_path_rename(
        old_fd: __wasi_fd_t,
        old_path: *const ::c_char,
        old_path_len: size_t,
        new_fd: __wasi_fd_t,
        new_path: *const ::c_char,
        new_path_len: size_t,
    ) -> __wasi_errno_t;
    #[link_name = "fd_filestat_get"]
    pub fn __wasi_fd_filestat_get(
        fd: __wasi_fd_t,
        buf: *mut __wasi_filestat_t,
    ) -> __wasi_errno_t;
    #[link_name = "fd_filestat_set_times"]
    pub fn __wasi_fd_filestat_set_times(
        fd: __wasi_fd_t,
        st_atim: __wasi_timestamp_t,
        st_mtim: __wasi_timestamp_t,
        fstflags: __wasi_fstflags_t,
    ) -> __wasi_errno_t;
    #[link_name = "fd_filestat_set_size"]
    pub fn __wasi_fd_filestat_set_size(
        fd: __wasi_fd_t,
        st_size: __wasi_filesize_t,
    ) -> __wasi_errno_t;
    #[link_name = "path_filestat_get"]
    pub fn __wasi_path_filestat_get(
        fd: __wasi_fd_t,
        flags: __wasi_lookupflags_t,
        path: *const ::c_char,
        path_len: size_t,
        buf: *mut __wasi_filestat_t,
    ) -> __wasi_errno_t;
    #[link_name = "path_filestat_set_times"]
    pub fn __wasi_path_filestat_set_times(
        fd: __wasi_fd_t,
        flags: __wasi_lookupflags_t,
        path: *const ::c_char,
        path_len: size_t,
        st_atim: __wasi_timestamp_t,
        st_mtim: __wasi_timestamp_t,
        fstflags: __wasi_fstflags_t,
    ) -> __wasi_errno_t;
    #[link_name = "path_symlink"]
    pub fn __wasi_path_symlink(
        old_path: *const ::c_char,
        old_path_len: size_t,
        fd: __wasi_fd_t,
        new_path: *const ::c_char,
        new_path_len: size_t,
    ) -> __wasi_errno_t;
    #[link_name = "path_unlink_file"]
    pub fn __wasi_path_unlink_file(
        fd: __wasi_fd_t,
        path: *const ::c_char,
        path_len: size_t,
    ) -> __wasi_errno_t;
    #[link_name = "path_remove_directory"]
    pub fn __wasi_path_remove_directory(
        fd: __wasi_fd_t,
        path: *const ::c_char,
        path_len: size_t,
    ) -> __wasi_errno_t;
    #[link_name = "poll_oneoff"]
    pub fn __wasi_poll_oneoff(
        in_: *const __wasi_subscription_t,
        out: *mut __wasi_event_t,
        nsubscriptions: size_t,
        nevents: *mut size_t,
    ) -> __wasi_errno_t;
    #[link_name = "proc_exit"]
    pub fn __wasi_proc_exit(rval: __wasi_exitcode_t);
    #[link_name = "proc_raise"]
    pub fn __wasi_proc_raise(sig: __wasi_signal_t) -> __wasi_errno_t;
    #[link_name = "random_get"]
    pub fn __wasi_random_get(
        buf: *mut ::c_void,
        buf_len: size_t,
    ) -> __wasi_errno_t;
    #[link_name = "sock_recv"]
    pub fn __wasi_sock_recv(
        sock: __wasi_fd_t,
        ri_data: *const __wasi_iovec_t,
        ri_data_len: size_t,
        ri_flags: __wasi_riflags_t,
        ro_datalen: *mut size_t,
        ro_flags: *mut __wasi_roflags_t,
    ) -> __wasi_errno_t;
    #[link_name = "sock_send"]
    pub fn __wasi_sock_send(
        sock: __wasi_fd_t,
        si_data: *const __wasi_ciovec_t,
        si_data_len: size_t,
        si_flags: __wasi_siflags_t,
        so_datalen: *mut size_t,
    ) -> __wasi_errno_t;
    #[link_name = "sock_shutdown"]
    pub fn __wasi_sock_shutdown(
        sock: __wasi_fd_t,
        how: __wasi_sdflags_t,
    ) -> __wasi_errno_t;
    #[link_name = "sched_yield"]
    pub fn __wasi_sched_yield() -> __wasi_errno_t;
    #[link_name = "args_get"]
    pub fn __wasi_args_get(
        argv: *mut *mut c_char,
        argv_buf: *mut c_char,
    ) -> __wasi_errno_t;
    #[link_name = "args_sizes_get"]
    pub fn __wasi_args_sizes_get(
        argc: *mut size_t,
        argv_buf_size: *mut size_t,
    ) -> __wasi_errno_t;
    #[link_name = "environ_get"]
    pub fn __wasi_environ_get(
        environ: *mut *mut c_char,
        environ_buf: *mut c_char,
    ) -> __wasi_errno_t;
    #[link_name = "environ_sizes_get"]
    pub fn __wasi_environ_sizes_get(
        environ_count: *mut size_t,
        environ_buf_size: *mut size_t,
    ) -> __wasi_errno_t;
    #[link_name = "fd_prestat_get"]
    pub fn __wasi_fd_prestat_get(
        fd: __wasi_fd_t,
        buf: *mut __wasi_prestat_t,
    ) -> __wasi_errno_t;
    #[link_name = "fd_prestat_dir_name"]
    pub fn __wasi_fd_prestat_dir_name(
        fd: __wasi_fd_t,
        path: *mut c_char,
        path_len: size_t,
    ) -> __wasi_errno_t;
}
