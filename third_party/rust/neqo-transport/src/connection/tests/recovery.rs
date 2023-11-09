// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::super::{Connection, ConnectionParameters, Output, State};
use super::{
    assert_full_cwnd, connect, connect_force_idle, connect_rtt_idle, connect_with_rtt, cwnd,
    default_client, default_server, fill_cwnd, maybe_authenticate, new_client, send_and_receive,
    send_something, AT_LEAST_PTO, DEFAULT_RTT, DEFAULT_STREAM_DATA, POST_HANDSHAKE_CWND,
};
use crate::cc::CWND_MIN;
use crate::path::PATH_MTU_V6;
use crate::recovery::{
    FAST_PTO_SCALE, MAX_OUTSTANDING_UNACK, MIN_OUTSTANDING_UNACK, PTO_PACKET_COUNT,
};
use crate::rtt::GRANULARITY;
use crate::stats::MAX_PTO_COUNTS;
use crate::tparams::TransportParameter;
use crate::tracking::DEFAULT_ACK_DELAY;
use crate::StreamType;

use neqo_common::qdebug;
use neqo_crypto::AuthenticationStatus;
use std::mem;
use std::time::{Duration, Instant};
use test_fixture::{self, now, split_datagram};

#[test]
fn pto_works_basic() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    let mut now = now();

    let res = client.process(None, now);
    let idle_timeout = ConnectionParameters::default().get_idle_timeout();
    assert_eq!(res, Output::Callback(idle_timeout));

    // Send data on two streams
    let stream1 = client.stream_create(StreamType::UniDi).unwrap();
    assert_eq!(client.stream_send(stream1, b"hello").unwrap(), 5);
    assert_eq!(client.stream_send(stream1, b" world!").unwrap(), 7);

    let stream2 = client.stream_create(StreamType::UniDi).unwrap();
    assert_eq!(client.stream_send(stream2, b"there!").unwrap(), 6);

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
    let now = connect_rtt_idle(&mut client, &mut server, DEFAULT_RTT);

    // Send lots of data.
    let stream_id = client.stream_create(StreamType::UniDi).unwrap();
    let (dgrams, now) = fill_cwnd(&mut client, stream_id, now);
    assert_full_cwnd(&dgrams, POST_HANDSHAKE_CWND);

    // Fill the CWND after waiting for a PTO.
    let (dgrams, now) = fill_cwnd(&mut client, stream_id, now + AT_LEAST_PTO);
    // Two packets in the PTO.
    // The first should be full sized; the second might be small.
    assert_eq!(dgrams.len(), 2);
    assert_eq!(dgrams[0].len(), PATH_MTU_V6);

    // Both datagrams contain one or more STREAM frames.
    for d in dgrams {
        let stream_before = server.stats().frame_rx.stream;
        server.process_input(d, now);
        assert!(server.stats().frame_rx.stream > stream_before);
    }
}

#[test]
fn pto_works_ping() {
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);
    let mut now = now();

    let res = client.process(None, now);
    assert_eq!(
        res,
        Output::Callback(ConnectionParameters::default().get_idle_timeout())
    );

    now += Duration::from_secs(10);

    // Send a few packets from the client.
    let pkt0 = send_something(&mut client, now);
    let pkt1 = send_something(&mut client, now);
    let pkt2 = send_something(&mut client, now);
    let pkt3 = send_something(&mut client, now);

    // Nothing to do, should return callback
    let cb = client.process(None, now).callback();
    // The PTO timer is calculated with:
    //   RTT + max(rttvar * 4, GRANULARITY) + max_ack_delay
    // With zero RTT and rttvar, max_ack_delay is minimum too (GRANULARITY)
    assert_eq!(cb, GRANULARITY * 2);

    // Process these by server, skipping pkt0
    let srv0 = server.process(Some(pkt1), now).dgram();
    assert!(srv0.is_some()); // ooo, ack client pkt1

    now += Duration::from_millis(20);

    // process pkt2 (no ack yet)
    let srv1 = server.process(Some(pkt2), now).dgram();
    assert!(srv1.is_none());

    // process pkt3 (acked)
    let srv2 = server.process(Some(pkt3), now).dgram();
    // ack client pkt 2 & 3
    assert!(srv2.is_some());

    now += Duration::from_millis(20);
    // client processes ack
    let pkt4 = client.process(srv2, now).dgram();
    // client resends data from pkt0
    assert!(pkt4.is_some());

    // server sees ooo pkt0 and generates ack
    let srv3 = server.process(Some(pkt0), now).dgram();
    assert!(srv3.is_some());

    // Accept the acknowledgment.
    let pkt5 = client.process(srv3, now).dgram();
    assert!(pkt5.is_none());

    now += Duration::from_millis(70);
    // PTO expires. No unacked data. Only send PING.
    let client_pings = client.stats().frame_tx.ping;
    let pkt6 = client.process(None, now).dgram();
    assert_eq!(client.stats().frame_tx.ping, client_pings + 1);

    let server_pings = server.stats().frame_rx.ping;
    server.process_input(pkt6.unwrap(), now);
    assert_eq!(server.stats().frame_rx.ping, server_pings + 1);
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
    const HALF_RTT: Duration = Duration::from_millis(10);

    let mut now = now();
    // start handshake
    let mut client = default_client();
    let mut server = default_server();

    let pkt = client.process(None, now).dgram();
    let cb = client.process(None, now).callback();
    assert_eq!(cb, Duration::from_millis(300));

    now += HALF_RTT;
    let pkt = server.process(pkt, now).dgram();

    now += HALF_RTT;
    let pkt = client.process(pkt, now).dgram();

    let cb = client.process(None, now).callback();
    // The client now has a single RTT estimate (20ms), so
    // the handshake PTO is set based on that.
    assert_eq!(cb, HALF_RTT * 6);

    now += HALF_RTT;
    let pkt = server.process(pkt, now).dgram();
    assert!(pkt.is_none());

    now += HALF_RTT;
    client.authenticated(AuthenticationStatus::Ok, now);

    qdebug!("---- client: SH..FIN -> FIN");
    let pkt1 = client.process(None, now).dgram();
    assert!(pkt1.is_some());
    assert_eq!(*client.state(), State::Connected);

    let cb = client.process(None, now).callback();
    assert_eq!(cb, HALF_RTT * 6);

    let mut pto_counts = [0; MAX_PTO_COUNTS];
    assert_eq!(client.stats.borrow().pto_counts, pto_counts);

    // Wait for PTO to expire and resend a handshake packet.
    // Wait long enough that the 1-RTT PTO also fires.
    qdebug!("---- client: PTO");
    now += HALF_RTT * 6;
    let pkt2 = client.process(None, now).dgram();

    pto_counts[0] = 1;
    assert_eq!(client.stats.borrow().pto_counts, pto_counts);

    // Get a second PTO packet.
    // Add some application data to this datagram, then split the 1-RTT off.
    // We'll use that packet to force the server to acknowledge 1-RTT.
    let stream_id = client.stream_create(StreamType::UniDi).unwrap();
    client.stream_close_send(stream_id).unwrap();
    let pkt3 = client.process(None, now).dgram();
    let (pkt3_hs, pkt3_1rtt) = split_datagram(&pkt3.unwrap());

    // PTO has been doubled.
    let cb = client.process(None, now).callback();
    assert_eq!(cb, HALF_RTT * 12);

    // We still have only a single PTO
    assert_eq!(client.stats.borrow().pto_counts, pto_counts);

    qdebug!("---- server: receive FIN and send ACK");
    now += HALF_RTT;
    // Now let the server have pkt1 and expect an immediate Handshake ACK.
    // The output will be a Handshake packet with ACK and 1-RTT packet with
    // HANDSHAKE_DONE and (because of pkt3_1rtt) an ACK.
    // This should remove the 1-RTT PTO from messing this test up.
    let server_acks = server.stats().frame_tx.ack;
    let server_done = server.stats().frame_tx.handshake_done;
    server.process_input(pkt3_1rtt.unwrap(), now);
    let ack = server.process(pkt1, now).dgram();
    assert!(ack.is_some());
    assert_eq!(server.stats().frame_tx.ack, server_acks + 2);
    assert_eq!(server.stats().frame_tx.handshake_done, server_done + 1);

    // Check that the other packets (pkt2, pkt3) are Handshake packets.
    // The server discarded the Handshake keys already, therefore they are dropped.
    // Note that these don't include 1-RTT packets, because 1-RTT isn't send on PTO.
    let dropped_before1 = server.stats().dropped_rx;
    let server_frames = server.stats().frame_rx.all;
    server.process_input(pkt2.unwrap(), now);
    assert_eq!(1, server.stats().dropped_rx - dropped_before1);
    assert_eq!(server.stats().frame_rx.all, server_frames);

    let dropped_before2 = server.stats().dropped_rx;
    server.process_input(pkt3_hs, now);
    assert_eq!(1, server.stats().dropped_rx - dropped_before2);
    assert_eq!(server.stats().frame_rx.all, server_frames);

    now += HALF_RTT;

    // Let the client receive the ACK.
    // It should now be wait to acknowledge the HANDSHAKE_DONE.
    let cb = client.process(ack, now).callback();
    // The default ack delay is the RTT divided by the default ACK ratio of 4.
    let expected_ack_delay = HALF_RTT * 2 / 4;
    assert_eq!(cb, expected_ack_delay);

    // Let the ACK delay timer expire.
    now += cb;
    let out = client.process(None, now).dgram();
    assert!(out.is_some());
    let cb = client.process(None, now).callback();
    // The handshake keys are discarded, but now we're back to the idle timeout.
    // We don't send another PING because the handshake space is done and there
    // is nothing to probe for.

    let idle_timeout = ConnectionParameters::default().get_idle_timeout();
    assert_eq!(cb, idle_timeout - expected_ack_delay);
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
    mem::drop(server.process(pkt.dgram(), now));

    now += Duration::from_millis(10);
    client.authenticated(AuthenticationStatus::Ok, now);

    let stream = client.stream_create(StreamType::UniDi).unwrap();
    assert_eq!(stream, 2);
    assert_eq!(client.stream_send(stream, b"zero").unwrap(), 4);
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
    assert_eq!(server.stats().frame_rx.crypto, crypto_before + 1);
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
    mem::drop(send_something(&mut server, now));

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
    mem::drop(send_something(&mut server, now + AT_LEAST_PTO));
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
    mem::drop(send_something(&mut client, now));

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
    mem::drop(send_something(&mut server, now));
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

/// `sender` sends a little, `receiver` acknowledges it.
/// Repeat until `count` acknowledgements are sent.
/// Returns the last packet containing acknowledgements, if any.
fn trickle(sender: &mut Connection, receiver: &mut Connection, mut count: usize, now: Instant) {
    let id = sender.stream_create(StreamType::UniDi).unwrap();
    let mut maybe_ack = None;
    while count > 0 {
        qdebug!("trickle: remaining={}", count);
        assert_eq!(sender.stream_send(id, &[9]).unwrap(), 1);
        let dgram = sender.process(maybe_ack, now).dgram();

        maybe_ack = receiver.process(dgram, now).dgram();
        count -= usize::from(maybe_ack.is_some());
    }
    sender.process_input(maybe_ack.unwrap(), now);
}

/// Ensure that a PING frame is sent with ACK sometimes.
/// `fast` allows testing of when `MAX_OUTSTANDING_UNACK` packets are
/// outstanding (`fast` is `true`) within 1 PTO and when only
/// `MIN_OUTSTANDING_UNACK` packets arrive after 2 PTOs (`fast` is `false`).
fn ping_with_ack(fast: bool) {
    let mut sender = default_client();
    let mut receiver = default_server();
    let mut now = now();
    connect_force_idle(&mut sender, &mut receiver);
    let sender_acks_before = sender.stats().frame_tx.ack;
    let receiver_acks_before = receiver.stats().frame_tx.ack;
    let count = if fast {
        MAX_OUTSTANDING_UNACK
    } else {
        MIN_OUTSTANDING_UNACK
    };
    trickle(&mut sender, &mut receiver, count, now);
    assert_eq!(sender.stats().frame_tx.ack, sender_acks_before);
    assert_eq!(receiver.stats().frame_tx.ack, receiver_acks_before + count);
    assert_eq!(receiver.stats().frame_tx.ping, 0);

    if !fast {
        // Wait at least one PTO, from the reciever's perspective.
        // A receiver that hasn't received MAX_OUTSTANDING_UNACK won't send PING.
        now += receiver.pto() + Duration::from_micros(1);
        trickle(&mut sender, &mut receiver, 1, now);
        assert_eq!(receiver.stats().frame_tx.ping, 0);
    }

    // After a second PTO (or the first if fast), new acknowledgements come
    // with a PING frame and cause an ACK to be sent by the sender.
    now += receiver.pto() + Duration::from_micros(1);
    trickle(&mut sender, &mut receiver, 1, now);
    assert_eq!(receiver.stats().frame_tx.ping, 1);
    if let Output::Callback(t) = sender.process_output(now) {
        assert_eq!(t, DEFAULT_ACK_DELAY);
        assert!(sender.process_output(now + t).dgram().is_some());
    }
    assert_eq!(sender.stats().frame_tx.ack, sender_acks_before + 1);
}

#[test]
fn ping_with_ack_fast() {
    ping_with_ack(true);
}

#[test]
fn ping_with_ack_slow() {
    ping_with_ack(false);
}

#[test]
fn ping_with_ack_min() {
    const COUNT: usize = MIN_OUTSTANDING_UNACK - 2;
    let mut sender = default_client();
    let mut receiver = default_server();
    let mut now = now();
    connect_force_idle(&mut sender, &mut receiver);
    let sender_acks_before = sender.stats().frame_tx.ack;
    let receiver_acks_before = receiver.stats().frame_tx.ack;
    trickle(&mut sender, &mut receiver, COUNT, now);
    assert_eq!(sender.stats().frame_tx.ack, sender_acks_before);
    assert_eq!(receiver.stats().frame_tx.ack, receiver_acks_before + COUNT);
    assert_eq!(receiver.stats().frame_tx.ping, 0);

    // After 3 PTO, no PING because there are too few outstanding packets.
    now += receiver.pto() * 3 + Duration::from_micros(1);
    trickle(&mut sender, &mut receiver, 1, now);
    assert_eq!(receiver.stats().frame_tx.ping, 0);
}

/// This calculates the PTO timer immediately after connection establishment.
/// It depends on there only being 2 RTT samples in the handshake.
fn expected_pto(rtt: Duration) -> Duration {
    // PTO calculation is rtt + 4rttvar + ack delay.
    // rttvar should be (rtt + 4 * (rtt / 2) * (3/4)^n + 25ms)/2
    // where n is the number of round trips
    // This uses a 25ms ack delay as the ACK delay extension
    // is negotiated and no ACK_DELAY frame has been received.
    rtt + rtt * 9 / 8 + Duration::from_millis(25)
}

#[test]
fn fast_pto() {
    let mut client = new_client(ConnectionParameters::default().fast_pto(FAST_PTO_SCALE / 2));
    let mut server = default_server();
    let mut now = connect_rtt_idle(&mut client, &mut server, DEFAULT_RTT);

    let res = client.process(None, now);
    let idle_timeout = ConnectionParameters::default().get_idle_timeout() - (DEFAULT_RTT / 2);
    assert_eq!(res, Output::Callback(idle_timeout));

    // Send data on two streams
    let stream = client.stream_create(StreamType::UniDi).unwrap();
    assert_eq!(
        client.stream_send(stream, DEFAULT_STREAM_DATA).unwrap(),
        DEFAULT_STREAM_DATA.len()
    );

    // Send a packet after some time.
    now += idle_timeout / 2;
    let dgram = client.process_output(now).dgram();
    assert!(dgram.is_some());

    // Nothing to do, should return a callback.
    let cb = client.process_output(now).callback();
    assert_eq!(expected_pto(DEFAULT_RTT) / 2, cb);

    // Once the PTO timer expires, a PTO packet should be sent should want to send PTO packet.
    now += cb;
    let dgram = client.process(None, now).dgram();

    let stream_before = server.stats().frame_rx.stream;
    server.process_input(dgram.unwrap(), now);
    assert_eq!(server.stats().frame_rx.stream, stream_before + 1);
}

/// Even if the PTO timer is slowed right down, persistent congestion is declared
/// based on the "true" value of the timer.
#[test]
fn fast_pto_persistent_congestion() {
    let mut client = new_client(ConnectionParameters::default().fast_pto(FAST_PTO_SCALE * 2));
    let mut server = default_server();
    let mut now = connect_rtt_idle(&mut client, &mut server, DEFAULT_RTT);

    let res = client.process(None, now);
    let idle_timeout = ConnectionParameters::default().get_idle_timeout() - (DEFAULT_RTT / 2);
    assert_eq!(res, Output::Callback(idle_timeout));

    // Send packets spaced by the PTO timer.  And lose them.
    // Note: This timing is a tiny bit higher than the client will use
    // to determine persistent congestion. The ACK below adds another RTT
    // estimate, which will reduce rttvar by 3/4, so persistent congestion
    // will occur at `rtt + rtt*27/32 + 25ms`.
    // That is OK as we're still showing that this interval is less than
    // six times the PTO, which is what would be used if the scaling
    // applied to the PTO used to determine persistent congestion.
    let pc_interval = expected_pto(DEFAULT_RTT) * 3;
    println!("pc_interval {pc_interval:?}");
    let _drop1 = send_something(&mut client, now);

    // Check that the PTO matches expectations.
    let cb = client.process_output(now).callback();
    assert_eq!(expected_pto(DEFAULT_RTT) * 2, cb);

    now += pc_interval;
    let _drop2 = send_something(&mut client, now);
    let _drop3 = send_something(&mut client, now);
    let _drop4 = send_something(&mut client, now);
    let dgram = send_something(&mut client, now);

    // Now acknowledge the tail packet and enter persistent congestion.
    now += DEFAULT_RTT / 2;
    let ack = server.process(Some(dgram), now).dgram();
    now += DEFAULT_RTT / 2;
    client.process_input(ack.unwrap(), now);
    assert_eq!(cwnd(&client), CWND_MIN);
}
