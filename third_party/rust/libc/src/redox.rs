pub type c_char = i8;
pub type c_long = i64;
pub type c_ulong = u64;

pub type wchar_t = i16;

pub type off_t = usize;
pub type mode_t = u16;
pub type time_t = i64;
pub type pid_t = usize;
pub type gid_t = usize;
pub type uid_t = usize;

pub type in_addr_t = u32;
pub type in_port_t = u16;

pub type socklen_t = u32;
pub type sa_family_t = u16;

s! {
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
}

extern {
    pub fn memalign(align: ::size_t, size: ::size_t) -> *mut ::c_void;
}
