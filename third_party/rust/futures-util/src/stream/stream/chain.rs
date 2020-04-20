use core::pin::Pin;
use futures_core::stream::{FusedStream, Stream};
use futures_core::task::{Context, Poll};
use pin_utils::unsafe_pinned;

/// Stream for the [`chain`](super::StreamExt::chain) method.
#[derive(Debug)]
#[must_use = "streams do nothing unless polled"]
pub struct Chain<St1, St2> {
    first: Option<St1>,
    second: St2,
}

// All interactions with `Pin<&mut Chain<..>>` happen through these methods
impl<St1, St2> Chain<St1, St2>
where St1: Stream,
      St2: Stream<Item = St1::Item>,
{
    unsafe_pinned!(first: Option<St1>);
    unsafe_pinned!(second: St2);

    pub(super) fn new(stream1: St1, stream2: St2) -> Chain<St1, St2> {
        Chain {
            first: Some(stream1),
            second: stream2,
        }
    }
}

impl<St1, St2> FusedStream for Chain<St1, St2>
where St1: Stream,
      St2: FusedStream<Item=St1::Item>,
{
    fn is_terminated(&self) -> bool {
        self.first.is_none() && self.second.is_terminated()
    }
}

impl<St1, St2> Stream for Chain<St1, St2>
where St1: Stream,
      St2: Stream<Item=St1::Item>,
{
    type Item = St1::Item;

    fn poll_next(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Option<Self::Item>> {
        if let Some(first) = self.as_mut().first().as_pin_mut() {
            if let Some(item) = ready!(first.poll_next(cx)) {
                return Poll::Ready(Some(item))
            }
        }
        self.as_mut().first().set(None);
        self.as_mut().second().poll_next(cx)
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        if let Some(first) = &self.first {
            let (first_lower, first_upper) = first.size_hint();
            let (second_lower, second_upper) = self.second.size_hint();

            let lower = first_lower.saturating_add(second_lower);

            let upper = match (first_upper, second_upper) {
                (Some(x), Some(y)) => x.checked_add(y),
                _ => None
            };

            (lower, upper)
        } else {
            self.second.size_hint()
        }
    }
}
