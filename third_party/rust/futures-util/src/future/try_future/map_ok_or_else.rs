use core::pin::Pin;
use futures_core::future::{FusedFuture, Future, TryFuture};
use futures_core::task::{Context, Poll};
use pin_utils::{unsafe_pinned, unsafe_unpinned};

/// Future for the [`map_ok_or_else`](super::TryFutureExt::map_ok_or_else) method.
#[derive(Debug)]
#[must_use = "futures do nothing unless you `.await` or poll them"]
pub struct MapOkOrElse<Fut, F, E> {
    future: Fut,
    f: Option<F>,
    e: Option<E>,
}

impl<Fut, F, E> MapOkOrElse<Fut, F, E> {
    unsafe_pinned!(future: Fut);
    unsafe_unpinned!(f: Option<F>);
    unsafe_unpinned!(e: Option<E>);

    /// Creates a new MapOkOrElse.
    pub(super) fn new(future: Fut, e: E, f: F) -> Self {
        Self { future, f: Some(f), e: Some(e) }
    }
}

impl<Fut: Unpin, F, E> Unpin for MapOkOrElse<Fut, F, E> {}

impl<Fut, F, E, T> FusedFuture for MapOkOrElse<Fut, F, E>
    where Fut: TryFuture,
          F: FnOnce(Fut::Ok) -> T,
          E: FnOnce(Fut::Error) -> T,
{
    fn is_terminated(&self) -> bool {
        self.f.is_none() || self.e.is_none()
    }
}

impl<Fut, F, E, T> Future for MapOkOrElse<Fut, F, E>
    where Fut: TryFuture,
          F: FnOnce(Fut::Ok) -> T,
          E: FnOnce(Fut::Error) -> T,
{
    type Output = T;

    fn poll(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Self::Output> {
        self.as_mut()
            .future()
            .try_poll(cx)
            .map(|result| {
                match result {
                    Ok(i) => (self.as_mut().f().take().expect("MapOkOrElse must not be polled after it returned `Poll::Ready`"))(i),
                    Err(e) => (self.as_mut().e().take().expect("MapOkOrElse must not be polled after it returned `Poll::Ready`"))(e),
                }
            })
    }
}
