// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::cmp::Ordering;
use std::mem;
use std::time::Instant;

use crate::frame::{Frame, FrameType};
use crate::packet::PacketBuilder;
use crate::recovery::RecoveryToken;
use crate::{CloseError, ConnectionError};

#[derive(Clone, Debug, PartialEq, Ord, Eq)]
/// The state of the Connection.
pub enum State {
    Init,
    WaitInitial,
    Handshaking,
    Connected,
    Confirmed,
    Closing {
        error: ConnectionError,
        timeout: Instant,
    },
    Draining {
        error: ConnectionError,
        timeout: Instant,
    },
    Closed(ConnectionError),
}

impl State {
    #[must_use]
    pub fn connected(&self) -> bool {
        matches!(self, Self::Connected | Self::Confirmed)
    }

    #[must_use]
    pub fn closed(&self) -> bool {
        matches!(self, Self::Closing { .. } | Self::Draining { .. } | Self::Closed(_))
    }
}

// Implement Ord so that we can enforce monotonic state progression.
impl PartialOrd for State {
    #[allow(clippy::match_same_arms)] // Lint bug: rust-lang/rust-clippy#860
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        if mem::discriminant(self) == mem::discriminant(other) {
            return Some(Ordering::Equal);
        }
        Some(match (self, other) {
            (Self::Init, _) => Ordering::Less,
            (_, Self::Init) => Ordering::Greater,
            (Self::WaitInitial, _) => Ordering::Less,
            (_, Self::WaitInitial) => Ordering::Greater,
            (Self::Handshaking, _) => Ordering::Less,
            (_, Self::Handshaking) => Ordering::Greater,
            (Self::Connected, _) => Ordering::Less,
            (_, Self::Connected) => Ordering::Greater,
            (Self::Confirmed, _) => Ordering::Less,
            (_, Self::Confirmed) => Ordering::Greater,
            (Self::Closing { .. }, _) => Ordering::Less,
            (_, Self::Closing { .. }) => Ordering::Greater,
            (Self::Draining { .. }, _) => Ordering::Less,
            (_, Self::Draining { .. }) => Ordering::Greater,
            (Self::Closed(_), _) => unreachable!(),
        })
    }
}

type ClosingFrame = Frame<'static>;

/// `StateSignaling` manages whether we need to send HANDSHAKE_DONE and CONNECTION_CLOSE.
/// Valid state transitions are:
/// * Idle -> HandshakeDone: at the server when the handshake completes
/// * HandshakeDone -> Idle: when a HANDSHAKE_DONE frame is sent
/// * Idle/HandshakeDone -> Closing/Draining: when closing or draining
/// * Closing/Draining -> CloseSent: after sending CONNECTION_CLOSE
/// * CloseSent -> Closing: any time a new CONNECTION_CLOSE is needed
/// * -> Reset: from any state in case of a stateless reset
#[derive(Debug, Clone, PartialEq)]
pub enum StateSignaling {
    Idle,
    HandshakeDone,
    /// These states save the frame that needs to be sent.
    Closing(ClosingFrame),
    Draining(ClosingFrame),
    /// This state saves the frame that might need to be sent again.
    /// If it is `None`, then we are draining and don't send.
    CloseSent(Option<ClosingFrame>),
    Reset,
}

impl StateSignaling {
    pub fn handshake_done(&mut self) {
        if *self != Self::Idle {
            debug_assert!(false, "StateSignaling must be in Idle state.");
            return;
        }
        *self = Self::HandshakeDone
    }

    pub fn write_done(&mut self, builder: &mut PacketBuilder) -> Option<RecoveryToken> {
        if *self == Self::HandshakeDone && builder.remaining() >= 1 {
            *self = Self::Idle;
            builder.encode_varint(Frame::HandshakeDone.get_type());
            Some(RecoveryToken::HandshakeDone)
        } else {
            None
        }
    }

    fn make_close_frame(
        error: ConnectionError,
        frame_type: FrameType,
        message: impl AsRef<str>,
    ) -> ClosingFrame {
        let reason_phrase = message.as_ref().as_bytes().to_owned();
        Frame::ConnectionClose {
            error_code: CloseError::from(error),
            frame_type,
            reason_phrase,
        }
    }

    pub fn close(
        &mut self,
        error: ConnectionError,
        frame_type: FrameType,
        message: impl AsRef<str>,
    ) {
        if *self != Self::Reset {
            *self = Self::Closing(Self::make_close_frame(error, frame_type, message));
        }
    }

    pub fn drain(
        &mut self,
        error: ConnectionError,
        frame_type: FrameType,
        message: impl AsRef<str>,
    ) {
        if *self != Self::Reset {
            *self = Self::Draining(Self::make_close_frame(error, frame_type, message));
        }
    }

    /// If a close is pending, take a frame.
    pub fn close_frame(&mut self) -> Option<ClosingFrame> {
        match self {
            Self::Closing(frame) => {
                // When we are closing, we might need to send the close frame again.
                let frame = mem::replace(frame, Frame::Padding);
                *self = Self::CloseSent(Some(frame.clone()));
                Some(frame)
            }
            Self::Draining(frame) => {
                // When we are draining, just send once.
                let frame = mem::replace(frame, Frame::Padding);
                *self = Self::CloseSent(None);
                Some(frame)
            }
            _ => None,
        }
    }

    /// If a close can be sent again, prepare to send it again.
    pub fn send_close(&mut self) {
        if let Self::CloseSent(Some(frame)) = self {
            let frame = mem::replace(frame, Frame::Padding);
            *self = Self::Closing(frame);
        }
    }

    /// We just got a stateless reset.  Terminate.
    pub fn reset(&mut self) {
        *self = Self::Reset;
    }
}
