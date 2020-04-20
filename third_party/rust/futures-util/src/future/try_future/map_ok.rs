use core::pin::Pin;
use futures_core::future::{FusedFuture, Future, TryFuture};
use futures_core::task::{Context, Poll};
use pin_utils::{unsafe_pinned, unsafe_unpinned};

/// Future for the [`map_ok`](super::TryFutureExt::map_ok) method.
#[derive(Debug)]
#[must_use = "futures do nothing unless you `.await` or poll them"]
pub struct MapOk<Fut, F> {
    future: Fut,
    f: Option<F>,
}

impl<Fut, F> MapOk<Fut, F> {
    unsafe_pinned!(future: Fut);
    unsafe_unpinned!(f: Option<F>);

    /// Creates a new MapOk.
    pub(super) fn new(future: Fut, f: F) -> MapOk<Fut, F> {
        MapOk { future, f: Some(f) }
    }
}

impl<Fut: Unpin, F> Unpin for MapOk<Fut, F> {}

impl<Fut, F, T> FusedFuture for MapOk<Fut, F>
    where Fut: TryFuture,
          F: FnOnce(Fut::Ok) -> T,
{
    fn is_terminated(&self) -> bool {
        self.f.is_none()
    }
}

impl<Fut, F, T> Future for MapOk<Fut, F>
    where Fut: TryFuture,
          F: FnOnce(Fut::Ok) -> T,
{
    type Output = Result<T, Fut::Error>;

    fn poll(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Self::Output> {
        self.as_mut()
            .future()
            .try_poll(cx)
            .map(|result| {
                let op = self.as_mut().f().take()
                    .expect("MapOk must not be polled after it returned `Poll::Ready`");
                result.map(op)
            })
    }
}
