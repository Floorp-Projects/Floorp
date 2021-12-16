// Copyright © 2021 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use std::io::Result;

use mio::{event::Source, Interest, Registry, Token};

#[cfg(unix)]
mod unix;
#[cfg(unix)]
pub use self::unix::*;

#[cfg(windows)]
mod windows;
#[cfg(windows)]
pub use self::windows::*;

impl Source for Pipe {
    fn register(&mut self, registry: &Registry, token: Token, interests: Interest) -> Result<()> {
        self.0.register(registry, token, interests)
    }

    fn reregister(&mut self, registry: &Registry, token: Token, interests: Interest) -> Result<()> {
        self.0.reregister(registry, token, interests)
    }

    fn deregister(&mut self, registry: &Registry) -> Result<()> {
        self.0.deregister(registry)
    }
}

pub trait RecvMsg {
    // Receive data from the associated connection.  `recv_msg` expects the capacity of
    // the `ConnectionBuffer` members have been adjusted appropriately by the caller.
    fn recv_msg(&mut self, buf: &mut ConnectionBuffer) -> Result<usize>;
}

pub trait SendMsg {
    // Send data on the associated connection.  `send_msg` consumes and adjusts the length of the
    // `ConnectionBuffer` members based on the size of the successful send operation.
    fn send_msg(&mut self, buf: &mut ConnectionBuffer) -> Result<usize>;
}
