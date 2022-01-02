// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::module_name_repetitions)]

use super::Node;
use neqo_common::{qtrace, Datagram};
use neqo_transport::Output;
use std::cmp::max;
use std::collections::VecDeque;
use std::convert::TryFrom;
use std::fmt::{self, Debug};
use std::time::{Duration, Instant};

/// One second in nanoseconds.
const ONE_SECOND_NS: u128 = 1_000_000_000;

/// This models a link with a tail drop router at the front of it.
pub struct TailDrop {
    /// An overhead associated with each entry.  This accounts for
    /// layer 2, IP, and UDP overheads.
    overhead: usize,
    /// The rate at which bytes egress the link, in bytes per second.
    rate: usize,
    /// The depth of the queue, in bytes.
    capacity: usize,

    /// A counter for how many bytes are enqueued.
    used: usize,
    /// A queue of unsent bytes.
    queue: VecDeque<Datagram>,
    /// The time that the next datagram can enter the link.
    next_deque: Option<Instant>,

    /// Any sub-ns delay from the last enqueue.
    sub_ns_delay: u32,
    /// The time it takes a byte to exit the other end of the link.
    delay: Duration,
    /// The packets that are on the link and when they can be delivered.
    on_link: VecDeque<(Instant, Datagram)>,

    /// The number of packets received.
    received: usize,
    /// The number of packets dropped.
    dropped: usize,
    /// The number of packets delivered.
    delivered: usize,
    /// The maximum amount of queue capacity ever used.
    /// As packets leave the queue as soon as they start being used, this doesn't
    /// count them.
    maxq: usize,
}

impl TailDrop {
    /// Make a new taildrop node with the given rate, queue capacity, and link delay.
    pub fn new(rate: usize, capacity: usize, delay: Duration) -> Self {
        Self {
            overhead: 64,
            rate,
            capacity,
            used: 0,
            queue: VecDeque::new(),
            next_deque: None,
            sub_ns_delay: 0,
            delay,
            on_link: VecDeque::new(),
            received: 0,
            dropped: 0,
            delivered: 0,
            maxq: 0,
        }
    }

    /// A tail drop queue on a 10Mbps link (approximated to 1 million bytes per second)
    /// with a fat 32k buffer (about 30ms), and the default forward delay of 50ms.
    pub fn dsl_uplink() -> Self {
        TailDrop::new(1_000_000, 32_768, Duration::from_millis(50))
    }

    /// Cut downlink to one fifth of the uplink (2Mbps), and reduce the buffer to 1/4.
    pub fn dsl_downlink() -> Self {
        TailDrop::new(200_000, 8_192, Duration::from_millis(50))
    }

    /// How "big" is this datagram, accounting for overheads.
    /// This approximates by using the same overhead for storing in the queue
    /// and for sending on the wire.
    fn size(&self, d: &Datagram) -> usize {
        d.len() + self.overhead
    }

    /// Start sending a datagram.
    fn send(&mut self, d: Datagram, now: Instant) {
        // How many bytes are we "transmitting"?
        let sz = u128::try_from(self.size(&d)).unwrap();

        // Calculate how long it takes to put the packet on the link.
        // Perform the calculation based on 2^32 seconds and save any remainder.
        // This ensures that high rates and small packets don't result in rounding
        // down times too badly.
        // Duration consists of a u64 and a u32, so we have 32 high bits to spare.
        let t = sz * (ONE_SECOND_NS << 32) / u128::try_from(self.rate).unwrap()
            + u128::from(self.sub_ns_delay);
        let send_ns = u64::try_from(t >> 32).unwrap();
        assert_ne!(send_ns, 0, "sending a packet takes <1ns");
        self.sub_ns_delay = u32::try_from(t & u128::from(u32::MAX)).unwrap();
        let deque_time = now + Duration::from_nanos(send_ns);
        self.next_deque = Some(deque_time);

        // Now work out when the packet is fully received at the other end of
        // the link. Setup to deliver the packet then.
        let delivery_time = deque_time + self.delay;
        self.on_link.push_back((delivery_time, d));
    }

    /// Enqueue for sending.  Maybe.  If this overflows the queue, drop it instead.
    fn maybe_enqueue(&mut self, d: Datagram, now: Instant) {
        self.received += 1;
        if self.next_deque.is_none() {
            // Nothing in the queue and nothing still sending.
            debug_assert!(self.queue.is_empty());
            self.send(d, now);
        } else if self.used + self.size(&d) <= self.capacity {
            self.used += self.size(&d);
            self.maxq = max(self.maxq, self.used);
            self.queue.push_back(d);
        } else {
            qtrace!("taildrop dropping {} bytes", d.len());
            self.dropped += 1;
        }
    }

    /// If the last packet that was sending has been sent, start sending
    /// the next one.
    fn maybe_send(&mut self, now: Instant) {
        if self.next_deque.as_ref().map_or(false, |t| *t <= now) {
            if let Some(d) = self.queue.pop_front() {
                self.used -= self.size(&d);
                self.send(d, now);
            } else {
                self.next_deque = None;
                self.sub_ns_delay = 0;
            }
        }
    }
}

impl Node for TailDrop {
    fn process(&mut self, d: Option<Datagram>, now: Instant) -> Output {
        if let Some(dgram) = d {
            self.maybe_enqueue(dgram, now);
        }

        self.maybe_send(now);

        if let Some((t, _)) = self.on_link.front() {
            if *t <= now {
                let (_, d) = self.on_link.pop_front().unwrap();
                self.delivered += 1;
                Output::Datagram(d)
            } else {
                Output::Callback(*t - now)
            }
        } else {
            Output::None
        }
    }

    fn print_summary(&self, test_name: &str) {
        println!(
            "{}: taildrop: rx {} drop {} tx {} maxq {}",
            test_name, self.received, self.dropped, self.delivered, self.maxq,
        );
    }
}

impl Debug for TailDrop {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str("taildrop")
    }
}
