use {Poll, Async, Future, AsyncSink};
use stream::{Stream, Fuse};
use sink::Sink;

/// Future for the `Stream::forward` combinator, which sends a stream of values
/// to a sink and then waits until the sink has fully flushed those values.
#[derive(Debug)]
#[must_use = "futures do nothing unless polled"]
pub struct Forward<T: Stream, U> {
    sink: Option<U>,
    stream: Option<Fuse<T>>,
    buffered: Option<T::Item>,
}


pub fn new<T, U>(stream: T, sink: U) -> Forward<T, U>
    where U: Sink<SinkItem=T::Item>,
          T: Stream,
          T::Error: From<U::SinkError>,
{
    Forward {
        sink: Some(sink),
        stream: Some(stream.fuse()),
        buffered: None,
    }
}

impl<T, U> Forward<T, U>
    where U: Sink<SinkItem=T::Item>,
          T: Stream,
          T::Error: From<U::SinkError>,
{
    fn sink_mut(&mut self) -> &mut U {
        self.sink.as_mut().take()
            .expect("Attempted to poll Forward after completion")
    }

    fn stream_mut(&mut self) -> &mut Fuse<T> {
        self.stream.as_mut().take()
            .expect("Attempted to poll Forward after completion")
    }

    fn take_result(&mut self) -> (T, U) {
        let sink = self.sink.take()
            .expect("Attempted to poll Forward after completion");
        let fuse = self.stream.take()
            .expect("Attempted to poll Forward after completion");
        return (fuse.into_inner(), sink)
    }

    fn try_start_send(&mut self, item: T::Item) -> Poll<(), U::SinkError> {
        debug_assert!(self.buffered.is_none());
        if let AsyncSink::NotReady(item) = try!(self.sink_mut().start_send(item)) {
            self.buffered = Some(item);
            return Ok(Async::NotReady)
        }
        Ok(Async::Ready(()))
    }
}

impl<T, U> Future for Forward<T, U>
    where U: Sink<SinkItem=T::Item>,
          T: Stream,
          T::Error: From<U::SinkError>,
{
    type Item = (T, U);
    type Error = T::Error;

    fn poll(&mut self) -> Poll<(T, U), T::Error> {
        // If we've got an item buffered already, we need to write it to the
        // sink before we can do anything else
        if let Some(item) = self.buffered.take() {
            try_ready!(self.try_start_send(item))
        }

        loop {
            match try!(self.stream_mut().poll()) {
                Async::Ready(Some(item)) => try_ready!(self.try_start_send(item)),
                Async::Ready(None) => {
                    try_ready!(self.sink_mut().close());
                    return Ok(Async::Ready(self.take_result()))
                }
                Async::NotReady => {
                    try_ready!(self.sink_mut().poll_complete());
                    return Ok(Async::NotReady)
                }
            }
        }
    }
}
