#![warn(rust_2018_idioms)]

use bytes::buf::{BufExt, BufMutExt};
use bytes::{Buf, BufMut, Bytes};
#[cfg(feature = "std")]
use std::io::IoSlice;

#[test]
fn collect_two_bufs() {
    let a = Bytes::from(&b"hello"[..]);
    let b = Bytes::from(&b"world"[..]);

    let res = a.chain(b).to_bytes();
    assert_eq!(res, &b"helloworld"[..]);
}

#[test]
fn writing_chained() {
    let mut a = [0u8; 64];
    let mut b = [0u8; 64];

    {
        let mut buf = (&mut a[..]).chain_mut(&mut b[..]);

        for i in 0u8..128 {
            buf.put_u8(i);
        }
    }

    for i in 0..64 {
        let expect = i as u8;
        assert_eq!(expect, a[i]);
        assert_eq!(expect + 64, b[i]);
    }
}

#[test]
fn iterating_two_bufs() {
    let a = Bytes::from(&b"hello"[..]);
    let b = Bytes::from(&b"world"[..]);

    let res: Vec<u8> = a.chain(b).into_iter().collect();
    assert_eq!(res, &b"helloworld"[..]);
}

#[cfg(feature = "std")]
#[test]
fn vectored_read() {
    let a = Bytes::from(&b"hello"[..]);
    let b = Bytes::from(&b"world"[..]);

    let mut buf = a.chain(b);

    {
        let b1: &[u8] = &mut [];
        let b2: &[u8] = &mut [];
        let b3: &[u8] = &mut [];
        let b4: &[u8] = &mut [];
        let mut iovecs = [
            IoSlice::new(b1),
            IoSlice::new(b2),
            IoSlice::new(b3),
            IoSlice::new(b4),
        ];

        assert_eq!(2, buf.bytes_vectored(&mut iovecs));
        assert_eq!(iovecs[0][..], b"hello"[..]);
        assert_eq!(iovecs[1][..], b"world"[..]);
        assert_eq!(iovecs[2][..], b""[..]);
        assert_eq!(iovecs[3][..], b""[..]);
    }

    buf.advance(2);

    {
        let b1: &[u8] = &mut [];
        let b2: &[u8] = &mut [];
        let b3: &[u8] = &mut [];
        let b4: &[u8] = &mut [];
        let mut iovecs = [
            IoSlice::new(b1),
            IoSlice::new(b2),
            IoSlice::new(b3),
            IoSlice::new(b4),
        ];

        assert_eq!(2, buf.bytes_vectored(&mut iovecs));
        assert_eq!(iovecs[0][..], b"llo"[..]);
        assert_eq!(iovecs[1][..], b"world"[..]);
        assert_eq!(iovecs[2][..], b""[..]);
        assert_eq!(iovecs[3][..], b""[..]);
    }

    buf.advance(3);

    {
        let b1: &[u8] = &mut [];
        let b2: &[u8] = &mut [];
        let b3: &[u8] = &mut [];
        let b4: &[u8] = &mut [];
        let mut iovecs = [
            IoSlice::new(b1),
            IoSlice::new(b2),
            IoSlice::new(b3),
            IoSlice::new(b4),
        ];

        assert_eq!(1, buf.bytes_vectored(&mut iovecs));
        assert_eq!(iovecs[0][..], b"world"[..]);
        assert_eq!(iovecs[1][..], b""[..]);
        assert_eq!(iovecs[2][..], b""[..]);
        assert_eq!(iovecs[3][..], b""[..]);
    }

    buf.advance(3);

    {
        let b1: &[u8] = &mut [];
        let b2: &[u8] = &mut [];
        let b3: &[u8] = &mut [];
        let b4: &[u8] = &mut [];
        let mut iovecs = [
            IoSlice::new(b1),
            IoSlice::new(b2),
            IoSlice::new(b3),
            IoSlice::new(b4),
        ];

        assert_eq!(1, buf.bytes_vectored(&mut iovecs));
        assert_eq!(iovecs[0][..], b"ld"[..]);
        assert_eq!(iovecs[1][..], b""[..]);
        assert_eq!(iovecs[2][..], b""[..]);
        assert_eq!(iovecs[3][..], b""[..]);
    }
}
