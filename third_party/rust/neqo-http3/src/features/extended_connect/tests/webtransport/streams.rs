// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::features::extended_connect::tests::webtransport::WtTest;
use crate::{features::extended_connect::SessionCloseReason, Error};
use neqo_transport::StreamType;
use std::mem;

#[test]
fn wt_client_stream_uni() {
    const BUF_CLIENT: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let wt_session = wt.create_wt_session();
    let wt_stream = wt.create_wt_stream_client(wt_session.stream_id(), StreamType::UniDi);
    let send_stats = wt.send_stream_stats(wt_stream).unwrap();
    assert_eq!(send_stats.bytes_written(), 0);
    assert_eq!(send_stats.bytes_sent(), 0);
    assert_eq!(send_stats.bytes_acked(), 0);

    wt.send_data_client(wt_stream, BUF_CLIENT);
    wt.receive_data_server(wt_stream, true, BUF_CLIENT, false);
    let send_stats = wt.send_stream_stats(wt_stream).unwrap();
    assert_eq!(send_stats.bytes_written(), BUF_CLIENT.len() as u64);
    assert_eq!(send_stats.bytes_sent(), BUF_CLIENT.len() as u64);
    assert_eq!(send_stats.bytes_acked(), BUF_CLIENT.len() as u64);

    // Send data again to test if the stats has the expected values.
    wt.send_data_client(wt_stream, BUF_CLIENT);
    wt.receive_data_server(wt_stream, false, BUF_CLIENT, false);
    let send_stats = wt.send_stream_stats(wt_stream).unwrap();
    assert_eq!(send_stats.bytes_written(), (BUF_CLIENT.len() * 2) as u64);
    assert_eq!(send_stats.bytes_sent(), (BUF_CLIENT.len() * 2) as u64);
    assert_eq!(send_stats.bytes_acked(), (BUF_CLIENT.len() * 2) as u64);

    let recv_stats = wt.recv_stream_stats(wt_stream);
    assert_eq!(recv_stats.unwrap_err(), Error::InvalidStreamId);
}

#[test]
fn wt_client_stream_bidi() {
    const BUF_CLIENT: &[u8] = &[0; 10];
    const BUF_SERVER: &[u8] = &[1; 20];

    let mut wt = WtTest::new();
    let wt_session = wt.create_wt_session();
    let wt_client_stream = wt.create_wt_stream_client(wt_session.stream_id(), StreamType::BiDi);
    wt.send_data_client(wt_client_stream, BUF_CLIENT);
    let mut wt_server_stream = wt.receive_data_server(wt_client_stream, true, BUF_CLIENT, false);
    wt.send_data_server(&mut wt_server_stream, BUF_SERVER);
    wt.receive_data_client(wt_client_stream, false, BUF_SERVER, false);
    let send_stats = wt.send_stream_stats(wt_client_stream).unwrap();
    assert_eq!(send_stats.bytes_written(), BUF_CLIENT.len() as u64);
    assert_eq!(send_stats.bytes_sent(), BUF_CLIENT.len() as u64);
    assert_eq!(send_stats.bytes_acked(), BUF_CLIENT.len() as u64);

    let recv_stats = wt.recv_stream_stats(wt_client_stream).unwrap();
    assert_eq!(recv_stats.bytes_received(), BUF_SERVER.len() as u64);
    assert_eq!(recv_stats.bytes_read(), BUF_SERVER.len() as u64);
}

#[test]
fn wt_server_stream_uni() {
    const BUF_SERVER: &[u8] = &[2; 30];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();
    let mut wt_server_stream = WtTest::create_wt_stream_server(&mut wt_session, StreamType::UniDi);
    wt.send_data_server(&mut wt_server_stream, BUF_SERVER);
    wt.receive_data_client(wt_server_stream.stream_id(), true, BUF_SERVER, false);
    let send_stats = wt.send_stream_stats(wt_server_stream.stream_id());
    assert_eq!(send_stats.unwrap_err(), Error::InvalidStreamId);

    let recv_stats = wt.recv_stream_stats(wt_server_stream.stream_id()).unwrap();
    assert_eq!(recv_stats.bytes_received(), BUF_SERVER.len() as u64);
    assert_eq!(recv_stats.bytes_read(), BUF_SERVER.len() as u64);
}

#[test]
fn wt_server_stream_bidi() {
    const BUF_CLIENT: &[u8] = &[0; 10];
    const BUF_SERVER: &[u8] = &[1; 20];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();
    let mut wt_server_stream = WtTest::create_wt_stream_server(&mut wt_session, StreamType::BiDi);
    wt.send_data_server(&mut wt_server_stream, BUF_SERVER);
    wt.receive_data_client(wt_server_stream.stream_id(), true, BUF_SERVER, false);
    wt.send_data_client(wt_server_stream.stream_id(), BUF_CLIENT);
    mem::drop(wt.receive_data_server(wt_server_stream.stream_id(), false, BUF_CLIENT, false));
    let stats = wt.send_stream_stats(wt_server_stream.stream_id()).unwrap();
    assert_eq!(stats.bytes_written(), BUF_CLIENT.len() as u64);
    assert_eq!(stats.bytes_sent(), BUF_CLIENT.len() as u64);
    assert_eq!(stats.bytes_acked(), BUF_CLIENT.len() as u64);

    let recv_stats = wt.recv_stream_stats(wt_server_stream.stream_id()).unwrap();
    assert_eq!(recv_stats.bytes_received(), BUF_SERVER.len() as u64);
    assert_eq!(recv_stats.bytes_read(), BUF_SERVER.len() as u64);
}

#[test]
fn wt_client_stream_uni_close() {
    const BUF_CLIENT: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let wt_session = wt.create_wt_session();
    let wt_stream = wt.create_wt_stream_client(wt_session.stream_id(), StreamType::UniDi);
    wt.send_data_client(wt_stream, BUF_CLIENT);
    wt.close_stream_sending_client(wt_stream);
    wt.receive_data_server(wt_stream, true, BUF_CLIENT, true);
}

#[test]
fn wt_client_stream_bidi_close() {
    const BUF_CLIENT: &[u8] = &[0; 10];
    const BUF_SERVER: &[u8] = &[1; 20];

    let mut wt = WtTest::new();
    let wt_session = wt.create_wt_session();
    let wt_client_stream = wt.create_wt_stream_client(wt_session.stream_id(), StreamType::BiDi);

    wt.send_data_client(wt_client_stream, BUF_CLIENT);
    wt.close_stream_sending_client(wt_client_stream);

    let mut wt_server_stream = wt.receive_data_server(wt_client_stream, true, BUF_CLIENT, true);

    wt.send_data_server(&mut wt_server_stream, BUF_SERVER);
    wt.close_stream_sending_server(&mut wt_server_stream);
    wt.receive_data_client(wt_client_stream, false, BUF_SERVER, true);
}

#[test]
fn wt_server_stream_uni_closed() {
    const BUF_SERVER: &[u8] = &[2; 30];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();
    let mut wt_server_stream = WtTest::create_wt_stream_server(&mut wt_session, StreamType::UniDi);
    wt.send_data_server(&mut wt_server_stream, BUF_SERVER);
    wt.close_stream_sending_server(&mut wt_server_stream);
    wt.receive_data_client(wt_server_stream.stream_id(), true, BUF_SERVER, true);
}

#[test]
fn wt_server_stream_bidi_close() {
    const BUF_CLIENT: &[u8] = &[0; 10];
    const BUF_SERVER: &[u8] = &[1; 20];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();
    let mut wt_server_stream = WtTest::create_wt_stream_server(&mut wt_session, StreamType::BiDi);
    wt.send_data_server(&mut wt_server_stream, BUF_SERVER);
    wt.close_stream_sending_server(&mut wt_server_stream);
    wt.receive_data_client(wt_server_stream.stream_id(), true, BUF_SERVER, true);
    wt.send_data_client(wt_server_stream.stream_id(), BUF_CLIENT);
    wt.close_stream_sending_client(wt_server_stream.stream_id());
    mem::drop(wt.receive_data_server(wt_server_stream.stream_id(), false, BUF_CLIENT, true));
}

#[test]
fn wt_client_stream_uni_reset() {
    const BUF_CLIENT: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let wt_session = wt.create_wt_session();
    let wt_stream = wt.create_wt_stream_client(wt_session.stream_id(), StreamType::UniDi);
    wt.send_data_client(wt_stream, BUF_CLIENT);
    mem::drop(wt.receive_data_server(wt_stream, true, BUF_CLIENT, false));
    wt.reset_stream_client(wt_stream);
    wt.receive_reset_server(wt_stream, Error::HttpNoError.code());
}

#[test]
fn wt_server_stream_uni_reset() {
    const BUF_SERVER: &[u8] = &[2; 30];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();
    let mut wt_server_stream = WtTest::create_wt_stream_server(&mut wt_session, StreamType::UniDi);
    wt.send_data_server(&mut wt_server_stream, BUF_SERVER);
    wt.receive_data_client(wt_server_stream.stream_id(), true, BUF_SERVER, false);
    wt.reset_stream_server(&mut wt_server_stream);
    wt.receive_reset_client(wt_server_stream.stream_id());
}

#[test]
fn wt_client_stream_bidi_reset() {
    const BUF_CLIENT: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let wt_session = wt.create_wt_session();
    let wt_client_stream = wt.create_wt_stream_client(wt_session.stream_id(), StreamType::BiDi);

    wt.send_data_client(wt_client_stream, BUF_CLIENT);
    let mut wt_server_stream = wt.receive_data_server(wt_client_stream, true, BUF_CLIENT, false);

    wt.reset_stream_client(wt_client_stream);
    wt.receive_reset_server(wt_server_stream.stream_id(), Error::HttpNoError.code());

    wt.reset_stream_server(&mut wt_server_stream);
    wt.receive_reset_client(wt_client_stream);
}

#[test]
fn wt_server_stream_bidi_reset() {
    const BUF_SERVER: &[u8] = &[1; 20];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();
    let mut wt_server_stream = WtTest::create_wt_stream_server(&mut wt_session, StreamType::BiDi);
    wt.send_data_server(&mut wt_server_stream, BUF_SERVER);
    wt.receive_data_client(wt_server_stream.stream_id(), true, BUF_SERVER, false);

    wt.reset_stream_client(wt_server_stream.stream_id());
    wt.receive_reset_server(wt_server_stream.stream_id(), Error::HttpNoError.code());

    wt.reset_stream_server(&mut wt_server_stream);
    wt.receive_reset_client(wt_server_stream.stream_id());
}

#[test]
fn wt_client_stream_uni_stop_sending() {
    const BUF_CLIENT: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let wt_session = wt.create_wt_session();
    let wt_stream = wt.create_wt_stream_client(wt_session.stream_id(), StreamType::UniDi);
    wt.send_data_client(wt_stream, BUF_CLIENT);
    let mut wt_server_stream = wt.receive_data_server(wt_stream, true, BUF_CLIENT, false);
    wt.stream_stop_sending_server(&mut wt_server_stream);
    wt.receive_stop_sending_client(wt_stream);
}

#[test]
fn wt_server_stream_uni_stop_sending() {
    const BUF_SERVER: &[u8] = &[2; 30];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();
    let mut wt_server_stream = WtTest::create_wt_stream_server(&mut wt_session, StreamType::UniDi);
    wt.send_data_server(&mut wt_server_stream, BUF_SERVER);
    wt.receive_data_client(wt_server_stream.stream_id(), true, BUF_SERVER, false);
    wt.stream_stop_sending_client(wt_server_stream.stream_id());
    wt.receive_stop_sending_server(wt_server_stream.stream_id(), Error::HttpNoError.code());
}

#[test]
fn wt_client_stream_bidi_stop_sending() {
    const BUF_CLIENT: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let wt_session = wt.create_wt_session();

    let wt_client_stream = wt.create_wt_stream_client(wt_session.stream_id(), StreamType::BiDi);

    wt.send_data_client(wt_client_stream, BUF_CLIENT);

    let mut wt_server_stream = wt.receive_data_server(wt_client_stream, true, BUF_CLIENT, false);

    wt.stream_stop_sending_client(wt_client_stream);

    wt.receive_stop_sending_server(wt_server_stream.stream_id(), Error::HttpNoError.code());
    wt.stream_stop_sending_server(&mut wt_server_stream);
    wt.receive_stop_sending_client(wt_server_stream.stream_id());
}

#[test]
fn wt_server_stream_bidi_stop_sending() {
    const BUF_SERVER: &[u8] = &[1; 20];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();
    let mut wt_server_stream = WtTest::create_wt_stream_server(&mut wt_session, StreamType::BiDi);

    wt.send_data_server(&mut wt_server_stream, BUF_SERVER);
    wt.receive_data_client(wt_server_stream.stream_id(), true, BUF_SERVER, false);
    wt.stream_stop_sending_client(wt_server_stream.stream_id());
    wt.receive_stop_sending_server(wt_server_stream.stream_id(), Error::HttpNoError.code());
    wt.stream_stop_sending_server(&mut wt_server_stream);
    wt.receive_stop_sending_client(wt_server_stream.stream_id());
}

// For the following tests the client cancels a session. The streams are in different states:
//  1) Both sides of a bidirectional client stream are opened.
//  2) A client unidirectional stream is opened.
//  3) A client unidirectional stream has been closed and both sides consumed the closing info.
//  4) A client unidirectional stream has been closed, but only the server has consumed the closing info.
//  5) A client unidirectional stream has been closed, but only the client has consum the closing info.
//  6) Both sides of a bidirectional server stream are opened.
//  7) A server unidirectional stream is opened.
//  8) A server unidirectional stream has been closed and both sides consumed the closing info.
//  9) A server unidirectional stream has been closed, but only the server has consumed the closing info.
//  10) A server unidirectional stream has been closed, but only the client has consumed the closing info.
//  11) Both sides of a bidirectional stream have been closed and consumed by both sides.
//  12) Both sides of a bidirectional stream have been closed, but not consumed by both sides.
//  13) Multiples open streams

#[test]
fn wt_client_session_close_1() {
    const BUF: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let wt_session = wt.create_wt_session();

    let bidi_from_client = wt.create_wt_stream_client(wt_session.stream_id(), StreamType::BiDi);
    wt.send_data_client(bidi_from_client, BUF);
    std::mem::drop(wt.receive_data_server(bidi_from_client, true, BUF, false));

    wt.cancel_session_client(wt_session.stream_id());

    wt.check_events_after_closing_session_server(
        &[bidi_from_client],
        Some(Error::HttpRequestCancelled.code()),
        &[bidi_from_client],
        Some(Error::HttpRequestCancelled.code()),
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Error(Error::HttpNoError.code()),
        )),
    );

    wt.check_events_after_closing_session_client(
        &[bidi_from_client],
        Some(Error::HttpRequestCancelled.code()),
        &[bidi_from_client],
        Some(Error::HttpRequestCancelled.code()),
        false,
        &None,
    );
}

#[test]
fn wt_client_session_close_2() {
    const BUF: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let wt_session = wt.create_wt_session();

    let unidi_from_client = wt.create_wt_stream_client(wt_session.stream_id(), StreamType::UniDi);

    wt.send_data_client(unidi_from_client, BUF);
    std::mem::drop(wt.receive_data_server(unidi_from_client, true, BUF, false));

    wt.cancel_session_client(wt_session.stream_id());

    wt.check_events_after_closing_session_server(
        &[unidi_from_client],
        Some(Error::HttpRequestCancelled.code()),
        &[],
        None,
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Error(Error::HttpNoError.code()),
        )),
    );

    wt.check_events_after_closing_session_client(
        &[],
        None,
        &[unidi_from_client],
        Some(Error::HttpRequestCancelled.code()),
        false,
        &None,
    );
}

#[test]
fn wt_client_session_close_3() {
    const BUF: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let wt_session = wt.create_wt_session();

    let unidi_from_client = wt.create_wt_stream_client(wt_session.stream_id(), StreamType::UniDi);

    wt.send_data_client(unidi_from_client, BUF);
    std::mem::drop(wt.receive_data_server(unidi_from_client, true, BUF, false));
    wt.close_stream_sending_client(unidi_from_client);

    wt.cancel_session_client(wt_session.stream_id());

    wt.check_events_after_closing_session_server(
        &[],
        None,
        &[],
        None,
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Error(Error::HttpNoError.code()),
        )),
    );

    wt.check_events_after_closing_session_client(&[], None, &[], None, false, &None);
}

#[test]
fn wt_client_session_close_4() {
    const BUF: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let wt_session = wt.create_wt_session();

    let unidi_from_client = wt.create_wt_stream_client(wt_session.stream_id(), StreamType::UniDi);

    wt.send_data_client(unidi_from_client, BUF);
    let mut unidi_from_client_s = wt.receive_data_server(unidi_from_client, true, BUF, false);
    wt.stream_stop_sending_server(&mut unidi_from_client_s);

    wt.cancel_session_client(wt_session.stream_id());

    wt.check_events_after_closing_session_server(
        &[],
        None,
        &[],
        None,
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Error(Error::HttpNoError.code()),
        )),
    );

    wt.check_events_after_closing_session_client(
        &[],
        None,
        &[unidi_from_client],
        Some(Error::HttpNoError.code()),
        false,
        &None,
    );
}

#[test]
fn wt_client_session_close_5() {
    const BUF: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let wt_session = wt.create_wt_session();

    let unidi_from_client = wt.create_wt_stream_client(wt_session.stream_id(), StreamType::UniDi);

    wt.send_data_client(unidi_from_client, BUF);
    mem::drop(wt.receive_data_server(unidi_from_client, true, BUF, false));
    wt.reset_stream_client(unidi_from_client);

    wt.cancel_session_client(wt_session.stream_id());

    wt.check_events_after_closing_session_server(
        &[unidi_from_client],
        Some(Error::HttpNoError.code()),
        &[],
        None,
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Error(Error::HttpNoError.code()),
        )),
    );

    wt.check_events_after_closing_session_client(&[], None, &[], None, false, &None);
}

#[test]
fn wt_client_session_close_6() {
    const BUF: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    let mut bidi_from_server = WtTest::create_wt_stream_server(&mut wt_session, StreamType::BiDi);
    wt.send_data_server(&mut bidi_from_server, BUF);
    wt.receive_data_client(bidi_from_server.stream_id(), true, BUF, false);

    wt.cancel_session_client(wt_session.stream_id());

    wt.check_events_after_closing_session_server(
        &[bidi_from_server.stream_id()],
        Some(Error::HttpRequestCancelled.code()),
        &[bidi_from_server.stream_id()],
        Some(Error::HttpRequestCancelled.code()),
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Error(Error::HttpNoError.code()),
        )),
    );

    wt.check_events_after_closing_session_client(
        &[bidi_from_server.stream_id()],
        Some(Error::HttpRequestCancelled.code()),
        &[bidi_from_server.stream_id()],
        Some(Error::HttpRequestCancelled.code()),
        false,
        &None,
    );
}

#[test]
fn wt_client_session_close_7() {
    const BUF: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    let mut unidi_from_server = WtTest::create_wt_stream_server(&mut wt_session, StreamType::UniDi);
    wt.send_data_server(&mut unidi_from_server, BUF);
    wt.receive_data_client(unidi_from_server.stream_id(), true, BUF, false);

    wt.cancel_session_client(wt_session.stream_id());

    wt.check_events_after_closing_session_server(
        &[],
        None,
        &[unidi_from_server.stream_id()],
        Some(Error::HttpRequestCancelled.code()),
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Error(Error::HttpNoError.code()),
        )),
    );

    wt.check_events_after_closing_session_client(
        &[unidi_from_server.stream_id()],
        Some(Error::HttpRequestCancelled.code()),
        &[],
        None,
        false,
        &None,
    );
}

#[test]
fn wt_client_session_close_8() {
    const BUF: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    let mut unidi_server = WtTest::create_wt_stream_server(&mut wt_session, StreamType::UniDi);
    wt.send_data_server(&mut unidi_server, BUF);
    wt.close_stream_sending_server(&mut unidi_server);
    wt.receive_data_client(unidi_server.stream_id(), true, BUF, true);

    wt.cancel_session_client(wt_session.stream_id());

    wt.check_events_after_closing_session_server(
        &[],
        None,
        &[],
        None,
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Error(Error::HttpNoError.code()),
        )),
    );

    wt.check_events_after_closing_session_client(&[], None, &[], None, false, &None);
}

#[test]
fn wt_client_session_close_9() {
    const BUF: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    let mut unidi_server = WtTest::create_wt_stream_server(&mut wt_session, StreamType::UniDi);
    wt.send_data_server(&mut unidi_server, BUF);
    wt.stream_stop_sending_client(unidi_server.stream_id());

    wt.cancel_session_client(wt_session.stream_id());

    wt.check_events_after_closing_session_server(
        &[],
        None,
        &[unidi_server.stream_id()],
        Some(Error::HttpNoError.code()),
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Error(Error::HttpNoError.code()),
        )),
    );

    wt.check_events_after_closing_session_client(&[], None, &[], None, false, &None);
}

#[test]
fn wt_client_session_close_10() {
    const BUF: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    let mut unidi_server = WtTest::create_wt_stream_server(&mut wt_session, StreamType::UniDi);
    wt.send_data_server(&mut unidi_server, BUF);
    wt.close_stream_sending_server(&mut unidi_server);

    wt.cancel_session_client(wt_session.stream_id());

    wt.check_events_after_closing_session_server(
        &[],
        None,
        &[],
        None,
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Error(Error::HttpNoError.code()),
        )),
    );

    wt.check_events_after_closing_session_client(
        &[unidi_server.stream_id()],
        Some(Error::HttpRequestCancelled.code()),
        &[],
        None,
        false,
        &None,
    );
}

#[test]
fn wt_client_session_close_11() {
    const BUF: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    let mut bidi_server = WtTest::create_wt_stream_server(&mut wt_session, StreamType::BiDi);
    wt.send_data_server(&mut bidi_server, BUF);
    wt.close_stream_sending_server(&mut bidi_server);
    wt.receive_data_client(bidi_server.stream_id(), true, BUF, true);
    wt.stream_stop_sending_server(&mut bidi_server);
    wt.receive_stop_sending_client(bidi_server.stream_id());

    wt.cancel_session_client(wt_session.stream_id());

    wt.check_events_after_closing_session_server(
        &[],
        None,
        &[],
        None,
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Error(Error::HttpNoError.code()),
        )),
    );

    wt.check_events_after_closing_session_client(&[], None, &[], None, false, &None);
}

#[test]
fn wt_client_session_close_12() {
    const BUF: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    let mut bidi_server = WtTest::create_wt_stream_server(&mut wt_session, StreamType::BiDi);
    wt.send_data_server(&mut bidi_server, BUF);
    wt.close_stream_sending_server(&mut bidi_server);
    wt.stream_stop_sending_server(&mut bidi_server);

    wt.cancel_session_client(wt_session.stream_id());

    wt.check_events_after_closing_session_server(
        &[],
        None,
        &[],
        None,
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Error(Error::HttpNoError.code()),
        )),
    );

    wt.check_events_after_closing_session_client(
        &[bidi_server.stream_id()],
        Some(Error::HttpRequestCancelled.code()),
        &[bidi_server.stream_id()],
        Some(Error::HttpNoError.code()),
        false,
        &None,
    );
}

#[test]
fn wt_client_session_close_13() {
    const BUF: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let wt_session = wt.create_wt_session();

    let bidi_client_1 = wt.create_wt_stream_client(wt_session.stream_id(), StreamType::BiDi);
    wt.send_data_client(bidi_client_1, BUF);
    std::mem::drop(wt.receive_data_server(bidi_client_1, true, BUF, false));
    let bidi_client_2 = wt.create_wt_stream_client(wt_session.stream_id(), StreamType::BiDi);
    wt.send_data_client(bidi_client_2, BUF);
    std::mem::drop(wt.receive_data_server(bidi_client_2, true, BUF, false));

    wt.cancel_session_client(wt_session.stream_id());

    wt.check_events_after_closing_session_server(
        &[bidi_client_1, bidi_client_2],
        Some(Error::HttpRequestCancelled.code()),
        &[bidi_client_1, bidi_client_2],
        Some(Error::HttpRequestCancelled.code()),
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Error(Error::HttpNoError.code()),
        )),
    );

    wt.check_events_after_closing_session_client(
        &[bidi_client_1, bidi_client_2],
        Some(Error::HttpRequestCancelled.code()),
        &[bidi_client_1, bidi_client_2],
        Some(Error::HttpRequestCancelled.code()),
        false,
        &None,
    );
}

// For the following tests the server cancels a session. The streams are in different states:
//  1) Both sides of a bidirectional client stream are opened.
//  2) A client unidirectional stream is opened.
//  3) A client unidirectional stream has been closed and consumed by both sides.
//  4) A client unidirectional stream has been closed, but not consumed by the client.
//  5) Both sides of a bidirectional server stream are opened.
//  6) A server unidirectional stream is opened.
//  7) A server unidirectional stream has been closed and consumed by both sides.
//  8) A server unidirectional stream has been closed, but not consumed by the client.
//  9) Both sides of a bidirectional stream have been closed and consumed by both sides.
//  10) Both sides of a bidirectional stream have been closed, but not consumed by the client.
//  12) Multiples open streams

#[test]
fn wt_client_session_server_close_1() {
    const BUF: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    let bidi_client = wt.create_wt_stream_client(wt_session.stream_id(), StreamType::BiDi);
    wt.send_data_client(bidi_client, BUF);
    std::mem::drop(wt.receive_data_server(bidi_client, true, BUF, false));

    wt.cancel_session_server(&mut wt_session);

    wt.check_events_after_closing_session_client(
        &[bidi_client],
        Some(Error::HttpRequestCancelled.code()),
        &[bidi_client],
        Some(Error::HttpRequestCancelled.code()),
        false,
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Error(Error::HttpNoError.code()),
        )),
    );

    wt.check_events_after_closing_session_server(
        &[bidi_client],
        Some(Error::HttpRequestCancelled.code()),
        &[bidi_client],
        Some(Error::HttpRequestCancelled.code()),
        &None,
    );
}

#[test]
fn wt_client_session_server_close_2() {
    const BUF: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    let unidi_client = wt.create_wt_stream_client(wt_session.stream_id(), StreamType::UniDi);
    wt.send_data_client(unidi_client, BUF);
    std::mem::drop(wt.receive_data_server(unidi_client, true, BUF, false));

    wt.cancel_session_server(&mut wt_session);

    wt.check_events_after_closing_session_client(
        &[],
        None,
        &[unidi_client],
        Some(Error::HttpRequestCancelled.code()),
        false,
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Error(Error::HttpNoError.code()),
        )),
    );

    wt.check_events_after_closing_session_server(
        &[unidi_client],
        Some(Error::HttpRequestCancelled.code()),
        &[],
        None,
        &None,
    );
}

#[test]
fn wt_client_session_server_close_3() {
    const BUF: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    let unidi_client = wt.create_wt_stream_client(wt_session.stream_id(), StreamType::UniDi);
    wt.send_data_client(unidi_client, BUF);
    let mut unidi_client_s = wt.receive_data_server(unidi_client, true, BUF, false);
    wt.stream_stop_sending_server(&mut unidi_client_s);
    wt.receive_stop_sending_client(unidi_client);

    wt.cancel_session_server(&mut wt_session);

    wt.check_events_after_closing_session_client(
        &[],
        None,
        &[],
        None,
        false,
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Error(Error::HttpNoError.code()),
        )),
    );

    wt.check_events_after_closing_session_server(&[], None, &[], None, &None);
}

#[test]
fn wt_client_session_server_close_4() {
    const BUF: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    let unidi_client = wt.create_wt_stream_client(wt_session.stream_id(), StreamType::UniDi);
    wt.send_data_client(unidi_client, BUF);
    let mut unidi_client_s = wt.receive_data_server(unidi_client, true, BUF, false);
    wt.stream_stop_sending_server(&mut unidi_client_s);

    wt.cancel_session_server(&mut wt_session);

    wt.check_events_after_closing_session_client(
        &[],
        None,
        &[unidi_client],
        Some(Error::HttpNoError.code()),
        false,
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Error(Error::HttpNoError.code()),
        )),
    );

    wt.check_events_after_closing_session_server(&[], None, &[], None, &None);
}

#[test]
fn wt_client_session_server_close_5() {
    const BUF: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    let mut bidi_server = WtTest::create_wt_stream_server(&mut wt_session, StreamType::BiDi);
    wt.send_data_server(&mut bidi_server, BUF);
    wt.receive_data_client(bidi_server.stream_id(), true, BUF, false);

    wt.cancel_session_server(&mut wt_session);

    wt.check_events_after_closing_session_client(
        &[bidi_server.stream_id()],
        Some(Error::HttpRequestCancelled.code()),
        &[bidi_server.stream_id()],
        Some(Error::HttpRequestCancelled.code()),
        false,
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Error(Error::HttpNoError.code()),
        )),
    );

    wt.check_events_after_closing_session_server(
        &[bidi_server.stream_id()],
        Some(Error::HttpRequestCancelled.code()),
        &[bidi_server.stream_id()],
        Some(Error::HttpRequestCancelled.code()),
        &None,
    );
}

#[test]
fn wt_client_session_server_close_6() {
    const BUF: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    let mut unidi_server = WtTest::create_wt_stream_server(&mut wt_session, StreamType::UniDi);
    wt.send_data_server(&mut unidi_server, BUF);
    wt.receive_data_client(unidi_server.stream_id(), true, BUF, false);

    wt.cancel_session_server(&mut wt_session);

    wt.check_events_after_closing_session_client(
        &[unidi_server.stream_id()],
        Some(Error::HttpRequestCancelled.code()),
        &[],
        None,
        false,
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Error(Error::HttpNoError.code()),
        )),
    );
    wt.check_events_after_closing_session_server(
        &[],
        None,
        &[unidi_server.stream_id()],
        Some(Error::HttpRequestCancelled.code()),
        &None,
    );
}

#[test]
fn wt_client_session_server_close_7() {
    const BUF: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    let mut unidi_server = WtTest::create_wt_stream_server(&mut wt_session, StreamType::UniDi);
    wt.send_data_server(&mut unidi_server, BUF);
    wt.close_stream_sending_server(&mut unidi_server);
    wt.receive_data_client(unidi_server.stream_id(), true, BUF, true);

    wt.cancel_session_server(&mut wt_session);

    // Already close stream will not have a reset event.
    wt.check_events_after_closing_session_client(
        &[],
        None,
        &[],
        None,
        false,
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Error(Error::HttpNoError.code()),
        )),
    );

    wt.check_events_after_closing_session_server(&[], None, &[], None, &None);
}

#[test]
fn wt_client_session_server_close_8() {
    const BUF: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    let mut unidi_server = WtTest::create_wt_stream_server(&mut wt_session, StreamType::UniDi);
    wt.send_data_server(&mut unidi_server, BUF);
    wt.close_stream_sending_server(&mut unidi_server);

    wt.cancel_session_server(&mut wt_session);

    // The stream was only closed on the server side therefore it is cancelled on the client side.
    wt.check_events_after_closing_session_client(
        &[unidi_server.stream_id()],
        Some(Error::HttpRequestCancelled.code()),
        &[],
        None,
        false,
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Error(Error::HttpNoError.code()),
        )),
    );

    wt.check_events_after_closing_session_server(&[], None, &[], None, &None);
}

#[test]
fn wt_client_session_server_close_9() {
    const BUF: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    let mut bidi_server = WtTest::create_wt_stream_server(&mut wt_session, StreamType::BiDi);
    wt.send_data_server(&mut bidi_server, BUF);
    wt.close_stream_sending_server(&mut bidi_server);
    wt.receive_data_client(bidi_server.stream_id(), true, BUF, true);
    wt.stream_stop_sending_server(&mut bidi_server);
    wt.receive_stop_sending_client(bidi_server.stream_id());

    wt.cancel_session_server(&mut wt_session);

    // Already close stream will not have a reset event.
    wt.check_events_after_closing_session_client(
        &[],
        None,
        &[],
        None,
        false,
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Error(Error::HttpNoError.code()),
        )),
    );

    wt.check_events_after_closing_session_server(&[], None, &[], None, &None);
}

#[test]
fn wt_client_session_server_close_10() {
    const BUF: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    let mut bidi_server = WtTest::create_wt_stream_server(&mut wt_session, StreamType::BiDi);
    wt.send_data_server(&mut bidi_server, BUF);
    wt.close_stream_sending_server(&mut bidi_server);
    wt.stream_stop_sending_server(&mut bidi_server);

    wt.cancel_session_server(&mut wt_session);

    wt.check_events_after_closing_session_client(
        &[bidi_server.stream_id()],
        Some(Error::HttpRequestCancelled.code()),
        &[bidi_server.stream_id()],
        Some(Error::HttpNoError.code()),
        false,
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Error(Error::HttpNoError.code()),
        )),
    );

    wt.check_events_after_closing_session_server(&[], None, &[], None, &None);
}

#[test]
fn wt_client_session_server_close_11() {
    const BUF: &[u8] = &[0; 10];

    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    let bidi_client_1 = wt.create_wt_stream_client(wt_session.stream_id(), StreamType::BiDi);
    wt.send_data_client(bidi_client_1, BUF);
    std::mem::drop(wt.receive_data_server(bidi_client_1, true, BUF, false));
    let bidi_client_2 = wt.create_wt_stream_client(wt_session.stream_id(), StreamType::BiDi);
    wt.send_data_client(bidi_client_2, BUF);
    std::mem::drop(wt.receive_data_server(bidi_client_2, true, BUF, false));

    wt.cancel_session_server(&mut wt_session);

    wt.check_events_after_closing_session_client(
        &[bidi_client_1, bidi_client_2],
        Some(Error::HttpRequestCancelled.code()),
        &[bidi_client_1, bidi_client_2],
        Some(Error::HttpRequestCancelled.code()),
        false,
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Error(Error::HttpNoError.code()),
        )),
    );

    wt.check_events_after_closing_session_server(
        &[bidi_client_1, bidi_client_2],
        Some(Error::HttpRequestCancelled.code()),
        &[bidi_client_1, bidi_client_2],
        Some(Error::HttpRequestCancelled.code()),
        &None,
    );
}

#[test]
fn wt_session_close_frame_and_streams_client() {
    const BUF: &[u8] = &[0; 10];
    const ERROR_NUM: u32 = 23;
    const ERROR_MESSAGE: &str = "Something went wrong";
    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    let mut unidi_server = WtTest::create_wt_stream_server(&mut wt_session, StreamType::UniDi);
    wt.send_data_server(&mut unidi_server, BUF);
    wt.exchange_packets();

    wt.session_close_frame_client(wt_session.stream_id(), ERROR_NUM, ERROR_MESSAGE);
    wt.check_events_after_closing_session_client(
        &[unidi_server.stream_id()],
        Some(Error::HttpRequestCancelled.code()),
        &[],
        None,
        false,
        &None,
    );
    wt.exchange_packets();

    wt.check_events_after_closing_session_server(
        &[],
        None,
        &[unidi_server.stream_id()],
        Some(Error::HttpRequestCancelled.code()),
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Clean {
                error: ERROR_NUM,
                message: ERROR_MESSAGE.to_string(),
            },
        )),
    );
}
