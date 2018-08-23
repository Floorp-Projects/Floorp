extern crate mio;

use mio::{Events, Poll, Token, Ready, PollOpt};
use mio::net::TcpListener;
use std::time::Duration;

#[test]
fn run_once_with_nothing() {
    let mut events = Events::with_capacity(1024);
    let poll = Poll::new().unwrap();
    poll.poll(&mut events, Some(Duration::from_millis(100))).unwrap();
}

#[test]
fn add_then_drop() {
    let mut events = Events::with_capacity(1024);
    let l = TcpListener::bind(&"127.0.0.1:0".parse().unwrap()).unwrap();
    let poll = Poll::new().unwrap();
    poll.register(&l, Token(1), Ready::readable() | Ready::writable(), PollOpt::edge()).unwrap();
    drop(l);
    poll.poll(&mut events, Some(Duration::from_millis(100))).unwrap();

}
