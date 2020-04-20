use std::collections::HashMap;
use std::net::{self, Shutdown};
use std::time::{Duration, Instant};

use mio::{Token, Ready, PollOpt, Poll, Events};
use mio::event::{Evented, Event};
use mio::net::TcpStream;

struct TestPoll {
    poll: Poll,
    events: Events,
    buf: HashMap<Token, Ready>,
}

impl TestPoll {
    fn new() -> TestPoll {
        TestPoll {
            poll: Poll::new().unwrap(),
            events: Events::with_capacity(1024),
            buf: HashMap::new(),
        }
    }

    fn register<E: ?Sized>(&self, handle: &E, token: Token, interest: Ready, opts: PollOpt)
        where E: Evented
    {
        self.poll.register(handle, token, interest, opts).unwrap();
    }

    fn wait_for(&mut self, token: Token, ready: Ready) -> Result<(), &'static str> {
        let now = Instant::now();

        loop {
            if now.elapsed() > Duration::from_secs(1) {
                return Err("not ready");
            }

            if let Some(curr) = self.buf.get(&token) {
                if curr.contains(ready) {
                    break;
                }
            }

            self.poll.poll(&mut self.events, Some(Duration::from_millis(250))).unwrap();

            for event in &self.events {
                let curr = self.buf.entry(event.token())
                    .or_insert(Ready::empty());

                *curr |= event.readiness();
            }
        }

        *self.buf.get_mut(&token).unwrap() -= ready;
        Ok(())
    }

    fn check_idle(&mut self) -> Result<(), Event> {
        self.poll.poll(&mut self.events, Some(Duration::from_millis(100))).unwrap();

        if let Some(e) = self.events.iter().next() {
            Err(e)
        } else {
            Ok(())
        }
    }
}

macro_rules! assert_ready {
    ($poll:expr, $token:expr, $ready:expr) => {{
        match $poll.wait_for($token, $ready) {
            Ok(_) => {}
            Err(_) => panic!("not ready; token = {:?}; interest = {:?}", $token, $ready),
        }
    }}
}

macro_rules! assert_not_ready {
    ($poll:expr, $token:expr, $ready:expr) => {{
        match $poll.wait_for($token, $ready) {
            Ok(_) => panic!("is ready; token = {:?}; interest = {:?}", $token, $ready),
            Err(_) => {}
        }
    }}
}

macro_rules! assert_hup_ready {
    ($poll:expr) => {
        #[cfg(unix)]
        {
            use mio::unix::UnixReady;
            assert_ready!($poll, Token(0), Ready::from(UnixReady::hup()))
        }
    }
}

macro_rules! assert_not_hup_ready {
    ($poll:expr) => {
        #[cfg(unix)]
        {
            use mio::unix::UnixReady;
            assert_not_ready!($poll, Token(0), Ready::from(UnixReady::hup()))
        }
    }
}

macro_rules! assert_idle {
    ($poll:expr) => {
        match $poll.check_idle() {
            Ok(()) => {}
            Err(e) => panic!("not idle; event = {:?}", e),
        }
    }
}

// TODO: replace w/ assertive
// https://github.com/carllerche/assertive
macro_rules! assert_ok {
    ($e:expr) => {
        assert_ok!($e,)
    };
    ($e:expr,) => {{
        use std::result::Result::*;
        match $e {
            Ok(v) => v,
            Err(e) => panic!("assertion failed: error = {:?}", e),
        }
    }};
    ($e:expr, $($arg:tt)+) => {{
        use std::result::Result::*;
        match $e {
            Ok(v) => v,
            Err(e) => panic!("assertion failed: error = {:?}: {}", e, format_args!($($arg)+)),
        }
    }};
}

#[test]
fn test_write_shutdown() {
    use std::io::prelude::*;

    let mut poll = TestPoll::new();
    let mut buf = [0; 1024];

    let listener = assert_ok!(net::TcpListener::bind("127.0.0.1:0"));
    let addr = assert_ok!(listener.local_addr());

    let mut client = assert_ok!(TcpStream::connect(&addr));
    poll.register(&client,
                  Token(0),
                  Ready::readable() | Ready::writable(),
                  PollOpt::edge());

    let (socket, _) = assert_ok!(listener.accept());

    assert_ready!(poll, Token(0), Ready::writable());

    // Polling should not have any events
    assert_idle!(poll);

    // Now, shutdown the write half of the socket.
    assert_ok!(socket.shutdown(Shutdown::Write));

    assert_ready!(poll, Token(0), Ready::readable());

    assert_not_hup_ready!(poll);

    let n = assert_ok!(client.read(&mut buf));
    assert_eq!(n, 0);
}

#[test]
fn test_graceful_shutdown() {
    use std::io::prelude::*;

    let mut poll = TestPoll::new();
    let mut buf = [0; 1024];

    let listener = assert_ok!(net::TcpListener::bind("127.0.0.1:0"));
    let addr = assert_ok!(listener.local_addr());

    let mut client = assert_ok!(TcpStream::connect(&addr));
    poll.register(&client,
                  Token(0),
                  Ready::readable() | Ready::writable(),
                  PollOpt::edge());

    let (mut socket, _) = assert_ok!(listener.accept());

    assert_ready!(poll, Token(0), Ready::writable());

    // Polling should not have any events
    assert_idle!(poll);

    // Now, shutdown the write half of the socket.
    assert_ok!(client.shutdown(Shutdown::Write));

    let n = assert_ok!(socket.read(&mut buf));
    assert_eq!(0, n);
    drop(socket);

    assert_ready!(poll, Token(0), Ready::readable());
    #[cfg(not(any(target_os = "bitrig", target_os = "dragonfly",
        target_os = "freebsd", target_os = "ios", target_os = "macos",
        target_os = "netbsd", target_os = "openbsd")))]
    assert_hup_ready!(poll);

    let mut buf = [0; 1024];
    let n = assert_ok!(client.read(&mut buf));
    assert_eq!(n, 0);
}

#[test]
fn test_abrupt_shutdown() {
    use net2::TcpStreamExt;
    use std::io::Read;

    let mut poll = TestPoll::new();
    let mut buf = [0; 1024];

    let listener = assert_ok!(net::TcpListener::bind("127.0.0.1:0"));
    let addr = assert_ok!(listener.local_addr());

    let mut client = assert_ok!(TcpStream::connect(&addr));
    poll.register(&client,
                  Token(0),
                  Ready::readable() | Ready::writable(),
                  PollOpt::edge());

    let (socket, _) = assert_ok!(listener.accept());
    assert_ok!(socket.set_linger(Some(Duration::from_millis(0))));
    // assert_ok!(socket.set_linger(None));

    // Wait to be connected
    assert_ready!(poll, Token(0), Ready::writable());

    drop(socket);

    #[cfg(not(any(target_os = "bitrig", target_os = "dragonfly",
        target_os = "freebsd", target_os = "ios", target_os = "macos",
        target_os = "netbsd", target_os = "openbsd")))]
    assert_hup_ready!(poll);
    assert_ready!(poll, Token(0), Ready::writable());
    assert_ready!(poll, Token(0), Ready::readable());

    let res = client.read(&mut buf);
    assert!(res.is_err(), "not err = {:?}", res);
}
