// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::super::{Connection, Output, State};
use super::{
    assert_error, connect, connect_force_idle, connect_with_rtt, default_client, default_server,
    get_tokens, handshake, maybe_authenticate, send_something, CountingConnectionIdGenerator,
    AT_LEAST_PTO, DEFAULT_RTT, DEFAULT_STREAM_DATA,
};
use crate::connection::AddressValidation;
use crate::events::ConnectionEvent;
use crate::path::PATH_MTU_V6;
use crate::server::ValidateAddress;
use crate::tparams::{TransportParameter, MIN_ACK_DELAY};
use crate::tracking::DEFAULT_ACK_DELAY;
use crate::{
    ConnectionError, ConnectionParameters, EmptyConnectionIdGenerator, Error, QuicVersion,
    StreamType,
};

use neqo_common::{event::Provider, qdebug, Datagram};
use neqo_crypto::{
    constants::TLS_CHACHA20_POLY1305_SHA256, generate_ech_keys, AuthenticationStatus,
};
use std::cell::RefCell;
use std::convert::TryFrom;
use std::mem;
use std::net::{IpAddr, Ipv6Addr, SocketAddr};
use std::rc::Rc;
use std::time::Duration;
use test_fixture::{self, addr, assertions, fixture_init, now, split_datagram};

const ECH_CONFIG_ID: u8 = 7;
const ECH_PUBLIC_NAME: &str = "public.example";

#[test]
fn full_handshake() {
    qdebug!("---- client: generate CH");
    let mut client = default_client();
    let out = client.process(None, now());
    assert!(out.as_dgram_ref().is_some());
    assert_eq!(out.as_dgram_ref().unwrap().len(), PATH_MTU_V6);

    qdebug!("---- server: CH -> SH, EE, CERT, CV, FIN");
    let mut server = default_server();
    let out = server.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_some());
    assert_eq!(out.as_dgram_ref().unwrap().len(), PATH_MTU_V6);

    qdebug!("---- client: cert verification");
    let out = client.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_some());

    let out = server.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_none());

    assert!(maybe_authenticate(&mut client));

    qdebug!("---- client: SH..FIN -> FIN");
    let out = client.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_some());
    assert_eq!(*client.state(), State::Connected);

    qdebug!("---- server: FIN -> ACKS");
    let out = server.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_some());
    assert_eq!(*server.state(), State::Confirmed);

    qdebug!("---- client: ACKS -> 0");
    let out = client.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_none());
    assert_eq!(*client.state(), State::Confirmed);
}

#[test]
fn handshake_failed_authentication() {
    qdebug!("---- client: generate CH");
    let mut client = default_client();
    let out = client.process(None, now());
    assert!(out.as_dgram_ref().is_some());

    qdebug!("---- server: CH -> SH, EE, CERT, CV, FIN");
    let mut server = default_server();
    let out = server.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_some());

    qdebug!("---- client: cert verification");
    let out = client.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_some());

    let out = server.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_none());

    let authentication_needed = |e| matches!(e, ConnectionEvent::AuthenticationNeeded);
    assert!(client.events().any(authentication_needed));
    qdebug!("---- client: Alert(certificate_revoked)");
    client.authenticated(AuthenticationStatus::CertRevoked, now());

    qdebug!("---- client: -> Alert(certificate_revoked)");
    let out = client.process(None, now());
    assert!(out.as_dgram_ref().is_some());

    qdebug!("---- server: Alert(certificate_revoked)");
    let out = server.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_some());
    assert_error(&client, &ConnectionError::Transport(Error::CryptoAlert(44)));
    assert_error(&server, &ConnectionError::Transport(Error::PeerError(300)));
}

#[test]
fn no_alpn() {
    fixture_init();
    let mut client = Connection::new_client(
        "example.com",
        &["bad-alpn"],
        Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
        addr(),
        addr(),
        ConnectionParameters::default(),
        now(),
    )
    .unwrap();
    let mut server = default_server();

    handshake(&mut client, &mut server, now(), Duration::new(0, 0));
    // TODO (mt): errors are immediate, which means that we never send CONNECTION_CLOSE
    // and the client never sees the server's rejection of its handshake.
    //assert_error(&client, ConnectionError::Transport(Error::CryptoAlert(120)));
    assert_error(
        &server,
        &ConnectionError::Transport(Error::CryptoAlert(120)),
    );
}

#[test]
fn dup_server_flight1() {
    qdebug!("---- client: generate CH");
    let mut client = default_client();
    let out = client.process(None, now());
    assert!(out.as_dgram_ref().is_some());
    assert_eq!(out.as_dgram_ref().unwrap().len(), PATH_MTU_V6);
    qdebug!("Output={:0x?}", out.as_dgram_ref());

    qdebug!("---- server: CH -> SH, EE, CERT, CV, FIN");
    let mut server = default_server();
    let out_to_rep = server.process(out.dgram(), now());
    assert!(out_to_rep.as_dgram_ref().is_some());
    qdebug!("Output={:0x?}", out_to_rep.as_dgram_ref());

    qdebug!("---- client: cert verification");
    let out = client.process(Some(out_to_rep.as_dgram_ref().unwrap().clone()), now());
    assert!(out.as_dgram_ref().is_some());
    qdebug!("Output={:0x?}", out.as_dgram_ref());

    let out = server.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_none());

    assert!(maybe_authenticate(&mut client));

    qdebug!("---- client: SH..FIN -> FIN");
    let out = client.process(None, now());
    assert!(out.as_dgram_ref().is_some());
    qdebug!("Output={:0x?}", out.as_dgram_ref());

    assert_eq!(3, client.stats().packets_rx);
    assert_eq!(0, client.stats().dups_rx);
    assert_eq!(1, client.stats().dropped_rx);

    qdebug!("---- Dup, ignored");
    let out = client.process(out_to_rep.dgram(), now());
    assert!(out.as_dgram_ref().is_none());
    qdebug!("Output={:0x?}", out.as_dgram_ref());

    // Four packets total received, 1 of them is a dup and one has been dropped because Initial keys
    // are dropped.  Add 2 counts of the padding that the server adds to Initial packets.
    assert_eq!(6, client.stats().packets_rx);
    assert_eq!(1, client.stats().dups_rx);
    assert_eq!(3, client.stats().dropped_rx);
}

// Test that we split crypto data if they cannot fit into one packet.
// To test this we will use a long server certificate.
#[test]
fn crypto_frame_split() {
    let mut client = default_client();

    let mut server = Connection::new_server(
        test_fixture::LONG_CERT_KEYS,
        test_fixture::DEFAULT_ALPN,
        Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
        ConnectionParameters::default(),
    )
    .expect("create a server");

    let client1 = client.process(None, now());
    assert!(client1.as_dgram_ref().is_some());

    // The entire server flight doesn't fit in a single packet because the
    // certificate is large, therefore the server will produce 2 packets.
    let server1 = server.process(client1.dgram(), now());
    assert!(server1.as_dgram_ref().is_some());
    let server2 = server.process(None, now());
    assert!(server2.as_dgram_ref().is_some());

    let client2 = client.process(server1.dgram(), now());
    // This is an ack.
    assert!(client2.as_dgram_ref().is_some());
    // The client might have the certificate now, so we can't guarantee that
    // this will work.
    let auth1 = maybe_authenticate(&mut client);
    assert_eq!(*client.state(), State::Handshaking);

    // let server process the ack for the first packet.
    let server3 = server.process(client2.dgram(), now());
    assert!(server3.as_dgram_ref().is_none());

    // Consume the second packet from the server.
    let client3 = client.process(server2.dgram(), now());

    // Check authentication.
    let auth2 = maybe_authenticate(&mut client);
    assert!(auth1 ^ auth2);
    // Now client has all data to finish handshake.
    assert_eq!(*client.state(), State::Connected);

    let client4 = client.process(server3.dgram(), now());
    // One of these will contain data depending on whether Authentication was completed
    // after the first or second server packet.
    assert!(client3.as_dgram_ref().is_some() ^ client4.as_dgram_ref().is_some());

    mem::drop(server.process(client3.dgram(), now()));
    mem::drop(server.process(client4.dgram(), now()));

    assert_eq!(*client.state(), State::Connected);
    assert_eq!(*server.state(), State::Confirmed);
}

/// Run a single ChaCha20-Poly1305 test and get a PTO.
#[test]
fn chacha20poly1305() {
    let mut server = default_server();
    let mut client = Connection::new_client(
        test_fixture::DEFAULT_SERVER_NAME,
        test_fixture::DEFAULT_ALPN,
        Rc::new(RefCell::new(EmptyConnectionIdGenerator::default())),
        addr(),
        addr(),
        ConnectionParameters::default(),
        now(),
    )
    .expect("create a default client");
    client.set_ciphers(&[TLS_CHACHA20_POLY1305_SHA256]).unwrap();
    connect_force_idle(&mut client, &mut server);
}

/// Test that a server can send 0.5 RTT application data.
#[test]
fn send_05rtt() {
    let mut client = default_client();
    let mut server = default_server();

    let c1 = client.process(None, now()).dgram();
    assert!(c1.is_some());
    let s1 = server.process(c1, now()).dgram().unwrap();
    assert_eq!(s1.len(), PATH_MTU_V6);

    // The server should accept writes at this point.
    let s2 = send_something(&mut server, now());

    // Complete the handshake at the client.
    client.process_input(s1, now());
    maybe_authenticate(&mut client);
    assert_eq!(*client.state(), State::Connected);

    // The client should receive the 0.5-RTT data now.
    client.process_input(s2, now());
    let mut buf = vec![0; DEFAULT_STREAM_DATA.len() + 1];
    let stream_id = client
        .events()
        .find_map(|e| {
            if let ConnectionEvent::RecvStreamReadable { stream_id } = e {
                Some(stream_id)
            } else {
                None
            }
        })
        .unwrap();
    let (l, ended) = client.stream_recv(stream_id, &mut buf).unwrap();
    assert_eq!(&buf[..l], DEFAULT_STREAM_DATA);
    assert!(ended);
}

/// Test that a client buffers 0.5-RTT data when it arrives early.
#[test]
fn reorder_05rtt() {
    let mut client = default_client();
    let mut server = default_server();

    let c1 = client.process(None, now()).dgram();
    assert!(c1.is_some());
    let s1 = server.process(c1, now()).dgram().unwrap();

    // The server should accept writes at this point.
    let s2 = send_something(&mut server, now());

    // We can't use the standard facility to complete the handshake, so
    // drive it as aggressively as possible.
    client.process_input(s2, now());
    assert_eq!(client.stats().saved_datagrams, 1);

    // After processing the first packet, the client should go back and
    // process the 0.5-RTT packet data, which should make data available.
    client.process_input(s1, now());
    // We can't use `maybe_authenticate` here as that consumes events.
    client.authenticated(AuthenticationStatus::Ok, now());
    assert_eq!(*client.state(), State::Connected);

    let mut buf = vec![0; DEFAULT_STREAM_DATA.len() + 1];
    let stream_id = client
        .events()
        .find_map(|e| {
            if let ConnectionEvent::RecvStreamReadable { stream_id } = e {
                Some(stream_id)
            } else {
                None
            }
        })
        .unwrap();
    let (l, ended) = client.stream_recv(stream_id, &mut buf).unwrap();
    assert_eq!(&buf[..l], DEFAULT_STREAM_DATA);
    assert!(ended);
}

#[test]
fn reorder_05rtt_with_0rtt() {
    const RTT: Duration = Duration::from_millis(100);

    let mut client = default_client();
    let mut server = default_server();
    let validation = AddressValidation::new(now(), ValidateAddress::NoToken).unwrap();
    let validation = Rc::new(RefCell::new(validation));
    server.set_validation(Rc::clone(&validation));
    let mut now = connect_with_rtt(&mut client, &mut server, now(), RTT);

    // Include RTT in sending the ticket or the ticket age reported by the
    // client is wrong, which causes the server to reject 0-RTT.
    now += RTT / 2;
    server.send_ticket(now, &[]).unwrap();
    let ticket = server.process_output(now).dgram().unwrap();
    now += RTT / 2;
    client.process_input(ticket, now);

    let token = get_tokens(&mut client).pop().unwrap();
    let mut client = default_client();
    client.enable_resumption(now, token).unwrap();
    let mut server = default_server();

    // Send ClientHello and some 0-RTT.
    let c1 = send_something(&mut client, now);
    assertions::assert_coalesced_0rtt(&c1[..]);
    // Drop the 0-RTT from the coalesced datagram, so that the server
    // acknowledges the next 0-RTT packet.
    let (c1, _) = split_datagram(&c1);
    let c2 = send_something(&mut client, now);

    // Handle the first packet and send 0.5-RTT in response.  Drop the response.
    now += RTT / 2;
    mem::drop(server.process(Some(c1), now).dgram().unwrap());
    // The gap in 0-RTT will result in this 0.5 RTT containing an ACK.
    server.process_input(c2, now);
    let s2 = send_something(&mut server, now);

    // Save the 0.5 RTT.
    now += RTT / 2;
    client.process_input(s2, now);
    assert_eq!(client.stats().saved_datagrams, 1);

    // Now PTO at the client and cause the server to re-send handshake packets.
    now += AT_LEAST_PTO;
    let c3 = client.process(None, now).dgram();

    now += RTT / 2;
    let s3 = server.process(c3, now).dgram().unwrap();
    assertions::assert_no_1rtt(&s3[..]);

    // The client should be able to process the 0.5 RTT now.
    // This should contain an ACK, so we are processing an ACK from the past.
    now += RTT / 2;
    client.process_input(s3, now);
    maybe_authenticate(&mut client);
    let c4 = client.process(None, now).dgram();
    assert_eq!(*client.state(), State::Connected);
    assert_eq!(client.paths.rtt(), RTT);

    now += RTT / 2;
    server.process_input(c4.unwrap(), now);
    assert_eq!(*server.state(), State::Confirmed);
    assert_eq!(server.paths.rtt(), RTT);
}

/// Test that a server that coalesces 0.5 RTT with handshake packets
/// doesn't cause the client to drop application data.
#[test]
fn coalesce_05rtt() {
    const RTT: Duration = Duration::from_millis(100);
    let mut client = default_client();
    let mut server = default_server();
    let mut now = now();

    // The first exchange doesn't offer a chance for the server to send.
    // So drop the server flight and wait for the PTO.
    let c1 = client.process(None, now).dgram();
    assert!(c1.is_some());
    now += RTT / 2;
    let s1 = server.process(c1, now).dgram();
    assert!(s1.is_some());

    // Drop the server flight.  Then send some data.
    let stream_id = server.stream_create(StreamType::UniDi).unwrap();
    assert!(server.stream_send(stream_id, DEFAULT_STREAM_DATA).is_ok());
    assert!(server.stream_close_send(stream_id).is_ok());

    // Now after a PTO the client can send another packet.
    // The server should then send its entire flight again,
    // including the application data, which it sends in a 1-RTT packet.
    now += AT_LEAST_PTO;
    let c2 = client.process(None, now).dgram();
    assert!(c2.is_some());
    now += RTT / 2;
    let s2 = server.process(c2, now).dgram();
    // Even though there is a 1-RTT packet at the end of the datagram, the
    // flight should be padded to full size.
    assert_eq!(s2.as_ref().unwrap().len(), PATH_MTU_V6);

    // The client should process the datagram.  It can't process the 1-RTT
    // packet until authentication completes though.  So it saves it.
    now += RTT / 2;
    assert_eq!(client.stats().dropped_rx, 0);
    mem::drop(client.process(s2, now).dgram());
    // This packet will contain an ACK, but we can ignore it.
    assert_eq!(client.stats().dropped_rx, 0);
    assert_eq!(client.stats().packets_rx, 3);
    assert_eq!(client.stats().saved_datagrams, 1);

    // After (successful) authentication, the packet is processed.
    maybe_authenticate(&mut client);
    let c3 = client.process(None, now).dgram();
    assert!(c3.is_some());
    assert_eq!(client.stats().dropped_rx, 0); // No Initial padding.
    assert_eq!(client.stats().packets_rx, 4);
    assert_eq!(client.stats().saved_datagrams, 1);
    assert_eq!(client.stats().frame_rx.padding, 1); // Padding uses frames.

    // Allow the handshake to complete.
    now += RTT / 2;
    let s3 = server.process(c3, now).dgram();
    assert!(s3.is_some());
    assert_eq!(*server.state(), State::Confirmed);
    now += RTT / 2;
    mem::drop(client.process(s3, now).dgram());
    assert_eq!(*client.state(), State::Confirmed);

    assert_eq!(client.stats().dropped_rx, 0); // No dropped packets.
}

#[test]
fn reorder_handshake() {
    const RTT: Duration = Duration::from_millis(100);
    let mut client = default_client();
    let mut server = default_server();
    let mut now = now();

    let c1 = client.process(None, now).dgram();
    assert!(c1.is_some());

    now += RTT / 2;
    let s1 = server.process(c1, now).dgram();
    assert!(s1.is_some());

    // Drop the Initial packet from this.
    let (_, s_hs) = split_datagram(&s1.unwrap());
    assert!(s_hs.is_some());

    // Pass just the handshake packet in and the client can't handle it yet.
    // It can only send another Initial packet.
    now += RTT / 2;
    let dgram = client.process(s_hs, now).dgram();
    assertions::assert_initial(dgram.as_ref().unwrap(), false);
    assert_eq!(client.stats().saved_datagrams, 1);
    assert_eq!(client.stats().packets_rx, 1);

    // Get the server to try again.
    // Though we currently allow the server to arm its PTO timer, use
    // a second client Initial packet to cause it to send again.
    now += AT_LEAST_PTO;
    let c2 = client.process(None, now).dgram();
    now += RTT / 2;
    let s2 = server.process(c2, now).dgram();
    assert!(s2.is_some());

    let (s_init, s_hs) = split_datagram(&s2.unwrap());
    assert!(s_hs.is_some());

    // Processing the Handshake packet first should save it.
    now += RTT / 2;
    client.process_input(s_hs.unwrap(), now);
    assert_eq!(client.stats().saved_datagrams, 2);
    assert_eq!(client.stats().packets_rx, 2);

    client.process_input(s_init, now);
    // Each saved packet should now be "received" again.
    assert_eq!(client.stats().packets_rx, 7);
    maybe_authenticate(&mut client);
    let c3 = client.process(None, now).dgram();
    assert!(c3.is_some());

    // Note that though packets were saved and processed very late,
    // they don't cause the RTT to change.
    now += RTT / 2;
    let s3 = server.process(c3, now).dgram();
    assert_eq!(*server.state(), State::Confirmed);
    assert_eq!(server.paths.rtt(), RTT);

    now += RTT / 2;
    client.process_input(s3.unwrap(), now);
    assert_eq!(*client.state(), State::Confirmed);
    assert_eq!(client.paths.rtt(), RTT);
}

#[test]
fn reorder_1rtt() {
    const RTT: Duration = Duration::from_millis(100);
    const PACKETS: usize = 4; // Many, but not enough to overflow cwnd.
    let mut client = default_client();
    let mut server = default_server();
    let mut now = now();

    let c1 = client.process(None, now).dgram();
    assert!(c1.is_some());

    now += RTT / 2;
    let s1 = server.process(c1, now).dgram();
    assert!(s1.is_some());

    now += RTT / 2;
    client.process_input(s1.unwrap(), now);
    maybe_authenticate(&mut client);
    let c2 = client.process(None, now).dgram();
    assert!(c2.is_some());

    // Now get a bunch of packets from the client.
    // Give them to the server before giving it `c2`.
    for _ in 0..PACKETS {
        let d = send_something(&mut client, now);
        server.process_input(d, now + RTT / 2);
    }
    // The server has now received those packets, and saved them.
    // The two extra received are Initial + the junk we use for padding.
    assert_eq!(server.stats().packets_rx, PACKETS + 2);
    assert_eq!(server.stats().saved_datagrams, PACKETS);
    assert_eq!(server.stats().dropped_rx, 1);

    now += RTT / 2;
    let s2 = server.process(c2, now).dgram();
    // The server has now received those packets, and saved them.
    // The three additional are: an Initial ACK, a Handshake,
    // and a 1-RTT (containing NEW_CONNECTION_ID).
    assert_eq!(server.stats().packets_rx, PACKETS * 2 + 5);
    assert_eq!(server.stats().saved_datagrams, PACKETS);
    assert_eq!(server.stats().dropped_rx, 1);
    assert_eq!(*server.state(), State::Confirmed);
    assert_eq!(server.paths.rtt(), RTT);

    now += RTT / 2;
    client.process_input(s2.unwrap(), now);
    assert_eq!(client.paths.rtt(), RTT);

    // All the stream data that was sent should now be available.
    let streams = server
        .events()
        .filter_map(|e| {
            if let ConnectionEvent::RecvStreamReadable { stream_id } = e {
                Some(stream_id)
            } else {
                None
            }
        })
        .collect::<Vec<_>>();
    assert_eq!(streams.len(), PACKETS);
    for stream_id in streams {
        let mut buf = vec![0; DEFAULT_STREAM_DATA.len() + 1];
        let (recvd, fin) = server.stream_recv(stream_id, &mut buf).unwrap();
        assert_eq!(recvd, DEFAULT_STREAM_DATA.len());
        assert!(fin);
    }
}

#[cfg(not(feature = "fuzzing"))]
#[test]
fn corrupted_initial() {
    let mut client = default_client();
    let mut server = default_server();
    let d = client.process(None, now()).dgram().unwrap();
    let mut corrupted = Vec::from(&d[..]);
    // Find the last non-zero value and corrupt that.
    let (idx, _) = corrupted
        .iter()
        .enumerate()
        .rev()
        .find(|(_, &v)| v != 0)
        .unwrap();
    corrupted[idx] ^= 0x76;
    let dgram = Datagram::new(d.source(), d.destination(), corrupted);
    server.process_input(dgram, now());
    // The server should have received two packets,
    // the first should be dropped, the second saved.
    assert_eq!(server.stats().packets_rx, 2);
    assert_eq!(server.stats().dropped_rx, 2);
    assert_eq!(server.stats().saved_datagrams, 0);
}

#[test]
// Absent path PTU discovery, max v6 packet size should be PATH_MTU_V6.
fn verify_pkt_honors_mtu() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    let now = now();

    let res = client.process(None, now);
    let idle_timeout = ConnectionParameters::default().get_idle_timeout();
    assert_eq!(res, Output::Callback(idle_timeout));

    // Try to send a large stream and verify first packet is correctly sized
    let stream_id = client.stream_create(StreamType::UniDi).unwrap();
    assert_eq!(client.stream_send(stream_id, &[0xbb; 2000]).unwrap(), 2000);
    let pkt0 = client.process(None, now);
    assert!(matches!(pkt0, Output::Datagram(_)));
    assert_eq!(pkt0.as_dgram_ref().unwrap().len(), PATH_MTU_V6);
}

#[test]
fn extra_initial_hs() {
    let mut client = default_client();
    let mut server = default_server();
    let mut now = now();

    let c_init = client.process(None, now).dgram();
    assert!(c_init.is_some());
    now += DEFAULT_RTT / 2;
    let s_init = server.process(c_init, now).dgram();
    assert!(s_init.is_some());
    now += DEFAULT_RTT / 2;

    // Drop the Initial packet, keep only the Handshake.
    let (_, undecryptable) = split_datagram(&s_init.unwrap());
    assert!(undecryptable.is_some());

    // Feed the same undecryptable packet into the client a few times.
    // Do that EXTRA_INITIALS times and each time the client will emit
    // another Initial packet.
    for _ in 0..=super::super::EXTRA_INITIALS {
        let c_init = client.process(undecryptable.clone(), now).dgram();
        assertions::assert_initial(c_init.as_ref().unwrap(), false);
        now += DEFAULT_RTT / 10;
    }

    // After EXTRA_INITIALS, the client stops sending Initial packets.
    let nothing = client.process(undecryptable, now).dgram();
    assert!(nothing.is_none());

    // Until PTO, where another Initial can be used to complete the handshake.
    now += AT_LEAST_PTO;
    let c_init = client.process(None, now).dgram();
    assertions::assert_initial(c_init.as_ref().unwrap(), false);
    now += DEFAULT_RTT / 2;
    let s_init = server.process(c_init, now).dgram();
    now += DEFAULT_RTT / 2;
    client.process_input(s_init.unwrap(), now);
    maybe_authenticate(&mut client);
    let c_fin = client.process_output(now).dgram();
    assert_eq!(*client.state(), State::Connected);
    now += DEFAULT_RTT / 2;
    server.process_input(c_fin.unwrap(), now);
    assert_eq!(*server.state(), State::Confirmed);
}

#[test]
fn extra_initial_invalid_cid() {
    let mut client = default_client();
    let mut server = default_server();
    let mut now = now();

    let c_init = client.process(None, now).dgram();
    assert!(c_init.is_some());
    now += DEFAULT_RTT / 2;
    let s_init = server.process(c_init, now).dgram();
    assert!(s_init.is_some());
    now += DEFAULT_RTT / 2;

    // If the client receives a packet that contains the wrong connection
    // ID, it won't send another Initial.
    let (_, hs) = split_datagram(&s_init.unwrap());
    let hs = hs.unwrap();
    let mut copy = hs.to_vec();
    assert_ne!(copy[5], 0); // The DCID should be non-zero length.
    copy[6] ^= 0xc4;
    let dgram_copy = Datagram::new(hs.destination(), hs.source(), copy);
    let nothing = client.process(Some(dgram_copy), now).dgram();
    assert!(nothing.is_none());
}

fn connect_version(version: QuicVersion) {
    fixture_init();
    let mut client = Connection::new_client(
        test_fixture::DEFAULT_SERVER_NAME,
        test_fixture::DEFAULT_ALPN,
        Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
        addr(),
        addr(),
        ConnectionParameters::default().quic_version(version),
        now(),
    )
    .unwrap();
    let mut server = Connection::new_server(
        test_fixture::DEFAULT_KEYS,
        test_fixture::DEFAULT_ALPN,
        Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
        ConnectionParameters::default().quic_version(version),
    )
    .unwrap();
    connect_force_idle(&mut client, &mut server);
}

#[test]
fn connect_v1() {
    connect_version(QuicVersion::Version1);
}

#[test]
fn connect_29() {
    connect_version(QuicVersion::Draft29);
}

#[test]
fn connect_30() {
    connect_version(QuicVersion::Draft30);
}

#[test]
fn connect_31() {
    connect_version(QuicVersion::Draft31);
}

#[test]
fn connect_32() {
    connect_version(QuicVersion::Draft32);
}

#[test]
fn anti_amplification() {
    let mut client = default_client();
    let mut server = default_server();
    let mut now = now();

    // With a gigantic transport parameter, the server is unable to complete
    // the handshake within the amplification limit.
    let very_big = TransportParameter::Bytes(vec![0; PATH_MTU_V6 * 3]);
    server.set_local_tparam(0xce16, very_big).unwrap();

    let c_init = client.process_output(now).dgram();
    now += DEFAULT_RTT / 2;
    let s_init1 = server.process(c_init, now).dgram().unwrap();
    assert_eq!(s_init1.len(), PATH_MTU_V6);
    let s_init2 = server.process_output(now).dgram().unwrap();
    assert_eq!(s_init2.len(), PATH_MTU_V6);

    // Skip the gap for pacing here.
    let s_pacing = server.process_output(now).callback();
    assert_ne!(s_pacing, Duration::new(0, 0));
    now += s_pacing;

    let s_init3 = server.process_output(now).dgram().unwrap();
    assert_eq!(s_init3.len(), PATH_MTU_V6);
    let cb = server.process_output(now).callback();
    assert_ne!(cb, Duration::new(0, 0));

    now += DEFAULT_RTT / 2;
    client.process_input(s_init1, now);
    client.process_input(s_init2, now);
    let ack_count = client.stats().frame_tx.ack;
    let frame_count = client.stats().frame_tx.all;
    let ack = client.process(Some(s_init3), now).dgram().unwrap();
    assert!(!maybe_authenticate(&mut client)); // No need yet.

    // The client sends a padded datagram, with just ACK for Initial + Handshake.
    assert_eq!(client.stats().frame_tx.ack, ack_count + 2);
    assert_eq!(client.stats().frame_tx.all, frame_count + 2);
    assert_ne!(ack.len(), PATH_MTU_V6); // Not padded (it includes Handshake).

    now += DEFAULT_RTT / 2;
    let remainder = server.process(Some(ack), now).dgram();

    now += DEFAULT_RTT / 2;
    client.process_input(remainder.unwrap(), now);
    assert!(maybe_authenticate(&mut client)); // OK, we have all of it.
    let fin = client.process_output(now).dgram();
    assert_eq!(*client.state(), State::Connected);

    now += DEFAULT_RTT / 2;
    server.process_input(fin.unwrap(), now);
    assert_eq!(*server.state(), State::Confirmed);
}

#[cfg(not(feature = "fuzzing"))]
#[test]
fn garbage_initial() {
    let mut client = default_client();
    let mut server = default_server();

    let dgram = client.process_output(now()).dgram().unwrap();
    let (initial, rest) = split_datagram(&dgram);
    let mut corrupted = Vec::from(&initial[..initial.len() - 1]);
    corrupted.push(initial[initial.len() - 1] ^ 0xb7);
    corrupted.extend_from_slice(rest.as_ref().map_or(&[], |r| &r[..]));
    let garbage = Datagram::new(addr(), addr(), corrupted);
    assert_eq!(Output::None, server.process(Some(garbage), now()));
}

#[test]
fn drop_initial_packet_from_wrong_address() {
    let mut client = default_client();
    let out = client.process(None, now());
    assert!(out.as_dgram_ref().is_some());

    let mut server = default_server();
    let out = server.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_some());

    let p = out.dgram().unwrap();
    let dgram = Datagram::new(
        SocketAddr::new(IpAddr::V6(Ipv6Addr::new(0xfe80, 0, 0, 0, 0, 0, 0, 2)), 443),
        p.destination(),
        &p[..],
    );

    let out = client.process(Some(dgram), now());
    assert!(out.as_dgram_ref().is_none());
}

#[test]
fn drop_handshake_packet_from_wrong_address() {
    let mut client = default_client();
    let out = client.process(None, now());
    assert!(out.as_dgram_ref().is_some());

    let mut server = default_server();
    let out = server.process(out.dgram(), now());
    assert!(out.as_dgram_ref().is_some());

    let (s_in, s_hs) = split_datagram(&out.dgram().unwrap());

    // Pass the initial packet.
    mem::drop(client.process(Some(s_in), now()).dgram());

    let p = s_hs.unwrap();
    let dgram = Datagram::new(
        SocketAddr::new(IpAddr::V6(Ipv6Addr::new(0xfe80, 0, 0, 0, 0, 0, 0, 2)), 443),
        p.destination(),
        &p[..],
    );

    let out = client.process(Some(dgram), now());
    assert!(out.as_dgram_ref().is_none());
}

#[test]
fn ech() {
    let mut server = default_server();
    let (sk, pk) = generate_ech_keys().unwrap();
    server
        .server_enable_ech(ECH_CONFIG_ID, ECH_PUBLIC_NAME, &sk, &pk)
        .unwrap();

    let mut client = default_client();
    client.client_enable_ech(server.ech_config()).unwrap();

    connect(&mut client, &mut server);

    assert!(client.tls_info().unwrap().ech_accepted());
    assert!(server.tls_info().unwrap().ech_accepted());
    assert!(client.tls_preinfo().unwrap().ech_accepted().unwrap());
    assert!(server.tls_preinfo().unwrap().ech_accepted().unwrap());
}

fn damaged_ech_config(config: &[u8]) -> Vec<u8> {
    let mut cfg = Vec::from(config);
    // Ensure that the version and config_id is correct.
    assert_eq!(cfg[2], 0xfe);
    assert_eq!(cfg[3], 0x0d);
    assert_eq!(cfg[6], ECH_CONFIG_ID);
    // Change the config_id so that the server doesn't recognize it.
    cfg[6] ^= 0x94;
    cfg
}

#[test]
fn ech_retry() {
    fixture_init();
    let mut server = default_server();
    let (sk, pk) = generate_ech_keys().unwrap();
    server
        .server_enable_ech(ECH_CONFIG_ID, ECH_PUBLIC_NAME, &sk, &pk)
        .unwrap();

    let mut client = default_client();
    client
        .client_enable_ech(&damaged_ech_config(server.ech_config()))
        .unwrap();

    let dgram = client.process_output(now()).dgram();
    let dgram = server.process(dgram, now()).dgram();
    client.process_input(dgram.unwrap(), now());
    let auth_event = ConnectionEvent::EchFallbackAuthenticationNeeded {
        public_name: String::from(ECH_PUBLIC_NAME),
    };
    assert!(client.events().any(|e| e == auth_event));
    client.authenticated(AuthenticationStatus::Ok, now());
    assert!(client.state().error().is_some());

    // Tell the server about the error.
    let dgram = client.process_output(now()).dgram();
    server.process_input(dgram.unwrap(), now());
    assert_eq!(
        server.state().error(),
        Some(&ConnectionError::Transport(Error::PeerError(0x100 + 121)))
    );

    let updated_config =
        if let Some(ConnectionError::Transport(Error::EchRetry(c))) = client.state().error() {
            c
        } else {
            panic!(
                "Client state should be failed with EchRetry, is {:?}",
                client.state()
            );
        };

    let mut server = default_server();
    server
        .server_enable_ech(ECH_CONFIG_ID, ECH_PUBLIC_NAME, &sk, &pk)
        .unwrap();
    let mut client = default_client();
    client.client_enable_ech(updated_config).unwrap();

    connect(&mut client, &mut server);

    assert!(client.tls_info().unwrap().ech_accepted());
    assert!(server.tls_info().unwrap().ech_accepted());
    assert!(client.tls_preinfo().unwrap().ech_accepted().unwrap());
    assert!(server.tls_preinfo().unwrap().ech_accepted().unwrap());
}

#[test]
fn ech_retry_fallback_rejected() {
    fixture_init();
    let mut server = default_server();
    let (sk, pk) = generate_ech_keys().unwrap();
    server
        .server_enable_ech(ECH_CONFIG_ID, ECH_PUBLIC_NAME, &sk, &pk)
        .unwrap();

    let mut client = default_client();
    client
        .client_enable_ech(&damaged_ech_config(server.ech_config()))
        .unwrap();

    let dgram = client.process_output(now()).dgram();
    let dgram = server.process(dgram, now()).dgram();
    client.process_input(dgram.unwrap(), now());
    let auth_event = ConnectionEvent::EchFallbackAuthenticationNeeded {
        public_name: String::from(ECH_PUBLIC_NAME),
    };
    assert!(client.events().any(|e| e == auth_event));
    client.authenticated(AuthenticationStatus::PolicyRejection, now());
    assert!(client.state().error().is_some());

    if let Some(ConnectionError::Transport(Error::EchRetry(_))) = client.state().error() {
        panic!("Client should not get EchRetry error");
    }

    // Pass the error on.
    let dgram = client.process_output(now()).dgram();
    server.process_input(dgram.unwrap(), now());
    assert_eq!(
        server.state().error(),
        Some(&ConnectionError::Transport(Error::PeerError(298)))
    ); // A bad_certificate alert.
}

#[test]
fn bad_min_ack_delay() {
    const EXPECTED_ERROR: ConnectionError =
        ConnectionError::Transport(Error::TransportParameterError);
    let mut server = default_server();
    let max_ad = u64::try_from(DEFAULT_ACK_DELAY.as_micros()).unwrap();
    server
        .set_local_tparam(MIN_ACK_DELAY, TransportParameter::Integer(max_ad + 1))
        .unwrap();
    let mut client = default_client();

    let dgram = client.process_output(now()).dgram();
    let dgram = server.process(dgram, now()).dgram();
    client.process_input(dgram.unwrap(), now());
    client.authenticated(AuthenticationStatus::Ok, now());
    assert_eq!(client.state().error(), Some(&EXPECTED_ERROR));
    let dgram = client.process_output(now()).dgram();

    server.process_input(dgram.unwrap(), now());
    assert_eq!(
        server.state().error(),
        Some(&ConnectionError::Transport(Error::PeerError(
            Error::TransportParameterError.code()
        )))
    );
}
