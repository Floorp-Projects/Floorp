use {Poll, Async};
use stream::Stream;

/// A stream combinator which skips a number of elements before continuing.
///
/// This structure is produced by the `Stream::skip` method.
#[derive(Debug)]
#[must_use = "streams do nothing unless polled"]
pub struct Skip<S> {
    stream: S,
    remaining: u64,
}

pub fn new<S>(s: S, amt: u64) -> Skip<S>
    where S: Stream,
{
    Skip {
        stream: s,
        remaining: amt,
    }
}

// Forwarding impl of Sink from the underlying stream
impl<S> ::sink::Sink for Skip<S>
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

impl<S> Stream for Skip<S>
    where S: Stream,
{
    type Item = S::Item;
    type Error = S::Error;

    fn poll(&mut self) -> Poll<Option<S::Item>, S::Error> {
        while self.remaining > 0 {
            match try_ready!(self.stream.poll()) {
                Some(_) => self.remaining -= 1,
                None => return Ok(Async::Ready(None)),
            }
        }

        self.stream.poll()
    }
}
