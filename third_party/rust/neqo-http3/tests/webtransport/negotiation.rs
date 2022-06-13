// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::{connect, default_http3_client, default_http3_server, exchange_packets};
use neqo_common::{event::Provider, Encoder};
use neqo_crypto::AuthenticationStatus;
use neqo_http3::{
    settings::{HSetting, HSettingType, HSettings},
    Error, HFrame, Http3Client, Http3ClientEvent, Http3State, WebTransportEvent,
};
use neqo_transport::{Connection, ConnectionError, StreamType};
use std::time::Duration;
use test_fixture::*;

fn check_wt_event(client: &mut Http3Client, wt_enable_client: bool, wt_enable_server: bool) {
    let wt_event = client.events().find_map(|e| {
        if let Http3ClientEvent::WebTransport(WebTransportEvent::Negotiated(neg)) = e {
            Some(neg)
        } else {
            None
        }
    });

    assert_eq!(wt_event.is_some(), wt_enable_client);
    if let Some(wt) = wt_event {
        assert_eq!(wt, wt_enable_client && wt_enable_server);
    }
}

#[test]
fn negotiate_wt() {
    let (mut client, _server) = connect(true, true);
    assert!(client.webtransport_enabled());
    check_wt_event(&mut client, true, true);

    let (mut client, _server) = connect(true, false);
    assert!(!client.webtransport_enabled());
    check_wt_event(&mut client, true, false);

    let (mut client, _server) = connect(false, true);
    assert!(!client.webtransport_enabled());
    check_wt_event(&mut client, false, true);

    let (mut client, _server) = connect(false, false);
    assert!(!client.webtransport_enabled());
    check_wt_event(&mut client, false, false);
}

fn zero_rtt(client_org: bool, server_org: bool, client_resumed: bool, server_resumed: bool) {
    let (mut client, mut server) = connect(client_org, server_org);
    assert_eq!(client.webtransport_enabled(), client_org && server_org);

    // exchane token
    let out = server.process(None, now());
    // We do not have a token so we need to wait for a resumption token timer to trigger.
    let _ = client.process(out.dgram(), now() + Duration::from_millis(250));
    assert_eq!(client.state(), Http3State::Connected);
    let token = client
        .events()
        .find_map(|e| {
            if let Http3ClientEvent::ResumptionToken(token) = e {
                Some(token)
            } else {
                None
            }
        })
        .unwrap();

    let mut client = default_http3_client(client_resumed);
    let mut server = default_http3_server(server_resumed);
    client
        .enable_resumption(now(), &token)
        .expect("Set resumption token.");
    assert_eq!(client.state(), Http3State::ZeroRtt);

    exchange_packets(&mut client, &mut server);

    assert_eq!(&client.state(), &Http3State::Connected);
    assert_eq!(
        client.webtransport_enabled(),
        client_resumed && server_resumed
    );
    check_wt_event(&mut client, client_resumed, server_resumed);
}

#[test]
fn zero_rtt_wt_settings() {
    zero_rtt(true, true, true, true);
    zero_rtt(true, true, true, false);
    zero_rtt(true, true, false, true);
    zero_rtt(true, true, false, false);

    zero_rtt(true, false, true, false);
    zero_rtt(true, false, true, true);
    zero_rtt(true, false, false, false);
    zero_rtt(true, false, false, true);

    zero_rtt(false, false, false, false);
    zero_rtt(false, false, false, true);
    zero_rtt(false, false, true, false);
    zero_rtt(false, false, true, true);

    zero_rtt(false, true, false, true);
    zero_rtt(false, true, false, false);
    zero_rtt(false, true, true, false);
    zero_rtt(false, true, true, true);
}

fn exchange_packets2(client: &mut Http3Client, server: &mut Connection) {
    let mut out = None;
    loop {
        out = client.process(out, now()).dgram();
        out = server.process(out, now()).dgram();
        if out.is_none() {
            break;
        }
    }
}

#[test]
fn wrong_setting_value() {
    const CONTROL_STREAM_TYPE: &[u8] = &[0x0];
    let mut client = default_http3_client(false);
    let mut server = default_server_h3();

    exchange_packets2(&mut client, &mut server);
    client.authenticated(AuthenticationStatus::Ok, now());
    exchange_packets2(&mut client, &mut server);

    let control = server.stream_create(StreamType::UniDi).unwrap();
    server.stream_send(control, CONTROL_STREAM_TYPE).unwrap();
    // Encode a settings frame and send it.
    let mut enc = Encoder::default();
    let settings = HFrame::Settings {
        settings: HSettings::new(&[HSetting::new(HSettingType::EnableWebTransport, 2)]),
    };
    settings.encode(&mut enc);
    assert_eq!(
        server.stream_send(control, enc.as_ref()).unwrap(),
        enc.as_ref().len()
    );

    exchange_packets2(&mut client, &mut server);
    match client.state() {
        Http3State::Closing(err) | Http3State::Closed(err) => {
            assert_eq!(
                err,
                ConnectionError::Application(Error::HttpSettings.code())
            );
        }
        _ => panic!("Wrong state {:?}", client.state()),
    };
}
