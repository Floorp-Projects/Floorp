// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::connection::Http3Transaction;
use crate::hframe::{HFrame, HFrameReader};
use crate::Header;
use crate::{Error, Res};
use neqo_common::{qdebug, Encoder};
use neqo_qpack::decoder::QPackDecoder;
use neqo_qpack::encoder::QPackEncoder;
use neqo_transport::Connection;
use std::mem;

pub type Response = (Vec<Header>, Vec<u8>, Option<Error>);
pub type RequestHandler = Box<dyn FnMut(&[Header], bool) -> Response>;

#[derive(PartialEq, Debug)]
enum TransactionState {
    WaitingForRequestHeaders,
    ReadingRequestHeaders { buf: Vec<u8>, offset: usize },
    BlockedDecodingHeaders { buf: Vec<u8> },
    ReadingRequestDone,
    SendingResponse,
    Error,
    Closed,
}

#[derive(Debug)]
pub struct TransactionServer {
    state: TransactionState,
    stream_id: u64,
    frame_reader: HFrameReader,
    request_headers: Option<Vec<Header>>,
    response_buf: Option<Vec<u8>>,
    fin: bool,
}

impl TransactionServer {
    pub fn new(stream_id: u64) -> TransactionServer {
        TransactionServer {
            state: TransactionState::WaitingForRequestHeaders,
            stream_id,
            frame_reader: HFrameReader::new(),
            request_headers: None,
            response_buf: None,
            fin: false,
        }
    }

    pub fn get_request_headers(&self) -> &[Header] {
        if let Some(h) = &self.request_headers {
            h
        } else {
            &[]
        }
    }

    pub fn set_response(&mut self, headers: &[Header], data: Vec<u8>, encoder: &mut QPackEncoder) {
        qdebug!([self] "Encoding headers");
        let encoded_headers = encoder.encode_header_block(&headers, self.stream_id);
        let hframe = HFrame::Headers {
            len: encoded_headers.len() as u64,
        };
        let mut d = Encoder::default();
        hframe.encode(&mut d);
        d.encode(&encoded_headers);
        if !data.is_empty() {
            qdebug!([self] "Encoding data");
            let d_frame = HFrame::Data {
                len: data.len() as u64,
            };
            d_frame.encode(&mut d);
            d.encode(&data);
        }
        self.response_buf = Some(d.into());

        self.state = TransactionState::SendingResponse;
    }

    fn recv_frame(&mut self, conn: &mut Connection) -> Res<()> {
        if self.frame_reader.receive(conn, self.stream_id)? {
            self.state = TransactionState::Closed;
        }
        Ok(())
    }

    fn handle_headers_frame(&mut self, len: u64) -> Res<()> {
        if self.state == TransactionState::Closed {
            return Ok(());
        }
        if len == 0 {
            self.state = TransactionState::Error;
        } else {
            self.state = TransactionState::ReadingRequestHeaders {
                buf: vec![0; len as usize],
                offset: 0,
            };
        }
        Ok(())
    }

    pub fn done_reading_request(&self) -> bool {
        self.state == TransactionState::ReadingRequestDone
    }
}

impl ::std::fmt::Display for TransactionServer {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "TransactionServer {}", self.stream_id)
    }
}

impl Http3Transaction for TransactionServer {
    fn send(&mut self, conn: &mut Connection, _encoder: &mut QPackEncoder) -> Res<()> {
        let label = if ::log::log_enabled!(::log::Level::Debug) {
            format!("{}", self)
        } else {
            String::new()
        };
        if self.state == TransactionState::SendingResponse {
            if let Some(d) = &mut self.response_buf {
                let sent = conn.stream_send(self.stream_id, &d[..])?;
                qdebug!([label] "{} bytes sent", sent);
                if sent == d.len() {
                    self.response_buf = None;
                    conn.stream_close_send(self.stream_id)?;
                    self.state = TransactionState::Closed;
                    qdebug!([label] "done sending request");
                } else {
                    let b = d.split_off(sent);
                    self.response_buf = Some(b);
                }
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
        qdebug!([label] "state={:?}: receiving data.", self.state);
        loop {
            match self.state {
                TransactionState::WaitingForRequestHeaders => {
                    self.recv_frame(conn)?;
                    if !self.frame_reader.done() {
                        break Ok(());
                    }
                    qdebug!([label] "received a frame");
                    match self.frame_reader.get_frame()? {
                        HFrame::Headers { len } => self.handle_headers_frame(len)?,
                        _ => {
                            break { Err(Error::HttpFrameUnexpected) };
                        }
                    };
                }
                TransactionState::ReadingRequestHeaders {
                    ref mut buf,
                    ref mut offset,
                } => {
                    let (amount, fin) = conn.stream_recv(self.stream_id, &mut buf[*offset..])?;
                    qdebug!(
                        [label]
                        "state=ReadingHeaders: read {} bytes fin={}.",
                        amount,
                        fin
                    );
                    *offset += amount as usize;
                    self.fin = fin;
                    if fin && *offset < buf.len() {
                        self.state = TransactionState::Error;
                        break Ok(());
                    }
                    if *offset < buf.len() {
                        break Ok(());
                    }
                    // we have read the headers.
                    self.request_headers = decoder.decode_header_block(buf, self.stream_id)?;
                    if self.request_headers.is_none() {
                        qdebug!([label] "decoding header is blocked.");
                        let mut tmp: Vec<u8> = Vec::new();
                        mem::swap(&mut tmp, buf);
                        self.state = TransactionState::BlockedDecodingHeaders { buf: tmp };
                    } else {
                        self.state = TransactionState::ReadingRequestDone;
                    }
                    break Ok(());
                }
                TransactionState::BlockedDecodingHeaders { ref mut buf } => {
                    self.request_headers = decoder.decode_header_block(buf, self.stream_id)?;
                    if self.request_headers.is_some() {
                        self.state = TransactionState::ReadingRequestDone;
                    } else {
                        qdebug!([self] "decoding header is still blocked.");
                    }
                    break Ok(());
                }
                TransactionState::ReadingRequestDone => break Ok(()),
                TransactionState::SendingResponse => break Ok(()),
                TransactionState::Error => break Ok(()),
                TransactionState::Closed => {
                    panic!("Stream readable after being closed!");
                }
            };
        }
    }

    fn has_data_to_send(&self) -> bool {
        false
    }

    fn is_state_sending_data(&self) -> bool {
        self.state == TransactionState::SendingResponse
    }

    fn reset_receiving_side(&mut self) {}

    fn stop_sending(&mut self) {}

    fn done(&self) -> bool {
        self.state == TransactionState::Closed
    }

    fn close_send(&mut self, _conn: &mut Connection) -> Res<()> {
        Ok(())
    }
}
