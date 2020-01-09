// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(feature = "deny-warnings", deny(warnings))]

use neqo_common::{hex, matches, qdebug, qtrace, Datagram, Decoder, Encoder};
use neqo_crypto::{
    aead::Aead,
    constants::{TLS_AES_128_GCM_SHA256, TLS_VERSION_1_3},
    hkdf,
    hp::HpKey,
    AuthenticationStatus,
};
use neqo_transport::{
    server::{ActiveConnectionRef, Server},
    Connection, ConnectionError, Error, FixedConnectionIdManager, Output, State, StreamType,
    QUIC_VERSION,
};
use test_fixture::{self, assertions, default_client, now};

use std::cell::RefCell;
use std::convert::TryFrom;
use std::net::{IpAddr, Ipv4Addr, SocketAddr};
use std::rc::Rc;
use std::time::Duration;

// Different than the one in the fixture, which is a single connection.
fn default_server() -> Server {
    Server::new(
        now(),
        test_fixture::DEFAULT_KEYS,
        test_fixture::DEFAULT_ALPN,
        test_fixture::anti_replay(),
        Rc::new(RefCell::new(FixedConnectionIdManager::new(9))),
    )
    .expect("should create a server")
}

fn connected_server(server: &mut Server) -> ActiveConnectionRef {
    let server_connections = server.active_connections();
    assert_eq!(server_connections.len(), 1);
    assert_eq!(*server_connections[0].borrow().state(), State::Connected);
    server_connections[0].clone()
}

fn connect(client: &mut Connection, server: &mut Server) -> ActiveConnectionRef {
    server.set_retry_required(false);

    assert_eq!(*client.state(), State::Init);
    let dgram = client.process(None, now()).dgram(); // ClientHello
    assert!(dgram.is_some());
    let dgram = server.process(dgram, now()).dgram(); // ServerHello...
    assert!(dgram.is_some());

    // Ingest the server Certificate.
    let dgram = client.process(dgram, now()).dgram();
    assert!(dgram.is_some()); // This should just be an ACK.
    let dgram = server.process(dgram, now()).dgram();
    assert!(dgram.is_none()); // So the server should have nothing to say.

    // Now mark the server as authenticated.
    client.authenticated(AuthenticationStatus::Ok, now());
    let dgram = client.process(None, now()).dgram();
    assert!(dgram.is_some());
    assert_eq!(*client.state(), State::Connected);
    let dgram = server.process(dgram, now()).dgram();
    assert!(dgram.is_some()); // ACK + NST
    connected_server(server)
}

#[test]
fn single_client() {
    let mut server = default_server();
    let mut client = default_client();

    connect(&mut client, &mut server);
}

#[test]
fn retry() {
    let mut server = default_server();
    server.set_retry_required(true);
    let mut client = default_client();

    let dgram = client.process(None, now()).dgram(); // Initial
    assert!(dgram.is_some());
    let dgram = server.process(dgram, now()).dgram(); // Retry
    assert!(dgram.is_some());

    assertions::assert_retry(&dgram.as_ref().unwrap());

    let dgram = client.process(dgram, now()).dgram(); // Initial w/token
    assert!(dgram.is_some());
    let dgram = server.process(dgram, now()).dgram(); // Initial, HS
    assert!(dgram.is_some());
    let _ = client.process(dgram, now()).dgram(); // Ingest, drop any ACK.
    client.authenticated(AuthenticationStatus::Ok, now());
    let dgram = client.process(None, now()).dgram(); // Send Finished
    assert!(dgram.is_some());
    assert_eq!(*client.state(), State::Connected);
    let dgram = server.process(dgram, now()).dgram(); // (done)
    assert!(dgram.is_some()); // Note that this packet will be dropped...
    connected_server(&mut server);
}

// attempt a retry with 0-RTT, and have 0-RTT packets sent with the second ClientHello
#[test]
fn retry_0rtt() {
    let mut server = default_server();
    let mut client = default_client();

    let mut server_conn = connect(&mut client, &mut server);
    server_conn
        .borrow_mut()
        .send_ticket(now(), &[])
        .expect("ticket should go out");
    let dgram = server.process(None, now()).dgram();
    client.process_input(dgram.unwrap(), now()); // Consume ticket, ignore output.
    let token = client.resumption_token().expect("should get token");
    // Calling active_connections clears the set of active connections.
    assert_eq!(server.active_connections().len(), 1);

    server.set_retry_required(true);
    let mut client = default_client();
    client
        .set_resumption_token(now(), &token)
        .expect("should set token");

    let client_stream = client.stream_create(StreamType::UniDi).unwrap();
    client.stream_send(client_stream, &[1, 2, 3]).unwrap();

    let dgram = client.process(None, now()).dgram(); // Initial w/0-RTT
    assert!(dgram.is_some());
    assertions::assert_coalesced_0rtt(dgram.as_ref().unwrap());
    let dgram = server.process(dgram, now()).dgram(); // Retry
    assert!(dgram.is_some());
    assertions::assert_retry(dgram.as_ref().unwrap());

    // After retry, there should be a token and still coalesced 0-RTT.
    let dgram = client.process(dgram, now()).dgram();
    assert!(dgram.is_some());
    assertions::assert_coalesced_0rtt(dgram.as_ref().unwrap());

    let dgram = server.process(dgram, now()).dgram(); // Initial, HS
    assert!(dgram.is_some());
    let dgram = client.process(dgram, now()).dgram();
    // Note: the client doesn't need to authenticate the server here
    // as there is no certificate; authentication is based on the ticket.
    assert!(dgram.is_some());
    assert_eq!(*client.state(), State::Connected);
    let dgram = server.process(dgram, now()).dgram(); // (done)
    assert!(dgram.is_some());
    connected_server(&mut server);
    assert!(client.tls_info().unwrap().resumed());
}

#[test]
fn retry_different_ip() {
    let mut server = default_server();
    server.set_retry_required(true);
    let mut client = default_client();

    let dgram = client.process(None, now()).dgram(); // Initial
    assert!(dgram.is_some());
    let dgram = server.process(dgram, now()).dgram(); // Retry
    assert!(dgram.is_some());

    assertions::assert_retry(&dgram.as_ref().unwrap());

    let dgram = client.process(dgram, now()).dgram(); // Initial w/token
    assert!(dgram.is_some());

    // Change the source IP on the address from the client.
    let dgram = dgram.unwrap();
    let other_v4 = IpAddr::V4(Ipv4Addr::new(127, 0, 0, 2));
    let other_addr = SocketAddr::new(other_v4, 443);
    let from_other = Datagram::new(other_addr, dgram.destination(), &dgram[..]);
    let dgram = server.process(Some(from_other), now()).dgram();
    assert!(dgram.is_none());
}

#[test]
fn retry_after_initial() {
    let mut server = default_server();
    let mut retry_server = default_server();
    retry_server.set_retry_required(true);
    let mut client = default_client();

    let cinit = client.process(None, now()).dgram(); // Initial
    assert!(cinit.is_some());
    let server_flight = server.process(cinit.clone(), now()).dgram(); // Initial
    assert!(server_flight.is_some());

    // We need to have the client just process the Initial.
    // Rather than try to find the Initial, we can just truncate the Handshake that follows.
    let si = server_flight.as_ref().unwrap();
    let truncated = &si[..(si.len() - 1)];
    let just_initial = Datagram::new(si.source(), si.destination(), truncated);
    let dgram = client.process(Some(just_initial), now()).dgram();
    assert!(dgram.is_some());
    assert!(*client.state() != State::Connected);

    let retry = retry_server.process(cinit, now()).dgram(); // Retry!
    assert!(retry.is_some());
    assertions::assert_retry(&retry.as_ref().unwrap());

    // The client should ignore the retry.
    let junk = client.process(retry, now()).dgram();
    assert!(junk.is_none());

    // Either way, the client should still be able to process the server flight and connect.
    let dgram = client.process(server_flight, now()).dgram();
    assert!(dgram.is_some()); // Drop this one.
    assert!(test_fixture::maybe_authenticate(&mut client));
    let dgram = client.process(None, now()).dgram();
    assert!(dgram.is_some());

    assert_eq!(*client.state(), State::Connected);
    let dgram = server.process(dgram, now()).dgram(); // (done)
    assert!(dgram.is_some());
    connected_server(&mut server);
}

#[test]
fn retry_bad_odcid() {
    let mut server = default_server();
    server.set_retry_required(true);
    let mut client = default_client();

    let dgram = client.process(None, now()).dgram(); // Initial
    assert!(dgram.is_some());
    let dgram = server.process(dgram, now()).dgram(); // Retry
    assert!(dgram.is_some());

    let retry = &dgram.as_ref().unwrap();
    assertions::assert_retry(retry);

    let mut dec = Decoder::new(retry); // Start after version.
    dec.skip(5);
    dec.skip_vec(1); // DCID
    dec.skip_vec(1); // SCID
    let odcid_len = dec.decode_byte().unwrap();
    assert_ne!(odcid_len, 0);
    let odcid_offset = retry.len() - dec.remaining();
    assert!(odcid_offset < retry.len());
    let mut tweaked_retry = retry[..].to_vec();
    tweaked_retry[odcid_offset] ^= 0x45; // damage the ODCID
    let tweaked_packet = Datagram::new(retry.source(), retry.destination(), tweaked_retry);

    // The client should ignore this packet.
    let dgram = client.process(Some(tweaked_packet), now()).dgram();
    assert!(dgram.is_none());
}

#[test]
fn retry_bad_token() {
    let mut client = default_client();
    let mut retry_server = default_server();
    retry_server.set_retry_required(true);
    let mut server = default_server();

    // Send a retry to one server, then replay it to the other.
    let client_initial1 = client.process(None, now()).dgram();
    assert!(client_initial1.is_some());
    let retry = retry_server.process(client_initial1, now()).dgram();
    assert!(retry.is_some());
    let client_initial2 = client.process(retry, now()).dgram();
    assert!(client_initial2.is_some());

    let dgram = server.process(client_initial2, now()).dgram();
    assert!(dgram.is_none());
}

// Generate an AEAD and header protection object for a client Initial.
fn client_initial_aead_and_hp(dcid: &[u8]) -> (Aead, HpKey) {
    const INITIAL_SALT: &[u8] = &[
        0xc3, 0xee, 0xf7, 0x12, 0xc7, 0x2e, 0xbb, 0x5a, 0x11, 0xa7, 0xd2, 0x43, 0x2b, 0xb4, 0x63,
        0x65, 0xbe, 0xf9, 0xf5, 0x02,
    ];
    let initial_secret = hkdf::extract(
        TLS_VERSION_1_3,
        TLS_AES_128_GCM_SHA256,
        Some(
            hkdf::import_key(TLS_VERSION_1_3, TLS_AES_128_GCM_SHA256, INITIAL_SALT)
                .as_ref()
                .unwrap(),
        ),
        hkdf::import_key(TLS_VERSION_1_3, TLS_AES_128_GCM_SHA256, dcid)
            .as_ref()
            .unwrap(),
    )
    .unwrap();

    let secret = hkdf::expand_label(
        TLS_VERSION_1_3,
        TLS_AES_128_GCM_SHA256,
        &initial_secret,
        &[],
        "client in",
    )
    .unwrap();
    (
        Aead::new(TLS_VERSION_1_3, TLS_AES_128_GCM_SHA256, &secret, "quic ").unwrap(),
        HpKey::extract(TLS_VERSION_1_3, TLS_AES_128_GCM_SHA256, &secret, "quic hp").unwrap(),
    )
}

// This tests a simulated on-path attacker that intercepts the first
// client Initial packet and spoofs a retry.
// The tricky part is in rewriting the second client Initial so that
// the server doesn't reject the Initial for having a bad token.
// The client is the only one that can detect this, and that is because
// the original connection ID is not in transport parameters.
//
// Note that this depends on having the server produce a CID that is
// at least 8 bytes long.  Otherwise, the second Initial won't have a
// long enough connection ID.
#[test]
fn mitm_retry() {
    let mut client = default_client();
    let mut retry_server = default_server();
    retry_server.set_retry_required(true);
    let mut server = default_server();

    // Trigger initial and a second client Initial.
    let client_initial1 = client.process(None, now()).dgram();
    assert!(client_initial1.is_some());
    let retry = retry_server.process(client_initial1, now()).dgram();
    assert!(retry.is_some());
    let client_initial2 = client.process(retry, now()).dgram();
    assert!(client_initial2.is_some());

    // Now to start the epic process of decrypting the packet,
    // rewriting the header to remove the token, and then re-encrypting.
    let client_initial2 = client_initial2.unwrap();
    // First, decode the initial up to the packet number.
    let ci = &client_initial2[..];
    let mut dec = Decoder::new(&ci);
    let type_and_ver = dec.decode(5).unwrap().to_vec();
    assert_eq!(type_and_ver[0] & 0xf0, 0xc0);
    let dcid = dec.decode_vec(1).unwrap().to_vec();
    let scid = dec.decode_vec(1).unwrap().to_vec();
    dec.skip_vvec(); // Token.
    let mut payload_len = usize::try_from(dec.decode_varint().unwrap()).unwrap();
    let pn_offset = ci.len() - dec.remaining();

    // Now we have enough information to make keys.
    let (aead, hp) = client_initial_aead_and_hp(&dcid);
    // Make a copy of the header that can be modified.
    // Save 4 extra in case the packet number is that long.
    let mut header = ci[0..pn_offset + 4].to_vec();

    // Sample for masking and apply the mask.
    let sample_start = pn_offset + 4;
    let sample_end = sample_start + 16;
    let mask = hp.mask(&ci[sample_start..sample_end]).unwrap();
    header[0] ^= mask[0] & 0xf;
    let pn_len = 1 + usize::from(header[0] & 0x3);
    for i in 0..pn_len {
        header[pn_offset + i] ^= mask[1 + i];
    }
    // Trim down to size.
    header.truncate(pn_offset + pn_len);
    dec.skip(pn_len);
    payload_len -= pn_len;

    // Decrypt.
    // The packet number should be 1.
    let pn = Decoder::new(&header[pn_offset..])
        .decode_uint(pn_len)
        .unwrap();
    assert_eq!(pn, 1);
    let mut plaintext_buf = Vec::with_capacity(ci.len());
    plaintext_buf.resize_with(ci.len(), u8::default);
    let input = dec.decode(payload_len).unwrap();
    let plaintext = aead
        .decrypt(pn, &header, input, &mut plaintext_buf)
        .unwrap();

    // Now re-encode without the token.
    let mut enc = Encoder::with_capacity(header.len());
    enc.encode(&header[..5])
        .encode_vec(1, &dcid)
        .encode_vec(1, &scid)
        .encode_vvec(&[])
        .encode_varint(u64::try_from(payload_len + pn_len).unwrap());
    let pn_offset = enc.len();
    let notoken_header = enc.encode_uint(pn_len, pn).to_vec();
    qtrace!("notoken_header={}", hex(&notoken_header));

    // Encrypt.
    let mut notoken_packet = Encoder::with_capacity(1200)
        .encode(&notoken_header)
        .to_vec();
    notoken_packet.resize_with(1200, u8::default);
    aead.encrypt(
        pn,
        &notoken_header,
        plaintext,
        &mut notoken_packet[notoken_header.len()..],
    )
    .unwrap();
    // Unlike with decryption, don't truncate.
    // All 1200 bytes are needed to reach the minimum datagram size.

    // Mask header[0] and packet number.
    let sample_start = pn_offset + 4;
    let sample_end = sample_start + 16;
    let mask = hp.mask(&notoken_packet[sample_start..sample_end]).unwrap();
    qtrace!(
        "sample={} mask={}",
        hex(&notoken_packet[sample_start..sample_end]),
        hex(&mask)
    );
    notoken_packet[0] ^= mask[0] & 0xf;
    for i in 0..pn_len {
        notoken_packet[pn_offset + i] ^= mask[1 + i];
    }
    qtrace!("packet={}", hex(&notoken_packet));

    let new_datagram = Datagram::new(
        client_initial2.source(),
        client_initial2.destination(),
        notoken_packet,
    );
    qdebug!("passing modified Initial to the main server");
    let dgram = server.process(Some(new_datagram), now()).dgram();
    assert!(dgram.is_some());

    let dgram = client.process(dgram, now()).dgram(); // Generate an ACK.
    assert!(dgram.is_some());
    let dgram = server.process(dgram, now()).dgram();
    assert!(dgram.is_none());
    assert!(test_fixture::maybe_authenticate(&mut client));
    let dgram = client.process(dgram, now()).dgram();
    assert!(dgram.is_some()); // Client sending CLOSE_CONNECTIONs
    assert!(matches!(
        *client.state(),
        State::Closing{
            error: ConnectionError::Transport(Error::InvalidRetry),
            ..
        }
    ));
}

#[test]
fn version_negotiation() {
    let mut server = default_server();
    let mut client = default_client();

    // Any packet will do, but let's make something that looks real.
    let dgram = client.process(None, now()).dgram().expect("a datagram");
    let mut input = dgram.to_vec();
    input[1] ^= 0x12;
    let damaged = Datagram::new(dgram.source(), dgram.destination(), input.clone());
    let vn = server.process(Some(damaged), now()).dgram();

    let mut dec = Decoder::from(&input[5..]); // Skip past version.
    let dcid = dec.decode_vec(1).expect("client DCID").to_vec();
    let scid = dec.decode_vec(1).expect("client SCID").to_vec();

    // We should have received a VN packet.
    let vn = vn.expect("a vn packet");
    let mut dec = Decoder::from(&vn[1..]); // Skip first byte.

    assert_eq!(dec.decode_uint(4).expect("VN"), 0);
    assert_eq!(dec.decode_vec(1).expect("VN DCID"), &scid[..]);
    assert_eq!(dec.decode_vec(1).expect("VN SCID"), &dcid[..]);
    let mut found = false;
    while dec.remaining() > 0 {
        let v = dec.decode_uint(4).expect("supported version");
        found |= v == u64::from(QUIC_VERSION);
    }
    assert!(found, "valid version not found");

    let res = client.process(Some(vn), now());
    assert_eq!(res, Output::None);
    match client.state() {
        State::Closed(err) => {
            assert_eq!(*err, ConnectionError::Transport(Error::VersionNegotiation))
        }
        _ => panic!("Invalid client state"),
    }
}

#[test]
fn closed() {
    // Let a server connection idle and it should be removed.
    let mut server = default_server();
    let mut client = default_client();
    connect(&mut client, &mut server);

    let res = server.process(None, now());
    assert_eq!(res, Output::Callback(Duration::from_secs(60)));

    qtrace!("60s later");
    let res = server.process(None, now() + Duration::from_secs(60));
    assert_eq!(res, Output::None);
}
