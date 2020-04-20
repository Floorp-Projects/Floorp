use core::pin::Pin;
use futures_core::future::{FusedFuture, Future};
use futures_core::task::{Context, Poll};
use pin_utils::unsafe_pinned;

/// Future for the [`unit_error`](super::FutureExt::unit_error) combinator.
#[derive(Debug)]
#[must_use = "futures do nothing unless you `.await` or poll them"]
pub struct UnitError<Fut> {
    future: Fut,
}

impl<Fut> UnitError<Fut> {
    unsafe_pinned!(future: Fut);

    pub(super) fn new(future: Fut) -> UnitError<Fut> {
        UnitError { future }
    }
}

impl<Fut: Unpin> Unpin for UnitError<Fut> {}

impl<Fut: FusedFuture> FusedFuture for UnitError<Fut> {
    fn is_terminated(&self) -> bool { self.future.is_terminated() }
}

impl<Fut, T> Future for UnitError<Fut>
    where Fut: Future<Output = T>,
{
    type Output = Result<T, ()>;

    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Result<T, ()>> {
        self.future().poll(cx).map(Ok)
    }
}
