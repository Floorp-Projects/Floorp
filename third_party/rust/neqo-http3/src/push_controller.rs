// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::client_events::{Http3ClientEvent, Http3ClientEvents};
use crate::connection::Http3Connection;
use crate::frames::HFrame;
use crate::{CloseType, Error, Http3StreamInfo, HttpRecvStreamEvents, RecvStreamEvents, Res};
use neqo_common::{qerror, qinfo, qtrace, Header};
use neqo_transport::{Connection, StreamId};
use std::cell::RefCell;
use std::collections::VecDeque;
use std::convert::TryFrom;
use std::fmt::Debug;
use std::fmt::Display;
use std::mem;
use std::rc::Rc;
use std::slice::SliceIndex;

/// `PushStates`:
///   `Init`: there is no push stream nor a push promise. This state is only used to keep track of opened and closed
///           push streams.
///   `PushPromise`: the push has only ever receive a pushpromise frame
///   `OnlyPushStream`: there is only a push stream. All push stream events, i.e. `PushHeaderReady` and
///                     `PushDataReadable` will be delayed until a push promise is received (they are kept in
///                     `events`).
///   `Active`: there is a push steam and at least one push promise frame.
///   `Close`: the push stream has been closed or reset already.
#[derive(Debug, PartialEq, Clone)]
enum PushState {
    Init,
    PushPromise {
        headers: Vec<Header>,
    },
    OnlyPushStream {
        stream_id: StreamId,
        events: Vec<Http3ClientEvent>,
    },
    Active {
        stream_id: StreamId,
        headers: Vec<Header>,
    },
    Closed,
}

/// `ActivePushStreams` holds information about push streams.
///
/// `first_push_id` holds a `push_id` of the first element in `push_streams` if it is present.
/// `push_id` smaller than `first_push_id` have been already closed. `push_id` => `first_push_id`
/// are in `push_streams` or they are not yet opened.
#[derive(Debug)]
struct ActivePushStreams {
    push_streams: VecDeque<PushState>,
    first_push_id: u64,
}

impl ActivePushStreams {
    pub fn new() -> Self {
        Self {
            push_streams: VecDeque::new(),
            first_push_id: 0,
        }
    }

    /// Returns None if a stream has been closed already.
    pub fn get_mut(
        &mut self,
        push_id: u64,
    ) -> Option<&mut <usize as SliceIndex<[PushState]>>::Output> {
        if push_id < self.first_push_id {
            return None;
        }

        let inx = usize::try_from(push_id - self.first_push_id).unwrap();
        if inx >= self.push_streams.len() {
            self.push_streams.resize(inx + 1, PushState::Init);
        }
        match self.push_streams.get_mut(inx) {
            Some(PushState::Closed) => None,
            e => e,
        }
    }

    /// Returns None if a stream has been closed already.
    pub fn get(&mut self, push_id: u64) -> Option<&mut PushState> {
        self.get_mut(push_id)
    }

    /// Returns the State of a closed push stream or None for already closed streams.
    pub fn close(&mut self, push_id: u64) -> Option<PushState> {
        match self.get_mut(push_id) {
            None | Some(PushState::Closed) => None,
            Some(s) => {
                let res = mem::replace(s, PushState::Closed);
                while self.push_streams.get(0).is_some()
                    && *self.push_streams.get(0).unwrap() == PushState::Closed
                {
                    self.push_streams.pop_front();
                    self.first_push_id += 1;
                }
                Some(res)
            }
        }
    }

    #[must_use]
    pub fn number_done(&self) -> u64 {
        self.first_push_id
            + u64::try_from(
                self.push_streams
                    .iter()
                    .filter(|&e| e == &PushState::Closed)
                    .count(),
            )
            .unwrap()
    }

    pub fn clear(&mut self) {
        self.first_push_id = 0;
        self.push_streams.clear();
    }
}

/// `PushController` keeps information about push stream states.
///
/// A `PushStream` calls `add_new_push_stream` that may change the push state from Init to `OnlyPushStream` or from
/// `PushPromise` to `Active`. If a stream has already been closed `add_new_push_stream` returns false (the `PushStream`
/// will close the transport stream).
/// A `PushStream` calls `push_stream_reset` if the transport stream has been canceled.
/// When a push stream is done it calls `close`.
///
/// The `PushController` handles:
///  `PUSH_PROMISE` frame: frames may change the push state from Init to `PushPromise` and from `OnlyPushStream` to
///                        `Active`. Frames for a closed steams are ignored.
///  `CANCEL_PUSH` frame: (`handle_cancel_push` will be called). If a push is in state `PushPromise` or `Active`, any
///                       posted events will be removed and a `PushCanceled` event will be posted. If a push is in
///                       state `OnlyPushStream` or `Active` the transport stream and the `PushStream` will be closed.
///                       The frame will be ignored for already closed pushes.
///  Application calling cancel: the actions are similar to the `CANCEL_PUSH` frame. The difference is that
///                              `PushCanceled` will not be posted and a `CANCEL_PUSH` frame may be sent.
#[derive(Debug)]
pub(crate) struct PushController {
    max_concurent_push: u64,
    current_max_push_id: u64,
    // push_streams holds the states of push streams.
    // We keep a stream until the stream has been closed.
    push_streams: ActivePushStreams,
    // The keeps the next consecutive push_id that should be open.
    // All push_id < next_push_id_to_open are in the push_stream lists. If they are not in the list they have
    // been already closed.
    conn_events: Http3ClientEvents,
}

impl PushController {
    pub fn new(max_concurent_push: u64, conn_events: Http3ClientEvents) -> Self {
        PushController {
            max_concurent_push,
            current_max_push_id: 0,
            push_streams: ActivePushStreams::new(),
            conn_events,
        }
    }
}

impl Display for PushController {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Push controler")
    }
}

impl PushController {
    /// A new `push_promise` has been received.
    /// # Errors
    /// `HttpId` if `push_id` greater than it is allowed has been received.
    pub fn new_push_promise(
        &mut self,
        push_id: u64,
        ref_stream_id: StreamId,
        new_headers: Vec<Header>,
    ) -> Res<()> {
        qtrace!(
            [self],
            "New push promise push_id={} headers={:?} max_push={}",
            push_id,
            new_headers,
            self.max_concurent_push
        );

        self.check_push_id(push_id)?;

        match self.push_streams.get_mut(push_id) {
            None => {
                qtrace!("Push has been closed already {}.", push_id);
                Ok(())
            }
            Some(push_state) => match push_state {
                PushState::Init => {
                    self.conn_events
                        .push_promise(push_id, ref_stream_id, new_headers.clone());
                    *push_state = PushState::PushPromise {
                        headers: new_headers,
                    };
                    Ok(())
                }
                PushState::PushPromise { headers } | PushState::Active { headers, .. } => {
                    if new_headers != *headers {
                        return Err(Error::HttpGeneralProtocol);
                    }
                    self.conn_events
                        .push_promise(push_id, ref_stream_id, new_headers);
                    Ok(())
                }
                PushState::OnlyPushStream { stream_id, events } => {
                    let stream_id_tmp = *stream_id;
                    self.conn_events
                        .push_promise(push_id, ref_stream_id, new_headers.clone());

                    for e in events.drain(..) {
                        self.conn_events.insert(e);
                    }
                    *push_state = PushState::Active {
                        stream_id: stream_id_tmp,
                        headers: new_headers,
                    };
                    Ok(())
                }
                PushState::Closed => unreachable!("This is only internal; it is transfer to None"),
            },
        }
    }

    pub fn add_new_push_stream(&mut self, push_id: u64, stream_id: StreamId) -> Res<bool> {
        qtrace!(
            "A new push stream with push_id={} stream_id={}",
            push_id,
            stream_id
        );

        self.check_push_id(push_id)?;

        match self.push_streams.get_mut(push_id) {
            None => {
                qinfo!("Push has been closed already.");
                Ok(false)
            }
            Some(push_state) => match push_state {
                PushState::Init => {
                    *push_state = PushState::OnlyPushStream {
                        stream_id,
                        events: Vec::new(),
                    };
                    Ok(true)
                }
                PushState::PushPromise { headers } => {
                    let tmp = mem::take(headers);
                    *push_state = PushState::Active {
                        stream_id,
                        headers: tmp,
                    };
                    Ok(true)
                }
                // The following state have already have a push stream:
                // PushState::OnlyPushStream | PushState::Active
                _ => {
                    qerror!("Duplicate push stream.");
                    Err(Error::HttpId)
                }
            },
        }
    }

    fn check_push_id(&mut self, push_id: u64) -> Res<()> {
        // Check if push id is greater than what we allow.
        if push_id > self.current_max_push_id {
            qerror!("Push id is greater than current_max_push_id.");
            Err(Error::HttpId)
        } else {
            Ok(())
        }
    }

    pub fn handle_cancel_push(
        &mut self,
        push_id: u64,
        conn: &mut Connection,
        base_handler: &mut Http3Connection,
    ) -> Res<()> {
        qtrace!("CANCEL_PUSH frame has been received, push_id={}", push_id);

        self.check_push_id(push_id)?;

        match self.push_streams.close(push_id) {
            None => {
                qtrace!("Push has already been closed (push_id={}).", push_id);
                Ok(())
            }
            Some(ps) => match ps {
                PushState::Init => Ok(()),
                PushState::PushPromise { .. } => {
                    self.conn_events.remove_events_for_push_id(push_id);
                    self.conn_events.push_canceled(push_id);
                    Ok(())
                }
                PushState::OnlyPushStream { stream_id, .. }
                | PushState::Active { stream_id, .. } => {
                    mem::drop(base_handler.stream_stop_sending(
                        conn,
                        stream_id,
                        Error::HttpRequestCancelled.code(),
                    ));
                    self.conn_events.remove_events_for_push_id(push_id);
                    self.conn_events.push_canceled(push_id);
                    Ok(())
                }
                PushState::Closed => unreachable!("This is only internal; it is transfer to None"),
            },
        }
    }

    pub fn close(&mut self, push_id: u64) {
        qtrace!("Push stream has been closed.");
        if let Some(push_state) = self.push_streams.close(push_id) {
            debug_assert!(matches!(push_state, PushState::Active { .. }));
        } else {
            debug_assert!(false, "Closing non existing push stream!");
        }
    }

    pub fn cancel(
        &mut self,
        push_id: u64,
        conn: &mut Connection,
        base_handler: &mut Http3Connection,
    ) -> Res<()> {
        qtrace!("Cancel push_id={}", push_id);

        self.check_push_id(push_id)?;

        match self.push_streams.get(push_id) {
            None => {
                qtrace!("Push has already been closed.");
                // If we have some events for the push_id in the event queue, the caller still does not
                // not know that the push has been closed. Otherwise return InvalidStreamId.
                if self.conn_events.has_push(push_id) {
                    self.conn_events.remove_events_for_push_id(push_id);
                    Ok(())
                } else {
                    Err(Error::InvalidStreamId)
                }
            }
            Some(PushState::PushPromise { .. }) => {
                self.conn_events.remove_events_for_push_id(push_id);
                base_handler.queue_control_frame(&HFrame::CancelPush { push_id });
                self.push_streams.close(push_id);
                Ok(())
            }
            Some(PushState::Active { stream_id, .. }) => {
                self.conn_events.remove_events_for_push_id(push_id);
                // Cancel the stream. the transport steam may already be done, so ignore an error.
                mem::drop(base_handler.stream_stop_sending(
                    conn,
                    *stream_id,
                    Error::HttpRequestCancelled.code(),
                ));
                self.push_streams.close(push_id);
                Ok(())
            }
            Some(_) => Err(Error::InvalidStreamId),
        }
    }

    pub fn push_stream_reset(&mut self, push_id: u64, close_type: CloseType) {
        qtrace!("Push stream has been reset, push_id={}", push_id);

        if let Some(push_state) = self.push_streams.get(push_id) {
            match push_state {
                PushState::OnlyPushStream { .. } => {
                    self.push_streams.close(push_id);
                }
                PushState::Active { .. } => {
                    self.push_streams.close(push_id);
                    self.conn_events.remove_events_for_push_id(push_id);
                    if let CloseType::LocalError(app_error) = close_type {
                        self.conn_events.push_reset(push_id, app_error);
                    } else {
                        self.conn_events.push_canceled(push_id);
                    }
                }
                _ => {
                    debug_assert!(
                        false,
                        "Reset cannot actually happen because we do not have a stream."
                    );
                }
            }
        }
    }

    pub fn get_active_stream_id(&mut self, push_id: u64) -> Option<StreamId> {
        match self.push_streams.get(push_id) {
            Some(PushState::Active { stream_id, .. }) => Some(*stream_id),
            _ => None,
        }
    }

    pub fn maybe_send_max_push_id_frame(&mut self, base_handler: &mut Http3Connection) {
        let push_done = self.push_streams.number_done();
        if self.max_concurent_push > 0
            && (self.current_max_push_id - push_done) <= (self.max_concurent_push / 2)
        {
            self.current_max_push_id = push_done + self.max_concurent_push;
            base_handler.queue_control_frame(&HFrame::MaxPushId {
                push_id: self.current_max_push_id,
            });
        }
    }

    pub fn handle_zero_rtt_rejected(&mut self) {
        self.current_max_push_id = 0;
    }

    pub fn clear(&mut self) {
        self.push_streams.clear();
    }

    pub fn can_receive_push(&self) -> bool {
        self.max_concurent_push > 0
    }

    pub fn new_stream_event(&mut self, push_id: u64, event: Http3ClientEvent) {
        match self.push_streams.get_mut(push_id) {
            None => {
                debug_assert!(false, "Push has been closed already.");
            }
            Some(PushState::OnlyPushStream { events, .. }) => {
                events.push(event);
            }
            Some(PushState::Active { .. }) => {
                self.conn_events.insert(event);
            }
            Some(_) => {
                debug_assert!(false, "No record of a stream!");
            }
        }
    }
}

/// `RecvPushEvents` relays a push stream events to `PushController`.
/// It informs `PushController` when a push stream is done or canceled.
/// Also when headers or data is ready and `PushController` decide whether to post
/// `PushHeaderReady` and `PushDataReadable` events or to postpone them if
/// a `push_promise` has not been yet received for the stream.
#[derive(Debug)]
pub(crate) struct RecvPushEvents {
    push_id: u64,
    push_handler: Rc<RefCell<PushController>>,
}

impl RecvPushEvents {
    pub fn new(push_id: u64, push_handler: Rc<RefCell<PushController>>) -> Self {
        Self {
            push_id,
            push_handler,
        }
    }
}

impl RecvStreamEvents for RecvPushEvents {
    fn data_readable(&self, _stream_info: Http3StreamInfo) {
        self.push_handler.borrow_mut().new_stream_event(
            self.push_id,
            Http3ClientEvent::PushDataReadable {
                push_id: self.push_id,
            },
        );
    }

    fn recv_closed(&self, _stream_info: Http3StreamInfo, close_type: CloseType) {
        match close_type {
            CloseType::ResetApp(_) => {}
            CloseType::ResetRemote(_) | CloseType::LocalError(_) => self
                .push_handler
                .borrow_mut()
                .push_stream_reset(self.push_id, close_type),
            CloseType::Done => self.push_handler.borrow_mut().close(self.push_id),
        }
    }
}

impl HttpRecvStreamEvents for RecvPushEvents {
    fn header_ready(
        &self,
        _stream_info: Http3StreamInfo,
        headers: Vec<Header>,
        interim: bool,
        fin: bool,
    ) {
        self.push_handler.borrow_mut().new_stream_event(
            self.push_id,
            Http3ClientEvent::PushHeaderReady {
                push_id: self.push_id,
                headers,
                interim,
                fin,
            },
        );
    }
}
