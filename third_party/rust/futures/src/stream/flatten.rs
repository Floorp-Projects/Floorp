use {Poll, Async};
use stream::Stream;

/// A combinator used to flatten a stream-of-streams into one long stream of
/// elements.
///
/// This combinator is created by the `Stream::flatten` method.
#[derive(Debug)]
#[must_use = "streams do nothing unless polled"]
pub struct Flatten<S>
    where S: Stream,
{
    stream: S,
    next: Option<S::Item>,
}

pub fn new<S>(s: S) -> Flatten<S>
    where S: Stream,
          S::Item: Stream,
          <S::Item as Stream>::Error: From<S::Error>,
{
    Flatten {
        stream: s,
        next: None,
    }
}

// Forwarding impl of Sink from the underlying stream
impl<S> ::sink::Sink for Flatten<S>
    where S: ::sink::Sink + Stream
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

impl<S> Stream for Flatten<S>
    where S: Stream,
          S::Item: Stream,
          <S::Item as Stream>::Error: From<S::Error>,
{
    type Item = <S::Item as Stream>::Item;
    type Error = <S::Item as Stream>::Error;

    fn poll(&mut self) -> Poll<Option<Self::Item>, Self::Error> {
        loop {
            if self.next.is_none() {
                match try_ready!(self.stream.poll()) {
                    Some(e) => self.next = Some(e),
                    None => return Ok(Async::Ready(None)),
                }
            }
            assert!(self.next.is_some());
            match self.next.as_mut().unwrap().poll() {
                Ok(Async::Ready(None)) => self.next = None,
                other => return other,
            }
        }
    }
}
