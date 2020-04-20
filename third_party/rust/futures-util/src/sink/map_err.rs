use core::pin::Pin;
use futures_core::stream::{Stream, FusedStream};
use futures_core::task::{Context, Poll};
use futures_sink::{Sink};
use pin_utils::{unsafe_pinned, unsafe_unpinned};

/// Sink for the [`sink_map_err`](super::SinkExt::sink_map_err) method.
#[derive(Debug, Clone)]
#[must_use = "sinks do nothing unless polled"]
pub struct SinkMapErr<Si, F> {
    sink: Si,
    f: Option<F>,
}

impl<Si: Unpin, F> Unpin for SinkMapErr<Si, F> {}

impl<Si, F> SinkMapErr<Si, F> {
    unsafe_pinned!(sink: Si);
    unsafe_unpinned!(f: Option<F>);

    pub(super) fn new(sink: Si, f: F) -> SinkMapErr<Si, F> {
        SinkMapErr { sink, f: Some(f) }
    }

    /// Get a shared reference to the inner sink.
    pub fn get_ref(&self) -> &Si {
        &self.sink
    }

    /// Get a mutable reference to the inner sink.
    pub fn get_mut(&mut self) -> &mut Si {
        &mut self.sink
    }

    /// Get a pinned mutable reference to the inner sink.
    pub fn get_pin_mut(self: Pin<&mut Self>) -> Pin<&mut Si> {
        self.sink()
    }

    /// Consumes this combinator, returning the underlying sink.
    ///
    /// Note that this may discard intermediate state of this combinator, so
    /// care should be taken to avoid losing resources when this is called.
    pub fn into_inner(self) -> Si {
        self.sink
    }

    fn take_f(self: Pin<&mut Self>) -> F {
        self.f().take().expect("polled MapErr after completion")
    }
}

impl<Si, F, E, Item> Sink<Item> for SinkMapErr<Si, F>
    where Si: Sink<Item>,
          F: FnOnce(Si::Error) -> E,
{
    type Error = E;

    fn poll_ready(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Result<(), Self::Error>> {
        self.as_mut().sink().poll_ready(cx).map_err(|e| self.as_mut().take_f()(e))
    }

    fn start_send(
        mut self: Pin<&mut Self>,
        item: Item,
    ) -> Result<(), Self::Error> {
        self.as_mut().sink().start_send(item).map_err(|e| self.as_mut().take_f()(e))
    }

    fn poll_flush(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Result<(), Self::Error>> {
        self.as_mut().sink().poll_flush(cx).map_err(|e| self.as_mut().take_f()(e))
    }

    fn poll_close(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Result<(), Self::Error>> {
        self.as_mut().sink().poll_close(cx).map_err(|e| self.as_mut().take_f()(e))
    }
}

// Forwarding impl of Stream from the underlying sink
impl<S: Stream, F> Stream for SinkMapErr<S, F> {
    type Item = S::Item;

    fn poll_next(
        self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Option<S::Item>> {
        self.sink().poll_next(cx)
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.sink.size_hint()
    }
}

impl<S: FusedStream, F> FusedStream for SinkMapErr<S, F> {
    fn is_terminated(&self) -> bool {
        self.sink.is_terminated()
    }
}
