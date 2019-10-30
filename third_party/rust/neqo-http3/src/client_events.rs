// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::connection::{Http3Events, Http3State};
use neqo_transport::{AppError, StreamType};

use smallvec::SmallVec;
use std::cell::RefCell;
use std::collections::BTreeSet;
use std::rc::Rc;

#[derive(Debug, PartialOrd, Ord, PartialEq, Eq, Clone)]
pub enum Http3ClientEvent {
    /// Space available in the buffer for an application write to succeed.
    HeaderReady { stream_id: u64 },
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
    /// Client has received a GOAWAY frame
    GoawayReceived,
    /// Connection state change.
    StateChange(Http3State),
}

#[derive(Debug, Default, Clone)]
pub struct Http3ClientEvents {
    events: Rc<RefCell<BTreeSet<Http3ClientEvent>>>,
}

impl Http3ClientEvents {
    pub fn header_ready(&self, stream_id: u64) {
        self.insert(Http3ClientEvent::HeaderReady { stream_id });
    }

    pub fn data_readable(&self, stream_id: u64) {
        self.insert(Http3ClientEvent::DataReadable { stream_id });
    }

    pub fn stop_sending(&self, stream_id: u64, error: AppError) {
        self.insert(Http3ClientEvent::StopSending { stream_id, error });
    }

    // TODO: implement push.
    // pub fn new_push_stream(&self, stream_id: u64) {
    //     self.insert(Http3ClientEvent::NewPushStream { stream_id });
    // }

    pub fn authentication_needed(&self) {
        self.insert(Http3ClientEvent::AuthenticationNeeded);
    }

    pub fn goaway_received(&self) {
        self.insert(Http3ClientEvent::GoawayReceived);
    }

    pub fn events(&self) -> impl Iterator<Item = Http3ClientEvent> {
        self.events.replace(BTreeSet::new()).into_iter()
    }

    fn insert(&self, event: Http3ClientEvent) {
        self.events.borrow_mut().insert(event);
    }

    pub fn remove(&self, event: &Http3ClientEvent) -> bool {
        self.events.borrow_mut().remove(event)
    }
}

impl Http3Events for Http3ClientEvents {
    fn data_writable(&self, stream_id: u64) {
        self.insert(Http3ClientEvent::DataWritable { stream_id });
    }

    fn reset(&self, stream_id: u64, error: AppError) {
        self.insert(Http3ClientEvent::Reset { stream_id, error });
    }

    fn new_requests_creatable(&self, stream_type: StreamType) {
        if stream_type == StreamType::BiDi {
            self.insert(Http3ClientEvent::RequestsCreatable);
        }
    }

    fn connection_state_change(&self, state: Http3State) {
        self.insert(Http3ClientEvent::StateChange(state));
    }

    fn remove_events_for_stream_id(&self, remove_stream_id: u64) {
        let events_to_remove = self
            .events
            .borrow()
            .iter()
            .filter(|evt| match evt {
                Http3ClientEvent::HeaderReady { stream_id }
                | Http3ClientEvent::DataWritable { stream_id }
                | Http3ClientEvent::DataReadable { stream_id }
                | Http3ClientEvent::NewPushStream { stream_id }
                | Http3ClientEvent::Reset { stream_id, .. }
                | Http3ClientEvent::StopSending { stream_id, .. } => *stream_id == remove_stream_id,
                _ => false,
            })
            .cloned()
            .collect::<SmallVec<[_; 8]>>();

        for evt in events_to_remove {
            self.remove(&evt);
        }
    }
}
