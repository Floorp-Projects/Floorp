// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::module_name_repetitions)]

use crate::connection::Http3State;
use crate::settings::HSettingType;
use crate::{
    features::extended_connect::{ExtendedConnectEvents, ExtendedConnectType, SessionCloseReason},
    CloseType, Http3StreamInfo, HttpRecvStreamEvents, RecvStreamEvents, SendStreamEvents,
};
use neqo_common::{event::Provider as EventProvider, Header};
use neqo_crypto::ResumptionToken;
use neqo_transport::{AppError, StreamId, StreamType};

use std::cell::RefCell;
use std::collections::VecDeque;
use std::rc::Rc;

#[derive(Debug, PartialEq, Eq, Clone)]
pub enum WebTransportEvent {
    Negotiated(bool),
    Session {
        stream_id: StreamId,
        status: u16,
        headers: Vec<Header>,
    },
    SessionClosed {
        stream_id: StreamId,
        reason: SessionCloseReason,
        headers: Option<Vec<Header>>,
    },
    NewStream {
        stream_id: StreamId,
        session_id: StreamId,
    },
    Datagram {
        session_id: StreamId,
        datagram: Vec<u8>,
    },
}

#[derive(Debug, PartialEq, Eq, Clone)]
pub enum Http3ClientEvent {
    /// Response headers are received.
    HeaderReady {
        stream_id: StreamId,
        headers: Vec<Header>,
        interim: bool,
        fin: bool,
    },
    /// A stream can accept new data.
    DataWritable { stream_id: StreamId },
    /// New bytes available for reading.
    DataReadable { stream_id: StreamId },
    /// Peer reset the stream or there was an parsing error.
    Reset {
        stream_id: StreamId,
        error: AppError,
        local: bool,
    },
    /// Peer has sent a STOP_SENDING.
    StopSending {
        stream_id: StreamId,
        error: AppError,
    },
    /// A new push promise.
    PushPromise {
        push_id: u64,
        request_stream_id: StreamId,
        headers: Vec<Header>,
    },
    /// A push response headers are ready.
    PushHeaderReady {
        push_id: u64,
        headers: Vec<Header>,
        interim: bool,
        fin: bool,
    },
    /// New bytes are available on a push stream for reading.
    PushDataReadable { push_id: u64 },
    /// A push has been canceled.
    PushCanceled { push_id: u64 },
    /// A push stream was been reset due to a HttpGeneralProtocol error.
    /// Most common case are malformed response headers.
    PushReset { push_id: u64, error: AppError },
    /// New stream can be created
    RequestsCreatable,
    /// Cert authentication needed
    AuthenticationNeeded,
    /// Encrypted client hello fallback occurred.  The certificate for the
    /// name `public_name` needs to be authenticated in order to get
    /// an updated ECH configuration.
    EchFallbackAuthenticationNeeded { public_name: String },
    /// A new resumption token.
    ResumptionToken(ResumptionToken),
    /// Zero Rtt has been rejected.
    ZeroRttRejected,
    /// Client has received a GOAWAY frame
    GoawayReceived,
    /// Connection state change.
    StateChange(Http3State),
    /// WebTransport events
    WebTransport(WebTransportEvent),
}

#[derive(Debug, Default, Clone)]
pub struct Http3ClientEvents {
    events: Rc<RefCell<VecDeque<Http3ClientEvent>>>,
}

impl RecvStreamEvents for Http3ClientEvents {
    /// Add a new `DataReadable` event
    fn data_readable(&self, stream_info: Http3StreamInfo) {
        self.insert(Http3ClientEvent::DataReadable {
            stream_id: stream_info.stream_id(),
        });
    }

    /// Add a new `Reset` event.
    fn recv_closed(&self, stream_info: Http3StreamInfo, close_type: CloseType) {
        let stream_id = stream_info.stream_id();
        let (local, error) = match close_type {
            CloseType::ResetApp(_) => {
                self.remove_recv_stream_events(stream_id);
                return;
            }
            CloseType::Done => return,
            CloseType::ResetRemote(e) => {
                self.remove_recv_stream_events(stream_id);
                (false, e)
            }
            CloseType::LocalError(e) => {
                self.remove_recv_stream_events(stream_id);
                (true, e)
            }
        };

        self.insert(Http3ClientEvent::Reset {
            stream_id,
            error,
            local,
        });
    }
}

impl HttpRecvStreamEvents for Http3ClientEvents {
    /// Add a new `HeaderReady` event.
    fn header_ready(
        &self,
        stream_info: Http3StreamInfo,
        headers: Vec<Header>,
        interim: bool,
        fin: bool,
    ) {
        self.insert(Http3ClientEvent::HeaderReady {
            stream_id: stream_info.stream_id(),
            headers,
            interim,
            fin,
        });
    }
}

impl SendStreamEvents for Http3ClientEvents {
    /// Add a new `DataWritable` event.
    fn data_writable(&self, stream_info: Http3StreamInfo) {
        self.insert(Http3ClientEvent::DataWritable {
            stream_id: stream_info.stream_id(),
        });
    }

    fn send_closed(&self, stream_info: Http3StreamInfo, close_type: CloseType) {
        let stream_id = stream_info.stream_id();
        self.remove_send_stream_events(stream_id);
        if let CloseType::ResetRemote(error) = close_type {
            self.insert(Http3ClientEvent::StopSending { stream_id, error });
        }
    }
}

impl ExtendedConnectEvents for Http3ClientEvents {
    fn session_start(
        &self,
        connect_type: ExtendedConnectType,
        stream_id: StreamId,
        status: u16,
        headers: Vec<Header>,
    ) {
        if connect_type == ExtendedConnectType::WebTransport {
            self.insert(Http3ClientEvent::WebTransport(WebTransportEvent::Session {
                stream_id,
                status,
                headers,
            }));
        } else {
            unreachable!("There is only ExtendedConnectType::WebTransport.");
        }
    }

    fn session_end(
        &self,
        connect_type: ExtendedConnectType,
        stream_id: StreamId,
        reason: SessionCloseReason,
        headers: Option<Vec<Header>>,
    ) {
        if connect_type == ExtendedConnectType::WebTransport {
            self.insert(Http3ClientEvent::WebTransport(
                WebTransportEvent::SessionClosed {
                    stream_id,
                    reason,
                    headers,
                },
            ));
        } else {
            unreachable!("There are no other types.");
        }
    }

    fn extended_connect_new_stream(&self, stream_info: Http3StreamInfo) {
        self.insert(Http3ClientEvent::WebTransport(
            WebTransportEvent::NewStream {
                stream_id: stream_info.stream_id(),
                session_id: stream_info.session_id().unwrap(),
            },
        ));
    }

    fn new_datagram(&self, session_id: StreamId, datagram: Vec<u8>) {
        self.insert(Http3ClientEvent::WebTransport(
            WebTransportEvent::Datagram {
                session_id,
                datagram,
            },
        ));
    }
}

impl Http3ClientEvents {
    pub fn push_promise(&self, push_id: u64, request_stream_id: StreamId, headers: Vec<Header>) {
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

    pub fn push_reset(&self, push_id: u64, error: AppError) {
        self.remove_events_for_push_id(push_id);
        self.insert(Http3ClientEvent::PushReset { push_id, error });
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

    /// Add a new `AuthenticationNeeded` event
    pub(crate) fn ech_fallback_authentication_needed(&self, public_name: String) {
        self.insert(Http3ClientEvent::EchFallbackAuthenticationNeeded { public_name });
    }

    /// Add a new resumption token event.
    pub(crate) fn resumption_token(&self, token: ResumptionToken) {
        self.insert(Http3ClientEvent::ResumptionToken(token));
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

    pub fn insert(&self, event: Http3ClientEvent) {
        self.events.borrow_mut().push_back(event);
    }

    fn remove<F>(&self, f: F)
    where
        F: Fn(&Http3ClientEvent) -> bool,
    {
        self.events.borrow_mut().retain(|evt| !f(evt));
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
    fn remove_recv_stream_events(&self, stream_id: StreamId) {
        self.remove(|evt| {
            matches!(evt,
                Http3ClientEvent::HeaderReady { stream_id: x, .. }
                | Http3ClientEvent::DataReadable { stream_id: x }
                | Http3ClientEvent::PushPromise { request_stream_id: x, .. }
                | Http3ClientEvent::Reset { stream_id: x, .. } if *x == stream_id)
        });
    }

    fn remove_send_stream_events(&self, stream_id: StreamId) {
        self.remove(|evt| {
            matches!(evt,
                Http3ClientEvent::DataWritable { stream_id: x }
                | Http3ClientEvent::StopSending { stream_id: x, .. } if *x == stream_id)
        });
    }

    pub fn has_push(&self, push_id: u64) -> bool {
        for iter in &*self.events.borrow() {
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

    pub fn negotiation_done(&self, feature_type: HSettingType, succeeded: bool) {
        if feature_type == HSettingType::EnableWebTransport {
            self.insert(Http3ClientEvent::WebTransport(
                WebTransportEvent::Negotiated(succeeded),
            ));
        }
    }
}

impl EventProvider for Http3ClientEvents {
    type Event = Http3ClientEvent;

    /// Check if there is any event present.
    fn has_events(&self) -> bool {
        !self.events.borrow().is_empty()
    }

    /// Take the first event.
    fn next_event(&mut self) -> Option<Self::Event> {
        self.events.borrow_mut().pop_front()
    }
}
