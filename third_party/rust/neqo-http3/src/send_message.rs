// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::hframe::HFrame;
use crate::qlog;
use crate::Header;
use crate::{Error, Res};

use neqo_common::{qdebug, qinfo, qtrace, Encoder};
use neqo_qpack::encoder::QPackEncoder;
use neqo_transport::{AppError, Connection};
use std::cmp::min;
use std::convert::TryFrom;
use std::fmt::Debug;

const MAX_DATA_HEADER_SIZE_2: usize = (1 << 6) - 1; // Maximal amount of data with DATA frame header size 2
const MAX_DATA_HEADER_SIZE_2_LIMIT: usize = MAX_DATA_HEADER_SIZE_2 + 3; // 63 + 3 (size of the next buffer data frame header)
const MAX_DATA_HEADER_SIZE_3: usize = (1 << 14) - 1; // Maximal amount of data with DATA frame header size 3
const MAX_DATA_HEADER_SIZE_3_LIMIT: usize = MAX_DATA_HEADER_SIZE_3 + 5; // 16383 + 5 (size of the next buffer data frame header)
const MAX_DATA_HEADER_SIZE_5: usize = (1 << 30) - 1; // Maximal amount of data with DATA frame header size 3
const MAX_DATA_HEADER_SIZE_5_LIMIT: usize = MAX_DATA_HEADER_SIZE_5 + 9; // 1073741823 + 9 (size of the next buffer data frame header)

pub(crate) trait SendMessageEvents: Debug {
    fn data_writable(&self, stream_id: u64);
    fn remove_send_side_event(&self, stream_id: u64);
    fn stop_sending(&self, stream_id: u64, app_err: AppError);
}

/*
 *  SendMessage states:
 *    Uninitialized
 *    Initialized : Headers are present but still not encoded. A message body may be present as well.
 *                  The client side sends a message body using the send_body() function that directly
 *                  writes into a transport stream. The server side sets headers and body when
 *                  initializing a send message (TODO: make server use send_body as well)
 *    SendingInitialMessage : sending headers and maybe message body. From here we may switch to
 *                     SendingData or Closed (if the app does not want to send data and
 *                     has already closed the send stream).
 *    SendingData : We are sending request data until the app closes the stream.
 *    Closed
 */

#[derive(PartialEq, Debug)]
enum SendMessageState {
    Uninitialized,
    Initialized {
        headers: Vec<Header>,
        data: Option<Vec<u8>>,
        fin: bool,
    },
    SendingInitialMessage {
        buf: Vec<u8>,
        fin: bool,
    },
    SendingData,
    Closed,
}

impl SendMessageState {
    pub fn is_sending_closed(&self) -> bool {
        match self {
            Self::Initialized { fin, .. } | Self::SendingInitialMessage { fin, .. } => *fin,
            Self::SendingData => false,
            _ => true,
        }
    }

    pub fn done(&self) -> bool {
        matches!(self, Self::Closed)
    }

    pub fn is_state_sending_data(&self) -> bool {
        matches!(self, Self::SendingData)
    }
}

#[derive(Debug)]
pub(crate) struct SendMessage {
    state: SendMessageState,
    stream_id: u64,
    conn_events: Box<dyn SendMessageEvents>,
}

impl SendMessage {
    pub fn new(stream_id: u64, conn_events: Box<dyn SendMessageEvents>) -> Self {
        qinfo!("Create a request stream_id={}", stream_id);
        Self {
            state: SendMessageState::Uninitialized,
            stream_id,
            conn_events,
        }
    }

    pub fn new_with_headers(
        stream_id: u64,
        headers: Vec<Header>,
        conn_events: Box<dyn SendMessageEvents>,
    ) -> Self {
        qinfo!("Create a request stream_id={}", stream_id);
        Self {
            state: SendMessageState::Initialized {
                headers,
                data: None,
                fin: false,
            },
            stream_id,
            conn_events,
        }
    }

    pub fn set_message(&mut self, headers: &[Header], data: Option<&[u8]>) -> Res<()> {
        if !matches!(self.state, SendMessageState::Uninitialized) {
            return Err(Error::AlreadyInitialized);
        }

        self.state = SendMessageState::Initialized {
            headers: headers.to_vec(),
            data: if let Some(d) = data {
                Some(d.to_vec())
            } else {
                None
            },
            fin: true,
        };
        Ok(())
    }

    pub fn send_body(&mut self, conn: &mut Connection, buf: &[u8]) -> Res<usize> {
        qtrace!(
            [self],
            "send_body: state={:?} len={}",
            self.state,
            buf.len()
        );
        match self.state {
            SendMessageState::Uninitialized
            | SendMessageState::Initialized { .. }
            | SendMessageState::SendingInitialMessage { .. } => Ok(0),
            SendMessageState::SendingData => {
                let available = usize::try_from(
                    conn.stream_avail_send_space(self.stream_id)
                        .map_err(|e| Error::map_stream_send_errors(&e))?,
                )
                .unwrap_or(usize::max_value());
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
                let sent_fh = conn
                    .stream_send(self.stream_id, &enc)
                    .map_err(|e| Error::map_stream_send_errors(&e))?;
                debug_assert_eq!(sent_fh, enc.len());

                let sent = conn
                    .stream_send(self.stream_id, &buf[..to_send])
                    .map_err(|e| Error::map_stream_send_errors(&e))?;
                qlog::h3_data_moved_down(&mut conn.qlog_mut(), self.stream_id, to_send);
                Ok(sent)
            }
            SendMessageState::Closed => Err(Error::AlreadyClosed),
        }
    }

    pub fn done(&self) -> bool {
        self.state.done()
    }

    pub fn stream_writable(&self) {
        if self.state.is_state_sending_data() {
            self.conn_events.data_writable(self.stream_id);
        }
    }

    /// # Errors
    /// `ClosedCriticalStream` if the encoder stream is closed.
    /// `InternalError` if an unexpected error occurred.
    fn ensure_encoded(&mut self, conn: &mut Connection, encoder: &mut QPackEncoder) -> Res<()> {
        if let SendMessageState::Initialized { headers, data, fin } = &self.state {
            qdebug!([self], "Encoding headers");
            let header_block = encoder.encode_header_block(conn, &headers, self.stream_id)?;
            let hframe = HFrame::Headers {
                header_block: header_block.to_vec(),
            };
            let mut d = Encoder::default();
            hframe.encode(&mut d);
            if let Some(buf) = data {
                qdebug!([self], "Encoding data");
                let d_frame = HFrame::Data {
                    len: buf.len() as u64,
                };
                d_frame.encode(&mut d);
                d.encode(&buf);
            }

            self.state = SendMessageState::SendingInitialMessage {
                buf: d.into(),
                fin: *fin,
            };
        }
        Ok(())
    }

    /// # Errors
    /// `ClosedCriticalStream` if the encoder stream is closed.
    /// `InternalError` if an unexpected error occurred.
    /// `InvalidStreamId` if the stream does not exist,
    /// `AlreadyClosed` if the stream has already been closed.
    /// `TransportStreamDoesNotExist` if the transport stream does not exist (this may happen if `process_output`
    /// has not been called when needed, and HTTP3 layer has not picked up the info that the stream has been closed.)
    pub fn send(&mut self, conn: &mut Connection, encoder: &mut QPackEncoder) -> Res<()> {
        self.ensure_encoded(conn, encoder)?;

        let label = if ::log::log_enabled!(::log::Level::Debug) {
            format!("{}", self)
        } else {
            String::new()
        };

        if let SendMessageState::SendingInitialMessage { ref mut buf, fin } = self.state {
            let sent = conn
                .stream_send(self.stream_id, &buf)
                .map_err(|_| Error::map_send_errors())?;
            qlog::h3_data_moved_down(&mut conn.qlog_mut(), self.stream_id, sent);

            qtrace!([label], "{} bytes sent", sent);

            if sent == buf.len() {
                if fin {
                    conn.stream_close_send(self.stream_id)
                        .map_err(|_| Error::map_send_errors())?;
                    self.state = SendMessageState::Closed;
                    qtrace!([label], "done sending request");
                } else {
                    self.state = SendMessageState::SendingData;
                    self.conn_events.data_writable(self.stream_id);
                    qtrace!([label], "change to state SendingData");
                }
            } else {
                let b = buf.split_off(sent);
                *buf = b;
            }
        }
        Ok(())
    }

    // SendMessage owns headers and sends them. It may also own data for the server side.
    // This method returns if they're still being sent. Request body (if any) is sent by
    // http client afterwards using `send_request_body` after receiving DataWritable event.
    pub fn has_data_to_send(&self) -> bool {
        matches!(self.state, SendMessageState::Initialized {..} | SendMessageState::SendingInitialMessage { .. } )
    }

    pub fn close(&mut self, conn: &mut Connection) -> Res<()> {
        match self.state {
            SendMessageState::SendingInitialMessage { ref mut fin, .. }
            | SendMessageState::Initialized { ref mut fin, .. } => {
                *fin = true;
            }
            _ => {
                self.state = SendMessageState::Closed;
                conn.stream_close_send(self.stream_id)?;
            }
        }

        self.conn_events.remove_send_side_event(self.stream_id);
        Ok(())
    }

    pub fn stop_sending(&mut self, app_err: AppError) {
        if !self.state.is_sending_closed() {
            self.conn_events.remove_send_side_event(self.stream_id);
            self.conn_events.stop_sending(self.stream_id, app_err);
        }
    }
}

impl ::std::fmt::Display for SendMessage {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "SendMesage {}", self.stream_id)
    }
}
