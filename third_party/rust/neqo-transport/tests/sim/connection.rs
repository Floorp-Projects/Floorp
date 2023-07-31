// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::module_name_repetitions)]

use super::{Node, Rng};
use neqo_common::{event::Provider, qdebug, qtrace, Datagram};
use neqo_crypto::AuthenticationStatus;
use neqo_transport::{
    Connection, ConnectionEvent, ConnectionParameters, Output, State, StreamId, StreamType,
};
use std::{
    cmp::min,
    fmt::{self, Debug},
    time::Instant,
};

/// The status of the processing of an event.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum GoalStatus {
    /// The event didn't result in doing anything; the goal is waiting for something.
    Waiting,
    /// An action was taken as a result of the event.
    Active,
    /// The goal was accomplished.
    Done,
}

/// A goal for the connection.
/// Goals can be accomplished in any order.
pub trait ConnectionGoal {
    fn init(&mut self, _c: &mut Connection, _now: Instant) {}
    /// Perform some processing.
    fn process(&mut self, _c: &mut Connection, _now: Instant) -> GoalStatus {
        GoalStatus::Waiting
    }
    /// Handle an event from the provided connection, returning `true` when the
    /// goal is achieved.
    fn handle_event(&mut self, c: &mut Connection, e: &ConnectionEvent, now: Instant)
        -> GoalStatus;
}

pub struct ConnectionNode {
    c: Connection,
    goals: Vec<Box<dyn ConnectionGoal>>,
}

impl ConnectionNode {
    pub fn new_client(
        params: ConnectionParameters,
        goals: impl IntoIterator<Item = Box<dyn ConnectionGoal>>,
    ) -> Self {
        Self {
            c: test_fixture::new_client(params),
            goals: goals.into_iter().collect(),
        }
    }

    pub fn new_server(
        params: ConnectionParameters,
        goals: impl IntoIterator<Item = Box<dyn ConnectionGoal>>,
    ) -> Self {
        Self {
            c: test_fixture::new_server(test_fixture::DEFAULT_ALPN, params),
            goals: goals.into_iter().collect(),
        }
    }

    pub fn default_client(goals: impl IntoIterator<Item = Box<dyn ConnectionGoal>>) -> Self {
        Self::new_client(ConnectionParameters::default(), goals)
    }

    pub fn default_server(goals: impl IntoIterator<Item = Box<dyn ConnectionGoal>>) -> Self {
        Self::new_server(ConnectionParameters::default(), goals)
    }

    #[allow(dead_code)]
    pub fn clear_goals(&mut self) {
        self.goals.clear();
    }

    #[allow(dead_code)]
    pub fn add_goal(&mut self, goal: Box<dyn ConnectionGoal>) {
        self.goals.push(goal);
    }

    /// Process all goals using the given closure and return whether any were active.
    fn process_goals<F>(&mut self, mut f: F) -> bool
    where
        F: FnMut(&mut Box<dyn ConnectionGoal>, &mut Connection) -> GoalStatus,
    {
        // Waiting on drain_filter...
        // self.goals.drain_filter(|g| f(g,  &mut self.c, &e)).count();
        let mut active = false;
        let mut i = 0;
        while i < self.goals.len() {
            let status = f(&mut self.goals[i], &mut self.c);
            if status == GoalStatus::Done {
                self.goals.remove(i);
                active = true;
            } else {
                active |= status == GoalStatus::Active;
                i += 1;
            }
        }
        active
    }
}

impl Node for ConnectionNode {
    fn init(&mut self, _rng: Rng, now: Instant) {
        for g in &mut self.goals {
            g.init(&mut self.c, now);
        }
    }

    fn process(&mut self, mut d: Option<Datagram>, now: Instant) -> Output {
        _ = self.process_goals(|goal, c| goal.process(c, now));
        loop {
            let res = self.c.process(d.take(), now);

            let mut active = false;
            while let Some(e) = self.c.next_event() {
                qtrace!([self.c], "received event {:?}", e);

                // Perform authentication automatically.
                if matches!(e, ConnectionEvent::AuthenticationNeeded) {
                    self.c.authenticated(AuthenticationStatus::Ok, now);
                }

                active |= self.process_goals(|goal, c| goal.handle_event(c, &e, now));
            }
            // Exit at this point if the connection produced a datagram.
            // We also exit if none of the goals were active, as there is
            // no point trying again if they did nothing.
            if matches!(res, Output::Datagram(_)) || !active {
                return res;
            }
            qdebug!([self.c], "no datagram and goal activity, looping");
        }
    }

    fn done(&self) -> bool {
        self.goals.is_empty()
    }

    fn print_summary(&self, test_name: &str) {
        println!("{}: {:?}", test_name, self.c.stats());
    }
}

impl Debug for ConnectionNode {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Display::fmt(&self.c, f)
    }
}

#[derive(Debug, Clone)]
pub struct ReachState {
    target: State,
}

impl ReachState {
    pub fn new(target: State) -> Self {
        Self { target }
    }
}

impl ConnectionGoal for ReachState {
    fn handle_event(
        &mut self,
        _c: &mut Connection,
        e: &ConnectionEvent,
        _now: Instant,
    ) -> GoalStatus {
        if matches!(e, ConnectionEvent::StateChange(state) if *state == self.target) {
            GoalStatus::Done
        } else {
            GoalStatus::Waiting
        }
    }
}

#[derive(Debug)]
pub struct SendData {
    remaining: usize,
    stream_id: Option<StreamId>,
}

impl SendData {
    pub fn new(amount: usize) -> Self {
        Self {
            remaining: amount,
            stream_id: None,
        }
    }

    fn make_stream(&mut self, c: &mut Connection) {
        if self.stream_id.is_none() {
            if let Ok(stream_id) = c.stream_create(StreamType::UniDi) {
                qdebug!([c], "made stream {} for sending", stream_id);
                self.stream_id = Some(stream_id);
            }
        }
    }

    fn send(&mut self, c: &mut Connection, stream_id: StreamId) -> GoalStatus {
        const DATA: &[u8] = &[0; 4096];
        let mut status = GoalStatus::Waiting;
        loop {
            let end = min(self.remaining, DATA.len());
            let sent = c.stream_send(stream_id, &DATA[..end]).unwrap();
            if sent == 0 {
                return status;
            }
            self.remaining -= sent;
            qtrace!("sent {} remaining {}", sent, self.remaining);
            if self.remaining == 0 {
                c.stream_close_send(stream_id).unwrap();
                return GoalStatus::Done;
            }
            status = GoalStatus::Active;
        }
    }
}

impl ConnectionGoal for SendData {
    fn init(&mut self, c: &mut Connection, _now: Instant) {
        self.make_stream(c);
    }

    fn process(&mut self, c: &mut Connection, _now: Instant) -> GoalStatus {
        self.stream_id
            .map_or(GoalStatus::Waiting, |stream_id| self.send(c, stream_id))
    }

    fn handle_event(
        &mut self,
        c: &mut Connection,
        e: &ConnectionEvent,
        _now: Instant,
    ) -> GoalStatus {
        match e {
            ConnectionEvent::SendStreamCreatable {
                stream_type: StreamType::UniDi,
            }
            // TODO(mt): remove the second condition when #842 is fixed.
            | ConnectionEvent::StateChange(_) => {
                self.make_stream(c);
                GoalStatus::Active
            }

            ConnectionEvent::SendStreamWritable { stream_id }
                if Some(*stream_id) == self.stream_id =>
            {
                self.send(c, *stream_id)
            }

            // If we sent data in 0-RTT, then we didn't track how much we should
            // have sent.  This is trivial to fix if 0-RTT testing is ever needed.
            ConnectionEvent::ZeroRttRejected => panic!("not supported"),
            _ => GoalStatus::Waiting,
        }
    }
}

/// Receive a prescribed amount of data from any stream.
#[derive(Debug)]
pub struct ReceiveData {
    remaining: usize,
}

impl ReceiveData {
    pub fn new(amount: usize) -> Self {
        Self { remaining: amount }
    }

    fn recv(&mut self, c: &mut Connection, stream_id: StreamId) -> GoalStatus {
        let mut buf = vec![0; 4096];
        let mut status = GoalStatus::Waiting;
        loop {
            let end = min(self.remaining, buf.len());
            let (recvd, _) = c.stream_recv(stream_id, &mut buf[..end]).unwrap();
            qtrace!("received {} remaining {}", recvd, self.remaining);
            if recvd == 0 {
                return status;
            }
            self.remaining -= recvd;
            if self.remaining == 0 {
                return GoalStatus::Done;
            }
            status = GoalStatus::Active;
        }
    }
}

impl ConnectionGoal for ReceiveData {
    fn handle_event(
        &mut self,
        c: &mut Connection,
        e: &ConnectionEvent,
        _now: Instant,
    ) -> GoalStatus {
        if let ConnectionEvent::RecvStreamReadable { stream_id } = e {
            self.recv(c, *stream_id)
        } else {
            GoalStatus::Waiting
        }
    }
}
