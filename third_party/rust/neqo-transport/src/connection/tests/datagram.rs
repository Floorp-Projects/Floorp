// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::{
    assert_error, connect_force_idle, default_client, default_server, new_client, new_server,
    AT_LEAST_PTO,
};
use crate::events::{ConnectionEvent, OutgoingDatagramOutcome};
use crate::frame::FRAME_TYPE_DATAGRAM;
use crate::packet::PacketBuilder;
use crate::quic_datagrams::MAX_QUIC_DATAGRAM;
use crate::{Connection, ConnectionError, ConnectionParameters, Error};
use neqo_common::event::Provider;
use std::convert::TryFrom;
use test_fixture::now;

const DATAGRAM_LEN_MTU: u64 = 1310;
const DATA_MTU: &[u8] = &[1; 1310];
const DATA_BIGGER_THAN_MTU: &[u8] = &[0; 2620];
const DATAGRAM_LEN_SMALLER_THAN_MTU: u64 = 1200;
const DATA_SMALLER_THAN_MTU: &[u8] = &[0; 1200];
const DATA_SMALLER_THAN_MTU_2: &[u8] = &[0; 600];
const OUTGOING_QUEUE: usize = 2;

struct InsertDatagram<'a> {
    data: &'a [u8],
}

impl crate::connection::test_internal::FrameWriter for InsertDatagram<'_> {
    fn write_frames(&mut self, builder: &mut PacketBuilder) {
        builder.encode_varint(FRAME_TYPE_DATAGRAM);
        builder.encode(self.data);
    }
}

#[test]
fn datagram_disabled_both() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    assert_eq!(client.max_datagram_size(), Err(Error::NotAvailable));
    assert_eq!(server.max_datagram_size(), Err(Error::NotAvailable));
    assert_eq!(
        client.send_datagram(DATA_SMALLER_THAN_MTU, None),
        Err(Error::TooMuchData)
    );
    assert_eq!(server.stats().frame_tx.datagram, 0);
    assert_eq!(
        server.send_datagram(DATA_SMALLER_THAN_MTU, None),
        Err(Error::TooMuchData)
    );
    assert_eq!(server.stats().frame_tx.datagram, 0);
}

#[test]
fn datagram_enabled_on_client() {
    let mut client =
        new_client(ConnectionParameters::default().datagram_size(DATAGRAM_LEN_SMALLER_THAN_MTU));
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    assert_eq!(client.max_datagram_size(), Err(Error::NotAvailable));
    assert_eq!(
        server.max_datagram_size(),
        Ok(DATAGRAM_LEN_SMALLER_THAN_MTU)
    );
    assert_eq!(
        client.send_datagram(DATA_SMALLER_THAN_MTU, Some(1)),
        Err(Error::TooMuchData)
    );
    let dgram_sent = server.stats().frame_tx.datagram;
    assert_eq!(server.send_datagram(DATA_SMALLER_THAN_MTU, Some(1)), Ok(()));
    let out = server.process_output(now()).dgram().unwrap();
    assert_eq!(server.stats().frame_tx.datagram, dgram_sent + 1);

    client.process_input(out, now());
    assert!(matches!(
        client.next_event().unwrap(),
        ConnectionEvent::Datagram(data) if data == DATA_SMALLER_THAN_MTU
    ));
}

#[test]
fn datagram_enabled_on_server() {
    let mut client = default_client();
    let mut server =
        new_server(ConnectionParameters::default().datagram_size(DATAGRAM_LEN_SMALLER_THAN_MTU));
    connect_force_idle(&mut client, &mut server);

    assert_eq!(
        client.max_datagram_size(),
        Ok(DATAGRAM_LEN_SMALLER_THAN_MTU)
    );
    assert_eq!(server.max_datagram_size(), Err(Error::NotAvailable));
    assert_eq!(
        server.send_datagram(&DATA_SMALLER_THAN_MTU, Some(1)),
        Err(Error::TooMuchData)
    );
    let dgram_sent = client.stats().frame_tx.datagram;
    assert_eq!(client.send_datagram(DATA_SMALLER_THAN_MTU, Some(1)), Ok(()));
    let out = client.process_output(now()).dgram().unwrap();
    assert_eq!(client.stats().frame_tx.datagram, dgram_sent + 1);

    server.process_input(out, now());
    assert!(matches!(
        server.next_event().unwrap(),
        ConnectionEvent::Datagram(data) if data == DATA_SMALLER_THAN_MTU
    ));
}

fn connect_datagram() -> (Connection, Connection) {
    let mut client = new_client(
        ConnectionParameters::default()
            .datagram_size(MAX_QUIC_DATAGRAM)
            .outgoing_datagram_queue(OUTGOING_QUEUE),
    );
    let mut server = new_server(ConnectionParameters::default().datagram_size(MAX_QUIC_DATAGRAM));
    connect_force_idle(&mut client, &mut server);
    (client, server)
}

#[test]
fn mtu_limit() {
    let (client, server) = connect_datagram();

    assert_eq!(client.max_datagram_size(), Ok(DATAGRAM_LEN_MTU));
    assert_eq!(server.max_datagram_size(), Ok(DATAGRAM_LEN_MTU));
}

#[test]
fn limit_data_size() {
    let (mut client, mut server) = connect_datagram();

    assert!(u64::try_from(DATA_BIGGER_THAN_MTU.len()).unwrap() > DATAGRAM_LEN_MTU);
    // Datagram can be queued because they are smaller than allowed by the peer,
    // but they cannot be sent.
    assert_eq!(server.send_datagram(DATA_BIGGER_THAN_MTU, Some(1)), Ok(()));

    let dgram_sent_s = server.stats().frame_tx.datagram;
    assert!(server.process_output(now()).dgram().is_none());
    assert_eq!(server.stats().frame_tx.datagram, dgram_sent_s);
    assert!(matches!(
        server.next_event().unwrap(),
        ConnectionEvent::OutgoingDatagramOutcome { id, outcome } if id == 1 && outcome == OutgoingDatagramOutcome::DroppedTooBig
    ));

    // The same test for the client side.
    assert_eq!(client.send_datagram(DATA_BIGGER_THAN_MTU, Some(1)), Ok(()));
    let dgram_sent_c = client.stats().frame_tx.datagram;
    assert!(client.process_output(now()).dgram().is_none());
    assert_eq!(client.stats().frame_tx.datagram, dgram_sent_c);
    assert!(matches!(
        client.next_event().unwrap(),
        ConnectionEvent::OutgoingDatagramOutcome { id, outcome } if id == 1 && outcome == OutgoingDatagramOutcome::DroppedTooBig
    ));
}

#[test]
fn after_dgram_dropped_continue_writing_frames() {
    let (mut client, _) = connect_datagram();

    assert!(u64::try_from(DATA_BIGGER_THAN_MTU.len()).unwrap() > DATAGRAM_LEN_MTU);
    // Datagram can be queued because they are smaller than allowed by the peer,
    // but they cannot be sent.
    assert_eq!(client.send_datagram(DATA_BIGGER_THAN_MTU, Some(1)), Ok(()));
    assert_eq!(client.send_datagram(DATA_SMALLER_THAN_MTU, Some(2)), Ok(()));

    let datagram_dropped = |e| {
        matches!(
        e,
        ConnectionEvent::OutgoingDatagramOutcome { id, outcome } if id == 1 && outcome == OutgoingDatagramOutcome::DroppedTooBig)
    };

    let dgram_sent_c = client.stats().frame_tx.datagram;
    assert!(client.process_output(now()).dgram().is_some());
    assert_eq!(client.stats().frame_tx.datagram, dgram_sent_c + 1);
    assert!(client.events().any(datagram_dropped));
}

#[test]
fn datagram_acked() {
    let (mut client, mut server) = connect_datagram();

    let dgram_sent = client.stats().frame_tx.datagram;
    assert_eq!(client.send_datagram(DATA_SMALLER_THAN_MTU, Some(1)), Ok(()));
    let out = client.process_output(now()).dgram();
    assert_eq!(client.stats().frame_tx.datagram, dgram_sent + 1);

    let dgram_received = server.stats().frame_rx.datagram;
    server.process_input(out.unwrap(), now());
    assert_eq!(server.stats().frame_rx.datagram, dgram_received + 1);
    let now = now() + AT_LEAST_PTO;
    // Ack should be sent
    let ack_sent = server.stats().frame_tx.ack;
    let out = server.process_output(now).dgram();
    assert_eq!(server.stats().frame_tx.ack, ack_sent + 1);

    assert!(matches!(
        server.next_event().unwrap(),
        ConnectionEvent::Datagram(data) if data == DATA_SMALLER_THAN_MTU
    ));

    client.process_input(out.unwrap(), now);
    assert!(matches!(
        client.next_event().unwrap(),
        ConnectionEvent::OutgoingDatagramOutcome { id, outcome } if id == 1 && outcome == OutgoingDatagramOutcome::Acked
    ));
}

#[test]
fn datagram_lost() {
    let (mut client, _) = connect_datagram();

    let dgram_sent = client.stats().frame_tx.datagram;
    assert_eq!(client.send_datagram(DATA_SMALLER_THAN_MTU, Some(1)), Ok(()));
    let _out = client.process_output(now()).dgram(); // This packet will be lost.
    assert_eq!(client.stats().frame_tx.datagram, dgram_sent + 1);

    // Wait for PTO
    let now = now() + AT_LEAST_PTO;
    let dgram_sent = client.stats().frame_tx.datagram;
    let pings_sent = client.stats().frame_tx.ping;
    let out = client.process_output(now).dgram();
    assert!(out.is_some()); //PING probing
                            // Datagram is not sent again.
    assert_eq!(client.stats().frame_tx.ping, pings_sent + 1);
    assert_eq!(client.stats().frame_tx.datagram, dgram_sent);

    assert!(matches!(
        client.next_event().unwrap(),
        ConnectionEvent::OutgoingDatagramOutcome { id, outcome } if id == 1 && outcome == OutgoingDatagramOutcome::Lost
    ));
}

#[test]
fn datagram_sent_once() {
    let (mut client, _) = connect_datagram();

    let dgram_sent = client.stats().frame_tx.datagram;
    assert_eq!(client.send_datagram(DATA_SMALLER_THAN_MTU, Some(1)), Ok(()));
    let _out = client.process_output(now()).dgram();
    assert_eq!(client.stats().frame_tx.datagram, dgram_sent + 1);

    // Call process_output again should not send any new Datagram.
    let dgram_sent = client.stats().frame_tx.datagram;
    assert!(client.process_output(now()).dgram().is_none());
    assert_eq!(client.stats().frame_tx.datagram, dgram_sent);
}

#[test]
fn dgram_no_allowed() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);
    server.test_frame_writer = Some(Box::new(InsertDatagram { data: DATA_MTU }));
    let out = server.process_output(now()).dgram().unwrap();
    server.test_frame_writer = None;

    client.process_input(out, now());

    assert_error(
        &client,
        &ConnectionError::Transport(Error::ProtocolViolation),
    );
}

#[test]
fn dgram_too_big() {
    let mut client =
        new_client(ConnectionParameters::default().datagram_size(DATAGRAM_LEN_SMALLER_THAN_MTU));
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    assert!(DATAGRAM_LEN_MTU > DATAGRAM_LEN_SMALLER_THAN_MTU);
    server.test_frame_writer = Some(Box::new(InsertDatagram { data: DATA_MTU }));
    let out = server.process_output(now()).dgram().unwrap();
    server.test_frame_writer = None;

    client.process_input(out, now());

    assert_error(
        &client,
        &ConnectionError::Transport(Error::ProtocolViolation),
    );
}

#[test]
fn outgoing_datagram_queue_full() {
    let (mut client, mut server) = connect_datagram();

    let dgram_sent = client.stats().frame_tx.datagram;
    assert_eq!(client.send_datagram(DATA_SMALLER_THAN_MTU, Some(1)), Ok(()));
    assert_eq!(
        client.send_datagram(DATA_SMALLER_THAN_MTU_2, Some(2)),
        Ok(())
    );
    // The outgoing datagram queue limit is 2, therefore the datagram with id 1
    // will be dropped after adding one more datagram.
    assert_eq!(client.send_datagram(DATA_MTU, Some(3)), Ok(()));
    assert!(matches!(
        client.next_event().unwrap(),
        ConnectionEvent::OutgoingDatagramOutcome { id, outcome } if id == 1 && outcome == OutgoingDatagramOutcome::DroppedQueueFull
    ));

    // Send DATA_SMALLER_THAN_MTU_2 datagram
    let out = client.process_output(now()).dgram();
    assert_eq!(client.stats().frame_tx.datagram, dgram_sent + 1);
    server.process_input(out.unwrap(), now());
    assert!(matches!(
        server.next_event().unwrap(),
        ConnectionEvent::Datagram(data) if data == DATA_SMALLER_THAN_MTU_2
    ));

    // Send DATA_SMALLER_THAN_MTU_2 datagram
    let dgram_sent = client.stats().frame_tx.datagram;
    let out = client.process_output(now()).dgram();
    assert_eq!(client.stats().frame_tx.datagram, dgram_sent + 1);
    server.process_input(out.unwrap(), now());
    assert!(matches!(
        server.next_event().unwrap(),
        ConnectionEvent::Datagram(data) if data == DATA_MTU
    ));
}

fn send_datagram(client: &mut Connection, server: &mut Connection, data: &[u8]) {
    let dgram_sent = server.stats().frame_tx.datagram;
    assert_eq!(server.send_datagram(data, Some(1)), Ok(()));
    let out = server.process_output(now()).dgram().unwrap();
    assert_eq!(server.stats().frame_tx.datagram, dgram_sent + 1);

    let dgram_received = client.stats().frame_rx.datagram;
    client.process_input(out, now());
    assert_eq!(client.stats().frame_rx.datagram, dgram_received + 1);
}

#[test]
fn multiple_datagram_events() {
    const DATA_SIZE: usize = 1200;
    const MAX_QUEUE: usize = 3;
    const FIRST_DATAGRAM: &[u8] = &[0; DATA_SIZE];
    const SECOND_DATAGRAM: &[u8] = &[1; DATA_SIZE];
    const THIRD_DATAGRAM: &[u8] = &[2; DATA_SIZE];
    const FOURTH_DATAGRAM: &[u8] = &[3; DATA_SIZE];

    let mut client = new_client(
        ConnectionParameters::default()
            .datagram_size(u64::try_from(DATA_SIZE).unwrap())
            .incoming_datagram_queue(MAX_QUEUE),
    );
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    send_datagram(&mut client, &mut server, FIRST_DATAGRAM);
    send_datagram(&mut client, &mut server, SECOND_DATAGRAM);
    send_datagram(&mut client, &mut server, THIRD_DATAGRAM);

    let mut datagrams = client.events().filter_map(|evt| {
        if let ConnectionEvent::Datagram(d) = evt {
            Some(d)
        } else {
            None
        }
    });
    assert_eq!(datagrams.next().unwrap(), FIRST_DATAGRAM);
    assert_eq!(datagrams.next().unwrap(), SECOND_DATAGRAM);
    assert_eq!(datagrams.next().unwrap(), THIRD_DATAGRAM);
    assert!(datagrams.next().is_none());

    // New events can be queued.
    send_datagram(&mut client, &mut server, FOURTH_DATAGRAM);
    let mut datagrams = client.events().filter_map(|evt| {
        if let ConnectionEvent::Datagram(d) = evt {
            Some(d)
        } else {
            None
        }
    });
    assert_eq!(datagrams.next().unwrap(), FOURTH_DATAGRAM);
    assert!(datagrams.next().is_none())
}

#[test]
fn too_many_datagram_events() {
    const DATA_SIZE: usize = 1200;
    const MAX_QUEUE: usize = 2;
    const FIRST_DATAGRAM: &[u8] = &[0; DATA_SIZE];
    const SECOND_DATAGRAM: &[u8] = &[1; DATA_SIZE];
    const THIRD_DATAGRAM: &[u8] = &[2; DATA_SIZE];
    const FOURTH_DATAGRAM: &[u8] = &[3; DATA_SIZE];

    let mut client = new_client(
        ConnectionParameters::default()
            .datagram_size(u64::try_from(DATA_SIZE).unwrap())
            .incoming_datagram_queue(MAX_QUEUE),
    );
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    send_datagram(&mut client, &mut server, FIRST_DATAGRAM);
    send_datagram(&mut client, &mut server, SECOND_DATAGRAM);
    send_datagram(&mut client, &mut server, THIRD_DATAGRAM);

    // Datagram with FIRST_DATAGRAM data will be dropped.
    assert!(matches!(
        client.next_event().unwrap(),
        ConnectionEvent::Datagram(data) if data == SECOND_DATAGRAM
    ));
    assert!(matches!(
        client.next_event().unwrap(),
        ConnectionEvent::IncomingDatagramDropped
    ));
    assert!(matches!(
        client.next_event().unwrap(),
        ConnectionEvent::Datagram(data) if data == THIRD_DATAGRAM
    ));
    assert!(client.next_event().is_none());

    // New events can be queued.
    send_datagram(&mut client, &mut server, FOURTH_DATAGRAM);
    assert!(matches!(
        client.next_event().unwrap(),
        ConnectionEvent::Datagram(data) if data == FOURTH_DATAGRAM
    ));
    assert!(client.next_event().is_none());
}

#[test]
fn multiple_quic_datagrams_in_one_packet() {
    let (mut client, mut server) = connect_datagram();

    let dgram_sent = client.stats().frame_tx.datagram;
    // Enqueue 2 datagrams that can fit in a single packet.
    assert_eq!(
        client.send_datagram(DATA_SMALLER_THAN_MTU_2, Some(1)),
        Ok(())
    );
    assert_eq!(
        client.send_datagram(DATA_SMALLER_THAN_MTU_2, Some(2)),
        Ok(())
    );

    let out = client.process_output(now()).dgram();
    assert_eq!(client.stats().frame_tx.datagram, dgram_sent + 2);
    server.process_input(out.unwrap(), now());
    let datagram = |e: &_| matches!(e, ConnectionEvent::Datagram(..));
    assert_eq!(server.events().filter(datagram).count(), 2);
}
