// Copyright 2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::cmp;
use std::fmt;
use std::fs::File;
use std::io;
use std::io::{ErrorKind, Read, Write};
use std::mem;
use std::net::Shutdown;
use std::net::{self, Ipv4Addr, Ipv6Addr};
use std::ops::Neg;
use std::os::unix::prelude::*;
use std::time::Duration;
use syscall;

use libc::{self, c_int, c_uint, c_void, socklen_t, ssize_t};

use libc::IPV6_ADD_MEMBERSHIP;
use libc::IPV6_DROP_MEMBERSHIP;

const MSG_NOSIGNAL: c_int = 0x0;

use libc::TCP_KEEPIDLE as KEEPALIVE_OPTION;

use crate::utils::One;
use crate::SockAddr;

pub const IPPROTO_TCP: i32 = libc::IPPROTO_TCP;

pub const IPPROTO_ICMP: i32 = -1;
pub const IPPROTO_ICMPV6: i32 = -1;
pub const IPPROTO_UDP: i32 = -1;
pub const SOCK_RAW: i32 = -1;
pub const SOCK_SEQPACKET: i32 = -1;

pub struct Socket {
    fd: c_int,
}

impl Socket {
    pub fn new(family: c_int, ty: c_int, protocol: c_int) -> io::Result<Socket> {
        if ty == -1 {
            return Err(io::Error::new(ErrorKind::Other, "Type not implemented yet"));
        }
        if protocol == -1 {
            return Err(io::Error::new(
                ErrorKind::Other,
                "Protocol not implemented yet",
            ));
        }
        unsafe {
            let fd = cvt(libc::socket(family, ty, protocol))?;
            let fd = Socket::from_raw_fd(fd as RawFd);
            set_cloexec(fd.as_raw_fd() as c_int)?;
            Ok(fd)
        }
    }

    pub fn pair(_family: c_int, _ty: c_int, _protocol: c_int) -> io::Result<(Socket, Socket)> {
        return Err(io::Error::new(ErrorKind::Other, "Not implemented yet"));
    }

    pub fn bind(&self, addr: &SockAddr) -> io::Result<()> {
        unsafe { cvt(libc::bind(self.fd, addr.as_ptr(), addr.len() as _)).map(|_| ()) }
    }

    pub fn listen(&self, backlog: i32) -> io::Result<()> {
        unsafe { cvt(libc::listen(self.fd, backlog)).map(|_| ()) }
    }

    pub fn connect(&self, addr: &SockAddr) -> io::Result<()> {
        unsafe { cvt(libc::connect(self.fd, addr.as_ptr(), addr.len())).map(|_| ()) }
    }

    pub fn connect_timeout(&self, addr: &SockAddr, timeout: Duration) -> io::Result<()> {
        if timeout.as_secs() == 0 && timeout.subsec_nanos() == 0 {
            return Err(io::Error::new(
                ErrorKind::InvalidInput,
                "cannot set a 0 duration timeout",
            ));
        }
        if timeout.as_secs() > ::std::i64::MAX as u64 {
            return Err(io::Error::new(
                ErrorKind::InvalidInput,
                "too large duration",
            ));
        }

        self.connect(addr)?;

        let mut event = File::open("event:")?;
        let mut time = File::open("time:")?;

        event.write(&syscall::Event {
            id: self.fd as usize,
            flags: syscall::EVENT_WRITE,
            data: 0,
        })?;

        event.write(&syscall::Event {
            id: time.as_raw_fd(),
            flags: syscall::EVENT_WRITE,
            data: 1,
        })?;

        let mut current = syscall::TimeSpec::default();
        time.read(&mut current)?;
        current.tv_sec += timeout.as_secs() as i64;
        current.tv_nsec += timeout.subsec_nanos() as i32;
        time.write(&current)?;

        let mut out = syscall::Event::default();
        event.read(&mut out)?;

        if out.data == 1 {
            // the timeout we registered
            return Err(io::Error::new(ErrorKind::TimedOut, "connection timed out"));
        }

        Ok(())
    }

    pub fn local_addr(&self) -> io::Result<SockAddr> {
        unsafe {
            let mut storage: libc::sockaddr_storage = mem::zeroed();
            let mut len = mem::size_of_val(&storage) as libc::socklen_t;
            cvt(libc::getsockname(
                self.fd,
                &mut storage as *mut _ as *mut _,
                &mut len,
            ))?;
            Ok(SockAddr::from_raw_parts(
                &storage as *const _ as *const _,
                len,
            ))
        }
    }

    pub fn peer_addr(&self) -> io::Result<SockAddr> {
        unsafe {
            let mut storage: libc::sockaddr_storage = mem::zeroed();
            let mut len = mem::size_of_val(&storage) as libc::socklen_t;
            cvt(libc::getpeername(
                self.fd,
                &mut storage as *mut _ as *mut _,
                &mut len,
            ))?;
            Ok(SockAddr::from_raw_parts(
                &storage as *const _ as *const _,
                len as c_uint,
            ))
        }
    }

    pub fn try_clone(&self) -> io::Result<Socket> {
        return Err(io::Error::new(ErrorKind::Other, "Not implemented yet"));
    }

    #[allow(unused_mut)]
    pub fn accept(&self) -> io::Result<(Socket, SockAddr)> {
        return Err(io::Error::new(ErrorKind::Other, "Not implemented yet"));
    }

    pub fn take_error(&self) -> io::Result<Option<io::Error>> {
        unsafe {
            let raw: c_int = self.getsockopt(libc::SOL_SOCKET, libc::SO_ERROR)?;
            if raw == 0 {
                Ok(None)
            } else {
                Ok(Some(io::Error::from_raw_os_error(raw as i32)))
            }
        }
    }

    pub fn set_nonblocking(&self, nonblocking: bool) -> io::Result<()> {
        unsafe {
            let previous = cvt(libc::fcntl(self.fd, libc::F_GETFL))?;
            let new = if nonblocking {
                previous | libc::O_NONBLOCK
            } else {
                previous & !libc::O_NONBLOCK
            };
            if new != previous {
                cvt(libc::fcntl(self.fd, libc::F_SETFL, new))?;
            }
            Ok(())
        }
    }

    pub fn shutdown(&self, _how: Shutdown) -> io::Result<()> {
        return Err(io::Error::new(ErrorKind::Other, "Not implemented yet"));
    }

    pub fn recv(&self, buf: &mut [u8]) -> io::Result<usize> {
        unsafe {
            let n = cvt({
                libc::recv(
                    self.fd,
                    buf.as_mut_ptr() as *mut c_void,
                    cmp::min(buf.len(), max_len()),
                    0,
                )
            })?;
            Ok(n as usize)
        }
    }

    pub fn peek(&self, _buf: &mut [u8]) -> io::Result<usize> {
        return Err(io::Error::new(ErrorKind::Other, "Not implemented yet"));
    }

    pub fn recv_from(&self, buf: &mut [u8]) -> io::Result<(usize, SockAddr)> {
        self.recvfrom(buf, 0)
    }

    pub fn peek_from(&self, _buf: &mut [u8]) -> io::Result<(usize, SockAddr)> {
        return Err(io::Error::new(ErrorKind::Other, "Not implemented yet"));
    }

    fn recvfrom(&self, buf: &mut [u8], flags: c_int) -> io::Result<(usize, SockAddr)> {
        unsafe {
            let mut storage: libc::sockaddr_storage = mem::zeroed();
            let mut addrlen = mem::size_of_val(&storage) as socklen_t;

            let n = cvt({
                libc::recvfrom(
                    self.fd,
                    buf.as_mut_ptr() as *mut c_void,
                    cmp::min(buf.len(), max_len()),
                    flags,
                    &mut storage as *mut _ as *mut _,
                    &mut addrlen,
                )
            })?;
            let addr = SockAddr::from_raw_parts(&storage as *const _ as *const _, addrlen);
            Ok((n as usize, addr))
        }
    }

    pub fn send(&self, buf: &[u8]) -> io::Result<usize> {
        unsafe {
            let n = cvt({
                libc::send(
                    self.fd,
                    buf.as_ptr() as *const c_void,
                    cmp::min(buf.len(), max_len()),
                    MSG_NOSIGNAL,
                )
            })?;
            Ok(n as usize)
        }
    }

    pub fn send_to(&self, buf: &[u8], addr: &SockAddr) -> io::Result<usize> {
        unsafe {
            let n = cvt({
                libc::sendto(
                    self.fd,
                    buf.as_ptr() as *const c_void,
                    cmp::min(buf.len(), max_len()),
                    MSG_NOSIGNAL,
                    addr.as_ptr(),
                    addr.len(),
                )
            })?;
            Ok(n as usize)
        }
    }

    // ================================================

    pub fn ttl(&self) -> io::Result<u32> {
        unsafe {
            let raw: c_int = self.getsockopt(libc::IPPROTO_IP, libc::IP_TTL)?;
            Ok(raw as u32)
        }
    }

    pub fn set_ttl(&self, ttl: u32) -> io::Result<()> {
        unsafe { self.setsockopt(libc::IPPROTO_IP, libc::IP_TTL, ttl as c_int) }
    }

    pub fn unicast_hops_v6(&self) -> io::Result<u32> {
        return Err(io::Error::new(ErrorKind::Other, "Not implemented yet"));
    }

    pub fn set_unicast_hops_v6(&self, _hops: u32) -> io::Result<()> {
        return Err(io::Error::new(ErrorKind::Other, "Not implemented yet"));
    }

    pub fn only_v6(&self) -> io::Result<bool> {
        unsafe {
            let raw: c_int = self.getsockopt(libc::IPPROTO_IPV6, libc::IPV6_V6ONLY)?;
            Ok(raw != 0)
        }
    }

    pub fn set_only_v6(&self, only_v6: bool) -> io::Result<()> {
        unsafe { self.setsockopt(libc::IPPROTO_IPV6, libc::IPV6_V6ONLY, only_v6 as c_int) }
    }

    pub fn read_timeout(&self) -> io::Result<Option<Duration>> {
        unsafe {
            Ok(timeval2dur(
                self.getsockopt(libc::SOL_SOCKET, libc::SO_RCVTIMEO)?,
            ))
        }
    }

    pub fn set_read_timeout(&self, dur: Option<Duration>) -> io::Result<()> {
        unsafe { self.setsockopt(libc::SOL_SOCKET, libc::SO_RCVTIMEO, dur2timeval(dur)?) }
    }

    pub fn write_timeout(&self) -> io::Result<Option<Duration>> {
        unsafe {
            Ok(timeval2dur(
                self.getsockopt(libc::SOL_SOCKET, libc::SO_SNDTIMEO)?,
            ))
        }
    }

    pub fn set_write_timeout(&self, dur: Option<Duration>) -> io::Result<()> {
        unsafe { self.setsockopt(libc::SOL_SOCKET, libc::SO_SNDTIMEO, dur2timeval(dur)?) }
    }

    pub fn nodelay(&self) -> io::Result<bool> {
        unsafe {
            let raw: c_int = self.getsockopt(libc::IPPROTO_TCP, libc::TCP_NODELAY)?;
            Ok(raw != 0)
        }
    }

    pub fn set_nodelay(&self, nodelay: bool) -> io::Result<()> {
        unsafe { self.setsockopt(libc::IPPROTO_TCP, libc::TCP_NODELAY, nodelay as c_int) }
    }

    pub fn broadcast(&self) -> io::Result<bool> {
        unsafe {
            let raw: c_int = self.getsockopt(libc::SOL_SOCKET, libc::SO_BROADCAST)?;
            Ok(raw != 0)
        }
    }

    pub fn set_broadcast(&self, broadcast: bool) -> io::Result<()> {
        unsafe { self.setsockopt(libc::SOL_SOCKET, libc::SO_BROADCAST, broadcast as c_int) }
    }

    pub fn multicast_loop_v4(&self) -> io::Result<bool> {
        unsafe {
            let raw: c_int = self.getsockopt(libc::IPPROTO_IP, libc::IP_MULTICAST_LOOP)?;
            Ok(raw != 0)
        }
    }

    pub fn set_multicast_loop_v4(&self, multicast_loop_v4: bool) -> io::Result<()> {
        unsafe {
            self.setsockopt(
                libc::IPPROTO_IP,
                libc::IP_MULTICAST_LOOP,
                multicast_loop_v4 as c_int,
            )
        }
    }

    pub fn multicast_ttl_v4(&self) -> io::Result<u32> {
        unsafe {
            let raw: c_int = self.getsockopt(libc::IPPROTO_IP, libc::IP_MULTICAST_TTL)?;
            Ok(raw as u32)
        }
    }

    pub fn set_multicast_ttl_v4(&self, multicast_ttl_v4: u32) -> io::Result<()> {
        unsafe {
            self.setsockopt(
                libc::IPPROTO_IP,
                libc::IP_MULTICAST_TTL,
                multicast_ttl_v4 as c_int,
            )
        }
    }

    pub fn multicast_hops_v6(&self) -> io::Result<u32> {
        return Err(io::Error::new(ErrorKind::Other, "Not implemented yet"));
    }

    pub fn set_multicast_hops_v6(&self, _hops: u32) -> io::Result<()> {
        return Err(io::Error::new(ErrorKind::Other, "Not implemented yet"));
    }

    pub fn multicast_if_v4(&self) -> io::Result<Ipv4Addr> {
        return Err(io::Error::new(ErrorKind::Other, "Not implemented yet"));
    }

    pub fn set_multicast_if_v4(&self, _interface: &Ipv4Addr) -> io::Result<()> {
        return Err(io::Error::new(ErrorKind::Other, "Not implemented yet"));
    }

    pub fn multicast_if_v6(&self) -> io::Result<u32> {
        return Err(io::Error::new(ErrorKind::Other, "Not implemented yet"));
    }

    pub fn set_multicast_if_v6(&self, _interface: u32) -> io::Result<()> {
        return Err(io::Error::new(ErrorKind::Other, "Not implemented yet"));
    }

    pub fn multicast_loop_v6(&self) -> io::Result<bool> {
        unsafe {
            let raw: c_int = self.getsockopt(libc::IPPROTO_IPV6, libc::IPV6_MULTICAST_LOOP)?;
            Ok(raw != 0)
        }
    }

    pub fn set_multicast_loop_v6(&self, multicast_loop_v6: bool) -> io::Result<()> {
        unsafe {
            self.setsockopt(
                libc::IPPROTO_IPV6,
                libc::IPV6_MULTICAST_LOOP,
                multicast_loop_v6 as c_int,
            )
        }
    }

    pub fn join_multicast_v4(&self, multiaddr: &Ipv4Addr, interface: &Ipv4Addr) -> io::Result<()> {
        let multiaddr = to_s_addr(multiaddr);
        let interface = to_s_addr(interface);
        let mreq = libc::ip_mreq {
            imr_multiaddr: libc::in_addr { s_addr: multiaddr },
            imr_interface: libc::in_addr { s_addr: interface },
        };
        unsafe { self.setsockopt(libc::IPPROTO_IP, libc::IP_ADD_MEMBERSHIP, mreq) }
    }

    pub fn join_multicast_v6(&self, multiaddr: &Ipv6Addr, interface: u32) -> io::Result<()> {
        let multiaddr = to_in6_addr(multiaddr);
        let mreq = libc::ipv6_mreq {
            ipv6mr_multiaddr: multiaddr,
            ipv6mr_interface: to_ipv6mr_interface(interface),
        };
        unsafe { self.setsockopt(libc::IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, mreq) }
    }

    pub fn leave_multicast_v4(&self, multiaddr: &Ipv4Addr, interface: &Ipv4Addr) -> io::Result<()> {
        let multiaddr = to_s_addr(multiaddr);
        let interface = to_s_addr(interface);
        let mreq = libc::ip_mreq {
            imr_multiaddr: libc::in_addr { s_addr: multiaddr },
            imr_interface: libc::in_addr { s_addr: interface },
        };
        unsafe { self.setsockopt(libc::IPPROTO_IP, libc::IP_DROP_MEMBERSHIP, mreq) }
    }

    pub fn leave_multicast_v6(&self, multiaddr: &Ipv6Addr, interface: u32) -> io::Result<()> {
        let multiaddr = to_in6_addr(multiaddr);
        let mreq = libc::ipv6_mreq {
            ipv6mr_multiaddr: multiaddr,
            ipv6mr_interface: to_ipv6mr_interface(interface),
        };
        unsafe { self.setsockopt(libc::IPPROTO_IPV6, IPV6_DROP_MEMBERSHIP, mreq) }
    }

    pub fn linger(&self) -> io::Result<Option<Duration>> {
        unsafe {
            Ok(linger2dur(
                self.getsockopt(libc::SOL_SOCKET, libc::SO_LINGER)?,
            ))
        }
    }

    pub fn set_linger(&self, dur: Option<Duration>) -> io::Result<()> {
        unsafe { self.setsockopt(libc::SOL_SOCKET, libc::SO_LINGER, dur2linger(dur)) }
    }

    pub fn set_reuse_address(&self, reuse: bool) -> io::Result<()> {
        unsafe { self.setsockopt(libc::SOL_SOCKET, libc::SO_REUSEADDR, reuse as c_int) }
    }

    pub fn reuse_address(&self) -> io::Result<bool> {
        unsafe {
            let raw: c_int = self.getsockopt(libc::SOL_SOCKET, libc::SO_REUSEADDR)?;
            Ok(raw != 0)
        }
    }

    pub fn recv_buffer_size(&self) -> io::Result<usize> {
        unsafe {
            let raw: c_int = self.getsockopt(libc::SOL_SOCKET, libc::SO_RCVBUF)?;
            Ok(raw as usize)
        }
    }

    pub fn set_recv_buffer_size(&self, size: usize) -> io::Result<()> {
        unsafe {
            // TODO: casting usize to a c_int should be a checked cast
            self.setsockopt(libc::SOL_SOCKET, libc::SO_RCVBUF, size as c_int)
        }
    }

    pub fn send_buffer_size(&self) -> io::Result<usize> {
        unsafe {
            let raw: c_int = self.getsockopt(libc::SOL_SOCKET, libc::SO_SNDBUF)?;
            Ok(raw as usize)
        }
    }

    pub fn set_send_buffer_size(&self, size: usize) -> io::Result<()> {
        unsafe {
            // TODO: casting usize to a c_int should be a checked cast
            self.setsockopt(libc::SOL_SOCKET, libc::SO_SNDBUF, size as c_int)
        }
    }

    pub fn keepalive(&self) -> io::Result<Option<Duration>> {
        unsafe {
            let raw: c_int = self.getsockopt(libc::SOL_SOCKET, libc::SO_KEEPALIVE)?;
            if raw == 0 {
                return Ok(None);
            }
            let secs: c_int = self.getsockopt(libc::IPPROTO_TCP, KEEPALIVE_OPTION)?;
            Ok(Some(Duration::new(secs as u64, 0)))
        }
    }

    pub fn set_keepalive(&self, keepalive: Option<Duration>) -> io::Result<()> {
        unsafe {
            self.setsockopt(
                libc::SOL_SOCKET,
                libc::SO_KEEPALIVE,
                keepalive.is_some() as c_int,
            )?;
            if let Some(dur) = keepalive {
                // TODO: checked cast here
                self.setsockopt(libc::IPPROTO_TCP, KEEPALIVE_OPTION, dur.as_secs() as c_int)?;
            }
            Ok(())
        }
    }

    unsafe fn setsockopt<T>(&self, opt: c_int, val: c_int, payload: T) -> io::Result<()>
    where
        T: Copy,
    {
        let payload = &payload as *const T as *const c_void;
        cvt(libc::setsockopt(
            self.fd,
            opt,
            val,
            payload,
            mem::size_of::<T>() as libc::socklen_t,
        ))?;
        Ok(())
    }

    unsafe fn getsockopt<T: Copy>(&self, opt: c_int, val: c_int) -> io::Result<T> {
        let mut slot: T = mem::zeroed();
        let mut len = mem::size_of::<T>() as libc::socklen_t;
        cvt(libc::getsockopt(
            self.fd,
            opt,
            val,
            &mut slot as *mut _ as *mut _,
            &mut len,
        ))?;
        assert_eq!(len as usize, mem::size_of::<T>());
        Ok(slot)
    }
}

impl Read for Socket {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        <&Socket>::read(&mut &*self, buf)
    }
}

impl<'a> Read for &'a Socket {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        unsafe {
            let n = cvt({
                libc::read(
                    self.fd,
                    buf.as_mut_ptr() as *mut c_void,
                    cmp::min(buf.len(), max_len()),
                )
            })?;
            Ok(n as usize)
        }
    }
}

impl Write for Socket {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        <&Socket>::write(&mut &*self, buf)
    }

    fn flush(&mut self) -> io::Result<()> {
        <&Socket>::flush(&mut &*self)
    }
}

impl<'a> Write for &'a Socket {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.send(buf)
    }

    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}

impl fmt::Debug for Socket {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut f = f.debug_struct("Socket");
        f.field("fd", &self.fd);
        if let Ok(addr) = self.local_addr() {
            f.field("local_addr", &addr);
        }
        if let Ok(addr) = self.peer_addr() {
            f.field("peer_addr", &addr);
        }
        f.finish()
    }
}

impl AsRawFd for Socket {
    fn as_raw_fd(&self) -> RawFd {
        self.fd as RawFd
    }
}

impl IntoRawFd for Socket {
    fn into_raw_fd(self) -> RawFd {
        let fd = self.fd as RawFd;
        mem::forget(self);
        return fd;
    }
}

impl FromRawFd for Socket {
    unsafe fn from_raw_fd(fd: RawFd) -> Socket {
        Socket { fd: fd as c_int }
    }
}

impl AsRawFd for crate::Socket {
    fn as_raw_fd(&self) -> RawFd {
        self.inner.as_raw_fd()
    }
}

impl IntoRawFd for crate::Socket {
    fn into_raw_fd(self) -> RawFd {
        self.inner.into_raw_fd()
    }
}

impl FromRawFd for crate::Socket {
    unsafe fn from_raw_fd(fd: RawFd) -> crate::Socket {
        crate::Socket {
            inner: Socket::from_raw_fd(fd),
        }
    }
}

impl Drop for Socket {
    fn drop(&mut self) {
        unsafe {
            let _ = libc::close(self.fd);
        }
    }
}

impl From<Socket> for net::TcpStream {
    fn from(socket: Socket) -> net::TcpStream {
        unsafe { net::TcpStream::from_raw_fd(socket.into_raw_fd()) }
    }
}

impl From<Socket> for net::TcpListener {
    fn from(socket: Socket) -> net::TcpListener {
        unsafe { net::TcpListener::from_raw_fd(socket.into_raw_fd()) }
    }
}

impl From<Socket> for net::UdpSocket {
    fn from(socket: Socket) -> net::UdpSocket {
        unsafe { net::UdpSocket::from_raw_fd(socket.into_raw_fd()) }
    }
}

impl From<net::TcpStream> for Socket {
    fn from(socket: net::TcpStream) -> Socket {
        unsafe { Socket::from_raw_fd(socket.into_raw_fd()) }
    }
}

impl From<net::TcpListener> for Socket {
    fn from(socket: net::TcpListener) -> Socket {
        unsafe { Socket::from_raw_fd(socket.into_raw_fd()) }
    }
}

impl From<net::UdpSocket> for Socket {
    fn from(socket: net::UdpSocket) -> Socket {
        unsafe { Socket::from_raw_fd(socket.into_raw_fd()) }
    }
}

fn max_len() -> usize {
    // The maximum read limit on most posix-like systems is `SSIZE_MAX`,
    // with the man page quoting that if the count of bytes to read is
    // greater than `SSIZE_MAX` the result is "unspecified".
    <ssize_t>::max_value() as usize
}

fn cvt<T: One + PartialEq + Neg<Output = T>>(t: T) -> io::Result<T> {
    let one: T = T::one();
    if t == -one {
        Err(io::Error::last_os_error())
    } else {
        Ok(t)
    }
}

fn set_cloexec(fd: c_int) -> io::Result<()> {
    unsafe {
        let previous = cvt(libc::fcntl(fd, libc::F_GETFD))?;
        let new = previous | syscall::O_CLOEXEC as i32;
        if new != previous {
            cvt(libc::fcntl(fd, libc::F_SETFD, new))?;
        }
        Ok(())
    }
}

fn dur2timeval(dur: Option<Duration>) -> io::Result<libc::timeval> {
    match dur {
        Some(dur) => {
            if dur.as_secs() == 0 && dur.subsec_nanos() == 0 {
                return Err(io::Error::new(
                    ErrorKind::InvalidInput,
                    "cannot set a 0 duration timeout",
                ));
            }

            let secs = if dur.as_secs() > libc::time_t::max_value() as u64 {
                libc::time_t::max_value()
            } else {
                dur.as_secs() as libc::time_t
            };
            let mut timeout = libc::timeval {
                tv_sec: secs,
                tv_usec: (dur.subsec_nanos() / 1000) as libc::suseconds_t,
            };
            if timeout.tv_sec == 0 && timeout.tv_usec == 0 {
                timeout.tv_usec = 1;
            }
            Ok(timeout)
        }
        None => Ok(libc::timeval {
            tv_sec: 0,
            tv_usec: 0,
        }),
    }
}

fn timeval2dur(raw: libc::timeval) -> Option<Duration> {
    if raw.tv_sec == 0 && raw.tv_usec == 0 {
        None
    } else {
        let sec = raw.tv_sec as u64;
        let nsec = (raw.tv_usec as u32) * 1000;
        Some(Duration::new(sec, nsec))
    }
}

fn to_s_addr(addr: &Ipv4Addr) -> libc::in_addr_t {
    let octets = addr.octets();
    crate::hton(
        ((octets[0] as libc::in_addr_t) << 24)
            | ((octets[1] as libc::in_addr_t) << 16)
            | ((octets[2] as libc::in_addr_t) << 8)
            | ((octets[3] as libc::in_addr_t) << 0),
    )
}

fn to_in6_addr(addr: &Ipv6Addr) -> libc::in6_addr {
    let mut ret: libc::in6_addr = unsafe { mem::zeroed() };
    ret.s6_addr = addr.octets();
    return ret;
}

fn to_ipv6mr_interface(value: u32) -> libc::c_uint {
    value as libc::c_uint
}

fn linger2dur(linger_opt: libc::linger) -> Option<Duration> {
    if linger_opt.l_onoff == 0 {
        None
    } else {
        Some(Duration::from_secs(linger_opt.l_linger as u64))
    }
}

fn dur2linger(dur: Option<Duration>) -> libc::linger {
    match dur {
        Some(d) => libc::linger {
            l_onoff: 1,
            l_linger: d.as_secs() as c_int,
        },
        None => libc::linger {
            l_onoff: 0,
            l_linger: 0,
        },
    }
}

#[test]
fn test_ip() {
    let ip = Ipv4Addr::new(127, 0, 0, 1);
    assert_eq!(ip, from_s_addr(to_s_addr(&ip)));
}
