use core::pin::Pin;
use futures_core::future::{FusedFuture, Future, TryFuture};
use futures_core::task::{Context, Poll};
use pin_utils::{unsafe_pinned, unsafe_unpinned};

/// Future for the [`inspect_err`](super::TryFutureExt::inspect_err) method.
#[derive(Debug)]
#[must_use = "futures do nothing unless you `.await` or poll them"]
pub struct InspectErr<Fut, F> {
    future: Fut,
    f: Option<F>,
}

impl<Fut: Unpin, F> Unpin for InspectErr<Fut, F> {}

impl<Fut, F> InspectErr<Fut, F>
where
    Fut: TryFuture,
    F: FnOnce(&Fut::Error),
{
    unsafe_pinned!(future: Fut);
    unsafe_unpinned!(f: Option<F>);

    pub(super) fn new(future: Fut, f: F) -> Self {
        Self { future, f: Some(f) }
    }
}

impl<Fut, F> FusedFuture for InspectErr<Fut, F>
where
    Fut: TryFuture + FusedFuture,
    F: FnOnce(&Fut::Error),
{
    fn is_terminated(&self) -> bool {
        self.future.is_terminated()
    }
}

impl<Fut, F> Future for InspectErr<Fut, F>
where
    Fut: TryFuture,
    F: FnOnce(&Fut::Error),
{
    type Output = Result<Fut::Ok, Fut::Error>;

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        let e = ready!(self.as_mut().future().try_poll(cx));
        if let Err(e) = &e {
            self.as_mut().f().take().expect("cannot poll InspectErr twice")(e);
        }
        Poll::Ready(e)
    }
}
