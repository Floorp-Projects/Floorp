use core::fmt;
use core::pin::Pin;
use futures_core::future::Future;
use futures_core::stream::{FusedStream, Stream};
use futures_core::task::{Context, Poll};
#[cfg(feature = "sink")]
use futures_sink::Sink;
use pin_utils::{unsafe_pinned, unsafe_unpinned};

/// Stream for the [`filter`](super::StreamExt::filter) method.
#[must_use = "streams do nothing unless polled"]
pub struct Filter<St, Fut, F>
    where St: Stream,
{
    stream: St,
    f: F,
    pending_fut: Option<Fut>,
    pending_item: Option<St::Item>,
}

impl<St, Fut, F> Unpin for Filter<St, Fut, F>
where
    St: Stream + Unpin,
    Fut: Unpin,
{}

impl<St, Fut, F> fmt::Debug for Filter<St, Fut, F>
where
    St: Stream + fmt::Debug,
    St::Item: fmt::Debug,
    Fut: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Filter")
            .field("stream", &self.stream)
            .field("pending_fut", &self.pending_fut)
            .field("pending_item", &self.pending_item)
            .finish()
    }
}

impl<St, Fut, F> Filter<St, Fut, F>
where St: Stream,
      F: FnMut(&St::Item) -> Fut,
      Fut: Future<Output = bool>,
{
    unsafe_pinned!(stream: St);
    unsafe_unpinned!(f: F);
    unsafe_pinned!(pending_fut: Option<Fut>);
    unsafe_unpinned!(pending_item: Option<St::Item>);

    pub(super) fn new(stream: St, f: F) -> Filter<St, Fut, F> {
        Filter {
            stream,
            f,
            pending_fut: None,
            pending_item: None,
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

impl<St, Fut, F> FusedStream for Filter<St, Fut, F>
    where St: Stream + FusedStream,
          F: FnMut(&St::Item) -> Fut,
          Fut: Future<Output = bool>,
{
    fn is_terminated(&self) -> bool {
        self.pending_fut.is_none() && self.stream.is_terminated()
    }
}

impl<St, Fut, F> Stream for Filter<St, Fut, F>
    where St: Stream,
          F: FnMut(&St::Item) -> Fut,
          Fut: Future<Output = bool>,
{
    type Item = St::Item;

    fn poll_next(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Option<St::Item>> {
        loop {
            if self.pending_fut.is_none() {
                let item = match ready!(self.as_mut().stream().poll_next(cx)) {
                    Some(e) => e,
                    None => return Poll::Ready(None),
                };
                let fut = (self.as_mut().f())(&item);
                self.as_mut().pending_fut().set(Some(fut));
                *self.as_mut().pending_item() = Some(item);
            }

            let yield_item = ready!(self.as_mut().pending_fut().as_pin_mut().unwrap().poll(cx));
            self.as_mut().pending_fut().set(None);
            let item = self.as_mut().pending_item().take().unwrap();

            if yield_item {
                return Poll::Ready(Some(item));
            }
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let pending_len = if self.pending_item.is_some() { 1 } else { 0 };
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
impl<S, Fut, F, Item> Sink<Item> for Filter<S, Fut, F>
    where S: Stream + Sink<Item>,
          F: FnMut(&S::Item) -> Fut,
          Fut: Future<Output = bool>,
{
    type Error = S::Error;

    delegate_sink!(stream, Item);
}
