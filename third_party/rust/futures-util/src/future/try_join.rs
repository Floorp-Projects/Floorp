#![allow(non_snake_case)]

use crate::future::{MaybeDone, maybe_done, TryFutureExt, IntoFuture};
use core::fmt;
use core::pin::Pin;
use futures_core::future::{Future, TryFuture};
use futures_core::task::{Context, Poll};
use pin_utils::unsafe_pinned;

macro_rules! generate {
    ($(
        $(#[$doc:meta])*
        ($Join:ident, <Fut1, $($Fut:ident),*>),
    )*) => ($(
        $(#[$doc])*
        #[must_use = "futures do nothing unless you `.await` or poll them"]
        pub struct $Join<Fut1: TryFuture, $($Fut: TryFuture),*> {
            Fut1: MaybeDone<IntoFuture<Fut1>>,
            $($Fut: MaybeDone<IntoFuture<$Fut>>,)*
        }

        impl<Fut1, $($Fut),*> fmt::Debug for $Join<Fut1, $($Fut),*>
        where
            Fut1: TryFuture + fmt::Debug,
            Fut1::Ok: fmt::Debug,
            Fut1::Error: fmt::Debug,
            $(
                $Fut: TryFuture + fmt::Debug,
                $Fut::Ok: fmt::Debug,
                $Fut::Error: fmt::Debug,
            )*
        {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                f.debug_struct(stringify!($Join))
                    .field("Fut1", &self.Fut1)
                    $(.field(stringify!($Fut), &self.$Fut))*
                    .finish()
            }
        }

        impl<Fut1, $($Fut),*> $Join<Fut1, $($Fut),*>
        where
            Fut1: TryFuture,
            $(
                $Fut: TryFuture<Error=Fut1::Error>
            ),*
        {
            fn new(Fut1: Fut1, $($Fut: $Fut),*) -> $Join<Fut1, $($Fut),*> {
                $Join {
                    Fut1: maybe_done(TryFutureExt::into_future(Fut1)),
                    $($Fut: maybe_done(TryFutureExt::into_future($Fut))),*
                }
            }

            unsafe_pinned!(Fut1: MaybeDone<IntoFuture<Fut1>>);
            $(
                unsafe_pinned!($Fut: MaybeDone<IntoFuture<$Fut>>);
            )*
        }

        impl<Fut1, $($Fut),*> Future for $Join<Fut1, $($Fut),*>
        where
            Fut1: TryFuture,
            $(
                $Fut: TryFuture<Error=Fut1::Error>
            ),*
        {
            type Output = Result<(Fut1::Ok, $($Fut::Ok),*), Fut1::Error>;

            fn poll(
                mut self: Pin<&mut Self>, cx: &mut Context<'_>
            ) -> Poll<Self::Output> {
                let mut all_done = true;
                if self.as_mut().Fut1().poll(cx).is_pending() {
                    all_done = false;
                } else if self.as_mut().Fut1().output_mut().unwrap().is_err() {
                    return Poll::Ready(Err(
                        self.as_mut().Fut1().take_output().unwrap().err().unwrap()));
                }
                $(
                    if self.as_mut().$Fut().poll(cx).is_pending() {
                        all_done = false;
                    } else if self.as_mut().$Fut().output_mut().unwrap().is_err() {
                        return Poll::Ready(Err(
                            self.as_mut().$Fut().take_output().unwrap().err().unwrap()));
                    }
                )*

                if all_done {
                    Poll::Ready(Ok((
                        self.as_mut().Fut1().take_output().unwrap().ok().unwrap(),
                        $(
                            self.as_mut().$Fut().take_output().unwrap().ok().unwrap()
                        ),*
                    )))
                } else {
                    Poll::Pending
                }
            }
        }
    )*)
}

generate! {
    /// Future for the [`try_join`](try_join()) function.
    (TryJoin, <Fut1, Fut2>),

    /// Future for the [`try_join3`] function.
    (TryJoin3, <Fut1, Fut2, Fut3>),

    /// Future for the [`try_join4`] function.
    (TryJoin4, <Fut1, Fut2, Fut3, Fut4>),

    /// Future for the [`try_join5`] function.
    (TryJoin5, <Fut1, Fut2, Fut3, Fut4, Fut5>),
}

/// Joins the result of two futures, waiting for them both to complete or
/// for one to produce an error.
///
/// This function will return a new future which awaits both futures to
/// complete. If successful, the returned future will finish with a tuple of
/// both results. If unsuccesful, it will complete with the first error
/// encountered.
///
/// Note that this function consumes the passed futures and returns a
/// wrapped version of it.
///
/// # Examples
///
/// When used on multiple futures that return [`Ok`], `try_join` will return
/// [`Ok`] of a tuple of the values:
///
/// ```
/// # futures::executor::block_on(async {
/// use futures::future;
///
/// let a = future::ready(Ok::<i32, i32>(1));
/// let b = future::ready(Ok::<i32, i32>(2));
/// let pair = future::try_join(a, b);
///
/// assert_eq!(pair.await, Ok((1, 2)));
/// # });
/// ```
///
/// If one of the futures resolves to an error, `try_join` will return
/// that error:
///
/// ```
/// # futures::executor::block_on(async {
/// use futures::future;
///
/// let a = future::ready(Ok::<i32, i32>(1));
/// let b = future::ready(Err::<i32, i32>(2));
/// let pair = future::try_join(a, b);
///
/// assert_eq!(pair.await, Err(2));
/// # });
/// ```
pub fn try_join<Fut1, Fut2>(future1: Fut1, future2: Fut2) -> TryJoin<Fut1, Fut2>
where
    Fut1: TryFuture,
    Fut2: TryFuture<Error = Fut1::Error>,
{
    TryJoin::new(future1, future2)
}

/// Same as [`try_join`](try_join()), but with more futures.
///
/// # Examples
///
/// ```
/// # futures::executor::block_on(async {
/// use futures::future;
///
/// let a = future::ready(Ok::<i32, i32>(1));
/// let b = future::ready(Ok::<i32, i32>(2));
/// let c = future::ready(Ok::<i32, i32>(3));
/// let tuple = future::try_join3(a, b, c);
///
/// assert_eq!(tuple.await, Ok((1, 2, 3)));
/// # });
/// ```
pub fn try_join3<Fut1, Fut2, Fut3>(
    future1: Fut1,
    future2: Fut2,
    future3: Fut3,
) -> TryJoin3<Fut1, Fut2, Fut3>
where
    Fut1: TryFuture,
    Fut2: TryFuture<Error = Fut1::Error>,
    Fut3: TryFuture<Error = Fut1::Error>,
{
    TryJoin3::new(future1, future2, future3)
}

/// Same as [`try_join`](try_join()), but with more futures.
///
/// # Examples
///
/// ```
/// # futures::executor::block_on(async {
/// use futures::future;
///
/// let a = future::ready(Ok::<i32, i32>(1));
/// let b = future::ready(Ok::<i32, i32>(2));
/// let c = future::ready(Ok::<i32, i32>(3));
/// let d = future::ready(Ok::<i32, i32>(4));
/// let tuple = future::try_join4(a, b, c, d);
///
/// assert_eq!(tuple.await, Ok((1, 2, 3, 4)));
/// # });
/// ```
pub fn try_join4<Fut1, Fut2, Fut3, Fut4>(
    future1: Fut1,
    future2: Fut2,
    future3: Fut3,
    future4: Fut4,
) -> TryJoin4<Fut1, Fut2, Fut3, Fut4>
where
    Fut1: TryFuture,
    Fut2: TryFuture<Error = Fut1::Error>,
    Fut3: TryFuture<Error = Fut1::Error>,
    Fut4: TryFuture<Error = Fut1::Error>,
{
    TryJoin4::new(future1, future2, future3, future4)
}

/// Same as [`try_join`](try_join()), but with more futures.
///
/// # Examples
///
/// ```
/// # futures::executor::block_on(async {
/// use futures::future;
///
/// let a = future::ready(Ok::<i32, i32>(1));
/// let b = future::ready(Ok::<i32, i32>(2));
/// let c = future::ready(Ok::<i32, i32>(3));
/// let d = future::ready(Ok::<i32, i32>(4));
/// let e = future::ready(Ok::<i32, i32>(5));
/// let tuple = future::try_join5(a, b, c, d, e);
///
/// assert_eq!(tuple.await, Ok((1, 2, 3, 4, 5)));
/// # });
/// ```
pub fn try_join5<Fut1, Fut2, Fut3, Fut4, Fut5>(
    future1: Fut1,
    future2: Fut2,
    future3: Fut3,
    future4: Fut4,
    future5: Fut5,
) -> TryJoin5<Fut1, Fut2, Fut3, Fut4, Fut5>
where
    Fut1: TryFuture,
    Fut2: TryFuture<Error = Fut1::Error>,
    Fut3: TryFuture<Error = Fut1::Error>,
    Fut4: TryFuture<Error = Fut1::Error>,
    Fut5: TryFuture<Error = Fut1::Error>,
{
    TryJoin5::new(future1, future2, future3, future4, future5)
}
