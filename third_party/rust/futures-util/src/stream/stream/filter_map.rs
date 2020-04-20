use core::fmt;
use core::pin::Pin;
use futures_core::future::Future;
use futures_core::stream::{FusedStream, Stream};
use futures_core::task::{Context, Poll};
#[cfg(feature = "sink")]
use futures_sink::Sink;
use pin_utils::{unsafe_pinned, unsafe_unpinned};

/// Stream for the [`filter_map`](super::StreamExt::filter_map) method.
#[must_use = "streams do nothing unless polled"]
pub struct FilterMap<St, Fut, F> {
    stream: St,
    f: F,
    pending: Option<Fut>,
}

impl<St, Fut, F> Unpin for FilterMap<St, Fut, F>
where
    St: Unpin,
    Fut: Unpin,
{}

impl<St, Fut, F> fmt::Debug for FilterMap<St, Fut, F>
where
    St: fmt::Debug,
    Fut: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("FilterMap")
            .field("stream", &self.stream)
            .field("pending", &self.pending)
            .finish()
    }
}

impl<St, Fut, F> FilterMap<St, Fut, F>
    where St: Stream,
          F: FnMut(St::Item) -> Fut,
          Fut: Future,
{
    unsafe_pinned!(stream: St);
    unsafe_unpinned!(f: F);
    unsafe_pinned!(pending: Option<Fut>);

    pub(super) fn new(stream: St, f: F) -> FilterMap<St, Fut, F> {
        FilterMap { stream, f, pending: None }
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

impl<St, Fut, F, T> FusedStream for FilterMap<St, Fut, F>
    where St: Stream + FusedStream,
          F: FnMut(St::Item) -> Fut,
          Fut: Future<Output = Option<T>>,
{
    fn is_terminated(&self) -> bool {
        self.pending.is_none() && self.stream.is_terminated()
    }
}

impl<St, Fut, F, T> Stream for FilterMap<St, Fut, F>
    where St: Stream,
          F: FnMut(St::Item) -> Fut,
          Fut: Future<Output = Option<T>>,
{
    type Item = T;

    fn poll_next(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Option<T>> {
        loop {
            if self.pending.is_none() {
                let item = match ready!(self.as_mut().stream().poll_next(cx)) {
                    Some(e) => e,
                    None => return Poll::Ready(None),
                };
                let fut = (self.as_mut().f())(item);
                self.as_mut().pending().set(Some(fut));
            }

            let item = ready!(self.as_mut().pending().as_pin_mut().unwrap().poll(cx));
            self.as_mut().pending().set(None);
            if item.is_some() {
                return Poll::Ready(item);
            }
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let pending_len = if self.pending.is_some() { 1 } else { 0 };
        let (_, upper) = self.stream.size_hint();
        let upper = match upper {
            Some(x) => x.checked_add(pending_len),
            None => None,
        };
        (0, upper) // can't know a lower bound, due to the predicate
    }
}

// Forwarding impl of Sink from the underlying stream
#[cfg(feature = "sink")]
impl<S, Fut, F, Item> Sink<Item> for FilterMap<S, Fut, F>
    where S: Stream + Sink<Item>,
          F: FnMut(S::Item) -> Fut,
          Fut: Future,
{
    type Error = S::Error;

    delegate_sink!(stream, Item);
}
