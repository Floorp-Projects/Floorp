// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::client_events::Http3ClientEvents;
use crate::control_stream_local::{ControlStreamLocal, HTTP3_UNI_STREAM_TYPE_CONTROL};
use crate::control_stream_remote::ControlStreamRemote;
use crate::hframe::{HFrame, HSettingType};
use crate::server_events::Http3ServerEvents;
use crate::stream_type_reader::NewStreamTypeReader;
use crate::transaction_client::TransactionClient;
use crate::transaction_server::TransactionServer;
use neqo_common::{matches, qdebug, qerror, qinfo, qtrace, qwarn, Datagram};
use neqo_crypto::AntiReplay;
use neqo_qpack::decoder::{QPackDecoder, QPACK_UNI_STREAM_TYPE_DECODER};
use neqo_qpack::encoder::{QPackEncoder, QPACK_UNI_STREAM_TYPE_ENCODER};
use neqo_transport::{
    AppError, CloseError, Connection, ConnectionEvent, ConnectionIdManager, Output, Role, State,
    StreamType,
};
use std::cell::RefCell;
use std::collections::{BTreeSet, HashMap};
use std::fmt::Debug;
use std::mem;
use std::net::SocketAddr;
use std::rc::Rc;
use std::time::Instant;

use crate::{Error, Res};

const HTTP3_UNI_STREAM_TYPE_PUSH: u64 = 0x1;

const MAX_HEADER_LIST_SIZE_DEFAULT: u64 = u64::max_value();

pub trait Http3Events: Default + Debug {
    fn reset(&self, stream_id: u64, error: AppError);
    fn connection_state_change(&self, state: Http3State);
    fn remove_events_for_stream_id(&self, remove_stream_id: u64);
}

pub trait Http3Transaction: Debug {
    fn send(&mut self, conn: &mut Connection, encoder: &mut QPackEncoder) -> Res<()>;
    fn receive(&mut self, conn: &mut Connection, decoder: &mut QPackDecoder) -> Res<()>;
    fn has_data_to_send(&self) -> bool;
    fn is_state_sending_data(&self) -> bool;
    fn reset_receiving_side(&mut self);
    fn stop_sending(&mut self);
    fn done(&self) -> bool;
    fn close_send(&mut self, conn: &mut Connection) -> Res<()>;
}

pub trait Http3Handler<E: Http3Events, T: Http3Transaction> {
    fn new() -> Self;
    fn handle_stream_creatable(&mut self, events: &mut E, stream_type: StreamType) -> Res<()>;
    fn handle_new_push_stream(&mut self) -> Res<()>;
    fn handle_new_bidi_stream(
        &mut self,
        transactions: &mut HashMap<u64, T>,
        events: &mut E,
        stream_id: u64,
    ) -> Res<()>;
    fn handle_send_stream_writable(
        &mut self,
        transactions: &mut HashMap<u64, T>,
        events: &mut E,
        stream_id: u64,
    ) -> Res<()>;
    fn handle_stream_stop_sending(
        &mut self,
        transactions: &mut HashMap<u64, T>,
        events: &mut E,
        conn: &mut Connection,
        stop_stream_id: u64,
        app_err: AppError,
    ) -> Res<()>;
    fn handle_goaway(
        &mut self,
        transactions: &mut HashMap<u64, T>,
        events: &mut E,
        state: &mut Http3State,
        goaway_stream_id: u64,
    ) -> Res<()>;
    fn handle_max_push_id(&mut self, stream_id: u64) -> Res<()>;
    fn handle_authentication_needed(&self, events: &mut E) -> Res<()>;
}

#[derive(Debug, PartialEq, PartialOrd, Ord, Eq, Clone)]
pub enum Http3State {
    Initializing,
    Connected,
    GoingAway,
    Closing(CloseError),
    Closed(CloseError),
}

pub struct Http3Connection<E: Http3Events, T: Http3Transaction, H: Http3Handler<E, T>> {
    pub state: Http3State,
    pub conn: Connection,
    max_header_list_size: u64,
    control_stream_local: ControlStreamLocal,
    control_stream_remote: ControlStreamRemote,
    new_streams: HashMap<u64, NewStreamTypeReader>,
    pub qpack_encoder: QPackEncoder,
    pub qpack_decoder: QPackDecoder,
    settings_received: bool,
    streams_have_data_to_send: BTreeSet<u64>,
    pub events: E,
    pub transactions: HashMap<u64, T>,
    handler: H,
}

impl<E: Http3Events + Default, T: Http3Transaction, H: Http3Handler<E, T>> ::std::fmt::Display
    for Http3Connection<E, T, H>
{
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Http3 connection {:?}", self.role())
    }
}

impl<E: Http3Events + Default, T: Http3Transaction, H: Http3Handler<E, T>>
    Http3Connection<E, T, H>
{
    pub fn new_client(
        server_name: &str,
        protocols: &[impl AsRef<str>],
        cid_manager: Rc<RefCell<dyn ConnectionIdManager>>,
        local_addr: SocketAddr,
        remote_addr: SocketAddr,
        max_table_size: u32,
        max_blocked_streams: u16,
    ) -> Res<Self> {
        if max_table_size > (1 << 30) - 1 {
            panic!("Wrong max_table_size");
        }
        let conn =
            Connection::new_client(server_name, protocols, cid_manager, local_addr, remote_addr)?;
        Http3Connection::new_client_with_conn(conn, max_table_size, max_blocked_streams)
    }

    pub fn new_client_with_conn(
        c: Connection,
        max_table_size: u32,
        max_blocked_streams: u16,
    ) -> Res<Self> {
        if max_table_size > (1 << 30) - 1 {
            panic!("Wrong max_table_size");
        }
        Ok(Http3Connection {
            state: Http3State::Initializing,
            conn: c,
            max_header_list_size: MAX_HEADER_LIST_SIZE_DEFAULT,
            control_stream_local: ControlStreamLocal::default(),
            control_stream_remote: ControlStreamRemote::new(),
            new_streams: HashMap::new(),
            qpack_encoder: QPackEncoder::new(true),
            qpack_decoder: QPackDecoder::new(max_table_size, max_blocked_streams),
            settings_received: false,
            streams_have_data_to_send: BTreeSet::new(),
            events: E::default(),
            transactions: HashMap::new(),
            handler: H::new(),
        })
    }

    pub fn new_server(
        certs: &[impl AsRef<str>],
        protocols: &[impl AsRef<str>],
        anti_replay: &AntiReplay,
        cid_manager: Rc<RefCell<dyn ConnectionIdManager>>,
        max_table_size: u32,
        max_blocked_streams: u16,
    ) -> Res<Self> {
        if max_table_size > (1 << 30) - 1 {
            panic!("Wrong max_table_size");
        }
        Ok(Http3Connection {
            state: Http3State::Initializing,
            conn: Connection::new_server(certs, protocols, anti_replay, cid_manager)?,
            max_header_list_size: MAX_HEADER_LIST_SIZE_DEFAULT,
            control_stream_local: ControlStreamLocal::default(),
            control_stream_remote: ControlStreamRemote::new(),
            new_streams: HashMap::new(),
            qpack_encoder: QPackEncoder::new(true),
            qpack_decoder: QPackDecoder::new(max_table_size, max_blocked_streams),
            settings_received: false,
            streams_have_data_to_send: BTreeSet::new(),
            events: E::default(),
            transactions: HashMap::new(),
            handler: H::new(),
        })
    }

    fn initialize_http3_connection(&mut self) -> Res<()> {
        qinfo!([self] "Initialize the http3 connection.");
        self.control_stream_local.create(&mut self.conn)?;
        self.send_settings();
        self.create_qpack_streams()?;
        Ok(())
    }

    fn send_settings(&mut self) {
        qdebug!([self] "Send settings.");
        self.control_stream_local.queue_frame(HFrame::Settings {
            settings: vec![
                (
                    HSettingType::MaxTableSize,
                    self.qpack_decoder.get_max_table_size().into(),
                ),
                (
                    HSettingType::BlockedStreams,
                    self.qpack_decoder.get_blocked_streams().into(),
                ),
            ],
        });
    }

    fn create_qpack_streams(&mut self) -> Res<()> {
        qdebug!([self] "create_qpack_streams.");
        self.qpack_encoder
            .add_send_stream(self.conn.stream_create(StreamType::UniDi)?);
        self.qpack_decoder
            .add_send_stream(self.conn.stream_create(StreamType::UniDi)?);
        Ok(())
    }

    // This function takes the provided result and check for an error.
    // An error results in closing the connection.
    fn check_result<ERR>(&mut self, now: Instant, res: Res<ERR>) -> bool {
        match &res {
            Err(e) => {
                qinfo!([self] "Connection error: {}.", e);
                self.close(now, e.code(), &format!("{}", e));
                true
            }
            _ => false,
        }
    }

    pub fn role(&self) -> Role {
        self.conn.role()
    }

    pub fn process(&mut self, dgram: Option<Datagram>, now: Instant) -> Output {
        qtrace!([self] "Process.");
        if let Some(d) = dgram {
            self.process_input(d, now);
        }
        self.process_http3(now);
        self.process_output(now)
    }

    pub fn process_input(&mut self, dgram: Datagram, now: Instant) {
        qtrace!([self] "Process input.");
        self.conn.process_input(dgram, now);
    }

    pub fn process_timer(&mut self, now: Instant) {
        qtrace!([self] "Process timer.");
        self.conn.process_timer(now);
    }

    pub fn conn(&mut self) -> &mut Connection {
        &mut self.conn
    }

    pub fn process_http3(&mut self, now: Instant) {
        qtrace!([self] "Process http3 internal.");
        match self.state {
            Http3State::Connected | Http3State::GoingAway => {
                let res = self.check_connection_events();
                if self.check_result(now, res) {
                    return;
                }
                let res = self.process_sending();
                self.check_result(now, res);
            }
            Http3State::Closed { .. } => {}
            _ => {
                let res = self.check_connection_events();
                let _ = self.check_result(now, res);
            }
        }
    }

    pub fn process_output(&mut self, now: Instant) -> Output {
        qtrace!([self] "Process output.");
        self.conn.process_output(now)
    }

    pub fn stream_stop_sending(&mut self, stream_id: u64, app_error: AppError) -> Res<()> {
        self.conn.stream_stop_sending(stream_id, app_error)?;
        Ok(())
    }

    pub fn insert_streams_have_data_to_send(&mut self, stream_id: u64) {
        self.streams_have_data_to_send.insert(stream_id);
    }

    fn process_sending(&mut self) -> Res<()> {
        // check if control stream has data to send.
        self.control_stream_local.send(&mut self.conn)?;

        let to_send = mem::replace(&mut self.streams_have_data_to_send, BTreeSet::new());
        for stream_id in to_send {
            if let Some(t) = &mut self.transactions.get_mut(&stream_id) {
                t.send(&mut self.conn, &mut self.qpack_encoder)?;
                if t.has_data_to_send() {
                    self.streams_have_data_to_send.insert(stream_id);
                }
            }
        }
        self.qpack_decoder.send(&mut self.conn)?;
        self.qpack_encoder.send(&mut self.conn)?;
        Ok(())
    }

    // If this return an error the connection must be closed.
    fn check_connection_events(&mut self) -> Res<()> {
        qtrace!([self] "Check connection events.");
        while let Some(e) = self.conn.next_event() {
            qdebug!([self] "check_connection_events - event {:?}.", e);
            match e {
                ConnectionEvent::NewStream {
                    stream_id,
                    stream_type,
                } => self.handle_new_stream(stream_id, stream_type)?,
                ConnectionEvent::SendStreamWritable { stream_id } => {
                    self.handler.handle_send_stream_writable(
                        &mut self.transactions,
                        &mut self.events,
                        stream_id,
                    )?
                }
                ConnectionEvent::RecvStreamReadable { stream_id } => {
                    self.handle_stream_readable(stream_id)?
                }
                ConnectionEvent::RecvStreamReset {
                    stream_id,
                    app_error,
                } => self.handle_stream_reset(stream_id, app_error)?,
                ConnectionEvent::SendStreamStopSending {
                    stream_id,
                    app_error,
                } => self.handler.handle_stream_stop_sending(
                    &mut self.transactions,
                    &mut self.events,
                    &mut self.conn,
                    stream_id,
                    app_error,
                )?,
                ConnectionEvent::SendStreamComplete { stream_id } => {
                    self.handle_stream_complete(stream_id)?
                }
                ConnectionEvent::SendStreamCreatable { stream_type } => self
                    .handler
                    .handle_stream_creatable(&mut self.events, stream_type)?,
                ConnectionEvent::AuthenticationNeeded => self
                    .handler
                    .handle_authentication_needed(&mut self.events)?,
                ConnectionEvent::StateChange(state) => {
                    match state {
                        State::Connected => self.handle_connection_connected()?,
                        State::Closing { error, .. } => {
                            self.handle_connection_closing(error.clone().into())?
                        }
                        State::Closed(error) => {
                            self.handle_connection_closed(error.clone().into())?
                        }
                        _ => {}
                    };
                }
                ConnectionEvent::ZeroRttRejected => {
                    // TODO(mt) work out what to do here.
                    // Everything will have to be redone: SETTINGS, qpack streams, and requests.
                }
            }
        }
        Ok(())
    }

    fn handle_new_stream(&mut self, stream_id: u64, stream_type: StreamType) -> Res<()> {
        qinfo!([self] "A new stream: {:?} {}.", stream_type, stream_id);
        assert!(self.state_active());
        match stream_type {
            StreamType::BiDi => self.handler.handle_new_bidi_stream(
                &mut self.transactions,
                &mut self.events,
                stream_id,
            ),
            StreamType::UniDi => {
                let stream_type;
                let fin;
                {
                    let ns = &mut self
                        .new_streams
                        .entry(stream_id)
                        .or_insert_with(NewStreamTypeReader::new);
                    stream_type = ns.get_type(&mut self.conn, stream_id);
                    fin = ns.fin();
                }

                if fin {
                    self.new_streams.remove(&stream_id);
                    Ok(())
                } else if let Some(t) = stream_type {
                    self.decode_new_stream(t, stream_id)?;
                    self.new_streams.remove(&stream_id);
                    Ok(())
                } else {
                    Ok(())
                }
            }
        }
    }

    fn handle_stream_readable(&mut self, stream_id: u64) -> Res<()> {
        qtrace!([self] "Readable stream {}.", stream_id);

        assert!(self.state_active());

        let label = if ::log::log_enabled!(::log::Level::Debug) {
            format!("{}", self)
        } else {
            String::new()
        };
        let mut unblocked_streams: Vec<u64> = Vec::new();

        if self.handle_read_stream(stream_id)? {
            qdebug!([label] "Request/response stream {} read.", stream_id);
        } else if self
            .control_stream_remote
            .receive_if_this_stream(&mut self.conn, stream_id)?
        {
            qdebug!(
                [self]
                "The remote control stream ({}) is readable.",
                stream_id
            );
            while self.control_stream_remote.frame_reader_done()
                || self.control_stream_remote.recvd_fin()
            {
                self.handle_control_frame()?;
                self.control_stream_remote
                    .receive_if_this_stream(&mut self.conn, stream_id)?;
            }
        } else if self
            .qpack_encoder
            .recv_if_encoder_stream(&mut self.conn, stream_id)?
        {
            qdebug!(
                [self]
                "The qpack encoder stream ({}) is readable.",
                stream_id
            );
        } else if self.qpack_decoder.is_recv_stream(stream_id) {
            qdebug!(
                [self]
                "The qpack decoder stream ({}) is readable.",
                stream_id
            );
            unblocked_streams = self.qpack_decoder.receive(&mut self.conn, stream_id)?;
        } else if let Some(ns) = self.new_streams.get_mut(&stream_id) {
            let stream_type = ns.get_type(&mut self.conn, stream_id);
            let fin = ns.fin();
            if fin {
                self.new_streams.remove(&stream_id);
            }
            if let Some(t) = stream_type {
                self.decode_new_stream(t, stream_id)?;
                self.new_streams.remove(&stream_id);
            }
        } else {
            // For a new stream we receive NewStream event and a
            // RecvStreamReadable event.
            // In most cases we decode a new stream already on the NewStream
            // event and remove it from self.new_streams.
            // Therefore, while processing RecvStreamReadable there will be no
            // entry for the stream in self.new_streams.
            qdebug!("Unknown stream.");
        }

        for stream_id in unblocked_streams {
            qinfo!([self] "Stream {} is unblocked", stream_id);
            self.handle_read_stream(stream_id)?;
        }
        Ok(())
    }

    fn handle_stream_reset(&mut self, stream_id: u64, app_err: AppError) -> Res<()> {
        qinfo!([self] "Handle a stream reset stream_id={} app_err={}", stream_id, app_err);

        assert!(self.state_active());

        if let Some(t) = self.transactions.get_mut(&stream_id) {
            // Post the reset event.
            self.events.reset(stream_id, app_err);
            // Close both sides of the transaction_client.
            t.reset_receiving_side();
            t.stop_sending();
            // close sending side of the transport stream as well. The server may have done
            // it se well, but just to be sure.
            let _ = self.conn.stream_reset_send(stream_id, app_err);
            // remove the stream
            self.transactions.remove(&stream_id);
        }
        Ok(())
    }

    fn handle_stream_complete(&mut self, _stream_id: u64) -> Res<()> {
        Ok(())
    }

    fn handle_connection_connected(&mut self) -> Res<()> {
        assert_eq!(self.state, Http3State::Initializing);
        self.events.connection_state_change(Http3State::Connected);
        self.state = Http3State::Connected;
        self.initialize_http3_connection()
    }

    fn handle_connection_closing(&mut self, error_code: CloseError) -> Res<()> {
        assert!(self.state_active() || self.state_closing());
        self.events
            .connection_state_change(Http3State::Closing(error_code));
        self.state = Http3State::Closing(error_code);
        Ok(())
    }

    fn handle_connection_closed(&mut self, error_code: CloseError) -> Res<()> {
        self.events
            .connection_state_change(Http3State::Closed(error_code));
        self.state = Http3State::Closed(error_code);
        Ok(())
    }

    fn handle_read_stream(&mut self, stream_id: u64) -> Res<bool> {
        let label = if ::log::log_enabled!(::log::Level::Debug) {
            format!("{}", self)
        } else {
            String::new()
        };

        assert!(self.state_active());

        if let Some(transaction) = &mut self.transactions.get_mut(&stream_id) {
            qinfo!([label] "Request/response stream {} is readable.", stream_id);
            match transaction.receive(&mut self.conn, &mut self.qpack_decoder) {
                Err(e) => {
                    qerror!([label] "Error {} ocurred", e);
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

    fn decode_new_stream(&mut self, stream_type: u64, stream_id: u64) -> Res<()> {
        match stream_type {
            HTTP3_UNI_STREAM_TYPE_CONTROL => {
                self.control_stream_remote.add_remote_stream(stream_id)?;
                Ok(())
            }

            HTTP3_UNI_STREAM_TYPE_PUSH => {
                qinfo!([self] "A new push stream {}.", stream_id);
                self.handler.handle_new_push_stream()
            }
            QPACK_UNI_STREAM_TYPE_ENCODER => {
                qinfo!([self] "A new remote qpack encoder stream {}", stream_id);
                self.qpack_decoder
                    .add_recv_stream(stream_id)
                    .map_err(|_| Error::HttpStreamCreationError)
            }
            QPACK_UNI_STREAM_TYPE_DECODER => {
                qinfo!([self] "A new remote qpack decoder stream {}", stream_id);
                self.qpack_encoder
                    .add_recv_stream(stream_id)
                    .map_err(|_| Error::HttpStreamCreationError)
            }
            // TODO reserved stream types
            _ => {
                self.conn
                    .stream_stop_sending(stream_id, Error::HttpStreamCreationError.code())?;
                Ok(())
            }
        }
    }

    pub fn close(&mut self, now: Instant, error: AppError, msg: &str) {
        qinfo!([self] "Close connection error {:?} msg={}.", error, msg);
        assert!(self.state_active());
        self.state = Http3State::Closing(CloseError::Application(error));
        if !self.transactions.is_empty() && (error == 0) {
            qwarn!("close() called when streams still active");
        }
        self.transactions.clear();
        self.conn.close(now, error, msg);
    }

    pub fn stream_reset(&mut self, stream_id: u64, error: AppError) -> Res<()> {
        qinfo!([self] "Reset stream {} error={}.", stream_id, error);
        assert!(self.state_active());
        let mut transaction = self
            .transactions
            .remove(&stream_id)
            .ok_or(Error::InvalidStreamId)?;
        transaction.stop_sending();
        // Stream maybe already be closed and we may get an error here, but we do not care.
        let _ = self.conn.stream_reset_send(stream_id, error);
        transaction.reset_receiving_side();
        // Stream maybe already be closed and we may get an error here, but we do not care.
        self.conn.stream_stop_sending(stream_id, error)?;
        self.events.remove_events_for_stream_id(stream_id);
        Ok(())
    }

    pub fn stream_close_send(&mut self, stream_id: u64) -> Res<()> {
        qinfo!([self] "Close sending side for stream {}.", stream_id);
        assert!(self.state_active());
        let transaction = self
            .transactions
            .get_mut(&stream_id)
            .ok_or(Error::InvalidStreamId)?;
        transaction.close_send(&mut self.conn)?;
        if transaction.done() {
            self.transactions.remove(&stream_id);
        }
        Ok(())
    }

    fn handle_control_frame(&mut self) -> Res<()> {
        if self.control_stream_remote.recvd_fin() {
            return Err(Error::HttpClosedCriticalStream);
        }
        if self.control_stream_remote.frame_reader_done() {
            let f = self.control_stream_remote.get_frame()?;
            qinfo!([self] "Handle a control frame {:?}", f);
            if let HFrame::Settings { .. } = f {
                if self.settings_received {
                    qerror!([self] "SETTINGS frame already received");
                    return Err(Error::HttpFrameUnexpected);
                }
                self.settings_received = true;
            } else if !self.settings_received {
                qerror!([self] "SETTINGS frame not received");
                return Err(Error::HttpMissingSettings);
            }
            return match f {
                HFrame::Settings { settings } => {
                    self.handle_settings(&settings)?;
                    Ok(())
                }
                HFrame::CancelPush { .. } => Err(Error::HttpFrameUnexpected),
                HFrame::Goaway { stream_id } => self.handler.handle_goaway(
                    &mut self.transactions,
                    &mut self.events,
                    &mut self.state,
                    stream_id,
                ),
                HFrame::MaxPushId { push_id } => self.handler.handle_max_push_id(push_id),
                _ => Err(Error::HttpFrameUnexpected),
            };
        }
        Ok(())
    }

    fn handle_settings(&mut self, s: &[(HSettingType, u64)]) -> Res<()> {
        qinfo!([self] "Handle SETTINGS frame.");
        for (t, v) in s {
            qinfo!([self] " {:?} = {:?}", t, v);
            match t {
                HSettingType::MaxHeaderListSize => {
                    self.max_header_list_size = *v;
                }
                HSettingType::MaxTableSize => self.qpack_encoder.set_max_capacity(*v)?,
                HSettingType::BlockedStreams => self.qpack_encoder.set_max_blocked_streams(*v)?,

                _ => {}
            }
        }
        Ok(())
    }

    fn state_active(&self) -> bool {
        match self.state {
            Http3State::Connected | Http3State::GoingAway => true,
            _ => false,
        }
    }

    fn state_closing(&self) -> bool {
        matches!(self.state, Http3State::Closing(_))
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

#[derive(Default)]
pub struct Http3ClientHandler {}

impl Http3Handler<Http3ClientEvents, TransactionClient> for Http3ClientHandler {
    fn new() -> Self {
        Http3ClientHandler::default()
    }

    fn handle_stream_creatable(
        &mut self,
        events: &mut Http3ClientEvents,
        stream_type: StreamType,
    ) -> Res<()> {
        events.new_requests_creatable(stream_type);
        Ok(())
    }

    fn handle_new_push_stream(&mut self) -> Res<()> {
        // TODO implement PUSH
        qerror!([self] "PUSH is not implemented!");
        Err(Error::HttpIdError)
    }

    fn handle_new_bidi_stream(
        &mut self,
        _transactions: &mut HashMap<u64, TransactionClient>,
        _events: &mut Http3ClientEvents,
        _stream_id: u64,
    ) -> Res<()> {
        qerror!("Client received a new bidirectional stream!");
        Err(Error::HttpStreamCreationError)
    }

    fn handle_send_stream_writable(
        &mut self,
        transactions: &mut HashMap<u64, TransactionClient>,
        events: &mut Http3ClientEvents,
        stream_id: u64,
    ) -> Res<()> {
        qtrace!([self] "Writable stream {}.", stream_id);

        if let Some(t) = transactions.get_mut(&stream_id) {
            if t.is_state_sending_data() {
                events.data_writable(stream_id);
            }
        }
        Ok(())
    }

    fn handle_stream_stop_sending(
        &mut self,
        transactions: &mut HashMap<u64, TransactionClient>,
        events: &mut Http3ClientEvents,
        conn: &mut Connection,
        stop_stream_id: u64,
        app_err: AppError,
    ) -> Res<()> {
        qinfo!([self] "Handle stream_stop_sending stream_id={} app_err={}", stop_stream_id, app_err);

        if let Some(t) = transactions.get_mut(&stop_stream_id) {
            // close sending side.
            t.stop_sending();

            // If error is Error::EarlyResponse we will post StopSending event,
            // otherwise post reset.
            if app_err == Error::HttpEarlyResponse.code() && !t.is_sending_closed() {
                events.stop_sending(stop_stream_id, app_err);
            }
            // if error is not Error::EarlyResponse we will close receiving part as well.
            if app_err != Error::HttpEarlyResponse.code() {
                events.reset(stop_stream_id, app_err);
                // The server may close its sending side as well, but just to be sure
                // we will do it ourselves.
                let _ = conn.stream_stop_sending(stop_stream_id, app_err);
                t.reset_receiving_side();
            }
            if t.done() {
                transactions.remove(&stop_stream_id);
            }
        }
        Ok(())
    }

    fn handle_goaway(
        &mut self,
        transactions: &mut HashMap<u64, TransactionClient>,
        events: &mut Http3ClientEvents,
        state: &mut Http3State,
        goaway_stream_id: u64,
    ) -> Res<()> {
        qinfo!([self] "handle_goaway");
        // Issue reset events for streams >= goaway stream id
        for id in transactions
            .iter()
            .filter(|(id, _)| **id >= goaway_stream_id)
            .map(|(id, _)| *id)
        {
            events.reset(id, Error::HttpRequestRejected.code())
        }
        events.goaway_received();

        // Actually remove (i.e. don't retain) these streams
        transactions.retain(|id, _| *id < goaway_stream_id);

        if *state == Http3State::Connected {
            *state = Http3State::GoingAway;
        }
        Ok(())
    }

    fn handle_max_push_id(&mut self, stream_id: u64) -> Res<()> {
        qerror!([self] "handle_max_push_id={}.", stream_id);
        Err(Error::HttpFrameUnexpected)
    }

    fn handle_authentication_needed(&self, events: &mut Http3ClientEvents) -> Res<()> {
        events.authentication_needed();
        Ok(())
    }
}

impl ::std::fmt::Display for Http3ClientHandler {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Http3 connection client")
    }
}

#[derive(Default)]
pub struct Http3ServerHandler {}

impl Http3Handler<Http3ServerEvents, TransactionServer> for Http3ServerHandler {
    fn new() -> Self {
        Http3ServerHandler::default()
    }

    fn handle_stream_creatable(
        &mut self,
        _events: &mut Http3ServerEvents,
        _stream_type: StreamType,
    ) -> Res<()> {
        Ok(())
    }

    fn handle_new_push_stream(&mut self) -> Res<()> {
        qerror!([self] "Error: server receives a push stream!");
        Err(Error::HttpStreamCreationError)
    }

    fn handle_new_bidi_stream(
        &mut self,
        transactions: &mut HashMap<u64, TransactionServer>,
        events: &mut Http3ServerEvents,
        stream_id: u64,
    ) -> Res<()> {
        transactions.insert(stream_id, TransactionServer::new(stream_id, events.clone()));
        Ok(())
    }

    fn handle_send_stream_writable(
        &mut self,
        _transactions: &mut HashMap<u64, TransactionServer>,
        _events: &mut Http3ServerEvents,
        _stream_id: u64,
    ) -> Res<()> {
        Ok(())
    }

    fn handle_goaway(
        &mut self,
        _transactions: &mut HashMap<u64, TransactionServer>,
        _events: &mut Http3ServerEvents,
        _state: &mut Http3State,
        _goaway_stream_id: u64,
    ) -> Res<()> {
        qerror!([self] "handle_goaway");
        Err(Error::HttpFrameUnexpected)
    }

    fn handle_stream_stop_sending(
        &mut self,
        _transactions: &mut HashMap<u64, TransactionServer>,
        _events: &mut Http3ServerEvents,
        _conn: &mut Connection,
        _stop_stream_id: u64,
        _app_err: AppError,
    ) -> Res<()> {
        Ok(())
    }

    fn handle_max_push_id(&mut self, stream_id: u64) -> Res<()> {
        qinfo!([self] "handle_max_push_id={}.", stream_id);
        // TODO
        Ok(())
    }

    fn handle_authentication_needed(&self, _events: &mut Http3ServerEvents) -> Res<()> {
        Err(Error::HttpInternalError)
    }
}

impl ::std::fmt::Display for Http3ServerHandler {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Http3 connection server")
    }
}
