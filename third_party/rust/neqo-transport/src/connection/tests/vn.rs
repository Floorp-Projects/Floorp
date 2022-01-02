// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::super::{ConnectionError, Output, State};
use super::{default_client, default_server};
use crate::packet::PACKET_BIT_LONG;
use crate::{Error, QuicVersion};

use neqo_common::{Datagram, Decoder, Encoder};
use std::mem;
use std::time::Duration;
use test_fixture::{self, addr, now};

// The expected PTO duration after the first Initial is sent.
const INITIAL_PTO: Duration = Duration::from_millis(300);

#[test]
fn unknown_version() {
    let mut client = default_client();
    // Start the handshake.
    mem::drop(client.process(None, now()).dgram());

    let mut unknown_version_packet = vec![0x80, 0x1a, 0x1a, 0x1a, 0x1a];
    unknown_version_packet.resize(1200, 0x0);
    mem::drop(client.process(
        Some(Datagram::new(addr(), addr(), unknown_version_packet)),
        now(),
    ));
    assert_eq!(1, client.stats().dropped_rx);
}

#[test]
fn server_receive_unknown_first_packet() {
    let mut server = default_server();

    let mut unknown_version_packet = vec![0x80, 0x1a, 0x1a, 0x1a, 0x1a];
    unknown_version_packet.resize(1200, 0x0);

    assert_eq!(
        server.process(
            Some(Datagram::new(addr(), addr(), unknown_version_packet,)),
            now(),
        ),
        Output::None
    );

    assert_eq!(1, server.stats().dropped_rx);
}

fn create_vn(initial_pkt: &[u8], versions: &[u32]) -> Vec<u8> {
    let mut dec = Decoder::from(&initial_pkt[5..]); // Skip past version.
    let dst_cid = dec.decode_vec(1).expect("client DCID");
    let src_cid = dec.decode_vec(1).expect("client SCID");

    let mut encoder = Encoder::default();
    encoder.encode_byte(PACKET_BIT_LONG);
    encoder.encode(&[0; 4]); // Zero version == VN.
    encoder.encode_vec(1, dst_cid);
    encoder.encode_vec(1, src_cid);

    for v in versions {
        encoder.encode_uint(4, *v);
    }
    encoder.into()
}

#[test]
fn version_negotiation_current_version() {
    let mut client = default_client();
    // Start the handshake.
    let initial_pkt = client
        .process(None, now())
        .dgram()
        .expect("a datagram")
        .to_vec();

    let vn = create_vn(
        &initial_pkt,
        &[0x1a1a_1a1a, QuicVersion::default().as_u32()],
    );

    let dgram = Datagram::new(addr(), addr(), vn);
    let delay = client.process(Some(dgram), now()).callback();
    assert_eq!(delay, INITIAL_PTO);
    assert_eq!(*client.state(), State::WaitInitial);
    assert_eq!(1, client.stats().dropped_rx);
}

#[test]
fn version_negotiation_only_reserved() {
    let mut client = default_client();
    // Start the handshake.
    let initial_pkt = client
        .process(None, now())
        .dgram()
        .expect("a datagram")
        .to_vec();

    let vn = create_vn(&initial_pkt, &[0x1a1a_1a1a, 0x2a2a_2a2a]);

    let dgram = Datagram::new(addr(), addr(), vn);
    assert_eq!(client.process(Some(dgram), now()), Output::None);
    match client.state() {
        State::Closed(err) => {
            assert_eq!(*err, ConnectionError::Transport(Error::VersionNegotiation));
        }
        _ => panic!("Invalid client state"),
    }
}

#[test]
fn version_negotiation_corrupted() {
    let mut client = default_client();
    // Start the handshake.
    let initial_pkt = client
        .process(None, now())
        .dgram()
        .expect("a datagram")
        .to_vec();

    let vn = create_vn(&initial_pkt, &[0x1a1a_1a1a, 0x2a2a_2a2a]);

    let dgram = Datagram::new(addr(), addr(), &vn[..vn.len() - 1]);
    let delay = client.process(Some(dgram), now()).callback();
    assert_eq!(delay, INITIAL_PTO);
    assert_eq!(*client.state(), State::WaitInitial);
    assert_eq!(1, client.stats().dropped_rx);
}

#[test]
fn version_negotiation_empty() {
    let mut client = default_client();
    // Start the handshake.
    let initial_pkt = client
        .process(None, now())
        .dgram()
        .expect("a datagram")
        .to_vec();

    let vn = create_vn(&initial_pkt, &[]);

    let dgram = Datagram::new(addr(), addr(), vn);
    let delay = client.process(Some(dgram), now()).callback();
    assert_eq!(delay, INITIAL_PTO);
    assert_eq!(*client.state(), State::WaitInitial);
    assert_eq!(1, client.stats().dropped_rx);
}

#[test]
fn version_negotiation_not_supported() {
    let mut client = default_client();
    // Start the handshake.
    let initial_pkt = client
        .process(None, now())
        .dgram()
        .expect("a datagram")
        .to_vec();

    let vn = create_vn(&initial_pkt, &[0x1a1a_1a1a, 0x2a2a_2a2a, 0xff00_0001]);

    assert_eq!(
        client.process(Some(Datagram::new(addr(), addr(), vn)), now(),),
        Output::None
    );
    match client.state() {
        State::Closed(err) => {
            assert_eq!(*err, ConnectionError::Transport(Error::VersionNegotiation));
        }
        _ => panic!("Invalid client state"),
    }
}

#[test]
fn version_negotiation_bad_cid() {
    let mut client = default_client();
    // Start the handshake.
    let initial_pkt = client
        .process(None, now())
        .dgram()
        .expect("a datagram")
        .to_vec();

    let mut vn = create_vn(&initial_pkt, &[0x1a1a_1a1a, 0x2a2a_2a2a, 0xff00_0001]);
    vn[6] ^= 0xc4;

    let dgram = Datagram::new(addr(), addr(), vn);
    let delay = client.process(Some(dgram), now()).callback();
    assert_eq!(delay, INITIAL_PTO);
    assert_eq!(*client.state(), State::WaitInitial);
    assert_eq!(1, client.stats().dropped_rx);
}
