// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::super::{Output, State, LOCAL_IDLE_TIMEOUT};
use super::{
    assert_full_cwnd, connect, connect_force_idle, connect_with_rtt, default_client,
    default_server, fill_cwnd, maybe_authenticate, send_and_receive, send_something, AT_LEAST_PTO,
    POST_HANDSHAKE_CWND,
};
use crate::frame::StreamType;
use crate::path::PATH_MTU_V6;
use crate::recovery::PTO_PACKET_COUNT;
use crate::stats::MAX_PTO_COUNTS;
use crate::tparams::TransportParameter;
use crate::tracking::ACK_DELAY;

use neqo_common::qdebug;
use neqo_crypto::AuthenticationStatus;
use std::time::Duration;
use test_fixture::{self, now, split_datagram};

#[test]
fn pto_works_basic() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    let mut now = now();

    let res = client.process(None, now);
    assert_eq!(res, Output::Callback(LOCAL_IDLE_TIMEOUT));

    // Send data on two streams
    assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
    assert_eq!(client.stream_send(2, b"hello").unwrap(), 5);
    assert_eq!(client.stream_send(2, b" world").unwrap(), 6);

    assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 6);
    assert_eq!(client.stream_send(6, b"there!").unwrap(), 6);

    // Send a packet after some time.
    now += Duration::from_secs(10);
    let out = client.process(None, now);
    assert!(out.dgram().is_some());

    // Nothing to do, should return callback
    let out = client.process(None, now);
    assert!(matches!(out, Output::Callback(_)));

    // One second later, it should want to send PTO packet
    now += AT_LEAST_PTO;
    let out = client.process(None, now);

    let stream_before = server.stats().frame_rx.stream;
    server.process_input(out.dgram().unwrap(), now);
    assert_eq!(server.stats().frame_rx.stream, stream_before + 2);
}

#[test]
fn pto_works_full_cwnd() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    let res = client.process(None, now());
    assert_eq!(res, Output::Callback(LOCAL_IDLE_TIMEOUT));

    // Send lots of data.
    assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
    let (dgrams, now) = fill_cwnd(&mut client, 2, now());
    assert_full_cwnd(&dgrams, POST_HANDSHAKE_CWND);

    neqo_common::qwarn!("waiting over");
    // Fill the CWND after waiting for a PTO.
    let (dgrams, now) = fill_cwnd(&mut client, 2, now + AT_LEAST_PTO);
    // Two packets in the PTO.
    // The first should be full sized; the second might be small.
    assert_eq!(dgrams.len(), 2);
    assert_eq!(dgrams[0].len(), PATH_MTU_V6);

    // Both datagrams contain a STREAM frame.
    for d in dgrams {
        let stream_before = server.stats().frame_rx.stream;
        server.process_input(d, now);
        assert_eq!(server.stats().frame_rx.stream, stream_before + 1);
    }
}

#[test]
#[allow(clippy::cognitive_complexity)]
fn pto_works_ping() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    let now = now();

    let res = client.process(None, now);
    assert_eq!(res, Output::Callback(LOCAL_IDLE_TIMEOUT));

    // Send "zero" pkt
    assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
    assert_eq!(client.stream_send(2, b"zero").unwrap(), 4);
    let pkt0 = client.process(None, now + Duration::from_secs(10));
    assert!(matches!(pkt0, Output::Datagram(_)));

    // Send "one" pkt
    assert_eq!(client.stream_send(2, b"one").unwrap(), 3);
    let pkt1 = client.process(None, now + Duration::from_secs(10));

    // Send "two" pkt
    assert_eq!(client.stream_send(2, b"two").unwrap(), 3);
    let pkt2 = client.process(None, now + Duration::from_secs(10));

    // Send "three" pkt
    assert_eq!(client.stream_send(2, b"three").unwrap(), 5);
    let pkt3 = client.process(None, now + Duration::from_secs(10));

    // Nothing to do, should return callback
    let out = client.process(None, now + Duration::from_secs(10));
    // Check callback delay is what we expect
    assert!(matches!(out, Output::Callback(x) if x == Duration::from_millis(45)));

    // Process these by server, skipping pkt0
    let srv0_pkt1 = server.process(pkt1.dgram(), now + Duration::from_secs(10));
    // ooo, ack client pkt 1
    assert!(matches!(srv0_pkt1, Output::Datagram(_)));

    // process pkt2 (no ack yet)
    let srv2 = server.process(
        pkt2.dgram(),
        now + Duration::from_secs(10) + Duration::from_millis(20),
    );
    assert!(matches!(srv2, Output::Callback(_)));

    // process pkt3 (acked)
    let srv2 = server.process(
        pkt3.dgram(),
        now + Duration::from_secs(10) + Duration::from_millis(20),
    );
    // ack client pkt 2 & 3
    assert!(matches!(srv2, Output::Datagram(_)));

    // client processes ack
    let pkt4 = client.process(
        srv2.dgram(),
        now + Duration::from_secs(10) + Duration::from_millis(40),
    );
    // client resends data from pkt0
    assert!(matches!(pkt4, Output::Datagram(_)));

    // server sees ooo pkt0 and generates ack
    let srv_pkt2 = server.process(
        pkt0.dgram(),
        now + Duration::from_secs(10) + Duration::from_millis(40),
    );
    assert!(matches!(srv_pkt2, Output::Datagram(_)));

    // Orig data is acked
    let pkt5 = client.process(
        srv_pkt2.dgram(),
        now + Duration::from_secs(10) + Duration::from_millis(40),
    );
    assert!(matches!(pkt5, Output::Callback(_)));

    // PTO expires. No unacked data. Only send PING.
    let pkt6 = client.process(
        None,
        now + Duration::from_secs(10) + Duration::from_millis(110),
    );

    let ping_before = server.stats().frame_rx.ping;
    server.process_input(
        pkt6.dgram().unwrap(),
        now + Duration::from_secs(10) + Duration::from_millis(110),
    );
    assert_eq!(server.stats().frame_rx.ping, ping_before + 1);
}

#[test]
fn pto_initial() {
    const INITIAL_PTO: Duration = Duration::from_millis(300);
    let mut now = now();

    qdebug!("---- client: generate CH");
    let mut client = default_client();
    let pkt1 = client.process(None, now).dgram();
    assert!(pkt1.is_some());
    assert_eq!(pkt1.clone().unwrap().len(), PATH_MTU_V6);

    let delay = client.process(None, now).callback();
    assert_eq!(delay, INITIAL_PTO);

    // Resend initial after PTO.
    now += delay;
    let pkt2 = client.process(None, now).dgram();
    assert!(pkt2.is_some());
    assert_eq!(pkt2.unwrap().len(), PATH_MTU_V6);

    let pkt3 = client.process(None, now).dgram();
    assert!(pkt3.is_some());
    assert_eq!(pkt3.unwrap().len(), PATH_MTU_V6);

    let delay = client.process(None, now).callback();
    // PTO has doubled.
    assert_eq!(delay, INITIAL_PTO * 2);

    // Server process the first initial pkt.
    let mut server = default_server();
    let out = server.process(pkt1, now).dgram();
    assert!(out.is_some());

    // Client receives ack for the first initial packet as well a Handshake packet.
    // After the handshake packet the initial keys and the crypto stream for the initial
    // packet number space will be discarded.
    // Here only an ack for the Handshake packet will be sent.
    let out = client.process(out, now).dgram();
    assert!(out.is_some());

    // We do not have PTO for the resent initial packet any more, but
    // the Handshake PTO timer should be armed.  As the RTT is apparently
    // the same as the initial PTO value, and there is only one sample,
    // the PTO will be 3x the INITIAL PTO.
    let delay = client.process(None, now).callback();
    assert_eq!(delay, INITIAL_PTO * 3);
}

/// A complete handshake that involves a PTO in the Handshake space.
#[test]
fn pto_handshake_complete() {
    let mut now = now();
    // start handshake
    let mut client = default_client();
    let mut server = default_server();

    let pkt = client.process(None, now).dgram();
    let cb = client.process(None, now).callback();
    assert_eq!(cb, Duration::from_millis(300));

    now += Duration::from_millis(10);
    let pkt = server.process(pkt, now).dgram();

    now += Duration::from_millis(10);
    let pkt = client.process(pkt, now).dgram();

    let cb = client.process(None, now).callback();
    // The client now has a single RTT estimate (20ms), so
    // the handshake PTO is set based on that.
    assert_eq!(cb, Duration::from_millis(60));

    now += Duration::from_millis(10);
    let pkt = server.process(pkt, now).dgram();
    assert!(pkt.is_none());

    now += Duration::from_millis(10);
    client.authenticated(AuthenticationStatus::Ok, now);

    qdebug!("---- client: SH..FIN -> FIN");
    let pkt1 = client.process(None, now).dgram();
    assert!(pkt1.is_some());

    let cb = client.process(None, now).callback();
    assert_eq!(cb, Duration::from_millis(60));

    let mut pto_counts = [0; MAX_PTO_COUNTS];
    assert_eq!(client.stats.borrow().pto_counts, pto_counts);

    // Wait for PTO to expire and resend a handshake packet
    now += Duration::from_millis(60);
    let pkt2 = client.process(None, now).dgram();
    assert!(pkt2.is_some());

    pto_counts[0] = 1;
    assert_eq!(client.stats.borrow().pto_counts, pto_counts);

    // Get a second PTO packet.
    let pkt3 = client.process(None, now).dgram();
    assert!(pkt3.is_some());

    // PTO has been doubled.
    let cb = client.process(None, now).callback();
    assert_eq!(cb, Duration::from_millis(120));

    // We still have only a single PTO
    assert_eq!(client.stats.borrow().pto_counts, pto_counts);

    now += Duration::from_millis(10);
    // Server receives the first packet.
    // The output will be a Handshake packet with an ack and a app pn space packet with
    // HANDSHAKE_DONE.
    let pkt = server.process(pkt1, now).dgram();
    assert!(pkt.is_some());

    // Check that the PTO packets (pkt2, pkt3) are Handshake packets.
    // The server discarded the Handshake keys already, therefore they are dropped.
    let dropped_before1 = server.stats().dropped_rx;
    let frames_before = server.stats().frame_rx.all;
    server.process_input(pkt2.unwrap(), now);
    assert_eq!(1, server.stats().dropped_rx - dropped_before1);
    assert_eq!(server.stats().frame_rx.all, frames_before);

    let dropped_before2 = server.stats().dropped_rx;
    server.process_input(pkt3.unwrap(), now);
    assert_eq!(1, server.stats().dropped_rx - dropped_before2);
    assert_eq!(server.stats().frame_rx.all, frames_before);

    now += Duration::from_millis(10);
    // Client receive ack for the first packet
    let cb = client.process(pkt, now).callback();
    // Ack delay timer for the packet carrying HANDSHAKE_DONE.
    assert_eq!(cb, ACK_DELAY);

    // Let the ack timer expire.
    now += cb;
    let out = client.process(None, now).dgram();
    assert!(out.is_some());
    let cb = client.process(None, now).callback();
    // The handshake keys are discarded, but now we're back to the idle timeout.
    // We don't send another PING because the handshake space is done and there
    // is nothing to probe for.

    pto_counts[0] = 1;
    assert_eq!(client.stats.borrow().pto_counts, pto_counts);
    assert_eq!(cb, LOCAL_IDLE_TIMEOUT - ACK_DELAY);
}

/// Test that PTO in the Handshake space contains the right frames.
#[test]
fn pto_handshake_frames() {
    let mut now = now();
    qdebug!("---- client: generate CH");
    let mut client = default_client();
    let pkt = client.process(None, now);

    now += Duration::from_millis(10);
    qdebug!("---- server: CH -> SH, EE, CERT, CV, FIN");
    let mut server = default_server();
    let pkt = server.process(pkt.dgram(), now);

    now += Duration::from_millis(10);
    qdebug!("---- client: cert verification");
    let pkt = client.process(pkt.dgram(), now);

    now += Duration::from_millis(10);
    let _ = server.process(pkt.dgram(), now);

    now += Duration::from_millis(10);
    client.authenticated(AuthenticationStatus::Ok, now);

    assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
    assert_eq!(client.stream_send(2, b"zero").unwrap(), 4);
    qdebug!("---- client: SH..FIN -> FIN and 1RTT packet");
    let pkt1 = client.process(None, now).dgram();
    assert!(pkt1.is_some());

    // Get PTO timer.
    let out = client.process(None, now);
    assert_eq!(out, Output::Callback(Duration::from_millis(60)));

    // Wait for PTO to expire and resend a handshake packet.
    now += Duration::from_millis(60);
    let pkt2 = client.process(None, now).dgram();
    assert!(pkt2.is_some());

    now += Duration::from_millis(10);
    let crypto_before = server.stats().frame_rx.crypto;
    server.process_input(pkt2.unwrap(), now);
    assert_eq!(server.stats().frame_rx.crypto, crypto_before + 1)
}

/// In the case that the Handshake takes too many packets, the server might
/// be stalled on the anti-amplification limit.  If a Handshake ACK from the
/// client is lost, the client has to keep the PTO timer armed or the server
/// might be unable to send anything, causing a deadlock.
#[test]
fn handshake_ack_pto() {
    const RTT: Duration = Duration::from_millis(10);
    let mut now = now();
    let mut client = default_client();
    let mut server = default_server();
    // This is a greasing transport parameter, and large enough that the
    // server needs to send two Handshake packets.
    let big = TransportParameter::Bytes(vec![0; PATH_MTU_V6]);
    server.set_local_tparam(0xce16, big).unwrap();

    let c1 = client.process(None, now).dgram();

    now += RTT / 2;
    let s1 = server.process(c1, now).dgram();
    assert!(s1.is_some());
    let s2 = server.process(None, now).dgram();
    assert!(s1.is_some());

    // Now let the client have the Initial, but drop the first coalesced Handshake packet.
    now += RTT / 2;
    let (initial, _) = split_datagram(&s1.unwrap());
    client.process_input(initial, now);
    let c2 = client.process(s2, now).dgram();
    assert!(c2.is_some()); // This is an ACK.  Drop it.
    let delay = client.process(None, now).callback();
    assert_eq!(delay, RTT * 3);

    let mut pto_counts = [0; MAX_PTO_COUNTS];
    assert_eq!(client.stats.borrow().pto_counts, pto_counts);

    // Wait for the PTO and ensure that the client generates a packet.
    now += delay;
    let c3 = client.process(None, now).dgram();
    assert!(c3.is_some());

    now += RTT / 2;
    let ping_before = server.stats().frame_rx.ping;
    server.process_input(c3.unwrap(), now);
    assert_eq!(server.stats().frame_rx.ping, ping_before + 1);

    pto_counts[0] = 1;
    assert_eq!(client.stats.borrow().pto_counts, pto_counts);

    // Now complete the handshake as cheaply as possible.
    let dgram = server.process(None, now).dgram();
    client.process_input(dgram.unwrap(), now);
    maybe_authenticate(&mut client);
    let dgram = client.process(None, now).dgram();
    assert_eq!(*client.state(), State::Connected);
    let dgram = server.process(dgram, now).dgram();
    assert_eq!(*server.state(), State::Confirmed);
    client.process_input(dgram.unwrap(), now);
    assert_eq!(*client.state(), State::Confirmed);

    assert_eq!(client.stats.borrow().pto_counts, pto_counts);
}

#[test]
fn loss_recovery_crash() {
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);
    let now = now();

    // The server sends something, but we will drop this.
    let _ = send_something(&mut server, now);

    // Then send something again, but let it through.
    let ack = send_and_receive(&mut server, &mut client, now);
    assert!(ack.is_some());

    // Have the server process the ACK.
    let cb = server.process(ack, now).callback();
    assert!(cb > Duration::from_secs(0));

    // Now we leap into the future.  The server should regard the first
    // packet as lost based on time alone.
    let dgram = server.process(None, now + AT_LEAST_PTO).dgram();
    assert!(dgram.is_some());

    // This crashes.
    let _ = send_something(&mut server, now + AT_LEAST_PTO);
}

// If we receive packets after the PTO timer has fired, we won't clear
// the PTO state, but we might need to acknowledge those packets.
// This shouldn't happen, but we found that some implementations do this.
#[test]
fn ack_after_pto() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    let mut now = now();

    // The client sends and is forced into a PTO.
    let _ = send_something(&mut client, now);

    // Jump forward to the PTO and drain the PTO packets.
    now += AT_LEAST_PTO;
    for _ in 0..PTO_PACKET_COUNT {
        let dgram = client.process(None, now).dgram();
        assert!(dgram.is_some());
    }
    assert!(client.process(None, now).dgram().is_none());

    // The server now needs to send something that will cause the
    // client to want to acknowledge it.  A little out of order
    // delivery is just the thing.
    // Note: The server can't ACK anything here, but none of what
    // the client has sent so far has been transferred.
    let _ = send_something(&mut server, now);
    let dgram = send_something(&mut server, now);

    // The client is now after a PTO, but if it receives something
    // that demands acknowledgment, it will send just the ACK.
    let ack = client.process(Some(dgram), now).dgram();
    assert!(ack.is_some());

    // Make sure that the packet only contained an ACK frame.
    let all_frames_before = server.stats().frame_rx.all;
    let ack_before = server.stats().frame_rx.ack;
    server.process_input(ack.unwrap(), now);
    assert_eq!(server.stats().frame_rx.all, all_frames_before + 1);
    assert_eq!(server.stats().frame_rx.ack, ack_before + 1);
}

/// When we declare a packet as lost, we keep it around for a while for another loss period.
/// Those packets should not affect how we report the loss recovery timer.
/// As the loss recovery timer based on RTT we use that to drive the state.
#[test]
fn lost_but_kept_and_lr_timer() {
    const RTT: Duration = Duration::from_secs(1);
    let mut client = default_client();
    let mut server = default_server();
    let mut now = connect_with_rtt(&mut client, &mut server, now(), RTT);

    // Two packets (p1, p2) are sent at around t=0.  The first is lost.
    let _p1 = send_something(&mut client, now);
    let p2 = send_something(&mut client, now);

    // At t=RTT/2 the server receives the packet and ACKs it.
    now += RTT / 2;
    let ack = server.process(Some(p2), now).dgram();
    assert!(ack.is_some());
    // The client also sends another two packets (p3, p4), again losing the first.
    let _p3 = send_something(&mut client, now);
    let p4 = send_something(&mut client, now);

    // At t=RTT the client receives the ACK and goes into timed loss recovery.
    // The client doesn't call p1 lost at this stage, but it will soon.
    now += RTT / 2;
    let res = client.process(ack, now);
    // The client should be on a loss recovery timer as p1 is missing.
    let lr_timer = res.callback();
    // Loss recovery timer should be RTT/8, but only check for 0 or >=RTT/2.
    assert_ne!(lr_timer, Duration::from_secs(0));
    assert!(lr_timer < (RTT / 2));
    // The server also receives and acknowledges p4, again sending an ACK.
    let ack = server.process(Some(p4), now).dgram();
    assert!(ack.is_some());

    // At t=RTT*3/2 the client should declare p1 to be lost.
    now += RTT / 2;
    // So the client will send the data from p1 again.
    let res = client.process(None, now);
    assert!(res.dgram().is_some());
    // When the client processes the ACK, it should engage the
    // loss recovery timer for p3, not p1 (even though it still tracks p1).
    let res = client.process(ack, now);
    let lr_timer2 = res.callback();
    assert_eq!(lr_timer, lr_timer2);
}

/// We should not be setting the loss recovery timer based on packets
/// that are sent prior to the largest acknowledged.
/// Testing this requires that we construct a case where one packet
/// number space causes the loss recovery timer to be engaged.  At the same time,
/// there is a packet in another space that hasn't been acknowledged AND
/// that packet number space has not received acknowledgments for later packets.
#[test]
fn loss_time_past_largest_acked() {
    const RTT: Duration = Duration::from_secs(10);
    const INCR: Duration = Duration::from_millis(1);
    let mut client = default_client();
    let mut server = default_server();

    let mut now = now();

    // Start the handshake.
    let c_in = client.process(None, now).dgram();
    now += RTT / 2;
    let s_hs1 = server.process(c_in, now).dgram();

    // Get some spare server handshake packets for the client to ACK.
    // This involves a time machine, so be a little cautious.
    // This test uses an RTT of 10s, but our server starts
    // with a much lower RTT estimate, so the PTO at this point should
    // be much smaller than an RTT and so the server shouldn't see
    // time go backwards.
    let s_pto = server.process(None, now).callback();
    assert_ne!(s_pto, Duration::from_secs(0));
    assert!(s_pto < RTT);
    let s_hs2 = server.process(None, now + s_pto).dgram();
    assert!(s_hs2.is_some());
    let s_hs3 = server.process(None, now + s_pto).dgram();
    assert!(s_hs3.is_some());

    // Get some Handshake packets from the client.
    // We need one to be left unacknowledged before one that is acknowledged.
    // So that the client engages the loss recovery timer.
    // This is complicated by the fact that it is hard to cause the client
    // to generate an ack-eliciting packet.  For that, we use the Finished message.
    // Reordering delivery ensures that the later packet is also acknowledged.
    now += RTT / 2;
    let c_hs1 = client.process(s_hs1, now).dgram();
    assert!(c_hs1.is_some()); // This comes first, so it's useless.
    maybe_authenticate(&mut client);
    let c_hs2 = client.process(None, now).dgram();
    assert!(c_hs2.is_some()); // This one will elicit an ACK.

    // The we need the outstanding packet to be sent after the
    // application data packet, so space these out a tiny bit.
    let _p1 = send_something(&mut client, now + INCR);
    let c_hs3 = client.process(s_hs2, now + (INCR * 2)).dgram();
    assert!(c_hs3.is_some()); // This will be left outstanding.
    let c_hs4 = client.process(s_hs3, now + (INCR * 3)).dgram();
    assert!(c_hs4.is_some()); // This will be acknowledged.

    // Process c_hs2 and c_hs4, but skip c_hs3.
    // Then get an ACK for the client.
    now += RTT / 2;
    // Deliver c_hs4 first, but don't generate a packet.
    server.process_input(c_hs4.unwrap(), now);
    let s_ack = server.process(c_hs2, now).dgram();
    assert!(s_ack.is_some());
    // This includes an ACK, but it also includes HANDSHAKE_DONE,
    // which we need to remove because that will cause the Handshake loss
    // recovery state to be dropped.
    let (s_hs_ack, _s_ap_ack) = split_datagram(&s_ack.unwrap());

    // Now the client should start its loss recovery timer based on the ACK.
    now += RTT / 2;
    let c_ack = client.process(Some(s_hs_ack), now).dgram();
    assert!(c_ack.is_none());
    // The client should now have the loss recovery timer active.
    let lr_time = client.process(None, now).callback();
    assert_ne!(lr_time, Duration::from_secs(0));
    assert!(lr_time < (RTT / 2));

    // Skipping forward by the loss recovery timer should cause the client to
    // mark packets as lost and retransmit, after which we should be on the PTO
    // timer.
    now += lr_time;
    let delay = client.process(None, now).callback();
    assert_ne!(delay, Duration::from_secs(0));
    assert!(delay > lr_time);
}
