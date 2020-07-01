// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::hframe::{HFrame, HFrameReader};
use crate::push_controller::PushController;
use crate::qlog;
use crate::{Error, Header, Res};

use neqo_common::{matches, qdebug, qinfo, qtrace};
use neqo_qpack::decoder::QPackDecoder;
use neqo_transport::Connection;
use std::cell::RefCell;
use std::cmp::min;
use std::convert::TryFrom;
use std::fmt::Debug;
use std::rc::Rc;

pub(crate) trait RecvMessageEvents: Debug {
    fn header_ready(&self, stream_id: u64, headers: Option<Vec<Header>>, fin: bool);
    fn data_readable(&self, stream_id: u64);
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
 */
#[derive(PartialEq, Debug)]
enum RecvMessageState {
    WaitingForResponseHeaders,
    DecodingHeaders { header_block: Vec<u8>, fin: bool },
    WaitingForData,
    ReadingData { remaining_data_len: usize },
    WaitingForFinAfterTrailers,
    ClosePending, // Close must first be read by application
    Closed,
}

#[derive(Debug)]
pub(crate) struct RecvMessage {
    state: RecvMessageState,
    frame_reader: HFrameReader,
    conn_events: Box<dyn RecvMessageEvents>,
    push_handler: Option<Rc<RefCell<PushController>>>,
    stream_id: u64,
}

impl ::std::fmt::Display for RecvMessage {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "RecvMessage stream_id:{}", self.stream_id)
    }
}

impl RecvMessage {
    pub fn new(
        stream_id: u64,
        conn_events: Box<dyn RecvMessageEvents>,
        push_handler: Option<Rc<RefCell<PushController>>>,
    ) -> Self {
        Self {
            state: RecvMessageState::WaitingForResponseHeaders,
            frame_reader: HFrameReader::new(),
            conn_events,
            push_handler,
            stream_id,
        }
    }

    fn handle_headers_frame(&mut self, header_block: Vec<u8>, fin: bool) -> Res<()> {
        match self.state {
            RecvMessageState::WaitingForResponseHeaders => {
                if header_block.is_empty() {
                    self.add_headers(None, fin);
                } else {
                    self.state = RecvMessageState::DecodingHeaders { header_block, fin };
                }
             }
            RecvMessageState::WaitingForData => {
                // TODO implement trailers, for now just ignore them.
                self.state = RecvMessageState::WaitingForFinAfterTrailers;
            }
            RecvMessageState::WaitingForFinAfterTrailers => {
                return Err(Error::HttpFrameUnexpected);
            }
            _ => unreachable!("This functions is only called in WaitingForResponseHeaders | WaitingForData | WaitingForFinAfterTrailers state.")
         }
        Ok(())
    }

    fn handle_data_frame(&mut self, len: u64, fin: bool) -> Res<()> {
        match self.state {
            RecvMessageState::WaitingForResponseHeaders | RecvMessageState::WaitingForFinAfterTrailers => {
                return Err(Error::HttpFrameUnexpected);
            }
            RecvMessageState::WaitingForData => {
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

    fn add_headers(&mut self, headers: Option<Vec<Header>>, fin: bool) {
        if fin {
            self.conn_events.header_ready(self.stream_id, headers, true);
            self.state = RecvMessageState::Closed;
        } else {
            self.conn_events
                .header_ready(self.stream_id, headers, false);
            self.state = RecvMessageState::WaitingForData;
        }
    }

    fn set_state_to_close_pending(&mut self) {
        // Stream has received fin. Depending on headers state set header_ready
        // or data_readable event so that app can pick up the fin.
        qtrace!(
            [self],
            "set_state_to_close_pending:  state={:?}",
            self.state
        );

        match self.state {
            RecvMessageState::WaitingForResponseHeaders => {
                self.conn_events.header_ready(self.stream_id, None, true);
                self.state = RecvMessageState::Closed;
            }
            RecvMessageState::ReadingData { .. } => {}
            RecvMessageState::WaitingForData | RecvMessageState::WaitingForFinAfterTrailers => {
                self.conn_events.data_readable(self.stream_id)
            }
            _ => unreachable!("Closing an already closed transaction."),
        }
        if !matches!(self.state, RecvMessageState::Closed) {
            self.state = RecvMessageState::ClosePending;
        }
    }

    fn recv_frame(&mut self, conn: &mut Connection) -> Res<(Option<HFrame>, bool)> {
        qtrace!([self], "receiving frame header");
        let fin = self.frame_reader.receive(conn, self.stream_id)?;
        if self.frame_reader.done() {
            qdebug!([self], "A new frame has been received.");
            Ok((Some(self.frame_reader.get_frame()?), fin))
        } else {
            Ok((None, fin))
        }
    }

    pub fn read_data(
        &mut self,
        conn: &mut Connection,
        decoder: &mut QPackDecoder,
        buf: &mut [u8],
    ) -> Res<(usize, bool)> {
        let mut written = 0;
        loop {
            match self.state {
                RecvMessageState::ReadingData {
                    ref mut remaining_data_len,
                } => {
                    let to_read = min(*remaining_data_len, buf.len() - written);
                    let (amount, fin) =
                        conn.stream_recv(self.stream_id, &mut buf[written..written + to_read])?;
                    qlog::h3_data_moved_up(
                        &mut conn.qlog_mut().borrow_mut(),
                        self.stream_id,
                        amount,
                    );

                    debug_assert!(amount <= to_read);
                    *remaining_data_len -= amount;
                    written += amount;

                    if fin {
                        if *remaining_data_len > 0 {
                            return Err(Error::HttpFrame);
                        }
                        self.state = RecvMessageState::Closed;
                        break Ok((written, fin));
                    } else if *remaining_data_len == 0 {
                        self.state = RecvMessageState::WaitingForData;
                        self.receive_internal(conn, decoder, false)?;
                    } else {
                        break Ok((written, false));
                    }
                }
                RecvMessageState::ClosePending => {
                    self.state = RecvMessageState::Closed;
                    break Ok((written, true));
                }
                _ => break Ok((written, false)),
            }
        }
    }

    fn receive_internal(
        &mut self,
        conn: &mut Connection,
        decoder: &mut QPackDecoder,
        post_readable_event: bool,
    ) -> Res<()> {
        let label = ::neqo_common::log_subject!(::log::Level::Debug, self);
        loop {
            qdebug!([label], "state={:?}.", self.state);
            match self.state {
                // In the following 3 states we need to read frames.
                RecvMessageState::WaitingForResponseHeaders
                | RecvMessageState::WaitingForData
                | RecvMessageState::WaitingForFinAfterTrailers => {
                    match self.recv_frame(conn)? {
                        (None, true) => {
                            self.set_state_to_close_pending();
                            break Ok(());
                        }
                        (None, false) => break Ok(()),
                        (Some(frame), fin) => {
                            qinfo!(
                                [self],
                                "A new frame has been received: {:?}; state={:?}",
                                frame,
                                self.state
                            );
                            match frame {
                                HFrame::Headers { header_block } => {
                                    self.handle_headers_frame(header_block, fin)?
                                }
                                HFrame::Data { len } => self.handle_data_frame(len, fin)?,
                                HFrame::PushPromise {
                                    push_id,
                                    header_block,
                                } => self
                                    .push_handler
                                    .as_ref()
                                    .ok_or(Error::HttpId)?
                                    .borrow()
                                    .new_push_promise(push_id, header_block)?,
                                _ => break Err(Error::HttpFrameUnexpected),
                            }
                            if matches!(self.state, RecvMessageState::Closed) {
                                break Ok(());
                            }
                            if fin && !matches!(self.state, RecvMessageState::DecodingHeaders{..}) {
                                self.set_state_to_close_pending();
                                break Ok(());
                            }
                        }
                    };
                }
                RecvMessageState::DecodingHeaders {
                    ref header_block,
                    fin,
                } => {
                    if let Some(headers) =
                        decoder.decode_header_block(header_block, self.stream_id)?
                    {
                        self.add_headers(Some(headers), fin);
                        if fin {
                            break Ok(());
                        }
                    } else {
                        qinfo!([self], "decoding header is blocked.");
                        break Ok(());
                    }
                }
                RecvMessageState::ReadingData { .. } => {
                    if post_readable_event {
                        self.conn_events.data_readable(self.stream_id);
                    }
                    break Ok(());
                }
                RecvMessageState::ClosePending | RecvMessageState::Closed => {
                    panic!("Stream readable after being closed!");
                }
            };
        }
    }

    pub fn receive(&mut self, conn: &mut Connection, decoder: &mut QPackDecoder) -> Res<()> {
        self.receive_internal(conn, decoder, true)
    }

    pub fn done(&self) -> bool {
        self.state == RecvMessageState::Closed
    }
}
