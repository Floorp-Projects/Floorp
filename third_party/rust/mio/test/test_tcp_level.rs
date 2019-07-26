use {expect_events, sleep_ms, TryRead};
use mio::{Events, Poll, PollOpt, Ready, Token};
use mio::event::Event;
use mio::net::{TcpListener, TcpStream};
use std::io::Write;
use std::time::Duration;

const MS: u64 = 1_000;

#[test]
pub fn test_tcp_listener_level_triggered() {
    let poll = Poll::new().unwrap();
    let mut pevents = Events::with_capacity(1024);

    // Create the listener
    let l = TcpListener::bind(&"127.0.0.1:0".parse().unwrap()).unwrap();

    // Register the listener with `Poll`
    poll.register(&l, Token(0), Ready::readable(), PollOpt::level()).unwrap();

    let s1 = TcpStream::connect(&l.local_addr().unwrap()).unwrap();
    poll.register(&s1, Token(1), Ready::readable(), PollOpt::edge()).unwrap();

    while filter(&pevents, Token(0)).is_empty() {
        poll.poll(&mut pevents, Some(Duration::from_millis(MS))).unwrap();
    }
    let events = filter(&pevents, Token(0));

    assert_eq!(events.len(), 1);
    assert_eq!(events[0], Event::new(Ready::readable(), Token(0)));

    poll.poll(&mut pevents, Some(Duration::from_millis(MS))).unwrap();
    let events = filter(&pevents, Token(0));
    assert_eq!(events.len(), 1);
    assert_eq!(events[0], Event::new(Ready::readable(), Token(0)));

    // Accept the connection then test that the events stop
    let _ = l.accept().unwrap();

    poll.poll(&mut pevents, Some(Duration::from_millis(MS))).unwrap();
    let events = filter(&pevents, Token(0));
    assert!(events.is_empty(), "actual={:?}", events);

    let s3 = TcpStream::connect(&l.local_addr().unwrap()).unwrap();
    poll.register(&s3, Token(2), Ready::readable(), PollOpt::edge()).unwrap();

    while filter(&pevents, Token(0)).is_empty() {
        poll.poll(&mut pevents, Some(Duration::from_millis(MS))).unwrap();
    }
    let events = filter(&pevents, Token(0));

    assert_eq!(events.len(), 1);
    assert_eq!(events[0], Event::new(Ready::readable(), Token(0)));

    drop(l);

    poll.poll(&mut pevents, Some(Duration::from_millis(MS))).unwrap();
    let events = filter(&pevents, Token(0));
    assert!(events.is_empty());
}

#[test]
pub fn test_tcp_stream_level_triggered() {
    drop(::env_logger::init());
    let poll = Poll::new().unwrap();
    let mut pevents = Events::with_capacity(1024);

    // Create the listener
    let l = TcpListener::bind(&"127.0.0.1:0".parse().unwrap()).unwrap();

    // Register the listener with `Poll`
    poll.register(&l, Token(0), Ready::readable(), PollOpt::edge()).unwrap();

    let mut s1 = TcpStream::connect(&l.local_addr().unwrap()).unwrap();
    poll.register(&s1, Token(1), Ready::readable() | Ready::writable(), PollOpt::level()).unwrap();

    // Sleep a bit to ensure it arrives at dest
    sleep_ms(250);

    expect_events(&poll, &mut pevents, 2, vec![
        Event::new(Ready::readable(), Token(0)),
        Event::new(Ready::writable(), Token(1)),
    ]);

    // Server side of socket
    let (mut s1_tx, _) = l.accept().unwrap();

    // Sleep a bit to ensure it arrives at dest
    sleep_ms(250);

    expect_events(&poll, &mut pevents, 2, vec![
        Event::new(Ready::writable(), Token(1))
    ]);

    // Register the socket
    poll.register(&s1_tx, Token(123), Ready::readable(), PollOpt::edge()).unwrap();

    debug!("writing some data ----------");

    // Write some data
    let res = s1_tx.write(b"hello world!");
    assert!(res.unwrap() > 0);

    // Sleep a bit to ensure it arrives at dest
    sleep_ms(250);

    debug!("looking at rx end ----------");

    // Poll rx end
    expect_events(&poll, &mut pevents, 2, vec![
        Event::new(Ready::readable(), Token(1))
    ]);

    debug!("reading ----------");

    // Reading the data should clear it
    let mut res = vec![];
    while s1.try_read_buf(&mut res).unwrap().is_some() {
    }

    assert_eq!(res, b"hello world!");

    debug!("checking just read ----------");

    expect_events(&poll, &mut pevents, 1, vec![
        Event::new(Ready::writable(), Token(1))]);

    // Closing the socket clears all active level events
    drop(s1);

    debug!("checking everything is gone ----------");

    poll.poll(&mut pevents, Some(Duration::from_millis(MS))).unwrap();
    let events = filter(&pevents, Token(1));
    assert!(events.is_empty());
}

fn filter(events: &Events, token: Token) -> Vec<Event> {
    (0..events.len()).map(|i| events.get(i).unwrap())
                     .filter(|e| e.token() == token)
                     .collect()
}
