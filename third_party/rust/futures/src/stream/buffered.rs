use std::prelude::v1::*;

use std::fmt;
use std::mem;

use {Async, IntoFuture, Poll, Future};
use stream::{Stream, Fuse};

/// An adaptor for a stream of futures to execute the futures concurrently, if
/// possible.
///
/// This adaptor will buffer up a list of pending futures, and then return their
/// results in the order that they were pulled out of the original stream. This
/// is created by the `Stream::buffered` method.
#[must_use = "streams do nothing unless polled"]
pub struct Buffered<S>
    where S: Stream,
          S::Item: IntoFuture,
{
    stream: Fuse<S>,
    futures: Vec<State<<S::Item as IntoFuture>::Future>>,
    cur: usize,
}

impl<S> fmt::Debug for Buffered<S>
    where S: Stream + fmt::Debug,
          S::Item: IntoFuture,
          <<S as Stream>::Item as IntoFuture>::Future: fmt::Debug,
          <<S as Stream>::Item as IntoFuture>::Item: fmt::Debug,
          <<S as Stream>::Item as IntoFuture>::Error: fmt::Debug,
{
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.debug_struct("Stream")
            .field("stream", &self.stream)
            .field("futures", &self.futures)
            .field("cur", &self.cur)
            .finish()
    }
}

#[derive(Debug)]
enum State<S: Future> {
    Empty,
    Running(S),
    Finished(Result<S::Item, S::Error>),
}

pub fn new<S>(s: S, amt: usize) -> Buffered<S>
    where S: Stream,
          S::Item: IntoFuture<Error=<S as Stream>::Error>,
{
    Buffered {
        stream: super::fuse::new(s),
        futures: (0..amt).map(|_| State::Empty).collect(),
        cur: 0,
    }
}

// Forwarding impl of Sink from the underlying stream
impl<S> ::sink::Sink for Buffered<S>
    where S: ::sink::Sink + Stream,
          S::Item: IntoFuture,
{
    type SinkItem = S::SinkItem;
    type SinkError = S::SinkError;

    fn start_send(&mut self, item: S::SinkItem) -> ::StartSend<S::SinkItem, S::SinkError> {
        self.stream.start_send(item)
    }

    fn poll_complete(&mut self) -> Poll<(), S::SinkError> {
        self.stream.poll_complete()
    }

    fn close(&mut self) -> Poll<(), S::SinkError> {
        self.stream.close()
    }
}

impl<S> Stream for Buffered<S>
    where S: Stream,
          S::Item: IntoFuture<Error=<S as Stream>::Error>,
{
    type Item = <S::Item as IntoFuture>::Item;
    type Error = <S as Stream>::Error;

    fn poll(&mut self) -> Poll<Option<Self::Item>, Self::Error> {
        // First, try to fill in all the futures
        for i in 0..self.futures.len() {
            let mut idx = self.cur + i;
            if idx >= self.futures.len() {
                idx -= self.futures.len();
            }

            if let State::Empty = self.futures[idx] {
                match try!(self.stream.poll()) {
                    Async::Ready(Some(future)) => {
                        let future = future.into_future();
                        self.futures[idx] = State::Running(future);
                    }
                    Async::Ready(None) => break,
                    Async::NotReady => break,
                }
            }
        }

        // Next, try and step all the futures forward
        for future in self.futures.iter_mut() {
            let result = match *future {
                State::Running(ref mut s) => {
                    match s.poll() {
                        Ok(Async::NotReady) => continue,
                        Ok(Async::Ready(e)) => Ok(e),
                        Err(e) => Err(e),
                    }
                }
                _ => continue,
            };
            *future = State::Finished(result);
        }

        // Check to see if our current future is done.
        if let State::Finished(_) = self.futures[self.cur] {
            let r = match mem::replace(&mut self.futures[self.cur], State::Empty) {
                State::Finished(r) => r,
                _ => panic!(),
            };
            self.cur += 1;
            if self.cur >= self.futures.len() {
                self.cur = 0;
            }
            return Ok(Async::Ready(Some(try!(r))))
        }

        if self.stream.is_done() {
            if let State::Empty = self.futures[self.cur] {
                return Ok(Async::Ready(None))
            }
        }
        Ok(Async::NotReady)
    }
}
