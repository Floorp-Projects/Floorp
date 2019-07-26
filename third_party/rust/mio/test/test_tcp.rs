use std::cmp;
use std::io::prelude::*;
use std::io;
use std::net;
use std::sync::mpsc::channel;
use std::thread;
use std::time::Duration;

use net2::{self, TcpStreamExt};

use {TryRead, TryWrite};
use mio::{Token, Ready, PollOpt, Poll, Events};
use iovec::IoVec;
use mio::net::{TcpListener, TcpStream};

#[test]
fn accept() {
    struct H { hit: bool, listener: TcpListener, shutdown: bool }

    let l = TcpListener::bind(&"127.0.0.1:0".parse().unwrap()).unwrap();
    let addr = l.local_addr().unwrap();

    let t = thread::spawn(move || {
        net::TcpStream::connect(&addr).unwrap();
    });

    let poll = Poll::new().unwrap();

    poll.register(&l, Token(1), Ready::readable(), PollOpt::edge()).unwrap();

    let mut events = Events::with_capacity(128);

    let mut h = H { hit: false, listener: l, shutdown: false };
    while !h.shutdown {
        poll.poll(&mut events, None).unwrap();

        for event in &events {
            h.hit = true;
            assert_eq!(event.token(), Token(1));
            assert!(event.readiness().is_readable());
            assert!(h.listener.accept().is_ok());
            h.shutdown = true;
        }
    }
    assert!(h.hit);
    assert!(h.listener.accept().unwrap_err().kind() == io::ErrorKind::WouldBlock);
    t.join().unwrap();
}

#[test]
fn connect() {
    struct H { hit: u32, shutdown: bool }

    let l = net::TcpListener::bind("127.0.0.1:0").unwrap();
    let addr = l.local_addr().unwrap();

    let (tx, rx) = channel();
    let (tx2, rx2) = channel();
    let t = thread::spawn(move || {
        let s = l.accept().unwrap();
        rx.recv().unwrap();
        drop(s);
        tx2.send(()).unwrap();
    });

    let poll = Poll::new().unwrap();
    let s = TcpStream::connect(&addr).unwrap();

    poll.register(&s, Token(1), Ready::readable() | Ready::writable(), PollOpt::edge()).unwrap();

    let mut events = Events::with_capacity(128);

    let mut h = H { hit: 0, shutdown: false };
    while !h.shutdown {
        poll.poll(&mut events, None).unwrap();

        for event in &events {
            assert_eq!(event.token(), Token(1));
            match h.hit {
                0 => assert!(event.readiness().is_writable()),
                1 => assert!(event.readiness().is_readable()),
                _ => panic!(),
            }
            h.hit += 1;
            h.shutdown = true;
        }
    }
    assert_eq!(h.hit, 1);
    tx.send(()).unwrap();
    rx2.recv().unwrap();
    h.shutdown = false;
    while !h.shutdown {
        poll.poll(&mut events, None).unwrap();

        for event in &events {
            assert_eq!(event.token(), Token(1));
            match h.hit {
                0 => assert!(event.readiness().is_writable()),
                1 => assert!(event.readiness().is_readable()),
                _ => panic!(),
            }
            h.hit += 1;
            h.shutdown = true;
        }
    }
    assert_eq!(h.hit, 2);
    t.join().unwrap();
}

#[test]
fn read() {
    const N: usize = 16 * 1024 * 1024;
    struct H { amt: usize, socket: TcpStream, shutdown: bool }

    let l = net::TcpListener::bind("127.0.0.1:0").unwrap();
    let addr = l.local_addr().unwrap();

    let t = thread::spawn(move || {
        let mut s = l.accept().unwrap().0;
        let b = [0; 1024];
        let mut amt = 0;
        while amt < N {
            amt += s.write(&b).unwrap();
        }
    });

    let poll = Poll::new().unwrap();
    let s = TcpStream::connect(&addr).unwrap();

    poll.register(&s, Token(1), Ready::readable(), PollOpt::edge()).unwrap();

    let mut events = Events::with_capacity(128);

    let mut h = H { amt: 0, socket: s, shutdown: false };
    while !h.shutdown {
        poll.poll(&mut events, None).unwrap();

        for event in &events {
            assert_eq!(event.token(), Token(1));
            let mut b = [0; 1024];
            loop {
                if let Some(amt) = h.socket.try_read(&mut b).unwrap() {
                    h.amt += amt;
                } else {
                    break
                }
                if h.amt >= N {
                    h.shutdown = true;
                    break
                }
            }
        }
    }
    t.join().unwrap();
}

#[test]
fn peek() {
    const N: usize = 16 * 1024 * 1024;
    struct H { amt: usize, socket: TcpStream, shutdown: bool }

    let l = net::TcpListener::bind("127.0.0.1:0").unwrap();
    let addr = l.local_addr().unwrap();

    let t = thread::spawn(move || {
        let mut s = l.accept().unwrap().0;
        let b = [0; 1024];
        let mut amt = 0;
        while amt < N {
            amt += s.write(&b).unwrap();
        }
    });

    let poll = Poll::new().unwrap();
    let s = TcpStream::connect(&addr).unwrap();

    poll.register(&s, Token(1), Ready::readable(), PollOpt::edge()).unwrap();

    let mut events = Events::with_capacity(128);

    let mut h = H { amt: 0, socket: s, shutdown: false };
    while !h.shutdown {
        poll.poll(&mut events, None).unwrap();

        for event in &events {
            assert_eq!(event.token(), Token(1));
            let mut b = [0; 1024];
            match h.socket.peek(&mut b) {
                Ok(_) => (),
                Err(ref e) if e.kind() == io::ErrorKind::WouldBlock => {
                    continue
                },
                Err(e) => panic!("unexpected error: {:?}", e),
            }

            loop {
                if let Some(amt) = h.socket.try_read(&mut b).unwrap() {
                    h.amt += amt;
                } else {
                    break
                }
                if h.amt >= N {
                    h.shutdown = true;
                    break
                }
            }
        }
    }
    t.join().unwrap();
}

#[test]
fn read_bufs() {
    const N: usize = 16 * 1024 * 1024;

    let l = net::TcpListener::bind("127.0.0.1:0").unwrap();
    let addr = l.local_addr().unwrap();

    let t = thread::spawn(move || {
        let mut s = l.accept().unwrap().0;
        let b = [1; 1024];
        let mut amt = 0;
        while amt < N {
            amt += s.write(&b).unwrap();
        }
    });

    let poll = Poll::new().unwrap();
    let mut events = Events::with_capacity(128);

    let s = TcpStream::connect(&addr).unwrap();

    poll.register(&s, Token(1), Ready::readable(), PollOpt::level()).unwrap();

    let b1 = &mut [0; 10][..];
    let b2 = &mut [0; 383][..];
    let b3 = &mut [0; 28][..];
    let b4 = &mut [0; 8][..];
    let b5 = &mut [0; 128][..];
    let mut b: [&mut IoVec; 5] = [
        b1.into(),
        b2.into(),
        b3.into(),
        b4.into(),
        b5.into(),
    ];

    let mut so_far = 0;
    loop {
        for buf in b.iter_mut() {
            for byte in buf.as_mut_bytes() {
                *byte = 0;
            }
        }

        poll.poll(&mut events, None).unwrap();

        match s.read_bufs(&mut b) {
            Ok(0) => {
                assert_eq!(so_far, N);
                break
            }
            Ok(mut n) => {
                so_far += n;
                for buf in b.iter() {
                    let buf = buf.as_bytes();
                    for byte in buf[..cmp::min(n, buf.len())].iter() {
                        assert_eq!(*byte, 1);
                    }
                    n = n.saturating_sub(buf.len());
                    if n == 0 {
                        break
                    }
                }
                assert_eq!(n, 0);
            }
            Err(e) => assert_eq!(e.kind(), io::ErrorKind::WouldBlock),
        }
    }

    t.join().unwrap();
}

#[test]
fn write() {
    const N: usize = 16 * 1024 * 1024;
    struct H { amt: usize, socket: TcpStream, shutdown: bool }

    let l = net::TcpListener::bind("127.0.0.1:0").unwrap();
    let addr = l.local_addr().unwrap();

    let t = thread::spawn(move || {
        let mut s = l.accept().unwrap().0;
        let mut b = [0; 1024];
        let mut amt = 0;
        while amt < N {
            amt += s.read(&mut b).unwrap();
        }
    });

    let poll = Poll::new().unwrap();
    let s = TcpStream::connect(&addr).unwrap();

    poll.register(&s, Token(1), Ready::writable(), PollOpt::edge()).unwrap();

    let mut events = Events::with_capacity(128);

    let mut h = H { amt: 0, socket: s, shutdown: false };
    while !h.shutdown {
        poll.poll(&mut events, None).unwrap();

        for event in &events {
            assert_eq!(event.token(), Token(1));
            let b = [0; 1024];
            loop {
                if let Some(amt) = h.socket.try_write(&b).unwrap() {
                    h.amt += amt;
                } else {
                    break
                }
                if h.amt >= N {
                    h.shutdown = true;
                    break
                }
            }
        }
    }
    t.join().unwrap();
}

#[test]
fn write_bufs() {
    const N: usize = 16 * 1024 * 1024;

    let l = net::TcpListener::bind("127.0.0.1:0").unwrap();
    let addr = l.local_addr().unwrap();

    let t = thread::spawn(move || {
        let mut s = l.accept().unwrap().0;
        let mut b = [0; 1024];
        let mut amt = 0;
        while amt < N {
            for byte in b.iter_mut() {
                *byte = 0;
            }
            let n = s.read(&mut b).unwrap();
            amt += n;
            for byte in b[..n].iter() {
                assert_eq!(*byte, 1);
            }
        }
    });

    let poll = Poll::new().unwrap();
    let mut events = Events::with_capacity(128);
    let s = TcpStream::connect(&addr).unwrap();
    poll.register(&s, Token(1), Ready::writable(), PollOpt::level()).unwrap();

    let b1 = &[1; 10][..];
    let b2 = &[1; 383][..];
    let b3 = &[1; 28][..];
    let b4 = &[1; 8][..];
    let b5 = &[1; 128][..];
    let b: [&IoVec; 5] = [
        b1.into(),
        b2.into(),
        b3.into(),
        b4.into(),
        b5.into(),
    ];

    let mut so_far = 0;
    while so_far < N {
        poll.poll(&mut events, None).unwrap();

        match s.write_bufs(&b) {
            Ok(n) => so_far += n,
            Err(e) => assert_eq!(e.kind(), io::ErrorKind::WouldBlock),
        }
    }

    t.join().unwrap();
}

#[test]
fn connect_then_close() {
    struct H { listener: TcpListener, shutdown: bool }

    let poll = Poll::new().unwrap();
    let l = TcpListener::bind(&"127.0.0.1:0".parse().unwrap()).unwrap();
    let s = TcpStream::connect(&l.local_addr().unwrap()).unwrap();

    poll.register(&l, Token(1), Ready::readable(), PollOpt::edge()).unwrap();
    poll.register(&s, Token(2), Ready::readable(), PollOpt::edge()).unwrap();

    let mut events = Events::with_capacity(128);

    let mut h = H { listener: l, shutdown: false };
    while !h.shutdown {
        poll.poll(&mut events, None).unwrap();

        for event in &events {
            if event.token() == Token(1) {
                let s = h.listener.accept().unwrap().0;
                poll.register(&s, Token(3), Ready::readable() | Ready::writable(),
                                        PollOpt::edge()).unwrap();
                drop(s);
            } else if event.token() == Token(2) {
                h.shutdown = true;
            }
        }
    }
}

#[test]
fn listen_then_close() {
    let poll = Poll::new().unwrap();
    let l = TcpListener::bind(&"127.0.0.1:0".parse().unwrap()).unwrap();

    poll.register(&l, Token(1), Ready::readable(), PollOpt::edge()).unwrap();
    drop(l);

    let mut events = Events::with_capacity(128);

    poll.poll(&mut events, Some(Duration::from_millis(100))).unwrap();

    for event in &events {
        if event.token() == Token(1) {
            panic!("recieved ready() on a closed TcpListener")
        }
    }
}

fn assert_send<T: Send>() {
}

fn assert_sync<T: Sync>() {
}

#[test]
fn test_tcp_sockets_are_send() {
    assert_send::<TcpListener>();
    assert_send::<TcpStream>();
    assert_sync::<TcpListener>();
    assert_sync::<TcpStream>();
}

#[test]
fn bind_twice_bad() {
    let l1 = TcpListener::bind(&"127.0.0.1:0".parse().unwrap()).unwrap();
    let addr = l1.local_addr().unwrap();
    assert!(TcpListener::bind(&addr).is_err());
}

#[test]
fn multiple_writes_immediate_success() {
    const N: usize = 16;
    let l = net::TcpListener::bind("127.0.0.1:0").unwrap();
    let addr = l.local_addr().unwrap();

    let t = thread::spawn(move || {
        let mut s = l.accept().unwrap().0;
        let mut b = [0; 1024];
        let mut amt = 0;
        while amt < 1024*N {
            for byte in b.iter_mut() {
                *byte = 0;
            }
            let n = s.read(&mut b).unwrap();
            amt += n;
            for byte in b[..n].iter() {
                assert_eq!(*byte, 1);
            }
        }
    });

    let poll = Poll::new().unwrap();
    let mut s = TcpStream::connect(&addr).unwrap();
    poll.register(&s, Token(1), Ready::writable(), PollOpt::level()).unwrap();
    let mut events = Events::with_capacity(16);

    // Wait for our TCP stream to connect
    'outer: loop {
        poll.poll(&mut events, None).unwrap();
        for event in events.iter() {
            if event.token() == Token(1) && event.readiness().is_writable() {
                break 'outer
            }
        }
    }

    for _ in 0..N {
        s.write_all(&[1; 1024]).unwrap();
    }

    t.join().unwrap();
}

#[test]
fn connection_reset_by_peer() {
    let poll = Poll::new().unwrap();
    let mut events = Events::with_capacity(16);
    let mut buf = [0u8; 16];

    // Create listener
    let l = TcpListener::bind(&"127.0.0.1:0".parse().unwrap()).unwrap();
    let addr = l.local_addr().unwrap();

    // Connect client
    let client = net2::TcpBuilder::new_v4().unwrap()
        .to_tcp_stream().unwrap();

    client.set_linger(Some(Duration::from_millis(0))).unwrap();
    client.connect(&addr).unwrap();

    // Convert to Mio stream
    let client = TcpStream::from_stream(client).unwrap();

    // Register server
    poll.register(&l, Token(0), Ready::readable(), PollOpt::edge()).unwrap();

    // Register interest in the client
    poll.register(&client, Token(1), Ready::readable() | Ready::writable(), PollOpt::edge()).unwrap();

    // Wait for listener to be ready
    let mut server;
    'outer:
    loop {
        poll.poll(&mut events, None).unwrap();

        for event in &events {
            if event.token() == Token(0) {
                match l.accept() {
                    Ok((sock, _)) => {
                        server = sock;
                        break 'outer;
                    }
                    Err(ref e) if e.kind() == io::ErrorKind::WouldBlock => {}
                    Err(e) => panic!("unexpected error {:?}", e),
                }
            }
        }
    }

    // Close the connection
    drop(client);

    // Wait a moment
    thread::sleep(Duration::from_millis(100));

    // Register interest in the server socket
    poll.register(&server, Token(3), Ready::readable(), PollOpt::edge()).unwrap();


    loop {
        poll.poll(&mut events, None).unwrap();

        for event in &events {
            if event.token() == Token(3) {
                assert!(event.readiness().is_readable());

                match server.read(&mut buf) {
                    Ok(0) |
                    Err(_) => {},

                    Ok(x) => panic!("expected empty buffer but read {} bytes", x),
                }
                return;
            }
        }
    }

}

#[test]
#[cfg_attr(target_os = "fuchsia", ignore)]
fn connect_error() {
    let poll = Poll::new().unwrap();
    let mut events = Events::with_capacity(16);

    // Pick a "random" port that shouldn't be in use.
    let l = match TcpStream::connect(&"127.0.0.1:38381".parse().unwrap()) {
        Ok(l) => l,
        Err(ref e) if e.kind() == io::ErrorKind::ConnectionRefused => {
            // Connection failed synchronously.  This is not a bug, but it
            // unfortunately doesn't get us the code coverage we want.
            return;
        },
        Err(e) => panic!("TcpStream::connect unexpected error {:?}", e)
    };

    poll.register(&l, Token(0), Ready::writable(), PollOpt::edge()).unwrap();

    'outer:
    loop {
        poll.poll(&mut events, None).unwrap();

        for event in &events {
            if event.token() == Token(0) {
                assert!(event.readiness().is_writable());
                break 'outer
            }
        }
    }

    assert!(l.take_error().unwrap().is_some());
}

#[test]
fn write_error() {
    let poll = Poll::new().unwrap();
    let mut events = Events::with_capacity(16);
    let (tx, rx) = channel();

    let listener = net::TcpListener::bind("127.0.0.1:0").unwrap();
    let addr = listener.local_addr().unwrap();
    let t = thread::spawn(move || {
        let (conn, _addr) = listener.accept().unwrap();
        rx.recv().unwrap();
        drop(conn);
    });

    let mut s = TcpStream::connect(&addr).unwrap();
    poll.register(&s,
                  Token(0),
                  Ready::readable() | Ready::writable(),
                  PollOpt::edge()).unwrap();

    let mut wait_writable = || {
        'outer:
        loop {
            poll.poll(&mut events, None).unwrap();

            for event in &events {
                if event.token() == Token(0) && event.readiness().is_writable() {
                    break 'outer
                }
            }
        }
    };

    wait_writable();

    tx.send(()).unwrap();
    t.join().unwrap();

    let buf = [0; 1024];
    loop {
        match s.write(&buf) {
            Ok(_) => {}
            Err(ref e) if e.kind() == io::ErrorKind::WouldBlock => {
                wait_writable()
            }
            Err(e) => {
                println!("good error: {}", e);
                break
            }
        }
    }
}
