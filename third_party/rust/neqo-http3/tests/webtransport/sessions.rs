// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::webtransport::WtTest;
use neqo_http3::Error;
use std::mem;

#[test]
fn wt_session() {
    let mut wt = WtTest::new();
    mem::drop(wt.create_wt_session());
}

#[test]
fn wt_session_reject() {
    let mut wt = WtTest::new();
    let (wt_session_id, _wt_session) = wt.negotiate_wt_session(false);

    wt.check_session_closed_event_client(wt_session_id, Some(Error::HttpRequestRejected.code()));
}

#[test]
fn wt_session_close_client() {
    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    wt.cancel_session_client(wt_session.stream_id());
    wt.check_session_closed_event_server(&mut wt_session, Some(Error::HttpNoError.code()));
}

#[test]
fn wt_session_close_server() {
    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    wt.cancel_session_server(&mut wt_session);
    wt.check_session_closed_event_client(wt_session.stream_id(), Some(Error::HttpNoError.code()));
}

#[test]
fn wt_session_close_server_close_send() {
    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    wt_session.stream_close_send().unwrap();
    wt.exchange_packets();
    wt.check_session_closed_event_client(
        wt_session.stream_id(),
        Some(Error::HttpGeneralProtocolStream.code()),
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
    wt.check_session_closed_event_client(wt_session.stream_id(), Some(Error::HttpNoError.code()));
}

#[test]
fn wt_session_close_server_reset() {
    let mut wt = WtTest::new();
    let mut wt_session = wt.create_wt_session();

    wt_session
        .stream_reset_send(Error::HttpNoError.code())
        .unwrap();
    wt.exchange_packets();
    wt.check_session_closed_event_client(wt_session.stream_id(), Some(Error::HttpNoError.code()));
}
