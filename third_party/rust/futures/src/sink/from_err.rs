use core::marker::PhantomData;

use {Sink, Poll, StartSend};

/// A sink combinator to change the error type of a sink.
///
/// This is created by the `Sink::from_err` method.
#[derive(Debug)]
#[must_use = "futures do nothing unless polled"]
pub struct SinkFromErr<S, E> where S: Sink {
    sink: S,
    f: PhantomData<E>
}

pub fn new<S, E>(sink: S) -> SinkFromErr<S, E>
    where S: Sink
{
    SinkFromErr {
        sink: sink,
        f: PhantomData
    }
}

impl<S, E> Sink for SinkFromErr<S, E>
    where S: Sink,
          E: From<S::SinkError>
{
    type SinkItem = S::SinkItem;
    type SinkError = E;

    fn start_send(&mut self, item: Self::SinkItem) -> StartSend<Self::SinkItem, Self::SinkError> {
        self.sink.start_send(item).map_err(|e| e.into())
    }

    fn poll_complete(&mut self) -> Poll<(), Self::SinkError> {
        self.sink.poll_complete().map_err(|e| e.into())
    }

    fn close(&mut self) -> Poll<(), Self::SinkError> {
        self.sink.close().map_err(|e| e.into())
    }
}

impl<S: ::stream::Stream, E> ::stream::Stream for SinkFromErr<S, E> where S: Sink {
    type Item = S::Item;
    type Error = S::Error;

    fn poll(&mut self) -> Poll<Option<S::Item>, S::Error> {
        self.sink.poll()
    }
}
