use {sleep_ms};
use mio::*;
use mio::net::{TcpListener, TcpStream};
use std::time::Duration;

const MS: u64 = 1_000;

#[test]
pub fn test_reregister_different_without_poll() {
    let mut events = Events::with_capacity(1024);
    let poll = Poll::new().unwrap();

    // Create the listener
    let l = TcpListener::bind(&"127.0.0.1:0".parse().unwrap()).unwrap();

    // Register the listener with `Poll`
    poll.register(&l, Token(0), Ready::readable(), PollOpt::edge() | PollOpt::oneshot()).unwrap();

    let s1 = TcpStream::connect(&l.local_addr().unwrap()).unwrap();
    poll.register(&s1, Token(2), Ready::readable(), PollOpt::edge()).unwrap();

    sleep_ms(MS);

    poll.reregister(&l, Token(0), Ready::writable(), PollOpt::edge() | PollOpt::oneshot()).unwrap();

    poll.poll(&mut events, Some(Duration::from_millis(MS))).unwrap();
    assert_eq!(events.len(), 0);
}
