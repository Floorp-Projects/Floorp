pub type in_addr_t = u32;
pub type in_port_t = u16;

pub type socklen_t = u32;
pub type sa_family_t = u16;

s! {
    pub struct in_addr {
        pub s_addr: in_addr_t,
    }

    pub struct ip_mreq {
        pub imr_multiaddr: in_addr,
        pub imr_interface: in_addr,
    }

    pub struct ipv6_mreq {
        pub ipv6mr_multiaddr: ::in6_addr,
        pub ipv6mr_interface: ::c_uint,
    }

    pub struct linger {
        pub l_onoff: ::c_int,
        pub l_linger: ::c_int,
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

    pub struct sockaddr_storage {
        pub ss_family: sa_family_t,
        pub __ss_padding: [u8; 26],
    }
}

pub const AF_INET: ::c_int = 2;
pub const AF_INET6: ::c_int = 23;

pub const SOCK_STREAM: ::c_int = 1;
pub const SOCK_DGRAM: ::c_int = 2;

pub const IPPROTO_TCP: ::c_int = 6;
pub const IPPROTO_IP: ::c_int = 0;
pub const IPPROTO_IPV6: ::c_int = 41;

pub const TCP_KEEPIDLE: ::c_int = 4;
pub const TCP_NODELAY: ::c_int = 8193;

pub const IP_TTL: ::c_int = 8;
pub const IP_MULTICAST_LOOP: ::c_int = 9;
pub const IP_MULTICAST_TTL: ::c_int = 10;
pub const IP_ADD_MEMBERSHIP: ::c_int = 11;
pub const IP_DROP_MEMBERSHIP: ::c_int = 12;

pub const IPV6_MULTICAST_LOOP: ::c_int = 19;
pub const IPV6_ADD_MEMBERSHIP: ::c_int = 20;
pub const IPV6_DROP_MEMBERSHIP: ::c_int = 21;
pub const IPV6_V6ONLY: ::c_int = 26;

pub const SOL_SOCKET: ::c_int = 65535;

pub const SO_REUSEADDR: ::c_int = 4;
pub const SO_BROADCAST: ::c_int = 6;
pub const SO_KEEPALIVE: ::c_int = 8;
pub const SO_RCVTIMEO: ::c_int = 20;
pub const SO_SNDTIMEO: ::c_int = 21;
pub const SO_LINGER: ::c_int = 128;
pub const SO_SNDBUF: ::c_int = 4097;
pub const SO_RCVBUF: ::c_int = 4098;
pub const SO_ERROR: ::c_int = 4105;

extern {
    pub fn socket(domain: ::c_int, ty: ::c_int, protocol: ::c_int) -> ::c_int;
    pub fn bind(fd: ::c_int, addr: *const sockaddr, len: socklen_t) -> ::c_int;
    pub fn connect(socket: ::c_int, address: *const sockaddr,
                   len: socklen_t) -> ::c_int;
    pub fn listen(socket: ::c_int, backlog: ::c_int) -> ::c_int;
    pub fn getsockname(socket: ::c_int, address: *mut sockaddr,
                       address_len: *mut socklen_t) -> ::c_int;
    pub fn getsockopt(sockfd: ::c_int,
                      level: ::c_int,
                      optname: ::c_int,
                      optval: *mut ::c_void,
                      optlen: *mut ::socklen_t) -> ::c_int;
    pub fn setsockopt(socket: ::c_int, level: ::c_int, name: ::c_int,
                      value: *const ::c_void,
                      option_len: socklen_t) -> ::c_int;
    pub fn getpeername(socket: ::c_int, address: *mut sockaddr,
                       address_len: *mut socklen_t) -> ::c_int;
    pub fn sendto(socket: ::c_int, buf: *const ::c_void, len: ::size_t,
                  flags: ::c_int, addr: *const sockaddr,
                  addrlen: socklen_t) -> ::ssize_t;
    pub fn send(socket: ::c_int, buf: *const ::c_void, len: ::size_t,
                flags: ::c_int) -> ::ssize_t;
    pub fn recvfrom(socket: ::c_int, buf: *mut ::c_void, len: ::size_t,
                    flags: ::c_int, addr: *mut ::sockaddr,
                    addrlen: *mut ::socklen_t) -> ::ssize_t;
    pub fn recv(socket: ::c_int, buf: *mut ::c_void, len: ::size_t,
                flags: ::c_int) -> ::ssize_t;
}
