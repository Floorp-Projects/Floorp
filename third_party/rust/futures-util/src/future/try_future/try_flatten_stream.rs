use super::FlattenStreamSink;
use core::fmt;
use core::pin::Pin;
use futures_core::future::TryFuture;
use futures_core::stream::{FusedStream, Stream, TryStream};
use futures_core::task::{Context, Poll};
#[cfg(feature = "sink")]
use futures_sink::Sink;
use pin_utils::unsafe_pinned;

/// Stream for the [`try_flatten_stream`](super::TryFutureExt::try_flatten_stream) method.
#[must_use = "streams do nothing unless polled"]
pub struct TryFlattenStream<Fut>
where
    Fut: TryFuture,
{
    inner: FlattenStreamSink<Fut>,
}

impl<Fut: TryFuture> TryFlattenStream<Fut>
where
    Fut: TryFuture,
    Fut::Ok: TryStream<Error = Fut::Error>,
{
    unsafe_pinned!(inner: FlattenStreamSink<Fut>);

    pub(super) fn new(future: Fut) -> Self {
        Self {
            inner: FlattenStreamSink::new(future),
        }
    }
}

impl<Fut> fmt::Debug for TryFlattenStream<Fut>
where
    Fut: TryFuture + fmt::Debug,
    Fut::Ok: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("TryFlattenStream")
            .field("inner", &self.inner)
            .finish()
    }
}

impl<Fut> FusedStream for TryFlattenStream<Fut>
where
    Fut: TryFuture,
    Fut::Ok: TryStream<Error = Fut::Error> + FusedStream,
{
    fn is_terminated(&self) -> bool {
        self.inner.is_terminated()
    }
}

impl<Fut> Stream for TryFlattenStream<Fut>
where
    Fut: TryFuture,
    Fut::Ok: TryStream<Error = Fut::Error>,
{
    type Item = Result<<Fut::Ok as TryStream>::Ok, Fut::Error>;

    fn poll_next(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Option<Self::Item>> {
        self.inner().poll_next(cx)
    }
}

#[cfg(feature = "sink")]
impl<Fut, Item> Sink<Item> for TryFlattenStream<Fut>
where
    Fut: TryFuture,
    Fut::Ok: TryStream<Error = Fut::Error> + Sink<Item, Error = Fut::Error>,
{
    type Error = Fut::Error;

    fn poll_ready(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Result<(), Self::Error>> {
        self.inner().poll_ready(cx)
    }

    fn start_send(self: Pin<&mut Self>, item: Item) -> Result<(), Self::Error> {
        self.inner().start_send(item)
    }

    fn poll_flush(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Result<(), Self::Error>> {
        self.inner().poll_flush(cx)
    }

    fn poll_close(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Result<(), Self::Error>> {
        self.inner().poll_close(cx)
    }
}
