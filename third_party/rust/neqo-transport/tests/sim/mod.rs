// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Tests with simulated network
#![cfg_attr(feature = "deny-warnings", deny(warnings))]
#![warn(clippy::pedantic)]

pub mod connection;
mod delay;
mod drop;
pub mod rng;
mod taildrop;

use neqo_common::{qdebug, qinfo, qtrace, Datagram, Encoder};
use neqo_transport::Output;
use rng::Random;
use std::cell::RefCell;
use std::cmp::min;
use std::convert::TryFrom;
use std::fmt::Debug;
use std::rc::Rc;
use std::time::{Duration, Instant};
use test_fixture::{self, now};

use NodeState::{Active, Idle, Waiting};

pub mod network {
    pub use super::delay::Delay;
    pub use super::drop::Drop;
    pub use super::taildrop::TailDrop;
}

type Rng = Rc<RefCell<Random>>;

/// A macro that turns a list of values into boxed versions of the same.
#[macro_export]
macro_rules! boxed {
    [$($v:expr),+ $(,)?] => {
        vec![ $( Box::new($v) as _ ),+ ]
    };
}

/// Create a simulation test case.  This takes either two or three arguments.
/// The two argument form takes a bare name (`ident`), a comma, and an array of
/// items that implement `Node`.
/// The three argument form adds a setup block that can be used to construct a
/// complex value that is then shared between all nodes.  The values in the
/// three-argument form have to be closures (or functions) that accept a reference
/// to the value returned by the setup.
#[macro_export]
macro_rules! simulate {
    ($n:ident, [ $($v:expr),+ $(,)? ] $(,)?) => {
        simulate!($n, (), [ $(|_| $v),+ ]);
    };
    ($n:ident, $setup:expr, [ $( $v:expr ),+ $(,)? ] $(,)?) => {
        #[test]
        fn $n() {
            let fixture = $setup;
            let mut nodes: Vec<Box<dyn $crate::sim::Node>> = Vec::new();
            $(
                let f: Box<dyn FnOnce(&_) -> _> = Box::new($v);
                nodes.push(Box::new(f(&fixture)));
            )*
            let mut sim = Simulator::new(stringify!($n), nodes);
            if let Ok(seed) = std::env::var("SIMULATION_SEED") {
                sim.seed_str(seed);
            }
            sim.run();
        }
    };
}

pub trait Node: Debug {
    fn init(&mut self, _rng: Rng, _now: Instant) {}
    /// Perform processing.  This optionally takes a datagram and produces either
    /// another data, a time that the simulator needs to wait, or nothing.
    fn process(&mut self, d: Option<Datagram>, now: Instant) -> Output;
    /// An node can report when it considers itself "done".
    fn done(&self) -> bool {
        true
    }
    fn print_summary(&self, _test_name: &str) {}
}

/// The state of a single node.  Nodes will be activated if they are `Active`
/// or if the previous node in the loop generated a datagram.  Nodes that return
/// `true` from `Node::done` will be activated as normal.
#[derive(Debug, PartialEq)]
enum NodeState {
    /// The node just produced a datagram.  It should be activated again as soon as possible.
    Active,
    /// The node is waiting.
    Waiting(Instant),
    /// The node became idle.
    Idle,
}

#[derive(Debug)]
struct NodeHolder {
    node: Box<dyn Node>,
    state: NodeState,
}

impl NodeHolder {
    fn ready(&self, now: Instant) -> bool {
        match self.state {
            Active => true,
            Waiting(t) => t >= now,
            Idle => false,
        }
    }
}

pub struct Simulator {
    name: String,
    nodes: Vec<NodeHolder>,
    rng: Rng,
}

impl Simulator {
    pub fn new(name: impl AsRef<str>, nodes: impl IntoIterator<Item = Box<dyn Node>>) -> Self {
        let name = String::from(name.as_ref());
        // The first node is marked as Active, the rest are idle.
        let mut it = nodes.into_iter();
        let nodes = it
            .next()
            .map(|node| NodeHolder {
                node,
                state: Active,
            })
            .into_iter()
            .chain(it.map(|node| NodeHolder { node, state: Idle }))
            .collect::<Vec<_>>();
        Self {
            name,
            nodes,
            rng: Rc::default(),
        }
    }

    pub fn seed(&mut self, seed: [u8; 32]) {
        self.rng = Rc::new(RefCell::new(Random::new(seed)));
    }

    /// Seed from a hex string.
    /// Though this is convenient, it panics if this isn't a 64 character hex string.
    pub fn seed_str(&mut self, seed: impl AsRef<str>) {
        let seed = Encoder::from_hex(seed);
        self.seed(<[u8; 32]>::try_from(seed.as_ref()).unwrap());
    }

    fn next_time(&self, now: Instant) -> Instant {
        let mut next = None;
        for n in &self.nodes {
            match n.state {
                Idle => continue,
                Active => return now,
                Waiting(a) => next = Some(next.map_or(a, |b| min(a, b))),
            }
        }
        next.expect("a node cannot be idle and not done")
    }

    /// Runs the simulation.
    pub fn run(mut self) -> Duration {
        let start = now();
        let mut now = start;
        let mut dgram = None;

        for n in &mut self.nodes {
            n.node.init(self.rng.clone(), now);
        }
        println!("{}: seed {}", self.name, self.rng.borrow().seed_str());

        let real_start = Instant::now();
        loop {
            for n in &mut self.nodes {
                if dgram.is_none() && !n.ready(now) {
                    qdebug!([self.name], "skipping {:?}", n.node);
                    continue;
                }

                qdebug!([self.name], "processing {:?}", n.node);
                let res = n.node.process(dgram.take(), now);
                n.state = match res {
                    Output::Datagram(d) => {
                        qtrace!([self.name], " => datagram {}", d.len());
                        dgram = Some(d);
                        Active
                    }
                    Output::Callback(delay) => {
                        qtrace!([self.name], " => callback {:?}", delay);
                        assert_ne!(delay, Duration::new(0, 0));
                        Waiting(now + delay)
                    }
                    Output::None => {
                        qtrace!([self.name], " => nothing");
                        assert!(n.node.done(), "nodes have to be done when they go idle");
                        Idle
                    }
                };
            }

            if self.nodes.iter().all(|n| n.node.done()) {
                let real_elapsed = Instant::now() - real_start;
                println!("{}: real elapsed time: {:?}", self.name, real_elapsed);
                let elapsed = now - start;
                println!("{}: simulated elapsed time: {:?}", self.name, elapsed);
                for n in &self.nodes {
                    n.node.print_summary(&self.name);
                }
                return elapsed;
            }

            if dgram.is_none() {
                let next = self.next_time(now);
                if next > now {
                    qinfo!(
                        [self.name],
                        "advancing time by {:?} to {:?}",
                        next - now,
                        next - start
                    );
                    now = next;
                }
            }
        }
    }
}
