use crate::never::Never;
use core::pin::Pin;
use futures_core::future::{FusedFuture, Future};
use futures_core::task::{self, Poll};
use pin_utils::unsafe_pinned;

/// Future for the [`never_error`](super::FutureExt::never_error) combinator.
#[derive(Debug)]
#[must_use = "futures do nothing unless you `.await` or poll them"]
pub struct NeverError<Fut> {
    future: Fut,
}

impl<Fut> NeverError<Fut> {
    unsafe_pinned!(future: Fut);

    pub(super) fn new(future: Fut) -> NeverError<Fut> {
        NeverError { future }
    }
}

impl<Fut: Unpin> Unpin for NeverError<Fut> {}

impl<Fut: FusedFuture> FusedFuture for NeverError<Fut> {
    fn is_terminated(&self) -> bool { self.future.is_terminated() }
}

impl<Fut, T> Future for NeverError<Fut>
    where Fut: Future<Output = T>,
{
    type Output = Result<T, Never>;

    fn poll(self: Pin<&mut Self>, cx: &mut task::Context<'_>) -> Poll<Self::Output> {
        self.future().poll(cx).map(Ok)
    }
}
