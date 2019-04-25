// Copyright 2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::fmt;
use std::io;
use std::mem;
use std::net::{Ipv4Addr, Ipv6Addr, SocketAddr, SocketAddrV4, SocketAddrV6};
#[cfg(target_os = "redox")]
use libc::c_int;
#[cfg(unix)]
use libc::c_int;
#[cfg(windows)]
use winapi::ctypes::c_int;

use sys;
use sys::c;

pub struct Socket {
    inner: sys::Socket,
}

impl Socket {
    pub fn new(family: c_int, ty: c_int) -> io::Result<Socket> {
        Ok(Socket { inner: try!(sys::Socket::new(family, ty)) })
    }

    pub fn bind(&self, addr: &SocketAddr) -> io::Result<()> {
        let (addr, len) = addr2raw(addr);
        unsafe {
            ::cvt(c::bind(self.inner.raw(), addr, len as c::socklen_t)).map(|_| ())
        }
    }

    pub fn listen(&self, backlog: i32) -> io::Result<()> {
        unsafe {
            ::cvt(c::listen(self.inner.raw(), backlog)).map(|_| ())
        }
    }

    pub fn connect(&self, addr: &SocketAddr) -> io::Result<()> {
        let (addr, len) = addr2raw(addr);
        unsafe {
            ::cvt(c::connect(self.inner.raw(), addr, len)).map(|_| ())
        }
    }

    pub fn getsockname(&self) -> io::Result<SocketAddr> {
        unsafe {
            let mut storage: c::sockaddr_storage = mem::zeroed();
            let mut len = mem::size_of_val(&storage) as c::socklen_t;
            try!(::cvt(c::getsockname(self.inner.raw(),
                                      &mut storage as *mut _ as *mut _,
                                      &mut len)));
            raw2addr(&storage, len)
        }
    }
}

impl fmt::Debug for Socket {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.inner.raw().fmt(f)
    }
}

impl ::AsInner for Socket {
    type Inner = sys::Socket;
    fn as_inner(&self) -> &sys::Socket { &self.inner }
}

impl ::FromInner for Socket {
    type Inner = sys::Socket;
    fn from_inner(sock: sys::Socket) -> Socket {
        Socket { inner: sock }
    }
}

impl ::IntoInner for Socket {
    type Inner = sys::Socket;
    fn into_inner(self) -> sys::Socket { self.inner }
}

fn addr2raw(addr: &SocketAddr) -> (*const c::sockaddr, c::socklen_t) {
    match *addr {
        SocketAddr::V4(ref a) => {
            (a as *const _ as *const _, mem::size_of_val(a) as c::socklen_t)
        }
        SocketAddr::V6(ref a) => {
            (a as *const _ as *const _, mem::size_of_val(a) as c::socklen_t)
        }
    }
}

fn raw2addr(storage: &c::sockaddr_storage, len: c::socklen_t) -> io::Result<SocketAddr> {
    match storage.ss_family as c_int {
        c::AF_INET => {
            unsafe {
                assert!(len as usize >= mem::size_of::<c::sockaddr_in>());
                let sa = storage as *const _ as *const c::sockaddr_in;
                let bits = c::sockaddr_in_u32(&(*sa));
                let ip = Ipv4Addr::new((bits >> 24) as u8,
                                       (bits >> 16) as u8,
                                       (bits >> 8) as u8,
                                       bits as u8);
                Ok(SocketAddr::V4(SocketAddrV4::new(ip, ::ntoh((*sa).sin_port))))
            }
        }
        c::AF_INET6 => {
            unsafe {
                assert!(len as usize >= mem::size_of::<c::sockaddr_in6>());

                let sa = storage as *const _ as *const c::sockaddr_in6;
                #[cfg(windows)]      let arr = (*sa).sin6_addr.u.Byte();
                #[cfg(not(windows))] let arr = (*sa).sin6_addr.s6_addr;

                let ip = Ipv6Addr::new(
                    (arr[0] as u16) << 8 | (arr[1] as u16),
                    (arr[2] as u16) << 8 | (arr[3] as u16),
                    (arr[4] as u16) << 8 | (arr[5] as u16),
                    (arr[6] as u16) << 8 | (arr[7] as u16),
                    (arr[8] as u16) << 8 | (arr[9] as u16),
                    (arr[10] as u16) << 8 | (arr[11] as u16),
                    (arr[12] as u16) << 8 | (arr[13] as u16),
                    (arr[14] as u16) << 8 | (arr[15] as u16),
                );

                #[cfg(windows)]      let sin6_scope_id = *(*sa).u.sin6_scope_id();
                #[cfg(not(windows))] let sin6_scope_id = (*sa).sin6_scope_id;

                Ok(SocketAddr::V6(SocketAddrV6::new(ip,
                                                    ::ntoh((*sa).sin6_port),
                                                    (*sa).sin6_flowinfo,
                                                    sin6_scope_id)))
            }
        }
        _ => Err(io::Error::new(io::ErrorKind::InvalidInput, "invalid argument")),
    }
}
