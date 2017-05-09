use Poll;
use stream::Stream;

/// A stream combinator which will change the error type of a stream from one
/// type to another.
///
/// This is produced by the `Stream::map_err` method.
#[derive(Debug)]
#[must_use = "streams do nothing unless polled"]
pub struct MapErr<S, F> {
    stream: S,
    f: F,
}

pub fn new<S, F, U>(s: S, f: F) -> MapErr<S, F>
    where S: Stream,
          F: FnMut(S::Error) -> U,
{
    MapErr {
        stream: s,
        f: f,
    }
}

// Forwarding impl of Sink from the underlying stream
impl<S, F> ::sink::Sink for MapErr<S, F>
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

impl<S, F, U> Stream for MapErr<S, F>
    where S: Stream,
          F: FnMut(S::Error) -> U,
{
    type Item = S::Item;
    type Error = U;

    fn poll(&mut self) -> Poll<Option<S::Item>, U> {
        self.stream.poll().map_err(&mut self.f)
    }
}
