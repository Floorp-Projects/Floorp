// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(feature = "deny-warnings", deny(warnings))]
#![warn(clippy::pedantic)]
#![cfg(not(feature = "fuzzing"))]

mod common;

use common::{
    apply_header_protection, client_initial_aead_and_hp, connected_server, decode_initial_header,
    default_server, get_ticket, remove_header_protection,
};
use neqo_common::{hex_with_len, qdebug, qtrace, Datagram, Encoder};
use neqo_crypto::AuthenticationStatus;
use neqo_transport::{server::ValidateAddress, ConnectionError, Error, State, StreamType};
use std::convert::TryFrom;
use std::net::{IpAddr, Ipv4Addr, SocketAddr};
use std::time::Duration;
use test_fixture::{self, addr, assertions, default_client, now, split_datagram};

#[test]
fn retry_basic() {
    let mut server = default_server();
    server.set_validation(ValidateAddress::Always);
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

#[test]
fn retry_expired() {
    let mut server = default_server();
    server.set_validation(ValidateAddress::Always);
    let mut client = default_client();
    let mut now = now();

    let dgram = client.process(None, now).dgram(); // Initial
    assert!(dgram.is_some());
    let dgram = server.process(dgram, now).dgram(); // Retry
    assert!(dgram.is_some());

    assertions::assert_retry(&dgram.as_ref().unwrap());

    let dgram = client.process(dgram, now).dgram(); // Initial w/token
    assert!(dgram.is_some());

    now += Duration::from_secs(60); // Too long for Retry.
    let dgram = server.process(dgram, now).dgram(); // Initial, HS
    assert!(dgram.is_none());
}

// Attempt a retry with 0-RTT, and have 0-RTT packets sent with the second ClientHello.
#[test]
fn retry_0rtt() {
    let mut server = default_server();
    let token = get_ticket(&mut server);
    server.set_validation(ValidateAddress::Always);

    let mut client = default_client();
    client.enable_resumption(now(), &token).unwrap();

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
    server.set_validation(ValidateAddress::Always);
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
fn new_token_different_ip() {
    let mut server = default_server();
    let token = get_ticket(&mut server);
    server.set_validation(ValidateAddress::NoToken);

    let mut client = default_client();
    client.enable_resumption(now(), &token).unwrap();

    let dgram = client.process(None, now()).dgram(); // Initial
    assert!(dgram.is_some());
    assertions::assert_initial(dgram.as_ref().unwrap(), true);

    // Now rewrite the source address.
    let d = dgram.unwrap();
    let src = SocketAddr::new(IpAddr::V4(Ipv4Addr::new(127, 0, 0, 2)), d.source().port());
    let dgram = Some(Datagram::new(src, d.destination(), &d[..]));
    let dgram = server.process(dgram, now()).dgram(); // Retry
    assert!(dgram.is_some());
    assertions::assert_retry(dgram.as_ref().unwrap());
}

#[test]
fn new_token_expired() {
    let mut server = default_server();
    let token = get_ticket(&mut server);
    server.set_validation(ValidateAddress::NoToken);

    let mut client = default_client();
    client.enable_resumption(now(), &token).unwrap();

    let dgram = client.process(None, now()).dgram(); // Initial
    assert!(dgram.is_some());
    assertions::assert_initial(dgram.as_ref().unwrap(), true);

    // Now move into the future.
    // We can't go too far or we'll overflow our field.  Not when checking,
    // but when trying to generate another Retry.  A month is fine.
    let the_future = now() + Duration::from_secs(60 * 60 * 24 * 30);
    let d = dgram.unwrap();
    let src = SocketAddr::new(d.source().ip(), d.source().port() + 1);
    let dgram = Some(Datagram::new(src, d.destination(), &d[..]));
    let dgram = server.process(dgram, the_future).dgram(); // Retry
    assert!(dgram.is_some());
    assertions::assert_retry(dgram.as_ref().unwrap());
}

#[test]
fn retry_after_initial() {
    let mut server = default_server();
    let mut retry_server = default_server();
    retry_server.set_validation(ValidateAddress::Always);
    let mut client = default_client();

    let cinit = client.process(None, now()).dgram(); // Initial
    assert!(cinit.is_some());
    let server_flight = server.process(cinit.clone(), now()).dgram(); // Initial
    assert!(server_flight.is_some());

    // We need to have the client just process the Initial.
    let (server_initial, _other) = split_datagram(server_flight.as_ref().unwrap());
    let dgram = client.process(Some(server_initial), now()).dgram();
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
fn retry_bad_integrity() {
    let mut server = default_server();
    server.set_validation(ValidateAddress::Always);
    let mut client = default_client();

    let dgram = client.process(None, now()).dgram(); // Initial
    assert!(dgram.is_some());
    let dgram = server.process(dgram, now()).dgram(); // Retry
    assert!(dgram.is_some());

    let retry = &dgram.as_ref().unwrap();
    assertions::assert_retry(retry);

    let mut tweaked = retry.to_vec();
    tweaked[retry.len() - 1] ^= 0x45; // damage the auth tag
    let tweaked_packet = Datagram::new(retry.source(), retry.destination(), tweaked);

    // The client should ignore this packet.
    let dgram = client.process(Some(tweaked_packet), now()).dgram();
    assert!(dgram.is_none());
}

#[test]
fn retry_bad_token() {
    let mut client = default_client();
    let mut retry_server = default_server();
    retry_server.set_validation(ValidateAddress::Always);
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

// This is really a client test, but we need a server with Retry to test it.
// In this test, the client sends Initial on PTO.  The Retry should cause
// all loss recovery timers to be reset, but we had a bug where the PTO timer
// was not properly reset.  This tests that the client generates a new Initial
// in response to receiving a Retry, even after it sends the Initial on PTO.
#[test]
fn retry_after_pto() {
    let mut client = default_client();
    let mut server = default_server();
    server.set_validation(ValidateAddress::Always);
    let mut now = now();

    let ci = client.process(None, now).dgram();
    assert!(ci.is_some()); // sit on this for a bit.RefCell

    // Let PTO fire on the client and then let it exhaust its PTO packets.
    now += Duration::from_secs(1);
    let pto1 = client.process(None, now).dgram();
    assert!(pto1.unwrap().len() >= 1200);
    let pto2 = client.process(None, now).dgram();
    assert!(pto2.unwrap().len() >= 1200);
    let cb = client.process(None, now).callback();
    assert_ne!(cb, Duration::new(0, 0));

    let retry = server.process(ci, now).dgram();
    assertions::assert_retry(&retry.as_ref().unwrap());

    let ci2 = client.process(retry, now).dgram();
    assert!(ci2.unwrap().len() >= 1200);
}

#[test]
fn vn_after_retry() {
    let mut server = default_server();
    server.set_validation(ValidateAddress::Always);
    let mut client = default_client();

    let dgram = client.process(None, now()).dgram(); // Initial
    assert!(dgram.is_some());
    let dgram = server.process(dgram, now()).dgram(); // Retry
    assert!(dgram.is_some());

    assertions::assert_retry(&dgram.as_ref().unwrap());

    let dgram = client.process(dgram, now()).dgram(); // Initial w/token
    assert!(dgram.is_some());

    let mut encoder = Encoder::default();
    encoder.encode_byte(0x80);
    encoder.encode(&[0; 4]); // Zero version == VN.
    encoder.encode_vec(1, &client.odcid().unwrap()[..]);
    encoder.encode_vec(1, &[]);
    encoder.encode_uint(4, 0x5a5a_6a6a_u64);
    let vn = Datagram::new(addr(), addr(), encoder);

    assert_ne!(
        client.process(Some(vn), now()).callback(),
        Duration::from_secs(0)
    );
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
#[allow(clippy::shadow_unrelated)]
fn mitm_retry() {
    let mut client = default_client();
    let mut retry_server = default_server();
    retry_server.set_validation(ValidateAddress::Always);
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
    let (protected_header, d_cid, s_cid, payload) = decode_initial_header(&client_initial2);

    // Now we have enough information to make keys.
    let (aead, hp) = client_initial_aead_and_hp(&d_cid);
    let (header, pn) = remove_header_protection(&hp, protected_header, payload);
    let pn_len = header.len() - protected_header.len();

    // Decrypt.
    assert_eq!(pn, 1);
    let mut plaintext_buf = vec![0; client_initial2.len()];
    let plaintext = aead
        .decrypt(pn, &header, &payload[pn_len..], &mut plaintext_buf)
        .unwrap();

    // Now re-encode without the token.
    let mut enc = Encoder::with_capacity(header.len());
    enc.encode(&header[..5])
        .encode_vec(1, d_cid)
        .encode_vec(1, s_cid)
        .encode_vvec(&[])
        .encode_varint(u64::try_from(payload.len()).unwrap());
    let pn_offset = enc.len();
    let notoken_header = enc.encode_uint(pn_len, pn).to_vec();
    qtrace!("notoken_header={}", hex_with_len(&notoken_header));

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

    apply_header_protection(&hp, &mut notoken_packet, pn_offset..(pn_offset + pn_len));
    qtrace!("packet={}", hex_with_len(&notoken_packet));

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
        State::Closing {
            error: ConnectionError::Transport(Error::ProtocolViolation),
            ..
        }
    ));
}
