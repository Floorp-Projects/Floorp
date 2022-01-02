// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::module_name_repetitions)]

pub mod session;
pub mod webtransport;

use crate::client_events::Http3ClientEvents;
use crate::features::NegotiationState;
use crate::settings::{HSettingType, HSettings};
use crate::{Http3StreamInfo, Http3StreamType};
use neqo_transport::{AppError, StreamId};
pub use session::ExtendedConnectSession;
use std::cell::RefCell;
use std::collections::BTreeSet;
use std::collections::HashMap;
use std::fmt::Debug;
use std::rc::Rc;

pub trait ExtendedConnectEvents: Debug {
    fn session_start(&self, connect_type: ExtendedConnectType, stream_id: StreamId);
    fn session_end(
        &self,
        connect_type: ExtendedConnectType,
        stream_id: StreamId,
        error: Option<AppError>,
    );
    fn extended_connect_new_stream(&self, stream_info: Http3StreamInfo);
}

#[derive(Debug, PartialEq, Copy, Clone, Eq)]
pub enum ExtendedConnectType {
    WebTransport,
}

impl ExtendedConnectType {
    #[must_use]
    #[allow(clippy::unused_self)] // This will change when we have more features using ExtendedConnectType.
    pub fn string(&self) -> &str {
        "webtransport"
    }

    #[must_use]
    #[allow(clippy::unused_self)] // this will change when there is more types of the extended CONNECT.
    pub fn setting_type(self) -> HSettingType {
        HSettingType::EnableWebTransport
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
pub struct ExtendedConnectFeature {
    connect_type: ExtendedConnectType,
    feature_negotiation: NegotiationState,
    sessions: HashMap<StreamId, Rc<RefCell<ExtendedConnectSession>>>,
}

impl ExtendedConnectFeature {
    #[must_use]
    pub fn new(connect_type: ExtendedConnectType, enable: bool) -> Self {
        Self {
            feature_negotiation: NegotiationState::new(enable, HSettingType::from(connect_type)),
            connect_type,
            sessions: HashMap::new(),
        }
    }

    pub fn set_listener(&mut self, new_listener: Http3ClientEvents) {
        self.feature_negotiation.set_listener(new_listener);
    }

    pub fn insert(&mut self, stream_id: StreamId, session: Rc<RefCell<ExtendedConnectSession>>) {
        self.sessions.insert(stream_id, session);
    }

    pub fn get_session(
        &mut self,
        stream_id: StreamId,
    ) -> Option<Rc<RefCell<ExtendedConnectSession>>> {
        if !matches!(self.feature_negotiation, NegotiationState::Negotiated) {
            return None;
        }

        self.sessions.get_mut(&stream_id).cloned()
    }

    pub fn handle_settings(&mut self, settings: &HSettings) {
        self.feature_negotiation.handle_settings(settings);
    }

    #[must_use]
    pub fn enabled(&self) -> bool {
        self.feature_negotiation.enabled()
    }

    pub fn remove(
        &mut self,
        stream_id: StreamId,
    ) -> Option<(BTreeSet<StreamId>, BTreeSet<StreamId>)> {
        self.sessions
            .remove(&stream_id)
            .and_then(|ec| ec.borrow_mut().take_sub_streams())
    }
}
