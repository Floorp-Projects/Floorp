use core::fmt;
use core::pin::Pin;
use futures_core::future::{FusedFuture, Future, TryFuture};
use futures_core::stream::TryStream;
use futures_core::task::{Context, Poll};
use pin_utils::{unsafe_pinned, unsafe_unpinned};

/// Future for the [`try_fold`](super::TryStreamExt::try_fold) method.
#[must_use = "futures do nothing unless you `.await` or poll them"]
pub struct TryFold<St, Fut, T, F> {
    stream: St,
    f: F,
    accum: Option<T>,
    future: Option<Fut>,
}

impl<St: Unpin, Fut: Unpin, T, F> Unpin for TryFold<St, Fut, T, F> {}

impl<St, Fut, T, F> fmt::Debug for TryFold<St, Fut, T, F>
where
    St: fmt::Debug,
    Fut: fmt::Debug,
    T: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("TryFold")
            .field("stream", &self.stream)
            .field("accum", &self.accum)
            .field("future", &self.future)
            .finish()
    }
}

impl<St, Fut, T, F> TryFold<St, Fut, T, F>
where St: TryStream,
      F: FnMut(T, St::Ok) -> Fut,
      Fut: TryFuture<Ok = T, Error = St::Error>,
{
    unsafe_pinned!(stream: St);
    unsafe_unpinned!(f: F);
    unsafe_unpinned!(accum: Option<T>);
    unsafe_pinned!(future: Option<Fut>);

    pub(super) fn new(stream: St, f: F, t: T) -> TryFold<St, Fut, T, F> {
        TryFold {
            stream,
            f,
            accum: Some(t),
            future: None,
        }
    }
}

impl<St, Fut, T, F> FusedFuture for TryFold<St, Fut, T, F>
    where St: TryStream,
          F: FnMut(T, St::Ok) -> Fut,
          Fut: TryFuture<Ok = T, Error = St::Error>,
{
    fn is_terminated(&self) -> bool {
        self.accum.is_none() && self.future.is_none()
    }
}

impl<St, Fut, T, F> Future for TryFold<St, Fut, T, F>
    where St: TryStream,
          F: FnMut(T, St::Ok) -> Fut,
          Fut: TryFuture<Ok = T, Error = St::Error>,
{
    type Output = Result<T, St::Error>;

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        loop {
            // we're currently processing a future to produce a new accum value
            if self.accum.is_none() {
                let accum = match ready!(
                    self.as_mut().future().as_pin_mut()
                       .expect("TryFold polled after completion")
                       .try_poll(cx)
                ) {
                    Ok(accum) => accum,
                    Err(e) => {
                        // Indicate that the future can no longer be polled.
                        self.as_mut().future().set(None);
                        return Poll::Ready(Err(e));
                    }
                };
                *self.as_mut().accum() = Some(accum);
                self.as_mut().future().set(None);
            }

            let item = match ready!(self.as_mut().stream().try_poll_next(cx)) {
                Some(Ok(item)) => Some(item),
                Some(Err(e)) => {
                    // Indicate that the future can no longer be polled.
                    *self.as_mut().accum() = None;
                    return Poll::Ready(Err(e));
                }
                None => None,
            };
            let accum = self.as_mut().accum().take().unwrap();

            if let Some(e) = item {
                let future = (self.as_mut().f())(accum, e);
                self.as_mut().future().set(Some(future));
            } else {
                return Poll::Ready(Ok(accum))
            }
        }
    }
}
