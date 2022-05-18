extern crate flate2;
extern crate futures;
extern crate tokio_io;

use flate2::read::{GzDecoder, MultiGzDecoder};
use futures::prelude::*;
use futures::task;
use std::cmp;
use std::fs::File;
use std::io::{self, Read};
use tokio_io::io::read_to_end;
use tokio_io::AsyncRead;

struct BadReader<T> {
    reader: T,
    x: bool,
}

impl<T> BadReader<T> {
    fn new(reader: T) -> BadReader<T> {
        BadReader { reader, x: true }
    }
}

impl<T: Read> Read for BadReader<T> {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        if self.x {
            self.x = false;
            let len = cmp::min(buf.len(), 1);
            self.reader.read(&mut buf[..len])
        } else {
            self.x = true;
            Err(io::ErrorKind::WouldBlock.into())
        }
    }
}

struct AssertAsync<T>(T);

impl<T: Read> Read for AssertAsync<T> {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        self.0.read(buf)
    }
}

impl<T: Read> AsyncRead for AssertAsync<T> {}

struct AlwaysNotify<T>(T);

impl<T: Future> Future for AlwaysNotify<T> {
    type Item = T::Item;
    type Error = T::Error;

    fn poll(&mut self) -> Poll<Self::Item, Self::Error> {
        let ret = self.0.poll();
        if let Ok(Async::NotReady) = &ret {
            task::current().notify();
        }
        ret
    }
}

#[test]
fn test_gz_asyncread() {
    let f = File::open("tests/good-file.gz").unwrap();

    let fut = read_to_end(AssertAsync(GzDecoder::new(BadReader::new(f))), Vec::new());
    let (_, content) = AlwaysNotify(fut).wait().unwrap();

    let mut expected = Vec::new();
    File::open("tests/good-file.txt")
        .unwrap()
        .read_to_end(&mut expected)
        .unwrap();

    assert_eq!(content, expected);
}

#[test]
fn test_multi_gz_asyncread() {
    let f = File::open("tests/multi.gz").unwrap();

    let fut = read_to_end(
        AssertAsync(MultiGzDecoder::new(BadReader::new(f))),
        Vec::new(),
    );
    let (_, content) = AlwaysNotify(fut).wait().unwrap();

    let mut expected = Vec::new();
    File::open("tests/multi.txt")
        .unwrap()
        .read_to_end(&mut expected)
        .unwrap();

    assert_eq!(content, expected);
}
