#![cfg_attr(feature = "deny-warnings", deny(warnings))]
#![warn(clippy::pedantic)]

use neqo_crypto::{
    generate_ech_keys, AuthenticationStatus, Client, Error, HandshakeState, SecretAgentPreInfo,
    Server, ZeroRttCheckResult, ZeroRttChecker, TLS_AES_128_GCM_SHA256,
    TLS_CHACHA20_POLY1305_SHA256, TLS_GRP_EC_SECP256R1, TLS_VERSION_1_3,
};

use std::boxed::Box;

mod handshake;
use crate::handshake::{
    connect, connect_fail, forward_records, resumption_setup, PermissiveZeroRttChecker, Resumption,
    ZERO_RTT_TOKEN_DATA,
};
use test_fixture::{fixture_init, now};

#[test]
fn make_client() {
    fixture_init();
    let _c = Client::new("server", true).expect("should create client");
}

#[test]
fn make_server() {
    fixture_init();
    let _s = Server::new(&["key"]).expect("should create server");
}

#[test]
fn basic() {
    fixture_init();
    let mut client = Client::new("server.example", true).expect("should create client");
    println!("client {:p}", &client);
    let mut server = Server::new(&["key"]).expect("should create server");
    println!("server {:p}", &server);

    let bytes = client.handshake(now(), &[]).expect("send CH");
    assert!(!bytes.is_empty());
    assert_eq!(*client.state(), HandshakeState::InProgress);

    let bytes = server
        .handshake(now(), &bytes[..])
        .expect("read CH, send SH");
    assert!(!bytes.is_empty());
    assert_eq!(*server.state(), HandshakeState::InProgress);

    let bytes = client.handshake(now(), &bytes[..]).expect("send CF");
    assert!(bytes.is_empty());
    assert_eq!(*client.state(), HandshakeState::AuthenticationPending);

    client.authenticated(AuthenticationStatus::Ok);
    assert_eq!(*client.state(), HandshakeState::Authenticated(0));

    // Calling handshake() again indicates that we're happy with the cert.
    let bytes = client.handshake(now(), &[]).expect("send CF");
    assert!(!bytes.is_empty());
    assert!(client.state().is_connected());

    let client_info = client.info().expect("got info");
    assert_eq!(TLS_VERSION_1_3, client_info.version());
    assert_eq!(TLS_AES_128_GCM_SHA256, client_info.cipher_suite());

    let bytes = server.handshake(now(), &bytes[..]).expect("finish");
    assert!(bytes.is_empty());
    assert!(server.state().is_connected());

    let server_info = server.info().expect("got info");
    assert_eq!(TLS_VERSION_1_3, server_info.version());
    assert_eq!(TLS_AES_128_GCM_SHA256, server_info.cipher_suite());
}

fn check_client_preinfo(client_preinfo: &SecretAgentPreInfo) {
    assert_eq!(client_preinfo.version(), None);
    assert_eq!(client_preinfo.cipher_suite(), None);
    assert!(!client_preinfo.early_data());
    assert_eq!(client_preinfo.early_data_cipher(), None);
    assert_eq!(client_preinfo.max_early_data(), 0);
    assert_eq!(client_preinfo.alpn(), None);
}

fn check_server_preinfo(server_preinfo: &SecretAgentPreInfo) {
    assert_eq!(server_preinfo.version(), Some(TLS_VERSION_1_3));
    assert_eq!(server_preinfo.cipher_suite(), Some(TLS_AES_128_GCM_SHA256));
    assert!(!server_preinfo.early_data());
    assert_eq!(server_preinfo.early_data_cipher(), None);
    assert_eq!(server_preinfo.max_early_data(), 0);
    assert_eq!(server_preinfo.alpn(), None);
}

#[test]
fn raw() {
    fixture_init();
    let mut client = Client::new("server.example", true).expect("should create client");
    println!("client {client:?}");
    let mut server = Server::new(&["key"]).expect("should create server");
    println!("server {server:?}");

    let client_records = client.handshake_raw(now(), None).expect("send CH");
    assert!(!client_records.is_empty());
    assert_eq!(*client.state(), HandshakeState::InProgress);

    check_client_preinfo(&client.preinfo().expect("get preinfo"));

    let server_records =
        forward_records(now(), &mut server, client_records).expect("read CH, send SH");
    assert!(!server_records.is_empty());
    assert_eq!(*server.state(), HandshakeState::InProgress);

    check_server_preinfo(&server.preinfo().expect("get preinfo"));

    let client_records = forward_records(now(), &mut client, server_records).expect("send CF");
    assert!(client_records.is_empty());
    assert_eq!(*client.state(), HandshakeState::AuthenticationPending);

    client.authenticated(AuthenticationStatus::Ok);
    assert_eq!(*client.state(), HandshakeState::Authenticated(0));

    // Calling handshake() again indicates that we're happy with the cert.
    let client_records = client.handshake_raw(now(), None).expect("send CF");
    assert!(!client_records.is_empty());
    assert!(client.state().is_connected());

    let server_records = forward_records(now(), &mut server, client_records).expect("finish");
    assert!(server_records.is_empty());
    assert!(server.state().is_connected());

    // The client should have one certificate for the server.
    let mut certs = client.peer_certificate().unwrap();
    assert_eq!(1, certs.count());

    // The server shouldn't have a client certificate.
    assert!(server.peer_certificate().is_none());
}

#[test]
fn chacha_client() {
    fixture_init();
    let mut client = Client::new("server.example", true).expect("should create client");
    let mut server = Server::new(&["key"]).expect("should create server");
    client
        .set_ciphers(&[TLS_CHACHA20_POLY1305_SHA256])
        .expect("ciphers set");

    connect(&mut client, &mut server);

    assert_eq!(
        client.info().unwrap().cipher_suite(),
        TLS_CHACHA20_POLY1305_SHA256
    );
    assert_eq!(
        server.info().unwrap().cipher_suite(),
        TLS_CHACHA20_POLY1305_SHA256
    );
}

#[test]
fn p256_server() {
    fixture_init();
    let mut client = Client::new("server.example", true).expect("should create client");
    let mut server = Server::new(&["key"]).expect("should create server");
    server
        .set_groups(&[TLS_GRP_EC_SECP256R1])
        .expect("groups set");

    connect(&mut client, &mut server);

    assert_eq!(client.info().unwrap().key_exchange(), TLS_GRP_EC_SECP256R1);
    assert_eq!(server.info().unwrap().key_exchange(), TLS_GRP_EC_SECP256R1);
}

#[test]
fn alpn() {
    fixture_init();
    let mut client = Client::new("server.example", true).expect("should create client");
    client.set_alpn(&["alpn"]).expect("should set ALPN");
    let mut server = Server::new(&["key"]).expect("should create server");
    server.set_alpn(&["alpn"]).expect("should set ALPN");

    connect(&mut client, &mut server);

    let expected = Some(String::from("alpn"));
    assert_eq!(expected.as_ref(), client.info().unwrap().alpn());
    assert_eq!(expected.as_ref(), server.info().unwrap().alpn());
}

#[test]
fn alpn_multi() {
    fixture_init();
    let mut client = Client::new("server.example", true).expect("should create client");
    client
        .set_alpn(&["dummy", "alpn"])
        .expect("should set ALPN");
    let mut server = Server::new(&["key"]).expect("should create server");
    server
        .set_alpn(&["alpn", "other"])
        .expect("should set ALPN");

    connect(&mut client, &mut server);

    let expected = Some(String::from("alpn"));
    assert_eq!(expected.as_ref(), client.info().unwrap().alpn());
    assert_eq!(expected.as_ref(), server.info().unwrap().alpn());
}

#[test]
fn alpn_server_pref() {
    fixture_init();
    let mut client = Client::new("server.example", true).expect("should create client");
    client
        .set_alpn(&["dummy", "alpn"])
        .expect("should set ALPN");
    let mut server = Server::new(&["key"]).expect("should create server");
    server
        .set_alpn(&["alpn", "dummy"])
        .expect("should set ALPN");

    connect(&mut client, &mut server);

    let expected = Some(String::from("alpn"));
    assert_eq!(expected.as_ref(), client.info().unwrap().alpn());
    assert_eq!(expected.as_ref(), server.info().unwrap().alpn());
}

#[test]
fn alpn_no_protocol() {
    fixture_init();
    let mut client = Client::new("server.example", true).expect("should create client");
    client.set_alpn(&["a"]).expect("should set ALPN");
    let mut server = Server::new(&["key"]).expect("should create server");
    server.set_alpn(&["b"]).expect("should set ALPN");

    connect_fail(&mut client, &mut server);

    // TODO(mt) check the error code
}

#[test]
fn alpn_client_only() {
    fixture_init();
    let mut client = Client::new("server.example", true).expect("should create client");
    client.set_alpn(&["alpn"]).expect("should set ALPN");
    let mut server = Server::new(&["key"]).expect("should create server");

    connect(&mut client, &mut server);

    assert_eq!(None, client.info().unwrap().alpn());
    assert_eq!(None, server.info().unwrap().alpn());
}

#[test]
fn alpn_server_only() {
    fixture_init();
    let mut client = Client::new("server.example", true).expect("should create client");
    let mut server = Server::new(&["key"]).expect("should create server");
    server.set_alpn(&["alpn"]).expect("should set ALPN");

    connect(&mut client, &mut server);

    assert_eq!(None, client.info().unwrap().alpn());
    assert_eq!(None, server.info().unwrap().alpn());
}

#[test]
fn resume() {
    let (_, token) = resumption_setup(Resumption::WithoutZeroRtt);

    let mut client = Client::new("server.example", true).expect("should create second client");
    let mut server = Server::new(&["key"]).expect("should create second server");

    client
        .enable_resumption(token)
        .expect("should accept token");
    connect(&mut client, &mut server);

    assert!(client.info().unwrap().resumed());
    assert!(server.info().unwrap().resumed());
}

#[test]
fn zero_rtt() {
    let (anti_replay, token) = resumption_setup(Resumption::WithZeroRtt);

    // Finally, 0-RTT should succeed.
    let mut client = Client::new("server.example", true).expect("should create client");
    let mut server = Server::new(&["key"]).expect("should create server");
    client
        .enable_resumption(token)
        .expect("should accept token");
    client.enable_0rtt().expect("should enable 0-RTT");
    server
        .enable_0rtt(
            anti_replay.as_ref().unwrap(),
            0xffff_ffff,
            Box::<PermissiveZeroRttChecker>::default(),
        )
        .expect("should enable 0-RTT");

    connect(&mut client, &mut server);
    assert!(client.info().unwrap().early_data_accepted());
    assert!(server.info().unwrap().early_data_accepted());
}

#[test]
fn zero_rtt_no_eoed() {
    let (anti_replay, token) = resumption_setup(Resumption::WithZeroRtt);

    // Finally, 0-RTT should succeed.
    let mut client = Client::new("server.example", true).expect("should create client");
    let mut server = Server::new(&["key"]).expect("should create server");
    client
        .enable_resumption(token)
        .expect("should accept token");
    client.enable_0rtt().expect("should enable 0-RTT");
    client
        .disable_end_of_early_data()
        .expect("should disable EOED");
    server
        .enable_0rtt(
            anti_replay.as_ref().unwrap(),
            0xffff_ffff,
            Box::<PermissiveZeroRttChecker>::default(),
        )
        .expect("should enable 0-RTT");
    server
        .disable_end_of_early_data()
        .expect("should disable EOED");

    connect(&mut client, &mut server);
    assert!(client.info().unwrap().early_data_accepted());
    assert!(server.info().unwrap().early_data_accepted());
}

#[derive(Debug)]
struct RejectZeroRtt {}
impl ZeroRttChecker for RejectZeroRtt {
    fn check(&self, token: &[u8]) -> ZeroRttCheckResult {
        assert_eq!(ZERO_RTT_TOKEN_DATA, token);
        ZeroRttCheckResult::Reject
    }
}

#[test]
fn reject_zero_rtt() {
    let (anti_replay, token) = resumption_setup(Resumption::WithZeroRtt);

    // Finally, 0-RTT should succeed.
    let mut client = Client::new("server.example", true).expect("should create client");
    let mut server = Server::new(&["key"]).expect("should create server");
    client
        .enable_resumption(token)
        .expect("should accept token");
    client.enable_0rtt().expect("should enable 0-RTT");
    server
        .enable_0rtt(
            anti_replay.as_ref().unwrap(),
            0xffff_ffff,
            Box::new(RejectZeroRtt {}),
        )
        .expect("should enable 0-RTT");

    connect(&mut client, &mut server);
    assert!(!client.info().unwrap().early_data_accepted());
    assert!(!server.info().unwrap().early_data_accepted());
}

#[test]
fn close() {
    fixture_init();
    let mut client = Client::new("server.example", true).expect("should create client");
    let mut server = Server::new(&["key"]).expect("should create server");
    connect(&mut client, &mut server);
    client.close();
    server.close();
}

#[test]
fn close_client_twice() {
    fixture_init();
    let mut client = Client::new("server.example", true).expect("should create client");
    let mut server = Server::new(&["key"]).expect("should create server");
    connect(&mut client, &mut server);
    client.close();
    client.close(); // Should be a noop.
}

#[test]
fn ech() {
    fixture_init();
    let mut server = Server::new(&["key"]).expect("should create server");
    let (sk, pk) = generate_ech_keys().expect("ECH keys");
    server
        .enable_ech(88, "public.example", &sk, &pk)
        .expect("should enable server ECH");

    let mut client = Client::new("server.example", true).expect("should create client");
    client
        .enable_ech(server.ech_config())
        .expect("should enable client ECH");

    connect(&mut client, &mut server);
    assert!(client.info().unwrap().ech_accepted());
    assert!(server.info().unwrap().ech_accepted());
    assert!(client.preinfo().unwrap().ech_accepted().unwrap());
    assert!(server.preinfo().unwrap().ech_accepted().unwrap());
}

#[test]
fn ech_retry() {
    const PUBLIC_NAME: &str = "public.example";
    const PRIVATE_NAME: &str = "private.example";
    const CONFIG_ID: u8 = 7;

    fixture_init();
    let mut server = Server::new(&["key"]).unwrap();
    let (sk, pk) = generate_ech_keys().unwrap();
    server.enable_ech(CONFIG_ID, PUBLIC_NAME, &sk, &pk).unwrap();

    let mut client = Client::new(PRIVATE_NAME, true).unwrap();
    let mut cfg = Vec::from(server.ech_config());
    // Ensure that the version and config_id is correct.
    assert_eq!(cfg[2], 0xfe);
    assert_eq!(cfg[3], 0x0d);
    assert_eq!(cfg[6], CONFIG_ID);
    // Change the config_id so that the server doesn't recognize this.
    cfg[6] ^= 0x94;
    client.enable_ech(&cfg).unwrap();

    // Long version of connect() so that we can check the state.
    let records = client.handshake_raw(now(), None).unwrap(); // ClientHello
    let records = forward_records(now(), &mut server, records).unwrap(); // ServerHello...
    let records = forward_records(now(), &mut client, records).unwrap(); // (empty)
    assert!(records.is_empty());

    // The client should now be expecting authentication.
    assert_eq!(
        *client.state(),
        HandshakeState::EchFallbackAuthenticationPending(String::from(PUBLIC_NAME))
    );
    client.authenticated(AuthenticationStatus::Ok);
    let Err(Error::EchRetry(updated_config)) = client.handshake_raw(now(), None) else {
        panic!(
            "Handshake should fail with EchRetry, state is instead {:?}",
            client.state()
        );
    };
    assert_eq!(
        client
            .preinfo()
            .unwrap()
            .ech_public_name()
            .unwrap()
            .unwrap(),
        PUBLIC_NAME
    );
    // We don't forward alerts, so we can't tell the server about them.
    // An ech_required alert should be set though.
    assert_eq!(client.alert(), Some(&121));

    let mut server = Server::new(&["key"]).unwrap();
    server.enable_ech(CONFIG_ID, PUBLIC_NAME, &sk, &pk).unwrap();
    let mut client = Client::new(PRIVATE_NAME, true).unwrap();
    client.enable_ech(&updated_config).unwrap();

    connect(&mut client, &mut server);

    assert!(client.info().unwrap().ech_accepted());
    assert!(server.info().unwrap().ech_accepted());
    assert!(client.preinfo().unwrap().ech_accepted().unwrap());
    assert!(server.preinfo().unwrap().ech_accepted().unwrap());
}
