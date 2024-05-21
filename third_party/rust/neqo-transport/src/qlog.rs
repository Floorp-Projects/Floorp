// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Functions that handle capturing QLOG traces.

use std::{
    ops::{Deref, RangeInclusive},
    time::Duration,
};

use neqo_common::{hex, qinfo, qlog::NeqoQlog, Decoder, IpTosEcn};
use qlog::events::{
    connectivity::{ConnectionStarted, ConnectionState, ConnectionStateUpdated},
    quic::{
        AckedRanges, ErrorSpace, MetricsUpdated, PacketDropped, PacketHeader, PacketLost,
        PacketReceived, PacketSent, QuicFrame, StreamType, VersionInformation,
    },
    EventData, RawInfo,
};
use smallvec::SmallVec;

use crate::{
    connection::State,
    frame::{CloseError, Frame},
    packet::{DecryptedPacket, PacketNumber, PacketType, PublicPacket},
    path::PathRef,
    recovery::SentPacket,
    stream_id::StreamType as NeqoStreamType,
    tparams::{self, TransportParametersHandler},
    version::{Version, VersionConfig, WireVersion},
};

pub fn connection_tparams_set(qlog: &mut NeqoQlog, tph: &TransportParametersHandler) {
    qlog.add_event_data(|| {
        let remote = tph.remote();
        #[allow(clippy::cast_possible_truncation)] // Nope.
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
                active_connection_id_limit: Some(remote.get_integer(tparams::ACTIVE_CONNECTION_ID_LIMIT) as u32),
                initial_max_data: Some(remote.get_integer(tparams::INITIAL_MAX_DATA)),
                initial_max_stream_data_bidi_local: Some(remote.get_integer(tparams::INITIAL_MAX_STREAM_DATA_BIDI_LOCAL)),
                initial_max_stream_data_bidi_remote: Some(remote.get_integer(tparams::INITIAL_MAX_STREAM_DATA_BIDI_REMOTE)),
                initial_max_stream_data_uni: Some(remote.get_integer(tparams::INITIAL_MAX_STREAM_DATA_UNI)),
                initial_max_streams_bidi: Some(remote.get_integer(tparams::INITIAL_MAX_STREAMS_BIDI)),
                initial_max_streams_uni: Some(remote.get_integer(tparams::INITIAL_MAX_STREAMS_UNI)),
                preferred_address: remote.get_preferred_address().and_then(|(paddr, cid)| {
                    Some(qlog::events::quic::PreferredAddress {
                        ip_v4: paddr.ipv4()?.ip().to_string(),
                        ip_v6: paddr.ipv6()?.ip().to_string(),
                        port_v4: paddr.ipv4()?.port(),
                        port_v6: paddr.ipv6()?.port(),
                        connection_id: cid.connection_id().to_string(),
                        stateless_reset_token: hex(cid.reset_token()),
                    })
                }),
            });

        Some(ev_data)
    });
}

pub fn server_connection_started(qlog: &mut NeqoQlog, path: &PathRef) {
    connection_started(qlog, path);
}

pub fn client_connection_started(qlog: &mut NeqoQlog, path: &PathRef) {
    connection_started(qlog, path);
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
    });
}

pub fn connection_state_updated(qlog: &mut NeqoQlog, new: &State) {
    qlog.add_event_data(|| {
        let ev_data = EventData::ConnectionStateUpdated(ConnectionStateUpdated {
            old: None,
            new: match new {
                State::Init | State::WaitInitial => ConnectionState::Attempted,
                State::WaitVersion | State::Handshaking => ConnectionState::HandshakeStarted,
                State::Connected => ConnectionState::HandshakeCompleted,
                State::Confirmed => ConnectionState::HandshakeConfirmed,
                State::Closing { .. } => ConnectionState::Closing,
                State::Draining { .. } => ConnectionState::Draining,
                State::Closed { .. } => ConnectionState::Closed,
            },
        });

        Some(ev_data)
    });
}

pub fn client_version_information_initiated(qlog: &mut NeqoQlog, version_config: &VersionConfig) {
    qlog.add_event_data(|| {
        Some(EventData::VersionInformation(VersionInformation {
            client_versions: Some(
                version_config
                    .all()
                    .iter()
                    .map(|v| format!("{:02x}", v.wire_version()))
                    .collect(),
            ),
            server_versions: None,
            chosen_version: Some(format!("{:02x}", version_config.initial().wire_version())),
        }))
    });
}

pub fn client_version_information_negotiated(
    qlog: &mut NeqoQlog,
    client: &[Version],
    server: &[WireVersion],
    chosen: Version,
) {
    qlog.add_event_data(|| {
        Some(EventData::VersionInformation(VersionInformation {
            client_versions: Some(
                client
                    .iter()
                    .map(|v| format!("{:02x}", v.wire_version()))
                    .collect(),
            ),
            server_versions: Some(server.iter().map(|v| format!("{v:02x}")).collect()),
            chosen_version: Some(format!("{:02x}", chosen.wire_version())),
        }))
    });
}

pub fn server_version_information_failed(
    qlog: &mut NeqoQlog,
    server: &[Version],
    client: WireVersion,
) {
    qlog.add_event_data(|| {
        Some(EventData::VersionInformation(VersionInformation {
            client_versions: Some(vec![format!("{client:02x}")]),
            server_versions: Some(
                server
                    .iter()
                    .map(|v| format!("{:02x}", v.wire_version()))
                    .collect(),
            ),
            chosen_version: None,
        }))
    });
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
        let header = PacketHeader::with_type(pt.into(), Some(pn), None, None, None);
        let raw = RawInfo {
            length: Some(plen as u64),
            payload_length: None,
            data: None,
        };

        let mut frames = SmallVec::new();
        while d.remaining() > 0 {
            if let Ok(f) = Frame::decode(&mut d) {
                frames.push(QuicFrame::from(f));
            } else {
                qinfo!("qlog: invalid frame");
                break;
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
    });
}

pub fn packet_dropped(qlog: &mut NeqoQlog, public_packet: &PublicPacket) {
    qlog.add_event_data(|| {
        let header =
            PacketHeader::with_type(public_packet.packet_type().into(), None, None, None, None);
        let raw = RawInfo {
            length: Some(public_packet.len() as u64),
            payload_length: None,
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
    });
}

pub fn packets_lost(qlog: &mut NeqoQlog, pkts: &[SentPacket]) {
    qlog.add_event_with_stream(|stream| {
        for pkt in pkts {
            let header =
                PacketHeader::with_type(pkt.packet_type().into(), Some(pkt.pn()), None, None, None);

            let ev_data = EventData::PacketLost(PacketLost {
                header: Some(header),
                trigger: None,
                frames: None,
            });

            stream.add_event_data_now(ev_data)?;
        }
        Ok(())
    });
}

pub fn packet_received(
    qlog: &mut NeqoQlog,
    public_packet: &PublicPacket,
    payload: &DecryptedPacket,
) {
    qlog.add_event_with_stream(|stream| {
        let mut d = Decoder::from(&payload[..]);

        let header = PacketHeader::with_type(
            public_packet.packet_type().into(),
            Some(payload.pn()),
            None,
            None,
            None,
        );
        let raw = RawInfo {
            length: Some(public_packet.len() as u64),
            payload_length: None,
            data: None,
        };

        let mut frames = Vec::new();

        while d.remaining() > 0 {
            if let Ok(f) = Frame::decode(&mut d) {
                frames.push(QuicFrame::from(f));
            } else {
                qinfo!("qlog: invalid frame");
                break;
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
    });
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
            #[allow(clippy::cast_precision_loss)] // Nought to do here.
            match metric {
                QlogMetric::MinRtt(v) => min_rtt = Some(v.as_secs_f32() * 1000.0),
                QlogMetric::SmoothedRtt(v) => smoothed_rtt = Some(v.as_secs_f32() * 1000.0),
                QlogMetric::LatestRtt(v) => latest_rtt = Some(v.as_secs_f32() * 1000.0),
                QlogMetric::RttVariance(v) => rtt_variance = Some(*v as f32),
                QlogMetric::PtoCount(v) => pto_count = Some(u16::try_from(*v).unwrap()),
                QlogMetric::CongestionWindow(v) => {
                    congestion_window = Some(u64::try_from(*v).unwrap());
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
    });
}

// Helper functions

#[allow(clippy::too_many_lines)] // Yeah, but it's a nice match.
#[allow(clippy::cast_possible_truncation, clippy::cast_precision_loss)] // No choice here.
impl From<Frame<'_>> for QuicFrame {
    fn from(frame: Frame) -> Self {
        match frame {
            Frame::Padding(len) => QuicFrame::Padding {
                length: None,
                payload_length: u32::from(len),
            },
            Frame::Ping => QuicFrame::Ping {
                length: None,
                payload_length: None,
            },
            Frame::Ack {
                largest_acknowledged,
                ack_delay,
                first_ack_range,
                ack_ranges,
                ecn_count,
            } => {
                let ranges =
                    Frame::decode_ack_frame(largest_acknowledged, first_ack_range, &ack_ranges)
                        .ok();

                let acked_ranges = ranges.map(|all| {
                    AckedRanges::Double(
                        all.into_iter()
                            .map(RangeInclusive::into_inner)
                            .collect::<Vec<_>>(),
                    )
                });

                QuicFrame::Ack {
                    ack_delay: Some(ack_delay as f32 / 1000.0),
                    acked_ranges,
                    ect1: ecn_count.map(|c| c[IpTosEcn::Ect1]),
                    ect0: ecn_count.map(|c| c[IpTosEcn::Ect0]),
                    ce: ecn_count.map(|c| c[IpTosEcn::Ce]),
                    length: None,
                    payload_length: None,
                }
            }
            Frame::ResetStream {
                stream_id,
                application_error_code,
                final_size,
            } => QuicFrame::ResetStream {
                stream_id: stream_id.as_u64(),
                error_code: application_error_code,
                final_size,
                length: None,
                payload_length: None,
            },
            Frame::StopSending {
                stream_id,
                application_error_code,
            } => QuicFrame::StopSending {
                stream_id: stream_id.as_u64(),
                error_code: application_error_code,
                length: None,
                payload_length: None,
            },
            Frame::Crypto { offset, data } => QuicFrame::Crypto {
                offset,
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
                offset,
                length: data.len() as u64,
                fin: Some(fin),
                raw: None,
            },
            Frame::MaxData { maximum_data } => QuicFrame::MaxData {
                maximum: maximum_data,
            },
            Frame::MaxStreamData {
                stream_id,
                maximum_stream_data,
            } => QuicFrame::MaxStreamData {
                stream_id: stream_id.as_u64(),
                maximum: maximum_stream_data,
            },
            Frame::MaxStreams {
                stream_type,
                maximum_streams,
            } => QuicFrame::MaxStreams {
                stream_type: match stream_type {
                    NeqoStreamType::BiDi => StreamType::Bidirectional,
                    NeqoStreamType::UniDi => StreamType::Unidirectional,
                },
                maximum: maximum_streams,
            },
            Frame::DataBlocked { data_limit } => QuicFrame::DataBlocked { limit: data_limit },
            Frame::StreamDataBlocked {
                stream_id,
                stream_data_limit,
            } => QuicFrame::StreamDataBlocked {
                stream_id: stream_id.as_u64(),
                limit: stream_data_limit,
            },
            Frame::StreamsBlocked {
                stream_type,
                stream_limit,
            } => QuicFrame::StreamsBlocked {
                stream_type: match stream_type {
                    NeqoStreamType::BiDi => StreamType::Bidirectional,
                    NeqoStreamType::UniDi => StreamType::Unidirectional,
                },
                limit: stream_limit,
            },
            Frame::NewConnectionId {
                sequence_number,
                retire_prior,
                connection_id,
                stateless_reset_token,
            } => QuicFrame::NewConnectionId {
                sequence_number: sequence_number as u32,
                retire_prior_to: retire_prior as u32,
                connection_id_length: Some(connection_id.len() as u8),
                connection_id: hex(connection_id),
                stateless_reset_token: Some(hex(stateless_reset_token)),
            },
            Frame::RetireConnectionId { sequence_number } => QuicFrame::RetireConnectionId {
                sequence_number: sequence_number as u32,
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
                reason: Some(reason_phrase),
                trigger_frame_type: Some(frame_type),
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
}

impl From<PacketType> for qlog::events::quic::PacketType {
    fn from(value: PacketType) -> Self {
        match value {
            PacketType::Initial => qlog::events::quic::PacketType::Initial,
            PacketType::Handshake => qlog::events::quic::PacketType::Handshake,
            PacketType::ZeroRtt => qlog::events::quic::PacketType::ZeroRtt,
            PacketType::Short => qlog::events::quic::PacketType::OneRtt,
            PacketType::Retry => qlog::events::quic::PacketType::Retry,
            PacketType::VersionNegotiation => qlog::events::quic::PacketType::VersionNegotiation,
            PacketType::OtherVersion => qlog::events::quic::PacketType::Unknown,
        }
    }
}
