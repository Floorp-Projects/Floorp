use core::pin::Pin;
use futures_core::stream::{FusedStream, Stream};
use futures_core::task::{Context, Poll};
#[cfg(feature = "sink")]
use futures_sink::Sink;
use pin_utils::{unsafe_pinned, unsafe_unpinned};

/// Stream for the [`fuse`](super::StreamExt::fuse) method.
#[derive(Debug)]
#[must_use = "streams do nothing unless polled"]
pub struct Fuse<St> {
    stream: St,
    done: bool,
}

impl<St: Unpin> Unpin for Fuse<St> {}

impl<St> Fuse<St> {
    unsafe_pinned!(stream: St);
    unsafe_unpinned!(done: bool);

    pub(super) fn new(stream: St) -> Fuse<St> {
        Fuse { stream, done: false }
    }

    /// Returns whether the underlying stream has finished or not.
    ///
    /// If this method returns `true`, then all future calls to poll are
    /// guaranteed to return `None`. If this returns `false`, then the
    /// underlying stream is still in use.
    pub fn is_done(&self) -> bool {
        self.done
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

impl<S: Stream> FusedStream for Fuse<S> {
    fn is_terminated(&self) -> bool {
        self.done
    }
}

impl<S: Stream> Stream for Fuse<S> {
    type Item = S::Item;

    fn poll_next(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Option<S::Item>> {
        if self.done {
            return Poll::Ready(None);
        }

        let item = ready!(self.as_mut().stream().poll_next(cx));
        if item.is_none() {
            *self.as_mut().done() = true;
        }
        Poll::Ready(item)
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        if self.done {
            (0, Some(0))
        } else {
            self.stream.size_hint()
        }
    }
}

// Forwarding impl of Sink from the underlying stream
#[cfg(feature = "sink")]
impl<S: Stream + Sink<Item>, Item> Sink<Item> for Fuse<S> {
    type Error = S::Error;

    delegate_sink!(stream, Item);
}
