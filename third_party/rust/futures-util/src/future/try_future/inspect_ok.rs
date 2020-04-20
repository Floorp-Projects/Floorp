use core::pin::Pin;
use futures_core::future::{FusedFuture, Future, TryFuture};
use futures_core::task::{Context, Poll};
use pin_utils::{unsafe_pinned, unsafe_unpinned};

/// Future for the [`inspect_ok`](super::TryFutureExt::inspect_ok) method.
#[derive(Debug)]
#[must_use = "futures do nothing unless you `.await` or poll them"]
pub struct InspectOk<Fut, F> {
    future: Fut,
    f: Option<F>,
}

impl<Fut: Unpin, F> Unpin for InspectOk<Fut, F> {}

impl<Fut, F> InspectOk<Fut, F>
where
    Fut: TryFuture,
    F: FnOnce(&Fut::Ok),
{
    unsafe_pinned!(future: Fut);
    unsafe_unpinned!(f: Option<F>);

    pub(super) fn new(future: Fut, f: F) -> Self {
        Self { future, f: Some(f) }
    }
}

impl<Fut, F> FusedFuture for InspectOk<Fut, F>
where
    Fut: TryFuture + FusedFuture,
    F: FnOnce(&Fut::Ok),
{
    fn is_terminated(&self) -> bool {
        self.future.is_terminated()
    }
}

impl<Fut, F> Future for InspectOk<Fut, F>
where
    Fut: TryFuture,
    F: FnOnce(&Fut::Ok),
{
    type Output = Result<Fut::Ok, Fut::Error>;

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        let e = ready!(self.as_mut().future().try_poll(cx));
        if let Ok(e) = &e {
            self.as_mut().f().take().expect("cannot poll InspectOk twice")(e);
        }
        Poll::Ready(e)
    }
}
