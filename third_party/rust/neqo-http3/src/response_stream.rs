// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::client_events::Http3ClientEvents;
use crate::hframe::{HFrame, HFrameReader};
use crate::{Error, Header, Res};
use neqo_common::{matches, qdebug, qinfo, qtrace};
use neqo_qpack::decoder::QPackDecoder;
use neqo_transport::Connection;
use std::cmp::min;
use std::mem;

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
enum ResponseStreamState {
    WaitingForResponseHeaders,
    DecodingHeaders { header_block: Vec<u8>, fin: bool },
    WaitingForData,
    ReadingData { remaining_data_len: usize },
    WaitingForFinAfterTrailers,
    ClosePending, // Close must first be read by application
    Closed,
}

#[derive(Debug, PartialEq)]
enum ResponseHeadersState {
    NoHeaders,
    Ready(Option<Vec<Header>>),
    Read,
}

#[derive(Debug)]
pub(crate) struct ResponseStream {
    state: ResponseStreamState,
    frame_reader: HFrameReader,
    response_headers_state: ResponseHeadersState,
    conn_events: Http3ClientEvents,
    stream_id: u64,
}

impl ::std::fmt::Display for ResponseStream {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "ResponseStream stream_id:{}", self.stream_id)
    }
}

impl ResponseStream {
    pub fn new(stream_id: u64, conn_events: Http3ClientEvents) -> Self {
        Self {
            state: ResponseStreamState::WaitingForResponseHeaders,
            frame_reader: HFrameReader::new(),
            response_headers_state: ResponseHeadersState::NoHeaders,
            conn_events,
            stream_id,
        }
    }

    fn handle_headers_frame(&mut self, header_block: Vec<u8>, fin: bool) -> Res<()> {
        match self.state {
            ResponseStreamState::WaitingForResponseHeaders => {
                if header_block.is_empty() {
                    self.add_headers(None)?;
                } else {
                    self.state = ResponseStreamState::DecodingHeaders { header_block, fin };
                }
             }
            ResponseStreamState::WaitingForData => {
                // TODO implement trailers, for now just ignore them.
                self.state = ResponseStreamState::WaitingForFinAfterTrailers;
            }
            ResponseStreamState::WaitingForFinAfterTrailers => {
                return Err(Error::HttpFrameUnexpected);
            }
            _ => unreachable!("This functions is only called in WaitingForResponseHeaders | WaitingForData | WaitingForFinAfterTrailers state.")
         }
        Ok(())
    }

    fn handle_data_frame(&mut self, len: u64, fin: bool) -> Res<()> {
        match self.state {
            ResponseStreamState::WaitingForResponseHeaders | ResponseStreamState::WaitingForFinAfterTrailers => {
                return Err(Error::HttpFrameUnexpected);
            }
            ResponseStreamState::WaitingForData => {
                if len > 0 {
                    if fin {
                        return Err(Error::HttpFrameError);
                    }
                    self.state = ResponseStreamState::ReadingData {
                        remaining_data_len: len as usize,
                    };
                }
            }
            _ => unreachable!("This functions is only called in WaitingForResponseHeaders | WaitingForData | WaitingForFinAfterTrailers state.")
        }
        Ok(())
    }

    fn add_headers(&mut self, headers: Option<Vec<Header>>) -> Res<()> {
        if self.response_headers_state != ResponseHeadersState::NoHeaders {
            debug_assert!(
                false,
                "self.response_headers_state must be in state ResponseHeadersState::NoHeaders."
            );
            return Err(Error::HttpInternalError);
        }
        self.response_headers_state = ResponseHeadersState::Ready(headers);
        self.conn_events.header_ready(self.stream_id);
        self.state = ResponseStreamState::WaitingForData;
        Ok(())
    }

    fn set_state_to_close_pending(&mut self) {
        // Stream has received fin. Depending on headers state set header_ready
        // or data_readable event so that app can pick up the fin.
        qtrace!(
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
        self.state = ResponseStreamState::ClosePending;
    }

    fn recv_frame(&mut self, conn: &mut Connection) -> Res<(Option<HFrame>, bool)> {
        qtrace!([self], "receiving frame header");
        let fin = self.frame_reader.receive(conn, self.stream_id)?;
        if !self.frame_reader.done() {
            Ok((None, fin))
        } else {
            qdebug!([self], "A new frame has been received.");
            Ok((Some(self.frame_reader.get_frame()?), fin))
        }
    }

    pub fn read_response_headers(&mut self) -> Res<(Vec<Header>, bool)> {
        if let ResponseHeadersState::Ready(ref mut headers) = self.response_headers_state {
            let hdrs = if let Some(ref mut hdrs) = headers {
                mem::replace(hdrs, Vec::new())
            } else {
                Vec::new()
            };

            self.response_headers_state = ResponseHeadersState::Read;

            let fin = if self.state == ResponseStreamState::ClosePending {
                self.state = ResponseStreamState::Closed;
                true
            } else {
                false
            };
            Ok((hdrs, fin))
        } else {
            Err(Error::Unavailable)
        }
    }

    pub fn read_response_data(
        &mut self,
        conn: &mut Connection,
        buf: &mut [u8],
    ) -> Res<(usize, bool)> {
        match self.state {
            ResponseStreamState::ReadingData {
                ref mut remaining_data_len,
            } => {
                let to_read = min(*remaining_data_len, buf.len());
                let (amount, fin) = conn.stream_recv(self.stream_id, &mut buf[..to_read])?;
                debug_assert!(amount <= to_read);
                *remaining_data_len -= amount;

                if fin {
                    if *remaining_data_len > 0 {
                        return Err(Error::HttpFrameError);
                    }
                    self.state = ResponseStreamState::Closed;
                } else if *remaining_data_len == 0 {
                    self.state = ResponseStreamState::WaitingForData;
                }

                Ok((amount, fin))
            }
            ResponseStreamState::ClosePending => {
                self.state = ResponseStreamState::Closed;
                Ok((0, true))
            }
            _ => Ok((0, false)),
        }
    }

    pub fn receive(&mut self, conn: &mut Connection, decoder: &mut QPackDecoder) -> Res<()> {
        let label = if ::log::log_enabled!(::log::Level::Debug) {
            format!("{}", self)
        } else {
            String::new()
        };
        loop {
            qdebug!([label], "state={:?}.", self.state);
            match self.state {
                // In the following 3 states we need to read frames.
                ResponseStreamState::WaitingForResponseHeaders
                | ResponseStreamState::WaitingForData
                | ResponseStreamState::WaitingForFinAfterTrailers => {
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
                                HFrame::PushPromise { .. } | HFrame::DuplicatePush { .. } => {
                                    break Err(Error::HttpIdError)
                                }
                                _ => break Err(Error::HttpFrameUnexpected),
                            }
                            if fin
                                && !matches!(self.state, ResponseStreamState::DecodingHeaders{..})
                            {
                                self.set_state_to_close_pending();
                                break Ok(());
                            }
                        }
                    };
                }
                ResponseStreamState::DecodingHeaders {
                    ref header_block,
                    fin,
                } => match decoder.decode_header_block(header_block, self.stream_id)? {
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
                },
                ResponseStreamState::ReadingData { .. } => {
                    self.conn_events.data_readable(self.stream_id);
                    break Ok(());
                }
                ResponseStreamState::ClosePending => {
                    panic!("Stream readable after being closed!");
                }
                ResponseStreamState::Closed => {
                    panic!("Stream readable after being closed!");
                }
            };
        }
    }

    pub fn is_closed(&self) -> bool {
        self.state == ResponseStreamState::Closed
    }

    pub fn close(&mut self) {
        self.state = ResponseStreamState::Closed;
    }
}
