// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::WebTransportSession;
use crate::{
    CloseType, Http3StreamInfo, Http3StreamType, ReceiveOutput, RecvStream, RecvStreamEvents, Res,
    SendStream, SendStreamEvents, Stream,
};
use neqo_common::Encoder;
use neqo_transport::{Connection, RecvStreamStats, SendStreamStats, StreamId};
use std::cell::RefCell;
use std::rc::Rc;

pub const WEBTRANSPORT_UNI_STREAM: u64 = 0x54;
pub const WEBTRANSPORT_STREAM: u64 = 0x41;

#[derive(Debug)]
pub(crate) struct WebTransportRecvStream {
    stream_id: StreamId,
    events: Box<dyn RecvStreamEvents>,
    session: Rc<RefCell<WebTransportSession>>,
    session_id: StreamId,
    fin: bool,
}

impl WebTransportRecvStream {
    pub fn new(
        stream_id: StreamId,
        session_id: StreamId,
        events: Box<dyn RecvStreamEvents>,
        session: Rc<RefCell<WebTransportSession>>,
    ) -> Self {
        Self {
            stream_id,
            events,
            session_id,
            session,
            fin: false,
        }
    }

    fn get_info(&self) -> Http3StreamInfo {
        Http3StreamInfo::new(self.stream_id, self.stream_type())
    }
}

impl Stream for WebTransportRecvStream {
    fn stream_type(&self) -> Http3StreamType {
        Http3StreamType::WebTransport(self.session_id)
    }
}

impl RecvStream for WebTransportRecvStream {
    fn receive(&mut self, _conn: &mut Connection) -> Res<(ReceiveOutput, bool)> {
        self.events.data_readable(self.get_info());
        Ok((ReceiveOutput::NoOutput, false))
    }

    fn reset(&mut self, close_type: CloseType) -> Res<()> {
        if !matches!(close_type, CloseType::ResetApp(_)) {
            self.events.recv_closed(self.get_info(), close_type);
        }
        self.session.borrow_mut().remove_recv_stream(self.stream_id);
        Ok(())
    }

    fn read_data(&mut self, conn: &mut Connection, buf: &mut [u8]) -> Res<(usize, bool)> {
        let (amount, fin) = conn.stream_recv(self.stream_id, buf)?;
        self.fin = fin;
        if fin {
            self.session.borrow_mut().remove_recv_stream(self.stream_id);
        }
        Ok((amount, fin))
    }

    fn stats(&mut self, conn: &mut Connection) -> Res<RecvStreamStats> {
        const TYPE_LEN_UNI: usize = Encoder::varint_len(WEBTRANSPORT_UNI_STREAM);
        const TYPE_LEN_BIDI: usize = Encoder::varint_len(WEBTRANSPORT_STREAM);

        let stream_header_size = if self.stream_id.is_server_initiated() {
            let id_len = if self.stream_id.is_uni() {
                TYPE_LEN_UNI
            } else {
                TYPE_LEN_BIDI
            };
            (id_len + Encoder::varint_len(self.session_id.as_u64())) as u64
        } else {
            0
        };

        let stats = conn.recv_stream_stats(self.stream_id)?;
        if stream_header_size == 0 {
            return Ok(stats);
        }

        let subtract_non_app_bytes =
            |count: u64| -> u64 { count.saturating_sub(stream_header_size) };

        let bytes_received = subtract_non_app_bytes(stats.bytes_received());
        let bytes_read = subtract_non_app_bytes(stats.bytes_read());

        Ok(RecvStreamStats::new(bytes_received, bytes_read))
    }
}

#[derive(Debug, PartialEq)]
enum WebTransportSenderStreamState {
    SendingInit { buf: Vec<u8>, fin: bool },
    SendingData,
    Done,
}

#[derive(Debug)]
pub(crate) struct WebTransportSendStream {
    stream_id: StreamId,
    state: WebTransportSenderStreamState,
    events: Box<dyn SendStreamEvents>,
    session: Rc<RefCell<WebTransportSession>>,
    session_id: StreamId,
}

impl WebTransportSendStream {
    pub fn new(
        stream_id: StreamId,
        session_id: StreamId,
        events: Box<dyn SendStreamEvents>,
        session: Rc<RefCell<WebTransportSession>>,
        local: bool,
    ) -> Self {
        Self {
            stream_id,
            state: if local {
                let mut d = Encoder::default();
                if stream_id.is_uni() {
                    d.encode_varint(WEBTRANSPORT_UNI_STREAM);
                } else {
                    d.encode_varint(WEBTRANSPORT_STREAM);
                }
                d.encode_varint(session_id.as_u64());
                WebTransportSenderStreamState::SendingInit {
                    buf: d.into(),
                    fin: false,
                }
            } else {
                WebTransportSenderStreamState::SendingData
            },
            events,
            session_id,
            session,
        }
    }

    fn set_done(&mut self, close_type: CloseType) {
        self.state = WebTransportSenderStreamState::Done;
        self.events.send_closed(self.get_info(), close_type);
        self.session.borrow_mut().remove_send_stream(self.stream_id);
    }

    fn get_info(&self) -> Http3StreamInfo {
        Http3StreamInfo::new(self.stream_id, self.stream_type())
    }
}

impl Stream for WebTransportSendStream {
    fn stream_type(&self) -> Http3StreamType {
        Http3StreamType::WebTransport(self.session_id)
    }
}

impl SendStream for WebTransportSendStream {
    fn send(&mut self, conn: &mut Connection) -> Res<()> {
        if let WebTransportSenderStreamState::SendingInit { ref mut buf, fin } = self.state {
            let sent = conn.stream_send(self.stream_id, &buf[..])?;
            if sent == buf.len() {
                if fin {
                    conn.stream_close_send(self.stream_id)?;
                    self.set_done(CloseType::Done);
                } else {
                    self.state = WebTransportSenderStreamState::SendingData;
                }
            } else {
                let b = buf.split_off(sent);
                *buf = b;
            }
        }
        Ok(())
    }

    fn has_data_to_send(&self) -> bool {
        matches!(
            self.state,
            WebTransportSenderStreamState::SendingInit { .. }
        )
    }

    fn stream_writable(&self) {
        self.events.data_writable(self.get_info());
    }

    fn done(&self) -> bool {
        self.state == WebTransportSenderStreamState::Done
    }

    fn send_data(&mut self, conn: &mut Connection, buf: &[u8]) -> Res<usize> {
        self.send(conn)?;
        if self.state == WebTransportSenderStreamState::SendingData {
            let sent = conn.stream_send(self.stream_id, buf)?;
            Ok(sent)
        } else {
            Ok(0)
        }
    }

    fn set_sendorder(&mut self, conn: &mut Connection, sendorder: Option<i64>) -> Res<()> {
        conn.stream_sendorder(self.stream_id, sendorder)
            .map_err(|_| crate::Error::InvalidStreamId)
    }

    fn set_fairness(&mut self, conn: &mut Connection, fairness: bool) -> Res<()> {
        conn.stream_fairness(self.stream_id, fairness)
            .map_err(|_| crate::Error::InvalidStreamId)
    }

    fn handle_stop_sending(&mut self, close_type: CloseType) {
        self.set_done(close_type);
    }

    fn close(&mut self, conn: &mut Connection) -> Res<()> {
        if let WebTransportSenderStreamState::SendingInit { ref mut fin, .. } = self.state {
            *fin = true;
        } else {
            self.state = WebTransportSenderStreamState::Done;
            conn.stream_close_send(self.stream_id)?;
            self.set_done(CloseType::Done);
        }
        Ok(())
    }

    fn stats(&mut self, conn: &mut Connection) -> Res<SendStreamStats> {
        const TYPE_LEN_UNI: usize = Encoder::varint_len(WEBTRANSPORT_UNI_STREAM);
        const TYPE_LEN_BIDI: usize = Encoder::varint_len(WEBTRANSPORT_STREAM);

        let stream_header_size = if self.stream_id.is_client_initiated() {
            let id_len = if self.stream_id.is_uni() {
                TYPE_LEN_UNI
            } else {
                TYPE_LEN_BIDI
            };
            (id_len + Encoder::varint_len(self.session_id.as_u64())) as u64
        } else {
            0
        };

        let stats = conn.send_stream_stats(self.stream_id)?;
        if stream_header_size == 0 {
            return Ok(stats);
        }

        let subtract_non_app_bytes =
            |count: u64| -> u64 { count.saturating_sub(stream_header_size) };

        let bytes_written = subtract_non_app_bytes(stats.bytes_written());
        let bytes_sent = subtract_non_app_bytes(stats.bytes_sent());
        let bytes_acked = subtract_non_app_bytes(stats.bytes_acked());
        Ok(SendStreamStats::new(bytes_written, bytes_sent, bytes_acked))
    }
}
