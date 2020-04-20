use super::chain::Chain;
use core::fmt;
use core::pin::Pin;
use futures_core::future::{FusedFuture, Future};
use futures_core::task::{Context, Poll};
use pin_utils::unsafe_pinned;

/// Future for the [`flatten`](super::FutureExt::flatten) method.
#[must_use = "futures do nothing unless you `.await` or poll them"]
pub struct Flatten<Fut>
    where Fut: Future,
{
    state: Chain<Fut, Fut::Output, ()>,
}

impl<Fut> Flatten<Fut>
    where Fut: Future,
          Fut::Output: Future,
{
    unsafe_pinned!(state: Chain<Fut, Fut::Output, ()>);

    pub(super) fn new(future: Fut) -> Flatten<Fut> {
        Flatten {
            state: Chain::new(future, ()),
        }
    }
}

impl<Fut> fmt::Debug for Flatten<Fut>
    where Fut: Future + fmt::Debug,
          Fut::Output: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Flatten")
            .field("state", &self.state)
            .finish()
    }
}

impl<Fut> FusedFuture for Flatten<Fut>
    where Fut: Future,
          Fut::Output: Future,
{
    fn is_terminated(&self) -> bool { self.state.is_terminated() }
}

impl<Fut> Future for Flatten<Fut>
    where Fut: Future,
          Fut::Output: Future,
{
    type Output = <Fut::Output as Future>::Output;

    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        self.state().poll(cx, |a, ()| a)
    }
}
