use crate::stream::{Fuse, FuturesOrdered, StreamExt};
use futures_core::future::Future;
use futures_core::stream::Stream;
use futures_core::task::{Context, Poll};
#[cfg(feature = "sink")]
use futures_sink::Sink;
use pin_utils::{unsafe_pinned, unsafe_unpinned};
use core::fmt;
use core::pin::Pin;

/// Stream for the [`buffered`](super::StreamExt::buffered) method.
#[must_use = "streams do nothing unless polled"]
pub struct Buffered<St>
where
    St: Stream,
    St::Item: Future,
{
    stream: Fuse<St>,
    in_progress_queue: FuturesOrdered<St::Item>,
    max: usize,
}

impl<St> Unpin for Buffered<St>
where
    St: Stream + Unpin,
    St::Item: Future,
{}

impl<St> fmt::Debug for Buffered<St>
where
    St: Stream + fmt::Debug,
    St::Item: Future,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Buffered")
            .field("stream", &self.stream)
            .field("in_progress_queue", &self.in_progress_queue)
            .field("max", &self.max)
            .finish()
    }
}

impl<St> Buffered<St>
where
    St: Stream,
    St::Item: Future,
{
    unsafe_pinned!(stream: Fuse<St>);
    unsafe_unpinned!(in_progress_queue: FuturesOrdered<St::Item>);

    pub(super) fn new(stream: St, n: usize) -> Buffered<St> {
        Buffered {
            stream: super::Fuse::new(stream),
            in_progress_queue: FuturesOrdered::new(),
            max: n,
        }
    }

    /// Acquires a reference to the underlying stream that this combinator is
    /// pulling from.
    pub fn get_ref(&self) -> &St {
        self.stream.get_ref()
    }

    /// Acquires a mutable reference to the underlying stream that this
    /// combinator is pulling from.
    ///
    /// Note that care must be taken to avoid tampering with the state of the
    /// stream which may otherwise confuse this combinator.
    pub fn get_mut(&mut self) -> &mut St {
        self.stream.get_mut()
    }

    /// Acquires a pinned mutable reference to the underlying stream that this
    /// combinator is pulling from.
    ///
    /// Note that care must be taken to avoid tampering with the state of the
    /// stream which may otherwise confuse this combinator.
    pub fn get_pin_mut(self: Pin<&mut Self>) -> Pin<&mut St> {
        self.stream().get_pin_mut()
    }

    /// Consumes this combinator, returning the underlying stream.
    ///
    /// Note that this may discard intermediate state of this combinator, so
    /// care should be taken to avoid losing resources when this is called.
    pub fn into_inner(self) -> St {
        self.stream.into_inner()
    }
}

impl<St> Stream for Buffered<St>
where
    St: Stream,
    St::Item: Future,
{
    type Item = <St::Item as Future>::Output;

    fn poll_next(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Option<Self::Item>> {
        // Try to spawn off as many futures as possible by filling up
        // our in_progress_queue of futures.
        while self.in_progress_queue.len() < self.max {
            match self.as_mut().stream().poll_next(cx) {
                Poll::Ready(Some(fut)) => self.as_mut().in_progress_queue().push(fut),
                Poll::Ready(None) | Poll::Pending => break,
            }
        }

        // Attempt to pull the next value from the in_progress_queue
        let res = self.as_mut().in_progress_queue().poll_next_unpin(cx);
        if let Some(val) = ready!(res) {
            return Poll::Ready(Some(val))
        }

        // If more values are still coming from the stream, we're not done yet
        if self.stream.is_done() {
            Poll::Ready(None)
        } else {
            Poll::Pending
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let queue_len = self.in_progress_queue.len();
        let (lower, upper) = self.stream.size_hint();
        let lower = lower.saturating_add(queue_len);
        let upper = match upper {
            Some(x) => x.checked_add(queue_len),
            None => None,
        };
        (lower, upper)
    }
}

// Forwarding impl of Sink from the underlying stream
#[cfg(feature = "sink")]
impl<S, Item> Sink<Item> for Buffered<S>
where
    S: Stream + Sink<Item>,
    S::Item: Future,
{
    type Error = S::Error;

    delegate_sink!(stream, Item);
}
