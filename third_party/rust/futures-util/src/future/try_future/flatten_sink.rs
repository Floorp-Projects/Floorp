use super::FlattenStreamSink;
use core::pin::Pin;
use futures_core::future::TryFuture;
use futures_core::stream::{FusedStream, Stream, TryStream};
use futures_core::task::{Context, Poll};
use futures_sink::Sink;
use pin_utils::unsafe_pinned;

/// Sink for the [`flatten_sink`](super::TryFutureExt::flatten_sink) method.
#[derive(Debug)]
#[must_use = "sinks do nothing unless polled"]
pub struct FlattenSink<Fut, Si>
where
    Fut: TryFuture<Ok = Si>,
{
    inner: FlattenStreamSink<Fut>,
}

impl<Fut, Si> FlattenSink<Fut, Si>
where
    Fut: TryFuture<Ok = Si>,
{
    unsafe_pinned!(inner: FlattenStreamSink<Fut>);

    pub(super) fn new(future: Fut) -> Self {
        Self {
            inner: FlattenStreamSink::new(future),
        }
    }
}

impl<Fut, S> FusedStream for FlattenSink<Fut, S>
where
    Fut: TryFuture<Ok = S>,
    S: TryStream<Error = Fut::Error> + FusedStream,
{
    fn is_terminated(&self) -> bool {
        self.inner.is_terminated()
    }
}

impl<Fut, S> Stream for FlattenSink<Fut, S>
where
    Fut: TryFuture<Ok = S>,
    S: TryStream<Error = Fut::Error>,
{
    type Item = Result<S::Ok, Fut::Error>;

    fn poll_next(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Option<Self::Item>> {
        self.inner().poll_next(cx)
    }
}

impl<Fut, Si, Item> Sink<Item> for FlattenSink<Fut, Si>
where
    Fut: TryFuture<Ok = Si>,
    Si: Sink<Item, Error = Fut::Error>,
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
