use core::pin::Pin;
use futures_core::stream::{FusedStream, Stream};
use futures_core::task::{Context, Poll};
#[cfg(feature = "sink")]
use futures_sink::Sink;
use pin_utils::{unsafe_pinned, unsafe_unpinned};

/// Stream for the [`enumerate`](super::StreamExt::enumerate) method.
#[derive(Debug)]
#[must_use = "streams do nothing unless polled"]
pub struct Enumerate<St> {
    stream: St,
    count: usize,
}

impl<St: Unpin> Unpin for Enumerate<St> {}

impl<St: Stream> Enumerate<St> {
    unsafe_pinned!(stream: St);
    unsafe_unpinned!(count: usize);

    pub(super) fn new(stream: St) -> Enumerate<St> {
        Enumerate {
            stream,
            count: 0,
        }
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

impl<St: Stream + FusedStream> FusedStream for Enumerate<St> {
    fn is_terminated(&self) -> bool {
        self.stream.is_terminated()
    }
}

impl<St: Stream> Stream for Enumerate<St> {
    type Item = (usize, St::Item);

    fn poll_next(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Option<Self::Item>> {
        match ready!(self.as_mut().stream().poll_next(cx)) {
            Some(item) => {
                let count = self.count;
                *self.as_mut().count() += 1;
                Poll::Ready(Some((count, item)))
            }
            None => Poll::Ready(None),
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.stream.size_hint()
    }
}

// Forwarding impl of Sink from the underlying stream
#[cfg(feature = "sink")]
impl<S, Item> Sink<Item> for Enumerate<S>
where
    S: Stream + Sink<Item>,
{
    type Error = S::Error;

    delegate_sink!(stream, Item);
}
