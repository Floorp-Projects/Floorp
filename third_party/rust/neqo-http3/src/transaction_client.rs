// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::hframe::HFrame;

use crate::client_events::Http3ClientEvents;
use crate::connection::Http3Transaction;
use crate::response_stream::ResponseStream;
use crate::Header;
use neqo_common::{qinfo, Encoder};
use neqo_qpack::decoder::QPackDecoder;
use neqo_qpack::encoder::QPackEncoder;
use neqo_transport::Connection;

use crate::{Error, Res};
use std::cmp::min;

const MAX_DATA_HEADER_SIZE_2: usize = (1 << 6) - 1; // Maximal amount of data with DATA frame header size 2
const MAX_DATA_HEADER_SIZE_2_LIMIT: usize = MAX_DATA_HEADER_SIZE_2 + 3; // 63 + 3 (size of the next buffer data frame header)
const MAX_DATA_HEADER_SIZE_3: usize = (1 << 14) - 1; // Maximal amount of data with DATA frame header size 3
const MAX_DATA_HEADER_SIZE_3_LIMIT: usize = MAX_DATA_HEADER_SIZE_3 + 5; // 16383 + 5 (size of the next buffer data frame header)
const MAX_DATA_HEADER_SIZE_5: usize = (1 << 30) - 1; // Maximal amount of data with DATA frame header size 3
const MAX_DATA_HEADER_SIZE_5_LIMIT: usize = MAX_DATA_HEADER_SIZE_5 + 9; // 1073741823 + 9 (size of the next buffer data frame header)

#[derive(PartialEq, Debug)]
struct Request {
    method: String,
    scheme: String,
    host: String,
    path: String,
    headers: Vec<Header>,
    buf: Option<Vec<u8>>,
}

impl Request {
    pub fn new(method: &str, scheme: &str, host: &str, path: &str, headers: &[Header]) -> Self {
        let mut r = Self {
            method: method.to_owned(),
            scheme: scheme.to_owned(),
            host: host.to_owned(),
            path: path.to_owned(),
            headers: Vec::new(),
            buf: None,
        };
        r.headers.push((":method".into(), method.to_owned()));
        r.headers.push((":scheme".into(), r.scheme.clone()));
        r.headers.push((":authority".into(), r.host.clone()));
        r.headers.push((":path".into(), r.path.clone()));
        r.headers.extend_from_slice(headers);
        r
    }

    pub fn ensure_encoded(&mut self, encoder: &mut QPackEncoder, stream_id: u64) {
        if self.buf.is_some() {
            return;
        }

        qinfo!([self], "Encoding headers for {}/{}", self.host, self.path);
        let header_block = encoder.encode_header_block(&self.headers, stream_id);
        let f = HFrame::Headers {
            header_block: header_block.to_vec(),
        };
        let mut d = Encoder::default();
        f.encode(&mut d);
        self.buf = Some(d.into());
    }

    pub fn send(
        &mut self,
        conn: &mut Connection,
        encoder: &mut QPackEncoder,
        stream_id: u64,
    ) -> Res<bool> {
        let label = if ::log::log_enabled!(::log::Level::Debug) {
            format!("{}", self)
        } else {
            String::new()
        };
        self.ensure_encoded(encoder, stream_id);
        if let Some(buf) = &mut self.buf {
            let sent = conn.stream_send(stream_id, &buf)?;
            qinfo!([label], "{} bytes sent", sent);

            if sent == buf.len() {
                qinfo!([label], "done sending request");
                Ok(true)
            } else {
                let b = buf.split_off(sent);
                self.buf = Some(b);
                Ok(false)
            }
        } else {
            panic!("We must have buffer in this state")
        }
    }
}

impl ::std::fmt::Display for Request {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Request {} {}/{}", self.method, self.host, self.path)
    }
}

/*
 *  Transaction send states:
 *    SendingHeaders : sending headers. From here we may switch to SendingData
 *                     or Closed (if the app does not want to send data and
 *                     has alreadyclosed the send stream).
 *    SendingData : We are sending request data until the app closes the stream.
 *    Closed
 */

#[derive(PartialEq, Debug)]
enum TransactionSendState {
    SendingHeaders { request: Request, fin: bool },
    SendingData,
    Closed,
}

//  This is used for normal request/responses.
#[derive(Debug)]
pub struct TransactionClient {
    send_state: TransactionSendState,
    response_stream: ResponseStream,
    stream_id: u64,
    conn_events: Http3ClientEvents,
}

impl TransactionClient {
    pub fn new(
        stream_id: u64,
        method: &str,
        scheme: &str,
        host: &str,
        path: &str,
        headers: &[Header],
        conn_events: Http3ClientEvents,
    ) -> Self {
        qinfo!("Create a request stream_id={}", stream_id);
        Self {
            send_state: TransactionSendState::SendingHeaders {
                request: Request::new(method, scheme, host, path, headers),
                fin: false,
            },
            response_stream: ResponseStream::new(stream_id, conn_events.clone()),
            stream_id,
            conn_events,
        }
    }

    pub fn send_request_body(&mut self, conn: &mut Connection, buf: &[u8]) -> Res<usize> {
        qinfo!(
            [self],
            "send_request_body: send_state={:?} len={}",
            self.send_state,
            buf.len()
        );
        match self.send_state {
            TransactionSendState::SendingHeaders { .. } => Ok(0),
            TransactionSendState::SendingData => {
                let available = conn.stream_avail_send_space(self.stream_id)? as usize;
                if available <= 2 {
                    return Ok(0);
                }
                let to_send;
                if available <= MAX_DATA_HEADER_SIZE_2_LIMIT {
                    // 63 + 3
                    to_send = min(min(buf.len(), available - 2), MAX_DATA_HEADER_SIZE_2);
                } else if available <= MAX_DATA_HEADER_SIZE_3_LIMIT {
                    // 16383 + 5
                    to_send = min(min(buf.len(), available - 3), MAX_DATA_HEADER_SIZE_3);
                } else if available <= MAX_DATA_HEADER_SIZE_5 {
                    // 1073741823 + 9
                    to_send = min(min(buf.len(), available - 5), MAX_DATA_HEADER_SIZE_5_LIMIT);
                } else {
                    to_send = min(buf.len(), available - 9);
                }

                qinfo!(
                    [self],
                    "send_request_body: available={} to_send={}.",
                    available,
                    to_send
                );

                let data_frame = HFrame::Data {
                    len: to_send as u64,
                };
                let mut enc = Encoder::default();
                data_frame.encode(&mut enc);
                match conn.stream_send(self.stream_id, &enc) {
                    Ok(sent) => {
                        debug_assert_eq!(sent, enc.len());
                    }
                    Err(e) => return Err(Error::TransportError(e)),
                }
                match conn.stream_send(self.stream_id, &buf[..to_send]) {
                    Ok(sent) => Ok(sent),
                    Err(e) => Err(Error::TransportError(e)),
                }
            }
            TransactionSendState::Closed => Err(Error::AlreadyClosed),
        }
    }

    pub fn is_sending_closed(&self) -> bool {
        match self.send_state {
            TransactionSendState::SendingHeaders { fin, .. } => fin,
            TransactionSendState::SendingData => false,
            _ => true,
        }
    }

    pub fn read_response_headers(&mut self) -> Res<(Vec<Header>, bool)> {
        self.response_stream.read_response_headers()
    }

    pub fn read_response_data(
        &mut self,
        conn: &mut Connection,
        buf: &mut [u8],
    ) -> Res<(usize, bool)> {
        self.response_stream.read_response_data(conn, buf)
    }

    pub fn is_state_sending_data(&self) -> bool {
        self.send_state == TransactionSendState::SendingData
    }
}

impl ::std::fmt::Display for TransactionClient {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "TransactionClient {}", self.stream_id)
    }
}

impl Http3Transaction for TransactionClient {
    fn send(&mut self, conn: &mut Connection, encoder: &mut QPackEncoder) -> Res<()> {
        let label = if ::log::log_enabled!(::log::Level::Debug) {
            format!("{}", self)
        } else {
            String::new()
        };
        if let TransactionSendState::SendingHeaders {
            ref mut request,
            fin,
        } = self.send_state
        {
            if request.send(conn, encoder, self.stream_id)? {
                if fin {
                    conn.stream_close_send(self.stream_id)?;
                    self.send_state = TransactionSendState::Closed;
                    qinfo!([label], "done sending request");
                } else {
                    self.send_state = TransactionSendState::SendingData;
                    self.conn_events.data_writable(self.stream_id);
                    qinfo!([label], "change to state SendingData");
                }
            }
        }
        Ok(())
    }

    fn receive(&mut self, conn: &mut Connection, decoder: &mut QPackDecoder) -> Res<()> {
        self.response_stream.receive(conn, decoder)
    }

    fn has_data_to_send(&self) -> bool {
        if let TransactionSendState::SendingHeaders { .. } = self.send_state {
            true
        } else {
            false
        }
    }

    fn reset_receiving_side(&mut self) {
        self.response_stream.close();
    }

    fn stop_sending(&mut self) {
        self.send_state = TransactionSendState::Closed;
    }

    fn done(&self) -> bool {
        self.send_state == TransactionSendState::Closed && self.response_stream.is_closed()
    }

    fn close_send(&mut self, conn: &mut Connection) -> Res<()> {
        match self.send_state {
            TransactionSendState::SendingHeaders { ref mut fin, .. } => {
                *fin = true;
            }
            _ => {
                self.send_state = TransactionSendState::Closed;
                conn.stream_close_send(self.stream_id)?;
            }
        }
        Ok(())
    }
}
