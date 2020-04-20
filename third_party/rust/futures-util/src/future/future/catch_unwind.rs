use futures_core::future::Future;
use futures_core::task::{Context, Poll};
use pin_utils::unsafe_pinned;
use std::any::Any;
use std::pin::Pin;
use std::panic::{catch_unwind, UnwindSafe, AssertUnwindSafe};

/// Future for the [`catch_unwind`](super::FutureExt::catch_unwind) method.
#[derive(Debug)]
#[must_use = "futures do nothing unless you `.await` or poll them"]
pub struct CatchUnwind<Fut> {
    future: Fut,
}

impl<Fut> CatchUnwind<Fut> where Fut: Future + UnwindSafe {
    unsafe_pinned!(future: Fut);

    pub(super) fn new(future: Fut) -> CatchUnwind<Fut> {
        CatchUnwind { future }
    }
}

impl<Fut> Future for CatchUnwind<Fut>
    where Fut: Future + UnwindSafe,
{
    type Output = Result<Fut::Output, Box<dyn Any + Send>>;

    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        catch_unwind(AssertUnwindSafe(|| self.future().poll(cx)))?.map(Ok)
    }
}
