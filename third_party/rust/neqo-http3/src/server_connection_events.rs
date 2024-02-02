// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{cell::RefCell, collections::VecDeque, rc::Rc};

use neqo_common::Header;
use neqo_transport::{AppError, StreamId};

use crate::{
    connection::Http3State,
    features::extended_connect::{ExtendedConnectEvents, ExtendedConnectType, SessionCloseReason},
    CloseType, Http3StreamInfo, HttpRecvStreamEvents, Priority, RecvStreamEvents, SendStreamEvents,
};

#[derive(Debug, PartialEq, Eq, Clone)]
pub(crate) enum Http3ServerConnEvent {
    /// Headers are ready.
    Headers {
        stream_info: Http3StreamInfo,
        headers: Vec<Header>,
        fin: bool,
    },
    PriorityUpdate {
        stream_id: StreamId,
        priority: Priority,
    },
    /// Request data is ready.
    DataReadable {
        stream_info: Http3StreamInfo,
    },
    DataWritable {
        stream_info: Http3StreamInfo,
    },
    StreamReset {
        stream_info: Http3StreamInfo,
        error: AppError,
    },
    StreamStopSending {
        stream_info: Http3StreamInfo,
        error: AppError,
    },
    /// Connection state change.
    StateChange(Http3State),
    ExtendedConnect {
        stream_id: StreamId,
        headers: Vec<Header>,
    },
    ExtendedConnectClosed {
        connect_type: ExtendedConnectType,
        stream_id: StreamId,
        reason: SessionCloseReason,
        headers: Option<Vec<Header>>,
    },
    ExtendedConnectNewStream(Http3StreamInfo),
    ExtendedConnectDatagram {
        session_id: StreamId,
        datagram: Vec<u8>,
    },
}

#[derive(Debug, Default, Clone)]
pub(crate) struct Http3ServerConnEvents {
    events: Rc<RefCell<VecDeque<Http3ServerConnEvent>>>,
}

impl SendStreamEvents for Http3ServerConnEvents {
    fn send_closed(&self, stream_info: Http3StreamInfo, close_type: CloseType) {
        if close_type != CloseType::Done {
            self.insert(Http3ServerConnEvent::StreamStopSending {
                stream_info,
                error: close_type.error().unwrap(),
            });
        }
    }

    fn data_writable(&self, stream_info: Http3StreamInfo) {
        self.insert(Http3ServerConnEvent::DataWritable { stream_info });
    }
}

impl RecvStreamEvents for Http3ServerConnEvents {
    /// Add a new `DataReadable` event
    fn data_readable(&self, stream_info: Http3StreamInfo) {
        self.insert(Http3ServerConnEvent::DataReadable { stream_info });
    }

    fn recv_closed(&self, stream_info: Http3StreamInfo, close_type: CloseType) {
        if close_type != CloseType::Done {
            self.remove_events_for_stream_id(stream_info);
            self.insert(Http3ServerConnEvent::StreamReset {
                stream_info,
                error: close_type.error().unwrap(),
            });
        }
    }
}

impl HttpRecvStreamEvents for Http3ServerConnEvents {
    /// Add a new `HeaderReady` event.
    fn header_ready(
        &self,
        stream_info: Http3StreamInfo,
        headers: Vec<Header>,
        _interim: bool,
        fin: bool,
    ) {
        self.insert(Http3ServerConnEvent::Headers {
            stream_info,
            headers,
            fin,
        });
    }

    fn extended_connect_new_session(&self, stream_id: StreamId, headers: Vec<Header>) {
        self.insert(Http3ServerConnEvent::ExtendedConnect { stream_id, headers });
    }
}

impl ExtendedConnectEvents for Http3ServerConnEvents {
    fn session_start(
        &self,
        _connect_type: ExtendedConnectType,
        _stream_id: StreamId,
        _status: u16,
        _headers: Vec<Header>,
    ) {
    }

    fn session_end(
        &self,
        connect_type: ExtendedConnectType,
        stream_id: StreamId,
        reason: SessionCloseReason,
        headers: Option<Vec<Header>>,
    ) {
        self.insert(Http3ServerConnEvent::ExtendedConnectClosed {
            connect_type,
            stream_id,
            reason,
            headers,
        });
    }

    fn extended_connect_new_stream(&self, stream_info: Http3StreamInfo) {
        self.insert(Http3ServerConnEvent::ExtendedConnectNewStream(stream_info));
    }

    fn new_datagram(&self, session_id: StreamId, datagram: Vec<u8>) {
        self.insert(Http3ServerConnEvent::ExtendedConnectDatagram {
            session_id,
            datagram,
        });
    }
}

impl Http3ServerConnEvents {
    fn insert(&self, event: Http3ServerConnEvent) {
        self.events.borrow_mut().push_back(event);
    }

    fn remove<F>(&self, f: F)
    where
        F: Fn(&Http3ServerConnEvent) -> bool,
    {
        self.events.borrow_mut().retain(|evt| !f(evt));
    }

    pub fn has_events(&self) -> bool {
        !self.events.borrow().is_empty()
    }

    pub fn next_event(&self) -> Option<Http3ServerConnEvent> {
        self.events.borrow_mut().pop_front()
    }

    pub fn connection_state_change(&self, state: Http3State) {
        self.insert(Http3ServerConnEvent::StateChange(state));
    }

    pub fn priority_update(&self, stream_id: StreamId, priority: Priority) {
        self.insert(Http3ServerConnEvent::PriorityUpdate {
            stream_id,
            priority,
        });
    }

    fn remove_events_for_stream_id(&self, stream_info: Http3StreamInfo) {
        self.remove(|evt| {
            matches!(evt,
                Http3ServerConnEvent::Headers { stream_info: x, .. } | Http3ServerConnEvent::DataReadable { stream_info: x, .. } if *x == stream_info)
        });
    }
}
