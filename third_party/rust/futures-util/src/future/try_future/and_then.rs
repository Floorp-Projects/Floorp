use super::{TryChain, TryChainAction};
use core::pin::Pin;
use futures_core::future::{FusedFuture, Future, TryFuture};
use futures_core::task::{Context, Poll};
use pin_utils::unsafe_pinned;

/// Future for the [`and_then`](super::TryFutureExt::and_then) method.
#[derive(Debug)]
#[must_use = "futures do nothing unless you `.await` or poll them"]
pub struct AndThen<Fut1, Fut2, F> {
    try_chain: TryChain<Fut1, Fut2, F>,
}

impl<Fut1, Fut2, F> AndThen<Fut1, Fut2, F>
    where Fut1: TryFuture,
          Fut2: TryFuture,
{
    unsafe_pinned!(try_chain: TryChain<Fut1, Fut2, F>);

    /// Creates a new `Then`.
    pub(super) fn new(future: Fut1, f: F) -> AndThen<Fut1, Fut2, F> {
        AndThen {
            try_chain: TryChain::new(future, f),
        }
    }
}

impl<Fut1, Fut2, F> FusedFuture for AndThen<Fut1, Fut2, F>
    where Fut1: TryFuture,
          Fut2: TryFuture<Error = Fut1::Error>,
          F: FnOnce(Fut1::Ok) -> Fut2,
{
    fn is_terminated(&self) -> bool {
        self.try_chain.is_terminated()
    }
}

impl<Fut1, Fut2, F> Future for AndThen<Fut1, Fut2, F>
    where Fut1: TryFuture,
          Fut2: TryFuture<Error = Fut1::Error>,
          F: FnOnce(Fut1::Ok) -> Fut2,
{
    type Output = Result<Fut2::Ok, Fut2::Error>;

    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        self.try_chain().poll(cx, |result, async_op| {
            match result {
                Ok(ok) => TryChainAction::Future(async_op(ok)),
                Err(err) => TryChainAction::Output(Err(err)),
            }
        })
    }
}
