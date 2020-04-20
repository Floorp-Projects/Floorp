use core::pin::Pin;
use futures_core::stream::{FusedStream, Stream};
use futures_core::task::{Context, Poll};
#[cfg(feature = "sink")]
use futures_sink::Sink;
use pin_utils::{unsafe_pinned, unsafe_unpinned};

/// Stream for the [`skip`](super::StreamExt::skip) method.
#[derive(Debug)]
#[must_use = "streams do nothing unless polled"]
pub struct Skip<St> {
    stream: St,
    remaining: usize,
}

impl<St: Unpin> Unpin for Skip<St> {}

impl<St: Stream> Skip<St> {
    unsafe_pinned!(stream: St);
    unsafe_unpinned!(remaining: usize);

    pub(super) fn new(stream: St, n: usize) -> Skip<St> {
        Skip {
            stream,
            remaining: n,
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

impl<St: FusedStream> FusedStream for Skip<St> {
    fn is_terminated(&self) -> bool {
        self.stream.is_terminated()
    }
}

impl<St: Stream> Stream for Skip<St> {
    type Item = St::Item;

    fn poll_next(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Option<St::Item>> {
        while self.remaining > 0 {
            match ready!(self.as_mut().stream().poll_next(cx)) {
                Some(_) => *self.as_mut().remaining() -= 1,
                None => return Poll::Ready(None),
            }
        }

        self.as_mut().stream().poll_next(cx)
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let (lower, upper) = self.stream.size_hint();

        let lower = lower.saturating_sub(self.remaining as usize);
        let upper = match upper {
            Some(x) => Some(x.saturating_sub(self.remaining as usize)),
            None => None,
        };

        (lower, upper)
    }
}

// Forwarding impl of Sink from the underlying stream
#[cfg(feature = "sink")]
impl<S, Item> Sink<Item> for Skip<S>
where
    S: Stream + Sink<Item>,
{
    type Error = S::Error;

    delegate_sink!(stream, Item);
}
