use core::fmt;
use core::pin::Pin;
use futures_core::future::Future;
use futures_core::stream::{Stream, FusedStream};
use futures_core::task::{Context, Poll};
#[cfg(feature = "sink")]
use futures_sink::Sink;
use pin_utils::{unsafe_pinned, unsafe_unpinned};

/// Stream for the [`take_while`](super::StreamExt::take_while) method.
#[must_use = "streams do nothing unless polled"]
pub struct TakeWhile<St: Stream , Fut, F> {
    stream: St,
    f: F,
    pending_fut: Option<Fut>,
    pending_item: Option<St::Item>,
    done_taking: bool,
}

impl<St: Unpin + Stream, Fut: Unpin, F> Unpin for TakeWhile<St, Fut, F> {}

impl<St, Fut, F> fmt::Debug for TakeWhile<St, Fut, F>
where
    St: Stream + fmt::Debug,
    St::Item: fmt::Debug,
    Fut: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("TakeWhile")
            .field("stream", &self.stream)
            .field("pending_fut", &self.pending_fut)
            .field("pending_item", &self.pending_item)
            .field("done_taking", &self.done_taking)
            .finish()
    }
}

impl<St: Stream, Fut, F> TakeWhile<St, Fut, F> {
    unsafe_pinned!(stream: St);
    unsafe_unpinned!(f: F);
    unsafe_pinned!(pending_fut: Option<Fut>);
    unsafe_unpinned!(pending_item: Option<St::Item>);
    unsafe_unpinned!(done_taking: bool);
}

impl<St, Fut, F> TakeWhile<St, Fut, F>
    where St: Stream,
          F: FnMut(&St::Item) -> Fut,
          Fut: Future<Output = bool>,
{
    pub(super) fn new(stream: St, f: F) -> TakeWhile<St, Fut, F> {
        TakeWhile {
            stream,
            f,
            pending_fut: None,
            pending_item: None,
            done_taking: false,
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

impl<St, Fut, F> Stream for TakeWhile<St, Fut, F>
    where St: Stream,
          F: FnMut(&St::Item) -> Fut,
          Fut: Future<Output = bool>,
{
    type Item = St::Item;

    fn poll_next(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Option<St::Item>> {
        if self.done_taking {
            return Poll::Ready(None);
        }

        if self.pending_item.is_none() {
            let item = match ready!(self.as_mut().stream().poll_next(cx)) {
                Some(e) => e,
                None => return Poll::Ready(None),
            };
            let fut = (self.as_mut().f())(&item);
            self.as_mut().pending_fut().set(Some(fut));
            *self.as_mut().pending_item() = Some(item);
        }

        let take = ready!(self.as_mut().pending_fut().as_pin_mut().unwrap().poll(cx));
        self.as_mut().pending_fut().set(None);
        let item = self.as_mut().pending_item().take().unwrap();

        if take {
            Poll::Ready(Some(item))
        } else {
            *self.as_mut().done_taking() = true;
            Poll::Ready(None)
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        if self.done_taking {
            return (0, Some(0));
        }

        let pending_len = if self.pending_item.is_some() { 1 } else { 0 };
        let (_, upper) = self.stream.size_hint();
        let upper = match upper {
            Some(x) => x.checked_add(pending_len),
            None => None,
        };
        (0, upper) // can't know a lower bound, due to the predicate
    }
}

impl<St, Fut, F> FusedStream for TakeWhile<St, Fut, F>
    where St: FusedStream,
          F: FnMut(&St::Item) -> Fut,
          Fut: Future<Output = bool>,
{
    fn is_terminated(&self) -> bool {
        self.done_taking || self.pending_item.is_none() && self.stream.is_terminated()
    }
}

// Forwarding impl of Sink from the underlying stream
#[cfg(feature = "sink")]
impl<S, Fut, F, Item> Sink<Item> for TakeWhile<S, Fut, F>
    where S: Stream + Sink<Item>,
{
    type Error = S::Error;

    delegate_sink!(stream, Item);
}
