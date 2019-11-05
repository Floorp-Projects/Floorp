// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::connection::{Http3Events, Http3State};
use crate::Header;
use neqo_common::matches;
use neqo_transport::AppError;

use std::cell::RefCell;
use std::collections::VecDeque;
use std::rc::Rc;

#[derive(Debug, PartialOrd, Ord, PartialEq, Eq, Clone)]
pub enum Http3ServerEvent {
    /// Headers are ready.
    Headers {
        stream_id: u64,
        headers: Vec<Header>,
        fin: bool,
    },
    /// Request data is ready.
    Data {
        stream_id: u64,
        data: Vec<u8>,
        fin: bool,
    },
    /// Peer reset the stream.
    Reset { stream_id: u64, error: AppError },
    /// Connection state change.
    StateChange(Http3State),
}

#[derive(Debug, Default, Clone)]
pub struct Http3ServerEvents {
    events: Rc<RefCell<VecDeque<Http3ServerEvent>>>,
}

impl Http3ServerEvents {
    fn insert(&self, event: Http3ServerEvent) {
        self.events.borrow_mut().push_back(event);
    }

    fn remove<F>(&self, f: F)
    where
        F: Fn(&Http3ServerEvent) -> bool,
    {
        self.events.borrow_mut().retain(|evt| !f(evt))
    }

    pub fn events(&self) -> impl Iterator<Item = Http3ServerEvent> {
        self.events.replace(VecDeque::new()).into_iter()
    }

    pub fn has_events(&self) -> bool {
        !self.events.borrow().is_empty()
    }

    pub fn next_event(&self) -> Option<Http3ServerEvent> {
        self.events.borrow_mut().pop_front()
    }

    pub fn headers(&self, stream_id: u64, headers: Vec<Header>, fin: bool) {
        self.insert(Http3ServerEvent::Headers {
            stream_id,
            headers,
            fin,
        });
    }

    pub fn data(&self, stream_id: u64, data: Vec<u8>, fin: bool) {
        self.insert(Http3ServerEvent::Data {
            stream_id,
            data,
            fin,
        });
    }
}

impl Http3Events for Http3ServerEvents {
    fn reset(&self, stream_id: u64, error: AppError) {
        self.insert(Http3ServerEvent::Reset { stream_id, error });
    }

    fn connection_state_change(&self, state: Http3State) {
        self.insert(Http3ServerEvent::StateChange(state));
    }

    fn remove_events_for_stream_id(&self, stream_id: u64) {
        self.remove(|evt| {
            matches!(evt,
                Http3ServerEvent::Reset { stream_id: x, .. } if *x == stream_id)
        });
    }
}
