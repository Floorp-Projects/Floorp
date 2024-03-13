// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::missing_errors_doc)] // Functions simply delegate to tokio and quinn-udp.
#![allow(clippy::missing_panics_doc)] // Functions simply delegate to tokio and quinn-udp.

use std::{
    io::{self, IoSliceMut},
    net::{SocketAddr, ToSocketAddrs},
    slice,
};

use quinn_udp::{EcnCodepoint, RecvMeta, Transmit, UdpSocketState};
use tokio::io::Interest;

use crate::{Datagram, IpTos};

/// Socket receive buffer size.
///
/// Allows reading multiple datagrams in a single [`Socket::recv`] call.
const RECV_BUF_SIZE: usize = u16::MAX as usize;

pub struct Socket {
    socket: tokio::net::UdpSocket,
    state: UdpSocketState,
    recv_buf: Vec<u8>,
}

impl Socket {
    /// Calls [`std::net::UdpSocket::bind`] and instantiates [`quinn_udp::UdpSocketState`].
    pub fn bind<A: ToSocketAddrs>(addr: A) -> Result<Self, io::Error> {
        let socket = std::net::UdpSocket::bind(addr)?;

        Ok(Self {
            state: quinn_udp::UdpSocketState::new((&socket).into())?,
            socket: tokio::net::UdpSocket::from_std(socket)?,
            recv_buf: vec![0; RECV_BUF_SIZE],
        })
    }

    /// See [`tokio::net::UdpSocket::local_addr`].
    pub fn local_addr(&self) -> io::Result<SocketAddr> {
        self.socket.local_addr()
    }

    /// See [`tokio::net::UdpSocket::writable`].
    pub async fn writable(&self) -> Result<(), io::Error> {
        self.socket.writable().await
    }

    /// See [`tokio::net::UdpSocket::readable`].
    pub async fn readable(&self) -> Result<(), io::Error> {
        self.socket.readable().await
    }

    /// Send the UDP datagram on the specified socket.
    pub fn send(&self, d: Datagram) -> io::Result<usize> {
        let transmit = Transmit {
            destination: d.destination(),
            ecn: EcnCodepoint::from_bits(Into::<u8>::into(d.tos())),
            contents: d.into_data().into(),
            segment_size: None,
            src_ip: None,
        };

        let n = self.socket.try_io(Interest::WRITABLE, || {
            self.state
                .send((&self.socket).into(), slice::from_ref(&transmit))
        })?;

        assert_eq!(n, 1, "only passed one slice");

        Ok(n)
    }

    /// Receive a UDP datagram on the specified socket.
    pub fn recv(&mut self, local_address: &SocketAddr) -> Result<Vec<Datagram>, io::Error> {
        let mut meta = RecvMeta::default();

        match self.socket.try_io(Interest::READABLE, || {
            self.state.recv(
                (&self.socket).into(),
                &mut [IoSliceMut::new(&mut self.recv_buf)],
                slice::from_mut(&mut meta),
            )
        }) {
            Ok(n) => {
                assert_eq!(n, 1, "only passed one slice");
            }
            Err(ref err)
                if err.kind() == io::ErrorKind::WouldBlock
                    || err.kind() == io::ErrorKind::Interrupted =>
            {
                return Ok(vec![])
            }
            Err(err) => {
                return Err(err);
            }
        };

        if meta.len == 0 {
            eprintln!("zero length datagram received?");
            return Ok(vec![]);
        }
        if meta.len == self.recv_buf.len() {
            eprintln!(
                "Might have received more than {} bytes",
                self.recv_buf.len()
            );
        }

        Ok(self.recv_buf[0..meta.len]
            .chunks(meta.stride.min(self.recv_buf.len()))
            .map(|d| {
                Datagram::new(
                    meta.addr,
                    *local_address,
                    meta.ecn.map(|n| IpTos::from(n as u8)).unwrap_or_default(),
                    None, // TODO: get the real TTL https://github.com/quinn-rs/quinn/issues/1749
                    d,
                )
            })
            .collect())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{IpTosDscp, IpTosEcn};

    #[tokio::test]
    async fn datagram_tos() -> Result<(), io::Error> {
        let sender = Socket::bind("127.0.0.1:0")?;
        let receiver_addr: SocketAddr = "127.0.0.1:0".parse().unwrap();
        let mut receiver = Socket::bind(receiver_addr)?;

        let datagram = Datagram::new(
            sender.local_addr()?,
            receiver.local_addr()?,
            IpTos::from((IpTosDscp::Le, IpTosEcn::Ect1)),
            None,
            "Hello, world!".as_bytes().to_vec(),
        );

        sender.writable().await?;
        sender.send(datagram.clone())?;

        receiver.readable().await?;
        let received_datagram = receiver
            .recv(&receiver_addr)
            .expect("receive to succeed")
            .into_iter()
            .next()
            .expect("receive to yield datagram");

        // Assert that the ECN is correct.
        assert_eq!(
            IpTosEcn::from(datagram.tos()),
            IpTosEcn::from(received_datagram.tos())
        );

        Ok(())
    }

    /// Expect [`Socket::recv`] to handle multiple [`Datagram`]s on GRO read.
    #[tokio::test]
    #[cfg_attr(not(any(target_os = "linux", target_os = "windows")), ignore)]
    async fn many_datagrams_through_gro() -> Result<(), io::Error> {
        const SEGMENT_SIZE: usize = 128;

        let sender = Socket::bind("127.0.0.1:0")?;
        let receiver_addr: SocketAddr = "127.0.0.1:0".parse().unwrap();
        let mut receiver = Socket::bind(receiver_addr)?;

        // `neqo_common::udp::Socket::send` does not yet
        // (https://github.com/mozilla/neqo/issues/1693) support GSO. Use
        // `quinn_udp` directly.
        let max_gso_segments = sender.state.max_gso_segments();
        let msg = vec![0xAB; SEGMENT_SIZE * max_gso_segments];
        let transmit = Transmit {
            destination: receiver.local_addr()?,
            ecn: EcnCodepoint::from_bits(Into::<u8>::into(IpTos::from((
                IpTosDscp::Le,
                IpTosEcn::Ect1,
            )))),
            contents: msg.clone().into(),
            segment_size: Some(SEGMENT_SIZE),
            src_ip: None,
        };
        sender.writable().await?;
        let n = sender.socket.try_io(Interest::WRITABLE, || {
            sender
                .state
                .send((&sender.socket).into(), slice::from_ref(&transmit))
        })?;
        assert_eq!(n, 1, "only passed one slice");

        // Allow for one GSO sendmmsg to result in multiple GRO recvmmsg.
        let mut num_received = 0;
        while num_received < max_gso_segments {
            receiver.readable().await?;
            receiver
                .recv(&receiver_addr)
                .expect("receive to succeed")
                .into_iter()
                .for_each(|d| {
                    assert_eq!(
                        SEGMENT_SIZE,
                        d.len(),
                        "Expect received datagrams to have same length as sent datagrams."
                    );
                    num_received += 1;
                });
        }

        Ok(())
    }
}
