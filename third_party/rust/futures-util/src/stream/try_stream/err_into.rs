use core::marker::PhantomData;
use core::pin::Pin;
use futures_core::stream::{FusedStream, Stream, TryStream};
use futures_core::task::{Context, Poll};
#[cfg(feature = "sink")]
use futures_sink::Sink;
use pin_utils::unsafe_pinned;

/// Stream for the [`err_into`](super::TryStreamExt::err_into) method.
#[derive(Debug)]
#[must_use = "streams do nothing unless polled"]
pub struct ErrInto<St, E> {
    stream: St,
    _marker: PhantomData<E>,
}

impl<St: Unpin, E> Unpin for ErrInto<St, E> {}

impl<St, E> ErrInto<St, E> {
    unsafe_pinned!(stream: St);

    pub(super) fn new(stream: St) -> Self {
        ErrInto { stream, _marker: PhantomData }
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

impl<St, E> FusedStream for ErrInto<St, E>
where
    St: TryStream + FusedStream,
    St::Error: Into<E>,
{
    fn is_terminated(&self) -> bool {
        self.stream.is_terminated()
    }
}

impl<St, E> Stream for ErrInto<St, E>
where
    St: TryStream,
    St::Error: Into<E>,
{
    type Item = Result<St::Ok, E>;

    fn poll_next(
        self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Option<Self::Item>> {
        self.stream().try_poll_next(cx)
            .map(|res| res.map(|some| some.map_err(Into::into)))
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.stream.size_hint()
    }
}

// Forwarding impl of Sink from the underlying stream
#[cfg(feature = "sink")]
impl<S, E, Item> Sink<Item> for ErrInto<S, E>
where
    S: Sink<Item>,
{
    type Error = S::Error;

    delegate_sink!(stream, Item);
}
