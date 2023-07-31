// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Stream management for a connection.
use crate::{
    fc::{LocalStreamLimits, ReceiverFlowControl, RemoteStreamLimits, SenderFlowControl},
    frame::Frame,
    packet::PacketBuilder,
    recovery::{RecoveryToken, StreamRecoveryToken},
    recv_stream::{RecvStream, RecvStreams},
    send_stream::{SendStream, SendStreams, TransmissionPriority},
    stats::FrameStats,
    stream_id::{StreamId, StreamType},
    tparams::{self, TransportParametersHandler},
    ConnectionEvents, Error, Res,
};
use neqo_common::{qtrace, qwarn, Role};
use std::cmp::Ordering;
use std::{cell::RefCell, rc::Rc};

pub type SendOrder = i64;

#[derive(Copy, Clone)]
pub struct StreamOrder {
    pub sendorder: Option<SendOrder>,
}

// We want highest to lowest, with None being higher than any value
impl Ord for StreamOrder {
    fn cmp(&self, other: &Self) -> Ordering {
        if self.sendorder.is_some() && other.sendorder.is_some() {
            // We want reverse order (high to low) when both values are specified.
            other.sendorder.cmp(&self.sendorder)
        } else {
            self.sendorder.cmp(&other.sendorder)
        }
    }
}

impl PartialOrd for StreamOrder {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl PartialEq for StreamOrder {
    fn eq(&self, other: &Self) -> bool {
        self.sendorder == other.sendorder
    }
}

impl Eq for StreamOrder {}

pub struct Streams {
    role: Role,
    tps: Rc<RefCell<TransportParametersHandler>>,
    events: ConnectionEvents,
    sender_fc: Rc<RefCell<SenderFlowControl<()>>>,
    receiver_fc: Rc<RefCell<ReceiverFlowControl<()>>>,
    remote_stream_limits: RemoteStreamLimits,
    local_stream_limits: LocalStreamLimits,
    pub(crate) send: SendStreams,
    pub(crate) recv: RecvStreams,
}

impl Streams {
    pub fn new(
        tps: Rc<RefCell<TransportParametersHandler>>,
        role: Role,
        events: ConnectionEvents,
    ) -> Self {
        let limit_bidi = tps
            .borrow()
            .local
            .get_integer(tparams::INITIAL_MAX_STREAMS_BIDI);
        let limit_uni = tps
            .borrow()
            .local
            .get_integer(tparams::INITIAL_MAX_STREAMS_UNI);
        let max_data = tps.borrow().local.get_integer(tparams::INITIAL_MAX_DATA);
        Self {
            role,
            tps,
            events,
            sender_fc: Rc::new(RefCell::new(SenderFlowControl::new((), 0))),
            receiver_fc: Rc::new(RefCell::new(ReceiverFlowControl::new((), max_data))),
            remote_stream_limits: RemoteStreamLimits::new(limit_bidi, limit_uni, role),
            local_stream_limits: LocalStreamLimits::new(role),
            send: SendStreams::default(),
            recv: RecvStreams::default(),
        }
    }

    pub fn is_stream_id_allowed(&self, stream_id: StreamId) -> bool {
        self.remote_stream_limits[stream_id.stream_type()].is_allowed(stream_id)
    }

    pub fn zero_rtt_rejected(&mut self) {
        self.clear_streams();
        debug_assert_eq!(
            self.remote_stream_limits[StreamType::BiDi].max_active(),
            self.tps
                .borrow()
                .local
                .get_integer(tparams::INITIAL_MAX_STREAMS_BIDI)
        );
        debug_assert_eq!(
            self.remote_stream_limits[StreamType::UniDi].max_active(),
            self.tps
                .borrow()
                .local
                .get_integer(tparams::INITIAL_MAX_STREAMS_UNI)
        );
        self.local_stream_limits = LocalStreamLimits::new(self.role);
    }

    pub fn input_frame(&mut self, frame: Frame, stats: &mut FrameStats) -> Res<()> {
        match frame {
            Frame::ResetStream {
                stream_id,
                application_error_code,
                final_size,
            } => {
                stats.reset_stream += 1;
                if let (_, Some(rs)) = self.obtain_stream(stream_id)? {
                    rs.reset(application_error_code, final_size)?;
                }
            }
            Frame::StopSending {
                stream_id,
                application_error_code,
            } => {
                stats.stop_sending += 1;
                self.events
                    .send_stream_stop_sending(stream_id, application_error_code);
                if let (Some(ss), _) = self.obtain_stream(stream_id)? {
                    ss.reset(application_error_code);
                }
            }
            Frame::Stream {
                fin,
                stream_id,
                offset,
                data,
                ..
            } => {
                stats.stream += 1;
                if let (_, Some(rs)) = self.obtain_stream(stream_id)? {
                    rs.inbound_stream_frame(fin, offset, data)?;
                }
            }
            Frame::MaxData { maximum_data } => {
                stats.max_data += 1;
                self.handle_max_data(maximum_data);
            }
            Frame::MaxStreamData {
                stream_id,
                maximum_stream_data,
            } => {
                qtrace!(
                    "Stream {} Received MaxStreamData {}",
                    stream_id,
                    maximum_stream_data
                );
                stats.max_stream_data += 1;
                if let (Some(ss), _) = self.obtain_stream(stream_id)? {
                    ss.set_max_stream_data(maximum_stream_data);
                }
            }
            Frame::MaxStreams {
                stream_type,
                maximum_streams,
            } => {
                stats.max_streams += 1;
                self.handle_max_streams(stream_type, maximum_streams);
            }
            Frame::DataBlocked { data_limit } => {
                // Should never happen since we set data limit to max
                qwarn!("Received DataBlocked with data limit {}", data_limit);
                stats.data_blocked += 1;
                self.handle_data_blocked();
            }
            Frame::StreamDataBlocked { stream_id, .. } => {
                qtrace!("Received StreamDataBlocked");
                stats.stream_data_blocked += 1;
                // Terminate connection with STREAM_STATE_ERROR if send-only
                // stream (-transport 19.13)
                if stream_id.is_send_only(self.role) {
                    return Err(Error::StreamStateError);
                }

                if let (_, Some(rs)) = self.obtain_stream(stream_id)? {
                    rs.send_flowc_update();
                }
            }
            Frame::StreamsBlocked { .. } => {
                stats.streams_blocked += 1;
                // We send an update evry time we retire a stream. There is no need to
                // trigger flow updates here.
            }
            _ => unreachable!("This is not a stream Frame"),
        }
        Ok(())
    }

    fn write_maintenance_frames(
        &mut self,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) {
        // Send `DATA_BLOCKED` as necessary.
        self.sender_fc
            .borrow_mut()
            .write_frames(builder, tokens, stats);
        if builder.is_full() {
            return;
        }

        // Send `MAX_DATA` as necessary.
        self.receiver_fc
            .borrow_mut()
            .write_frames(builder, tokens, stats);
        if builder.is_full() {
            return;
        }

        self.recv.write_frames(builder, tokens, stats);

        self.remote_stream_limits[StreamType::BiDi].write_frames(builder, tokens, stats);
        if builder.is_full() {
            return;
        }
        self.remote_stream_limits[StreamType::UniDi].write_frames(builder, tokens, stats);
        if builder.is_full() {
            return;
        }

        self.local_stream_limits[StreamType::BiDi].write_frames(builder, tokens, stats);
        if builder.is_full() {
            return;
        }

        self.local_stream_limits[StreamType::UniDi].write_frames(builder, tokens, stats);
    }

    pub fn write_frames(
        &mut self,
        priority: TransmissionPriority,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) {
        if priority == TransmissionPriority::Important {
            self.write_maintenance_frames(builder, tokens, stats);
            if builder.is_full() {
                return;
            }
        }

        self.send.write_frames(priority, builder, tokens, stats);
    }

    pub fn lost(&mut self, token: &StreamRecoveryToken) {
        match token {
            StreamRecoveryToken::Stream(st) => self.send.lost(st),
            StreamRecoveryToken::ResetStream { stream_id } => self.send.reset_lost(*stream_id),
            StreamRecoveryToken::StreamDataBlocked { stream_id, limit } => {
                self.send.blocked_lost(*stream_id, *limit)
            }
            StreamRecoveryToken::MaxStreamData {
                stream_id,
                max_data,
            } => {
                if let Ok((_, Some(rs))) = self.obtain_stream(*stream_id) {
                    rs.max_stream_data_lost(*max_data);
                }
            }
            StreamRecoveryToken::StopSending { stream_id } => {
                if let Ok((_, Some(rs))) = self.obtain_stream(*stream_id) {
                    rs.stop_sending_lost();
                }
            }
            StreamRecoveryToken::StreamsBlocked { stream_type, limit } => {
                self.local_stream_limits[*stream_type].frame_lost(*limit);
            }
            StreamRecoveryToken::MaxStreams {
                stream_type,
                max_streams,
            } => {
                self.remote_stream_limits[*stream_type].frame_lost(*max_streams);
            }
            StreamRecoveryToken::DataBlocked(limit) => {
                self.sender_fc.borrow_mut().frame_lost(*limit)
            }
            StreamRecoveryToken::MaxData(maximum_data) => {
                self.receiver_fc.borrow_mut().frame_lost(*maximum_data)
            }
        }
    }

    pub fn acked(&mut self, token: &StreamRecoveryToken) {
        match token {
            StreamRecoveryToken::Stream(st) => self.send.acked(st),
            StreamRecoveryToken::ResetStream { stream_id } => self.send.reset_acked(*stream_id),
            StreamRecoveryToken::StopSending { stream_id } => {
                if let Ok((_, Some(rs))) = self.obtain_stream(*stream_id) {
                    rs.stop_sending_acked();
                }
            }
            // We only worry when these are lost
            StreamRecoveryToken::DataBlocked(_)
            | StreamRecoveryToken::StreamDataBlocked { .. }
            | StreamRecoveryToken::MaxStreamData { .. }
            | StreamRecoveryToken::StreamsBlocked { .. }
            | StreamRecoveryToken::MaxStreams { .. }
            | StreamRecoveryToken::MaxData(_) => (),
        }
    }

    pub fn clear_streams(&mut self) {
        self.send.clear();
        self.recv.clear();
    }

    pub fn cleanup_closed_streams(&mut self) {
        // filter the list, removing closed streams
        self.send.remove_terminal();

        let send = &self.send;
        let (removed_bidi, removed_uni) = self.recv.clear_terminal(send, self.role);

        // Send max_streams updates if we removed remote-initiated recv streams.
        // The updates will be send if any steams has been removed.
        self.remote_stream_limits[StreamType::BiDi].add_retired(removed_bidi);
        self.remote_stream_limits[StreamType::UniDi].add_retired(removed_uni);
    }

    fn ensure_created_if_remote(&mut self, stream_id: StreamId) -> Res<()> {
        if !stream_id.is_remote_initiated(self.role)
            || !self.remote_stream_limits[stream_id.stream_type()].is_new_stream(stream_id)?
        {
            // If it is not a remote stream and stream already exist.
            return Ok(());
        }

        let tp = match stream_id.stream_type() {
            // From the local perspective, this is a remote- originated BiDi stream. From
            // the remote perspective, this is a local-originated BiDi stream. Therefore,
            // look at the local transport parameters for the
            // INITIAL_MAX_STREAM_DATA_BIDI_REMOTE value to decide how much this endpoint
            // will allow its peer to send.
            StreamType::BiDi => tparams::INITIAL_MAX_STREAM_DATA_BIDI_REMOTE,
            StreamType::UniDi => tparams::INITIAL_MAX_STREAM_DATA_UNI,
        };
        let recv_initial_max_stream_data = self.tps.borrow().local.get_integer(tp);

        while self.remote_stream_limits[stream_id.stream_type()].is_new_stream(stream_id)? {
            let next_stream_id =
                self.remote_stream_limits[stream_id.stream_type()].take_stream_id();
            self.events.new_stream(next_stream_id);

            self.recv.insert(
                next_stream_id,
                RecvStream::new(
                    next_stream_id,
                    recv_initial_max_stream_data,
                    Rc::clone(&self.receiver_fc),
                    self.events.clone(),
                ),
            );

            if next_stream_id.is_bidi() {
                // From the local perspective, this is a remote- originated BiDi stream.
                // From the remote perspective, this is a local-originated BiDi stream.
                // Therefore, look at the remote's transport parameters for the
                // INITIAL_MAX_STREAM_DATA_BIDI_LOCAL value to decide how much this endpoint
                // is allowed to send its peer.
                let send_initial_max_stream_data = self
                    .tps
                    .borrow()
                    .remote()
                    .get_integer(tparams::INITIAL_MAX_STREAM_DATA_BIDI_LOCAL);
                self.send.insert(
                    next_stream_id,
                    SendStream::new(
                        next_stream_id,
                        send_initial_max_stream_data,
                        Rc::clone(&self.sender_fc),
                        self.events.clone(),
                    ),
                );
            }
        }
        Ok(())
    }

    /// Get or make a stream, and implicitly open additional streams as
    /// indicated by its stream id.
    pub fn obtain_stream(
        &mut self,
        stream_id: StreamId,
    ) -> Res<(Option<&mut SendStream>, Option<&mut RecvStream>)> {
        self.ensure_created_if_remote(stream_id)?;
        Ok((
            self.send.get_mut(stream_id).ok(),
            self.recv.get_mut(stream_id).ok(),
        ))
    }

    pub fn set_sendorder(&mut self, stream_id: StreamId, sendorder: Option<SendOrder>) -> Res<()> {
        self.send.set_sendorder(stream_id, sendorder)
    }

    pub fn set_fairness(&mut self, stream_id: StreamId, fairness: bool) -> Res<()> {
        self.send.set_fairness(stream_id, fairness)
    }

    pub fn stream_create(&mut self, st: StreamType) -> Res<StreamId> {
        match self.local_stream_limits.take_stream_id(st) {
            None => Err(Error::StreamLimitError),
            Some(new_id) => {
                let send_limit_tp = match st {
                    StreamType::UniDi => tparams::INITIAL_MAX_STREAM_DATA_UNI,
                    StreamType::BiDi => tparams::INITIAL_MAX_STREAM_DATA_BIDI_REMOTE,
                };
                let send_limit = self.tps.borrow().remote().get_integer(send_limit_tp);
                let stream = SendStream::new(
                    new_id,
                    send_limit,
                    Rc::clone(&self.sender_fc),
                    self.events.clone(),
                );
                self.send.insert(new_id, stream);

                if st == StreamType::BiDi {
                    // From the local perspective, this is a local- originated BiDi stream. From the
                    // remote perspective, this is a remote-originated BiDi stream. Therefore, look at
                    // the local transport parameters for the INITIAL_MAX_STREAM_DATA_BIDI_LOCAL value
                    // to decide how much this endpoint will allow its peer to send.
                    let recv_initial_max_stream_data = self
                        .tps
                        .borrow()
                        .local
                        .get_integer(tparams::INITIAL_MAX_STREAM_DATA_BIDI_LOCAL);

                    self.recv.insert(
                        new_id,
                        RecvStream::new(
                            new_id,
                            recv_initial_max_stream_data,
                            Rc::clone(&self.receiver_fc),
                            self.events.clone(),
                        ),
                    );
                }
                Ok(new_id)
            }
        }
    }

    pub fn handle_max_data(&mut self, maximum_data: u64) {
        let conn_was_blocked = self.sender_fc.borrow().available() == 0;
        let conn_credit_increased = self.sender_fc.borrow_mut().update(maximum_data);

        if conn_was_blocked && conn_credit_increased {
            for (id, ss) in &mut self.send {
                if ss.avail() > 0 {
                    // These may not actually all be writable if one
                    // uses up all the conn credit. Not our fault.
                    self.events.send_stream_writable(*id);
                }
            }
        }
    }

    pub fn handle_data_blocked(&mut self) {
        self.receiver_fc.borrow_mut().send_flowc_update();
    }

    pub fn set_initial_limits(&mut self) {
        _ = self.local_stream_limits[StreamType::BiDi].update(
            self.tps
                .borrow()
                .remote()
                .get_integer(tparams::INITIAL_MAX_STREAMS_BIDI),
        );
        _ = self.local_stream_limits[StreamType::UniDi].update(
            self.tps
                .borrow()
                .remote()
                .get_integer(tparams::INITIAL_MAX_STREAMS_UNI),
        );

        // As a client, there are two sets of initial limits for sending stream data.
        // If the second limit is higher and streams have been created, then
        // ensure that streams are not blocked on the lower limit.
        if self.role == Role::Client {
            self.send.update_initial_limit(self.tps.borrow().remote());
        }

        self.sender_fc.borrow_mut().update(
            self.tps
                .borrow()
                .remote()
                .get_integer(tparams::INITIAL_MAX_DATA),
        );

        if self.local_stream_limits[StreamType::BiDi].available() > 0 {
            self.events.send_stream_creatable(StreamType::BiDi);
        }
        if self.local_stream_limits[StreamType::UniDi].available() > 0 {
            self.events.send_stream_creatable(StreamType::UniDi);
        }
    }

    pub fn handle_max_streams(&mut self, stream_type: StreamType, maximum_streams: u64) {
        if self.local_stream_limits[stream_type].update(maximum_streams) {
            self.events.send_stream_creatable(stream_type);
        }
    }

    pub fn get_send_stream_mut(&mut self, stream_id: StreamId) -> Res<&mut SendStream> {
        self.send.get_mut(stream_id)
    }

    pub fn get_send_stream(&self, stream_id: StreamId) -> Res<&SendStream> {
        self.send.get(stream_id)
    }

    pub fn get_recv_stream_mut(&mut self, stream_id: StreamId) -> Res<&mut RecvStream> {
        self.recv.get_mut(stream_id)
    }

    pub fn keep_alive(&mut self, stream_id: StreamId, keep: bool) -> Res<()> {
        self.recv.keep_alive(stream_id, keep)
    }

    pub fn need_keep_alive(&mut self) -> bool {
        self.recv.need_keep_alive()
    }
}
