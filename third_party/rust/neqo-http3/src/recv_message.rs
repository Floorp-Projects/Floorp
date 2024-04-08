// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{cell::RefCell, cmp::min, collections::VecDeque, fmt::Debug, rc::Rc};

use neqo_common::{qdebug, qinfo, qtrace, Header};
use neqo_qpack::decoder::QPackDecoder;
use neqo_transport::{Connection, StreamId};

use crate::{
    frames::{FrameReader, HFrame, StreamReaderConnectionWrapper, H3_FRAME_TYPE_HEADERS},
    headers_checks::{headers_valid, is_interim},
    priority::PriorityHandler,
    push_controller::PushController,
    qlog, CloseType, Error, Http3StreamInfo, Http3StreamType, HttpRecvStream, HttpRecvStreamEvents,
    MessageType, Priority, ReceiveOutput, RecvStream, Res, Stream,
};

#[allow(clippy::module_name_repetitions)]
pub(crate) struct RecvMessageInfo {
    pub message_type: MessageType,
    pub stream_type: Http3StreamType,
    pub stream_id: StreamId,
    pub header_frame_type_read: bool,
}

/*
 * Response stream state:
 *    WaitingForResponseHeaders : we wait for headers. in this state we can
 *                                also get a PUSH_PROMISE frame.
 *    DecodingHeaders : In this step the headers will be decoded. The stream
 *                      may be blocked in this state on encoder instructions.
 *    WaitingForData : we got HEADERS, we are waiting for one or more data
 *                     frames. In this state we can receive one or more
 *                     PUSH_PROMIS frames or a HEADERS frame carrying trailers.
 *    ReadingData : we got a DATA frame, now we letting the app read payload.
 *                  From here we will go back to WaitingForData state to wait
 *                  for more data frames or to CLosed state
 *    ClosePending : waiting for app to pick up data, after that we can delete
 * the TransactionClient.
 *    Closed
 *    ExtendedConnect: this request is for a WebTransport session. In this
 *                         state RecvMessage will not be treated as a HTTP
 *                         stream anymore. It is waiting to be transformed
 *                         into WebTransport session or to be closed.
 */
#[derive(Debug)]
enum RecvMessageState {
    WaitingForResponseHeaders { frame_reader: FrameReader },
    DecodingHeaders { header_block: Vec<u8>, fin: bool },
    WaitingForData { frame_reader: FrameReader },
    ReadingData { remaining_data_len: usize },
    WaitingForFinAfterTrailers { frame_reader: FrameReader },
    ClosePending, // Close must first be read by application
    Closed,
    ExtendedConnect,
}

#[derive(Debug)]
struct PushInfo {
    push_id: u64,
    header_block: Vec<u8>,
}

#[derive(Debug)]
pub(crate) struct RecvMessage {
    state: RecvMessageState,
    message_type: MessageType,
    stream_type: Http3StreamType,
    qpack_decoder: Rc<RefCell<QPackDecoder>>,
    conn_events: Box<dyn HttpRecvStreamEvents>,
    push_handler: Option<Rc<RefCell<PushController>>>,
    stream_id: StreamId,
    priority_handler: PriorityHandler,
    blocked_push_promise: VecDeque<PushInfo>,
}

impl ::std::fmt::Display for RecvMessage {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "RecvMessage stream_id:{}", self.stream_id)
    }
}

impl RecvMessage {
    pub fn new(
        message_info: &RecvMessageInfo,
        qpack_decoder: Rc<RefCell<QPackDecoder>>,
        conn_events: Box<dyn HttpRecvStreamEvents>,
        push_handler: Option<Rc<RefCell<PushController>>>,
        priority_handler: PriorityHandler,
    ) -> Self {
        Self {
            state: RecvMessageState::WaitingForResponseHeaders {
                frame_reader: if message_info.header_frame_type_read {
                    FrameReader::new_with_type(H3_FRAME_TYPE_HEADERS)
                } else {
                    FrameReader::new()
                },
            },
            message_type: message_info.message_type,
            stream_type: message_info.stream_type,
            qpack_decoder,
            conn_events,
            push_handler,
            stream_id: message_info.stream_id,
            priority_handler,
            blocked_push_promise: VecDeque::new(),
        }
    }

    fn handle_headers_frame(&mut self, header_block: Vec<u8>, fin: bool) -> Res<()> {
        match self.state {
            RecvMessageState::WaitingForResponseHeaders {..} => {
                if header_block.is_empty() {
                    return Err(Error::HttpGeneralProtocolStream);
                }
                    self.state = RecvMessageState::DecodingHeaders { header_block, fin };
             }
            RecvMessageState::WaitingForData { ..} => {
                // TODO implement trailers, for now just ignore them.
                self.state = RecvMessageState::WaitingForFinAfterTrailers{frame_reader: FrameReader::new()};
            }
            RecvMessageState::WaitingForFinAfterTrailers {..} => {
                return Err(Error::HttpFrameUnexpected);
            }
            _ => unreachable!("This functions is only called in WaitingForResponseHeaders | WaitingForData | WaitingForFinAfterTrailers state.")
         }
        Ok(())
    }

    fn handle_data_frame(&mut self, len: u64, fin: bool) -> Res<()> {
        match self.state {
            RecvMessageState::WaitingForResponseHeaders {..} | RecvMessageState::WaitingForFinAfterTrailers {..} => {
                return Err(Error::HttpFrameUnexpected);
            }
            RecvMessageState::WaitingForData {..} => {
                if len > 0 {
                    if fin {
                        return Err(Error::HttpFrame);
                    }
                    self.state = RecvMessageState::ReadingData {
                        remaining_data_len: usize::try_from(len).or(Err(Error::HttpFrame))?,
                    };
                }
            }
            _ => unreachable!("This functions is only called in WaitingForResponseHeaders | WaitingForData | WaitingForFinAfterTrailers state.")
        }
        Ok(())
    }

    fn add_headers(&mut self, mut headers: Vec<Header>, fin: bool) -> Res<()> {
        qtrace!([self], "Add new headers fin={}", fin);
        let interim = match self.message_type {
            MessageType::Request => false,
            MessageType::Response => is_interim(&headers)?,
        };
        headers_valid(&headers, self.message_type)?;
        if self.message_type == MessageType::Response {
            headers.retain(Header::is_allowed_for_response);
        }

        if fin && interim {
            return Err(Error::HttpGeneralProtocolStream);
        }

        let is_web_transport = self.message_type == MessageType::Request
            && headers
                .iter()
                .any(|h| h.name() == ":method" && h.value() == "CONNECT")
            && headers
                .iter()
                .any(|h| h.name() == ":protocol" && h.value() == "webtransport");
        if is_web_transport {
            self.conn_events
                .extended_connect_new_session(self.stream_id, headers);
        } else {
            self.conn_events
                .header_ready(self.get_stream_info(), headers, interim, fin);
        }

        if fin {
            self.set_closed();
        } else {
            self.state = if is_web_transport {
                self.stream_type = Http3StreamType::ExtendedConnect;
                RecvMessageState::ExtendedConnect
            } else if interim {
                RecvMessageState::WaitingForResponseHeaders {
                    frame_reader: FrameReader::new(),
                }
            } else {
                RecvMessageState::WaitingForData {
                    frame_reader: FrameReader::new(),
                }
            };
        }
        Ok(())
    }

    fn set_state_to_close_pending(&mut self, post_readable_event: bool) -> Res<()> {
        // Stream has received fin. Depending on headers state set header_ready
        // or data_readable event so that app can pick up the fin.
        qtrace!([self], "set_state_to_close_pending: state={:?}", self.state);

        match self.state {
            RecvMessageState::WaitingForResponseHeaders { .. } => {
                return Err(Error::HttpGeneralProtocolStream);
            }
            RecvMessageState::ReadingData { .. } => {}
            RecvMessageState::WaitingForData { .. }
            | RecvMessageState::WaitingForFinAfterTrailers { .. } => {
                if post_readable_event {
                    self.conn_events.data_readable(self.get_stream_info());
                }
            }
            _ => unreachable!("Closing an already closed transaction."),
        }
        if !matches!(self.state, RecvMessageState::Closed) {
            self.state = RecvMessageState::ClosePending;
        }
        Ok(())
    }

    fn handle_push_promise(&mut self, push_id: u64, header_block: Vec<u8>) -> Res<()> {
        if self.push_handler.is_none() {
            return Err(Error::HttpFrameUnexpected);
        }

        if !self.blocked_push_promise.is_empty() {
            self.blocked_push_promise.push_back(PushInfo {
                push_id,
                header_block,
            });
        } else if let Some(headers) = self
            .qpack_decoder
            .borrow_mut()
            .decode_header_block(&header_block, self.stream_id)?
        {
            self.push_handler
                .as_ref()
                .ok_or(Error::HttpFrameUnexpected)?
                .borrow_mut()
                .new_push_promise(push_id, self.stream_id, headers)?;
        } else {
            self.blocked_push_promise.push_back(PushInfo {
                push_id,
                header_block,
            });
        }
        Ok(())
    }

    fn receive_internal(&mut self, conn: &mut Connection, post_readable_event: bool) -> Res<()> {
        let label = ::neqo_common::log_subject!(::log::Level::Debug, self);
        loop {
            qdebug!([label], "state={:?}.", self.state);
            match &mut self.state {
                // In the following 3 states we need to read frames.
                RecvMessageState::WaitingForResponseHeaders { frame_reader }
                | RecvMessageState::WaitingForData { frame_reader }
                | RecvMessageState::WaitingForFinAfterTrailers { frame_reader } => {
                    match frame_reader.receive(&mut StreamReaderConnectionWrapper::new(
                        conn,
                        self.stream_id,
                    ))? {
                        (None, true) => {
                            break self.set_state_to_close_pending(post_readable_event);
                        }
                        (None, false) => break Ok(()),
                        (Some(frame), fin) => {
                            qinfo!(
                                [self],
                                "A new frame has been received: {:?}; state={:?} fin={}",
                                frame,
                                self.state,
                                fin,
                            );
                            match frame {
                                HFrame::Headers { header_block } => {
                                    self.handle_headers_frame(header_block, fin)?;
                                }
                                HFrame::Data { len } => self.handle_data_frame(len, fin)?,
                                HFrame::PushPromise {
                                    push_id,
                                    header_block,
                                } => self.handle_push_promise(push_id, header_block)?,
                                _ => break Err(Error::HttpFrameUnexpected),
                            }
                            if matches!(self.state, RecvMessageState::Closed) {
                                break Ok(());
                            }
                            if fin
                                && !matches!(self.state, RecvMessageState::DecodingHeaders { .. })
                            {
                                break self.set_state_to_close_pending(post_readable_event);
                            }
                        }
                    };
                }
                RecvMessageState::DecodingHeaders {
                    ref header_block,
                    fin,
                } => {
                    if self
                        .qpack_decoder
                        .borrow()
                        .refers_dynamic_table(header_block)?
                        && !self.blocked_push_promise.is_empty()
                    {
                        qinfo!(
                            [self],
                            "decoding header is blocked waiting for a push_promise header block."
                        );
                        break Ok(());
                    }
                    let done = *fin;
                    let d_headers = self
                        .qpack_decoder
                        .borrow_mut()
                        .decode_header_block(header_block, self.stream_id)?;
                    if let Some(headers) = d_headers {
                        self.add_headers(headers, done)?;
                        if matches!(
                            self.state,
                            RecvMessageState::Closed | RecvMessageState::ExtendedConnect
                        ) {
                            break Ok(());
                        }
                    } else {
                        qinfo!([self], "decoding header is blocked.");
                        break Ok(());
                    }
                }
                RecvMessageState::ReadingData { .. } => {
                    if post_readable_event {
                        self.conn_events.data_readable(self.get_stream_info());
                    }
                    break Ok(());
                }
                RecvMessageState::ClosePending | RecvMessageState::Closed => {
                    panic!("Stream readable after being closed!");
                }
                RecvMessageState::ExtendedConnect => {
                    // Ignore read event, this request is waiting to be picked up by a new
                    // WebTransportSession
                    break Ok(());
                }
            };
        }
    }

    fn set_closed(&mut self) {
        if !self.blocked_push_promise.is_empty() {
            self.qpack_decoder
                .borrow_mut()
                .cancel_stream(self.stream_id);
        }
        self.state = RecvMessageState::Closed;
        self.conn_events
            .recv_closed(self.get_stream_info(), CloseType::Done);
    }

    fn closing(&self) -> bool {
        matches!(
            self.state,
            RecvMessageState::ClosePending | RecvMessageState::Closed
        )
    }

    fn get_stream_info(&self) -> Http3StreamInfo {
        Http3StreamInfo::new(self.stream_id, Http3StreamType::Http)
    }
}

impl Stream for RecvMessage {
    fn stream_type(&self) -> Http3StreamType {
        self.stream_type
    }
}

impl RecvStream for RecvMessage {
    fn receive(&mut self, conn: &mut Connection) -> Res<(ReceiveOutput, bool)> {
        self.receive_internal(conn, true)?;
        Ok((
            ReceiveOutput::NoOutput,
            matches!(self.state, RecvMessageState::Closed),
        ))
    }

    fn reset(&mut self, close_type: CloseType) -> Res<()> {
        if !self.closing() || !self.blocked_push_promise.is_empty() {
            self.qpack_decoder
                .borrow_mut()
                .cancel_stream(self.stream_id);
        }
        self.conn_events
            .recv_closed(self.get_stream_info(), close_type);
        self.state = RecvMessageState::Closed;
        Ok(())
    }

    fn read_data(&mut self, conn: &mut Connection, buf: &mut [u8]) -> Res<(usize, bool)> {
        let mut written = 0;
        loop {
            match self.state {
                RecvMessageState::ReadingData {
                    ref mut remaining_data_len,
                } => {
                    let to_read = min(*remaining_data_len, buf.len() - written);
                    let (amount, fin) = conn
                        .stream_recv(self.stream_id, &mut buf[written..written + to_read])
                        .map_err(|e| Error::map_stream_recv_errors(&Error::from(e)))?;
                    qlog::h3_data_moved_up(conn.qlog_mut(), self.stream_id, amount);

                    debug_assert!(amount <= to_read);
                    *remaining_data_len -= amount;
                    written += amount;

                    if fin {
                        if *remaining_data_len > 0 {
                            return Err(Error::HttpFrame);
                        }
                        self.set_closed();
                        break Ok((written, fin));
                    } else if *remaining_data_len == 0 {
                        self.state = RecvMessageState::WaitingForData {
                            frame_reader: FrameReader::new(),
                        };
                        self.receive_internal(conn, false)?;
                    } else {
                        break Ok((written, false));
                    }
                }
                RecvMessageState::ClosePending => {
                    self.set_closed();
                    break Ok((written, true));
                }
                _ => break Ok((written, false)),
            }
        }
    }

    fn http_stream(&mut self) -> Option<&mut dyn HttpRecvStream> {
        Some(self)
    }
}

impl HttpRecvStream for RecvMessage {
    fn header_unblocked(&mut self, conn: &mut Connection) -> Res<(ReceiveOutput, bool)> {
        while let Some(p) = self.blocked_push_promise.front() {
            if let Some(headers) = self
                .qpack_decoder
                .borrow_mut()
                .decode_header_block(&p.header_block, self.stream_id)?
            {
                self.push_handler
                    .as_ref()
                    .ok_or(Error::HttpFrameUnexpected)?
                    .borrow_mut()
                    .new_push_promise(p.push_id, self.stream_id, headers)?;
                self.blocked_push_promise.pop_front();
            } else {
                return Ok((ReceiveOutput::NoOutput, false));
            }
        }

        self.receive(conn)
    }

    fn maybe_update_priority(&mut self, priority: Priority) -> bool {
        self.priority_handler.maybe_update_priority(priority)
    }

    fn priority_update_frame(&mut self) -> Option<HFrame> {
        self.priority_handler.maybe_encode_frame(self.stream_id)
    }

    fn priority_update_sent(&mut self) {
        self.priority_handler.priority_update_sent();
    }

    fn set_new_listener(&mut self, conn_events: Box<dyn HttpRecvStreamEvents>) {
        self.state = RecvMessageState::WaitingForData {
            frame_reader: FrameReader::new(),
        };
        self.conn_events = conn_events;
    }

    fn extended_connect_wait_for_response(&self) -> bool {
        matches!(self.state, RecvMessageState::ExtendedConnect)
    }
}
