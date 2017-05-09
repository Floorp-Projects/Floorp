use {Future, Poll, Async};
use stream::Stream;

/// A combinator used to temporarily convert a stream into a future.
///
/// This future is returned by the `Stream::into_future` method.
#[derive(Debug)]
#[must_use = "futures do nothing unless polled"]
pub struct StreamFuture<S> {
    stream: Option<S>,
}

pub fn new<S: Stream>(s: S) -> StreamFuture<S> {
    StreamFuture { stream: Some(s) }
}

impl<S: Stream> Future for StreamFuture<S> {
    type Item = (Option<S::Item>, S);
    type Error = (S::Error, S);

    fn poll(&mut self) -> Poll<Self::Item, Self::Error> {
        let item = {
            let s = self.stream.as_mut().expect("polling StreamFuture twice");
            match s.poll() {
                Ok(Async::NotReady) => return Ok(Async::NotReady),
                Ok(Async::Ready(e)) => Ok(e),
                Err(e) => Err(e),
            }
        };
        let stream = self.stream.take().unwrap();
        match item {
            Ok(e) => Ok(Async::Ready((e, stream))),
            Err(e) => Err((e, stream)),
        }
    }
}
