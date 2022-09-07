// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(feature = "deny-warnings", deny(warnings))]
#![warn(clippy::use_self)]

mod common;

use common::{
    apply_header_protection, decode_initial_header, initial_aead_and_hp, remove_header_protection,
};
use neqo_common::{Datagram, Decoder, Role};
use neqo_transport::{ConnectionParameters, State, Version};
use test_fixture::{self, default_client, default_server, new_client, now, split_datagram};

#[test]
fn connect() {
    let (_client, _server) = test_fixture::connect();
}

#[test]
fn truncate_long_packet() {
    let mut client = default_client();
    let mut server = default_server();

    let dgram = client.process(None, now()).dgram();
    assert!(dgram.is_some());
    let dgram = server.process(dgram, now()).dgram();
    assert!(dgram.is_some());

    // This will truncate the Handshake packet from the server.
    let dupe = dgram.as_ref().unwrap().clone();
    // Count the padding in the packet, plus 1.
    let tail = dupe.iter().rev().take_while(|b| **b == 0).count() + 1;
    let truncated = Datagram::new(
        dupe.source(),
        dupe.destination(),
        &dupe[..(dupe.len() - tail)],
    );
    let hs_probe = client.process(Some(truncated), now()).dgram();
    assert!(hs_probe.is_some());

    // Now feed in the untruncated packet.
    let dgram = client.process(dgram, now()).dgram();
    assert!(dgram.is_some()); // Throw this ACK away.
    assert!(test_fixture::maybe_authenticate(&mut client));
    let dgram = client.process(None, now()).dgram();
    assert!(dgram.is_some());

    assert!(client.state().connected());
    let dgram = server.process(dgram, now()).dgram();
    assert!(dgram.is_some());
    assert!(server.state().connected());
}

/// Test that reordering parts of the server Initial doesn't change things.
#[test]
fn reorder_server_initial() {
    // A simple ACK frame for a single packet with packet number 0.
    const ACK_FRAME: &[u8] = &[0x02, 0x00, 0x00, 0x00, 0x00];

    let mut client = new_client(
        ConnectionParameters::default().versions(Version::Version1, vec![Version::Version1]),
    );
    let mut server = default_server();

    let client_initial = client.process_output(now()).dgram();
    let (_, client_dcid, _, _) =
        decode_initial_header(client_initial.as_ref().unwrap(), Role::Client);
    let client_dcid = client_dcid.to_owned();

    let server_packet = server.process(client_initial, now()).dgram();
    let (server_initial, server_hs) = split_datagram(server_packet.as_ref().unwrap());
    let (protected_header, _, _, payload) = decode_initial_header(&server_initial, Role::Server);

    // Now decrypt the packet.
    let (aead, hp) = initial_aead_and_hp(&client_dcid, Role::Server);
    let (header, pn) = remove_header_protection(&hp, protected_header, payload);
    assert_eq!(pn, 0);
    let pn_len = header.len() - protected_header.len();
    let mut buf = vec![0; payload.len()];
    let mut plaintext = aead
        .decrypt(pn, &header, &payload[pn_len..], &mut buf)
        .unwrap()
        .to_owned();

    // Now we need to find the frames.  Make some really strong assumptions.
    let mut dec = Decoder::new(&plaintext[..]);
    assert_eq!(dec.decode(ACK_FRAME.len()), Some(ACK_FRAME));
    assert_eq!(dec.decode_varint(), Some(0x06)); // CRYPTO
    assert_eq!(dec.decode_varint(), Some(0x00)); // offset
    dec.skip_vvec(); // Skip over the payload.
    let end = dec.offset();

    // Move the ACK frame after the CRYPTO frame.
    plaintext[..end].rotate_left(ACK_FRAME.len());

    // And rebuild a packet.
    let mut packet = header.clone();
    packet.resize(1200, 0);
    aead.encrypt(pn, &header, &plaintext, &mut packet[header.len()..])
        .unwrap();
    apply_header_protection(&hp, &mut packet, protected_header.len()..header.len());
    let reordered = Datagram::new(
        server_initial.source(),
        server_initial.destination(),
        packet,
    );

    // Now a connection can be made successfully.
    // Though we modified the server's Initial packet, we get away with it.
    // TLS only authenticates the content of the CRYPTO frame, which was untouched.
    client.process_input(reordered, now());
    client.process_input(server_hs.unwrap(), now());
    assert!(test_fixture::maybe_authenticate(&mut client));
    let finished = client.process_output(now()).dgram();
    assert_eq!(*client.state(), State::Connected);

    let done = server.process(finished, now()).dgram();
    assert_eq!(*server.state(), State::Confirmed);

    client.process_input(done.unwrap(), now());
    assert_eq!(*client.state(), State::Confirmed);
}
