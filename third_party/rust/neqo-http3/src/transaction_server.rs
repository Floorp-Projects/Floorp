// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::connection::Http3Transaction;
use crate::hframe::{HFrame, HFrameReader};
use crate::server_connection_events::Http3ServerConnEvents;
use crate::Header;
use crate::{Error, Res};
use neqo_common::{matches, qdebug, qinfo, qtrace, Encoder};
use neqo_qpack::decoder::QPackDecoder;
use neqo_qpack::encoder::QPackEncoder;
use neqo_transport::Connection;
use std::mem;

#[derive(PartialEq, Debug)]
enum TransactionRecvState {
    WaitingForHeaders,
    ReadingHeaders { buf: Vec<u8>, offset: usize },
    BlockedDecodingHeaders { buf: Vec<u8>, fin: bool },
    WaitingForData,
    ReadingData { remaining_data_len: usize },
    Closed,
}

#[derive(PartialEq, Debug)]
enum TransactionSendState {
    Initial,
    SendingResponse { buf: Vec<u8> },
    Closed,
}

#[derive(Debug)]
pub struct TransactionServer {
    recv_state: TransactionRecvState,
    send_state: TransactionSendState,
    stream_id: u64,
    frame_reader: HFrameReader,
    conn_events: Http3ServerConnEvents,
}

impl TransactionServer {
    pub fn new(stream_id: u64, conn_events: Http3ServerConnEvents) -> TransactionServer {
        qinfo!("Create a request stream_id={}", stream_id);
        TransactionServer {
            recv_state: TransactionRecvState::WaitingForHeaders,
            send_state: TransactionSendState::Initial,
            stream_id,
            frame_reader: HFrameReader::new(),
            conn_events,
        }
    }

    pub fn set_response(&mut self, headers: &[Header], data: Vec<u8>, encoder: &mut QPackEncoder) {
        qdebug!([self], "Encoding headers");
        let encoded_headers = encoder.encode_header_block(&headers, self.stream_id);
        let hframe = HFrame::Headers {
            len: encoded_headers.len() as u64,
        };
        let mut d = Encoder::default();
        hframe.encode(&mut d);
        d.encode(&encoded_headers);
        if !data.is_empty() {
            qdebug!([self], "Encoding data");
            let d_frame = HFrame::Data {
                len: data.len() as u64,
            };
            d_frame.encode(&mut d);
            d.encode(&data);
        }

        self.send_state = TransactionSendState::SendingResponse { buf: d.into() };
    }

    fn recv_frame_header(&mut self, conn: &mut Connection) -> Res<(Option<HFrame>, bool)> {
        qtrace!([self], "receiving frame header");
        let fin = self.frame_reader.receive(conn, self.stream_id)?;
        if !self.frame_reader.done() {
            Ok((None, fin))
        } else {
            qinfo!([self], "A new frame has been received.");
            Ok((Some(self.frame_reader.get_frame()?), fin))
        }
    }

    fn read_headers_frame_body(
        &mut self,
        conn: &mut Connection,
        decoder: &mut QPackDecoder,
    ) -> Res<bool> {
        let label = if ::log::log_enabled!(::log::Level::Debug) {
            format!("{}", self)
        } else {
            String::new()
        };
        if let TransactionRecvState::ReadingHeaders {
            ref mut buf,
            ref mut offset,
        } = self.recv_state
        {
            let (amount, fin) = conn.stream_recv(self.stream_id, &mut buf[*offset..])?;
            qdebug!([label], "read_headers: read {} bytes fin={}.", amount, fin);
            *offset += amount as usize;
            if *offset < buf.len() {
                if fin {
                    // Malformed frame
                    return Err(Error::HttpFrameError);
                }
                return Ok(true);
            }

            // we have read the headers, try decoding them.
            qinfo!(
                [label],
                "read_headers: read all headers, try decoding them."
            );
            match decoder.decode_header_block(buf, self.stream_id)? {
                Some(headers) => {
                    self.conn_events.headers(self.stream_id, headers, fin);
                    if fin {
                        self.recv_state = TransactionRecvState::Closed;
                    } else {
                        self.recv_state = TransactionRecvState::WaitingForData;
                    }
                    Ok(fin)
                }
                None => {
                    let mut tmp: Vec<u8> = Vec::new();
                    mem::swap(&mut tmp, buf);
                    self.recv_state =
                        TransactionRecvState::BlockedDecodingHeaders { buf: tmp, fin };
                    Ok(true)
                }
            }
        } else {
            panic!("This is only called when recv_state is ReadingHeaders.");
        }
    }

    fn handle_frame_in_state_waiting_for_headers(&mut self, frame: HFrame, fin: bool) -> Res<()> {
        qdebug!([self], "A new frame has been received: {:?}", frame);
        match frame {
            HFrame::Headers { len } => self.handle_headers_frame(len, fin),
            _ => Err(Error::HttpFrameUnexpected),
        }
    }

    fn handle_frame_in_state_waiting_for_data(&mut self, frame: HFrame, fin: bool) -> Res<()> {
        qdebug!([self], "A new frame has been received: {:?}", frame);
        match frame {
            HFrame::Data { len } => self.handle_data_frame(len, fin),
            _ => Err(Error::HttpFrameUnexpected),
        }
    }

    fn handle_headers_frame(&mut self, len: u64, fin: bool) -> Res<()> {
        qinfo!([self], "A new header frame len={} fin={}", len, fin);
        if len == 0 {
            self.conn_events.headers(self.stream_id, Vec::new(), fin);
        } else {
            if fin {
                return Err(Error::HttpFrameError);
            }
            self.recv_state = TransactionRecvState::ReadingHeaders {
                buf: vec![0; len as usize],
                offset: 0,
            };
        }
        Ok(())
    }

    fn handle_data_frame(&mut self, len: u64, fin: bool) -> Res<()> {
        qinfo!([self], "A new data frame len={} fin={}", len, fin);
        if len > 0 {
            if fin {
                return Err(Error::HttpFrameError);
            }
            self.recv_state = TransactionRecvState::ReadingData {
                remaining_data_len: len as usize,
            };
        } else if fin {
            self.conn_events.data(self.stream_id, Vec::new(), true);
            self.recv_state = TransactionRecvState::Closed;
        }
        Ok(())
    }
}

impl ::std::fmt::Display for TransactionServer {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "TransactionServer {}", self.stream_id)
    }
}

impl Http3Transaction for TransactionServer {
    fn send(&mut self, conn: &mut Connection, _encoder: &mut QPackEncoder) -> Res<()> {
        qtrace!([self], "Sending response.");
        let label = if ::log::log_enabled!(::log::Level::Debug) {
            format!("{}", self)
        } else {
            String::new()
        };
        if let TransactionSendState::SendingResponse { ref mut buf } = self.send_state {
            let sent = conn.stream_send(self.stream_id, &buf[..])?;
            qinfo!([label], "{} bytes sent", sent);
            if sent == buf.len() {
                conn.stream_close_send(self.stream_id)?;
                self.send_state = TransactionSendState::Closed;
                qinfo!([label], "done sending request");
            } else {
                let mut b = buf.split_off(sent);
                mem::swap(buf, &mut b);
            }
        }

        Ok(())
    }

    fn receive(&mut self, conn: &mut Connection, decoder: &mut QPackDecoder) -> Res<()> {
        let label = if ::log::log_enabled!(::log::Level::Debug) {
            format!("{}", self)
        } else {
            String::new()
        };

        loop {
            qtrace!(
                [label],
                "[recv_state={:?}] receiving data.",
                self.recv_state
            );
            match self.recv_state {
                TransactionRecvState::WaitingForHeaders => {
                    let (f, fin) = self.recv_frame_header(conn)?;
                    match f {
                        None => {
                            if fin {
                                self.conn_events.headers(self.stream_id, Vec::new(), true);
                                self.recv_state = TransactionRecvState::Closed;
                            }
                            return Ok(());
                        }
                        Some(new_f) => {
                            self.handle_frame_in_state_waiting_for_headers(new_f, fin)?;
                            if fin {
                                self.recv_state = TransactionRecvState::Closed;
                                return Ok(());
                            }
                        }
                    }
                }
                TransactionRecvState::ReadingHeaders { .. } => {
                    if self.read_headers_frame_body(conn, decoder)? {
                        break Ok(());
                    }
                }
                TransactionRecvState::BlockedDecodingHeaders { ref mut buf, fin } => {
                    match decoder.decode_header_block(buf, self.stream_id)? {
                        Some(headers) => {
                            self.conn_events.headers(self.stream_id, headers, fin);
                            if fin {
                                return Ok(());
                            }
                        }
                        None => {
                            qinfo!([self], "decoding header is blocked.");
                            return Ok(());
                        }
                    }
                }
                TransactionRecvState::WaitingForData => {
                    let (f, fin) = self.recv_frame_header(conn)?;
                    match f {
                        None => {
                            if fin {
                                self.conn_events.data(self.stream_id, Vec::new(), true);
                            }
                            return Ok(());
                        }
                        Some(new_f) => {
                            self.handle_frame_in_state_waiting_for_data(new_f, fin)?;
                            if fin {
                                return Ok(());
                            }
                        }
                    };
                }
                TransactionRecvState::ReadingData {
                    ref mut remaining_data_len,
                } => {
                    // TODO add available(stream_id) to neqo_transport.
                    assert!(*remaining_data_len > 0);
                    while *remaining_data_len != 0 {
                        let to_read = if *remaining_data_len > 1024 {
                            1024
                        } else {
                            *remaining_data_len
                        };

                        let mut data = vec![0x0; to_read];
                        let (amount, fin) = conn.stream_recv(self.stream_id, &mut data[..])?;
                        assert!(amount <= to_read);
                        if amount > 0 {
                            data.truncate(amount);
                            self.conn_events.data(self.stream_id, data, fin);
                            *remaining_data_len -= amount;
                        }
                        if fin {
                            if *remaining_data_len > 0 {
                                return Err(Error::HttpFrameError);
                            }
                            self.recv_state = TransactionRecvState::Closed;
                            return Ok(());
                        } else if *remaining_data_len == 0 {
                            self.recv_state = TransactionRecvState::WaitingForData;
                            break;
                        }
                        if amount == 0 {
                            return Ok(());
                        }
                    }
                }
                TransactionRecvState::Closed => {
                    panic!("Stream readable after being closed!");
                }
            };
        }
    }

    fn has_data_to_send(&self) -> bool {
        matches!(self.send_state, TransactionSendState::SendingResponse { .. })
    }

    fn reset_receiving_side(&mut self) {
        self.recv_state = TransactionRecvState::Closed;
    }

    fn stop_sending(&mut self) {}

    fn done(&self) -> bool {
        self.send_state == TransactionSendState::Closed
            && self.recv_state == TransactionRecvState::Closed
    }

    fn close_send(&mut self, _conn: &mut Connection) -> Res<()> {
        Ok(())
    }
}
