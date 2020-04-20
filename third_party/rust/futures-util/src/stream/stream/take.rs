use core::cmp;
use core::pin::Pin;
use futures_core::stream::{Stream, FusedStream};
use futures_core::task::{Context, Poll};
#[cfg(feature = "sink")]
use futures_sink::Sink;
use pin_utils::{unsafe_pinned, unsafe_unpinned};

/// Stream for the [`take`](super::StreamExt::take) method.
#[derive(Debug)]
#[must_use = "streams do nothing unless polled"]
pub struct Take<St> {
    stream: St,
    remaining: usize,
}

impl<St: Unpin> Unpin for Take<St> {}

impl<St: Stream> Take<St> {
    unsafe_pinned!(stream: St);
    unsafe_unpinned!(remaining: usize);

    pub(super) fn new(stream: St, n: usize) -> Take<St> {
        Take {
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

impl<St> Stream for Take<St>
    where St: Stream,
{
    type Item = St::Item;

    fn poll_next(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Option<St::Item>> {
        if self.remaining == 0 {
            Poll::Ready(None)
        } else {
            let next = ready!(self.as_mut().stream().poll_next(cx));
            match next {
                Some(_) => *self.as_mut().remaining() -= 1,
                None => *self.as_mut().remaining() = 0,
            }
            Poll::Ready(next)
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        if self.remaining == 0 {
            return (0, Some(0));
        }

        let (lower, upper) = self.stream.size_hint();

        let lower = cmp::min(lower, self.remaining as usize);

        let upper = match upper {
            Some(x) if x < self.remaining as usize => Some(x),
            _ => Some(self.remaining as usize)
        };

        (lower, upper)
    }
}

impl<St> FusedStream for Take<St>
    where St: FusedStream,
{
    fn is_terminated(&self) -> bool {
        self.remaining == 0 || self.stream.is_terminated()
    }
}

// Forwarding impl of Sink from the underlying stream
#[cfg(feature = "sink")]
impl<S, Item> Sink<Item> for Take<S>
    where S: Stream + Sink<Item>,
{
    type Error = S::Error;

    delegate_sink!(stream, Item);
}
