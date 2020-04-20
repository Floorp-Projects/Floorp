use core::pin::Pin;
use futures_core::future::Future;
use futures_core::stream::TryStream;
use futures_core::task::{Context, Poll};
use pin_utils::{unsafe_pinned, unsafe_unpinned};

/// Future for the [`try_concat`](super::TryStreamExt::try_concat) method.
#[derive(Debug)]
#[must_use = "futures do nothing unless you `.await` or poll them"]
pub struct TryConcat<St: TryStream> {
    stream: St,
    accum: Option<St::Ok>,
}

impl<St: TryStream + Unpin> Unpin for TryConcat<St> {}

impl<St> TryConcat<St>
where
    St: TryStream,
    St::Ok: Extend<<St::Ok as IntoIterator>::Item> + IntoIterator + Default,
{
    unsafe_pinned!(stream: St);
    unsafe_unpinned!(accum: Option<St::Ok>);

    pub(super) fn new(stream: St) -> TryConcat<St> {
        TryConcat {
            stream,
            accum: None,
        }
    }
}

impl<St> Future for TryConcat<St>
where
    St: TryStream,
    St::Ok: Extend<<St::Ok as IntoIterator>::Item> + IntoIterator + Default,
{
    type Output = Result<St::Ok, St::Error>;

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        loop {
            match ready!(self.as_mut().stream().try_poll_next(cx)?) {
                Some(x) => {
                    let accum = self.as_mut().accum();
                    if let Some(a) = accum {
                        a.extend(x)
                    } else {
                        *accum = Some(x)
                    }
                },
                None => {
                    return Poll::Ready(Ok(self.as_mut().accum().take().unwrap_or_default()))
                }
            }
        }
    }
}
