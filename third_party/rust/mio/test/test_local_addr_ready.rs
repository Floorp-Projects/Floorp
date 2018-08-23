use {TryWrite};
use mio::{Events, Poll, PollOpt, Ready, Token};
use mio::net::{TcpListener, TcpStream};

const LISTEN: Token = Token(0);
const CLIENT: Token = Token(1);
const SERVER: Token = Token(2);

struct MyHandler {
    listener: TcpListener,
    connected: TcpStream,
    accepted: Option<TcpStream>,
    shutdown: bool,
}

#[test]
fn local_addr_ready() {
    let addr = "127.0.0.1:0".parse().unwrap();
    let server = TcpListener::bind(&addr).unwrap();
    let addr = server.local_addr().unwrap();

    let poll = Poll::new().unwrap();
    poll.register(&server, LISTEN, Ready::readable(),
                        PollOpt::edge()).unwrap();

    let sock = TcpStream::connect(&addr).unwrap();
    poll.register(&sock, CLIENT, Ready::readable(),
                        PollOpt::edge()).unwrap();

    let mut events = Events::with_capacity(1024);

    let mut handler = MyHandler {
        listener: server,
        connected: sock,
        accepted: None,
        shutdown: false,
    };

    while !handler.shutdown {
        poll.poll(&mut events, None).unwrap();

        for event in &events {
            match event.token() {
                LISTEN => {
                    let sock = handler.listener.accept().unwrap().0;
                    poll.register(&sock,
                                  SERVER,
                                  Ready::writable(),
                                  PollOpt::edge()).unwrap();
                    handler.accepted = Some(sock);
                }
                SERVER => {
                    handler.accepted.as_ref().unwrap().peer_addr().unwrap();
                    handler.accepted.as_ref().unwrap().local_addr().unwrap();
                    handler.accepted.as_mut().unwrap().try_write(&[1, 2, 3]).unwrap();
                    handler.accepted = None;
                }
                CLIENT => {
                    handler.connected.peer_addr().unwrap();
                    handler.connected.local_addr().unwrap();
                    handler.shutdown = true;
                }
                _ => panic!("unexpected token"),
            }
        }
    }
}
