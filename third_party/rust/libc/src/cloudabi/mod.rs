pub type in_addr_t = u32;
pub type in_port_t = u16;
pub type pthread_key_t = usize;
pub type pthread_t = usize;
pub type sa_family_t = u8;
pub type socklen_t = usize;
pub type time_t = i64;

s! {
    pub struct addrinfo {
        pub ai_flags: ::c_int,
        pub ai_family: ::c_int,
        pub ai_socktype: ::c_int,
        pub ai_protocol: ::c_int,
        pub ai_addrlen: ::socklen_t,
        pub ai_addr: *mut ::sockaddr,
        pub ai_canonname: *mut ::c_char,
        pub ai_next: *mut addrinfo,
    }

    pub struct in_addr {
        pub s_addr: in_addr_t,
    }

    pub struct in6_addr {
        pub s6_addr: [u8; 16],
    }

    pub struct pthread_attr_t {
        __detachstate: ::c_int,
        __stacksize: usize,
    }

    pub struct sockaddr {
        pub sa_family: sa_family_t,
        pub sa_data: [::c_char; 0],
    }

    pub struct sockaddr_in {
        pub sin_family: ::sa_family_t,
        pub sin_port: ::in_port_t,
        pub sin_addr: ::in_addr,
    }

    pub struct sockaddr_in6 {
        pub sin6_family: sa_family_t,
        pub sin6_port: ::in_port_t,
        pub sin6_flowinfo: u32,
        pub sin6_addr: ::in6_addr,
        pub sin6_scope_id: u32,
    }

    pub struct sockaddr_storage {
        pub ss_family: ::sa_family_t,
        __ss_data: [u8; 32],
    }
}

pub const _SC_NPROCESSORS_ONLN: ::c_int = 52;
pub const _SC_PAGESIZE: ::c_int = 54;

pub const AF_INET: ::c_int = 1;
pub const AF_INET6: ::c_int = 2;

pub const EACCES: ::c_int = 2;
pub const EADDRINUSE: ::c_int = 3;
pub const EADDRNOTAVAIL: ::c_int = 4;
pub const EAGAIN: ::c_int = 6;
pub const ECONNABORTED: ::c_int = 13;
pub const ECONNREFUSED: ::c_int = 14;
pub const ECONNRESET: ::c_int = 15;
pub const EEXIST: ::c_int = 20;
pub const EINTR: ::c_int = 27;
pub const EINVAL: ::c_int = 28;
pub const ENOENT: ::c_int = 44;
pub const ENOTCONN: ::c_int = 53;
pub const EPERM: ::c_int = 63;
pub const EPIPE: ::c_int = 64;
pub const ETIMEDOUT: ::c_int = 73;
pub const EWOULDBLOCK: ::c_int = EAGAIN;

pub const EAI_SYSTEM: ::c_int = 9;

pub const EXIT_FAILURE: ::c_int = 1;
pub const EXIT_SUCCESS: ::c_int = 0;

pub const PTHREAD_STACK_MIN: ::size_t = 1024;

pub const SOCK_DGRAM: ::c_int = 128;
pub const SOCK_STREAM: ::c_int = 130;

extern {
    pub fn arc4random_buf(buf: *const ::c_void, len: ::size_t);
    pub fn freeaddrinfo(res: *mut addrinfo);
    pub fn gai_strerror(errcode: ::c_int) -> *const ::c_char;
    pub fn getaddrinfo(
        node: *const c_char,
        service: *const c_char,
        hints: *const addrinfo,
        res: *mut *mut addrinfo,
    ) -> ::c_int;
    pub fn getsockopt(
        sockfd: ::c_int,
        level: ::c_int,
        optname: ::c_int,
        optval: *mut ::c_void,
        optlen: *mut ::socklen_t,
    ) -> ::c_int;
    pub fn posix_memalign(
        memptr: *mut *mut ::c_void,
        align: ::size_t,
        size: ::size_t,
    ) -> ::c_int;
    pub fn pthread_attr_destroy(attr: *mut ::pthread_attr_t) -> ::c_int;
    pub fn pthread_attr_init(attr: *mut ::pthread_attr_t) -> ::c_int;
    pub fn pthread_attr_setstacksize(
        attr: *mut ::pthread_attr_t,
        stack_size: ::size_t,
    ) -> ::c_int;
    pub fn pthread_create(
        native: *mut ::pthread_t,
        attr: *const ::pthread_attr_t,
        f: extern fn(*mut ::c_void) -> *mut ::c_void,
        value: *mut ::c_void,
    ) -> ::c_int;
    pub fn pthread_detach(thread: ::pthread_t) -> ::c_int;
    pub fn pthread_getspecific(key: pthread_key_t) -> *mut ::c_void;
    pub fn pthread_join(
        native: ::pthread_t,
        value: *mut *mut ::c_void,
    ) -> ::c_int;
    pub fn pthread_key_create(
        key: *mut pthread_key_t,
        dtor: Option<unsafe extern fn(*mut ::c_void)>,
    ) -> ::c_int;
    pub fn pthread_key_delete(key: pthread_key_t) -> ::c_int;
    pub fn pthread_setspecific(
        key: pthread_key_t,
        value: *const ::c_void,
    ) -> ::c_int;
    pub fn send(
        socket: ::c_int,
        buf: *const ::c_void,
        len: ::size_t,
        flags: ::c_int,
    ) -> ::ssize_t;
    pub fn sysconf(name: ::c_int) -> ::c_long;
}

cfg_if! {
    if #[cfg(target_arch = "aarch64")] {
        mod aarch64;
        pub use self::aarch64::*;
    } else if #[cfg(any(target_arch = "arm"))] {
        mod arm;
        pub use self::arm::*;
    } else if #[cfg(any(target_arch = "x86"))] {
        mod x86;
        pub use self::x86::*;
    } else if #[cfg(any(target_arch = "x86_64"))] {
        mod x86_64;
        pub use self::x86_64::*;
    } else {
        // Unknown target_arch
    }
}
