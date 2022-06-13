// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use neqo_common::Encoder;
use std::cmp::{min, Ordering};
use std::mem;
use std::rc::Rc;
use std::time::Instant;

use crate::frame::{
    FrameType, FRAME_TYPE_CONNECTION_CLOSE_APPLICATION, FRAME_TYPE_CONNECTION_CLOSE_TRANSPORT,
    FRAME_TYPE_HANDSHAKE_DONE,
};
use crate::packet::PacketBuilder;
use crate::path::PathRef;
use crate::recovery::RecoveryToken;
use crate::{ConnectionError, Error, Res};

#[derive(Clone, Debug, PartialEq, Eq)]
/// The state of the Connection.
pub enum State {
    /// A newly created connection.
    Init,
    /// Waiting for the first Initial packet.
    WaitInitial,
    /// Waiting to confirm which version was selected.
    /// For a client, this is confirmed when a CRYPTO frame is received;
    /// the version of the packet determines the version.
    /// For a server, this is confirmed when transport parameters are
    /// received and processed.
    WaitVersion,
    /// Exchanging Handshake packets.
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
        matches!(
            self,
            Self::Closing { .. } | Self::Draining { .. } | Self::Closed(_)
        )
    }

    pub fn error(&self) -> Option<&ConnectionError> {
        if let Self::Closing { error, .. } | Self::Draining { error, .. } | Self::Closed(error) =
            self
        {
            Some(error)
        } else {
            None
        }
    }
}

// Implement `PartialOrd` so that we can enforce monotonic state progression.
impl PartialOrd for State {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for State {
    fn cmp(&self, other: &Self) -> Ordering {
        if mem::discriminant(self) == mem::discriminant(other) {
            return Ordering::Equal;
        }
        #[allow(clippy::match_same_arms)] // Lint bug: rust-lang/rust-clippy#860
        match (self, other) {
            (Self::Init, _) => Ordering::Less,
            (_, Self::Init) => Ordering::Greater,
            (Self::WaitInitial, _) => Ordering::Less,
            (_, Self::WaitInitial) => Ordering::Greater,
            (Self::WaitVersion, _) => Ordering::Less,
            (_, Self::WaitVersion) => Ordering::Greater,
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
        }
    }
}

#[derive(Debug, Clone)]
pub struct ClosingFrame {
    path: PathRef,
    error: ConnectionError,
    frame_type: FrameType,
    reason_phrase: Vec<u8>,
}

impl ClosingFrame {
    fn new(
        path: PathRef,
        error: ConnectionError,
        frame_type: FrameType,
        message: impl AsRef<str>,
    ) -> Self {
        let reason_phrase = message.as_ref().as_bytes().to_vec();
        Self {
            path,
            error,
            frame_type,
            reason_phrase,
        }
    }

    pub fn path(&self) -> &PathRef {
        &self.path
    }

    pub fn sanitize(&self) -> Option<Self> {
        if let ConnectionError::Application(_) = self.error {
            // The default CONNECTION_CLOSE frame that is sent when an application
            // error code needs to be sent in an Initial or Handshake packet.
            Some(Self {
                path: Rc::clone(&self.path),
                error: ConnectionError::Transport(Error::ApplicationError),
                frame_type: 0,
                reason_phrase: Vec::new(),
            })
        } else {
            None
        }
    }

    pub fn write_frame(&self, builder: &mut PacketBuilder) {
        // Allow 8 bytes for the reason phrase to ensure that if it needs to be
        // truncated there is still at least a few bytes of the value.
        if builder.remaining() < 1 + 8 + 8 + 2 + 8 {
            return;
        }
        match &self.error {
            ConnectionError::Transport(e) => {
                builder.encode_varint(FRAME_TYPE_CONNECTION_CLOSE_TRANSPORT);
                builder.encode_varint(e.code());
                builder.encode_varint(self.frame_type);
            }
            ConnectionError::Application(code) => {
                builder.encode_varint(FRAME_TYPE_CONNECTION_CLOSE_APPLICATION);
                builder.encode_varint(*code);
            }
        }
        // Truncate the reason phrase if it doesn't fit.  As we send this frame in
        // multiple packet number spaces, limit the overall size to 256.
        let available = min(256, builder.remaining());
        let reason = if available < Encoder::vvec_len(self.reason_phrase.len()) {
            &self.reason_phrase[..available - 2]
        } else {
            &self.reason_phrase
        };
        builder.encode_vvec(reason);
    }
}

/// `StateSignaling` manages whether we need to send HANDSHAKE_DONE and CONNECTION_CLOSE.
/// Valid state transitions are:
/// * Idle -> HandshakeDone: at the server when the handshake completes
/// * HandshakeDone -> Idle: when a HANDSHAKE_DONE frame is sent
/// * Idle/HandshakeDone -> Closing/Draining: when closing or draining
/// * Closing/Draining -> CloseSent: after sending CONNECTION_CLOSE
/// * CloseSent -> Closing: any time a new CONNECTION_CLOSE is needed
/// * -> Reset: from any state in case of a stateless reset
#[derive(Debug, Clone)]
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
        if !matches!(self, Self::Idle) {
            debug_assert!(false, "StateSignaling must be in Idle state.");
            return;
        }
        *self = Self::HandshakeDone
    }

    pub fn write_done(&mut self, builder: &mut PacketBuilder) -> Res<Option<RecoveryToken>> {
        if matches!(self, Self::HandshakeDone) && builder.remaining() >= 1 {
            *self = Self::Idle;
            builder.encode_varint(FRAME_TYPE_HANDSHAKE_DONE);
            if builder.len() > builder.limit() {
                return Err(Error::InternalError(14));
            }
            Ok(Some(RecoveryToken::HandshakeDone))
        } else {
            Ok(None)
        }
    }

    pub fn close(
        &mut self,
        path: PathRef,
        error: ConnectionError,
        frame_type: FrameType,
        message: impl AsRef<str>,
    ) {
        if !matches!(self, Self::Reset) {
            *self = Self::Closing(ClosingFrame::new(path, error, frame_type, message));
        }
    }

    pub fn drain(
        &mut self,
        path: PathRef,
        error: ConnectionError,
        frame_type: FrameType,
        message: impl AsRef<str>,
    ) {
        if !matches!(self, Self::Reset) {
            *self = Self::Draining(ClosingFrame::new(path, error, frame_type, message));
        }
    }

    /// If a close is pending, take a frame.
    pub fn close_frame(&mut self) -> Option<ClosingFrame> {
        match self {
            Self::Closing(frame) => {
                // When we are closing, we might need to send the close frame again.
                let res = Some(frame.clone());
                *self = Self::CloseSent(Some(frame.clone()));
                res
            }
            Self::Draining(frame) => {
                // When we are draining, just send once.
                let res = Some(frame.clone());
                *self = Self::CloseSent(None);
                res
            }
            _ => None,
        }
    }

    /// If a close can be sent again, prepare to send it again.
    pub fn send_close(&mut self) {
        if let Self::CloseSent(Some(frame)) = self {
            *self = Self::Closing(frame.clone());
        }
    }

    /// We just got a stateless reset.  Terminate.
    pub fn reset(&mut self) {
        *self = Self::Reset;
    }
}
