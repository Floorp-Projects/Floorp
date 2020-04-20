use crate::stream::{Fuse, FuturesUnordered, StreamExt, IntoStream};
use crate::future::{IntoFuture, TryFutureExt};
use futures_core::future::TryFuture;
use futures_core::stream::{Stream, TryStream};
use futures_core::task::{Context, Poll};
#[cfg(feature = "sink")]
use futures_sink::Sink;
use pin_utils::{unsafe_pinned, unsafe_unpinned};
use core::pin::Pin;

/// Stream for the
/// [`try_buffer_unordered`](super::TryStreamExt::try_buffer_unordered) method.
#[derive(Debug)]
#[must_use = "streams do nothing unless polled"]
pub struct TryBufferUnordered<St>
    where St: TryStream
{
    stream: Fuse<IntoStream<St>>,
    in_progress_queue: FuturesUnordered<IntoFuture<St::Ok>>,
    max: usize,
}

impl<St> Unpin for TryBufferUnordered<St>
    where St: TryStream + Unpin
{}

impl<St> TryBufferUnordered<St>
    where St: TryStream,
          St::Ok: TryFuture,
{
    unsafe_pinned!(stream: Fuse<IntoStream<St>>);
    unsafe_unpinned!(in_progress_queue: FuturesUnordered<IntoFuture<St::Ok>>);

    pub(super) fn new(stream: St, n: usize) -> Self {
        TryBufferUnordered {
            stream: IntoStream::new(stream).fuse(),
            in_progress_queue: FuturesUnordered::new(),
            max: n,
        }
    }

    /// Acquires a reference to the underlying stream that this combinator is
    /// pulling from.
    pub fn get_ref(&self) -> &St {
        self.stream.get_ref().get_ref()
    }

    /// Acquires a mutable reference to the underlying stream that this
    /// combinator is pulling from.
    ///
    /// Note that care must be taken to avoid tampering with the state of the
    /// stream which may otherwise confuse this combinator.
    pub fn get_mut(&mut self) -> &mut St {
        self.stream.get_mut().get_mut()
    }

    /// Acquires a pinned mutable reference to the underlying stream that this
    /// combinator is pulling from.
    ///
    /// Note that care must be taken to avoid tampering with the state of the
    /// stream which may otherwise confuse this combinator.
    pub fn get_pin_mut(self: Pin<&mut Self>) -> Pin<&mut St> {
        self.stream().get_pin_mut().get_pin_mut()
    }

    /// Consumes this combinator, returning the underlying stream.
    ///
    /// Note that this may discard intermediate state of this combinator, so
    /// care should be taken to avoid losing resources when this is called.
    pub fn into_inner(self) -> St {
        self.stream.into_inner().into_inner()
    }
}

impl<St> Stream for TryBufferUnordered<St>
    where St: TryStream,
          St::Ok: TryFuture<Error = St::Error>,
{
    type Item = Result<<St::Ok as TryFuture>::Ok, St::Error>;

    fn poll_next(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Option<Self::Item>> {
        // First up, try to spawn off as many futures as possible by filling up
        // our queue of futures. Propagate errors from the stream immediately.
        while self.in_progress_queue.len() < self.max {
            match self.as_mut().stream().poll_next(cx)? {
                Poll::Ready(Some(fut)) => self.as_mut().in_progress_queue().push(fut.into_future()),
                Poll::Ready(None) | Poll::Pending => break,
            }
        }

        // Attempt to pull the next value from the in_progress_queue
        match self.as_mut().in_progress_queue().poll_next_unpin(cx) {
            x @ Poll::Pending | x @ Poll::Ready(Some(_)) => return x,
            Poll::Ready(None) => {}
        }

        // If more values are still coming from the stream, we're not done yet
        if self.stream.is_done() {
            Poll::Ready(None)
        } else {
            Poll::Pending
        }
    }
}

// Forwarding impl of Sink from the underlying stream
#[cfg(feature = "sink")]
impl<S, Item, E> Sink<Item> for TryBufferUnordered<S>
    where S: TryStream + Sink<Item, Error = E>,
          S::Ok: TryFuture<Error = E>,
{
    type Error = E;

    delegate_sink!(stream, Item);
}
