// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// https://datatracker.ietf.org/doc/html/draft-ietf-quic-datagram

use crate::frame::{FRAME_TYPE_DATAGRAM, FRAME_TYPE_DATAGRAM_WITH_LEN};
use crate::packet::PacketBuilder;
use crate::recovery::RecoveryToken;
use crate::{events::OutgoingDatagramOutcome, ConnectionEvents, Error, Res, Stats};
use neqo_common::Encoder;
use std::cmp::min;
use std::collections::VecDeque;
use std::convert::TryFrom;

pub const MAX_QUIC_DATAGRAM: u64 = 65535;

#[derive(Debug, Clone, Copy)]
pub enum DatagramTracking {
    None,
    Id(u64),
}

impl From<Option<u64>> for DatagramTracking {
    fn from(v: Option<u64>) -> Self {
        match v {
            Some(id) => Self::Id(id),
            None => Self::None,
        }
    }
}

impl From<DatagramTracking> for Option<u64> {
    fn from(v: DatagramTracking) -> Self {
        match v {
            DatagramTracking::Id(id) => Some(id),
            DatagramTracking::None => None,
        }
    }
}

struct QuicDatagram {
    data: Vec<u8>,
    tracking: DatagramTracking,
}

impl QuicDatagram {
    fn tracking(&self) -> &DatagramTracking {
        &self.tracking
    }
}

impl AsRef<[u8]> for QuicDatagram {
    #[must_use]
    fn as_ref(&self) -> &[u8] {
        &self.data[..]
    }
}

pub struct QuicDatagrams {
    /// The max size of a datagram that would be acceptable.
    local_datagram_size: u64,
    /// The max size of a datagram that would be acceptable by the peer.
    remote_datagram_size: u64,
    max_queued_outgoing_datagrams: usize,
    /// The max number of datagrams that will be queued in connection events.
    /// If the number is exceeded, the oldest datagram will be dropped.
    max_queued_incoming_datagrams: usize,
    /// Datagram queued for sending.
    datagrams: VecDeque<QuicDatagram>,
    conn_events: ConnectionEvents,
}

impl QuicDatagrams {
    pub fn new(
        local_datagram_size: u64,
        max_queued_outgoing_datagrams: usize,
        max_queued_incoming_datagrams: usize,
        conn_events: ConnectionEvents,
    ) -> Self {
        Self {
            local_datagram_size,
            remote_datagram_size: 0,
            max_queued_outgoing_datagrams,
            max_queued_incoming_datagrams,
            datagrams: VecDeque::with_capacity(max_queued_outgoing_datagrams),
            conn_events,
        }
    }

    pub fn remote_datagram_size(&self) -> u64 {
        self.remote_datagram_size
    }

    pub fn set_remote_datagram_size(&mut self, v: u64) {
        self.remote_datagram_size = min(v, MAX_QUIC_DATAGRAM);
    }

    /// This function tries to write a datagram frame into a packet.
    /// If the frame does not fit into the packet, the datagram will
    /// be dropped and a DatagramLost event will be posted.
    pub fn write_frames(
        &mut self,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut Stats,
    ) {
        while let Some(dgram) = self.datagrams.pop_front() {
            let len = dgram.as_ref().len();
            if builder.remaining() > len {
                // We need 1 more than `len` for the Frame type.
                let length_len = Encoder::varint_len(u64::try_from(len).unwrap());
                // Include a length if there is space for another frame after this one.
                if builder.remaining() >= 1 + length_len + len + PacketBuilder::MINIMUM_FRAME_SIZE {
                    builder.encode_varint(FRAME_TYPE_DATAGRAM_WITH_LEN);
                    builder.encode_vvec(dgram.as_ref());
                } else {
                    builder.encode_varint(FRAME_TYPE_DATAGRAM);
                    builder.encode(dgram.as_ref());
                    builder.mark_full();
                }
                debug_assert!(builder.len() <= builder.limit());
                stats.frame_tx.datagram += 1;
                tokens.push(RecoveryToken::Datagram(*dgram.tracking()));
            } else if tokens.is_empty() {
                // If the packet is empty, except packet headers, and the
                // datagram cannot fit, drop it.
                // Also continue trying to write the next QuicDatagram.
                self.conn_events
                    .datagram_outcome(dgram.tracking(), OutgoingDatagramOutcome::DroppedTooBig);
                stats.datagram_tx.dropped_too_big += 1;
            } else {
                self.datagrams.push_front(dgram);
                // Try later on an empty packet.
                return;
            }
        }
    }

    /// Returns true if there was an unsent datagram that has been dismissed.
    /// # Error
    /// The function returns `TooMuchData` if the supply buffer is bigger than
    /// the allowed remote datagram size. The funcion does not check if the
    /// datagram can fit into a packet (i.e. MTU limit). This is checked during
    /// creation of an actual packet and the datagram will be dropped if it does
    /// not fit into the packet.
    pub fn add_datagram(
        &mut self,
        buf: &[u8],
        tracking: DatagramTracking,
        stats: &mut Stats,
    ) -> Res<()> {
        if u64::try_from(buf.len()).unwrap() > self.remote_datagram_size {
            return Err(Error::TooMuchData);
        }
        if self.datagrams.len() == self.max_queued_outgoing_datagrams {
            self.conn_events.datagram_outcome(
                self.datagrams.pop_front().unwrap().tracking(),
                OutgoingDatagramOutcome::DroppedQueueFull,
            );
            stats.datagram_tx.dropped_queue_full += 1;
        }
        self.datagrams.push_back(QuicDatagram {
            data: buf.to_vec(),
            tracking,
        });
        Ok(())
    }

    pub fn handle_datagram(&self, data: &[u8], stats: &mut Stats) -> Res<()> {
        if self.local_datagram_size < u64::try_from(data.len()).unwrap() {
            return Err(Error::ProtocolViolation);
        }
        self.conn_events
            .add_datagram(self.max_queued_incoming_datagrams, data, stats);
        Ok(())
    }
}
