// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Collecting a list of events relevant to whoever is using the Connection.

use std::{cell::RefCell, collections::VecDeque, rc::Rc};

use neqo_common::event::Provider as EventProvider;
use neqo_crypto::ResumptionToken;

use crate::{
    connection::State,
    quic_datagrams::DatagramTracking,
    stream_id::{StreamId, StreamType},
    AppError, Stats,
};

#[derive(Debug, PartialOrd, Ord, PartialEq, Eq)]
pub enum OutgoingDatagramOutcome {
    DroppedTooBig,
    DroppedQueueFull,
    Lost,
    Acked,
}

#[derive(Debug, PartialOrd, Ord, PartialEq, Eq)]
pub enum ConnectionEvent {
    /// Cert authentication needed
    AuthenticationNeeded,
    /// Encrypted client hello fallback occurred.  The certificate for the
    /// public name needs to be authenticated.
    EchFallbackAuthenticationNeeded {
        public_name: String,
    },
    /// A new uni (read) or bidi stream has been opened by the peer.
    NewStream {
        stream_id: StreamId,
    },
    /// Space available in the buffer for an application write to succeed.
    SendStreamWritable {
        stream_id: StreamId,
    },
    /// New bytes available for reading.
    RecvStreamReadable {
        stream_id: StreamId,
    },
    /// Peer reset the stream.
    RecvStreamReset {
        stream_id: StreamId,
        app_error: AppError,
    },
    /// Peer has sent STOP_SENDING
    SendStreamStopSending {
        stream_id: StreamId,
        app_error: AppError,
    },
    /// Peer has acked everything sent on the stream.
    SendStreamComplete {
        stream_id: StreamId,
    },
    /// Peer increased MAX_STREAMS
    SendStreamCreatable {
        stream_type: StreamType,
    },
    /// Connection state change.
    StateChange(State),
    /// The server rejected 0-RTT.
    /// This event invalidates all state in streams that has been created.
    /// Any data written to streams needs to be written again.
    ZeroRttRejected,
    ResumptionToken(ResumptionToken),
    Datagram(Vec<u8>),
    OutgoingDatagramOutcome {
        id: u64,
        outcome: OutgoingDatagramOutcome,
    },
    IncomingDatagramDropped,
}

#[derive(Debug, Default, Clone)]
#[allow(clippy::module_name_repetitions)]
pub struct ConnectionEvents {
    events: Rc<RefCell<VecDeque<ConnectionEvent>>>,
}

impl ConnectionEvents {
    pub fn authentication_needed(&self) {
        self.insert(ConnectionEvent::AuthenticationNeeded);
    }

    pub fn ech_fallback_authentication_needed(&self, public_name: String) {
        self.insert(ConnectionEvent::EchFallbackAuthenticationNeeded { public_name });
    }

    pub fn new_stream(&self, stream_id: StreamId) {
        self.insert(ConnectionEvent::NewStream { stream_id });
    }

    pub fn recv_stream_readable(&self, stream_id: StreamId) {
        self.insert(ConnectionEvent::RecvStreamReadable { stream_id });
    }

    pub fn recv_stream_reset(&self, stream_id: StreamId, app_error: AppError) {
        // If reset, no longer readable.
        self.remove(|evt| matches!(evt, ConnectionEvent::RecvStreamReadable { stream_id: x } if *x == stream_id.as_u64()));

        self.insert(ConnectionEvent::RecvStreamReset {
            stream_id,
            app_error,
        });
    }

    pub fn send_stream_writable(&self, stream_id: StreamId) {
        self.insert(ConnectionEvent::SendStreamWritable { stream_id });
    }

    pub fn send_stream_stop_sending(&self, stream_id: StreamId, app_error: AppError) {
        // If stopped, no longer writable.
        self.remove(|evt| matches!(evt, ConnectionEvent::SendStreamWritable { stream_id: x } if *x == stream_id));

        self.insert(ConnectionEvent::SendStreamStopSending {
            stream_id,
            app_error,
        });
    }

    pub fn send_stream_complete(&self, stream_id: StreamId) {
        self.remove(|evt| matches!(evt, ConnectionEvent::SendStreamWritable { stream_id: x } if *x == stream_id));

        self.remove(|evt| matches!(evt, ConnectionEvent::SendStreamStopSending { stream_id: x, .. } if *x == stream_id.as_u64()));

        self.insert(ConnectionEvent::SendStreamComplete { stream_id });
    }

    pub fn send_stream_creatable(&self, stream_type: StreamType) {
        self.insert(ConnectionEvent::SendStreamCreatable { stream_type });
    }

    pub fn connection_state_change(&self, state: State) {
        // If closing, existing events no longer relevant.
        match state {
            State::Closing { .. } | State::Closed(_) => self.events.borrow_mut().clear(),
            _ => (),
        }
        self.insert(ConnectionEvent::StateChange(state));
    }

    pub fn client_resumption_token(&self, token: ResumptionToken) {
        self.insert(ConnectionEvent::ResumptionToken(token));
    }

    pub fn client_0rtt_rejected(&self) {
        // If 0rtt rejected, must start over and existing events are no longer
        // relevant.
        self.events.borrow_mut().clear();
        self.insert(ConnectionEvent::ZeroRttRejected);
    }

    pub fn recv_stream_complete(&self, stream_id: StreamId) {
        // If stopped, no longer readable.
        self.remove(|evt| matches!(evt, ConnectionEvent::RecvStreamReadable { stream_id: x } if *x == stream_id.as_u64()));
    }

    // The number of datagrams in the events queue is limited to max_queued_datagrams.
    // This function ensure this and deletes the oldest datagrams if needed.
    fn check_datagram_queued(&self, max_queued_datagrams: usize, stats: &mut Stats) {
        let mut q = self.events.borrow_mut();
        let mut remove = None;
        if q.iter()
            .filter(|evt| matches!(evt, ConnectionEvent::Datagram(_)))
            .count()
            == max_queued_datagrams
        {
            if let Some(d) = q
                .iter()
                .rev()
                .enumerate()
                .filter(|(_, evt)| matches!(evt, ConnectionEvent::Datagram(_)))
                .take(1)
                .next()
            {
                remove = Some(d.0);
            }
        }
        if let Some(r) = remove {
            q.remove(r);
            q.push_back(ConnectionEvent::IncomingDatagramDropped);
            stats.incoming_datagram_dropped += 1;
        }
    }

    pub fn add_datagram(&self, max_queued_datagrams: usize, data: &[u8], stats: &mut Stats) {
        self.check_datagram_queued(max_queued_datagrams, stats);
        self.events
            .borrow_mut()
            .push_back(ConnectionEvent::Datagram(data.to_vec()));
    }

    pub fn datagram_outcome(
        &self,
        dgram_tracker: &DatagramTracking,
        outcome: OutgoingDatagramOutcome,
    ) {
        if let DatagramTracking::Id(id) = dgram_tracker {
            self.events
                .borrow_mut()
                .push_back(ConnectionEvent::OutgoingDatagramOutcome { id: *id, outcome });
        }
    }

    fn insert(&self, event: ConnectionEvent) {
        let mut q = self.events.borrow_mut();

        // Special-case two enums that are not strictly PartialEq equal but that
        // we wish to avoid inserting duplicates.
        let already_present = match &event {
            ConnectionEvent::SendStreamStopSending { stream_id, .. } => q.iter().any(|evt| {
                matches!(evt, ConnectionEvent::SendStreamStopSending { stream_id: x, .. }
		                    if *x == *stream_id)
            }),
            ConnectionEvent::RecvStreamReset { stream_id, .. } => q.iter().any(|evt| {
                matches!(evt, ConnectionEvent::RecvStreamReset { stream_id: x, .. }
		                    if *x == *stream_id)
            }),
            _ => q.contains(&event),
        };
        if !already_present {
            q.push_back(event);
        }
    }

    fn remove<F>(&self, f: F)
    where
        F: Fn(&ConnectionEvent) -> bool,
    {
        self.events.borrow_mut().retain(|evt| !f(evt));
    }
}

impl EventProvider for ConnectionEvents {
    type Event = ConnectionEvent;

    fn has_events(&self) -> bool {
        !self.events.borrow().is_empty()
    }

    fn next_event(&mut self) -> Option<Self::Event> {
        self.events.borrow_mut().pop_front()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{ConnectionError, Error};

    #[test]
    fn event_culling() {
        let mut evts = ConnectionEvents::default();

        evts.client_0rtt_rejected();
        evts.client_0rtt_rejected();
        assert_eq!(evts.events().count(), 1);
        assert_eq!(evts.events().count(), 0);

        evts.new_stream(4.into());
        evts.new_stream(4.into());
        assert_eq!(evts.events().count(), 1);

        evts.recv_stream_readable(6.into());
        evts.recv_stream_reset(6.into(), 66);
        evts.recv_stream_reset(6.into(), 65);
        assert_eq!(evts.events().count(), 1);

        evts.send_stream_writable(8.into());
        evts.send_stream_writable(8.into());
        evts.send_stream_stop_sending(8.into(), 55);
        evts.send_stream_stop_sending(8.into(), 56);
        let events = evts.events().collect::<Vec<_>>();
        assert_eq!(events.len(), 1);
        assert_eq!(
            events[0],
            ConnectionEvent::SendStreamStopSending {
                stream_id: StreamId::new(8),
                app_error: 55
            }
        );

        evts.send_stream_writable(8.into());
        evts.send_stream_writable(8.into());
        evts.send_stream_stop_sending(8.into(), 55);
        evts.send_stream_stop_sending(8.into(), 56);
        evts.send_stream_complete(8.into());
        assert_eq!(evts.events().count(), 1);

        evts.send_stream_writable(8.into());
        evts.send_stream_writable(9.into());
        evts.send_stream_stop_sending(10.into(), 55);
        evts.send_stream_stop_sending(11.into(), 56);
        evts.send_stream_complete(12.into());
        assert_eq!(evts.events().count(), 5);

        evts.send_stream_writable(8.into());
        evts.send_stream_writable(9.into());
        evts.send_stream_stop_sending(10.into(), 55);
        evts.send_stream_stop_sending(11.into(), 56);
        evts.send_stream_complete(12.into());
        evts.client_0rtt_rejected();
        assert_eq!(evts.events().count(), 1);

        evts.send_stream_writable(9.into());
        evts.send_stream_stop_sending(10.into(), 55);
        evts.connection_state_change(State::Closed(ConnectionError::Transport(
            Error::StreamStateError,
        )));
        assert_eq!(evts.events().count(), 1);
    }
}
