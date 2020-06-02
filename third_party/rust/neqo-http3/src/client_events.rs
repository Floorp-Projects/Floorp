// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::module_name_repetitions)]

use crate::connection::Http3State;
use crate::recv_message::RecvMessageEvents;
use crate::send_message::SendMessageEvents;
use crate::Header;
use neqo_common::matches;
use neqo_transport::{AppError, StreamType};

use std::cell::RefCell;
use std::collections::VecDeque;
use std::rc::Rc;

#[derive(Debug, PartialOrd, Ord, PartialEq, Eq, Clone)]
pub enum Http3ClientEvent {
    /// Space available in the buffer for an application write to succeed.
    HeaderReady {
        stream_id: u64,
        headers: Option<Vec<Header>>,
        fin: bool,
    },
    /// A stream can accept new data.
    DataWritable { stream_id: u64 },
    /// New bytes available for reading.
    DataReadable { stream_id: u64 },
    /// Peer reset the stream.
    Reset { stream_id: u64, error: AppError },
    /// Peer has send STOP_SENDING with error code EarlyResponse, other error will post a reset event.
    StopSending { stream_id: u64, error: AppError },
    ///A new push stream
    NewPushStream { stream_id: u64 },
    /// New stream can be created
    RequestsCreatable,
    /// Cert authentication needed
    AuthenticationNeeded,
    /// Zero Rtt has been rejected.
    ZeroRttRejected,
    /// Client has received a GOAWAY frame
    GoawayReceived,
    /// Connection state change.
    StateChange(Http3State),
}

#[derive(Debug, Default, Clone)]
pub struct Http3ClientEvents {
    events: Rc<RefCell<VecDeque<Http3ClientEvent>>>,
}

impl RecvMessageEvents for Http3ClientEvents {
    /// Add a new `HeaderReady` event.
    fn header_ready(&self, stream_id: u64, headers: Option<Vec<Header>>, fin: bool) {
        self.insert(Http3ClientEvent::HeaderReady {
            stream_id,
            headers,
            fin,
        });
    }

    /// Add a new `DataReadable` event
    fn data_readable(&self, stream_id: u64) {
        self.insert(Http3ClientEvent::DataReadable { stream_id });
    }
}

impl SendMessageEvents for Http3ClientEvents {
    /// Add a new `DataWritable` event.
    fn data_writable(&self, stream_id: u64) {
        self.insert(Http3ClientEvent::DataWritable { stream_id });
    }
}

impl Http3ClientEvents {
    /// Add a new `StopSending` event
    pub(crate) fn stop_sending(&self, stream_id: u64, error: AppError) {
        // Remove DataWritable event if any.
        self.remove(|evt| {
            matches!(evt, Http3ClientEvent::DataWritable {
                    stream_id: x } if *x == stream_id)
        });
        self.insert(Http3ClientEvent::StopSending { stream_id, error });
    }

    // TODO: implement push.
    // pub fn new_push_stream(&self, stream_id: u64) {
    //     self.insert(Http3ClientEvent::NewPushStream { stream_id });
    // }

    /// Add a new `RequestCreatable` event
    pub(crate) fn new_requests_creatable(&self, stream_type: StreamType) {
        if stream_type == StreamType::BiDi {
            self.insert(Http3ClientEvent::RequestsCreatable);
        }
    }

    /// Add a new `AuthenticationNeeded` event
    pub(crate) fn authentication_needed(&self) {
        self.insert(Http3ClientEvent::AuthenticationNeeded);
    }

    /// Add a new `ZeroRttRejected` event.
    pub(crate) fn zero_rtt_rejected(&self) {
        self.insert(Http3ClientEvent::ZeroRttRejected);
    }

    /// Add a new `GoawayReceived` event.
    pub(crate) fn goaway_received(&self) {
        self.remove(|evt| matches!(evt, Http3ClientEvent::RequestsCreatable));
        self.insert(Http3ClientEvent::GoawayReceived);
    }

    /// Take all events currently in the queue.
    pub(crate) fn events(&self) -> impl Iterator<Item = Http3ClientEvent> {
        self.events.replace(VecDeque::new()).into_iter()
    }

    /// Check if there is any event present.
    pub fn has_events(&self) -> bool {
        !self.events.borrow().is_empty()
    }

    /// Take the first event.
    pub fn next_event(&self) -> Option<Http3ClientEvent> {
        self.events.borrow_mut().pop_front()
    }

    fn insert(&self, event: Http3ClientEvent) {
        self.events.borrow_mut().push_back(event);
    }

    fn remove<F>(&self, f: F)
    where
        F: Fn(&Http3ClientEvent) -> bool,
    {
        self.events.borrow_mut().retain(|evt| !f(evt))
    }

    /// Add a new `Reset` event.
    pub(crate) fn reset(&self, stream_id: u64, error: AppError) {
        self.remove_events_for_stream_id(stream_id);
        self.insert(Http3ClientEvent::Reset { stream_id, error });
    }

    /// Add a new `StateChange` event.
    pub(crate) fn connection_state_change(&self, state: Http3State) {
        // If closing, existing events no longer relevant.
        match state {
            Http3State::Closing { .. } | Http3State::Closed(_) => self.events.borrow_mut().clear(),
            _ => (),
        }
        self.insert(Http3ClientEvent::StateChange(state));
    }

    /// Remove all events for a stream
    pub(crate) fn remove_events_for_stream_id(&self, stream_id: u64) {
        self.remove(|evt| {
            matches!(evt,
                Http3ClientEvent::HeaderReady { stream_id: x, .. }
                | Http3ClientEvent::DataWritable { stream_id: x }
                | Http3ClientEvent::DataReadable { stream_id: x }
                | Http3ClientEvent::NewPushStream { stream_id: x }
                | Http3ClientEvent::Reset { stream_id: x, .. }
                | Http3ClientEvent::StopSending { stream_id: x, .. } if *x == stream_id)
        });
    }
}
