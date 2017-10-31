pub type c_char = u8;
pub type wchar_t = u32;

pub const O_DIRECT: ::c_int = 0x10000;
pub const O_DIRECTORY: ::c_int = 0x4000;
pub const O_NOFOLLOW: ::c_int = 0x8000;
pub const O_LARGEFILE: ::c_int = 0o400000;

pub const SYS_pivot_root: ::c_long = 218;
pub const SYS_gettid: ::c_long = 224;
pub const SYS_perf_event_open: ::c_long = 364;
pub const SYS_memfd_create: ::c_long = 385;
