// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::module_name_repetitions)]

pub(crate) mod webtransport_session;
pub(crate) mod webtransport_streams;

use crate::client_events::Http3ClientEvents;
use crate::features::NegotiationState;
use crate::settings::{HSettingType, HSettings};
use crate::{CloseType, Http3StreamInfo, Http3StreamType};
use neqo_common::Header;
use neqo_transport::{AppError, StreamId};
use std::fmt::Debug;
pub(crate) use webtransport_session::WebTransportSession;

#[derive(Debug, PartialEq, Eq, Clone)]
pub enum SessionCloseReason {
    Error(AppError),
    Status(u16),
    Clean { error: u32, message: String },
}

impl From<CloseType> for SessionCloseReason {
    fn from(close_type: CloseType) -> SessionCloseReason {
        match close_type {
            CloseType::ResetApp(e) | CloseType::ResetRemote(e) | CloseType::LocalError(e) => {
                SessionCloseReason::Error(e)
            }
            CloseType::Done => SessionCloseReason::Clean {
                error: 0,
                message: String::new(),
            },
        }
    }
}

pub(crate) trait ExtendedConnectEvents: Debug {
    fn session_start(
        &self,
        connect_type: ExtendedConnectType,
        stream_id: StreamId,
        status: u16,
        headers: Vec<Header>,
    );
    fn session_end(
        &self,
        connect_type: ExtendedConnectType,
        stream_id: StreamId,
        reason: SessionCloseReason,
        headers: Option<Vec<Header>>,
    );
    fn extended_connect_new_stream(&self, stream_info: Http3StreamInfo);
    fn new_datagram(&self, session_id: StreamId, datagram: Vec<u8>);
}

#[derive(Debug, PartialEq, Copy, Clone, Eq)]
pub(crate) enum ExtendedConnectType {
    WebTransport,
}

impl ExtendedConnectType {
    #[must_use]
    #[allow(clippy::unused_self)] // This will change when we have more features using ExtendedConnectType.
    pub fn string(&self) -> &str {
        "webtransport"
    }

    #[allow(clippy::unused_self)] // This will change when we have more features using ExtendedConnectType.
    #[must_use]
    pub fn get_stream_type(self, session_id: StreamId) -> Http3StreamType {
        Http3StreamType::WebTransport(session_id)
    }
}

impl From<ExtendedConnectType> for HSettingType {
    fn from(_type: ExtendedConnectType) -> Self {
        // This will change when we have more features using ExtendedConnectType.
        HSettingType::EnableWebTransport
    }
}

#[derive(Debug)]
pub(crate) struct ExtendedConnectFeature {
    feature_negotiation: NegotiationState,
}

impl ExtendedConnectFeature {
    #[must_use]
    pub fn new(connect_type: ExtendedConnectType, enable: bool) -> Self {
        Self {
            feature_negotiation: NegotiationState::new(enable, HSettingType::from(connect_type)),
        }
    }

    pub fn set_listener(&mut self, new_listener: Http3ClientEvents) {
        self.feature_negotiation.set_listener(new_listener);
    }

    pub fn handle_settings(&mut self, settings: &HSettings) {
        self.feature_negotiation.handle_settings(settings);
    }

    #[must_use]
    pub fn enabled(&self) -> bool {
        self.feature_negotiation.enabled()
    }
}
#[cfg(test)]
mod tests;
