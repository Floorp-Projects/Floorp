// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::module_name_repetitions)]

use crate::connection::Http3State;
use crate::send_message::SendMessageEvents;
use crate::Header;
use crate::RecvMessageEvents;

use neqo_transport::{AppError, StreamType};

use std::cell::RefCell;
use std::collections::VecDeque;
use std::rc::Rc;

#[derive(Debug, PartialOrd, Ord, PartialEq, Eq, Clone)]
pub enum Http3ClientEvent {
    /// Response headers are received.
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
    /// Peer has sent a STOP_SENDING.
    StopSending { stream_id: u64, error: AppError },
    /// A new push promise.
    PushPromise {
        push_id: u64,
        request_stream_id: u64,
        headers: Vec<Header>,
    },
    /// A push response headers are ready.
    PushHeaderReady {
        push_id: u64,
        headers: Option<Vec<Header>>,
        fin: bool,
    },
    /// New bytes are available on a push stream for reading.
    PushDataReadable { push_id: u64 },
    /// A push has been canceled.
    PushCanceled { push_id: u64 },
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

    /// Add a new `Reset` event.
    fn reset(&self, stream_id: u64, error: AppError) {
        self.remove(|evt| {
            matches!(evt,
                Http3ClientEvent::HeaderReady { stream_id: x, .. }
                | Http3ClientEvent::DataReadable { stream_id: x }
                | Http3ClientEvent::PushPromise { request_stream_id: x, .. }
                | Http3ClientEvent::Reset { stream_id: x, .. } if *x == stream_id)
        });
        self.insert(Http3ClientEvent::Reset { stream_id, error });
    }
}

impl SendMessageEvents for Http3ClientEvents {
    /// Add a new `DataWritable` event.
    fn data_writable(&self, stream_id: u64) {
        self.insert(Http3ClientEvent::DataWritable { stream_id });
    }

    fn remove_send_side_event(&self, stream_id: u64) {
        self.remove(|evt| {
            matches!(evt,
                Http3ClientEvent::DataWritable { stream_id: x }
                | Http3ClientEvent::StopSending { stream_id: x, .. } if *x == stream_id)
        });
    }

    /// Add a new `StopSending` event
    fn stop_sending(&self, stream_id: u64, error: AppError) {
        // The stream has received a STOP_SENDING frame, we should remove any DataWritable event.
        self.remove_send_side_event(stream_id);
        self.insert(Http3ClientEvent::StopSending { stream_id, error });
    }
}

impl Http3ClientEvents {
    pub fn push_promise(&self, push_id: u64, request_stream_id: u64, headers: Vec<Header>) {
        self.insert(Http3ClientEvent::PushPromise {
            push_id,
            request_stream_id,
            headers,
        });
    }

    pub fn push_canceled(&self, push_id: u64) {
        self.remove_events_for_push_id(push_id);
        self.insert(Http3ClientEvent::PushCanceled { push_id });
    }

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

    pub fn insert(&self, event: Http3ClientEvent) {
        self.events.borrow_mut().push_back(event);
    }

    fn remove<F>(&self, f: F)
    where
        F: Fn(&Http3ClientEvent) -> bool,
    {
        self.events.borrow_mut().retain(|evt| !f(evt))
    }

    /// Add a new `StateChange` event.
    pub(crate) fn connection_state_change(&self, state: Http3State) {
        match state {
            // If closing, existing events no longer relevant.
            Http3State::Closing { .. } | Http3State::Closed(_) => self.events.borrow_mut().clear(),
            Http3State::Connected => {
                self.remove(|evt| {
                    matches!(evt, Http3ClientEvent::StateChange(Http3State::ZeroRtt))
                });
            }
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
                | Http3ClientEvent::PushPromise { request_stream_id: x, .. }
                | Http3ClientEvent::Reset { stream_id: x, .. }
                | Http3ClientEvent::StopSending { stream_id: x, .. } if *x == stream_id)
        });
    }

    pub fn has_push(&self, push_id: u64) -> bool {
        for iter in self.events.borrow().iter() {
            if matches!(iter, Http3ClientEvent::PushPromise{push_id:x, ..} if *x == push_id) {
                return true;
            }
        }
        false
    }

    pub fn remove_events_for_push_id(&self, push_id: u64) {
        self.remove(|evt| {
            matches!(evt,
                Http3ClientEvent::PushPromise{ push_id: x, .. }
                | Http3ClientEvent::PushHeaderReady{ push_id: x, .. }
                | Http3ClientEvent::PushDataReadable{ push_id: x, .. }
                | Http3ClientEvent::PushCanceled{ push_id: x, .. } if *x == push_id)
        });
    }
}
