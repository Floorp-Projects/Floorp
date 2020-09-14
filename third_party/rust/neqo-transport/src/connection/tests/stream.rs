// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::super::State;
use super::{connect, default_client, default_server, maybe_authenticate};
use crate::events::ConnectionEvent;
use crate::frame::{Frame, StreamType};
use crate::recv_stream::RECV_BUFFER_SIZE;
use crate::send_stream::SEND_BUFFER_SIZE;
use crate::tparams::{self, TransportParameter};
use crate::tracking::MAX_UNACKED_PKTS;
use crate::Error;

use neqo_common::qdebug;
use std::convert::TryFrom;
use test_fixture::now;

#[test]
fn stream_create() {
    let mut client = default_client();

    let out = client.process(None, now());
    let mut server = default_server();
    let out = server.process(out.dgram(), now());

    let out = client.process(out.dgram(), now());
    let _ = server.process(out.dgram(), now());
    assert!(maybe_authenticate(&mut client));
    let out = client.process(None, now());

    // client now in State::Connected
    assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
    assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 6);
    assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);
    assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 4);

    let _ = server.process(out.dgram(), now());
    // server now in State::Connected
    assert_eq!(server.stream_create(StreamType::UniDi).unwrap(), 3);
    assert_eq!(server.stream_create(StreamType::UniDi).unwrap(), 7);
    assert_eq!(server.stream_create(StreamType::BiDi).unwrap(), 1);
    assert_eq!(server.stream_create(StreamType::BiDi).unwrap(), 5);
}

#[test]
#[allow(clippy::cognitive_complexity)]
// tests stream send/recv after connection is established.
fn transfer() {
    let mut client = default_client();
    let mut server = default_server();

    qdebug!("---- client");
    let out = client.process(None, now());
    assert!(out.as_dgram_ref().is_some());
    qdebug!("Output={:0x?}", out.as_dgram_ref());
    // -->> Initial[0]: CRYPTO[CH]

    qdebug!("---- server");
    let out = server.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_some());
    qdebug!("Output={:0x?}", out.as_dgram_ref());
    // <<-- Initial[0]: CRYPTO[SH] ACK[0]
    // <<-- Handshake[0]: CRYPTO[EE, CERT, CV, FIN]

    qdebug!("---- client");
    let out = client.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_some());
    qdebug!("Output={:0x?}", out.as_dgram_ref());
    // -->> Initial[1]: ACK[0]

    let out = server.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_none());

    assert!(maybe_authenticate(&mut client));

    qdebug!("---- client");
    let out = client.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_some());
    assert_eq!(*client.state(), State::Connected);
    qdebug!("Output={:0x?}", out.as_dgram_ref());
    // -->> Handshake[0]: CRYPTO[FIN], ACK[0]

    qdebug!("---- server");
    let out = server.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_some());
    assert_eq!(*server.state(), State::Confirmed);
    qdebug!("Output={:0x?}", out.as_dgram_ref());
    // ACK and HANDSHAKE_DONE
    // -->> nothing

    qdebug!("---- client");
    // Send
    let client_stream_id = client.stream_create(StreamType::UniDi).unwrap();
    client.stream_send(client_stream_id, &[6; 100]).unwrap();
    client.stream_send(client_stream_id, &[7; 40]).unwrap();
    client.stream_send(client_stream_id, &[8; 4000]).unwrap();

    // Send to another stream but some data after fin has been set
    let client_stream_id2 = client.stream_create(StreamType::UniDi).unwrap();
    client.stream_send(client_stream_id2, &[6; 60]).unwrap();
    client.stream_close_send(client_stream_id2).unwrap();
    client.stream_send(client_stream_id2, &[7; 50]).unwrap_err();
    // Sending this much takes a few datagrams.
    let mut datagrams = vec![];
    let mut out = client.process(out.dgram(), now());
    while let Some(d) = out.dgram() {
        datagrams.push(d);
        out = client.process(None, now());
    }
    assert_eq!(datagrams.len(), 4);
    assert_eq!(*client.state(), State::Confirmed);

    qdebug!("---- server");
    for (d_num, d) in datagrams.into_iter().enumerate() {
        let out = server.process(Some(d), now());
        assert_eq!(
            out.as_dgram_ref().is_some(),
            (d_num + 1) % (MAX_UNACKED_PKTS + 1) == 0
        );
        qdebug!("Output={:0x?}", out.as_dgram_ref());
    }
    assert_eq!(*server.state(), State::Confirmed);

    let mut buf = vec![0; 4000];

    let mut stream_ids = server.events().filter_map(|evt| match evt {
        ConnectionEvent::NewStream { stream_id, .. } => Some(stream_id),
        _ => None,
    });
    let stream_id = stream_ids.next().expect("should have a new stream event");
    let (received1, fin1) = server.stream_recv(stream_id.as_u64(), &mut buf).unwrap();
    assert_eq!(received1, 4000);
    assert_eq!(fin1, false);
    let (received2, fin2) = server.stream_recv(stream_id.as_u64(), &mut buf).unwrap();
    assert_eq!(received2, 140);
    assert_eq!(fin2, false);

    let stream_id = stream_ids
        .next()
        .expect("should have a second new stream event");
    let (received3, fin3) = server.stream_recv(stream_id.as_u64(), &mut buf).unwrap();
    assert_eq!(received3, 60);
    assert_eq!(fin3, true);
}

#[test]
// Send fin even if a peer closes a reomte bidi send stream before sending any data.
fn report_fin_when_stream_closed_wo_data() {
    // Note that the two servers in this test will get different anti-replay filters.
    // That's OK because we aren't testing anti-replay.
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    // create a stream
    let stream_id = client.stream_create(StreamType::BiDi).unwrap();
    client.stream_send(stream_id, &[0x00]).unwrap();
    let out = client.process(None, now());
    let _ = server.process(out.dgram(), now());

    assert_eq!(Ok(()), server.stream_close_send(stream_id));
    let out = server.process(None, now());
    let _ = client.process(out.dgram(), now());
    let stream_readable = |e| matches!(e, ConnectionEvent::RecvStreamReadable {..});
    assert!(client.events().any(stream_readable));
}

#[test]
fn max_data() {
    const SMALL_MAX_DATA: usize = 16383;

    let mut client = default_client();
    let mut server = default_server();

    server
        .set_local_tparam(
            tparams::INITIAL_MAX_DATA,
            TransportParameter::Integer(u64::try_from(SMALL_MAX_DATA).unwrap()),
        )
        .unwrap();

    connect(&mut client, &mut server);

    let stream_id = client.stream_create(StreamType::UniDi).unwrap();
    assert_eq!(client.events().count(), 2); // SendStreamWritable, StateChange(connected)
    assert_eq!(stream_id, 2);
    assert_eq!(
        client.stream_avail_send_space(stream_id).unwrap(),
        SMALL_MAX_DATA
    );
    assert_eq!(
        client
            .stream_send(stream_id, &vec![b'a'; RECV_BUFFER_SIZE].into_boxed_slice())
            .unwrap(),
        SMALL_MAX_DATA
    );
    assert_eq!(client.events().count(), 0);

    assert_eq!(client.stream_send(stream_id, b"hello").unwrap(), 0);
    client
        .send_streams
        .get_mut(stream_id.into())
        .unwrap()
        .mark_as_sent(0, 4096, false);
    assert_eq!(client.events().count(), 0);
    client
        .send_streams
        .get_mut(stream_id.into())
        .unwrap()
        .mark_as_acked(0, 4096, false);
    assert_eq!(client.events().count(), 0);

    assert_eq!(client.stream_send(stream_id, b"hello").unwrap(), 0);
    // no event because still limited by conn max data
    assert_eq!(client.events().count(), 0);

    // Increase max data. Avail space now limited by stream credit
    client.handle_max_data(100_000_000);
    assert_eq!(
        client.stream_avail_send_space(stream_id).unwrap(),
        SEND_BUFFER_SIZE - SMALL_MAX_DATA
    );

    // Increase max stream data. Avail space now limited by tx buffer
    client
        .send_streams
        .get_mut(stream_id.into())
        .unwrap()
        .set_max_stream_data(100_000_000);
    assert_eq!(
        client.stream_avail_send_space(stream_id).unwrap(),
        SEND_BUFFER_SIZE - SMALL_MAX_DATA + 4096
    );

    let evts = client.events().collect::<Vec<_>>();
    assert_eq!(evts.len(), 1);
    assert!(matches!(evts[0], ConnectionEvent::SendStreamWritable{..}));
}

#[test]
// If we send a stop_sending to the peer, we should not accept more data from the peer.
fn do_not_accept_data_after_stop_sending() {
    // Note that the two servers in this test will get different anti-replay filters.
    // That's OK because we aren't testing anti-replay.
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    // create a stream
    let stream_id = client.stream_create(StreamType::BiDi).unwrap();
    client.stream_send(stream_id, &[0x00]).unwrap();
    let out = client.process(None, now());
    let _ = server.process(out.dgram(), now());

    let stream_readable = |e| matches!(e, ConnectionEvent::RecvStreamReadable {..});
    assert!(server.events().any(stream_readable));

    // Send one more packet from client. The packet should arrive after the server
    // has already requested stop_sending.
    client.stream_send(stream_id, &[0x00]).unwrap();
    let out_second_data_frame = client.process(None, now());
    // Call stop sending.
    assert_eq!(
        Ok(()),
        server.stream_stop_sending(stream_id, Error::NoError.code())
    );

    // Receive the second data frame. The frame should be ignored and now
    // DataReadable events should be posted.
    let out = server.process(out_second_data_frame.dgram(), now());
    assert!(!server.events().any(stream_readable));

    let _ = client.process(out.dgram(), now());
    assert_eq!(
        Err(Error::FinalSizeError),
        client.stream_send(stream_id, &[0x00])
    );
}

#[test]
// Server sends stop_sending, the client simultaneous sends reset.
fn simultaneous_stop_sending_and_reset() {
    // Note that the two servers in this test will get different anti-replay filters.
    // That's OK because we aren't testing anti-replay.
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    // create a stream
    let stream_id = client.stream_create(StreamType::BiDi).unwrap();
    client.stream_send(stream_id, &[0x00]).unwrap();
    let out = client.process(None, now());
    let _ = server.process(out.dgram(), now());

    let stream_readable = |e| matches!(e, ConnectionEvent::RecvStreamReadable {..});
    assert!(server.events().any(stream_readable));

    // The client resets the stream. The packet with reset should arrive after the server
    // has already requested stop_sending.
    client
        .stream_reset_send(stream_id, Error::NoError.code())
        .unwrap();
    let out_reset_frame = client.process(None, now());
    // Call stop sending.
    assert_eq!(
        Ok(()),
        server.stream_stop_sending(stream_id, Error::NoError.code())
    );

    // Receive the second data frame. The frame should be ignored and now
    // DataReadable events should be posted.
    let out = server.process(out_reset_frame.dgram(), now());
    assert!(!server.events().any(stream_readable));

    // The client gets the STOP_SENDING frame.
    let _ = client.process(out.dgram(), now());
    assert_eq!(
        Err(Error::InvalidStreamId),
        client.stream_send(stream_id, &[0x00])
    );
}

#[test]
fn client_fin_reorder() {
    let mut client = default_client();
    let mut server = default_server();

    // Send ClientHello.
    let client_hs = client.process(None, now());
    assert!(client_hs.as_dgram_ref().is_some());

    let server_hs = server.process(client_hs.dgram(), now());
    assert!(server_hs.as_dgram_ref().is_some()); // ServerHello, etc...

    let client_ack = client.process(server_hs.dgram(), now());
    assert!(client_ack.as_dgram_ref().is_some());

    let server_out = server.process(client_ack.dgram(), now());
    assert!(server_out.as_dgram_ref().is_none());

    assert!(maybe_authenticate(&mut client));
    assert_eq!(*client.state(), State::Connected);

    let client_fin = client.process(None, now());
    assert!(client_fin.as_dgram_ref().is_some());

    let client_stream_id = client.stream_create(StreamType::UniDi).unwrap();
    client.stream_send(client_stream_id, &[1, 2, 3]).unwrap();
    let client_stream_data = client.process(None, now());
    assert!(client_stream_data.as_dgram_ref().is_some());

    // Now stream data gets before client_fin
    let server_out = server.process(client_stream_data.dgram(), now());
    assert!(server_out.as_dgram_ref().is_none()); // the packet will be discarded

    assert_eq!(*server.state(), State::Handshaking);
    let server_out = server.process(client_fin.dgram(), now());
    assert!(server_out.as_dgram_ref().is_some());
}

#[test]
fn after_fin_is_read_conn_events_for_stream_should_be_removed() {
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    let id = server.stream_create(StreamType::BiDi).unwrap();
    server.stream_send(id, &[6; 10]).unwrap();
    server.stream_close_send(id).unwrap();
    let out = server.process(None, now()).dgram();
    assert!(out.is_some());

    let _ = client.process(out, now());

    // read from the stream before checking connection events.
    let mut buf = vec![0; 4000];
    let (_, fin) = client.stream_recv(id, &mut buf).unwrap();
    assert_eq!(fin, true);

    // Make sure we do not have RecvStreamReadable events for the stream when fin has been read.
    let readable_stream_evt =
        |e| matches!(e, ConnectionEvent::RecvStreamReadable { stream_id } if stream_id == id);
    assert!(!client.events().any(readable_stream_evt));
}

#[test]
fn after_stream_stop_sending_is_called_conn_events_for_stream_should_be_removed() {
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    let id = server.stream_create(StreamType::BiDi).unwrap();
    server.stream_send(id, &[6; 10]).unwrap();
    server.stream_close_send(id).unwrap();
    let out = server.process(None, now()).dgram();
    assert!(out.is_some());

    let _ = client.process(out, now());

    // send stop seending.
    client
        .stream_stop_sending(id, Error::NoError.code())
        .unwrap();

    // Make sure we do not have RecvStreamReadable events for the stream after stream_stop_sending
    // has been called.
    let readable_stream_evt =
        |e| matches!(e, ConnectionEvent::RecvStreamReadable { stream_id } if stream_id == id);
    assert!(!client.events().any(readable_stream_evt));
}

#[test]
fn stream_data_blocked_generates_max_stream_data() {
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    let now = now();

    // Try to say we're blocked beyond the initial data window
    server
        .flow_mgr
        .borrow_mut()
        .stream_data_blocked(3.into(), RECV_BUFFER_SIZE as u64 * 4);

    let out = server.process(None, now);
    assert!(out.as_dgram_ref().is_some());

    let frames = client.test_process_input(out.dgram().unwrap(), now);
    assert!(frames
        .iter()
        .any(|(f, _)| matches!(f, Frame::StreamDataBlocked { .. })));

    let out = client.process_output(now);
    assert!(out.as_dgram_ref().is_some());

    let frames = server.test_process_input(out.dgram().unwrap(), now);
    // Client should have sent a MaxStreamData frame with just the initial
    // window value.
    assert!(frames.iter().any(
        |(f, _)| matches!(f, Frame::MaxStreamData { maximum_stream_data, .. }
				   if *maximum_stream_data == RECV_BUFFER_SIZE as u64)
    ));
}
