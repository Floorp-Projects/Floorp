use core::mem;

use {Poll, Async};
use future::Future;
use stream::Stream;

/// A stream combinator to concatenate the results of a stream into the first
/// yielded item.
///
/// This structure is produced by the `Stream::concat` method.
#[derive(Debug)]
#[must_use = "streams do nothing unless polled"]
pub struct Concat<S>
    where S: Stream,
{
    stream: S,
    extend: Inner<S::Item>,
}

pub fn new<S>(s: S) -> Concat<S>
    where S: Stream,
          S::Item: Extend<<<S as Stream>::Item as IntoIterator>::Item> + IntoIterator,
{
    Concat {
        stream: s,
        extend: Inner::First,
    }
}

impl<S> Future for Concat<S>
    where S: Stream,
          S::Item: Extend<<<S as Stream>::Item as IntoIterator>::Item> + IntoIterator,

{
    type Item = S::Item;
    type Error = S::Error;

    fn poll(&mut self) -> Poll<Self::Item, Self::Error> {
        loop {
            match self.stream.poll() {
                Ok(Async::Ready(Some(i))) => {
                    match self.extend {
                        Inner::First => {
                            self.extend = Inner::Extending(i);
                        },
                        Inner::Extending(ref mut e) => {
                            e.extend(i);
                        },
                        Inner::Done => unreachable!(),
                    }
                },
                Ok(Async::Ready(None)) => return Ok(Async::Ready(expect(self.extend.take()))),
                Ok(Async::NotReady) => return Ok(Async::NotReady),
                Err(e) => {
                    self.extend.take();
                    return Err(e)
                }
            }
        }
    }
}

#[derive(Debug)]
enum Inner<E> {
    First,
    Extending(E),
    Done,
}

impl<E> Inner<E> {
    fn take(&mut self) -> Option<E> {
        match mem::replace(self, Inner::Done) {
            Inner::Extending(e) => Some(e),
            _ => None,
        }
    }
}

fn expect<T>(opt: Option<T>) -> T {
    opt.expect("cannot poll Concat again")
}
