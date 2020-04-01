// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::connection::{HandleReadableOutput, Http3Connection, Http3State, Http3Transaction};
use crate::hframe::HFrame;
use crate::server_connection_events::{Http3ServerConnEvent, Http3ServerConnEvents};
use crate::transaction_server::TransactionServer;
use crate::{Error, Header, Res};
use neqo_common::{qdebug, qinfo, qtrace};
use neqo_transport::{AppError, Connection, ConnectionEvent, StreamType};
use std::time::Instant;

#[derive(Debug)]
pub struct Http3ServerHandler {
    base_handler: Http3Connection<TransactionServer>,
    events: Http3ServerConnEvents,
    needs_processing: bool,
}

impl ::std::fmt::Display for Http3ServerHandler {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Http3 server connection")
    }
}

impl Http3ServerHandler {
    pub fn new(max_table_size: u32, max_blocked_streams: u16) -> Self {
        Self {
            base_handler: Http3Connection::new(max_table_size, max_blocked_streams),
            events: Http3ServerConnEvents::default(),
            needs_processing: false,
        }
    }
    pub fn set_response(&mut self, stream_id: u64, headers: &[Header], data: Vec<u8>) -> Res<()> {
        self.base_handler
            .transactions
            .get_mut(&stream_id)
            .ok_or(Error::InvalidStreamId)?
            .set_response(headers, data, &mut self.base_handler.qpack_encoder);
        self.base_handler
            .insert_streams_have_data_to_send(stream_id);
        Ok(())
    }

    pub fn stream_reset(
        &mut self,
        conn: &mut Connection,
        stream_id: u64,
        app_error: AppError,
    ) -> Res<()> {
        self.base_handler.stream_reset(conn, stream_id, app_error)?;
        self.events.remove_events_for_stream_id(stream_id);
        self.needs_processing = true;
        Ok(())
    }

    pub fn process_http3(&mut self, conn: &mut Connection, now: Instant) {
        qtrace!([self], "Process http3 internal.");
        match self.base_handler.state() {
            Http3State::Connected | Http3State::GoingAway => {
                let res = self.check_connection_events(conn);
                if self.check_result(conn, now, res) {
                    return;
                }
                let res = self.base_handler.process_sending(conn);
                self.check_result(conn, now, res);
            }
            Http3State::Closed { .. } => {}
            _ => {
                let res = self.check_connection_events(conn);
                let _ = self.check_result(conn, now, res);
            }
        }
    }

    pub fn next_event(&mut self) -> Option<Http3ServerConnEvent> {
        self.events.next_event()
    }

    pub fn should_be_processed(&mut self) -> bool {
        if self.needs_processing {
            self.needs_processing = false;
            return true;
        }
        self.base_handler.has_data_to_send() | self.events.has_events()
    }

    // This function takes the provided result and check for an error.
    // An error results in closing the connection.
    fn check_result<ERR>(&mut self, conn: &mut Connection, now: Instant, res: Res<ERR>) -> bool {
        match &res {
            Err(e) => {
                qinfo!([self], "Connection error: {}.", e);
                conn.close(now, e.code(), &format!("{}", e));
                self.base_handler.close(e.code());
                self.events
                    .connection_state_change(self.base_handler.state());
                true
            }
            _ => false,
        }
    }

    // If this return an error the connection must be closed.
    fn check_connection_events(&mut self, conn: &mut Connection) -> Res<()> {
        qtrace!([self], "Check connection events.");
        while let Some(e) = conn.next_event() {
            qdebug!([self], "check_connection_events - event {:?}.", e);
            match e {
                ConnectionEvent::NewStream {
                    stream_id,
                    stream_type,
                } => match stream_type {
                    StreamType::BiDi => self.base_handler.add_transaction(
                        stream_id,
                        TransactionServer::new(stream_id, self.events.clone()),
                    ),
                    StreamType::UniDi => {
                        if self.base_handler.handle_new_unidi_stream(conn, stream_id)? {
                            return Err(Error::HttpStreamCreationError);
                        }
                    }
                },
                ConnectionEvent::SendStreamWritable { .. } => {}
                ConnectionEvent::RecvStreamReadable { stream_id } => {
                    self.handle_stream_readable(conn, stream_id)?
                }
                ConnectionEvent::RecvStreamReset {
                    stream_id,
                    app_error,
                } => {
                    let _ = self
                        .base_handler
                        .handle_stream_reset(conn, stream_id, app_error)?;
                }
                ConnectionEvent::SendStreamStopSending {
                    stream_id,
                    app_error,
                } => self.handle_stream_stop_sending(conn, stream_id, app_error),
                ConnectionEvent::SendStreamComplete { .. } => {}
                ConnectionEvent::SendStreamCreatable { .. } => {}
                ConnectionEvent::AuthenticationNeeded => return Err(Error::HttpInternalError),
                ConnectionEvent::StateChange(state) => {
                    if self.base_handler.handle_state_change(conn, &state)? {
                        self.events
                            .connection_state_change(self.base_handler.state());
                    }
                }
                ConnectionEvent::ZeroRttRejected => return Err(Error::HttpInternalError),
            }
        }
        Ok(())
    }

    fn handle_stream_readable(&mut self, conn: &mut Connection, stream_id: u64) -> Res<()> {
        match self.base_handler.handle_stream_readable(conn, stream_id)? {
            HandleReadableOutput::PushStream => Err(Error::HttpStreamCreationError),
            HandleReadableOutput::ControlFrames(control_frames) => {
                for f in control_frames.into_iter() {
                    match f {
                        HFrame::MaxPushId { .. } => {
                            // TODO implement push
                            Ok(())
                        }
                        HFrame::Goaway { .. } => Err(Error::HttpFrameUnexpected),
                        _ => unreachable!(
                            "we should only put MaxPushId and Goaway into control_frames."
                        ),
                    }?;
                }
                Ok(())
            }
            _ => Ok(()),
        }
    }

    fn handle_stream_stop_sending(
        &mut self,
        conn: &mut Connection,
        stop_stream_id: u64,
        app_err: AppError,
    ) {
        if let Some(t) = self.base_handler.transactions.get_mut(&stop_stream_id) {
            // close sending side.
            t.stop_sending();
            // receiving side may be closed already, just ignore an error in the following line.
            let _ = conn.stream_stop_sending(stop_stream_id, app_err);
            t.reset_receiving_side();
            self.base_handler.transactions.remove(&stop_stream_id);
        }
    }
}
