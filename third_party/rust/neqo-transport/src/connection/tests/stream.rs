// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::super::State;
use super::{
    connect, connect_force_idle, default_client, default_server, maybe_authenticate, new_client,
    send_something, DEFAULT_STREAM_DATA,
};
use crate::events::ConnectionEvent;
use crate::recv_stream::RECV_BUFFER_SIZE;
use crate::send_stream::{SendStreamState, SEND_BUFFER_SIZE};
use crate::tparams::{self, TransportParameter};
use crate::tracking::MAX_UNACKED_PKTS;
use crate::ConnectionParameters;
use crate::{Error, StreamId, StreamType};

use neqo_common::{event::Provider, qdebug};
use std::cmp::max;
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
// tests stream send/recv after connection is established.
fn transfer() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    qdebug!("---- client sends");
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
    let mut out = client.process_output(now());
    while let Some(d) = out.dgram() {
        datagrams.push(d);
        out = client.process_output(now());
    }
    assert_eq!(datagrams.len(), 4);
    assert_eq!(*client.state(), State::Confirmed);

    qdebug!("---- server receives");
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
    let first_stream = stream_ids.next().expect("should have a new stream event");
    let second_stream = stream_ids
        .next()
        .expect("should have a second new stream event");
    assert!(stream_ids.next().is_none());
    let (received1, fin1) = server.stream_recv(first_stream.as_u64(), &mut buf).unwrap();
    assert_eq!(received1, 4000);
    assert_eq!(fin1, false);
    let (received2, fin2) = server.stream_recv(first_stream.as_u64(), &mut buf).unwrap();
    assert_eq!(received2, 140);
    assert_eq!(fin2, false);

    let (received3, fin3) = server
        .stream_recv(second_stream.as_u64(), &mut buf)
        .unwrap();
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

    server.stream_close_send(stream_id).unwrap();
    let out = server.process(None, now());
    let _ = client.process(out.dgram(), now());
    let stream_readable = |e| matches!(e, ConnectionEvent::RecvStreamReadable { .. });
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
        .streams
        .get_send_stream_mut(stream_id.into())
        .unwrap()
        .mark_as_sent(0, 4096, false);
    assert_eq!(client.events().count(), 0);
    client
        .streams
        .get_send_stream_mut(stream_id.into())
        .unwrap()
        .mark_as_acked(0, 4096, false);
    assert_eq!(client.events().count(), 0);

    assert_eq!(client.stream_send(stream_id, b"hello").unwrap(), 0);
    // no event because still limited by conn max data
    assert_eq!(client.events().count(), 0);

    // Increase max data. Avail space now limited by stream credit
    client.streams.handle_max_data(100_000_000);
    assert_eq!(
        client.stream_avail_send_space(stream_id).unwrap(),
        SEND_BUFFER_SIZE - SMALL_MAX_DATA
    );

    // Increase max stream data. Avail space now limited by tx buffer
    client
        .streams
        .get_send_stream_mut(stream_id.into())
        .unwrap()
        .set_max_stream_data(100_000_000);
    assert_eq!(
        client.stream_avail_send_space(stream_id).unwrap(),
        SEND_BUFFER_SIZE - SMALL_MAX_DATA + 4096
    );

    let evts = client.events().collect::<Vec<_>>();
    assert_eq!(evts.len(), 1);
    assert!(matches!(
        evts[0],
        ConnectionEvent::SendStreamWritable { .. }
    ));
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

    let stream_readable = |e| matches!(e, ConnectionEvent::RecvStreamReadable { .. });
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

    // Receive the second data frame. The frame should be ignored and
    // DataReadable events shouldn't be posted.
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
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    // create a stream
    let stream_id = client.stream_create(StreamType::BiDi).unwrap();
    client.stream_send(stream_id, &[0x00]).unwrap();
    let out = client.process(None, now());
    let ack = server.process(out.dgram(), now()).dgram();

    let stream_readable =
        |e| matches!(e, ConnectionEvent::RecvStreamReadable { stream_id: id } if id == stream_id);
    assert!(server.events().any(stream_readable));

    // The client resets the stream. The packet with reset should arrive after the server
    // has already requested stop_sending.
    client.stream_reset_send(stream_id, 0).unwrap();
    let out_reset_frame = client.process(ack, now()).dgram();

    // Send something out of order to force the server to generate an
    // acknowledgment at the next opportunity.
    let force_ack = send_something(&mut client, now());
    server.process_input(force_ack, now());

    // Call stop sending.
    server.stream_stop_sending(stream_id, 0).unwrap();
    // Receive the second data frame. The frame should be ignored and
    // DataReadable events shouldn't be posted.
    let ack = server.process(out_reset_frame, now()).dgram();
    assert!(ack.is_some());
    assert!(!server.events().any(stream_readable));

    // The client gets the STOP_SENDING frame.
    client.process_input(ack.unwrap(), now());
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

    // Send some data and consume some flow control.
    let stream_id = server.stream_create(StreamType::UniDi).unwrap();
    let _ = server.stream_send(stream_id, DEFAULT_STREAM_DATA).unwrap();
    let dgram = server.process(None, now).dgram();
    assert!(dgram.is_some());

    // Consume the data.
    client.process_input(dgram.unwrap(), now);
    let mut buf = [0; 10];
    let (count, end) = client.stream_recv(stream_id, &mut buf[..]).unwrap();
    assert_eq!(count, DEFAULT_STREAM_DATA.len());
    assert!(!end);

    // Now send `STREAM_DATA_BLOCKED`.
    let internal_stream = server
        .streams
        .get_send_stream_mut(StreamId::from(stream_id))
        .unwrap();
    if let SendStreamState::Send { fc, .. } = internal_stream.state() {
        fc.blocked();
    } else {
        panic!("unexpected stream state");
    }
    let dgram = server.process_output(now).dgram();
    assert!(dgram.is_some());

    let sdb_before = client.stats().frame_rx.stream_data_blocked;
    let dgram = client.process(dgram, now).dgram();
    assert_eq!(client.stats().frame_rx.stream_data_blocked, sdb_before + 1);
    assert!(dgram.is_some());

    // Client should have sent a MAX_STREAM_DATA frame with just a small increase
    // on the default window size.
    let msd_before = server.stats().frame_rx.max_stream_data;
    server.process_input(dgram.unwrap(), now);
    assert_eq!(server.stats().frame_rx.max_stream_data, msd_before + 1);

    // Test that the entirety of the receive buffer is available now.
    let mut written = 0;
    loop {
        const LARGE_BUFFER: &[u8] = &[0; 1024];
        let amount = server.stream_send(stream_id, LARGE_BUFFER).unwrap();
        if amount == 0 {
            break;
        }
        written += amount;
    }
    assert_eq!(written, RECV_BUFFER_SIZE);
}

/// See <https://github.com/mozilla/neqo/issues/871>
#[test]
fn max_streams_after_bidi_closed() {
    const REQUEST: &[u8] = b"ping";
    const RESPONSE: &[u8] = b"pong";
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    let stream_id = client.stream_create(StreamType::BiDi).unwrap();
    while client.stream_create(StreamType::BiDi).is_ok() {
        // Exhaust the stream limit.
    }
    // Write on the one stream and send that out.
    let _ = client.stream_send(stream_id, REQUEST).unwrap();
    client.stream_close_send(stream_id).unwrap();
    let dgram = client.process(None, now()).dgram();

    // Now handle the stream and send an incomplete response.
    server.process_input(dgram.unwrap(), now());
    server.stream_send(stream_id, RESPONSE).unwrap();
    let dgram = server.process_output(now()).dgram();

    // The server shouldn't have released more stream credit.
    client.process_input(dgram.unwrap(), now());
    let e = client.stream_create(StreamType::BiDi).unwrap_err();
    assert!(matches!(e, Error::StreamLimitError));

    // Closing the stream isn't enough.
    server.stream_close_send(stream_id).unwrap();
    let dgram = server.process_output(now()).dgram();
    client.process_input(dgram.unwrap(), now());
    assert!(client.stream_create(StreamType::BiDi).is_err());

    // The server needs to see an acknowledgment from the client for its
    // response AND the server has to read all of the request.
    // and the server needs to read all the data.  Read first.
    let mut buf = [0; REQUEST.len()];
    let (count, fin) = server.stream_recv(stream_id, &mut buf).unwrap();
    assert_eq!(&buf[..count], REQUEST);
    assert!(fin);

    // We need an ACK from the client now, but that isn't guaranteed,
    // so give the client one more packet just in case.
    let dgram = send_something(&mut server, now());
    client.process_input(dgram, now());

    // Now get the client to send the ACK and have the server handle that.
    let dgram = send_something(&mut client, now());
    let dgram = server.process(Some(dgram), now()).dgram();
    client.process_input(dgram.unwrap(), now());
    assert!(client.stream_create(StreamType::BiDi).is_ok());
    assert!(client.stream_create(StreamType::BiDi).is_err());
}

#[test]
fn no_dupdata_readable_events() {
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    // create a stream
    let stream_id = client.stream_create(StreamType::BiDi).unwrap();
    client.stream_send(stream_id, &[0x00]).unwrap();
    let out = client.process(None, now());
    let _ = server.process(out.dgram(), now());

    // We have a data_readable event.
    let stream_readable = |e| matches!(e, ConnectionEvent::RecvStreamReadable { .. });
    assert!(server.events().any(stream_readable));

    // Send one more data frame from client. The previous stream data has not been read yet,
    // therefore there should not be a new DataReadable event.
    client.stream_send(stream_id, &[0x00]).unwrap();
    let out_second_data_frame = client.process(None, now());
    let _ = server.process(out_second_data_frame.dgram(), now());
    assert!(!server.events().any(stream_readable));

    // One more frame with a fin will not produce a new DataReadable event, because the
    // previous stream data has not been read yet.
    client.stream_send(stream_id, &[0x00]).unwrap();
    client.stream_close_send(stream_id).unwrap();
    let out_third_data_frame = client.process(None, now());
    let _ = server.process(out_third_data_frame.dgram(), now());
    assert!(!server.events().any(stream_readable));
}

#[test]
fn no_dupdata_readable_events_empty_last_frame() {
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    // create a stream
    let stream_id = client.stream_create(StreamType::BiDi).unwrap();
    client.stream_send(stream_id, &[0x00]).unwrap();
    let out = client.process(None, now());
    let _ = server.process(out.dgram(), now());

    // We have a data_readable event.
    let stream_readable = |e| matches!(e, ConnectionEvent::RecvStreamReadable { .. });
    assert!(server.events().any(stream_readable));

    // An empty frame with a fin will not produce a new DataReadable event, because
    // the previous stream data has not been read yet.
    client.stream_close_send(stream_id).unwrap();
    let out_second_data_frame = client.process(None, now());
    let _ = server.process(out_second_data_frame.dgram(), now());
    assert!(!server.events().any(stream_readable));
}

fn change_flow_control(stream_type: StreamType, new_fc: u64) {
    const RECV_BUFFER_START: u64 = 300;

    let mut client = new_client(
        ConnectionParameters::default()
            .max_stream_data(StreamType::BiDi, true, RECV_BUFFER_START)
            .max_stream_data(StreamType::UniDi, true, RECV_BUFFER_START),
    );
    let mut server = default_server();
    connect(&mut client, &mut server);

    // create a stream
    let stream_id = server.stream_create(stream_type).unwrap();
    let written1 = server.stream_send(stream_id, &[0x0; 10000]).unwrap();
    assert_eq!(u64::try_from(written1).unwrap(), RECV_BUFFER_START);

    // Send the stream to the client.
    let out = server.process(None, now());
    let _ = client.process(out.dgram(), now());

    // change max_stream_data for stream_id.
    client.set_stream_max_data(stream_id, new_fc).unwrap();

    // server should receive a MAX_SREAM_DATA frame if the flow control window is updated.
    let out2 = client.process(None, now());
    let out3 = server.process(out2.dgram(), now());
    let expected = if RECV_BUFFER_START < new_fc { 1 } else { 0 };
    assert_eq!(server.stats().frame_rx.max_stream_data, expected);

    // If the flow control window has been increased, server can write more data.
    let written2 = server.stream_send(stream_id, &[0x0; 10000]).unwrap();
    if RECV_BUFFER_START < new_fc {
        assert_eq!(u64::try_from(written2).unwrap(), new_fc - RECV_BUFFER_START);
    } else {
        assert_eq!(written2, 0);
    }

    // Exchange packets so that client gets all data.
    let out4 = client.process(out3.dgram(), now());
    let out5 = server.process(out4.dgram(), now());
    let _ = client.process(out5.dgram(), now());

    // read all data by client
    let mut buf = [0x0; 10000];
    let (read, _) = client.stream_recv(stream_id, &mut buf).unwrap();
    assert_eq!(u64::try_from(read).unwrap(), max(RECV_BUFFER_START, new_fc));

    let out4 = client.process(None, now());
    let _ = server.process(out4.dgram(), now());

    let written3 = server.stream_send(stream_id, &[0x0; 10000]).unwrap();
    assert_eq!(u64::try_from(written3).unwrap(), new_fc);
}

#[test]
fn increase_decrease_flow_control() {
    const RECV_BUFFER_NEW_BIGGER: u64 = 400;
    const RECV_BUFFER_NEW_SMALLER: u64 = 200;

    change_flow_control(StreamType::UniDi, RECV_BUFFER_NEW_BIGGER);
    change_flow_control(StreamType::BiDi, RECV_BUFFER_NEW_BIGGER);

    change_flow_control(StreamType::UniDi, RECV_BUFFER_NEW_SMALLER);
    change_flow_control(StreamType::BiDi, RECV_BUFFER_NEW_SMALLER);
}
