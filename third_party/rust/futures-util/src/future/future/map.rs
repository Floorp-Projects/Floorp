use core::pin::Pin;
use futures_core::future::{FusedFuture, Future};
use futures_core::task::{Context, Poll};
use pin_utils::{unsafe_pinned, unsafe_unpinned};

/// Future for the [`map`](super::FutureExt::map) method.
#[derive(Debug)]
#[must_use = "futures do nothing unless you `.await` or poll them"]
pub struct Map<Fut, F> {
    future: Fut,
    f: Option<F>,
}

impl<Fut, F> Map<Fut, F> {
    unsafe_pinned!(future: Fut);
    unsafe_unpinned!(f: Option<F>);

    /// Creates a new Map.
    pub(super) fn new(future: Fut, f: F) -> Map<Fut, F> {
        Map { future, f: Some(f) }
    }
}

impl<Fut: Unpin, F> Unpin for Map<Fut, F> {}

impl<Fut, F, T> FusedFuture for Map<Fut, F>
    where Fut: Future,
          F: FnOnce(Fut::Output) -> T,
{
    fn is_terminated(&self) -> bool { self.f.is_none() }
}

impl<Fut, F, T> Future for Map<Fut, F>
    where Fut: Future,
          F: FnOnce(Fut::Output) -> T,
{
    type Output = T;

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<T> {
        self.as_mut()
            .future()
            .poll(cx)
            .map(|output| {
                let f = self.f().take()
                    .expect("Map must not be polled after it returned `Poll::Ready`");
                f(output)
            })
    }
}
