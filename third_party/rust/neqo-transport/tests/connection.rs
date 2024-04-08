// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

mod common;

use common::{
    apply_header_protection, decode_initial_header, initial_aead_and_hp, remove_header_protection,
};
use neqo_common::{Datagram, Decoder, Encoder, Role};
use neqo_transport::{ConnectionError, ConnectionParameters, Error, State, Version};
use test_fixture::{default_client, default_server, new_client, now, split_datagram};

#[test]
fn connect() {
    let (_client, _server) = test_fixture::connect();
}

#[test]
fn truncate_long_packet() {
    let mut client = default_client();
    let mut server = default_server();

    let out = client.process(None, now());
    assert!(out.as_dgram_ref().is_some());
    let out = server.process(out.as_dgram_ref(), now());
    assert!(out.as_dgram_ref().is_some());

    // This will truncate the Handshake packet from the server.
    let dupe = out.as_dgram_ref().unwrap().clone();
    // Count the padding in the packet, plus 1.
    let tail = dupe.iter().rev().take_while(|b| **b == 0).count() + 1;
    let truncated = Datagram::new(
        dupe.source(),
        dupe.destination(),
        dupe.tos(),
        dupe.ttl(),
        &dupe[..(dupe.len() - tail)],
    );
    let hs_probe = client.process(Some(&truncated), now()).dgram();
    assert!(hs_probe.is_some());

    // Now feed in the untruncated packet.
    let out = client.process(out.as_dgram_ref(), now());
    assert!(out.as_dgram_ref().is_some()); // Throw this ACK away.
    assert!(test_fixture::maybe_authenticate(&mut client));
    let out = client.process(None, now());
    assert!(out.as_dgram_ref().is_some());

    assert!(client.state().connected());
    let out = server.process(out.as_dgram_ref(), now());
    assert!(out.as_dgram_ref().is_some());
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

    let client_initial = client.process_output(now());
    let (_, client_dcid, _, _) =
        decode_initial_header(client_initial.as_dgram_ref().unwrap(), Role::Client);
    let client_dcid = client_dcid.to_owned();

    let server_packet = server.process(client_initial.as_dgram_ref(), now()).dgram();
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
        server_initial.tos(),
        server_initial.ttl(),
        packet,
    );

    // Now a connection can be made successfully.
    // Though we modified the server's Initial packet, we get away with it.
    // TLS only authenticates the content of the CRYPTO frame, which was untouched.
    client.process_input(&reordered, now());
    client.process_input(&server_hs.unwrap(), now());
    assert!(test_fixture::maybe_authenticate(&mut client));
    let finished = client.process_output(now());
    assert_eq!(*client.state(), State::Connected);

    let done = server.process(finished.as_dgram_ref(), now());
    assert_eq!(*server.state(), State::Confirmed);

    client.process_input(done.as_dgram_ref().unwrap(), now());
    assert_eq!(*client.state(), State::Confirmed);
}

fn set_payload(server_packet: &Option<Datagram>, client_dcid: &[u8], payload: &[u8]) -> Datagram {
    let (server_initial, _server_hs) = split_datagram(server_packet.as_ref().unwrap());
    let (protected_header, _, _, orig_payload) =
        decode_initial_header(&server_initial, Role::Server);

    // Now decrypt the packet.
    let (aead, hp) = initial_aead_and_hp(client_dcid, Role::Server);
    let (mut header, pn) = remove_header_protection(&hp, protected_header, orig_payload);
    assert_eq!(pn, 0);
    // Re-encode the packet number as four bytes, so we have enough material for the header
    // protection sample if payload is empty.
    let pn_pos = header.len() - 2;
    header[pn_pos] = u8::try_from(4 + aead.expansion()).unwrap();
    header.resize(header.len() + 3, 0);
    header[0] |= 0b0000_0011; // Set the packet number length to 4.

    // And build a packet containing the given payload.
    let mut packet = header.clone();
    packet.resize(header.len() + payload.len() + aead.expansion(), 0);
    aead.encrypt(pn, &header, payload, &mut packet[header.len()..])
        .unwrap();
    apply_header_protection(&hp, &mut packet, protected_header.len()..header.len());
    Datagram::new(
        server_initial.source(),
        server_initial.destination(),
        server_initial.tos(),
        server_initial.ttl(),
        packet,
    )
}

/// Test that the stack treats a packet without any frames as a protocol violation.
#[test]
fn packet_without_frames() {
    let mut client = new_client(
        ConnectionParameters::default().versions(Version::Version1, vec![Version::Version1]),
    );
    let mut server = default_server();

    let client_initial = client.process_output(now());
    let (_, client_dcid, _, _) =
        decode_initial_header(client_initial.as_dgram_ref().unwrap(), Role::Client);

    let server_packet = server.process(client_initial.as_dgram_ref(), now()).dgram();
    let modified = set_payload(&server_packet, client_dcid, &[]);
    client.process_input(&modified, now());
    assert_eq!(
        client.state(),
        &State::Closed(ConnectionError::Transport(Error::ProtocolViolation))
    );
}

/// Test that the stack permits a packet containing only padding.
#[test]
fn packet_with_only_padding() {
    let mut client = new_client(
        ConnectionParameters::default().versions(Version::Version1, vec![Version::Version1]),
    );
    let mut server = default_server();

    let client_initial = client.process_output(now());
    let (_, client_dcid, _, _) =
        decode_initial_header(client_initial.as_dgram_ref().unwrap(), Role::Client);

    let server_packet = server.process(client_initial.as_dgram_ref(), now()).dgram();
    let modified = set_payload(&server_packet, client_dcid, &[0]);
    client.process_input(&modified, now());
    assert_eq!(client.state(), &State::WaitInitial);
}

/// Overflow the crypto buffer.
#[allow(clippy::similar_names)] // For ..._scid and ..._dcid, which are fine.
#[test]
fn overflow_crypto() {
    let mut client = new_client(
        ConnectionParameters::default().versions(Version::Version1, vec![Version::Version1]),
    );
    let mut server = default_server();

    let client_initial = client.process_output(now()).dgram();
    let (_, client_dcid, _, _) =
        decode_initial_header(client_initial.as_ref().unwrap(), Role::Client);
    let client_dcid = client_dcid.to_owned();

    let server_packet = server.process(client_initial.as_ref(), now()).dgram();
    let (server_initial, _) = split_datagram(server_packet.as_ref().unwrap());

    // Now decrypt the server packet to get AEAD and HP instances.
    // We won't be using the packet, but making new ones.
    let (aead, hp) = initial_aead_and_hp(&client_dcid, Role::Server);
    let (_, server_dcid, server_scid, _) = decode_initial_header(&server_initial, Role::Server);

    // Send in 100 packets, each with 1000 bytes of crypto frame data each,
    // eventually this will overrun the buffer we keep for crypto data.
    let mut payload = Encoder::with_capacity(1024);
    for pn in 0..100_u64 {
        payload.truncate(0);
        payload
            .encode_varint(0x06_u64) // CRYPTO frame type.
            .encode_varint(pn * 1000 + 1) // offset
            .encode_varint(1000_u64); // length
        let plen = payload.len();
        payload.pad_to(plen + 1000, 44);

        let mut packet = Encoder::with_capacity(1200);
        packet
            .encode_byte(0xc1) // Initial with packet number length of 2.
            .encode_uint(4, Version::Version1.wire_version())
            .encode_vec(1, server_dcid)
            .encode_vec(1, server_scid)
            .encode_vvec(&[]) // token
            .encode_varint(u64::try_from(2 + payload.len() + aead.expansion()).unwrap()); // length
        let pn_offset = packet.len();
        packet.encode_uint(2, pn);

        let mut packet = Vec::from(packet);
        let header = packet.clone();
        packet.resize(header.len() + payload.len() + aead.expansion(), 0);
        aead.encrypt(pn, &header, payload.as_ref(), &mut packet[header.len()..])
            .unwrap();
        apply_header_protection(&hp, &mut packet, pn_offset..(pn_offset + 2));
        packet.resize(1200, 0); // Initial has to be 1200 bytes!

        let dgram = Datagram::new(
            server_initial.source(),
            server_initial.destination(),
            server_initial.tos(),
            server_initial.ttl(),
            packet,
        );
        client.process_input(&dgram, now());
        if let State::Closing { error, .. } = client.state() {
            assert!(
                matches!(
                    error,
                    ConnectionError::Transport(Error::CryptoBufferExceeded),
                ),
                "the connection need to abort on crypto buffer"
            );
            assert!(pn > 64, "at least 64000 bytes of data is buffered");
            return;
        }
    }
    panic!("Was not able to overflow the crypto buffer");
}
