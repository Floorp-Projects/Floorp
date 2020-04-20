use crate::stream::{StreamExt, Fuse};
use core::cmp;
use core::pin::Pin;
use futures_core::stream::{FusedStream, Stream};
use futures_core::task::{Context, Poll};
use pin_utils::{unsafe_pinned, unsafe_unpinned};

/// Stream for the [`zip`](super::StreamExt::zip) method.
#[derive(Debug)]
#[must_use = "streams do nothing unless polled"]
pub struct Zip<St1: Stream, St2: Stream> {
    stream1: Fuse<St1>,
    stream2: Fuse<St2>,
    queued1: Option<St1::Item>,
    queued2: Option<St2::Item>,
}

#[allow(clippy::type_repetition_in_bounds)] // https://github.com/rust-lang/rust-clippy/issues/4323
impl<St1, St2> Unpin for Zip<St1, St2>
where
    St1: Stream,
    Fuse<St1>: Unpin,
    St2: Stream,
    Fuse<St2>: Unpin,
{}

impl<St1: Stream, St2: Stream> Zip<St1, St2> {
    unsafe_pinned!(stream1: Fuse<St1>);
    unsafe_pinned!(stream2: Fuse<St2>);
    unsafe_unpinned!(queued1: Option<St1::Item>);
    unsafe_unpinned!(queued2: Option<St2::Item>);

    pub(super) fn new(stream1: St1, stream2: St2) -> Zip<St1, St2> {
        Zip {
            stream1: stream1.fuse(),
            stream2: stream2.fuse(),
            queued1: None,
            queued2: None,
        }
    }

    /// Acquires a reference to the underlying streams that this combinator is
    /// pulling from.
    pub fn get_ref(&self) -> (&St1, &St2) {
        (self.stream1.get_ref(), self.stream2.get_ref())
    }

    /// Acquires a mutable reference to the underlying streams that this
    /// combinator is pulling from.
    ///
    /// Note that care must be taken to avoid tampering with the state of the
    /// stream which may otherwise confuse this combinator.
    pub fn get_mut(&mut self) -> (&mut St1, &mut St2) {
        (self.stream1.get_mut(), self.stream2.get_mut())
    }

    /// Acquires a pinned mutable reference to the underlying streams that this
    /// combinator is pulling from.
    ///
    /// Note that care must be taken to avoid tampering with the state of the
    /// stream which may otherwise confuse this combinator.
    pub fn get_pin_mut(self: Pin<&mut Self>) -> (Pin<&mut St1>, Pin<&mut St2>) {
        unsafe {
            let Self { stream1, stream2, .. } = self.get_unchecked_mut();
            (Pin::new_unchecked(stream1).get_pin_mut(), Pin::new_unchecked(stream2).get_pin_mut())
        }
    }

    /// Consumes this combinator, returning the underlying streams.
    ///
    /// Note that this may discard intermediate state of this combinator, so
    /// care should be taken to avoid losing resources when this is called.
    pub fn into_inner(self) -> (St1, St2) {
        (self.stream1.into_inner(), self.stream2.into_inner())
    }
}

impl<St1, St2> FusedStream for Zip<St1, St2>
    where St1: Stream, St2: Stream,
{
    fn is_terminated(&self) -> bool {
        self.stream1.is_terminated() && self.stream2.is_terminated()
    }
}

impl<St1, St2> Stream for Zip<St1, St2>
    where St1: Stream, St2: Stream
{
    type Item = (St1::Item, St2::Item);

    fn poll_next(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Option<Self::Item>> {
        if self.queued1.is_none() {
            match self.as_mut().stream1().poll_next(cx) {
                Poll::Ready(Some(item1)) => *self.as_mut().queued1() = Some(item1),
                Poll::Ready(None) | Poll::Pending => {}
            }
        }
        if self.queued2.is_none() {
            match self.as_mut().stream2().poll_next(cx) {
                Poll::Ready(Some(item2)) => *self.as_mut().queued2() = Some(item2),
                Poll::Ready(None) | Poll::Pending => {}
            }
        }

        if self.queued1.is_some() && self.queued2.is_some() {
            let pair = (self.as_mut().queued1().take().unwrap(),
                        self.as_mut().queued2().take().unwrap());
            Poll::Ready(Some(pair))
        } else if self.stream1.is_done() || self.stream2.is_done() {
            Poll::Ready(None)
        } else {
            Poll::Pending
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let queued1_len = if self.queued1.is_some() { 1 } else { 0 };
        let queued2_len = if self.queued2.is_some() { 1 } else { 0 };
        let (stream1_lower, stream1_upper) = self.stream1.size_hint();
        let (stream2_lower, stream2_upper) = self.stream2.size_hint();

        let stream1_lower = stream1_lower.saturating_add(queued1_len);
        let stream2_lower = stream2_lower.saturating_add(queued2_len);

        let lower = cmp::min(stream1_lower, stream2_lower);

        let upper = match (stream1_upper, stream2_upper) {
            (Some(x), Some(y)) => {
                let x = x.saturating_add(queued1_len);
                let y = y.saturating_add(queued2_len);
                Some(cmp::min(x, y))
            }
            (Some(x), None) => x.checked_add(queued1_len),
            (None, Some(y)) => y.checked_add(queued2_len),
            (None, None) => None
        };

        (lower, upper)
    }
}
