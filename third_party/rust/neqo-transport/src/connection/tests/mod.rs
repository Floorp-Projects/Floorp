// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![deny(clippy::pedantic)]

use super::{
    Connection, ConnectionError, FixedConnectionIdManager, Output, State, LOCAL_IDLE_TIMEOUT,
};
use crate::addr_valid::{AddressValidation, ValidateAddress};
use crate::cc::CWND_INITIAL_PKTS;
use crate::events::ConnectionEvent;
use crate::frame::StreamType;
use crate::path::PATH_MTU_V6;
use crate::recovery::ACK_ONLY_SIZE_LIMIT;
use crate::{CongestionControlAlgorithm, QuicVersion};

use std::cell::RefCell;
use std::mem;
use std::rc::Rc;
use std::time::{Duration, Instant};

use neqo_common::{event::Provider, qdebug, qtrace, Datagram};
use neqo_crypto::{AllowZeroRtt, AuthenticationStatus, ResumptionToken};
use test_fixture::{self, fixture_init, loopback, now};

// All the tests.
mod cc;
mod close;
mod handshake;
mod idle;
mod keys;
mod recovery;
mod resumption;
mod stream;
mod vn;
mod zerortt;

const DEFAULT_RTT: Duration = Duration::from_millis(100);
const AT_LEAST_PTO: Duration = Duration::from_secs(1);
const DEFAULT_STREAM_DATA: &[u8] = b"message";

// This is fabulous: because test_fixture uses the public API for Connection,
// it gets a different type to the ones that are referenced via super::super::*.
// Thus, this code can't use default_client() and default_server() from
// test_fixture because they produce different types.
//
// These are a direct copy of those functions.
pub fn default_client() -> Connection {
    fixture_init();
    Connection::new_client(
        test_fixture::DEFAULT_SERVER_NAME,
        test_fixture::DEFAULT_ALPN,
        Rc::new(RefCell::new(FixedConnectionIdManager::new(3))),
        loopback(),
        loopback(),
        &CongestionControlAlgorithm::NewReno,
        QuicVersion::default(),
    )
    .expect("create a default client")
}
pub fn default_server() -> Connection {
    fixture_init();

    let mut c = Connection::new_server(
        test_fixture::DEFAULT_KEYS,
        test_fixture::DEFAULT_ALPN,
        Rc::new(RefCell::new(FixedConnectionIdManager::new(5))),
        &CongestionControlAlgorithm::NewReno,
        QuicVersion::default(),
    )
    .expect("create a default server");
    c.server_enable_0rtt(&test_fixture::anti_replay(), AllowZeroRtt {})
        .expect("enable 0-RTT");
    c
}

/// If state is `AuthenticationNeeded` call `authenticated()`. This function will
/// consume all outstanding events on the connection.
pub fn maybe_authenticate(conn: &mut Connection) -> bool {
    let authentication_needed = |e| matches!(e, ConnectionEvent::AuthenticationNeeded);
    if conn.events().any(authentication_needed) {
        conn.authenticated(AuthenticationStatus::Ok, now());
        return true;
    }
    false
}

/// Drive the handshake between the client and server.
fn handshake(
    client: &mut Connection,
    server: &mut Connection,
    now: Instant,
    rtt: Duration,
) -> Instant {
    let mut a = client;
    let mut b = server;
    let mut now = now;

    let mut input = None;
    let is_done = |c: &mut Connection| match c.state() {
        State::Confirmed | State::Closing { .. } | State::Closed(..) => true,
        _ => false,
    };

    while !is_done(a) {
        let _ = maybe_authenticate(a);
        let had_input = input.is_some();
        let output = a.process(input, now).dgram();
        assert!(had_input || output.is_some());
        input = output;
        qtrace!("t += {:?}", rtt / 2);
        now += rtt / 2;
        mem::swap(&mut a, &mut b);
    }
    let _ = a.process(input, now);
    now
}

fn connect_with_rtt(
    client: &mut Connection,
    server: &mut Connection,
    now: Instant,
    rtt: Duration,
) -> Instant {
    let now = handshake(client, server, now, rtt);
    assert_eq!(*client.state(), State::Confirmed);
    assert_eq!(*client.state(), State::Confirmed);

    assert_eq!(client.loss_recovery.rtt(), rtt);
    assert_eq!(server.loss_recovery.rtt(), rtt);
    now
}

fn connect(client: &mut Connection, server: &mut Connection) {
    connect_with_rtt(client, server, now(), Duration::new(0, 0));
}

fn assert_error(c: &Connection, err: &ConnectionError) {
    match c.state() {
        State::Closing { error, .. } | State::Draining { error, .. } | State::Closed(error) => {
            assert_eq!(*error, *err);
        }
        _ => panic!("bad state {:?}", c.state()),
    }
}

fn exchange_ticket(
    client: &mut Connection,
    server: &mut Connection,
    now: Instant,
) -> ResumptionToken {
    let validation = AddressValidation::new(now, ValidateAddress::NoToken).unwrap();
    let validation = Rc::new(RefCell::new(validation));
    server.set_validation(Rc::clone(&validation));
    server.send_ticket(now, &[]).expect("can send ticket");
    let ticket = server.process_output(now).dgram();
    assert!(ticket.is_some());
    client.process_input(ticket.unwrap(), now);
    assert_eq!(*client.state(), State::Confirmed);
    get_tokens(client).pop().expect("should have token")
}

/// Connect with an RTT and then force both peers to be idle.
/// Getting the client and server to reach an idle state is surprisingly hard.
/// The server sends `HANDSHAKE_DONE` at the end of the handshake, and the client
/// doesn't immediately acknowledge it.  Reordering packets does the trick.
fn connect_rtt_idle(client: &mut Connection, server: &mut Connection, rtt: Duration) -> Instant {
    let mut now = connect_with_rtt(client, server, now(), rtt);
    let p1 = send_something(server, now);
    let p2 = send_something(server, now);
    now += rtt / 2;
    // Delivering p2 first at the client causes it to want to ACK.
    client.process_input(p2, now);
    // Delivering p1 should not have the client change its mind about the ACK.
    let ack = client.process(Some(p1), now).dgram();
    assert!(ack.is_some());
    assert_eq!(
        server.process(ack, now),
        Output::Callback(LOCAL_IDLE_TIMEOUT)
    );
    assert_eq!(
        client.process_output(now),
        Output::Callback(LOCAL_IDLE_TIMEOUT)
    );
    now
}

fn connect_force_idle(client: &mut Connection, server: &mut Connection) {
    connect_rtt_idle(client, server, Duration::new(0, 0));
}

/// This fills the congestion window from a single source.
/// As the pacer will interfere with this, this moves time forward
/// as `Output::Callback` is received.  Because it is hard to tell
/// from the return value whether a timeout is an ACK delay, PTO, or
/// pacing, this looks at the congestion window to tell when to stop.
/// Returns a list of datagrams and the new time.
fn fill_cwnd(src: &mut Connection, stream: u64, mut now: Instant) -> (Vec<Datagram>, Instant) {
    const BLOCK_SIZE: usize = 4_096;
    let mut total_dgrams = Vec::new();

    qtrace!(
        "fill_cwnd starting cwnd: {}",
        src.loss_recovery.cwnd_avail()
    );

    loop {
        let bytes_sent = src.stream_send(stream, &[0x42; BLOCK_SIZE]).unwrap();
        qtrace!("fill_cwnd wrote {} bytes", bytes_sent);
        if bytes_sent < BLOCK_SIZE {
            break;
        }
    }

    loop {
        let pkt = src.process_output(now);
        qtrace!(
            "fill_cwnd cwnd remaining={}, output: {:?}",
            src.loss_recovery.cwnd_avail(),
            pkt
        );
        match pkt {
            Output::Datagram(dgram) => {
                total_dgrams.push(dgram);
            }
            Output::Callback(t) => {
                if src.loss_recovery.cwnd_avail() < ACK_ONLY_SIZE_LIMIT {
                    break;
                }
                now += t;
            }
            Output::None => panic!(),
        }
    }

    (total_dgrams, now)
}

/// This magic number is the size of the client's CWND after the handshake completes.
/// This includes the initial congestion window, as increased as a result
/// receiving acknowledgments for Initial and Handshake packets, which is
/// at least one full packet (the first Initial) and a little extra.
///
/// As we change how we build packets, or even as NSS changes,
/// this number might be different.  The tests that depend on this
/// value could fail as a result of variations, so it's OK to just
/// change this value, but it is good to first understand where the
/// change came from.
const POST_HANDSHAKE_CWND: usize = PATH_MTU_V6 * (CWND_INITIAL_PKTS + 1) + 75;

/// Determine the number of packets required to fill the CWND.
const fn cwnd_packets(data: usize) -> usize {
    (data + ACK_ONLY_SIZE_LIMIT - 1) / PATH_MTU_V6
}

/// Determine the size of the last packet.
/// The minimal size of a packet is `ACK_ONLY_SIZE_LIMIT`.
fn last_packet(cwnd: usize) -> usize {
    if (cwnd % PATH_MTU_V6) > ACK_ONLY_SIZE_LIMIT {
        cwnd % PATH_MTU_V6
    } else {
        PATH_MTU_V6
    }
}

/// Assert that the set of packets fill the CWND.
fn assert_full_cwnd(packets: &[Datagram], cwnd: usize) {
    assert_eq!(packets.len(), cwnd_packets(cwnd));
    let (last, rest) = packets.split_last().unwrap();
    assert!(rest.iter().all(|d| d.len() == PATH_MTU_V6));
    assert_eq!(last.len(), last_packet(cwnd));
}

/// Send something on a stream from `sender` to `receiver`.
/// Return the resulting datagram.
#[must_use]
fn send_something(sender: &mut Connection, now: Instant) -> Datagram {
    let stream_id = sender.stream_create(StreamType::UniDi).unwrap();
    assert!(sender.stream_send(stream_id, DEFAULT_STREAM_DATA).is_ok());
    assert!(sender.stream_close_send(stream_id).is_ok());
    qdebug!([sender], "send_something on {}", stream_id);
    let dgram = sender.process(None, now).dgram();
    dgram.expect("should have something to send")
}

/// Send something on a stream from `sender` to `receiver`.
/// Return any ACK that might result.
fn send_and_receive(
    sender: &mut Connection,
    receiver: &mut Connection,
    now: Instant,
) -> Option<Datagram> {
    let dgram = send_something(sender, now);
    receiver.process(Some(dgram), now).dgram()
}

fn get_tokens(client: &mut Connection) -> Vec<ResumptionToken> {
    client
        .events()
        .filter_map(|e| {
            if let ConnectionEvent::ResumptionToken(token) = e {
                Some(token)
            } else {
                None
            }
        })
        .collect()
}
