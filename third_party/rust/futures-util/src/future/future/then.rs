use super::Chain;
use core::pin::Pin;
use futures_core::future::{FusedFuture, Future};
use futures_core::task::{Context, Poll};
use pin_utils::unsafe_pinned;

/// Future for the [`then`](super::FutureExt::then) method.
#[derive(Debug)]
#[must_use = "futures do nothing unless you `.await` or poll them"]
pub struct Then<Fut1, Fut2, F> {
    chain: Chain<Fut1, Fut2, F>,
}

impl<Fut1, Fut2, F> Then<Fut1, Fut2, F>
    where Fut1: Future,
          Fut2: Future,
{
    unsafe_pinned!(chain: Chain<Fut1, Fut2, F>);

    /// Creates a new `Then`.
    pub(super) fn new(future: Fut1, f: F) -> Then<Fut1, Fut2, F> {
        Then {
            chain: Chain::new(future, f),
        }
    }
}

impl<Fut1, Fut2, F> FusedFuture for Then<Fut1, Fut2, F>
    where Fut1: Future,
          Fut2: Future,
          F: FnOnce(Fut1::Output) -> Fut2,
{
    fn is_terminated(&self) -> bool { self.chain.is_terminated() }
}

impl<Fut1, Fut2, F> Future for Then<Fut1, Fut2, F>
    where Fut1: Future,
          Fut2: Future,
          F: FnOnce(Fut1::Output) -> Fut2,
{
    type Output = Fut2::Output;

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Fut2::Output> {
        self.as_mut().chain().poll(cx, |output, f| f(output))
    }
}
