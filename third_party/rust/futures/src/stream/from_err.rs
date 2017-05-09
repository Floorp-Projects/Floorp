use core::marker::PhantomData;
use poll::Poll;
use Async;
use stream::Stream;

/// A stream combinator to change the error type of a stream.
///
/// This is created by the `Stream::from_err` method.
#[derive(Debug)]
#[must_use = "futures do nothing unless polled"]
pub struct FromErr<S, E> where S: Stream {
    stream: S,
    f: PhantomData<E>
}

pub fn new<S, E>(stream: S) -> FromErr<S, E>
    where S: Stream
{
    FromErr {
        stream: stream,
        f: PhantomData
    }
}

impl<S: Stream, E: From<S::Error>> Stream for FromErr<S, E> {
    type Item = S::Item;
    type Error = E;

    fn poll(&mut self) -> Poll<Option<S::Item>, E> {
        let e = match self.stream.poll() {
            Ok(Async::NotReady) => return Ok(Async::NotReady),
            other => other,
        };
        e.map_err(From::from)
    }
}

// Forwarding impl of Sink from the underlying stream
impl<S: Stream + ::sink::Sink, E> ::sink::Sink for FromErr<S, E> {
    type SinkItem = S::SinkItem;
    type SinkError = S::SinkError;

    fn start_send(&mut self, item: Self::SinkItem) -> ::StartSend<Self::SinkItem, Self::SinkError> {
        self.stream.start_send(item)
    }

    fn poll_complete(&mut self) -> Poll<(), Self::SinkError> {
        self.stream.poll_complete()
    }

    fn close(&mut self) -> Poll<(), Self::SinkError> {
        self.stream.close()
    }
}
