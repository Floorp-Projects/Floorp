use future::{Future, IntoFuture};
use stream::Stream;
use poll::Poll;
use Async;
use stack::{Stack, Drain};
use std::sync::Arc;
use task::{self, UnparkEvent};

use std::prelude::v1::*;

/// An adaptor for a stream of futures to execute the futures concurrently, if
/// possible, delivering results as they become available.
///
/// This adaptor will return their results in the order that they complete.
/// This is created by the `futures` method.
///
#[derive(Debug)]
#[must_use = "streams do nothing unless polled"]
pub struct FuturesUnordered<F>
    where F: Future
{
    futures: Vec<Option<F>>,
    stack: Arc<Stack<usize>>,
    pending: Option<Drain<usize>>,
    active: usize,
}

/// Converts a list of futures into a `Stream` of results from the futures.
///
/// This function will take an list of futures (e.g. a vector, an iterator,
/// etc), and return a stream. The stream will yield items as they become
/// available on the futures internally, in the order that they become
/// available. This function is similar to `buffer_unordered` in that it may
/// return items in a different order than in the list specified.
pub fn futures_unordered<I>(futures: I) -> FuturesUnordered<<I::Item as IntoFuture>::Future>
    where I: IntoIterator,
          I::Item: IntoFuture
{
    let futures = futures.into_iter()
                         .map(IntoFuture::into_future)
                         .map(Some)
                         .collect::<Vec<_>>();
    let stack = Arc::new(Stack::new());
    for i in 0..futures.len() {
        stack.push(i);
    }
    FuturesUnordered {
        active: futures.len(),
        futures: futures,
        pending: None,
        stack: stack,
    }
}

impl<F> FuturesUnordered<F>
    where F: Future
{
    fn poll_pending(&mut self, mut drain: Drain<usize>)
                    -> Option<Poll<Option<F::Item>, F::Error>> {
        while let Some(id) = drain.next() {
            // If this future was already done just skip the notification
            if self.futures[id].is_none() {
                continue
            }
            let event = UnparkEvent::new(self.stack.clone(), id);
            let ret = match task::with_unpark_event(event, || {
                self.futures[id]
                    .as_mut()
                    .unwrap()
                    .poll()
            }) {
                Ok(Async::NotReady) => continue,
                Ok(Async::Ready(val)) => Ok(Async::Ready(Some(val))),
                Err(e) => Err(e),
            };
            self.pending = Some(drain);
            self.active -= 1;
            self.futures[id] = None;
            return Some(ret)
        }
        None
    }
}

impl<F> Stream for FuturesUnordered<F>
    where F: Future
{
    type Item = F::Item;
    type Error = F::Error;

    fn poll(&mut self) -> Poll<Option<Self::Item>, Self::Error> {
        if self.active == 0 {
            return Ok(Async::Ready(None))
        }
        if let Some(drain) = self.pending.take() {
            if let Some(ret) = self.poll_pending(drain) {
                return ret
            }
        }
        let drain = self.stack.drain();
        if let Some(ret) = self.poll_pending(drain) {
            return ret
        }
        assert!(self.active > 0);
        Ok(Async::NotReady)
    }
}
