use core::pin::Pin;
use futures_core::future::{FusedFuture, Future};
use futures_core::task::{Context, Poll};
use pin_utils::{unsafe_pinned, unsafe_unpinned};

/// Future for the [`inspect`](super::FutureExt::inspect) method.
#[derive(Debug)]
#[must_use = "futures do nothing unless you `.await` or poll them"]
pub struct Inspect<Fut, F> {
    future: Fut,
    f: Option<F>,
}

impl<Fut: Future, F: FnOnce(&Fut::Output)> Inspect<Fut, F> {
    unsafe_pinned!(future: Fut);
    unsafe_unpinned!(f: Option<F>);

    pub(super) fn new(future: Fut, f: F) -> Inspect<Fut, F> {
        Inspect {
            future,
            f: Some(f),
        }
    }
}

impl<Fut: Future + Unpin, F> Unpin for Inspect<Fut, F> {}

impl<Fut, F> FusedFuture for Inspect<Fut, F>
    where Fut: FusedFuture,
          F: FnOnce(&Fut::Output),
{
    fn is_terminated(&self) -> bool { self.future.is_terminated() }
}

impl<Fut, F> Future for Inspect<Fut, F>
    where Fut: Future,
          F: FnOnce(&Fut::Output),
{
    type Output = Fut::Output;

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Fut::Output> {
        let e = ready!(self.as_mut().future().poll(cx));
        let f = self.as_mut().f().take().expect("cannot poll Inspect twice");
        f(&e);
        Poll::Ready(e)
    }
}
