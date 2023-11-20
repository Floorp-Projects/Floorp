// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::{
    super::State, assert_error, connect, connect_force_idle, default_client, default_server,
    maybe_authenticate, new_client, new_server, send_something, DEFAULT_STREAM_DATA,
};
use crate::{
    events::ConnectionEvent,
    recv_stream::RECV_BUFFER_SIZE,
    send_stream::{OrderGroup, SendStreamState, SEND_BUFFER_SIZE},
    streams::{SendOrder, StreamOrder},
    tparams::{self, TransportParameter},
    // tracking::DEFAULT_ACK_PACKET_TOLERANCE,
    Connection,
    ConnectionError,
    ConnectionParameters,
    Error,
    StreamId,
    StreamType,
};
use std::collections::HashMap;

use neqo_common::{event::Provider, qdebug};
use std::{cmp::max, convert::TryFrom, mem};
use test_fixture::now;

#[test]
fn stream_create() {
    let mut client = default_client();

    let out = client.process(None, now());
    let mut server = default_server();
    let out = server.process(out.dgram(), now());

    let out = client.process(out.dgram(), now());
    mem::drop(server.process(out.dgram(), now()));
    assert!(maybe_authenticate(&mut client));
    let out = client.process(None, now());

    // client now in State::Connected
    assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
    assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 6);
    assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);
    assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 4);

    mem::drop(server.process(out.dgram(), now()));
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
    for d in datagrams {
        let out = server.process(Some(d), now());
        // With an RTT of zero, the server will acknowledge every packet immediately.
        assert!(out.as_dgram_ref().is_some());
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
    let (received1, fin1) = server.stream_recv(first_stream, &mut buf).unwrap();
    assert_eq!(received1, 4000);
    assert!(!fin1);
    let (received2, fin2) = server.stream_recv(first_stream, &mut buf).unwrap();
    assert_eq!(received2, 140);
    assert!(!fin2);

    let (received3, fin3) = server.stream_recv(second_stream, &mut buf).unwrap();
    assert_eq!(received3, 60);
    assert!(fin3);
}

#[derive(PartialEq, Eq, PartialOrd, Ord)]
struct IdEntry {
    sendorder: StreamOrder,
    stream_id: StreamId,
}

// tests stream sendorder priorization
fn sendorder_test(order_of_sendorder: &[Option<SendOrder>]) {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    qdebug!("---- client sends");
    // open all streams and set the sendorders
    let mut ordered = Vec::new();
    let mut streams = Vec::<StreamId>::new();
    for sendorder in order_of_sendorder {
        let id = client.stream_create(StreamType::UniDi).unwrap();
        streams.push(id);
        ordered.push((id, *sendorder));
        // must be set before sendorder
        client.streams.set_fairness(id, true).ok();
        client.streams.set_sendorder(id, *sendorder).ok();
    }
    // Write some data to all the streams
    for stream_id in streams {
        client.stream_send(stream_id, &[6; 100]).unwrap();
    }

    // Sending this much takes a few datagrams.
    // Note: this test uses an RTT of 0 which simplifies things (no pacing)
    let mut datagrams = Vec::new();
    let mut out = client.process_output(now());
    while let Some(d) = out.dgram() {
        datagrams.push(d);
        out = client.process_output(now());
    }
    assert_eq!(*client.state(), State::Confirmed);

    qdebug!("---- server receives");
    for (_, d) in datagrams.into_iter().enumerate() {
        let out = server.process(Some(d), now());
        qdebug!("Output={:0x?}", out.as_dgram_ref());
    }
    assert_eq!(*server.state(), State::Confirmed);

    let stream_ids = server
        .events()
        .filter_map(|evt| match evt {
            ConnectionEvent::RecvStreamReadable { stream_id, .. } => Some(stream_id),
            _ => None,
        })
        .enumerate()
        .map(|(a, b)| (b, a))
        .collect::<HashMap<_, _>>();

    // streams should arrive in priority order, not order of creation, if sendorder prioritization
    // is working correctly

    // 'ordered' has the send order currently.  Re-sort it by sendorder, but
    // if two items from the same sendorder exist, secondarily sort by the ordering in
    // the stream_ids vector (HashMap<StreamId, index: usize>)
    ordered.sort_unstable_by_key(|(stream_id, sendorder)| {
        (
            StreamOrder {
                sendorder: *sendorder,
            },
            stream_ids[stream_id],
        )
    });
    // make sure everything now is in the same order, since we modified the order of
    // same-sendorder items to match the ordering of those we saw in reception
    for (i, (stream_id, _sendorder)) in ordered.iter().enumerate() {
        assert_eq!(i, stream_ids[stream_id]);
    }
}

#[test]
fn sendorder_0() {
    sendorder_test(&[None, Some(1), Some(2), Some(3)]);
}
#[test]
fn sendorder_1() {
    sendorder_test(&[Some(3), Some(2), Some(1), None]);
}
#[test]
fn sendorder_2() {
    sendorder_test(&[Some(3), None, Some(2), Some(1)]);
}
#[test]
fn sendorder_3() {
    sendorder_test(&[Some(1), Some(2), None, Some(3)]);
}
#[test]
fn sendorder_4() {
    sendorder_test(&[
        Some(1),
        Some(2),
        Some(1),
        None,
        Some(3),
        Some(1),
        Some(3),
        None,
    ]);
}

// Tests stream sendorder priorization
// Converts Vecs of u64's into StreamIds
fn fairness_test<S, R>(source: S, number_iterates: usize, truncate_to: usize, result_array: &R)
where
    S: IntoIterator,
    S::Item: Into<StreamId>,
    R: IntoIterator + std::fmt::Debug,
    R::Item: Into<StreamId>,
    Vec<u64>: PartialEq<R>,
{
    // test the OrderGroup code used for fairness
    let mut group: OrderGroup = OrderGroup::default();
    for stream_id in source {
        group.insert(stream_id.into());
    }
    {
        let mut iterator1 = group.iter();
        // advance_by() would help here
        let mut n = number_iterates;
        while n > 0 {
            iterator1.next();
            n -= 1;
        }
        // let iterator1 go out of scope
    }
    group.truncate(truncate_to);

    let iterator2 = group.iter();
    let result: Vec<u64> = iterator2.map(StreamId::as_u64).collect();
    assert_eq!(result, *result_array);
}

#[test]
fn ordergroup_0() {
    let source: [u64; 0] = [];
    let result: [u64; 0] = [];
    fairness_test(source, 1, usize::MAX, &result);
}

#[test]
fn ordergroup_1() {
    let source: [u64; 6] = [0, 1, 2, 3, 4, 5];
    let result: [u64; 6] = [1, 2, 3, 4, 5, 0];
    fairness_test(source, 1, usize::MAX, &result);
}

#[test]
fn ordergroup_2() {
    let source: [u64; 6] = [0, 1, 2, 3, 4, 5];
    let result: [u64; 6] = [2, 3, 4, 5, 0, 1];
    fairness_test(source, 2, usize::MAX, &result);
}

#[test]
fn ordergroup_3() {
    let source: [u64; 6] = [0, 1, 2, 3, 4, 5];
    let result: [u64; 6] = [0, 1, 2, 3, 4, 5];
    fairness_test(source, 10, usize::MAX, &result);
}

#[test]
fn ordergroup_4() {
    let source: [u64; 6] = [0, 1, 2, 3, 4, 5];
    let result: [u64; 6] = [0, 1, 2, 3, 4, 5];
    fairness_test(source, 0, usize::MAX, &result);
}

#[test]
fn ordergroup_5() {
    let source: [u64; 1] = [0];
    let result: [u64; 1] = [0];
    fairness_test(source, 1, usize::MAX, &result);
}

#[test]
fn ordergroup_6() {
    let source: [u64; 6] = [0, 1, 2, 3, 4, 5];
    let result: [u64; 6] = [5, 0, 1, 2, 3, 4];
    fairness_test(source, 5, usize::MAX, &result);
}

#[test]
fn ordergroup_7() {
    let source: [u64; 6] = [0, 1, 2, 3, 4, 5];
    let result: [u64; 3] = [0, 1, 2];
    fairness_test(source, 5, 3, &result);
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
    mem::drop(server.process(out.dgram(), now()));

    server.stream_close_send(stream_id).unwrap();
    let out = server.process(None, now());
    mem::drop(client.process(out.dgram(), now()));
    let stream_readable = |e| matches!(e, ConnectionEvent::RecvStreamReadable { .. });
    assert!(client.events().any(stream_readable));
}

fn exchange_data(client: &mut Connection, server: &mut Connection) {
    let mut input = None;
    loop {
        let out = client.process(input, now()).dgram();
        let c_done = out.is_none();
        let out = server.process(out, now()).dgram();
        if out.is_none() && c_done {
            break;
        }
        input = out;
    }
}

#[test]
fn sending_max_data() {
    const SMALL_MAX_DATA: usize = 2048;

    let mut client = default_client();
    let mut server = new_server(
        ConnectionParameters::default().max_data(u64::try_from(SMALL_MAX_DATA).unwrap()),
    );

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
            .stream_send(stream_id, &[b'a'; SMALL_MAX_DATA + 1])
            .unwrap(),
        SMALL_MAX_DATA
    );

    exchange_data(&mut client, &mut server);

    let mut buf = vec![0; 40000];
    let (received, fin) = server.stream_recv(stream_id, &mut buf).unwrap();
    assert_eq!(received, SMALL_MAX_DATA);
    assert!(!fin);

    let out = server.process(None, now()).dgram();
    client.process_input(out.unwrap(), now());

    assert_eq!(
        client
            .stream_send(stream_id, &[b'a'; SMALL_MAX_DATA + 1])
            .unwrap(),
        SMALL_MAX_DATA
    );
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
            .stream_send(stream_id, &[b'a'; SMALL_MAX_DATA + 1])
            .unwrap(),
        SMALL_MAX_DATA
    );
    assert_eq!(client.events().count(), 0);

    assert_eq!(client.stream_send(stream_id, b"hello").unwrap(), 0);
    client
        .streams
        .get_send_stream_mut(stream_id)
        .unwrap()
        .mark_as_sent(0, 4096, false);
    assert_eq!(client.events().count(), 0);
    client
        .streams
        .get_send_stream_mut(stream_id)
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
        .get_send_stream_mut(stream_id)
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
fn exceed_max_data() {
    const SMALL_MAX_DATA: usize = 1024;

    let mut client = default_client();
    let mut server = new_server(
        ConnectionParameters::default().max_data(u64::try_from(SMALL_MAX_DATA).unwrap()),
    );

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
            .stream_send(stream_id, &[b'a'; SMALL_MAX_DATA + 1])
            .unwrap(),
        SMALL_MAX_DATA
    );

    assert_eq!(client.stream_send(stream_id, b"hello").unwrap(), 0);

    // Artificially trick the client to think that it has more flow control credit.
    client.streams.handle_max_data(100_000_000);
    assert_eq!(client.stream_send(stream_id, b"h").unwrap(), 1);

    exchange_data(&mut client, &mut server);

    assert_error(
        &client,
        &ConnectionError::Transport(Error::PeerError(Error::FlowControlError.code())),
    );
    assert_error(
        &server,
        &ConnectionError::Transport(Error::FlowControlError),
    );
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
    mem::drop(server.process(out.dgram(), now()));

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

    mem::drop(client.process(out.dgram(), now()));
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

    mem::drop(client.process(out, now()));

    // read from the stream before checking connection events.
    let mut buf = vec![0; 4000];
    let (_, fin) = client.stream_recv(id, &mut buf).unwrap();
    assert!(fin);

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

    mem::drop(client.process(out, now()));

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
    _ = server.stream_send(stream_id, DEFAULT_STREAM_DATA).unwrap();
    let dgram = server.process(None, now).dgram();
    assert!(dgram.is_some());

    // Consume the data.
    client.process_input(dgram.unwrap(), now);
    let mut buf = [0; 10];
    let (count, end) = client.stream_recv(stream_id, &mut buf[..]).unwrap();
    assert_eq!(count, DEFAULT_STREAM_DATA.len());
    assert!(!end);

    // Now send `STREAM_DATA_BLOCKED`.
    let internal_stream = server.streams.get_send_stream_mut(stream_id).unwrap();
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
    _ = client.stream_send(stream_id, REQUEST).unwrap();
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
    mem::drop(server.process(out.dgram(), now()));

    // We have a data_readable event.
    let stream_readable = |e| matches!(e, ConnectionEvent::RecvStreamReadable { .. });
    assert!(server.events().any(stream_readable));

    // Send one more data frame from client. The previous stream data has not been read yet,
    // therefore there should not be a new DataReadable event.
    client.stream_send(stream_id, &[0x00]).unwrap();
    let out_second_data_frame = client.process(None, now());
    mem::drop(server.process(out_second_data_frame.dgram(), now()));
    assert!(!server.events().any(stream_readable));

    // One more frame with a fin will not produce a new DataReadable event, because the
    // previous stream data has not been read yet.
    client.stream_send(stream_id, &[0x00]).unwrap();
    client.stream_close_send(stream_id).unwrap();
    let out_third_data_frame = client.process(None, now());
    mem::drop(server.process(out_third_data_frame.dgram(), now()));
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
    mem::drop(server.process(out.dgram(), now()));

    // We have a data_readable event.
    let stream_readable = |e| matches!(e, ConnectionEvent::RecvStreamReadable { .. });
    assert!(server.events().any(stream_readable));

    // An empty frame with a fin will not produce a new DataReadable event, because
    // the previous stream data has not been read yet.
    client.stream_close_send(stream_id).unwrap();
    let out_second_data_frame = client.process(None, now());
    mem::drop(server.process(out_second_data_frame.dgram(), now()));
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
    mem::drop(client.process(out.dgram(), now()));

    // change max_stream_data for stream_id.
    client.set_stream_max_data(stream_id, new_fc).unwrap();

    // server should receive a MAX_SREAM_DATA frame if the flow control window is updated.
    let out2 = client.process(None, now());
    let out3 = server.process(out2.dgram(), now());
    let expected = usize::from(RECV_BUFFER_START < new_fc);
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
    mem::drop(client.process(out5.dgram(), now()));

    // read all data by client
    let mut buf = [0x0; 10000];
    let (read, _) = client.stream_recv(stream_id, &mut buf).unwrap();
    assert_eq!(u64::try_from(read).unwrap(), max(RECV_BUFFER_START, new_fc));

    let out4 = client.process(None, now());
    mem::drop(server.process(out4.dgram(), now()));

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

#[test]
fn session_flow_control_stop_sending_state_recv() {
    const SMALL_MAX_DATA: usize = 1024;

    let mut client = default_client();
    let mut server = new_server(
        ConnectionParameters::default().max_data(u64::try_from(SMALL_MAX_DATA).unwrap()),
    );

    connect(&mut client, &mut server);

    let stream_id = client.stream_create(StreamType::UniDi).unwrap();
    assert_eq!(
        client.stream_avail_send_space(stream_id).unwrap(),
        SMALL_MAX_DATA
    );

    // send 1 byte so that the server learns about the stream.
    assert_eq!(client.stream_send(stream_id, b"a").unwrap(), 1);

    exchange_data(&mut client, &mut server);

    server
        .stream_stop_sending(stream_id, Error::NoError.code())
        .unwrap();

    assert_eq!(
        client
            .stream_send(stream_id, &[b'a'; SMALL_MAX_DATA])
            .unwrap(),
        SMALL_MAX_DATA - 1
    );

    // In this case the final size is only known after RESET frame is received.
    // The server sends STOP_SENDING -> the client sends RESET -> the server
    // sends MAX_DATA.
    let out = server.process(None, now()).dgram();
    let out = client.process(out, now()).dgram();
    // the client is still limited.
    let stream_id2 = client.stream_create(StreamType::UniDi).unwrap();
    assert_eq!(client.stream_avail_send_space(stream_id2).unwrap(), 0);
    let out = server.process(out, now()).dgram();
    client.process_input(out.unwrap(), now());
    assert_eq!(
        client.stream_avail_send_space(stream_id2).unwrap(),
        SMALL_MAX_DATA
    );
}

#[test]
fn session_flow_control_stop_sending_state_size_known() {
    const SMALL_MAX_DATA: usize = 1024;

    let mut client = default_client();
    let mut server = new_server(
        ConnectionParameters::default().max_data(u64::try_from(SMALL_MAX_DATA).unwrap()),
    );

    connect(&mut client, &mut server);

    let stream_id = client.stream_create(StreamType::UniDi).unwrap();
    assert_eq!(
        client.stream_avail_send_space(stream_id).unwrap(),
        SMALL_MAX_DATA
    );

    // send 1 byte so that the server learns about the stream.
    assert_eq!(
        client
            .stream_send(stream_id, &[b'a'; SMALL_MAX_DATA + 1])
            .unwrap(),
        SMALL_MAX_DATA
    );

    let out1 = client.process(None, now()).dgram();
    // Delay this packet and let the server receive fin first (it will enter SizeKnown state).
    client.stream_close_send(stream_id).unwrap();
    let out2 = client.process(None, now()).dgram();

    server.process_input(out2.unwrap(), now());

    server
        .stream_stop_sending(stream_id, Error::NoError.code())
        .unwrap();

    // In this case the final size is known when stream_stop_sending is called
    // and the server releases flow control immediately and sends STOP_SENDING and
    // MAX_DATA in the same packet.
    let out = server.process(out1, now()).dgram();
    client.process_input(out.unwrap(), now());

    // The flow control should have been updated and the client can again send
    // SMALL_MAX_DATA.
    let stream_id2 = client.stream_create(StreamType::UniDi).unwrap();
    assert_eq!(
        client.stream_avail_send_space(stream_id2).unwrap(),
        SMALL_MAX_DATA
    );
}

#[test]
fn session_flow_control_stop_sending_state_data_recvd() {
    const SMALL_MAX_DATA: usize = 1024;

    let mut client = default_client();
    let mut server = new_server(
        ConnectionParameters::default().max_data(u64::try_from(SMALL_MAX_DATA).unwrap()),
    );

    connect(&mut client, &mut server);

    let stream_id = client.stream_create(StreamType::UniDi).unwrap();
    assert_eq!(
        client.stream_avail_send_space(stream_id).unwrap(),
        SMALL_MAX_DATA
    );

    // send 1 byte so that the server learns about the stream.
    assert_eq!(
        client
            .stream_send(stream_id, &[b'a'; SMALL_MAX_DATA + 1])
            .unwrap(),
        SMALL_MAX_DATA
    );

    client.stream_close_send(stream_id).unwrap();

    exchange_data(&mut client, &mut server);

    // The stream is DataRecvd state
    server
        .stream_stop_sending(stream_id, Error::NoError.code())
        .unwrap();

    exchange_data(&mut client, &mut server);

    // The flow control should have been updated and the client can again send
    // SMALL_MAX_DATA.
    let stream_id2 = client.stream_create(StreamType::UniDi).unwrap();
    assert_eq!(
        client.stream_avail_send_space(stream_id2).unwrap(),
        SMALL_MAX_DATA
    );
}

#[test]
fn session_flow_control_affects_all_streams() {
    const SMALL_MAX_DATA: usize = 1024;

    let mut client = default_client();
    let mut server = new_server(
        ConnectionParameters::default().max_data(u64::try_from(SMALL_MAX_DATA).unwrap()),
    );

    connect(&mut client, &mut server);

    let stream_id = client.stream_create(StreamType::UniDi).unwrap();
    assert_eq!(
        client.stream_avail_send_space(stream_id).unwrap(),
        SMALL_MAX_DATA
    );

    let stream_id2 = client.stream_create(StreamType::UniDi).unwrap();
    assert_eq!(
        client.stream_avail_send_space(stream_id2).unwrap(),
        SMALL_MAX_DATA
    );

    assert_eq!(
        client
            .stream_send(stream_id, &[b'a'; SMALL_MAX_DATA / 2 + 1])
            .unwrap(),
        SMALL_MAX_DATA / 2 + 1
    );

    assert_eq!(
        client.stream_avail_send_space(stream_id).unwrap(),
        SMALL_MAX_DATA / 2 - 1
    );
    assert_eq!(
        client.stream_avail_send_space(stream_id2).unwrap(),
        SMALL_MAX_DATA / 2 - 1
    );

    exchange_data(&mut client, &mut server);

    let mut buf = [0x0; SMALL_MAX_DATA];
    let (read, _) = server.stream_recv(stream_id, &mut buf).unwrap();
    assert_eq!(read, SMALL_MAX_DATA / 2 + 1);

    exchange_data(&mut client, &mut server);

    assert_eq!(
        client.stream_avail_send_space(stream_id).unwrap(),
        SMALL_MAX_DATA
    );

    assert_eq!(
        client.stream_avail_send_space(stream_id2).unwrap(),
        SMALL_MAX_DATA
    );
}

fn connect_w_different_limit(bidi_limit: u64, unidi_limit: u64) {
    let mut client = default_client();
    let out = client.process(None, now());
    let mut server = new_server(
        ConnectionParameters::default()
            .max_streams(StreamType::BiDi, bidi_limit)
            .max_streams(StreamType::UniDi, unidi_limit),
    );
    let out = server.process(out.dgram(), now());

    let out = client.process(out.dgram(), now());
    mem::drop(server.process(out.dgram(), now()));

    assert!(maybe_authenticate(&mut client));

    let mut bidi_events = 0;
    let mut unidi_events = 0;
    let mut connected_events = 0;
    for e in client.events() {
        match e {
            ConnectionEvent::SendStreamCreatable { stream_type } => {
                if stream_type == StreamType::BiDi {
                    bidi_events += 1;
                } else {
                    unidi_events += 1;
                }
            }
            ConnectionEvent::StateChange(State::Connected) => {
                connected_events += 1;
            }
            _ => {}
        }
    }
    assert_eq!(bidi_events, usize::from(bidi_limit > 0));
    assert_eq!(unidi_events, usize::from(unidi_limit > 0));
    assert_eq!(connected_events, 1);
}

#[test]
fn client_stream_creatable_event() {
    connect_w_different_limit(0, 0);
    connect_w_different_limit(0, 1);
    connect_w_different_limit(1, 0);
    connect_w_different_limit(1, 1);
}
