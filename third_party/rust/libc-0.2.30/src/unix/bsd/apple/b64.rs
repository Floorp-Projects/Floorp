//! 64-bit specific Apple (ios/darwin) definitions

pub type c_long = i64;
pub type c_ulong = u64;
pub type boolean_t = ::c_uint;

s! {
    pub struct pthread_attr_t {
        __sig: c_long,
        __opaque: [::c_char; 56]
    }

    pub struct timeval32 {
        pub tv_sec: i32,
        pub tv_usec: i32,
    }

    pub struct if_data {
        pub ifi_type: ::c_uchar,
        pub ifi_typelen: ::c_uchar,
        pub ifi_physical: ::c_uchar,
        pub ifi_addrlen: ::c_uchar,
        pub ifi_hdrlen: ::c_uchar,
        pub ifi_recvquota: ::c_uchar,
        pub ifi_xmitquota: ::c_uchar,
        pub ifi_unused1: ::c_uchar,
        pub ifi_mtu: u32,
        pub ifi_metric: u32,
        pub ifi_baudrate: u32,
        pub ifi_ipackets: u32,
        pub ifi_ierrors: u32,
        pub ifi_opackets: u32,
        pub ifi_oerrors: u32,
        pub ifi_collisions: u32,
        pub ifi_ibytes: u32,
        pub ifi_obytes: u32,
        pub ifi_imcasts: u32,
        pub ifi_omcasts: u32,
        pub ifi_iqdrops: u32,
        pub ifi_noproto: u32,
        pub ifi_recvtiming: u32,
        pub ifi_xmittiming: u32,
        pub ifi_lastchange: timeval32,
        pub ifi_unused2: u32,
        pub ifi_hwassist: u32,
        pub ifi_reserved1: u32,
        pub ifi_reserved2: u32,
    }
}

pub const __PTHREAD_MUTEX_SIZE__: usize = 56;
pub const __PTHREAD_COND_SIZE__: usize = 40;
pub const __PTHREAD_CONDATTR_SIZE__: usize = 8;
pub const __PTHREAD_RWLOCK_SIZE__: usize = 192;
pub const __PTHREAD_RWLOCKATTR_SIZE__: usize = 16;

pub const TIOCTIMESTAMP: ::c_ulong = 0x40107459;
pub const TIOCDCDTIMESTAMP: ::c_ulong = 0x40107458;

pub const FIONREAD: ::c_ulong = 0x4004667f;
