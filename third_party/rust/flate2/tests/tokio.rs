#![cfg(feature = "tokio")]

extern crate flate2;
extern crate futures;
extern crate rand;
extern crate tokio_io;
extern crate tokio_tcp;
extern crate tokio_threadpool;

use std::io::{Read, Write};
use std::iter;
use std::net::{Shutdown, TcpListener};
use std::thread;

use flate2::read;
use flate2::write;
use flate2::Compression;
use futures::Future;
use rand::{thread_rng, Rng};
use tokio_io::io::{copy, shutdown};
use tokio_io::AsyncRead;
use tokio_tcp::TcpStream;

#[test]
fn tcp_stream_echo_pattern() {
    const N: u8 = 16;
    const M: usize = 16 * 1024;

    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let addr = listener.local_addr().unwrap();
    let t = thread::spawn(move || {
        let a = listener.accept().unwrap().0;
        let b = a.try_clone().unwrap();

        let t = thread::spawn(move || {
            let mut b = read::DeflateDecoder::new(b);
            let mut buf = [0; M];
            for i in 0..N {
                b.read_exact(&mut buf).unwrap();
                for byte in buf.iter() {
                    assert_eq!(*byte, i);
                }
            }

            assert_eq!(b.read(&mut buf).unwrap(), 0);
        });

        let mut a = write::ZlibEncoder::new(a, Compression::default());
        for i in 0..N {
            let buf = [i; M];
            a.write_all(&buf).unwrap();
        }
        a.finish().unwrap().shutdown(Shutdown::Write).unwrap();

        t.join().unwrap();
    });

    let stream = TcpStream::connect(&addr);
    let copy = stream
        .and_then(|s| {
            let (a, b) = s.split();
            let a = read::ZlibDecoder::new(a);
            let b = write::DeflateEncoder::new(b, Compression::default());
            copy(a, b)
        })
        .then(|result| {
            let (amt, _a, b) = result.unwrap();
            assert_eq!(amt, (N as u64) * (M as u64));
            shutdown(b).map(|_| ())
        })
        .map_err(|err| panic!("{}", err));

    let threadpool = tokio_threadpool::Builder::new().build();
    threadpool.spawn(copy);
    threadpool.shutdown().wait().unwrap();
    t.join().unwrap();
}

#[test]
fn echo_random() {
    let v = iter::repeat(())
        .take(1024 * 1024)
        .map(|()| thread_rng().gen::<u8>())
        .collect::<Vec<_>>();
    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let addr = listener.local_addr().unwrap();
    let v2 = v.clone();
    let t = thread::spawn(move || {
        let a = listener.accept().unwrap().0;
        let b = a.try_clone().unwrap();

        let mut v3 = v2.clone();
        let t = thread::spawn(move || {
            let mut b = read::DeflateDecoder::new(b);
            let mut buf = [0; 1024];
            while v3.len() > 0 {
                let n = b.read(&mut buf).unwrap();
                for (actual, expected) in buf[..n].iter().zip(&v3) {
                    assert_eq!(*actual, *expected);
                }
                v3.drain(..n);
            }

            assert_eq!(b.read(&mut buf).unwrap(), 0);
        });

        let mut a = write::ZlibEncoder::new(a, Compression::default());
        a.write_all(&v2).unwrap();
        a.finish().unwrap().shutdown(Shutdown::Write).unwrap();

        t.join().unwrap();
    });

    let stream = TcpStream::connect(&addr);
    let copy = stream
        .and_then(|s| {
            let (a, b) = s.split();
            let a = read::ZlibDecoder::new(a);
            let b = write::DeflateEncoder::new(b, Compression::default());
            copy(a, b)
        })
        .then(move |result| {
            let (amt, _a, b) = result.unwrap();
            assert_eq!(amt, v.len() as u64);
            shutdown(b).map(|_| ())
        })
        .map_err(|err| panic!("{}", err));

    let threadpool = tokio_threadpool::Builder::new().build();
    threadpool.spawn(copy);
    threadpool.shutdown().wait().unwrap();
    t.join().unwrap();
}
