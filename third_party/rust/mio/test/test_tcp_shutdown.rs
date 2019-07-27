use std::net::Shutdown;
use std::time::Duration;

use mio::{Token, Ready, PollOpt, Poll, Events};
use mio::net::TcpStream;

macro_rules! wait {
    ($poll:ident, $ready:ident) => {{
        use std::time::Instant;

        let now = Instant::now();
        let mut events = Events::with_capacity(16);
        let mut found = false;

        while !found {
            if now.elapsed() > Duration::from_secs(5) {
                panic!("not ready");
            }

            $poll.poll(&mut events, Some(Duration::from_secs(1))).unwrap();

            for event in &events {
                #[cfg(unix)]
                {
                    use mio::unix::UnixReady;
                    assert!(!UnixReady::from(event.readiness()).is_hup());
                }

                if event.token() == Token(0) && event.readiness().$ready() {
                    found = true;
                    break;
                }
            }
        }
    }};
}

#[test]
fn test_write_shutdown() {
    let poll = Poll::new().unwrap();

    let listener = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
    let addr = listener.local_addr().unwrap();

    let mut ready = Ready::readable() | Ready::writable();

    #[cfg(unix)]
    {
        ready |= mio::unix::UnixReady::hup();
    }

    let client = TcpStream::connect(&addr).unwrap();
    poll.register(&client,
                  Token(0),
                  ready,
                  PollOpt::edge()).unwrap();

    let (socket, _) = listener.accept().unwrap();

    wait!(poll, is_writable);

    let mut events = Events::with_capacity(16);

    // Polling should not have any events
    poll.poll(&mut events, Some(Duration::from_millis(100))).unwrap();
    assert!(events.iter().next().is_none());

    println!("SHUTTING DOWN");
    // Now, shutdown the write half of the socket.
    socket.shutdown(Shutdown::Write).unwrap();

    wait!(poll, is_readable);
}
