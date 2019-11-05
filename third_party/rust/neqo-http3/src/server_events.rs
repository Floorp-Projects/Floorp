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
pub enum Http3ServerEvent {
    /// A stream can accept new data.
    DataWritable { stream_id: u64 },
    /// Peer reset the stream.
    Reset { stream_id: u64, error: AppError },
    /// New stream can be created
    RequestsCreatable,
    /// Connection state change.
    StateChange(Http3State),
}

#[derive(Debug, Default, Clone)]
pub struct Http3ServerEvents {
    events: Rc<RefCell<BTreeSet<Http3ServerEvent>>>,
}

impl Http3ServerEvents {
    fn insert(&self, event: Http3ServerEvent) {
        self.events.borrow_mut().insert(event);
    }

    pub fn remove(&self, event: &Http3ServerEvent) -> bool {
        self.events.borrow_mut().remove(event)
    }
}

impl Http3Events for Http3ServerEvents {
    fn data_writable(&self, stream_id: u64) {
        self.insert(Http3ServerEvent::DataWritable { stream_id });
    }

    fn reset(&self, stream_id: u64, error: AppError) {
        self.insert(Http3ServerEvent::Reset { stream_id, error });
    }

    fn new_requests_creatable(&self, _stream_type: StreamType) {
        self.insert(Http3ServerEvent::RequestsCreatable);
    }

    fn connection_state_change(&self, state: Http3State) {
        self.insert(Http3ServerEvent::StateChange(state));
    }

    fn remove_events_for_stream_id(&self, remove_stream_id: u64) {
        let events_to_remove = self
            .events
            .borrow()
            .iter()
            .filter(|evt| match evt {
                Http3ServerEvent::DataWritable { stream_id }
                | Http3ServerEvent::Reset { stream_id, .. } => *stream_id == remove_stream_id,
                _ => false,
            })
            .cloned()
            .collect::<SmallVec<[_; 8]>>();

        for evt in events_to_remove {
            self.remove(&evt);
        }
    }
}
