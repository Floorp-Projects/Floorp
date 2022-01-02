// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::module_name_repetitions)]

use super::{ExtendedConnectEvents, ExtendedConnectType};
use crate::{
    CloseType, Error, Http3StreamInfo, HttpRecvStreamEvents, RecvStreamEvents, SendStreamEvents,
};
use neqo_common::{qtrace, Header, Role};
use neqo_transport::StreamId;
use std::cell::RefCell;
use std::collections::BTreeSet;
use std::mem;
use std::rc::Rc;

#[derive(Debug, PartialEq)]
enum SessionState {
    Negotiating,
    Active(StreamId),
    Done,
}

#[derive(Debug)]
pub struct ExtendedConnectSession {
    connect_type: ExtendedConnectType,
    state: SessionState,
    events: Box<dyn ExtendedConnectEvents>,
    send_streams: BTreeSet<StreamId>,
    recv_streams: BTreeSet<StreamId>,
    role: Role,
}

impl ::std::fmt::Display for ExtendedConnectSession {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(
            f,
            "ExtendedConnectSesssion for {}",
            self.connect_type.string(),
        )
    }
}

impl ExtendedConnectSession {
    #[must_use]
    pub fn new(
        connect_type: ExtendedConnectType,
        events: Box<dyn ExtendedConnectEvents>,
        role: Role,
    ) -> Self {
        Self {
            connect_type,
            state: SessionState::Negotiating,
            events,
            send_streams: BTreeSet::new(),
            recv_streams: BTreeSet::new(),
            role,
        }
    }

    fn close(&mut self, stream_id: StreamId, close_type: CloseType) {
        if self.state == SessionState::Done {
            return;
        }
        qtrace!("ExtendedConnect close the session");
        self.state = SessionState::Done;
        if let CloseType::ResetApp(_) = close_type {
            return;
        }
        self.events
            .session_end(self.connect_type, stream_id, close_type.error());
    }

    pub fn negotiation_done(&mut self, stream_id: StreamId, succeeded: bool) {
        if self.state == SessionState::Done {
            return;
        }
        self.state = if succeeded {
            self.events.session_start(self.connect_type, stream_id);
            SessionState::Active(stream_id)
        } else {
            self.events.session_end(self.connect_type, stream_id, None);
            SessionState::Done
        };
    }

    pub fn add_stream(&mut self, stream_id: StreamId) {
        if let SessionState::Active(session_id) = self.state {
            if stream_id.is_bidi() {
                self.send_streams.insert(stream_id);
                self.recv_streams.insert(stream_id);
            } else if stream_id.is_self_initiated(self.role) {
                self.send_streams.insert(stream_id);
            } else {
                self.recv_streams.insert(stream_id);
            }

            if !stream_id.is_self_initiated(self.role) {
                self.events
                    .extended_connect_new_stream(Http3StreamInfo::new(
                        stream_id,
                        self.connect_type.get_stream_type(session_id),
                    ));
            }
        }
    }

    pub fn remove_recv_stream(&mut self, stream_id: StreamId) {
        self.recv_streams.remove(&stream_id);
    }

    pub fn remove_send_stream(&mut self, stream_id: StreamId) {
        self.send_streams.remove(&stream_id);
    }

    #[must_use]
    pub fn is_active(&self) -> bool {
        matches!(self.state, SessionState::Active(_))
    }

    pub fn take_sub_streams(&mut self) -> Option<(BTreeSet<StreamId>, BTreeSet<StreamId>)> {
        Some((
            mem::take(&mut self.recv_streams),
            mem::take(&mut self.send_streams),
        ))
    }
}

impl RecvStreamEvents for Rc<RefCell<ExtendedConnectSession>> {
    fn data_readable(&self, stream_info: Http3StreamInfo) {
        // A session request is not expected to receive any data. This may change in
        // the future.
        self.borrow_mut().close(
            stream_info.stream_id(),
            CloseType::LocalError(Error::HttpGeneralProtocolStream.code()),
        );
    }

    fn recv_closed(&self, stream_info: Http3StreamInfo, close_type: CloseType) {
        self.borrow_mut().close(stream_info.stream_id(), close_type);
    }
}

impl HttpRecvStreamEvents for Rc<RefCell<ExtendedConnectSession>> {
    fn header_ready(
        &self,
        stream_info: Http3StreamInfo,
        headers: Vec<Header>,
        _interim: bool,
        _fin: bool,
    ) {
        qtrace!("ExtendedConnect response headers {:?}", headers);
        self.borrow_mut().negotiation_done(
            stream_info.stream_id(),
            headers
                .iter()
                .find_map(|h| {
                    if h.name() == ":status" && h.value() == "200" {
                        Some(())
                    } else {
                        None
                    }
                })
                .is_some(),
        );
    }
}

impl SendStreamEvents for Rc<RefCell<ExtendedConnectSession>> {
    fn data_writable(&self, _stream_info: Http3StreamInfo) {}

    /// Add a new `StopSending` event
    fn send_closed(&self, stream_info: Http3StreamInfo, close_type: CloseType) {
        self.borrow_mut().close(stream_info.stream_id(), close_type);
    }
}
