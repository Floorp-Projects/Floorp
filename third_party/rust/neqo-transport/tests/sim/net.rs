// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::rng::RandomDuration;
use super::{Node, Rng};
use neqo_common::Datagram;
use neqo_transport::Output;
use std::collections::BTreeMap;
use std::fmt::{self, Debug};
use std::iter;
use std::ops::Range;
use std::time::{Duration, Instant};

/// 
pub struct RandomDrop {
    threshold: u64,
    max: u64,
    rng: Rng,
}

impl RandomDuration {
    /// Make a new random `Duration` generator.  This asserts if the range provided
    /// is inverted (i.e., `bounds.start > bounds.end`), or spans 2^64
    /// or more nanoseconds.
    /// A zero-length range means that random values won't be taken from the Rng
    pub fn new(bounds: Range<Duration>, rng: Rng) -> Self {
        let max = u64::try_from((bounds.end - bounds.start).as_nanos()).unwrap();
        Self {
            start: bounds.start,
            max,
            rng,
        }
    }

    fn next(&mut self) -> Duration {
        let r = if self.max == 0 {
            Duration::new(0, 0)
        } else {
            self.rng.borrow_mut().random_from(0..self.max)
        }
        self.start + Duration::from_nanos(r)
    }
}

enum DelayState {
    New(Range<Duration>),
    Ready(RandomDuration),
}

pub struct Delay {
    state: DelayState,
    queue: BTreeMap<Instant, Datagram>,
}

impl Delay
{
    pub fn new(bounds: Range<Duration>) -> Self
    {
        Self {
            State: DelayState::New(bounds),
            queue: BTreeMap::default(),
        }
    }

    fn insert(&mut self, d: Datagram, now: Instant) {
        let mut t = if let State::Ready(r) = self.state {
        now + self.source.next()
        } else {
            unreachable!();
        }
        while self.queue.contains_key(&t) {
            // This is a little inefficient, but it avoids drops on collisions,
            // which are super-common for a fixed delay.
            t += Duration::from_nanos(1);
        }
        self.queue.insert(t, d);
    }
}

impl Node for Delay
{
    fn init(&mut self, rng: Rng, now: Instant) {
        if let DelayState::New(bounds) = self.state {
            self.state = RandomDuration::new(bounds);
        } else {
            unreachable!();
        }
    }

    fn process(&mut self, d: Option<Datagram>, now: Instant) -> Output {
        if let Some(dgram) = d {
            self.insert(dgram, now);
        }
        if let Some((&k, _)) = self.queue.range(..now).nth(0) {
            Output::Datagram(self.queue.remove(&k).unwrap())
        } else if let Some(&t) = self.queue.keys().nth(0) {
            Output::Callback(t - now)
        } else {
            Output::None
        }
    }
}

impl<T> Debug for Delay<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str("delay")
    }
}
