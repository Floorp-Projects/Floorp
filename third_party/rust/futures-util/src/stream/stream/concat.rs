use core::pin::Pin;
use futures_core::future::{Future, FusedFuture};
use futures_core::stream::{Stream, FusedStream};
use futures_core::task::{Context, Poll};
use pin_utils::{unsafe_pinned, unsafe_unpinned};

/// Future for the [`concat`](super::StreamExt::concat) method.
#[derive(Debug)]
#[must_use = "futures do nothing unless you `.await` or poll them"]
pub struct Concat<St: Stream> {
    stream: St,
    accum: Option<St::Item>,
}

impl<St: Stream + Unpin> Unpin for Concat<St> {}

impl<St> Concat<St>
where St: Stream,
      St::Item: Extend<<St::Item as IntoIterator>::Item> +
                IntoIterator + Default,
{
    unsafe_pinned!(stream: St);
    unsafe_unpinned!(accum: Option<St::Item>);

    pub(super) fn new(stream: St) -> Concat<St> {
        Concat {
            stream,
            accum: None,
        }
    }
}

impl<St> Future for Concat<St>
where St: Stream,
      St::Item: Extend<<St::Item as IntoIterator>::Item> +
                IntoIterator + Default,
{
    type Output = St::Item;

    fn poll(
        mut self: Pin<&mut Self>, cx: &mut Context<'_>
    ) -> Poll<Self::Output> {
        loop {
            match ready!(self.as_mut().stream().poll_next(cx)) {
                None => {
                    return Poll::Ready(self.as_mut().accum().take().unwrap_or_default())
                }
                Some(e) => {
                    let accum = self.as_mut().accum();
                    if let Some(a) = accum {
                        a.extend(e)
                    } else {
                        *accum = Some(e)
                    }
                }
            }
        }
    }
}

impl<St> FusedFuture for Concat<St>
where St: FusedStream,
      St::Item: Extend<<St::Item as IntoIterator>::Item> +
                IntoIterator + Default,
{
    fn is_terminated(&self) -> bool {
        self.accum.is_none() && self.stream.is_terminated()
    }
}
