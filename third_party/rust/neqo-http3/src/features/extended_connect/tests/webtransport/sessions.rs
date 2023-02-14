// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::features::extended_connect::tests::webtransport::{
    default_http3_client, default_http3_server, wt_default_parameters, WtTest,
};
use crate::{
    features::extended_connect::SessionCloseReason, frames::WebTransportFrame, Error, Header,
    Http3ClientEvent, Http3OrWebTransportStream, Http3Server, Http3ServerEvent, Http3State,
    Priority, WebTransportEvent, WebTransportServerEvent, WebTransportSessionAcceptAction,
};
use neqo_common::{event::Provider, Encoder};
use neqo_transport::StreamType;
use std::mem;
use test_fixture::now;

#[test]
fn wt_session() {
    let mut wt = WtTest::new();
    mem::drop(wt.create_wt_session());
}

#[test]
fn wt_session_reject() {
    let mut wt = WtTest::new();
    let headers = vec![Header::new(":status", "404")];
    let accept_res = WebTransportSessionAcceptAction::Reject(headers.clone());
    let (wt_session_id, _wt_session) = wt.negotiate_wt_session(&accept_res);

    wt.check_session_closed_event_client(
        wt_session_id,
        &SessionCloseReason::Status(404),
        &Some(headers),
    );
}

#[test]
fn wt_session_close_client() {
    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    wt.cancel_session_client(wt_session.stream_id());
    wt.check_session_closed_event_server(
        &mut wt_session,
        &SessionCloseReason::Error(Error::HttpNoError.code()),
    );
}

#[test]
fn wt_session_close_server() {
    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    wt.cancel_session_server(&mut wt_session);
    wt.check_session_closed_event_client(
        wt_session.stream_id(),
        &SessionCloseReason::Error(Error::HttpNoError.code()),
        &None,
    );
}

#[test]
fn wt_session_close_server_close_send() {
    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    wt_session.stream_close_send().unwrap();
    wt.exchange_packets();
    wt.check_session_closed_event_client(
        wt_session.stream_id(),
        &SessionCloseReason::Clean {
            error: 0,
            message: String::new(),
        },
        &None,
    );
}

#[test]
fn wt_session_close_server_stop_sending() {
    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    wt_session
        .stream_stop_sending(Error::HttpNoError.code())
        .unwrap();
    wt.exchange_packets();
    wt.check_session_closed_event_client(
        wt_session.stream_id(),
        &SessionCloseReason::Error(Error::HttpNoError.code()),
        &None,
    );
}

#[test]
fn wt_session_close_server_reset() {
    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    wt_session
        .stream_reset_send(Error::HttpNoError.code())
        .unwrap();
    wt.exchange_packets();
    wt.check_session_closed_event_client(
        wt_session.stream_id(),
        &SessionCloseReason::Error(Error::HttpNoError.code()),
        &None,
    );
}

#[test]
fn wt_session_response_with_1xx() {
    let mut wt = WtTest::new();

    let wt_session_id = wt
        .client
        .webtransport_create_session(now(), &("https", "something.com", "/"), &[])
        .unwrap();
    wt.exchange_packets();

    let mut wt_server_session = None;
    while let Some(event) = wt.server.next_event() {
        if let Http3ServerEvent::WebTransport(WebTransportServerEvent::NewSession {
            session,
            headers,
        }) = event
        {
            assert!(
                headers
                    .iter()
                    .any(|h| h.name() == ":method" && h.value() == "CONNECT")
                    && headers
                        .iter()
                        .any(|h| h.name() == ":protocol" && h.value() == "webtransport")
            );
            wt_server_session = Some(session);
        }
    }

    let mut wt_server_session = wt_server_session.unwrap();

    // Send interim response.
    wt_server_session
        .send_headers(&[Header::new(":status", "111")])
        .unwrap();
    wt_server_session
        .response(&WebTransportSessionAcceptAction::Accept)
        .unwrap();

    wt.exchange_packets();

    let wt_session_negotiated_event = |e| {
        matches!(
            e,
            Http3ClientEvent::WebTransport(WebTransportEvent::Session{
                stream_id,
                status,
                headers,
            }) if (
                stream_id == wt_session_id &&
                status == 200 &&
                headers.contains(&Header::new(":status", "200"))
            )
        )
    };
    assert!(wt.client.events().any(wt_session_negotiated_event));

    assert_eq!(wt_session_id, wt_server_session.stream_id());
}

#[test]
fn wt_session_response_with_redirect() {
    let headers = [Header::new(":status", "302"), Header::new("location", "/")].to_vec();
    let mut wt = WtTest::new();

    let accept_res = WebTransportSessionAcceptAction::Reject(headers.clone());

    let (wt_session_id, _wt_session) = wt.negotiate_wt_session(&accept_res);

    wt.check_session_closed_event_client(
        wt_session_id,
        &SessionCloseReason::Status(302),
        &Some(headers),
    );
}

#[test]
fn wt_session_respone_200_with_fin() {
    let mut wt = WtTest::new();

    let wt_session_id = wt
        .client
        .webtransport_create_session(now(), &("https", "something.com", "/"), &[])
        .unwrap();
    wt.exchange_packets();
    let mut wt_server_session = None;
    while let Some(event) = wt.server.next_event() {
        if let Http3ServerEvent::WebTransport(WebTransportServerEvent::NewSession {
            session,
            headers,
        }) = event
        {
            assert!(
                headers
                    .iter()
                    .any(|h| h.name() == ":method" && h.value() == "CONNECT")
                    && headers
                        .iter()
                        .any(|h| h.name() == ":protocol" && h.value() == "webtransport")
            );
            wt_server_session = Some(session);
        }
    }

    let mut wt_server_session = wt_server_session.unwrap();
    wt_server_session
        .response(&WebTransportSessionAcceptAction::Accept)
        .unwrap();
    wt_server_session.stream_close_send().unwrap();

    wt.exchange_packets();

    let wt_session_close_event = |e| {
        matches!(
            e,
            Http3ClientEvent::WebTransport(WebTransportEvent::SessionClosed{
                stream_id,
                reason,
                headers,
                ..
            }) if (
                stream_id == wt_session_id &&
                reason == SessionCloseReason::Clean{ error: 0, message: String::new()} &&
                headers.is_none()
            )
        )
    };
    assert!(wt.client.events().any(wt_session_close_event));

    assert_eq!(wt_session_id, wt_server_session.stream_id());
}

#[test]
fn wt_session_close_frame_client() {
    const ERROR_NUM: u32 = 23;
    const ERROR_MESSAGE: &str = "Something went wrong";
    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    wt.session_close_frame_client(wt_session.stream_id(), ERROR_NUM, ERROR_MESSAGE);
    wt.exchange_packets();

    wt.check_session_closed_event_server(
        &mut wt_session,
        &SessionCloseReason::Clean {
            error: ERROR_NUM,
            message: ERROR_MESSAGE.to_string(),
        },
    );
}

#[test]
fn wt_session_close_frame_server() {
    const ERROR_NUM: u32 = 23;
    const ERROR_MESSAGE: &str = "Something went wrong";
    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    WtTest::session_close_frame_server(&mut wt_session, ERROR_NUM, ERROR_MESSAGE);
    wt.exchange_packets();

    wt.check_session_closed_event_client(
        wt_session.stream_id(),
        &SessionCloseReason::Clean {
            error: ERROR_NUM,
            message: ERROR_MESSAGE.to_string(),
        },
        &None,
    );
}

#[test]
fn wt_unknown_session_frame_client() {
    const UNKNOWN_FRAME_LEN: usize = 832;
    const BUF: &[u8] = &[0; 10];
    const ERROR_NUM: u32 = 23;
    const ERROR_MESSAGE: &str = "Something went wrong";
    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    // Send an unknown frame.
    let mut enc = Encoder::with_capacity(UNKNOWN_FRAME_LEN + 4);
    enc.encode_varint(1028_u64); // Arbitrary type.
    enc.encode_varint(UNKNOWN_FRAME_LEN as u64);
    let mut buf: Vec<_> = enc.into();
    buf.resize(UNKNOWN_FRAME_LEN + buf.len(), 0);
    wt.client.send_data(wt_session.stream_id(), &buf).unwrap();
    wt.exchange_packets();

    // The session is still active
    let mut unidi_server = WtTest::create_wt_stream_server(&mut wt_session, StreamType::UniDi);
    wt.send_data_server(&mut unidi_server, BUF);
    wt.receive_data_client(unidi_server.stream_id(), true, BUF, false);

    // Now close the session.
    wt.session_close_frame_client(wt_session.stream_id(), ERROR_NUM, ERROR_MESSAGE);
    wt.exchange_packets();

    wt.check_events_after_closing_session_client(
        &[unidi_server.stream_id()],
        Some(Error::HttpRequestCancelled.code()),
        &[],
        None,
        false,
        &None,
    );
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

#[test]
fn wt_close_session_frame_broken_client() {
    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    // Send a incorrect CloseSession frame.
    let mut enc = Encoder::default();
    WebTransportFrame::CloseSession {
        error: 5,
        message: "Hello".to_string(),
    }
    .encode(&mut enc);
    let mut buf: Vec<_> = enc.into();
    // Corrupt the string.
    buf[9] = 0xff;
    wt.client.send_data(wt_session.stream_id(), &buf).unwrap();
    wt.exchange_packets();

    // check that the webtransport session is closed.
    wt.check_session_closed_event_client(
        wt_session.stream_id(),
        &SessionCloseReason::Error(Error::HttpGeneralProtocolStream.code()),
        &None,
    );
    wt.check_session_closed_event_server(
        &mut wt_session,
        &SessionCloseReason::Error(Error::HttpGeneralProtocolStream.code()),
    );

    // The Http3 session is still working.
    assert_eq!(wt.client.state(), Http3State::Connected);
    assert_eq!(wt_session.state(), Http3State::Connected);
}

fn receive_request(server: &mut Http3Server) -> Option<Http3OrWebTransportStream> {
    while let Some(event) = server.next_event() {
        if let Http3ServerEvent::Headers { stream, .. } = event {
            return Some(stream);
        }
    }
    None
}

#[test]
// Ignoring this test as it is panicking at wt.create_wt_stream_client
// Issue # 1386 is created to track this
#[ignore]
fn wt_close_session_cannot_be_sent_at_once() {
    const BUF: &[u8] = &[0; 443];
    const ERROR_NUM: u32 = 23;
    const ERROR_MESSAGE: &str = "Something went wrong";

    let client = default_http3_client(wt_default_parameters());
    let server = default_http3_server(wt_default_parameters());
    let mut wt = WtTest::new_with(client, server);

    let mut wt_session = wt.create_wt_session();

    // Fill the flow control window using an unrelated http stream.
    let req_id = wt
        .client
        .fetch(
            now(),
            "GET",
            &("https", "something.com", "/"),
            &[],
            Priority::default(),
        )
        .unwrap();
    assert_eq!(req_id, 4);
    wt.exchange_packets();
    let mut req = receive_request(&mut wt.server).unwrap();
    req.send_headers(&[
        Header::new(":status", "200"),
        Header::new("content-length", BUF.len().to_string()),
    ])
    .unwrap();
    req.send_data(BUF).unwrap();

    // Now close the session.
    WtTest::session_close_frame_server(&mut wt_session, ERROR_NUM, ERROR_MESSAGE);
    // server cannot create new streams.
    assert_eq!(
        wt_session.create_stream(StreamType::UniDi),
        Err(Error::InvalidStreamId)
    );

    let out = wt.server.process(None, now()).dgram();
    let out = wt.client.process(out, now()).dgram();

    // Client has not received the full CloseSession frame and it can create more streams.
    let unidi_client = wt.create_wt_stream_client(wt_session.stream_id(), StreamType::UniDi);

    let out = wt.server.process(out, now()).dgram();
    let out = wt.client.process(out, now()).dgram();
    let out = wt.server.process(out, now()).dgram();
    let out = wt.client.process(out, now()).dgram();
    let out = wt.server.process(out, now()).dgram();
    let _out = wt.client.process(out, now()).dgram();

    wt.check_events_after_closing_session_client(
        &[],
        None,
        &[unidi_client],
        Some(Error::HttpRequestCancelled.code()),
        false,
        &Some((
            wt_session.stream_id(),
            SessionCloseReason::Clean {
                error: ERROR_NUM,
                message: ERROR_MESSAGE.to_string(),
            },
        )),
    );
    wt.check_events_after_closing_session_server(&[], None, &[], None, &None);
}
