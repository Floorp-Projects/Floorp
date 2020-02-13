// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::hframe::{HFrame, HFrameReader};

use crate::client_events::Http3ClientEvents;
use crate::connection::Http3Transaction;
use crate::Header;
use neqo_common::{qdebug, qinfo, qtrace, Encoder};
use neqo_qpack::decoder::QPackDecoder;
use neqo_qpack::encoder::QPackEncoder;
use neqo_transport::Connection;

use crate::{Error, Res};
use std::cmp::min;
use std::mem;

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
    pub fn new(method: &str, scheme: &str, host: &str, path: &str, headers: &[Header]) -> Request {
        let mut r = Request {
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
        let encoded_headers = encoder.encode_header_block(&self.headers, stream_id);
        let f = HFrame::Headers {
            len: encoded_headers.len() as u64,
        };
        let mut d = Encoder::default();
        f.encode(&mut d);
        d.encode(&encoded_headers[..]);
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

/*
 * Transaction receive state:
 *    WaitingForResponseHeaders : we wait for headers. in this state we can
 *                                also get a PUSH_PROMISE frame.
 *    ReadingHeaders : we have HEADERS frame and now we are reading header
 *                     block. This may block on encoder instructions. In this
 *                     state we do no read from the stream.
 *    BlockedDecodingHeaders : Decoding headers is blocked on encoder
 *                             instructions.
 *    WaitingForData : we got HEADERS, we are waiting for one or more data
 *                     frames. In this state we can receive one or more
 *                     PUSH_PROMIS frames or a HEADERS frame carrying trailers.
 *    ReadingData : we got a DATA frame, now we letting the app read payload.
 *                  From here we will go back to WaitingForData state to wait
 *                  for more data frames or to CLosed state
 *    ReadingTrailers : reading trailers.
 *    ClosePending : waiting for app to pick up data, after that we can delete
 * the TransactionClient.
 *    Closed
 */
#[derive(PartialEq, Debug)]
enum TransactionRecvState {
    WaitingForResponseHeaders,
    ReadingHeaders { buf: Vec<u8>, offset: usize },
    BlockedDecodingHeaders { buf: Vec<u8>, fin: bool },
    WaitingForData,
    ReadingData { remaining_data_len: usize },
    //    ReadingTrailers,
    ClosePending, // Close must first be read by application
    Closed,
}

#[derive(Debug, PartialEq)]
enum ResponseHeadersState {
    NoHeaders,
    Ready(Option<Vec<Header>>),
    Read,
}

//  This is used for normal request/responses.
#[derive(Debug)]
pub struct TransactionClient {
    send_state: TransactionSendState,
    recv_state: TransactionRecvState,
    stream_id: u64,
    frame_reader: HFrameReader,
    response_headers_state: ResponseHeadersState,
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
    ) -> TransactionClient {
        qinfo!("Create a request stream_id={}", stream_id);
        TransactionClient {
            send_state: TransactionSendState::SendingHeaders {
                request: Request::new(method, scheme, host, path, headers),
                fin: false,
            },
            recv_state: TransactionRecvState::WaitingForResponseHeaders,
            stream_id,
            response_headers_state: ResponseHeadersState::NoHeaders,
            frame_reader: HFrameReader::new(),
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

    fn handle_frame_in_state_waiting_for_headers(&mut self, frame: HFrame, fin: bool) -> Res<()> {
        qinfo!(
            [self],
            "A new frame has been received: {:?}; state={:?}",
            frame,
            self.recv_state
        );
        match frame {
            HFrame::Headers { len } => self.handle_headers_frame(len, fin),
            HFrame::PushPromise { .. } => Err(Error::HttpIdError),
            _ => Err(Error::HttpFrameUnexpected),
        }
    }

    fn handle_headers_frame(&mut self, len: u64, fin: bool) -> Res<()> {
        if len == 0 {
            self.add_headers(None)
        } else {
            if fin {
                return Err(Error::HttpFrameError);
            }
            self.recv_state = TransactionRecvState::ReadingHeaders {
                buf: vec![0; len as usize],
                offset: 0,
            };
            Ok(())
        }
    }

    fn handle_frame_in_state_waiting_for_data(&mut self, frame: HFrame, fin: bool) -> Res<()> {
        qinfo!(
            [self],
            "A new frame has been received: {:?}; state={:?}",
            frame,
            self.recv_state
        );
        match frame {
            HFrame::Data { len } => self.handle_data_frame(len, fin),
            HFrame::PushPromise { .. } => Err(Error::HttpIdError),
            HFrame::Headers { .. } => {
                // TODO implement trailers!
                Err(Error::HttpFrameUnexpected)
            }
            _ => Err(Error::HttpFrameUnexpected),
        }
    }

    fn handle_data_frame(&mut self, len: u64, fin: bool) -> Res<()> {
        if len > 0 {
            if fin {
                return Err(Error::HttpFrameError);
            }
            self.recv_state = TransactionRecvState::ReadingData {
                remaining_data_len: len as usize,
            };
        }
        Ok(())
    }

    fn add_headers(&mut self, headers: Option<Vec<Header>>) -> Res<()> {
        if self.response_headers_state != ResponseHeadersState::NoHeaders {
            return Err(Error::HttpInternalError);
        }
        self.response_headers_state = ResponseHeadersState::Ready(headers);
        self.conn_events.header_ready(self.stream_id);
        self.recv_state = TransactionRecvState::WaitingForData;
        Ok(())
    }

    fn set_state_to_close_pending(&mut self) {
        // Stream has received fin. Depending on headers state set header_ready
        // or data_readable event so that app can pick up the fin.
        qdebug!(
            [self],
            "set_state_to_close_pending:  response_headers_state={:?}",
            self.response_headers_state
        );
        match self.response_headers_state {
            ResponseHeadersState::NoHeaders => {
                self.conn_events.header_ready(self.stream_id);
                self.response_headers_state = ResponseHeadersState::Ready(None);
            }
            // In Ready state we are already waiting for app to pick up headers
            // it can also pick up fin, so we do not need a new event.
            ResponseHeadersState::Ready(..) => {}
            ResponseHeadersState::Read => self.conn_events.data_readable(self.stream_id),
        }
        self.recv_state = TransactionRecvState::ClosePending;
    }

    fn recv_frame_header(&mut self, conn: &mut Connection) -> Res<Option<(HFrame, bool)>> {
        qtrace!([self], "receiving frame header");
        let fin = self.frame_reader.receive(conn, self.stream_id)?;
        if !self.frame_reader.done() {
            if fin {
                //we have received stream fin while waiting for a frame.
                // !self.frame_reader.done() means that we do not have a new
                // frame at all. Set state to ClosePending and waith for app
                // to pick up fin.
                self.set_state_to_close_pending();
            }
            Ok(None)
        } else {
            qdebug!([self], "A new frame has been received.");
            Ok(Some((self.frame_reader.get_frame()?, fin)))
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
                    // Malformated frame
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
                    self.add_headers(Some(headers))?;
                    if fin {
                        self.set_state_to_close_pending();
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

    pub fn is_sending_closed(&self) -> bool {
        match self.send_state {
            TransactionSendState::SendingHeaders { fin, .. } => fin,
            TransactionSendState::SendingData => false,
            _ => true,
        }
    }

    pub fn read_response_headers(&mut self) -> Res<(Vec<Header>, bool)> {
        if let ResponseHeadersState::Ready(ref mut headers) = self.response_headers_state {
            let mut tmp = Vec::new();
            if let Some(ref mut hdrs) = headers {
                mem::swap(&mut tmp, hdrs);
            }
            self.response_headers_state = ResponseHeadersState::Read;
            let mut fin = false;
            if self.recv_state == TransactionRecvState::ClosePending {
                fin = true;
                self.recv_state = TransactionRecvState::Closed;
            }
            Ok((tmp, fin))
        } else {
            Err(Error::Unavailable)
        }
    }

    pub fn read_response_data(
        &mut self,
        conn: &mut Connection,
        buf: &mut [u8],
    ) -> Res<(usize, bool)> {
        match self.recv_state {
            TransactionRecvState::ReadingData {
                ref mut remaining_data_len,
            } => {
                let to_read = if *remaining_data_len > buf.len() {
                    buf.len()
                } else {
                    *remaining_data_len
                };
                let (amount, fin) = conn.stream_recv(self.stream_id, &mut buf[..to_read])?;
                debug_assert!(amount <= to_read);
                *remaining_data_len -= amount;

                if fin {
                    if *remaining_data_len > 0 {
                        return Err(Error::HttpFrameError);
                    }
                    self.recv_state = TransactionRecvState::Closed;
                } else if *remaining_data_len == 0 {
                    self.recv_state = TransactionRecvState::WaitingForData;
                }

                Ok((amount, fin))
            }
            TransactionRecvState::ClosePending => {
                self.recv_state = TransactionRecvState::Closed;
                Ok((0, true))
            }
            _ => Ok((0, false)),
        }
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
        let label = if ::log::log_enabled!(::log::Level::Debug) {
            format!("{}", self)
        } else {
            String::new()
        };
        loop {
            qdebug!(
                [label],
                "send_state={:?} recv_state={:?}.",
                self.send_state,
                self.recv_state
            );
            match self.recv_state {
                TransactionRecvState::WaitingForResponseHeaders => {
                    match self.recv_frame_header(conn)? {
                        None => break Ok(()),
                        Some((f, fin)) => {
                            self.handle_frame_in_state_waiting_for_headers(f, fin)?;
                            if fin {
                                self.set_state_to_close_pending();
                                break Ok(());
                            }
                        }
                    };
                }
                TransactionRecvState::ReadingHeaders { .. } => {
                    if self.read_headers_frame_body(conn, decoder)? {
                        break Ok(());
                    }
                }
                TransactionRecvState::BlockedDecodingHeaders { ref buf, fin } => {
                    match decoder.decode_header_block(buf, self.stream_id)? {
                        Some(headers) => {
                            self.add_headers(Some(headers))?;
                            if fin {
                                self.set_state_to_close_pending();
                                break Ok(());
                            }
                        }
                        None => {
                            qinfo!([self], "decoding header is blocked.");
                            break Ok(());
                        }
                    }
                }
                TransactionRecvState::WaitingForData => {
                    match self.recv_frame_header(conn)? {
                        None => break Ok(()),
                        Some((f, fin)) => {
                            self.handle_frame_in_state_waiting_for_data(f, fin)?;
                            if fin {
                                self.set_state_to_close_pending();
                                break Ok(());
                            }
                        }
                    };
                }
                TransactionRecvState::ReadingData { .. } => {
                    self.conn_events.data_readable(self.stream_id);
                    break Ok(());
                }
                // TransactionRecvState::ReadingTrailers => break Ok(()),
                TransactionRecvState::ClosePending => {
                    panic!("Stream readable after being closed!");
                }
                TransactionRecvState::Closed => {
                    panic!("Stream readable after being closed!");
                }
            };
        }
    }

    fn has_data_to_send(&self) -> bool {
        if let TransactionSendState::SendingHeaders { .. } = self.send_state {
            true
        } else {
            false
        }
    }

    fn reset_receiving_side(&mut self) {
        self.recv_state = TransactionRecvState::Closed;
    }

    fn stop_sending(&mut self) {
        self.send_state = TransactionSendState::Closed;
    }

    fn done(&self) -> bool {
        self.send_state == TransactionSendState::Closed
            && self.recv_state == TransactionRecvState::Closed
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
