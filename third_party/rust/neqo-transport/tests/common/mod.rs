// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(feature = "deny-warnings", deny(warnings))]
#![warn(clippy::pedantic)]
#![allow(unused)]

use neqo_common::{event::Provider, hex_with_len, qtrace, Datagram, Decoder, Role};
use neqo_crypto::{
    constants::{TLS_AES_128_GCM_SHA256, TLS_VERSION_1_3},
    hkdf,
    hp::HpKey,
    Aead, AllowZeroRtt, AuthenticationStatus, ResumptionToken,
};
use neqo_transport::{
    server::{ActiveConnectionRef, Server, ValidateAddress},
    Connection, ConnectionEvent, ConnectionParameters, State,
};
use test_fixture::{self, default_client, now, CountingConnectionIdGenerator};

use std::cell::RefCell;
use std::convert::TryFrom;
use std::mem;
use std::ops::Range;
use std::rc::Rc;

/// Create a server.  This is different than the one in the fixture, which is a single connection.
pub fn new_server(params: ConnectionParameters) -> Server {
    Server::new(
        now(),
        test_fixture::DEFAULT_KEYS,
        test_fixture::DEFAULT_ALPN,
        test_fixture::anti_replay(),
        Box::new(AllowZeroRtt {}),
        Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
        params,
    )
    .expect("should create a server")
}

/// Create a server.  This is different than the one in the fixture, which is a single connection.
pub fn default_server() -> Server {
    new_server(ConnectionParameters::default())
}

// Check that there is at least one connection.  Returns a ref to the first confirmed connection.
pub fn connected_server(server: &mut Server) -> ActiveConnectionRef {
    let server_connections = server.active_connections();
    // Find confirmed connections.  There should only be one.
    let mut confirmed = server_connections
        .iter()
        .filter(|c: &&ActiveConnectionRef| *c.borrow().state() == State::Confirmed);
    let c = confirmed.next().expect("one confirmed");
    assert!(confirmed.next().is_none(), "only one confirmed");
    c.clone()
}

/// Connect.  This returns a reference to the server connection.
pub fn connect(client: &mut Connection, server: &mut Server) -> ActiveConnectionRef {
    server.set_validation(ValidateAddress::Never);

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
    assert!(dgram.is_some()); // ACK + HANDSHAKE_DONE + NST

    // Have the client process the HANDSHAKE_DONE.
    let dgram = client.process(dgram, now()).dgram();
    assert!(dgram.is_none());
    assert_eq!(*client.state(), State::Confirmed);

    connected_server(server)
}

// Decode the header of a client Initial packet, returning three values:
// * the entire header short of the packet number,
// * just the DCID,
// * just the SCID, and
// * the protected payload including the packet number.
// Any token is thrown away.
#[must_use]
pub fn decode_initial_header(dgram: &Datagram, role: Role) -> (&[u8], &[u8], &[u8], &[u8]) {
    let mut dec = Decoder::new(&dgram[..]);
    let type_and_ver = dec.decode(5).unwrap().to_vec();
    // The client sets the QUIC bit, the server might not.
    match role {
        Role::Client => assert_eq!(type_and_ver[0] & 0xf0, 0xc0),
        Role::Server => assert_eq!(type_and_ver[0] & 0xb0, 0x80),
    }
    let dest_cid = dec.decode_vec(1).unwrap();
    let src_cid = dec.decode_vec(1).unwrap();
    dec.skip_vvec(); // Ignore any the token.

    // Need to read of the length separately so that we can find the packet number.
    let payload_len = usize::try_from(dec.decode_varint().unwrap()).unwrap();
    let pn_offset = dgram.len() - dec.remaining();
    (
        &dgram[..pn_offset],
        dest_cid,
        src_cid,
        dec.decode(payload_len).unwrap(),
    )
}

/// Generate an AEAD and header protection object for a client Initial.
/// Note that this works for QUIC version 1 only.
#[must_use]
pub fn initial_aead_and_hp(dcid: &[u8], role: Role) -> (Aead, HpKey) {
    const INITIAL_SALT: &[u8] = &[
        0x38, 0x76, 0x2c, 0xf7, 0xf5, 0x59, 0x34, 0xb3, 0x4d, 0x17, 0x9a, 0xe6, 0xa4, 0xc8, 0x0c,
        0xad, 0xcc, 0xbb, 0x7f, 0x0a,
    ];
    let initial_secret = hkdf::extract(
        TLS_VERSION_1_3,
        TLS_AES_128_GCM_SHA256,
        Some(
            hkdf::import_key(TLS_VERSION_1_3, INITIAL_SALT)
                .as_ref()
                .unwrap(),
        ),
        hkdf::import_key(TLS_VERSION_1_3, dcid).as_ref().unwrap(),
    )
    .unwrap();

    let secret = hkdf::expand_label(
        TLS_VERSION_1_3,
        TLS_AES_128_GCM_SHA256,
        &initial_secret,
        &[],
        match role {
            Role::Client => "client in",
            Role::Server => "server in",
        },
    )
    .unwrap();
    (
        Aead::new(
            false,
            TLS_VERSION_1_3,
            TLS_AES_128_GCM_SHA256,
            &secret,
            "quic ",
        )
        .unwrap(),
        HpKey::extract(TLS_VERSION_1_3, TLS_AES_128_GCM_SHA256, &secret, "quic hp").unwrap(),
    )
}

// Remove header protection, returning the unmasked header and the packet number.
#[must_use]
pub fn remove_header_protection(hp: &HpKey, header: &[u8], payload: &[u8]) -> (Vec<u8>, u64) {
    // Make a copy of the header that can be modified.
    let mut fixed_header = header.to_vec();
    let pn_offset = header.len();
    // Save 4 extra in case the packet number is that long.
    fixed_header.extend_from_slice(&payload[..4]);

    // Sample for masking and apply the mask.
    let mask = hp.mask(&payload[4..20]).unwrap();
    fixed_header[0] ^= mask[0] & 0xf;
    let pn_len = 1 + usize::from(fixed_header[0] & 0x3);
    for i in 0..pn_len {
        fixed_header[pn_offset + i] ^= mask[1 + i];
    }
    // Trim down to size.
    fixed_header.truncate(pn_offset + pn_len);
    // The packet number should be 1.
    let pn = Decoder::new(&fixed_header[pn_offset..])
        .decode_uint(pn_len)
        .unwrap();

    (fixed_header, pn)
}

pub fn apply_header_protection(hp: &HpKey, packet: &mut [u8], pn_bytes: Range<usize>) {
    let sample_start = pn_bytes.start + 4;
    let sample_end = sample_start + 16;
    let mask = hp.mask(&packet[sample_start..sample_end]).unwrap();
    qtrace!(
        "sample={} mask={}",
        hex_with_len(&packet[sample_start..sample_end]),
        hex_with_len(&mask)
    );
    packet[0] ^= mask[0] & 0xf;
    for i in 0..(pn_bytes.end - pn_bytes.start) {
        packet[pn_bytes.start + i] ^= mask[1 + i];
    }
}

/// Scrub through client events to find a resumption token.
pub fn find_ticket(client: &mut Connection) -> ResumptionToken {
    client
        .events()
        .find_map(|e| {
            if let ConnectionEvent::ResumptionToken(token) = e {
                Some(token)
            } else {
                None
            }
        })
        .unwrap()
}

/// Connect to the server and have it generate a ticket.
pub fn generate_ticket(server: &mut Server) -> ResumptionToken {
    let mut client = default_client();
    let mut server_conn = connect(&mut client, server);

    server_conn.borrow_mut().send_ticket(now(), &[]).unwrap();
    let dgram = server.process(None, now()).dgram();
    client.process_input(dgram.unwrap(), now()); // Consume ticket, ignore output.
    let ticket = find_ticket(&mut client);

    // Have the client close the connection and then let the server clean up.
    client.close(now(), 0, "got a ticket");
    let dgram = client.process_output(now()).dgram();
    mem::drop(server.process(dgram, now()));
    // Calling active_connections clears the set of active connections.
    assert_eq!(server.active_connections().len(), 1);
    ticket
}
