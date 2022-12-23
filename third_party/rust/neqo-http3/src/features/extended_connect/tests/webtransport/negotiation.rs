// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::{connect, default_http3_client, default_http3_server, exchange_packets};
use crate::{
    settings::{HSetting, HSettingType, HSettings},
    Error, HFrame, Http3Client, Http3ClientEvent, Http3Parameters, Http3Server, Http3State,
    WebTransportEvent,
};
use neqo_common::{event::Provider, Encoder};
use neqo_crypto::AuthenticationStatus;
use neqo_transport::{Connection, ConnectionError, StreamType};
use std::time::Duration;
use test_fixture::{default_server_h3, now};

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

fn connect_wt(wt_enabled_client: bool, wt_enabled_server: bool) -> (Http3Client, Http3Server) {
    connect(
        Http3Parameters::default().webtransport(wt_enabled_client),
        Http3Parameters::default().webtransport(wt_enabled_server),
    )
}

#[test]
fn negotiate_wt() {
    let (mut client, _server) = connect_wt(true, true);
    assert!(client.webtransport_enabled());
    check_wt_event(&mut client, true, true);

    let (mut client, _server) = connect_wt(true, false);
    assert!(!client.webtransport_enabled());
    check_wt_event(&mut client, true, false);

    let (mut client, _server) = connect_wt(false, true);
    assert!(!client.webtransport_enabled());
    check_wt_event(&mut client, false, true);

    let (mut client, _server) = connect_wt(false, false);
    assert!(!client.webtransport_enabled());
    check_wt_event(&mut client, false, false);
}

#[derive(PartialEq, Eq)]
enum ClientState {
    ClientEnabled,
    ClientDisabled,
}

#[derive(PartialEq, Eq)]
enum ClientResumedState {
    ClientResumed,
    ClientSuspended,
}

#[derive(PartialEq, Eq)]
enum ServerState {
    ServerEnabled,
    ServerDisabled,
}

#[derive(PartialEq, Eq)]
enum ServerResumedState {
    ServerResumed,
    ServerSuspended,
}

fn zero_rtt(
    client_state: &ClientState,
    server_state: &ServerState,
    client_resumed: &ClientResumedState,
    server_resumed: &ServerResumedState,
) {
    let (mut client, mut server) = connect_wt(
        ClientState::ClientEnabled.eq(client_state),
        ServerState::ServerEnabled.eq(server_state),
    );
    assert_eq!(
        client.webtransport_enabled(),
        ClientState::ClientEnabled.eq(client_state) && ServerState::ServerEnabled.eq(server_state)
    );

    // exchane token
    let out = server.process(None, now());
    // We do not have a token so we need to wait for a resumption token timer to trigger.
    std::mem::drop(client.process(out.dgram(), now() + Duration::from_millis(250)));
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

    let mut client = default_http3_client(
        Http3Parameters::default()
            .webtransport(ClientResumedState::ClientResumed.eq(client_resumed)),
    );
    let mut server = default_http3_server(
        Http3Parameters::default()
            .webtransport(ServerResumedState::ServerResumed.eq(server_resumed)),
    );
    client
        .enable_resumption(now(), &token)
        .expect("Set resumption token.");
    assert_eq!(client.state(), Http3State::ZeroRtt);

    exchange_packets(&mut client, &mut server);

    assert_eq!(&client.state(), &Http3State::Connected);
    assert_eq!(
        client.webtransport_enabled(),
        ClientResumedState::ClientResumed.eq(client_resumed)
            && ServerResumedState::ServerResumed.eq(server_resumed)
    );
    check_wt_event(
        &mut client,
        ClientResumedState::ClientResumed.eq(client_resumed),
        ServerResumedState::ServerResumed.eq(server_resumed),
    );
}

#[test]
fn zero_rtt_wt_settings() {
    zero_rtt(
        &ClientState::ClientEnabled,
        &ServerState::ServerEnabled,
        &ClientResumedState::ClientResumed,
        &ServerResumedState::ServerResumed,
    );
    zero_rtt(
        &ClientState::ClientEnabled,
        &ServerState::ServerEnabled,
        &ClientResumedState::ClientResumed,
        &ServerResumedState::ServerSuspended,
    );
    zero_rtt(
        &ClientState::ClientEnabled,
        &ServerState::ServerEnabled,
        &ClientResumedState::ClientSuspended,
        &ServerResumedState::ServerResumed,
    );
    zero_rtt(
        &ClientState::ClientEnabled,
        &ServerState::ServerEnabled,
        &ClientResumedState::ClientSuspended,
        &ServerResumedState::ServerSuspended,
    );

    zero_rtt(
        &ClientState::ClientEnabled,
        &ServerState::ServerDisabled,
        &ClientResumedState::ClientResumed,
        &ServerResumedState::ServerSuspended,
    );
    zero_rtt(
        &ClientState::ClientEnabled,
        &ServerState::ServerDisabled,
        &ClientResumedState::ClientResumed,
        &ServerResumedState::ServerResumed,
    );
    zero_rtt(
        &ClientState::ClientEnabled,
        &ServerState::ServerDisabled,
        &ClientResumedState::ClientSuspended,
        &ServerResumedState::ServerSuspended,
    );
    zero_rtt(
        &ClientState::ClientEnabled,
        &ServerState::ServerDisabled,
        &ClientResumedState::ClientSuspended,
        &ServerResumedState::ServerResumed,
    );

    zero_rtt(
        &ClientState::ClientDisabled,
        &ServerState::ServerDisabled,
        &ClientResumedState::ClientSuspended,
        &ServerResumedState::ServerSuspended,
    );
    zero_rtt(
        &ClientState::ClientDisabled,
        &ServerState::ServerDisabled,
        &ClientResumedState::ClientSuspended,
        &ServerResumedState::ServerResumed,
    );
    zero_rtt(
        &ClientState::ClientDisabled,
        &ServerState::ServerDisabled,
        &ClientResumedState::ClientResumed,
        &ServerResumedState::ServerSuspended,
    );
    zero_rtt(
        &ClientState::ClientDisabled,
        &ServerState::ServerDisabled,
        &ClientResumedState::ClientResumed,
        &ServerResumedState::ServerResumed,
    );

    zero_rtt(
        &ClientState::ClientDisabled,
        &ServerState::ServerEnabled,
        &ClientResumedState::ClientSuspended,
        &ServerResumedState::ServerResumed,
    );
    zero_rtt(
        &ClientState::ClientDisabled,
        &ServerState::ServerEnabled,
        &ClientResumedState::ClientSuspended,
        &ServerResumedState::ServerSuspended,
    );
    zero_rtt(
        &ClientState::ClientDisabled,
        &ServerState::ServerEnabled,
        &ClientResumedState::ClientResumed,
        &ServerResumedState::ServerSuspended,
    );
    zero_rtt(
        &ClientState::ClientDisabled,
        &ServerState::ServerEnabled,
        &ClientResumedState::ClientResumed,
        &ServerResumedState::ServerResumed,
    );
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
    let mut client = default_http3_client(Http3Parameters::default());
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
