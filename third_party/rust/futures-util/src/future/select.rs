use core::pin::Pin;
use futures_core::future::{Future, FusedFuture};
use futures_core::task::{Context, Poll};
use crate::future::{Either, FutureExt};

/// Future for the [`select()`] function.
#[must_use = "futures do nothing unless you `.await` or poll them"]
#[derive(Debug)]
pub struct Select<A, B> {
    inner: Option<(A, B)>,
}

impl<A: Unpin, B: Unpin> Unpin for Select<A, B> {}

/// Waits for either one of two differently-typed futures to complete.
///
/// This function will return a new future which awaits for either one of both
/// futures to complete. The returned future will finish with both the value
/// resolved and a future representing the completion of the other work.
///
/// Note that this function consumes the receiving futures and returns a
/// wrapped version of them.
///
/// Also note that if both this and the second future have the same
/// output type you can use the `Either::factor_first` method to
/// conveniently extract out the value at the end.
///
/// # Examples
///
/// ```
/// use futures::future::{self, Either, Future, FutureExt};
///
/// // A poor-man's join implemented on top of select
///
/// fn join<A, B, E>(a: A, b: B) -> impl Future<Output=(A::Output, B::Output)>
///     where A: Future + Unpin,
///           B: Future + Unpin,
/// {
///     future::select(a, b).then(|either| {
///         match either {
///             Either::Left((x, b)) => b.map(move |y| (x, y)).left_future(),
///             Either::Right((y, a)) => a.map(move |x| (x, y)).right_future(),
///         }
///     })
/// }
/// ```
pub fn select<A, B>(future1: A, future2: B) -> Select<A, B>
    where A: Future + Unpin, B: Future + Unpin
{
    Select { inner: Some((future1, future2)) }
}

impl<A, B> Future for Select<A, B>
where
    A: Future + Unpin,
    B: Future + Unpin,
{
    type Output = Either<(A::Output, B), (B::Output, A)>;

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        let (mut a, mut b) = self.inner.take().expect("cannot poll Select twice");
        match a.poll_unpin(cx) {
            Poll::Ready(x) => Poll::Ready(Either::Left((x, b))),
            Poll::Pending => match b.poll_unpin(cx) {
                Poll::Ready(x) => Poll::Ready(Either::Right((x, a))),
                Poll::Pending => {
                    self.inner = Some((a, b));
                    Poll::Pending
                }
            }
        }
    }
}

impl<A, B> FusedFuture for Select<A, B>
where
    A: Future + Unpin,
    B: Future + Unpin,
{
    fn is_terminated(&self) -> bool {
        self.inner.is_none()
    }
}
