use core::mem;
use core::pin::Pin;
use futures_core::future::{FusedFuture, Future};
use futures_core::stream::{FusedStream, TryStream};
use futures_core::task::{Context, Poll};
use pin_utils::{unsafe_pinned, unsafe_unpinned};

/// Future for the [`try_collect`](super::TryStreamExt::try_collect) method.
#[derive(Debug)]
#[must_use = "futures do nothing unless you `.await` or poll them"]
pub struct TryCollect<St, C> {
    stream: St,
    items: C,
}

impl<St: TryStream, C: Default> TryCollect<St, C> {
    unsafe_pinned!(stream: St);
    unsafe_unpinned!(items: C);

    pub(super) fn new(s: St) -> TryCollect<St, C> {
        TryCollect {
            stream: s,
            items: Default::default(),
        }
    }

    fn finish(self: Pin<&mut Self>) -> C {
        mem::replace(self.items(), Default::default())
    }
}

impl<St: Unpin + TryStream, C> Unpin for TryCollect<St, C> {}

impl<St, C> FusedFuture for TryCollect<St, C>
where
    St: TryStream + FusedStream,
    C: Default + Extend<St::Ok>,
{
    fn is_terminated(&self) -> bool {
        self.stream.is_terminated()
    }
}

impl<St, C> Future for TryCollect<St, C>
where
    St: TryStream,
    C: Default + Extend<St::Ok>,
{
    type Output = Result<C, St::Error>;

    fn poll(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Self::Output> {
        loop {
            match ready!(self.as_mut().stream().try_poll_next(cx)?) {
                Some(x) => self.as_mut().items().extend(Some(x)),
                None => return Poll::Ready(Ok(self.as_mut().finish())),
            }
        }
    }
}
