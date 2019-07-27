use mio::*;
use mio::deprecated::{EventLoop, Handler};
use mio::net::{TcpListener, TcpStream};
use {sleep_ms};

struct TestHandler {
    tick: usize,
    state: usize,
}

impl TestHandler {
    fn new() -> TestHandler {
        TestHandler {
            tick: 0,
            state: 0,
        }
    }
}

impl Handler for TestHandler {
    type Timeout = usize;
    type Message = String;

    fn tick(&mut self, _event_loop: &mut EventLoop<TestHandler>) {
        debug!("Handler::tick()");
        self.tick += 1;

        assert_eq!(self.state, 1);
        self.state = 0;
    }

    fn ready(&mut self, _event_loop: &mut EventLoop<TestHandler>, token: Token, events: Ready) {
        debug!("READY: {:?} - {:?}", token, events);
        if events.is_readable() {
            debug!("Handler::ready() readable event");
            assert_eq!(token, Token(0));
            assert_eq!(self.state, 0);
            self.state = 1;
        }
    }
}

#[test]
pub fn test_tick() {
    debug!("Starting TEST_TICK");
    let mut event_loop = EventLoop::new().expect("Couldn't make event loop");

    let listener = TcpListener::bind(&"127.0.0.1:0".parse().unwrap()).unwrap();
    event_loop.register(&listener, Token(0), Ready::readable(), PollOpt::level()).unwrap();

    let client = TcpStream::connect(&listener.local_addr().unwrap()).unwrap();
    event_loop.register(&client, Token(1), Ready::readable(), PollOpt::edge()).unwrap();

    sleep_ms(250);

    let mut handler = TestHandler::new();

    for _ in 0..2 {
        event_loop.run_once(&mut handler, None).unwrap();
    }

    assert!(handler.tick == 2, "actual={}", handler.tick);
    assert!(handler.state == 0, "actual={}", handler.state);
}
