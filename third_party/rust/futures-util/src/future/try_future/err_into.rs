use core::marker::PhantomData;
use core::pin::Pin;
use futures_core::future::{FusedFuture, Future, TryFuture};
use futures_core::task::{Context, Poll};
use pin_utils::unsafe_pinned;

/// Future for the [`err_into`](super::TryFutureExt::err_into) method.
#[derive(Debug)]
#[must_use = "futures do nothing unless you `.await` or poll them"]
pub struct ErrInto<Fut, E> {
    future: Fut,
    _marker: PhantomData<E>,
}

impl<Fut: Unpin, E> Unpin for ErrInto<Fut, E> {}

impl<Fut, E> ErrInto<Fut, E> {
    unsafe_pinned!(future: Fut);

    pub(super) fn new(future: Fut) -> ErrInto<Fut, E> {
        ErrInto {
            future,
            _marker: PhantomData,
        }
    }
}

impl<Fut, E> FusedFuture for ErrInto<Fut, E>
    where Fut: TryFuture + FusedFuture,
          Fut::Error: Into<E>,
{
    fn is_terminated(&self) -> bool { self.future.is_terminated() }
}

impl<Fut, E> Future for ErrInto<Fut, E>
    where Fut: TryFuture,
          Fut::Error: Into<E>,
{
    type Output = Result<Fut::Ok, E>;

    fn poll(
        self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Self::Output> {
        self.future().try_poll(cx)
            .map(|res| res.map_err(Into::into))
    }
}
