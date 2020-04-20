use crate::stream::{self, Once};
use core::pin::Pin;
use futures_core::future::Future;
use futures_core::stream::{Stream, FusedStream};
use futures_core::task::{Context, Poll};
use pin_utils::unsafe_pinned;

/// Stream for the [`into_stream`](super::FutureExt::into_stream) method.
#[must_use = "streams do nothing unless polled"]
#[derive(Debug)]
pub struct IntoStream<Fut> {
    inner: Once<Fut>
}

impl<Fut: Future> IntoStream<Fut> {
    unsafe_pinned!(inner: Once<Fut>);

    pub(super) fn new(future: Fut) -> IntoStream<Fut> {
        IntoStream {
            inner: stream::once(future)
        }
    }
}

impl<Fut: Future> Stream for IntoStream<Fut> {
    type Item = Fut::Output;

    #[inline]
    fn poll_next(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Option<Self::Item>> {
        self.inner().poll_next(cx)
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        self.inner.size_hint()
    }
}

impl<Fut: Future> FusedStream for IntoStream<Fut> {
    fn is_terminated(&self) -> bool {
        self.inner.is_terminated()
    }
}
