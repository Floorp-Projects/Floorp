use {IntoFuture, Future, Poll, Async};
use stream::Stream;

/// A stream combinator which chains a computation onto values produced by a
/// stream.
///
/// This structure is produced by the `Stream::and_then` method.
#[derive(Debug)]
#[must_use = "streams do nothing unless polled"]
pub struct AndThen<S, F, U>
    where U: IntoFuture,
{
    stream: S,
    future: Option<U::Future>,
    f: F,
}

pub fn new<S, F, U>(s: S, f: F) -> AndThen<S, F, U>
    where S: Stream,
          F: FnMut(S::Item) -> U,
          U: IntoFuture<Error=S::Error>,
{
    AndThen {
        stream: s,
        future: None,
        f: f,
    }
}

// Forwarding impl of Sink from the underlying stream
impl<S, F, U: IntoFuture> ::sink::Sink for AndThen<S, F, U>
    where S: ::sink::Sink
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

impl<S, F, U> Stream for AndThen<S, F, U>
    where S: Stream,
          F: FnMut(S::Item) -> U,
          U: IntoFuture<Error=S::Error>,
{
    type Item = U::Item;
    type Error = S::Error;

    fn poll(&mut self) -> Poll<Option<U::Item>, S::Error> {
        if self.future.is_none() {
            let item = match try_ready!(self.stream.poll()) {
                None => return Ok(Async::Ready(None)),
                Some(e) => e,
            };
            self.future = Some((self.f)(item).into_future());
        }
        assert!(self.future.is_some());
        match self.future.as_mut().unwrap().poll() {
            Ok(Async::Ready(e)) => {
                self.future = None;
                Ok(Async::Ready(Some(e)))
            }
            Err(e) => {
                self.future = None;
                Err(e)
            }
            Ok(Async::NotReady) => Ok(Async::NotReady)
        }
    }
}
