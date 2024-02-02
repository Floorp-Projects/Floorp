// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::module_name_repetitions)]

use std::{
    collections::BTreeMap,
    convert::TryFrom,
    fmt::{self, Debug},
    ops::Range,
    time::{Duration, Instant},
};

use neqo_common::Datagram;
use neqo_transport::Output;

use super::{Node, Rng};

/// An iterator that shares a `Random` instance and produces uniformly
/// random `Duration`s within a specified range.
pub struct RandomDelay {
    start: Duration,
    max: u64,
    rng: Option<Rng>,
}

impl RandomDelay {
    /// Make a new random `Duration` generator.  This panics if the range provided
    /// is inverted (i.e., `bounds.start > bounds.end`), or spans 2^64
    /// or more nanoseconds.
    /// A zero-length range means that random values won't be taken from the Rng
    pub fn new(bounds: Range<Duration>) -> Self {
        let max = u64::try_from((bounds.end - bounds.start).as_nanos()).unwrap();
        Self {
            start: bounds.start,
            max,
            rng: None,
        }
    }

    pub fn set_rng(&mut self, rng: Rng) {
        self.rng = Some(rng);
    }

    pub fn next(&mut self) -> Duration {
        let mut rng = self.rng.as_ref().unwrap().borrow_mut();
        let r = rng.random_from(0..self.max);
        self.start + Duration::from_nanos(r)
    }
}

pub struct Delay {
    random: RandomDelay,
    queue: BTreeMap<Instant, Datagram>,
}

impl Delay {
    pub fn new(bounds: Range<Duration>) -> Self {
        Self {
            random: RandomDelay::new(bounds),
            queue: BTreeMap::default(),
        }
    }

    fn insert(&mut self, d: Datagram, now: Instant) {
        let mut t = now + self.random.next();
        while self.queue.contains_key(&t) {
            // This is a little inefficient, but it avoids drops on collisions,
            // which are super-common for a fixed delay.
            t += Duration::from_nanos(1);
        }
        self.queue.insert(t, d);
    }
}

impl Node for Delay {
    fn init(&mut self, rng: Rng, _now: Instant) {
        self.random.set_rng(rng);
    }

    fn process(&mut self, d: Option<Datagram>, now: Instant) -> Output {
        if let Some(dgram) = d {
            self.insert(dgram, now);
        }
        if let Some((&k, _)) = self.queue.range(..=now).next() {
            Output::Datagram(self.queue.remove(&k).unwrap())
        } else if let Some(&t) = self.queue.keys().next() {
            Output::Callback(t - now)
        } else {
            Output::None
        }
    }
}

impl Debug for Delay {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str("delay")
    }
}
