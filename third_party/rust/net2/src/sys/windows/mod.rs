// Copyright 2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(bad_style)]

use std::io;
use std::mem;
use std::net::{TcpListener, TcpStream, UdpSocket};
use std::os::windows::io::FromRawSocket;
use std::sync::{Once, ONCE_INIT};

use winapi::*;
use ws2_32::*;
use kernel32::*;

const WSA_FLAG_OVERLAPPED: DWORD = 0x01;
const HANDLE_FLAG_INHERIT: DWORD = 0x00000001;

pub mod c {
    pub use winapi::*;
    pub use ws2_32::*;

    pub use winapi::SOCKADDR as sockaddr;
    pub use winapi::SOCKADDR_STORAGE as sockaddr_storage;
    pub use winapi::SOCKADDR_IN as sockaddr_in;

    pub fn sockaddr_in_u32(sa: &sockaddr_in) -> u32 {
        ::ntoh(sa.sin_addr.S_un)
    }
}

mod impls;

fn init() {
    static INIT: Once = ONCE_INIT;

    INIT.call_once(|| {
        // Initialize winsock through the standard library by just creating a
        // dummy socket. Whether this is successful or not we drop the result as
        // libstd will be sure to have initialized winsock.
        let _ = UdpSocket::bind("127.0.0.1:34254");
    });
}

pub struct Socket {
    socket: SOCKET,
}

impl Socket {
    pub fn new(family: c_int, ty: c_int) -> io::Result<Socket> {
        init();
        let socket = try!(unsafe {
            match WSASocketW(family, ty, 0, 0 as *mut _, 0,
                             WSA_FLAG_OVERLAPPED) {
                INVALID_SOCKET => Err(io::Error::last_os_error()),
                n => Ok(Socket { socket: n }),
            }
        });
        try!(socket.set_no_inherit());
        Ok(socket)
    }

    pub fn raw(&self) -> SOCKET { self.socket }

    fn into_socket(self) -> SOCKET {
        let socket = self.socket;
        mem::forget(self);
        socket
    }

    pub fn into_tcp_listener(self) -> TcpListener {
        unsafe { TcpListener::from_raw_socket(self.into_socket()) }
    }

    pub fn into_tcp_stream(self) -> TcpStream {
        unsafe { TcpStream::from_raw_socket(self.into_socket()) }
    }

    pub fn into_udp_socket(self) -> UdpSocket {
        unsafe { UdpSocket::from_raw_socket(self.into_socket()) }
    }

    fn set_no_inherit(&self) -> io::Result<()> {
        ::cvt_win(unsafe {
            SetHandleInformation(self.socket as HANDLE, HANDLE_FLAG_INHERIT, 0)
        }).map(|_| ())
    }
}

impl ::FromInner for Socket {
    type Inner = SOCKET;
    fn from_inner(socket: SOCKET) -> Socket {
        Socket { socket: socket }
    }
}

impl Drop for Socket {
    fn drop(&mut self) {
        unsafe {
            let _ = closesocket(self.socket);
        }
    }
}
