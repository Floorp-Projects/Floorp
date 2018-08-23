use localhost;
use mio::*;
use mio::net::{TcpListener, TcpStream, UdpSocket};
use std::io::ErrorKind;

#[test]
fn test_tcp_register_multiple_event_loops() {
    let addr = localhost();
    let listener = TcpListener::bind(&addr).unwrap();

    let poll1 = Poll::new().unwrap();
    poll1.register(&listener, Token(0), Ready::readable() | Ready::writable(), PollOpt::edge()).unwrap();

    let poll2 = Poll::new().unwrap();

    // Try registering the same socket with the initial one
    let res = poll2.register(&listener, Token(0), Ready::readable() | Ready::writable(), PollOpt::edge());
    assert!(res.is_err());
    assert_eq!(res.unwrap_err().kind(), ErrorKind::Other);

    // Try cloning the socket and registering it again
    let listener2 = listener.try_clone().unwrap();
    let res = poll2.register(&listener2, Token(0), Ready::readable() | Ready::writable(), PollOpt::edge());
    assert!(res.is_err());
    assert_eq!(res.unwrap_err().kind(), ErrorKind::Other);

    // Try the stream
    let stream = TcpStream::connect(&addr).unwrap();

    poll1.register(&stream, Token(1), Ready::readable() | Ready::writable(), PollOpt::edge()).unwrap();

    let res = poll2.register(&stream, Token(1), Ready::readable() | Ready::writable(), PollOpt::edge());
    assert!(res.is_err());
    assert_eq!(res.unwrap_err().kind(), ErrorKind::Other);

    // Try cloning the socket and registering it again
    let stream2 = stream.try_clone().unwrap();
    let res = poll2.register(&stream2, Token(1), Ready::readable() | Ready::writable(), PollOpt::edge());
    assert!(res.is_err());
    assert_eq!(res.unwrap_err().kind(), ErrorKind::Other);
}

#[test]
fn test_udp_register_multiple_event_loops() {
    let addr = localhost();
    let socket = UdpSocket::bind(&addr).unwrap();

    let poll1 = Poll::new().unwrap();
    poll1.register(&socket, Token(0), Ready::readable() | Ready::writable(), PollOpt::edge()).unwrap();

    let poll2 = Poll::new().unwrap();

    // Try registering the same socket with the initial one
    let res = poll2.register(&socket, Token(0), Ready::readable() | Ready::writable(), PollOpt::edge());
    assert!(res.is_err());
    assert_eq!(res.unwrap_err().kind(), ErrorKind::Other);

    // Try cloning the socket and registering it again
    let socket2 = socket.try_clone().unwrap();
    let res = poll2.register(&socket2, Token(0), Ready::readable() | Ready::writable(), PollOpt::edge());
    assert!(res.is_err());
    assert_eq!(res.unwrap_err().kind(), ErrorKind::Other);
}
