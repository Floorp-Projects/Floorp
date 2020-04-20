use futures_core::stream::{Stream, FusedStream};
use futures_core::task::{Context, Poll};
use pin_utils::{unsafe_pinned, unsafe_unpinned};
use std::any::Any;
use std::pin::Pin;
use std::panic::{catch_unwind, UnwindSafe, AssertUnwindSafe};

/// Stream for the [`catch_unwind`](super::StreamExt::catch_unwind) method.
#[derive(Debug)]
#[must_use = "streams do nothing unless polled"]
pub struct CatchUnwind<St> {
    stream: St,
    caught_unwind: bool,
}

impl<St: Stream + UnwindSafe> CatchUnwind<St> {
    unsafe_pinned!(stream: St);
    unsafe_unpinned!(caught_unwind: bool);

    pub(super) fn new(stream: St) -> CatchUnwind<St> {
        CatchUnwind { stream, caught_unwind: false }
    }
}

impl<St: Stream + UnwindSafe> Stream for CatchUnwind<St> {
    type Item = Result<St::Item, Box<dyn Any + Send>>;

    fn poll_next(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Option<Self::Item>> {
        if self.caught_unwind {
            Poll::Ready(None)
        } else {
            let res = catch_unwind(AssertUnwindSafe(|| {
                self.as_mut().stream().poll_next(cx)
            }));

            match res {
                Ok(poll) => poll.map(|opt| opt.map(Ok)),
                Err(e) => {
                    *self.as_mut().caught_unwind() = true;
                    Poll::Ready(Some(Err(e)))
                },
            }
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        if self.caught_unwind {
            (0, Some(0))
        } else {
            self.stream.size_hint()
        }
    }
}

impl<St: FusedStream + UnwindSafe> FusedStream for CatchUnwind<St> {
    fn is_terminated(&self) -> bool {
        self.caught_unwind || self.stream.is_terminated()
    }
}
