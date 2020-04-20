//! Definition of the MaybeDone combinator

use core::mem;
use core::pin::Pin;
use futures_core::future::{FusedFuture, Future};
use futures_core::task::{Context, Poll};

/// A future that may have completed.
///
/// This is created by the [`maybe_done()`] function.
#[derive(Debug)]
pub enum MaybeDone<Fut: Future> {
    /// A not-yet-completed future
    Future(Fut),
    /// The output of the completed future
    Done(Fut::Output),
    /// The empty variant after the result of a [`MaybeDone`] has been
    /// taken using the [`take_output`](MaybeDone::take_output) method.
    Gone,
}

// Safe because we never generate `Pin<&mut Fut::Output>`
impl<Fut: Future + Unpin> Unpin for MaybeDone<Fut> {}

/// Wraps a future into a `MaybeDone`
///
/// # Examples
///
/// ```
/// # futures::executor::block_on(async {
/// use futures::future;
/// use futures::pin_mut;
///
/// let future = future::maybe_done(async { 5 });
/// pin_mut!(future);
/// assert_eq!(future.as_mut().take_output(), None);
/// let () = future.as_mut().await;
/// assert_eq!(future.as_mut().take_output(), Some(5));
/// assert_eq!(future.as_mut().take_output(), None);
/// # });
/// ```
pub fn maybe_done<Fut: Future>(future: Fut) -> MaybeDone<Fut> {
    MaybeDone::Future(future)
}

impl<Fut: Future> MaybeDone<Fut> {
    /// Returns an [`Option`] containing a mutable reference to the output of the future.
    /// The output of this method will be [`Some`] if and only if the inner
    /// future has been completed and [`take_output`](MaybeDone::take_output)
    /// has not yet been called.
    #[inline]
    pub fn output_mut(self: Pin<&mut Self>) -> Option<&mut Fut::Output> {
        unsafe {
            let this = self.get_unchecked_mut();
            match this {
                MaybeDone::Done(res) => Some(res),
                _ => None,
            }
        }
    }

    /// Attempt to take the output of a `MaybeDone` without driving it
    /// towards completion.
    #[inline]
    pub fn take_output(self: Pin<&mut Self>) -> Option<Fut::Output> {
        unsafe {
            let this = self.get_unchecked_mut();
            match this {
                MaybeDone::Done(_) => {},
                MaybeDone::Future(_) | MaybeDone::Gone => return None,
            };
            if let MaybeDone::Done(output) = mem::replace(this, MaybeDone::Gone) {
                Some(output)
            } else {
                unreachable!()
            }
        }
    }
}

impl<Fut: Future> FusedFuture for MaybeDone<Fut> {
    fn is_terminated(&self) -> bool {
        match self {
            MaybeDone::Future(_) => false,
            MaybeDone::Done(_) | MaybeDone::Gone => true,
        }
    }
}

impl<Fut: Future> Future for MaybeDone<Fut> {
    type Output = ();

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        let res = unsafe {
            match self.as_mut().get_unchecked_mut() {
                MaybeDone::Future(a) => ready!(Pin::new_unchecked(a).poll(cx)),
                MaybeDone::Done(_) => return Poll::Ready(()),
                MaybeDone::Gone => panic!("MaybeDone polled after value taken"),
            }
        };
        self.set(MaybeDone::Done(res));
        Poll::Ready(())
    }
}
