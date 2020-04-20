use core::pin::Pin;
use futures_core::future::{FusedFuture, Future, TryFuture};
use futures_core::task::{Context, Poll};
use pin_utils::{unsafe_pinned, unsafe_unpinned};

/// Future for the [`unwrap_or_else`](super::TryFutureExt::unwrap_or_else)
/// method.
#[derive(Debug)]
#[must_use = "futures do nothing unless you `.await` or poll them"]
pub struct UnwrapOrElse<Fut, F> {
    future: Fut,
    f: Option<F>,
}

impl<Fut, F> UnwrapOrElse<Fut, F> {
    unsafe_pinned!(future: Fut);
    unsafe_unpinned!(f: Option<F>);

    /// Creates a new UnwrapOrElse.
    pub(super) fn new(future: Fut, f: F) -> UnwrapOrElse<Fut, F> {
        UnwrapOrElse { future, f: Some(f) }
    }
}

impl<Fut: Unpin, F> Unpin for UnwrapOrElse<Fut, F> {}

impl<Fut, F> FusedFuture for UnwrapOrElse<Fut, F>
    where Fut: TryFuture,
          F: FnOnce(Fut::Error) -> Fut::Ok,
{
    fn is_terminated(&self) -> bool {
        self.f.is_none()
    }
}

impl<Fut, F> Future for UnwrapOrElse<Fut, F>
    where Fut: TryFuture,
          F: FnOnce(Fut::Error) -> Fut::Ok,
{
    type Output = Fut::Ok;

    fn poll(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Self::Output> {
        self.as_mut()
            .future()
            .try_poll(cx)
            .map(|result| {
                let op = self.as_mut().f().take()
                    .expect("UnwrapOrElse already returned `Poll::Ready` before");
                result.unwrap_or_else(op)
            })
    }
}
