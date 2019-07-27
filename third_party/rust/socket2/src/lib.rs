// Copyright 2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Utilities for handling sockets
//!
//! This crate is sort of an evolution of the `net2` crate after seeing the
//! issues on it over time. The intention of this crate is to provide as direct
//! as possible access to the system's functionality for sockets as possible. No
//! extra fluff (e.g. multiple syscalls or builders) provided in this crate. As
//! a result using this crate can be a little wordy, but it should give you
//! maximal flexibility over configuration of sockets.
//!
//! # Examples
//!
//! ```no_run
//! use std::net::SocketAddr;
//! use socket2::{Socket, Domain, Type};
//!
//! // create a TCP listener bound to two addresses
//! let socket = Socket::new(Domain::ipv6(), Type::stream(), None).unwrap();
//!
//! socket.bind(&"[::1]:12345".parse::<SocketAddr>().unwrap().into()).unwrap();
//! socket.set_only_v6(false);
//! socket.listen(128).unwrap();
//!
//! let listener = socket.into_tcp_listener();
//! // ...
//! ```

#![doc(html_root_url = "https://docs.rs/socket2/0.3")]
#![deny(missing_docs)]

use crate::utils::NetInt;

#[cfg(any(unix, target_os = "redox"))]
use libc::{sockaddr_storage, socklen_t};
#[cfg(windows)]
use winapi::shared::ws2def::SOCKADDR_STORAGE as sockaddr_storage;
#[cfg(windows)]
use winapi::um::ws2tcpip::socklen_t;

mod sockaddr;
mod socket;
mod utils;

#[cfg(unix)]
#[path = "sys/unix.rs"]
mod sys;
#[cfg(windows)]
#[path = "sys/windows.rs"]
mod sys;
#[cfg(target_os = "redox")]
#[path = "sys/redox/mod.rs"]
mod sys;

/// Newtype, owned, wrapper around a system socket.
///
/// This type simply wraps an instance of a file descriptor (`c_int`) on Unix
/// and an instance of `SOCKET` on Windows. This is the main type exported by
/// this crate and is intended to mirror the raw semantics of sockets on
/// platforms as closely as possible. Almost all methods correspond to
/// precisely one libc or OS API call which is essentially just a "Rustic
/// translation" of what's below.
///
/// # Examples
///
/// ```no_run
/// use std::net::SocketAddr;
/// use socket2::{Socket, Domain, Type, SockAddr};
///
/// // create a TCP listener bound to two addresses
/// let socket = Socket::new(Domain::ipv4(), Type::stream(), None).unwrap();
///
/// socket.bind(&"127.0.0.1:12345".parse::<SocketAddr>().unwrap().into()).unwrap();
/// socket.bind(&"127.0.0.1:12346".parse::<SocketAddr>().unwrap().into()).unwrap();
/// socket.listen(128).unwrap();
///
/// let listener = socket.into_tcp_listener();
/// // ...
/// ```
pub struct Socket {
    inner: sys::Socket,
}

/// The address of a socket.
///
/// `SockAddr`s may be constructed directly to and from the standard library
/// `SocketAddr`, `SocketAddrV4`, and `SocketAddrV6` types.
pub struct SockAddr {
    storage: sockaddr_storage,
    len: socklen_t,
}

/// Specification of the communication domain for a socket.
///
/// This is a newtype wrapper around an integer which provides a nicer API in
/// addition to an injection point for documentation. Convenience constructors
/// such as `Domain::ipv4`, `Domain::ipv6`, etc, are provided to avoid reaching
/// into libc for various constants.
///
/// This type is freely interconvertible with the `i32` type, however, if a raw
/// value needs to be provided.
#[derive(Copy, Clone)]
pub struct Domain(i32);

/// Specification of communication semantics on a socket.
///
/// This is a newtype wrapper around an integer which provides a nicer API in
/// addition to an injection point for documentation. Convenience constructors
/// such as `Type::stream`, `Type::dgram`, etc, are provided to avoid reaching
/// into libc for various constants.
///
/// This type is freely interconvertible with the `i32` type, however, if a raw
/// value needs to be provided.
#[derive(Copy, Clone)]
pub struct Type(i32);

/// Protocol specification used for creating sockets via `Socket::new`.
///
/// This is a newtype wrapper around an integer which provides a nicer API in
/// addition to an injection point for documentation.
///
/// This type is freely interconvertible with the `i32` type, however, if a raw
/// value needs to be provided.
#[derive(Copy, Clone)]
pub struct Protocol(i32);

fn hton<I: NetInt>(i: I) -> I {
    i.to_be()
}

#[cfg(not(target_os = "redox"))]
fn ntoh<I: NetInt>(i: I) -> I {
    I::from_be(i)
}
