use codec::Codec;
use frame::Ping;
use proto::PingPayload;

use bytes::Buf;
use futures::{Async, Poll};
use std::io;
use tokio_io::AsyncWrite;

/// Acknowledges ping requests from the remote.
#[derive(Debug)]
pub struct PingPong {
    pending_ping: Option<PendingPing>,
    pending_pong: Option<PingPayload>,
}

#[derive(Debug)]
struct PendingPing {
    payload: PingPayload,
    sent: bool,
}

/// Status returned from `PingPong::recv_ping`.
#[derive(Debug)]
pub(crate) enum ReceivedPing {
    MustAck,
    Unknown,
    Shutdown,
}

impl PingPong {
    pub fn new() -> Self {
        PingPong {
            pending_ping: None,
            pending_pong: None,
        }
    }

    pub fn ping_shutdown(&mut self) {
        assert!(self.pending_ping.is_none());

        self.pending_ping = Some(PendingPing {
            payload: Ping::SHUTDOWN,
            sent: false,
        });
    }

    /// Process a ping
    pub(crate) fn recv_ping(&mut self, ping: Ping) -> ReceivedPing {
        // The caller should always check that `send_pongs` returns ready before
        // calling `recv_ping`.
        assert!(self.pending_pong.is_none());

        if ping.is_ack() {
            if let Some(pending) = self.pending_ping.take() {
                if &pending.payload == ping.payload() {
                    trace!("recv PING ack");
                    return ReceivedPing::Shutdown;
                }

                // if not the payload we expected, put it back.
                self.pending_ping = Some(pending);
            }

            // else we were acked a ping we didn't send?
            // The spec doesn't require us to do anything about this,
            // so for resiliency, just ignore it for now.
            warn!("recv PING ack that we never sent: {:?}", ping);
            ReceivedPing::Unknown
        } else {
            // Save the ping's payload to be sent as an acknowledgement.
            self.pending_pong = Some(ping.into_payload());
            ReceivedPing::MustAck
        }
    }

    /// Send any pending pongs.
    pub fn send_pending_pong<T, B>(&mut self, dst: &mut Codec<T, B>) -> Poll<(), io::Error>
    where
        T: AsyncWrite,
        B: Buf,
    {
        if let Some(pong) = self.pending_pong.take() {
            if !dst.poll_ready()?.is_ready() {
                self.pending_pong = Some(pong);
                return Ok(Async::NotReady);
            }

            dst.buffer(Ping::pong(pong).into())
                .expect("invalid pong frame");
        }

        Ok(Async::Ready(()))
    }

    /// Send any pending pings.
    pub fn send_pending_ping<T, B>(&mut self, dst: &mut Codec<T, B>) -> Poll<(), io::Error>
    where
        T: AsyncWrite,
        B: Buf,
    {
        if let Some(ref mut ping) = self.pending_ping {
            if !ping.sent {
                if !dst.poll_ready()?.is_ready() {
                    return Ok(Async::NotReady);
                }

                dst.buffer(Ping::new(ping.payload).into())
                    .expect("invalid ping frame");
                ping.sent = true;
            }
        }

        Ok(Async::Ready(()))
    }
}

impl ReceivedPing {
    pub fn is_shutdown(&self) -> bool {
        match *self {
            ReceivedPing::Shutdown => true,
            _ => false,
        }
    }
}
