extern crate mio;
extern crate tempdir;
extern crate mio_uds;

use std::io::prelude::*;
use std::time::Duration;

use mio::*;
use mio_uds::*;
use tempdir::TempDir;

macro_rules! t {
    ($e:expr) => (match $e {
        Ok(e) => e,
        Err(e) => panic!("{} failed with {}", stringify!($e), e),
    })
}

#[test]
fn listener() {
    let td = t!(TempDir::new("uds"));
    let a = t!(UnixListener::bind(td.path().join("foo")));
    assert!(t!(a.accept()).is_none());
    t!(a.local_addr());
    assert!(t!(a.take_error()).is_none());
    let b = t!(a.try_clone());
    assert!(t!(b.accept()).is_none());

    let poll = t!(Poll::new());
    let mut events = Events::with_capacity(1024);

    t!(poll.register(&a, Token(1), Ready::readable(), PollOpt::edge()));

    let s = t!(UnixStream::connect(td.path().join("foo")));

    assert_eq!(t!(poll.poll(&mut events, None)), 1);

    let (s2, addr) = t!(a.accept()).unwrap();

    assert_eq!(t!(s.peer_addr()).as_pathname(), t!(s2.local_addr()).as_pathname());
    assert_eq!(t!(s.local_addr()).as_pathname(), t!(s2.peer_addr()).as_pathname());
    assert_eq!(addr.as_pathname(), t!(s.local_addr()).as_pathname());
}

#[test]
fn stream() {
    let poll = t!(Poll::new());
    let mut events = Events::with_capacity(1024);
    let (mut a, mut b) = t!(UnixStream::pair());

    let both = Ready::readable() | Ready::writable();
    t!(poll.register(&a, Token(1), both, PollOpt::edge()));
    t!(poll.register(&b, Token(2), both, PollOpt::edge()));

    assert_eq!(t!(poll.poll(&mut events, Some(Duration::new(0, 0)))), 2);
    assert_eq!(events.get(0).unwrap().readiness(), Ready::writable());
    assert_eq!(events.get(1).unwrap().readiness(), Ready::writable());

    assert_eq!(t!(a.write(&[3])), 1);

    assert_eq!(t!(poll.poll(&mut events, Some(Duration::new(0, 0)))), 1);
    assert!(events.get(0).unwrap().readiness().is_readable());
    assert_eq!(events.get(0).unwrap().token(), Token(2));

    assert_eq!(t!(b.read(&mut [0; 1024])), 1);
}
