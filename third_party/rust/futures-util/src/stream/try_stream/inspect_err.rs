use crate::stream::stream::inspect;
use core::fmt;
use core::pin::Pin;
use futures_core::stream::{FusedStream, Stream, TryStream};
use futures_core::task::{Context, Poll};
#[cfg(feature = "sink")]
use futures_sink::Sink;
use pin_utils::{unsafe_pinned, unsafe_unpinned};

/// Stream for the [`inspect_err`](super::TryStreamExt::inspect_err) method.
#[must_use = "streams do nothing unless polled"]
pub struct InspectErr<St, F> {
    stream: St,
    f: F,
}

impl<St: Unpin, F> Unpin for InspectErr<St, F> {}

impl<St, F> fmt::Debug for InspectErr<St, F>
where
    St: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("InspectErr")
            .field("stream", &self.stream)
            .finish()
    }
}

impl<St, F> InspectErr<St, F> {
    unsafe_pinned!(stream: St);
    unsafe_unpinned!(f: F);
}

impl<St, F> InspectErr<St, F>
where
    St: TryStream,
    F: FnMut(&St::Error),
{
    pub(super) fn new(stream: St, f: F) -> Self {
        Self { stream, f }
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

impl<St, F> FusedStream for InspectErr<St, F>
where
    St: TryStream + FusedStream,
    F: FnMut(&St::Error),
{
    fn is_terminated(&self) -> bool {
        self.stream.is_terminated()
    }
}

impl<St, F> Stream for InspectErr<St, F>
where
    St: TryStream,
    F: FnMut(&St::Error),
{
    type Item = Result<St::Ok, St::Error>;

    fn poll_next(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Option<Self::Item>> {
        self.as_mut()
            .stream()
            .try_poll_next(cx)
            .map(|opt| opt.map(|res| res.map_err(|e| inspect(e, self.as_mut().f()))))
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.stream.size_hint()
    }
}

// Forwarding impl of Sink from the underlying stream
#[cfg(feature = "sink")]
impl<S, F, Item> Sink<Item> for InspectErr<S, F>
where
    S: Sink<Item>,
{
    type Error = S::Error;

    delegate_sink!(stream, Item);
}
