use core::fmt::{Debug, Formatter, Result as FmtResult};
use core::pin::Pin;
use futures_core::task::{Context, Poll};
use futures_sink::Sink;
use pin_utils::unsafe_pinned;

/// Sink that clones incoming items and forwards them to two sinks at the same time.
///
/// Backpressure from any downstream sink propagates up, which means that this sink
/// can only process items as fast as its _slowest_ downstream sink.
#[must_use = "sinks do nothing unless polled"]
pub struct Fanout<Si1, Si2> {
    sink1: Si1,
    sink2: Si2
}

impl<Si1, Si2> Fanout<Si1, Si2> {
    unsafe_pinned!(sink1: Si1);
    unsafe_pinned!(sink2: Si2);

    pub(super) fn new(sink1: Si1, sink2: Si2) -> Fanout<Si1, Si2> {
        Fanout { sink1, sink2 }
    }

    /// Get a shared reference to the inner sinks.
    pub fn get_ref(&self) -> (&Si1, &Si2) {
        (&self.sink1, &self.sink2)
    }

    /// Get a mutable reference to the inner sinks.
    pub fn get_mut(&mut self) -> (&mut Si1, &mut Si2) {
        (&mut self.sink1, &mut self.sink2)
    }

    /// Get a pinned mutable reference to the inner sinks.
    pub fn get_pin_mut(self: Pin<&mut Self>) -> (Pin<&mut Si1>, Pin<&mut Si2>) {
        unsafe {
            let Self { sink1, sink2 } = self.get_unchecked_mut();
            (Pin::new_unchecked(sink1), Pin::new_unchecked(sink2))
        }
    }

    /// Consumes this combinator, returning the underlying sinks.
    ///
    /// Note that this may discard intermediate state of this combinator,
    /// so care should be taken to avoid losing resources when this is called.
    pub fn into_inner(self) -> (Si1, Si2) {
        (self.sink1, self.sink2)
    }
}

impl<Si1: Debug, Si2: Debug> Debug for Fanout<Si1, Si2> {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        f.debug_struct("Fanout")
            .field("sink1", &self.sink1)
            .field("sink2", &self.sink2)
            .finish()
    }
}

impl<Si1, Si2, Item> Sink<Item> for Fanout<Si1, Si2>
    where Si1: Sink<Item>,
          Item: Clone,
          Si2: Sink<Item, Error=Si1::Error>
{
    type Error = Si1::Error;

    fn poll_ready(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Result<(), Self::Error>> {
        let sink1_ready = self.as_mut().sink1().poll_ready(cx)?.is_ready();
        let sink2_ready = self.as_mut().sink2().poll_ready(cx)?.is_ready();
        let ready = sink1_ready && sink2_ready;
        if ready { Poll::Ready(Ok(())) } else { Poll::Pending }
    }

    fn start_send(
        mut self: Pin<&mut Self>,
        item: Item,
    ) -> Result<(), Self::Error> {
        self.as_mut().sink1().start_send(item.clone())?;
        self.as_mut().sink2().start_send(item)?;
        Ok(())
    }

    fn poll_flush(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Result<(), Self::Error>> {
        let sink1_ready = self.as_mut().sink1().poll_flush(cx)?.is_ready();
        let sink2_ready = self.as_mut().sink2().poll_flush(cx)?.is_ready();
        let ready = sink1_ready && sink2_ready;
        if ready { Poll::Ready(Ok(())) } else { Poll::Pending }
    }

    fn poll_close(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Result<(), Self::Error>> {
        let sink1_ready = self.as_mut().sink1().poll_close(cx)?.is_ready();
        let sink2_ready = self.as_mut().sink2().poll_close(cx)?.is_ready();
        let ready = sink1_ready && sink2_ready;
        if ready { Poll::Ready(Ok(())) } else { Poll::Pending }
    }
}
