use core::fmt;
use core::pin::Pin;
use futures_core::future::Future;
use futures_core::stream::{FusedStream, Stream};
use futures_core::task::{Context, Poll};
#[cfg(feature = "sink")]
use futures_sink::Sink;
use pin_utils::{unsafe_pinned, unsafe_unpinned};

struct StateFn<S, F> {
    state: S,
    f: F,
}

/// Stream for the [`scan`](super::StreamExt::scan) method.
#[must_use = "streams do nothing unless polled"]
pub struct Scan<St: Stream, S, Fut, F> {
    stream: St,
    state_f: Option<StateFn<S, F>>,
    future: Option<Fut>,
}

impl<St: Unpin + Stream, S, Fut: Unpin, F> Unpin for Scan<St, S, Fut, F> {}

impl<St, S, Fut, F> fmt::Debug for Scan<St, S, Fut, F>
where
    St: Stream + fmt::Debug,
    St::Item: fmt::Debug,
    S: fmt::Debug,
    Fut: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Scan")
            .field("stream", &self.stream)
            .field("state", &self.state_f.as_ref().map(|s| &s.state))
            .field("future", &self.future)
            .field("done_taking", &self.is_done_taking())
            .finish()
    }
}

impl<St: Stream, S, Fut, F> Scan<St, S, Fut, F> {
    unsafe_pinned!(stream: St);
    unsafe_unpinned!(state_f: Option<StateFn<S, F>>);
    unsafe_pinned!(future: Option<Fut>);

    /// Checks if internal state is `None`.
    fn is_done_taking(&self) -> bool {
        self.state_f.is_none()
    }
}

impl<B, St, S, Fut, F> Scan<St, S, Fut, F>
where
    St: Stream,
    F: FnMut(&mut S, St::Item) -> Fut,
    Fut: Future<Output = Option<B>>,
{
    pub(super) fn new(stream: St, initial_state: S, f: F) -> Scan<St, S, Fut, F> {
        Scan {
            stream,
            state_f: Some(StateFn {
                state: initial_state,
                f,
            }),
            future: None,
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

impl<B, St, S, Fut, F> Stream for Scan<St, S, Fut, F>
where
    St: Stream,
    F: FnMut(&mut S, St::Item) -> Fut,
    Fut: Future<Output = Option<B>>,
{
    type Item = B;

    fn poll_next(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Option<B>> {
        if self.is_done_taking() {
            return Poll::Ready(None);
        }

        if self.future.is_none() {
            let item = match ready!(self.as_mut().stream().poll_next(cx)) {
                Some(e) => e,
                None => return Poll::Ready(None),
            };
            let state_f = self.as_mut().state_f().as_mut().unwrap();
            let fut = (state_f.f)(&mut state_f.state, item);
            self.as_mut().future().set(Some(fut));
        }

        let item = ready!(self.as_mut().future().as_pin_mut().unwrap().poll(cx));
        self.as_mut().future().set(None);

        if item.is_none() {
            self.as_mut().state_f().take();
        }

        Poll::Ready(item)
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        if self.is_done_taking() {
            (0, Some(0))
        } else {
            self.stream.size_hint() // can't know a lower bound, due to the predicate
        }
    }
}

impl<B, St, S, Fut, F> FusedStream for Scan<St, S, Fut, F>
where
    St: FusedStream,
    F: FnMut(&mut S, St::Item) -> Fut,
    Fut: Future<Output = Option<B>>,
{
    fn is_terminated(&self) -> bool {
        self.is_done_taking() || self.future.is_none() && self.stream.is_terminated()
    }
}

// Forwarding impl of Sink from the underlying stream
#[cfg(feature = "sink")]
impl<S, Fut, F, Item> Sink<Item> for Scan<S, S, Fut, F>
where
    S: Stream + Sink<Item>,
{
    type Error = S::Error;

    delegate_sink!(stream, Item);
}
