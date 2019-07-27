use std::fmt;
use std::mem;
use std::net::{SocketAddr, SocketAddrV4, SocketAddrV6};
use std::ptr;

#[cfg(any(unix, target_os = "redox"))]
use libc::{
    sa_family_t, sockaddr, sockaddr_in, sockaddr_in6, sockaddr_storage, socklen_t, AF_INET,
    AF_INET6,
};
#[cfg(windows)]
use winapi::shared::ws2def::{
    ADDRESS_FAMILY as sa_family_t, AF_INET, AF_INET6, SOCKADDR as sockaddr,
    SOCKADDR_IN as sockaddr_in, SOCKADDR_STORAGE as sockaddr_storage,
};
#[cfg(windows)]
use winapi::shared::ws2ipdef::SOCKADDR_IN6_LH as sockaddr_in6;
#[cfg(windows)]
use winapi::um::ws2tcpip::socklen_t;

use crate::SockAddr;

impl fmt::Debug for SockAddr {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        let mut builder = fmt.debug_struct("SockAddr");
        builder.field("family", &self.family());
        if let Some(addr) = self.as_inet() {
            builder.field("inet", &addr);
        } else if let Some(addr) = self.as_inet6() {
            builder.field("inet6", &addr);
        }
        builder.finish()
    }
}

impl SockAddr {
    /// Constructs a `SockAddr` from its raw components.
    pub unsafe fn from_raw_parts(addr: *const sockaddr, len: socklen_t) -> SockAddr {
        let mut storage = mem::uninitialized::<sockaddr_storage>();
        ptr::copy_nonoverlapping(
            addr as *const _ as *const u8,
            &mut storage as *mut _ as *mut u8,
            len as usize,
        );

        SockAddr {
            storage: storage,
            len: len,
        }
    }

    /// Constructs a `SockAddr` with the family `AF_UNIX` and the provided path.
    ///
    /// This function is only available on Unix when the `unix` feature is
    /// enabled.
    ///
    /// # Failure
    ///
    /// Returns an error if the path is longer than `SUN_LEN`.
    #[cfg(all(unix, feature = "unix"))]
    pub fn unix<P>(path: P) -> ::std::io::Result<SockAddr>
    where
        P: AsRef<::std::path::Path>,
    {
        use libc::{c_char, sockaddr_un, AF_UNIX};
        use std::cmp::Ordering;
        use std::io;
        use std::os::unix::ffi::OsStrExt;

        unsafe {
            let mut addr = mem::zeroed::<sockaddr_un>();
            addr.sun_family = AF_UNIX as sa_family_t;

            let bytes = path.as_ref().as_os_str().as_bytes();

            match (bytes.get(0), bytes.len().cmp(&addr.sun_path.len())) {
                // Abstract paths don't need a null terminator
                (Some(&0), Ordering::Greater) => {
                    return Err(io::Error::new(
                        io::ErrorKind::InvalidInput,
                        "path must be no longer than SUN_LEN",
                    ));
                }
                (Some(&0), _) => {}
                (_, Ordering::Greater) | (_, Ordering::Equal) => {
                    return Err(io::Error::new(
                        io::ErrorKind::InvalidInput,
                        "path must be shorter than SUN_LEN",
                    ));
                }
                _ => {}
            }

            for (dst, src) in addr.sun_path.iter_mut().zip(bytes) {
                *dst = *src as c_char;
            }
            // null byte for pathname is already there since we zeroed up front

            let base = &addr as *const _ as usize;
            let path = &addr.sun_path as *const _ as usize;
            let sun_path_offset = path - base;

            let mut len = sun_path_offset + bytes.len();
            match bytes.get(0) {
                Some(&0) | None => {}
                Some(_) => len += 1,
            }
            Ok(SockAddr::from_raw_parts(
                &addr as *const _ as *const _,
                len as socklen_t,
            ))
        }
    }

    unsafe fn as_<T>(&self, family: sa_family_t) -> Option<T> {
        if self.storage.ss_family != family {
            return None;
        }

        Some(mem::transmute_copy(&self.storage))
    }

    /// Returns this address as a `SocketAddrV4` if it is in the `AF_INET`
    /// family.
    pub fn as_inet(&self) -> Option<SocketAddrV4> {
        unsafe { self.as_(AF_INET as sa_family_t) }
    }

    /// Returns this address as a `SocketAddrV6` if it is in the `AF_INET6`
    /// family.
    pub fn as_inet6(&self) -> Option<SocketAddrV6> {
        unsafe { self.as_(AF_INET6 as sa_family_t) }
    }

    /// Returns this address's family.
    pub fn family(&self) -> sa_family_t {
        self.storage.ss_family
    }

    /// Returns the size of this address in bytes.
    pub fn len(&self) -> socklen_t {
        self.len
    }

    /// Returns a raw pointer to the address.
    pub fn as_ptr(&self) -> *const sockaddr {
        &self.storage as *const _ as *const _
    }
}

// SocketAddrV4 and SocketAddrV6 are just wrappers around sockaddr_in and sockaddr_in6

// check to make sure that the sizes at least match up
fn _size_checks(v4: SocketAddrV4, v6: SocketAddrV6) {
    unsafe {
        mem::transmute::<SocketAddrV4, sockaddr_in>(v4);
        mem::transmute::<SocketAddrV6, sockaddr_in6>(v6);
    }
}

impl From<SocketAddrV4> for SockAddr {
    fn from(addr: SocketAddrV4) -> SockAddr {
        unsafe {
            SockAddr::from_raw_parts(
                &addr as *const _ as *const _,
                mem::size_of::<SocketAddrV4>() as socklen_t,
            )
        }
    }
}

impl From<SocketAddrV6> for SockAddr {
    fn from(addr: SocketAddrV6) -> SockAddr {
        unsafe {
            SockAddr::from_raw_parts(
                &addr as *const _ as *const _,
                mem::size_of::<SocketAddrV6>() as socklen_t,
            )
        }
    }
}

impl From<SocketAddr> for SockAddr {
    fn from(addr: SocketAddr) -> SockAddr {
        match addr {
            SocketAddr::V4(addr) => addr.into(),
            SocketAddr::V6(addr) => addr.into(),
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn inet() {
        let raw = "127.0.0.1:80".parse::<SocketAddrV4>().unwrap();
        let addr = SockAddr::from(raw);
        assert!(addr.as_inet6().is_none());
        let addr = addr.as_inet().unwrap();
        assert_eq!(raw, addr);
    }

    #[test]
    fn inet6() {
        let raw = "[2001:db8::ff00:42:8329]:80"
            .parse::<SocketAddrV6>()
            .unwrap();
        let addr = SockAddr::from(raw);
        assert!(addr.as_inet().is_none());
        let addr = addr.as_inet6().unwrap();
        assert_eq!(raw, addr);
    }
}
