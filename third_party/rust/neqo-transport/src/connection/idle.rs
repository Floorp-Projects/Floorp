// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::recovery::RecoveryToken;
use neqo_common::qtrace;
use std::{
    cmp::{max, min},
    time::{Duration, Instant},
};

#[derive(Debug, Clone)]
/// There's a little bit of different behavior for resetting idle timeout. See
/// -transport 10.2 ("Idle Timeout").
enum IdleTimeoutState {
    Init,
    PacketReceived(Instant),
    AckElicitingPacketSent(Instant),
}

#[derive(Debug, Clone)]
/// There's a little bit of different behavior for resetting idle timeout. See
/// -transport 10.2 ("Idle Timeout").
pub struct IdleTimeout {
    timeout: Duration,
    state: IdleTimeoutState,
    keep_alive_outstanding: bool,
}

impl IdleTimeout {
    pub fn new(timeout: Duration) -> Self {
        Self {
            timeout,
            state: IdleTimeoutState::Init,
            keep_alive_outstanding: false,
        }
    }
}

impl IdleTimeout {
    pub fn set_peer_timeout(&mut self, peer_timeout: Duration) {
        self.timeout = min(self.timeout, peer_timeout);
    }

    pub fn expiry(&self, now: Instant, pto: Duration, keep_alive: bool) -> Instant {
        let start = match self.state {
            IdleTimeoutState::Init => now,
            IdleTimeoutState::PacketReceived(t) | IdleTimeoutState::AckElicitingPacketSent(t) => t,
        };
        let delay = if keep_alive && !self.keep_alive_outstanding {
            // For a keep-alive timer, wait for half the timeout interval, but be sure
            // not to wait too little or we will send many unnecessary probes.
            max(self.timeout / 2, pto)
        } else {
            max(self.timeout, pto * 3)
        };
        qtrace!(
            "IdleTimeout::expiry@{now:?} pto={pto:?}, ka={keep_alive} => {t:?}",
            t = start + delay
        );
        start + delay
    }

    pub fn on_packet_sent(&mut self, now: Instant) {
        // Only reset idle timeout if we've received a packet since the last
        // time we reset the timeout here.
        match self.state {
            IdleTimeoutState::AckElicitingPacketSent(_) => {}
            IdleTimeoutState::Init | IdleTimeoutState::PacketReceived(_) => {
                self.state = IdleTimeoutState::AckElicitingPacketSent(now);
            }
        }
    }

    pub fn on_packet_received(&mut self, now: Instant) {
        // Only update if this doesn't rewind the idle timeout.
        // We sometimes process packets after caching them, which uses
        // the time the packet was received.  That could be in the past.
        let update = match self.state {
            IdleTimeoutState::Init => true,
            IdleTimeoutState::AckElicitingPacketSent(t) | IdleTimeoutState::PacketReceived(t) => {
                t <= now
            }
        };
        if update {
            self.state = IdleTimeoutState::PacketReceived(now);
        }
    }

    pub fn expired(&self, now: Instant, pto: Duration) -> bool {
        now >= self.expiry(now, pto, false)
    }

    pub fn send_keep_alive(
        &mut self,
        now: Instant,
        pto: Duration,
        tokens: &mut Vec<RecoveryToken>,
    ) -> bool {
        if !self.keep_alive_outstanding && now >= self.expiry(now, pto, true) {
            self.keep_alive_outstanding = true;
            tokens.push(RecoveryToken::KeepAlive);
            true
        } else {
            false
        }
    }

    pub fn lost_keep_alive(&mut self) {
        self.keep_alive_outstanding = false;
    }

    pub fn ack_keep_alive(&mut self) {
        self.keep_alive_outstanding = false;
    }
}
