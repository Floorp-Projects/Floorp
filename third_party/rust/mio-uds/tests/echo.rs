extern crate mio;
extern crate tempdir;
extern crate mio_uds;

use std::io::{self, Write, Read};
use std::io::ErrorKind::WouldBlock;

use tempdir::TempDir;

use mio::*;
use mio_uds::*;

macro_rules! t {
    ($e:expr) => (match $e {
        Ok(e) => e,
        Err(e) => panic!("{} failed with {}", stringify!($e), e),
    })
}

const SERVER: Token = Token(0);
const CLIENT: Token = Token(1);

struct EchoConn {
    sock: UnixStream,
    buf: Vec<u8>,
    token: Option<Token>,
    interest: Ready,
}

impl EchoConn {
    fn new(sock: UnixStream) -> EchoConn {
        EchoConn {
            sock: sock,
            buf: Vec::new(),
            token: None,
            interest: Ready::readable(),
        }
    }

    fn writable(&mut self, poll: &Poll) -> io::Result<()> {
        match self.sock.write(&self.buf) {
            Ok(n) => {
                assert_eq!(n, self.buf.len());
                self.interest.insert(Ready::readable());
                self.interest.remove(Ready::writable());
            }
            Err(ref e) if e.kind() == WouldBlock => {
                self.interest.insert(Ready::writable());
            }
            Err(e) => panic!("not implemented; client err={:?}", e),
        }

        assert!(self.interest.is_readable() || self.interest.is_writable(),
                "actual={:?}", self.interest);
        poll.reregister(&self.sock, self.token.unwrap(), self.interest,
                        PollOpt::edge() | PollOpt::oneshot())
    }

    fn readable(&mut self, poll: &Poll) -> io::Result<()> {
        let mut buf = [0; 1024];

        match self.sock.read(&mut buf) {
            Ok(r) => {
                self.buf = buf[..r].to_vec();

                self.interest.remove(Ready::readable());
                self.interest.insert(Ready::writable());
            }
            Err(ref e) if e.kind() == WouldBlock => {}
            Err(_e) => {
                self.interest.remove(Ready::readable());
            }
        }

        assert!(self.interest.is_readable() || self.interest.is_writable(),
                "actual={:?}", self.interest);
        poll.reregister(&self.sock, self.token.unwrap(), self.interest,
                        PollOpt::edge() | PollOpt::oneshot())
    }
}

struct EchoServer {
    sock: UnixListener,
    conns: Vec<Option<EchoConn>>,
}

impl EchoServer {
    fn accept(&mut self, poll: &Poll) -> io::Result<()> {
        let sock = t!(self.sock.accept()).unwrap().0;
        let conn = EchoConn::new(sock);
        let tok = Token(self.conns.len() + 2);
        self.conns.push(Some(conn));

        // Register the connection
        self.conn(tok).token = Some(tok);
        t!(poll.register(&self.conn(tok).sock, tok,
                         Ready::readable(),
                         PollOpt::edge() | PollOpt::oneshot()));

        Ok(())
    }

    fn conn_readable(&mut self, poll: &Poll, tok: Token) -> io::Result<()> {
        self.conn(tok).readable(poll)
    }

    fn conn_writable(&mut self, poll: &Poll, tok: Token) -> io::Result<()> {
        self.conn(tok).writable(poll)
    }

    fn conn<'a>(&'a mut self, tok: Token) -> &'a mut EchoConn {
        self.conns[usize::from(tok) - 2].as_mut().unwrap()
    }
}

struct EchoClient {
    sock: UnixStream,
    msgs: Vec<&'static str>,
    tx: &'static [u8],
    rx: &'static [u8],
    token: Token,
    interest: Ready,
    active: bool,
}


// Sends a message and expects to receive the same exact message, one at a time
impl EchoClient {
    fn new(sock: UnixStream, tok: Token,  mut msgs: Vec<&'static str>) -> EchoClient {
        let curr = msgs.remove(0);

        EchoClient {
            sock: sock,
            msgs: msgs,
            tx: curr.as_bytes(),
            rx: curr.as_bytes(),
            token: tok,
            interest: Ready::empty(),
            active: true,
        }
    }

    fn readable(&mut self, poll: &Poll) -> io::Result<()> {
        let mut buf = [0; 1024];
        match self.sock.read(&mut buf) {
            Ok(n) => {
                assert_eq!(&self.rx[..n], &buf[..n]);
                self.rx = &self.rx[n..];

                self.interest.remove(Ready::readable());

                if self.rx.len() == 0 {
                    self.next_msg(poll).unwrap();
                }
            }
            Err(ref e) if e.kind() == WouldBlock => {}
            Err(e) => panic!("error {}", e),
        }

        if !self.interest.is_empty() {
            assert!(self.interest.is_readable() || self.interest.is_writable(),
                    "actual={:?}", self.interest);
            try!(poll.reregister(&self.sock, self.token, self.interest,
                                 PollOpt::edge() | PollOpt::oneshot()));
        }

        Ok(())
    }

    fn writable(&mut self, poll: &Poll) -> io::Result<()> {
        match self.sock.write(self.tx) {
            Ok(r) => {
                self.tx = &self.tx[r..];
                self.interest.insert(Ready::readable());
                self.interest.remove(Ready::writable());
            }
            Err(ref e) if e.kind() == WouldBlock => {
                self.interest.insert(Ready::writable());
            }
            Err(e) => panic!("not implemented; client err={:?}", e)
        }

        assert!(self.interest.is_readable() || self.interest.is_writable(),
                "actual={:?}", self.interest);
        poll.reregister(&self.sock, self.token, self.interest,
                        PollOpt::edge() | PollOpt::oneshot())
    }

    fn next_msg(&mut self, poll: &Poll) -> io::Result<()> {
        if self.msgs.is_empty() {
            self.active = false;
            return Ok(());
        }

        let curr = self.msgs.remove(0);

        self.tx = curr.as_bytes();
        self.rx = curr.as_bytes();

        self.interest.insert(Ready::writable());
        assert!(self.interest.is_readable() || self.interest.is_writable(),
                "actual={:?}", self.interest);
        poll.reregister(&self.sock, self.token, self.interest,
                        PollOpt::edge() | PollOpt::oneshot())
    }
}

struct Echo {
    server: EchoServer,
    client: EchoClient,
}

impl Echo {
    fn new(srv: UnixListener, client: UnixStream, msgs: Vec<&'static str>) -> Echo {
        Echo {
            server: EchoServer {
                sock: srv,
                conns: Vec::new(),
            },
            client: EchoClient::new(client, CLIENT, msgs)
        }
    }

    fn ready(&mut self,
             poll: &Poll,
             token: Token,
             events: Ready) {
        println!("ready {:?} {:?}", token, events);
        if events.is_readable() {
            match token {
                SERVER => self.server.accept(poll).unwrap(),
                CLIENT => self.client.readable(poll).unwrap(),
                i => self.server.conn_readable(poll, i).unwrap()
            }
        }

        if events.is_writable() {
            match token {
                SERVER => panic!("received writable for token 0"),
                CLIENT => self.client.writable(poll).unwrap(),
                _ => self.server.conn_writable(poll, token).unwrap()
            }
        }
    }
}

#[test]
fn echo_server() {
    let tmp_dir = t!(TempDir::new("mio-uds"));
    let addr = tmp_dir.path().join("sock");

    let poll = t!(Poll::new());
    let mut events = Events::with_capacity(1024);

    let srv = t!(UnixListener::bind(&addr));
    t!(poll.register(&srv,
                     SERVER,
                     Ready::readable(),
                     PollOpt::edge() | PollOpt::oneshot()));

    let sock = t!(UnixStream::connect(&addr));
    t!(poll.register(&sock,
                     CLIENT,
                     Ready::writable(),
                     PollOpt::edge() | PollOpt::oneshot()));

    let mut echo = Echo::new(srv, sock, vec!["foo", "bar"]);
    while echo.client.active {
        t!(poll.poll(&mut events, None));

        for i in 0..events.len() {
            let event = events.get(i).unwrap();
            echo.ready(&poll, event.token(), event.readiness());
        }
    }
}
