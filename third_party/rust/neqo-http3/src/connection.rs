// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::control_stream_local::{ControlStreamLocal, HTTP3_UNI_STREAM_TYPE_CONTROL};
use crate::control_stream_remote::ControlStreamRemote;
use crate::hframe::HFrame;
use crate::hsettings_frame::{HSetting, HSettingType, HSettings};
use crate::stream_type_reader::NewStreamTypeReader;
use neqo_common::{matches, qdebug, qerror, qinfo, qtrace, qwarn};
use neqo_qpack::decoder::{QPackDecoder, QPACK_UNI_STREAM_TYPE_DECODER};
use neqo_qpack::encoder::{QPackEncoder, QPACK_UNI_STREAM_TYPE_ENCODER};
use neqo_transport::{AppError, CloseError, Connection, State, StreamType};
use std::collections::{BTreeSet, HashMap};
use std::fmt::Debug;
use std::mem;

use crate::{Error, Res};

const HTTP3_UNI_STREAM_TYPE_PUSH: u64 = 0x1;

pub(crate) enum HandleReadableOutput {
    NoOutput,
    PushStream,
    ControlFrames(Vec<HFrame>),
}

pub trait Http3Transaction: Debug {
    fn send(&mut self, conn: &mut Connection, encoder: &mut QPackEncoder) -> Res<()>;
    fn receive(&mut self, conn: &mut Connection, decoder: &mut QPackDecoder) -> Res<()>;
    fn has_data_to_send(&self) -> bool;
    fn reset_receiving_side(&mut self);
    fn stop_sending(&mut self);
    fn done(&self) -> bool;
    fn close_send(&mut self, conn: &mut Connection) -> Res<()>;
}

#[derive(Debug)]
enum Http3RemoteSettingsState {
    NotReceived,
    Received(HSettings),
    ZeroRtt(HSettings),
}

#[derive(Debug, PartialEq, PartialOrd, Ord, Eq, Clone)]
struct LocalSettings {
    max_table_size: u64,
    max_blocked_streams: u16,
}

#[derive(Debug, PartialEq, PartialOrd, Ord, Eq, Clone)]
pub enum Http3State {
    Initializing,
    ZeroRtt,
    Connected,
    GoingAway,
    Closing(CloseError),
    Closed(CloseError),
}

#[derive(Debug)]
pub struct Http3Connection<T: Http3Transaction> {
    pub state: Http3State,
    local_settings: LocalSettings,
    control_stream_local: ControlStreamLocal,
    control_stream_remote: ControlStreamRemote,
    new_streams: HashMap<u64, NewStreamTypeReader>,
    pub qpack_encoder: QPackEncoder,
    pub qpack_decoder: QPackDecoder,
    settings_state: Http3RemoteSettingsState,
    streams_have_data_to_send: BTreeSet<u64>,
    pub transactions: HashMap<u64, T>,
}

impl<T: Http3Transaction> ::std::fmt::Display for Http3Connection<T> {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Http3 connection")
    }
}

impl<T: Http3Transaction> Http3Connection<T> {
    pub fn new(max_table_size: u64, max_blocked_streams: u16) -> Self {
        if max_table_size > (1 << 30) - 1 {
            panic!("Wrong max_table_size");
        }
        Self {
            state: Http3State::Initializing,
            local_settings: LocalSettings {
                max_table_size,
                max_blocked_streams,
            },
            control_stream_local: ControlStreamLocal::default(),
            control_stream_remote: ControlStreamRemote::new(),
            new_streams: HashMap::new(),
            qpack_encoder: QPackEncoder::new(true),
            qpack_decoder: QPackDecoder::new(max_table_size, max_blocked_streams),
            settings_state: Http3RemoteSettingsState::NotReceived,
            streams_have_data_to_send: BTreeSet::new(),
            transactions: HashMap::new(),
        }
    }

    fn initialize_http3_connection(&mut self, conn: &mut Connection) -> Res<()> {
        qinfo!([self], "Initialize the http3 connection.");
        self.control_stream_local.create(conn)?;

        self.send_settings();
        self.create_qpack_streams(conn)?;
        Ok(())
    }

    fn send_settings(&mut self) {
        qdebug!([self], "Send settings.");
        self.control_stream_local.queue_frame(HFrame::Settings {
            settings: HSettings::new(&[
                HSetting {
                    setting_type: HSettingType::MaxTableCapacity,
                    value: self.qpack_decoder.get_max_table_size(),
                },
                HSetting {
                    setting_type: HSettingType::BlockedStreams,
                    value: self.qpack_decoder.get_blocked_streams().into(),
                },
            ]),
        });
    }

    fn create_qpack_streams(&mut self, conn: &mut Connection) -> Res<()> {
        qdebug!([self], "create_qpack_streams.");
        self.qpack_encoder
            .add_send_stream(conn.stream_create(StreamType::UniDi)?);
        self.qpack_decoder
            .add_send_stream(conn.stream_create(StreamType::UniDi)?);
        Ok(())
    }

    pub fn insert_streams_have_data_to_send(&mut self, stream_id: u64) {
        self.streams_have_data_to_send.insert(stream_id);
    }

    pub fn has_data_to_send(&self) -> bool {
        !self.streams_have_data_to_send.is_empty()
    }

    pub fn process_sending(&mut self, conn: &mut Connection) -> Res<()> {
        // check if control stream has data to send.
        self.control_stream_local.send(conn)?;

        let to_send = mem::replace(&mut self.streams_have_data_to_send, BTreeSet::new());
        for stream_id in to_send {
            if let Some(t) = &mut self.transactions.get_mut(&stream_id) {
                t.send(conn, &mut self.qpack_encoder)?;
                if t.has_data_to_send() {
                    self.streams_have_data_to_send.insert(stream_id);
                }
            }
        }
        self.qpack_decoder.send(conn)?;
        self.qpack_encoder.send(conn)?;
        Ok(())
    }

    pub fn set_resumption_settings(
        &mut self,
        conn: &mut Connection,
        settings: HSettings,
    ) -> Res<()> {
        if let Http3State::Initializing = &self.state {
            self.state = Http3State::ZeroRtt;
            self.initialize_http3_connection(conn)?;
            self.set_qpack_settings(&settings)?;
            self.settings_state = Http3RemoteSettingsState::ZeroRtt(settings);
            Ok(())
        } else {
            Err(Error::Unexpected)
        }
    }

    pub fn get_settings(&self) -> Option<HSettings> {
        if let Http3RemoteSettingsState::Received(settings) = &self.settings_state {
            Some(settings.clone())
        } else {
            None
        }
    }

    // This function adds a new unidi stream and try to read its type. Http3Connection can handle
    // a Http3 Control stream, Qpack streams and an unknown stream, but it cannot handle a Push stream.
    // If a Push stream has been discovered, return true and let the Http3Client/Server handle it.
    pub fn handle_new_unidi_stream(&mut self, conn: &mut Connection, stream_id: u64) -> Res<bool> {
        qtrace!([self], "A new stream: {}.", stream_id);
        debug_assert!(self.state_active());
        let stream_type;
        let fin;
        {
            let ns = &mut self
                .new_streams
                .entry(stream_id)
                .or_insert_with(NewStreamTypeReader::new);
            stream_type = ns.get_type(conn, stream_id);
            fin = ns.fin();
        }

        if fin {
            self.new_streams.remove(&stream_id);
            Ok(false)
        } else if let Some(t) = stream_type {
            self.new_streams.remove(&stream_id);
            self.decode_new_stream(conn, t, stream_id)
        } else {
            Ok(false)
        }
    }

    // This function handles reading from all streams, i.e. control, qpack, request/response
    // stream and unidi stream that are still do not have a type.
    // The function cannot handle:
    // 1) a Push stream (if a unkown unidi stream is decoded to be a push stream)
    // 2) frames MaxPushId or Goaway must be handled by Http3Client/Server.
    // The function returns HandleReadableOutput.
    pub(crate) fn handle_stream_readable(
        &mut self,
        conn: &mut Connection,
        stream_id: u64,
    ) -> Res<HandleReadableOutput> {
        qtrace!([self], "Readable stream {}.", stream_id);

        debug_assert!(self.state_active());

        let label = if ::log::log_enabled!(::log::Level::Debug) {
            format!("{}", self)
        } else {
            String::new()
        };

        if self.handle_read_stream(conn, stream_id)? {
            qdebug!([label], "Request/response stream {} read.", stream_id);
            Ok(HandleReadableOutput::NoOutput)
        } else if self
            .control_stream_remote
            .receive_if_this_stream(conn, stream_id)?
        {
            qdebug!(
                [self],
                "The remote control stream ({}) is readable.",
                stream_id
            );

            let mut control_frames = Vec::new();

            while self.control_stream_remote.frame_reader_done()
                || self.control_stream_remote.recvd_fin()
            {
                if let Some(f) = self.handle_control_frame()? {
                    control_frames.push(f);
                }
                self.control_stream_remote
                    .receive_if_this_stream(conn, stream_id)?;
            }
            if control_frames.is_empty() {
                Ok(HandleReadableOutput::NoOutput)
            } else {
                Ok(HandleReadableOutput::ControlFrames(control_frames))
            }
        } else if self.qpack_encoder.recv_if_encoder_stream(conn, stream_id)? {
            qdebug!(
                [self],
                "The qpack encoder stream ({}) is readable.",
                stream_id
            );
            Ok(HandleReadableOutput::NoOutput)
        } else if self.qpack_decoder.is_recv_stream(stream_id) {
            qdebug!(
                [self],
                "The qpack decoder stream ({}) is readable.",
                stream_id
            );
            let unblocked_streams = self.qpack_decoder.receive(conn, stream_id)?;
            for stream_id in unblocked_streams {
                qinfo!([self], "Stream {} is unblocked", stream_id);
                self.handle_read_stream(conn, stream_id)?;
            }
            Ok(HandleReadableOutput::NoOutput)
        } else if let Some(ns) = self.new_streams.get_mut(&stream_id) {
            let stream_type = ns.get_type(conn, stream_id);
            let fin = ns.fin();
            if fin {
                self.new_streams.remove(&stream_id);
            }
            if let Some(t) = stream_type {
                self.new_streams.remove(&stream_id);
                let push = self.decode_new_stream(conn, t, stream_id)?;
                if push {
                    return Ok(HandleReadableOutput::PushStream);
                }
            }

            Ok(HandleReadableOutput::NoOutput)
        } else {
            // For a new stream we receive NewStream event and a
            // RecvStreamReadable event.
            // In most cases we decode a new stream already on the NewStream
            // event and remove it from self.new_streams.
            // Therefore, while processing RecvStreamReadable there will be no
            // entry for the stream in self.new_streams.
            qdebug!("Unknown stream.");
            Ok(HandleReadableOutput::NoOutput)
        }
    }

    pub fn handle_stream_reset(
        &mut self,
        conn: &mut Connection,
        stream_id: u64,
        app_err: AppError,
    ) -> Res<bool> {
        qinfo!(
            [self],
            "Handle a stream reset stream_id={} app_err={}",
            stream_id,
            app_err
        );

        debug_assert!(self.state_active());

        if let Some(t) = self.transactions.get_mut(&stream_id) {
            // Close both sides of the transaction_client.
            t.reset_receiving_side();
            t.stop_sending();
            // close sending side of the transport stream as well. The server may have done
            // it as well, but just to be sure.
            let _ = conn.stream_reset_send(stream_id, app_err);
            // remove the stream
            self.transactions.remove(&stream_id);
            Ok(true)
        } else {
            Ok(false)
        }
    }

    pub fn handle_state_change(&mut self, conn: &mut Connection, state: &State) -> Res<bool> {
        qdebug!([self], "Handle state change {:?}", state);
        match state {
            State::Connected => {
                debug_assert!(matches!(
                    self.state,
                    Http3State::Initializing | Http3State::ZeroRtt
                ));
                if self.state == Http3State::Initializing {
                    self.initialize_http3_connection(conn)?;
                }
                self.state = Http3State::Connected;
                Ok(true)
            }
            State::Closing { error, .. } => {
                if !matches!(self.state, Http3State::Closing(_)| Http3State::Closed(_)) {
                    self.state = Http3State::Closing(error.clone().into());
                    Ok(true)
                } else {
                    Ok(false)
                }
            }
            State::Closed(error) => {
                if !matches!(self.state, Http3State::Closed(_)) {
                    self.state = Http3State::Closed(error.clone().into());
                    Ok(true)
                } else {
                    Ok(false)
                }
            }
            _ => Ok(false),
        }
    }

    pub fn handle_zero_rtt_rejected(&mut self) -> Res<()> {
        if self.state == Http3State::ZeroRtt {
            self.state = Http3State::Initializing;
            self.control_stream_local = ControlStreamLocal::default();
            self.control_stream_remote = ControlStreamRemote::new();
            self.new_streams.clear();
            self.qpack_encoder = QPackEncoder::new(true);
            self.qpack_decoder = QPackDecoder::new(
                self.local_settings.max_table_size,
                self.local_settings.max_blocked_streams,
            );
            self.settings_state = Http3RemoteSettingsState::NotReceived;
            self.streams_have_data_to_send.clear();
            // TODO: investigate whether this code can automatically retry failed transactions.
            self.transactions.clear();
            Ok(())
        } else {
            debug_assert!(false, "Zero rtt rejected in the wrong state.");
            Err(Error::HttpInternalError)
        }
    }

    fn handle_read_stream(&mut self, conn: &mut Connection, stream_id: u64) -> Res<bool> {
        let label = if ::log::log_enabled!(::log::Level::Debug) {
            format!("{}", self)
        } else {
            String::new()
        };

        debug_assert!(self.state_active());

        if let Some(transaction) = &mut self.transactions.get_mut(&stream_id) {
            qinfo!(
                [label],
                "Request/response stream {} is readable.",
                stream_id
            );
            match transaction.receive(conn, &mut self.qpack_decoder) {
                Err(e) => {
                    qerror!([label], "Error {} ocurred", e);
                    return Err(e);
                }
                Ok(()) => {
                    if transaction.done() {
                        self.transactions.remove(&stream_id);
                    }
                }
            }
            Ok(true)
        } else {
            Ok(false)
        }
    }

    // Returns true if it is a push stream.
    fn decode_new_stream(
        &mut self,
        conn: &mut Connection,
        stream_type: u64,
        stream_id: u64,
    ) -> Res<bool> {
        match stream_type {
            HTTP3_UNI_STREAM_TYPE_CONTROL => {
                self.control_stream_remote.add_remote_stream(stream_id)?;
                Ok(false)
            }

            HTTP3_UNI_STREAM_TYPE_PUSH => {
                qinfo!([self], "A new push stream {}.", stream_id);
                Ok(true)
            }
            QPACK_UNI_STREAM_TYPE_ENCODER => {
                qinfo!([self], "A new remote qpack encoder stream {}", stream_id);
                self.qpack_decoder
                    .add_recv_stream(stream_id)
                    .map_err(|_| Error::HttpStreamCreationError)?;
                Ok(false)
            }
            QPACK_UNI_STREAM_TYPE_DECODER => {
                qinfo!([self], "A new remote qpack decoder stream {}", stream_id);
                self.qpack_encoder
                    .add_recv_stream(stream_id)
                    .map_err(|_| Error::HttpStreamCreationError)?;
                Ok(false)
            }
            _ => {
                conn.stream_stop_sending(stream_id, Error::HttpStreamCreationError.code())?;
                Ok(false)
            }
        }
    }

    pub fn close(&mut self, error: AppError) {
        qinfo!([self], "Close connection error {:?}.", error);
        self.state = Http3State::Closing(CloseError::Application(error));
        if !self.transactions.is_empty() && (error == 0) {
            qwarn!("close() called when streams still active");
        }
        self.transactions.clear();
    }

    pub fn stream_reset(
        &mut self,
        conn: &mut Connection,
        stream_id: u64,
        error: AppError,
    ) -> Res<()> {
        qinfo!([self], "Reset stream {} error={}.", stream_id, error);
        let mut transaction = self
            .transactions
            .remove(&stream_id)
            .ok_or(Error::InvalidStreamId)?;
        transaction.stop_sending();
        // Stream maybe already be closed and we may get an error here, but we do not care.
        let _ = conn.stream_reset_send(stream_id, error);
        transaction.reset_receiving_side();
        // Stream maybe already be closed and we may get an error here, but we do not care.
        conn.stream_stop_sending(stream_id, error)?;
        Ok(())
    }

    pub fn stream_close_send(&mut self, conn: &mut Connection, stream_id: u64) -> Res<()> {
        qinfo!([self], "Close sending side for stream {}.", stream_id);
        debug_assert!(self.state_active() || self.state_zero_rtt());
        let transaction = self
            .transactions
            .get_mut(&stream_id)
            .ok_or(Error::InvalidStreamId)?;
        transaction.close_send(conn)?;
        if transaction.done() {
            self.transactions.remove(&stream_id);
        }
        Ok(())
    }

    // If the control stream has received frames MaxPushId or Goaway which handling is specific to
    // the client and server, we must give them to the specific client/server handler..
    fn handle_control_frame(&mut self) -> Res<Option<HFrame>> {
        if self.control_stream_remote.recvd_fin() {
            return Err(Error::HttpClosedCriticalStream);
        }
        if self.control_stream_remote.frame_reader_done() {
            let f = self.control_stream_remote.get_frame()?;
            qinfo!([self], "Handle a control frame {:?}", f);
            if !matches!(f, HFrame::Settings { .. })
                && !matches!(self.settings_state, Http3RemoteSettingsState::Received{..})
            {
                return Err(Error::HttpMissingSettings);
            }
            return match f {
                HFrame::Settings { settings } => {
                    self.handle_settings(settings)?;
                    Ok(None)
                }
                HFrame::CancelPush { .. } => Err(Error::HttpFrameUnexpected),
                HFrame::Goaway { .. } | HFrame::MaxPushId { .. } => Ok(Some(f)),
                _ => Err(Error::HttpFrameUnexpected),
            };
        }
        Ok(None)
    }

    fn set_qpack_settings(&mut self, settings: &[HSetting]) -> Res<()> {
        for s in settings {
            qinfo!([self], " {:?} = {:?}", s.setting_type, s.value);
            match s.setting_type {
                HSettingType::MaxTableCapacity => self.qpack_encoder.set_max_capacity(s.value)?,
                HSettingType::BlockedStreams => {
                    self.qpack_encoder.set_max_blocked_streams(s.value)?
                }
                _ => {}
            }
        }
        Ok(())
    }

    fn handle_settings(&mut self, new_settings: HSettings) -> Res<()> {
        qinfo!([self], "Handle SETTINGS frame.");
        match &self.settings_state {
            Http3RemoteSettingsState::NotReceived => {
                self.set_qpack_settings(&new_settings)?;
                self.settings_state = Http3RemoteSettingsState::Received(new_settings);
                Ok(())
            }
            Http3RemoteSettingsState::ZeroRtt(settings) => {
                let mut qpack_changed = false;
                for st in &[
                    HSettingType::MaxHeaderListSize,
                    HSettingType::MaxTableCapacity,
                    HSettingType::BlockedStreams,
                ] {
                    let zero_rtt_value = settings.get(*st);
                    let new_value = new_settings.get(*st);
                    if zero_rtt_value == new_value {
                        continue;
                    }
                    if zero_rtt_value > new_value {
                        qerror!(
                            [self],
                            "The new({}) and the old value({}) of setting {:?} do not match",
                            new_value,
                            zero_rtt_value,
                            st
                        );
                        return Err(Error::HttpSettingsError);
                    }

                    match st {
                        HSettingType::MaxTableCapacity => {
                            if zero_rtt_value != 0 {
                                return Err(Error::QpackError(neqo_qpack::Error::DecoderStream));
                            }
                            qpack_changed = true;
                        }
                        HSettingType::BlockedStreams => qpack_changed = true,
                        _ => (),
                    }
                }
                if qpack_changed {
                    qdebug!([self], "Settings after zero rtt differ.");
                    self.set_qpack_settings(&(new_settings))?;
                }
                self.settings_state = Http3RemoteSettingsState::Received(new_settings);
                Ok(())
            }
            Http3RemoteSettingsState::Received { .. } => Err(Error::HttpFrameUnexpected),
        }
    }

    fn state_active(&self) -> bool {
        matches!(self.state, Http3State::Connected | Http3State::GoingAway)
    }

    fn state_zero_rtt(&self) -> bool {
        matches!(self.state, Http3State::ZeroRtt)
    }

    pub fn state(&self) -> Http3State {
        self.state.clone()
    }

    pub fn add_transaction(&mut self, stream_id: u64, transaction: T) {
        if transaction.has_data_to_send() {
            self.streams_have_data_to_send.insert(stream_id);
        }
        self.transactions.insert(stream_id, transaction);
    }
}
