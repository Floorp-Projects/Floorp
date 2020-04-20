use core::fmt;
use core::pin::Pin;
use futures_core::stream::{FusedStream, Stream};
use futures_core::task::{Context, Poll};
#[cfg(feature = "sink")]
use futures_sink::Sink;
use pin_utils::{unsafe_pinned, unsafe_unpinned};

/// Stream for the [`inspect`](super::StreamExt::inspect) method.
#[must_use = "streams do nothing unless polled"]
pub struct Inspect<St, F> {
    stream: St,
    f: F,
}

impl<St: Unpin, F> Unpin for Inspect<St, F> {}

impl<St, F> fmt::Debug for Inspect<St, F>
where
    St: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Inspect")
            .field("stream", &self.stream)
            .finish()
    }
}

impl<St, F> Inspect<St, F>
    where St: Stream,
          F: FnMut(&St::Item),
{
    unsafe_pinned!(stream: St);
    unsafe_unpinned!(f: F);

    pub(super) fn new(stream: St, f: F) -> Inspect<St, F> {
        Inspect { stream, f }
    }

    /// Acquires a reference to the underlying stream that this combinator is
    /// pulling from.
    pub fn get_ref(&self) -> &St {
        &self.stream
    }

    /// Acquires a mutable reference to the underlying stream that this
    /// combinator is pulling from.
    ///
    /// Note that care must be taken to avoid tampering with the state of the
    /// stream which may otherwise confuse this combinator.
    pub fn get_mut(&mut self) -> &mut St {
        &mut self.stream
    }

    /// Acquires a pinned mutable reference to the underlying stream that this
    /// combinator is pulling from.
    ///
    /// Note that care must be taken to avoid tampering with the state of the
    /// stream which may otherwise confuse this combinator.
    pub fn get_pin_mut(self: Pin<&mut Self>) -> Pin<&mut St> {
        self.stream()
    }

    /// Consumes this combinator, returning the underlying stream.
    ///
    /// Note that this may discard intermediate state of this combinator, so
    /// care should be taken to avoid losing resources when this is called.
    pub fn into_inner(self) -> St {
        self.stream
    }
}

impl<St, F> FusedStream for Inspect<St, F>
    where St: FusedStream,
          F: FnMut(&St::Item),
{
    fn is_terminated(&self) -> bool {
        self.stream.is_terminated()
    }
}

// used by `TryStreamExt::{inspect_ok, inspect_err}`
#[inline]
pub(crate) fn inspect<T, F: FnMut(&T)>(x: T, mut f: F) -> T {
    f(&x);
    x
}

impl<St, F> Stream for Inspect<St, F>
    where St: Stream,
          F: FnMut(&St::Item),
{
    type Item = St::Item;

    fn poll_next(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Option<St::Item>> {
        self.as_mut()
            .stream()
            .poll_next(cx)
            .map(|opt| opt.map(|e| inspect(e, self.as_mut().f())))
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.stream.size_hint()
    }
}

// Forwarding impl of Sink from the underlying stream
#[cfg(feature = "sink")]
impl<S, F, Item> Sink<Item> for Inspect<S, F>
    where S: Stream + Sink<Item>,
          F: FnMut(&S::Item),
{
    type Error = S::Error;

    delegate_sink!(stream, Item);
}
