use {TryRead, TryWrite};
use mio::*;
use mio::deprecated::{EventLoop, Handler};
use mio::deprecated::unix::*;
use bytes::{Buf, ByteBuf, SliceBuf};
use slab::Slab;
use std::path::PathBuf;
use std::io::{self, Read};
use std::os::unix::io::{AsRawFd, FromRawFd};
use tempdir::TempDir;

const SERVER: Token = Token(10_000_000);
const CLIENT: Token = Token(10_000_001);

struct EchoConn {
    sock: UnixStream,
    pipe_fd: Option<PipeReader>,
    token: Option<Token>,
    interest: Ready,
}

impl EchoConn {
    fn new(sock: UnixStream) -> EchoConn {
        EchoConn {
            sock: sock,
            pipe_fd: None,
            token: None,
            interest: Ready::hup(),
        }
    }

    fn writable(&mut self, event_loop: &mut EventLoop<Echo>) -> io::Result<()> {
        let fd = self.pipe_fd.take().unwrap();

        match self.sock.try_write_send_fd(b"x", fd.as_raw_fd()) {
            Ok(None) => {
                debug!("client flushing buf; WOULDBLOCK");

                self.pipe_fd = Some(fd);
                self.interest.insert(Ready::writable());
            }
            Ok(Some(r)) => {
                debug!("CONN : we wrote {} bytes!", r);

                self.interest.insert(Ready::readable());
                self.interest.remove(Ready::writable());
            }
            Err(e) => debug!("not implemented; client err={:?}", e),
        }

        assert!(self.interest.is_readable() || self.interest.is_writable(), "actual={:?}", self.interest);
        event_loop.reregister(&self.sock, self.token.unwrap(), self.interest, PollOpt::edge() | PollOpt::oneshot())
    }

    fn readable(&mut self, event_loop: &mut EventLoop<Echo>) -> io::Result<()> {
        let mut buf = ByteBuf::mut_with_capacity(2048);

        match self.sock.try_read_buf(&mut buf) {
            Ok(None) => {
                panic!("We just got readable, but were unable to read from the socket?");
            }
            Ok(Some(r)) => {
                debug!("CONN : we read {} bytes!", r);
                self.interest.remove(Ready::readable());
                self.interest.insert(Ready::writable());
            }
            Err(e) => {
                debug!("not implemented; client err={:?}", e);
                self.interest.remove(Ready::readable());
            }

        };

        // create fd to pass back. Assume that the write will work
        // without blocking, for simplicity -- we're only testing that
        // the FD makes it through somehow
        let (rd, mut wr) = pipe().unwrap();
        let mut buf = buf.flip();
        match wr.try_write_buf(&mut buf) {
            Ok(None) => {
                panic!("writing to our own pipe blocked :(");
            }
            Ok(Some(r)) => {
                debug!("CONN: we wrote {} bytes to the FD", r);
            }
            Err(e) => {
                panic!("not implemented; client err={:?}", e);
            }
        }
        self.pipe_fd = Some(rd);

        assert!(self.interest.is_readable() || self.interest.is_writable(), "actual={:?}", self.interest);
        event_loop.reregister(&self.sock, self.token.unwrap(), self.interest, PollOpt::edge() | PollOpt::oneshot())
    }
}

struct EchoServer {
    sock: UnixListener,
    conns: Slab<EchoConn>
}

impl EchoServer {
    fn accept(&mut self, event_loop: &mut EventLoop<Echo>) -> io::Result<()> {
        debug!("server accepting socket");

        let sock = self.sock.accept().unwrap();
        let conn = EchoConn::new(sock);
        let tok = self.conns.insert(conn);

        // Register the connection
        self.conns[tok].token = Some(Token(tok));
        event_loop.register(&self.conns[tok].sock, Token(tok), Ready::readable(), PollOpt::edge() | PollOpt::oneshot())
            .expect("could not register socket with event loop");

        Ok(())
    }

    fn conn_readable(&mut self, event_loop: &mut EventLoop<Echo>, tok: Token) -> io::Result<()> {
        debug!("server conn readable; tok={:?}", tok);
        self.conn(tok).readable(event_loop)
    }

    fn conn_writable(&mut self, event_loop: &mut EventLoop<Echo>, tok: Token) -> io::Result<()> {
        debug!("server conn writable; tok={:?}", tok);
        self.conn(tok).writable(event_loop)
    }

    fn conn<'a>(&'a mut self, tok: Token) -> &'a mut EchoConn {
        &mut self.conns[tok.into()]
    }
}

struct EchoClient {
    sock: UnixStream,
    msgs: Vec<&'static str>,
    tx: SliceBuf<'static>,
    rx: SliceBuf<'static>,
    token: Token,
    interest: Ready,
}


// Sends a message and expects to receive the same exact message, one at a time
impl EchoClient {
    fn new(sock: UnixStream, tok: Token,  mut msgs: Vec<&'static str>) -> EchoClient {
        let curr = msgs.remove(0);

        EchoClient {
            sock: sock,
            msgs: msgs,
            tx: SliceBuf::wrap(curr.as_bytes()),
            rx: SliceBuf::wrap(curr.as_bytes()),
            token: tok,
            interest: Ready::none(),
        }
    }

    fn readable(&mut self, event_loop: &mut EventLoop<Echo>) -> io::Result<()> {
        debug!("client socket readable");

        let mut pipe: PipeReader;
        let mut buf = [0; 256];

        match self.sock.read_recv_fd(&mut buf) {
            Ok((_, None)) => {
                panic!("Did not receive passed file descriptor");
            }
            Ok((r, Some(fd))) => {
                assert_eq!(r, 1);
                assert_eq!(b'x', buf[0]);
                debug!("CLIENT : We read {} bytes!", r);
                pipe = From::<Io>::from(unsafe { Io::from_raw_fd(fd) });
            }
            Err(e) => {
                panic!("not implemented; client err={:?}", e);
            }
        };

        // read the data out of the FD itself
        let n = match pipe.read(&mut buf) {
            Ok(r) => {
                debug!("CLIENT : We read {} bytes from the FD", r);
                r
            }
            Err(e) => {
                panic!("not implemented, client err={:?}", e);
            }
        };

        for &actual in buf[0..n].iter() {
            let expect = self.rx.read_byte().unwrap();
            assert!(actual == expect, "actual={}; expect={}", actual, expect);
        }

        self.interest.remove(Ready::readable());

        if !self.rx.has_remaining() {
            self.next_msg(event_loop).unwrap();
        }

        if !self.interest.is_none() {
            assert!(self.interest.is_readable() || self.interest.is_writable(), "actual={:?}", self.interest);
            event_loop.reregister(&self.sock, self.token, self.interest, PollOpt::edge() | PollOpt::oneshot())?;
        }

        Ok(())
    }

    fn writable(&mut self, event_loop: &mut EventLoop<Echo>) -> io::Result<()> {
        debug!("client socket writable");

        match self.sock.try_write_buf(&mut self.tx) {
            Ok(None) => {
                debug!("client flushing buf; WOULDBLOCK");
                self.interest.insert(Ready::writable());
            }
            Ok(Some(r)) => {
                debug!("CLIENT : we wrote {} bytes!", r);
                self.interest.insert(Ready::readable());
                self.interest.remove(Ready::writable());
            }
            Err(e) => debug!("not implemented; client err={:?}", e)
        }

        assert!(self.interest.is_readable() || self.interest.is_writable(), "actual={:?}", self.interest);
        event_loop.reregister(&self.sock, self.token, self.interest, PollOpt::edge() | PollOpt::oneshot())
    }

    fn next_msg(&mut self, event_loop: &mut EventLoop<Echo>) -> io::Result<()> {
        if self.msgs.is_empty() {
            event_loop.shutdown();
            return Ok(());
        }

        let curr = self.msgs.remove(0);

        debug!("client prepping next message");
        self.tx = SliceBuf::wrap(curr.as_bytes());
        self.rx = SliceBuf::wrap(curr.as_bytes());

        self.interest.insert(Ready::writable());
        event_loop.reregister(&self.sock, self.token, self.interest, PollOpt::edge() | PollOpt::oneshot())
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
                conns: Slab::with_capacity(128)
            },
            client: EchoClient::new(client, CLIENT, msgs)
        }
    }
}

impl Handler for Echo {
    type Timeout = usize;
    type Message = ();

    fn ready(&mut self, event_loop: &mut EventLoop<Echo>, token: Token, events: Ready) {
        if events.is_readable() {
            match token {
                SERVER => self.server.accept(event_loop).unwrap(),
                CLIENT => self.client.readable(event_loop).unwrap(),
                i => self.server.conn_readable(event_loop, i).unwrap()
            };
        }

        if events.is_writable() {
            match token {
                SERVER => panic!("received writable for token 0"),
                CLIENT => self.client.writable(event_loop).unwrap(),
                _ => self.server.conn_writable(event_loop, token).unwrap()
            };
        }
    }
}

#[test]
pub fn test_unix_pass_fd() {
    debug!("Starting TEST_UNIX_PASS_FD");
    let mut event_loop = EventLoop::new().unwrap();

    let tmp_dir = TempDir::new("mio").unwrap();
    let addr = tmp_dir.path().join(&PathBuf::from("sock"));

    let srv = UnixListener::bind(&addr).unwrap();

    info!("listen for connections");
    event_loop.register(&srv, SERVER, Ready::readable(), PollOpt::edge() | PollOpt::oneshot()).unwrap();

    let sock = UnixStream::connect(&addr).unwrap();

    // Connect to the server
    event_loop.register(&sock, CLIENT, Ready::writable(), PollOpt::edge() | PollOpt::oneshot()).unwrap();

    // Start the event loop
    event_loop.run(&mut Echo::new(srv, sock, vec!["foo", "bar"])).unwrap();
}
