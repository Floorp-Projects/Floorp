use {TryRead, TryWrite};
use mio::*;
use mio::deprecated::{EventLoop, Handler};
use mio::deprecated::unix::*;
use bytes::{Buf, ByteBuf, MutByteBuf, SliceBuf};
use slab::Slab;
use std::io;
use std::path::PathBuf;
use tempdir::TempDir;

const SERVER: Token = Token(10_000_000);
const CLIENT: Token = Token(10_000_001);

struct EchoConn {
    sock: UnixStream,
    buf: Option<ByteBuf>,
    mut_buf: Option<MutByteBuf>,
    token: Option<Token>,
    interest: Ready
}

impl EchoConn {
    fn new(sock: UnixStream) -> EchoConn {
        EchoConn {
            sock: sock,
            buf: None,
            mut_buf: Some(ByteBuf::mut_with_capacity(2048)),
            token: None,
            interest: Ready::hup()
        }
    }

    fn writable(&mut self, event_loop: &mut EventLoop<Echo>) -> io::Result<()> {
        let mut buf = self.buf.take().unwrap();

        match self.sock.try_write_buf(&mut buf) {
            Ok(None) => {
                debug!("client flushing buf; WOULDBLOCK");

                self.buf = Some(buf);
                self.interest.insert(Ready::writable());
            }
            Ok(Some(r)) => {
                debug!("CONN : we wrote {} bytes!", r);

                self.mut_buf = Some(buf.flip());

                self.interest.insert(Ready::readable());
                self.interest.remove(Ready::writable());
                match self.sock.shutdown(Shutdown::Write) {
                   Err(e) => panic!(e),
                   _ => {},
                }
            }
            Err(e) => debug!("not implemented; client err={:?}", e),
        }

        event_loop.reregister(&self.sock, self.token.unwrap(), self.interest,
                              PollOpt::edge() | PollOpt::oneshot())
    }

    fn readable(&mut self, event_loop: &mut EventLoop<Echo>) -> io::Result<()> {
        let mut buf = self.mut_buf.take().unwrap();

        match self.sock.try_read_buf(&mut buf) {
            Ok(None) => {
                debug!("CONN : spurious read wakeup");
                self.mut_buf = Some(buf);
            }
            Ok(Some(r)) => {
                debug!("CONN : we read {} bytes!", r);

                // prepare to provide this to writable
                self.buf = Some(buf.flip());

                self.interest.remove(Ready::readable());
                self.interest.insert(Ready::writable());
            }
            Err(e) => {
                debug!("not implemented; client err={:?}", e);
                self.interest.remove(Ready::readable());
            }

        };

        event_loop.reregister(&self.sock, self.token.unwrap(), self.interest,
                              PollOpt::edge())
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
        let conn = EchoConn::new(sock,);
        let tok = self.conns.insert(conn);

        // Register the connection
        self.conns[tok].token = Some(Token(tok));
        event_loop.register(&self.conns[tok].sock, Token(tok), Ready::readable(),
                                PollOpt::edge() | PollOpt::oneshot())
            .expect("could not register socket with event loop");

        Ok(())
    }

    fn conn_readable(&mut self, event_loop: &mut EventLoop<Echo>,
                     tok: Token) -> io::Result<()> {
        debug!("server conn readable; tok={:?}", tok);
        self.conn(tok).readable(event_loop)
    }

    fn conn_writable(&mut self, event_loop: &mut EventLoop<Echo>,
                     tok: Token) -> io::Result<()> {
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
    mut_buf: Option<MutByteBuf>,
    token: Token,
    interest: Ready
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
            mut_buf: Some(ByteBuf::mut_with_capacity(2048)),
            token: tok,
            interest: Ready::none()
        }
    }

    fn readable(&mut self, event_loop: &mut EventLoop<Echo>) -> io::Result<()> {
        debug!("client socket readable");

        let mut buf = self.mut_buf.take().unwrap();

        match self.sock.try_read_buf(&mut buf) {
            Ok(None) => {
                debug!("CLIENT : spurious read wakeup");
                self.mut_buf = Some(buf);
            }
            Ok(Some(r)) => {
                if r == 0 {
                    self.interest.remove(Ready::readable());
                     event_loop.shutdown();
                } else {
                debug!("CLIENT : We read {} bytes!", r);

                // prepare for reading
                let mut buf = buf.flip();

                while buf.has_remaining() {
                    let actual = buf.read_byte().unwrap();
                    let expect = self.rx.read_byte().unwrap();

                    assert!(actual == expect, "actual={}; expect={}", actual, expect);
                }

                self.mut_buf = Some(buf.flip());
                if !self.rx.has_remaining() {
                    self.next_msg(event_loop).unwrap();
                }
                }
            }
            Err(e) => {
                panic!("not implemented; client err={:?}", e);
            }
        };

        event_loop.reregister(&self.sock, self.token, self.interest,
                              PollOpt::edge() | PollOpt::oneshot())
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

        event_loop.reregister(&self.sock, self.token, self.interest,
                              PollOpt::edge() | PollOpt::oneshot())
    }

    fn next_msg(&mut self, event_loop: &mut EventLoop<Echo>) -> io::Result<()> {
        if self.msgs.is_empty() {

            return Ok(());
        }

        let curr = self.msgs.remove(0);

        debug!("client prepping next message");
        self.tx = SliceBuf::wrap(curr.as_bytes());
        self.rx = SliceBuf::wrap(curr.as_bytes());

        self.interest.insert(Ready::writable());
        event_loop.reregister(&self.sock, self.token, self.interest,
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
                conns: Slab::with_capacity(128)
            },
            client: EchoClient::new(client, CLIENT, msgs)
        }
    }
}

impl Handler for Echo {
    type Timeout = usize;
    type Message = ();

    fn ready(&mut self, event_loop: &mut EventLoop<Echo>, token: Token,
             events: Ready) {
        debug!("ready {:?} {:?}", token, events);
        if events.is_readable() {
            match token {
                SERVER => self.server.accept(event_loop).unwrap(),
                CLIENT => self.client.readable(event_loop).unwrap(),
                i => self.server.conn_readable(event_loop, i).unwrap()
            }
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
pub fn test_echo_server() {
    debug!("Starting TEST_ECHO_SERVER");
    let mut event_loop = EventLoop::new().unwrap();

    let tmp_dir = TempDir::new("mio").unwrap();
    let addr = tmp_dir.path().join(&PathBuf::from("sock"));

    let srv = UnixListener::bind(&addr).unwrap();

    event_loop.register(&srv, SERVER, Ready::readable(),
                            PollOpt::edge() | PollOpt::oneshot()).unwrap();

    let sock = UnixStream::connect(&addr).unwrap();

    // Connect to the server
    event_loop.register(&sock, CLIENT, Ready::writable(),
                        PollOpt::edge() | PollOpt::oneshot()).unwrap();

    // Start the event loop
    event_loop.run(&mut Echo::new(srv, sock, vec!["foo", "bar"])).unwrap();
}
