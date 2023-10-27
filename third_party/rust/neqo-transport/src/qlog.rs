// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Functions that handle capturing QLOG traces.

use std::{
    convert::TryFrom,
    ops::{Deref, RangeInclusive},
    string::String,
    time::Duration,
};

use qlog::events::{
    connectivity::{ConnectionStarted, ConnectionState, ConnectionStateUpdated},
    quic::{
        AckedRanges, ErrorSpace, MetricsUpdated, PacketDropped, PacketHeader, PacketLost,
        PacketReceived, PacketSent, QuicFrame, StreamType,
    },
    Event, EventData, RawInfo,
};

use neqo_common::{hex, qinfo, qlog::NeqoQlog, Decoder};
use smallvec::SmallVec;

use crate::{
    connection::State,
    frame::{CloseError, Frame},
    packet::{DecryptedPacket, PacketNumber, PacketType, PublicPacket},
    path::PathRef,
    stream_id::StreamType as NeqoStreamType,
    tparams::{self, TransportParametersHandler},
    tracking::SentPacket,
};

pub fn connection_tparams_set(qlog: &mut NeqoQlog, tph: &TransportParametersHandler) {
    qlog.add_event(|| {
        let remote = tph.remote();
        let ev_data = EventData::TransportParametersSet(
            qlog::events::quic::TransportParametersSet {
                owner: None,
                resumption_allowed: None,
                early_data_enabled: None,
                tls_cipher: None,
                aead_tag_length: None,
                original_destination_connection_id: remote
                .get_bytes(tparams::ORIGINAL_DESTINATION_CONNECTION_ID)
                .map(hex),
                initial_source_connection_id: None,
                retry_source_connection_id: None,
                stateless_reset_token: remote.get_bytes(tparams::STATELESS_RESET_TOKEN).map(hex),
                disable_active_migration: if remote.get_empty(tparams::DISABLE_MIGRATION) {
                    Some(true)
                } else {
                    None
                },
                max_idle_timeout: Some(remote.get_integer(tparams::IDLE_TIMEOUT)),
                max_udp_payload_size: Some(remote.get_integer(tparams::MAX_UDP_PAYLOAD_SIZE) as u32),
                ack_delay_exponent: Some(remote.get_integer(tparams::ACK_DELAY_EXPONENT) as u16),
                max_ack_delay: Some(remote.get_integer(tparams::MAX_ACK_DELAY) as u16),
                // TODO(hawkinsw@obs.cr): We do not yet handle ACTIVE_CONNECTION_ID_LIMIT in tparams yet.
                active_connection_id_limit: None,
                initial_max_data: Some(remote.get_integer(tparams::INITIAL_MAX_DATA)),
                initial_max_stream_data_bidi_local: Some(remote.get_integer(tparams::INITIAL_MAX_STREAM_DATA_BIDI_LOCAL)),
                initial_max_stream_data_bidi_remote: Some(remote.get_integer(tparams::INITIAL_MAX_STREAM_DATA_BIDI_REMOTE)),
                initial_max_stream_data_uni: Some(remote.get_integer(tparams::INITIAL_MAX_STREAM_DATA_UNI)),
                initial_max_streams_bidi: Some(remote.get_integer(tparams::INITIAL_MAX_STREAMS_BIDI)),
                initial_max_streams_uni: Some(remote.get_integer(tparams::INITIAL_MAX_STREAMS_UNI)),
                // TODO(hawkinsw@obs.cr): We do not yet handle PREFERRED_ADDRESS in tparams yet.
                preferred_address: None,
            });

        // This event occurs very early, so just mark the time as 0.0.
        Some(Event::with_time(0.0, ev_data))
    })
}

pub fn server_connection_started(qlog: &mut NeqoQlog, path: &PathRef) {
    connection_started(qlog, path)
}

pub fn client_connection_started(qlog: &mut NeqoQlog, path: &PathRef) {
    connection_started(qlog, path)
}

fn connection_started(qlog: &mut NeqoQlog, path: &PathRef) {
    qlog.add_event_data(|| {
        let p = path.deref().borrow();
        let ev_data = EventData::ConnectionStarted(ConnectionStarted {
            ip_version: if p.local_address().ip().is_ipv4() {
                Some("ipv4".into())
            } else {
                Some("ipv6".into())
            },
            src_ip: format!("{}", p.local_address().ip()),
            dst_ip: format!("{}", p.remote_address().ip()),
            protocol: Some("QUIC".into()),
            src_port: p.local_address().port().into(),
            dst_port: p.remote_address().port().into(),
            src_cid: Some(format!("{}", p.local_cid())),
            dst_cid: Some(format!("{}", p.remote_cid())),
        });

        Some(ev_data)
    })
}

pub fn connection_state_updated(qlog: &mut NeqoQlog, new: &State) {
    qlog.add_event_data(|| {
        let ev_data = EventData::ConnectionStateUpdated(ConnectionStateUpdated {
            old: None,
            new: match new {
                State::Init => ConnectionState::Attempted,
                State::WaitInitial => ConnectionState::Attempted,
                State::WaitVersion | State::Handshaking => ConnectionState::HandshakeStarted,
                State::Connected => ConnectionState::HandshakeCompleted,
                State::Confirmed => ConnectionState::HandshakeConfirmed,
                State::Closing { .. } => ConnectionState::Draining,
                State::Draining { .. } => ConnectionState::Draining,
                State::Closed { .. } => ConnectionState::Closed,
            },
        });

        Some(ev_data)
    })
}

pub fn packet_sent(
    qlog: &mut NeqoQlog,
    pt: PacketType,
    pn: PacketNumber,
    plen: usize,
    body: &[u8],
) {
    qlog.add_event_with_stream(|stream| {
        let mut d = Decoder::from(body);
        let header = PacketHeader::with_type(to_qlog_pkt_type(pt), pn, None, None, None);
        let raw = RawInfo {
            length: None,
            payload_length: Some(plen as u64),
            data: None,
        };

        let mut frames = SmallVec::new();
        while d.remaining() > 0 {
            match Frame::decode(&mut d) {
                Ok(f) => {
                    frames.push(frame_to_qlogframe(&f));
                }
                Err(_) => {
                    qinfo!("qlog: invalid frame");
                    break;
                }
            }
        }

        let ev_data = EventData::PacketSent(PacketSent {
            header,
            frames: Some(frames),
            is_coalesced: None,
            retry_token: None,
            stateless_reset_token: None,
            supported_versions: None,
            raw: Some(raw),
            datagram_id: None,
            send_at_time: None,
            trigger: None,
        });

        stream.add_event_data_now(ev_data)
    })
}

pub fn packet_dropped(qlog: &mut NeqoQlog, payload: &PublicPacket) {
    qlog.add_event_data(|| {
        // TODO: packet number is optional in the spec but qlog crate doesn't support that, so use a placeholder value of 0
        let header =
            PacketHeader::with_type(to_qlog_pkt_type(payload.packet_type()), 0, None, None, None);
        let raw = RawInfo {
            length: None,
            payload_length: Some(payload.len() as u64),
            data: None,
        };

        let ev_data = EventData::PacketDropped(PacketDropped {
            header: Some(header),
            raw: Some(raw),
            datagram_id: None,
            details: None,
            trigger: None,
        });

        Some(ev_data)
    })
}

pub fn packets_lost(qlog: &mut NeqoQlog, pkts: &[SentPacket]) {
    qlog.add_event_with_stream(|stream| {
        for pkt in pkts {
            let header =
                PacketHeader::with_type(to_qlog_pkt_type(pkt.pt), pkt.pn, None, None, None);

            let ev_data = EventData::PacketLost(PacketLost {
                header: Some(header),
                trigger: None,
                frames: None,
            });

            stream.add_event_data_now(ev_data)?;
        }
        Ok(())
    })
}

pub fn packet_received(
    qlog: &mut NeqoQlog,
    public_packet: &PublicPacket,
    payload: &DecryptedPacket,
) {
    qlog.add_event_with_stream(|stream| {
        let mut d = Decoder::from(&payload[..]);

        let header = PacketHeader::with_type(
            to_qlog_pkt_type(public_packet.packet_type()),
            payload.pn(),
            None,
            None,
            None,
        );
        let raw = RawInfo {
            length: None,
            payload_length: Some(public_packet.len() as u64),
            data: None,
        };

        let mut frames = Vec::new();

        while d.remaining() > 0 {
            match Frame::decode(&mut d) {
                Ok(f) => frames.push(frame_to_qlogframe(&f)),
                Err(_) => {
                    qinfo!("qlog: invalid frame");
                    break;
                }
            }
        }

        let ev_data = EventData::PacketReceived(PacketReceived {
            header,
            frames: Some(frames),
            is_coalesced: None,
            retry_token: None,
            stateless_reset_token: None,
            supported_versions: None,
            raw: Some(raw),
            datagram_id: None,
            trigger: None,
        });

        stream.add_event_data_now(ev_data)
    })
}

#[allow(dead_code)]
pub enum QlogMetric {
    MinRtt(Duration),
    SmoothedRtt(Duration),
    LatestRtt(Duration),
    RttVariance(u64),
    MaxAckDelay(u64),
    PtoCount(usize),
    CongestionWindow(usize),
    BytesInFlight(usize),
    SsThresh(usize),
    PacketsInFlight(u64),
    InRecovery(bool),
    PacingRate(u64),
}

pub fn metrics_updated(qlog: &mut NeqoQlog, updated_metrics: &[QlogMetric]) {
    debug_assert!(!updated_metrics.is_empty());

    qlog.add_event_data(|| {
        let mut min_rtt: Option<f32> = None;
        let mut smoothed_rtt: Option<f32> = None;
        let mut latest_rtt: Option<f32> = None;
        let mut rtt_variance: Option<f32> = None;
        let mut pto_count: Option<u16> = None;
        let mut congestion_window: Option<u64> = None;
        let mut bytes_in_flight: Option<u64> = None;
        let mut ssthresh: Option<u64> = None;
        let mut packets_in_flight: Option<u64> = None;
        let mut pacing_rate: Option<u64> = None;

        for metric in updated_metrics {
            match metric {
                QlogMetric::MinRtt(v) => min_rtt = Some(v.as_secs_f32() * 1000.0),
                QlogMetric::SmoothedRtt(v) => smoothed_rtt = Some(v.as_secs_f32() * 1000.0),
                QlogMetric::LatestRtt(v) => latest_rtt = Some(v.as_secs_f32() * 1000.0),
                QlogMetric::RttVariance(v) => rtt_variance = Some(*v as f32),
                QlogMetric::PtoCount(v) => pto_count = Some(u16::try_from(*v).unwrap()),
                QlogMetric::CongestionWindow(v) => {
                    congestion_window = Some(u64::try_from(*v).unwrap())
                }
                QlogMetric::BytesInFlight(v) => bytes_in_flight = Some(u64::try_from(*v).unwrap()),
                QlogMetric::SsThresh(v) => ssthresh = Some(u64::try_from(*v).unwrap()),
                QlogMetric::PacketsInFlight(v) => packets_in_flight = Some(*v),
                QlogMetric::PacingRate(v) => pacing_rate = Some(*v),
                _ => (),
            }
        }

        let ev_data = EventData::MetricsUpdated(MetricsUpdated {
            min_rtt,
            smoothed_rtt,
            latest_rtt,
            rtt_variance,
            pto_count,
            congestion_window,
            bytes_in_flight,
            ssthresh,
            packets_in_flight,
            pacing_rate,
        });

        Some(ev_data)
    })
}

// Helper functions

fn frame_to_qlogframe(frame: &Frame) -> QuicFrame {
    match frame {
        Frame::Padding => QuicFrame::Padding,
        Frame::Ping => QuicFrame::Ping,
        Frame::Ack {
            largest_acknowledged,
            ack_delay,
            first_ack_range,
            ack_ranges,
        } => {
            let ranges =
                Frame::decode_ack_frame(*largest_acknowledged, *first_ack_range, ack_ranges).ok();

            let acked_ranges = ranges.map(|all| {
                AckedRanges::Double(
                    all.into_iter()
                        .map(RangeInclusive::into_inner)
                        .collect::<Vec<_>>(),
                )
            });

            QuicFrame::Ack {
                ack_delay: Some(*ack_delay as f32 / 1000.0),
                acked_ranges,
                ect1: None,
                ect0: None,
                ce: None,
            }
        }
        Frame::ResetStream {
            stream_id,
            application_error_code,
            final_size,
        } => QuicFrame::ResetStream {
            stream_id: stream_id.as_u64(),
            error_code: *application_error_code,
            final_size: *final_size,
        },
        Frame::StopSending {
            stream_id,
            application_error_code,
        } => QuicFrame::StopSending {
            stream_id: stream_id.as_u64(),
            error_code: *application_error_code,
        },
        Frame::Crypto { offset, data } => QuicFrame::Crypto {
            offset: *offset,
            length: data.len() as u64,
        },
        Frame::NewToken { token } => QuicFrame::NewToken {
            token: qlog::Token {
                ty: Some(qlog::TokenType::Retry),
                details: None,
                raw: Some(RawInfo {
                    data: Some(hex(token)),
                    length: Some(token.len() as u64),
                    payload_length: None,
                }),
            },
        },
        Frame::Stream {
            fin,
            stream_id,
            offset,
            data,
            ..
        } => QuicFrame::Stream {
            stream_id: stream_id.as_u64(),
            offset: *offset,
            length: data.len() as u64,
            fin: Some(*fin),
            raw: None,
        },
        Frame::MaxData { maximum_data } => QuicFrame::MaxData {
            maximum: *maximum_data,
        },
        Frame::MaxStreamData {
            stream_id,
            maximum_stream_data,
        } => QuicFrame::MaxStreamData {
            stream_id: stream_id.as_u64(),
            maximum: *maximum_stream_data,
        },
        Frame::MaxStreams {
            stream_type,
            maximum_streams,
        } => QuicFrame::MaxStreams {
            stream_type: match stream_type {
                NeqoStreamType::BiDi => StreamType::Bidirectional,
                NeqoStreamType::UniDi => StreamType::Unidirectional,
            },
            maximum: *maximum_streams,
        },
        Frame::DataBlocked { data_limit } => QuicFrame::DataBlocked { limit: *data_limit },
        Frame::StreamDataBlocked {
            stream_id,
            stream_data_limit,
        } => QuicFrame::StreamDataBlocked {
            stream_id: stream_id.as_u64(),
            limit: *stream_data_limit,
        },
        Frame::StreamsBlocked {
            stream_type,
            stream_limit,
        } => QuicFrame::StreamsBlocked {
            stream_type: match stream_type {
                NeqoStreamType::BiDi => StreamType::Bidirectional,
                NeqoStreamType::UniDi => StreamType::Unidirectional,
            },
            limit: *stream_limit,
        },
        Frame::NewConnectionId {
            sequence_number,
            retire_prior,
            connection_id,
            stateless_reset_token,
        } => QuicFrame::NewConnectionId {
            sequence_number: *sequence_number as u32,
            retire_prior_to: *retire_prior as u32,
            connection_id_length: Some(connection_id.len() as u8),
            connection_id: hex(connection_id),
            stateless_reset_token: Some(hex(stateless_reset_token)),
        },
        Frame::RetireConnectionId { sequence_number } => QuicFrame::RetireConnectionId {
            sequence_number: *sequence_number as u32,
        },
        Frame::PathChallenge { data } => QuicFrame::PathChallenge {
            data: Some(hex(data)),
        },
        Frame::PathResponse { data } => QuicFrame::PathResponse {
            data: Some(hex(data)),
        },
        Frame::ConnectionClose {
            error_code,
            frame_type,
            reason_phrase,
        } => QuicFrame::ConnectionClose {
            error_space: match error_code {
                CloseError::Transport(_) => Some(ErrorSpace::TransportError),
                CloseError::Application(_) => Some(ErrorSpace::ApplicationError),
            },
            error_code: Some(error_code.code()),
            error_code_value: Some(0),
            reason: Some(String::from_utf8_lossy(reason_phrase).to_string()),
            trigger_frame_type: Some(*frame_type),
        },
        Frame::HandshakeDone => QuicFrame::HandshakeDone,
        Frame::AckFrequency { .. } => QuicFrame::Unknown {
            frame_type_value: None,
            raw_frame_type: frame.get_type(),
            raw: None,
        },
        Frame::Datagram { data, .. } => QuicFrame::Datagram {
            length: data.len() as u64,
            raw: None,
        },
    }
}

fn to_qlog_pkt_type(ptype: PacketType) -> qlog::events::quic::PacketType {
    match ptype {
        PacketType::Initial => qlog::events::quic::PacketType::Initial,
        PacketType::Handshake => qlog::events::quic::PacketType::Handshake,
        PacketType::ZeroRtt => qlog::events::quic::PacketType::ZeroRtt,
        PacketType::Short => qlog::events::quic::PacketType::OneRtt,
        PacketType::Retry => qlog::events::quic::PacketType::Retry,
        PacketType::VersionNegotiation => qlog::events::quic::PacketType::VersionNegotiation,
        PacketType::OtherVersion => qlog::events::quic::PacketType::Unknown,
    }
}
