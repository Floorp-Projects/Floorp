// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::super::super::{ConnectionError, ERROR_AEAD_LIMIT_REACHED};
use super::super::{Connection, ConnectionParameters, Error, Output, State, StreamType};
use super::{
    connect, connect_force_idle, default_client, default_server, maybe_authenticate,
    send_and_receive, send_something, AT_LEAST_PTO,
};
use crate::crypto::{OVERWRITE_INVOCATIONS, UPDATE_WRITE_KEYS_AT};
use crate::packet::PacketNumber;
use crate::path::PATH_MTU_V6;

use neqo_common::{qdebug, Datagram};
use std::mem;
use test_fixture::{self, now};

fn check_discarded(peer: &mut Connection, pkt: Datagram, dropped: usize, dups: usize) {
    // Make sure to flush any saved datagrams before doing this.
    mem::drop(peer.process_output(now()));

    let before = peer.stats();
    let out = peer.process(Some(pkt), now());
    assert!(out.as_dgram_ref().is_none());
    let after = peer.stats();
    assert_eq!(dropped, after.dropped_rx - before.dropped_rx);
    assert_eq!(dups, after.dups_rx - before.dups_rx);
}

fn assert_update_blocked(c: &mut Connection) {
    assert_eq!(
        c.initiate_key_update().unwrap_err(),
        Error::KeyUpdateBlocked
    );
}

fn overwrite_invocations(n: PacketNumber) {
    OVERWRITE_INVOCATIONS.with(|v| {
        *v.borrow_mut() = Some(n);
    });
}

#[test]
fn discarded_initial_keys() {
    qdebug!("---- client: generate CH");
    let mut client = default_client();
    let init_pkt_c = client.process(None, now()).dgram();
    assert!(init_pkt_c.is_some());
    assert_eq!(init_pkt_c.as_ref().unwrap().len(), PATH_MTU_V6);

    qdebug!("---- server: CH -> SH, EE, CERT, CV, FIN");
    let mut server = default_server();
    let init_pkt_s = server.process(init_pkt_c.clone(), now()).dgram();
    assert!(init_pkt_s.is_some());

    qdebug!("---- client: cert verification");
    let out = client.process(init_pkt_s.clone(), now()).dgram();
    assert!(out.is_some());

    // The client has received handshake packet. It will remove the Initial keys.
    // We will check this by processing init_pkt_s a second time.
    // The initial packet should be dropped. The packet contains a Handshake packet as well, which
    // will be marked as dup.  And it will contain padding, which will be "dropped".
    check_discarded(&mut client, init_pkt_s.unwrap(), 2, 1);

    assert!(maybe_authenticate(&mut client));

    // The server has not removed the Initial keys yet, because it has not yet received a Handshake
    // packet from the client.
    // We will check this by processing init_pkt_c a second time.
    // The dropped packet is padding. The Initial packet has been mark dup.
    check_discarded(&mut server, init_pkt_c.clone().unwrap(), 1, 1);

    qdebug!("---- client: SH..FIN -> FIN");
    let out = client.process(None, now()).dgram();
    assert!(out.is_some());

    // The server will process the first Handshake packet.
    // After this the Initial keys will be dropped.
    let out = server.process(out, now()).dgram();
    assert!(out.is_some());

    // Check that the Initial keys are dropped at the server
    // We will check this by processing init_pkt_c a third time.
    // The Initial packet has been dropped and padding that follows it.
    // There is no dups, everything has been dropped.
    check_discarded(&mut server, init_pkt_c.unwrap(), 1, 0);
}

#[test]
fn key_update_client() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);
    let mut now = now();

    assert_eq!(client.get_epochs(), (Some(3), Some(3))); // (write, read)
    assert_eq!(server.get_epochs(), (Some(3), Some(3)));

    assert!(client.initiate_key_update().is_ok());
    assert_update_blocked(&mut client);

    // Initiating an update should only increase the write epoch.
    let idle_timeout = ConnectionParameters::default().get_idle_timeout();
    assert_eq!(Output::Callback(idle_timeout), client.process(None, now));
    assert_eq!(client.get_epochs(), (Some(4), Some(3)));

    // Send something to propagate the update.
    assert!(send_and_receive(&mut client, &mut server, now).is_none());

    // The server should now be waiting to discharge read keys.
    assert_eq!(server.get_epochs(), (Some(4), Some(3)));
    let res = server.process(None, now);
    if let Output::Callback(t) = res {
        assert!(t < idle_timeout);
    } else {
        panic!("server should now be waiting to clear keys");
    }

    // Without having had time to purge old keys, more updates are blocked.
    // The spec would permits it at this point, but we are more conservative.
    assert_update_blocked(&mut client);
    // The server can't update until it receives an ACK for a packet.
    assert_update_blocked(&mut server);

    // Waiting now for at least a PTO should cause the server to drop old keys.
    // But at this point the client hasn't received a key update from the server.
    // It will be stuck with old keys.
    now += AT_LEAST_PTO;
    let dgram = client.process(None, now).dgram();
    assert!(dgram.is_some()); // Drop this packet.
    assert_eq!(client.get_epochs(), (Some(4), Some(3)));
    mem::drop(server.process(None, now));
    assert_eq!(server.get_epochs(), (Some(4), Some(4)));

    // Even though the server has updated, it hasn't received an ACK yet.
    assert_update_blocked(&mut server);

    // Now get an ACK from the server.
    // The previous PTO packet (see above) was dropped, so we should get an ACK here.
    let dgram = send_and_receive(&mut client, &mut server, now);
    assert!(dgram.is_some());
    let res = client.process(dgram, now);
    // This is the first packet that the client has received from the server
    // with new keys, so its read timer just started.
    if let Output::Callback(t) = res {
        assert!(t < ConnectionParameters::default().get_idle_timeout());
    } else {
        panic!("client should now be waiting to clear keys");
    }

    assert_update_blocked(&mut client);
    assert_eq!(client.get_epochs(), (Some(4), Some(3)));
    // The server can't update until it gets something from the client.
    assert_update_blocked(&mut server);

    now += AT_LEAST_PTO;
    mem::drop(client.process(None, now));
    assert_eq!(client.get_epochs(), (Some(4), Some(4)));
}

#[test]
fn key_update_consecutive() {
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);
    let now = now();

    assert!(server.initiate_key_update().is_ok());
    assert_eq!(server.get_epochs(), (Some(4), Some(3)));

    // Server sends something.
    // Send twice and drop the first to induce an ACK from the client.
    mem::drop(send_something(&mut server, now)); // Drop this.

    // Another packet from the server will cause the client to ACK and update keys.
    let dgram = send_and_receive(&mut server, &mut client, now);
    assert!(dgram.is_some());
    assert_eq!(client.get_epochs(), (Some(4), Some(3)));

    // Have the server process the ACK.
    if let Output::Callback(_) = server.process(dgram, now) {
        assert_eq!(server.get_epochs(), (Some(4), Some(3)));
        // Now move the server temporarily into the future so that it
        // rotates the keys.  The client stays in the present.
        mem::drop(server.process(None, now + AT_LEAST_PTO));
        assert_eq!(server.get_epochs(), (Some(4), Some(4)));
    } else {
        panic!("server should have a timer set");
    }

    // Now update keys on the server again.
    assert!(server.initiate_key_update().is_ok());
    assert_eq!(server.get_epochs(), (Some(5), Some(4)));

    let dgram = send_something(&mut server, now + AT_LEAST_PTO);

    // However, as the server didn't wait long enough to update again, the
    // client hasn't rotated its keys, so the packet gets dropped.
    check_discarded(&mut client, dgram, 1, 0);
}

// Key updates can't be initiated too early.
#[test]
fn key_update_before_confirmed() {
    let mut client = default_client();
    assert_update_blocked(&mut client);
    let mut server = default_server();
    assert_update_blocked(&mut server);

    // Client Initial
    let dgram = client.process(None, now()).dgram();
    assert!(dgram.is_some());
    assert_update_blocked(&mut client);

    // Server Initial + Handshake
    let dgram = server.process(dgram, now()).dgram();
    assert!(dgram.is_some());
    assert_update_blocked(&mut server);

    // Client Handshake
    client.process_input(dgram.unwrap(), now());
    assert_update_blocked(&mut client);

    assert!(maybe_authenticate(&mut client));
    assert_update_blocked(&mut client);

    let dgram = client.process(None, now()).dgram();
    assert!(dgram.is_some());
    assert_update_blocked(&mut client);

    // Server HANDSHAKE_DONE
    let dgram = server.process(dgram, now()).dgram();
    assert!(dgram.is_some());
    assert!(server.initiate_key_update().is_ok());

    // Client receives HANDSHAKE_DONE
    let dgram = client.process(dgram, now()).dgram();
    assert!(dgram.is_none());
    assert!(client.initiate_key_update().is_ok());
}

#[test]
fn exhaust_write_keys() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    overwrite_invocations(0);
    let stream_id = client.stream_create(StreamType::UniDi).unwrap();
    assert!(client.stream_send(stream_id, b"explode!").is_ok());
    let dgram = client.process_output(now()).dgram();
    assert!(dgram.is_none());
    assert!(matches!(
        client.state(),
        State::Closed(ConnectionError::Transport(Error::KeysExhausted))
    ));
}

#[test]
fn exhaust_read_keys() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    let dgram = send_something(&mut client, now());

    overwrite_invocations(0);
    let dgram = server.process(Some(dgram), now()).dgram();
    assert!(matches!(
        server.state(),
        State::Closed(ConnectionError::Transport(Error::KeysExhausted))
    ));

    client.process_input(dgram.unwrap(), now());
    assert!(matches!(
        client.state(),
        State::Draining {
            error: ConnectionError::Transport(Error::PeerError(ERROR_AEAD_LIMIT_REACHED)),
            ..
        }
    ));
}

#[test]
fn automatic_update_write_keys() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    overwrite_invocations(UPDATE_WRITE_KEYS_AT);
    mem::drop(send_something(&mut client, now()));
    assert_eq!(client.get_epochs(), (Some(4), Some(3)));
}

#[test]
fn automatic_update_write_keys_later() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    overwrite_invocations(UPDATE_WRITE_KEYS_AT + 2);
    // No update after the first.
    mem::drop(send_something(&mut client, now()));
    assert_eq!(client.get_epochs(), (Some(3), Some(3)));
    // The second will update though.
    mem::drop(send_something(&mut client, now()));
    assert_eq!(client.get_epochs(), (Some(4), Some(3)));
}

#[test]
fn automatic_update_write_keys_blocked() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    // An outstanding key update will block the automatic update.
    client.initiate_key_update().unwrap();

    overwrite_invocations(UPDATE_WRITE_KEYS_AT);
    let stream_id = client.stream_create(StreamType::UniDi).unwrap();
    assert!(client.stream_send(stream_id, b"explode!").is_ok());
    let dgram = client.process_output(now()).dgram();
    // Not being able to update is fatal.
    assert!(dgram.is_none());
    assert!(matches!(
        client.state(),
        State::Closed(ConnectionError::Transport(Error::KeysExhausted))
    ));
}
