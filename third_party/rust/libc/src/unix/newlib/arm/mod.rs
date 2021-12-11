pub type clock_t = ::c_long;
pub type c_char = u8;
pub type wchar_t = u32;

pub type c_long = i32;
pub type c_ulong = u32;

s! {
    pub struct sockaddr {
        pub sa_family: ::sa_family_t,
        pub sa_data: [::c_char; 14],
    }

    pub struct sockaddr_in6 {
        pub sin6_family: ::sa_family_t,
        pub sin6_port: ::in_port_t,
        pub sin6_flowinfo: u32,
        pub sin6_addr: ::in6_addr,
        pub sin6_scope_id: u32,
    }

    pub struct sockaddr_in {
        pub sin_family: ::sa_family_t,
        pub sin_port: ::in_port_t,
        pub sin_addr: ::in_addr,
        pub sin_zero: [u8; 8],
    }

    pub struct sockaddr_storage {
        pub ss_family: ::sa_family_t,
        pub __ss_padding: [u8; 26],
    }
}

pub const AF_INET6: ::c_int = 23;

pub const FIONBIO: ::c_ulong = 1;

pub const POLLIN: ::c_short = 0x1;
pub const POLLPRI: ::c_short = 0x2;
pub const POLLHUP: ::c_short = 0x4;
pub const POLLERR: ::c_short = 0x8;
pub const POLLOUT: ::c_short = 0x10;
pub const POLLNVAL: ::c_short = 0x20;

pub const SOL_SOCKET: ::c_int = 65535;

pub const MSG_OOB: ::c_int = 1;
pub const MSG_PEEK: ::c_int = 2;
pub const MSG_DONTWAIT: ::c_int = 4;
pub const MSG_DONTROUTE: ::c_int = 0;
pub const MSG_WAITALL: ::c_int = 0;
pub const MSG_MORE: ::c_int = 0;
pub const MSG_NOSIGNAL: ::c_int = 0;
