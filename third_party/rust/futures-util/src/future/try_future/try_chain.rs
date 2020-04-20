use core::pin::Pin;
use futures_core::future::TryFuture;
use futures_core::task::{Context, Poll};

#[must_use = "futures do nothing unless you `.await` or poll them"]
#[derive(Debug)]
pub(crate) enum TryChain<Fut1, Fut2, Data> {
    First(Fut1, Option<Data>),
    Second(Fut2),
    Empty,
}

impl<Fut1: Unpin, Fut2: Unpin, Data> Unpin for TryChain<Fut1, Fut2, Data> {}

pub(crate) enum TryChainAction<Fut2>
    where Fut2: TryFuture,
{
    Future(Fut2),
    Output(Result<Fut2::Ok, Fut2::Error>),
}

impl<Fut1, Fut2, Data> TryChain<Fut1, Fut2, Data>
    where Fut1: TryFuture,
          Fut2: TryFuture,
{
    pub(crate) fn new(fut1: Fut1, data: Data) -> TryChain<Fut1, Fut2, Data> {
        TryChain::First(fut1, Some(data))
    }

    pub(crate) fn is_terminated(&self) -> bool {
        match self {
            TryChain::First(..) | TryChain::Second(_) => false,
            TryChain::Empty => true,
        }
    }

    pub(crate) fn poll<F>(
        self: Pin<&mut Self>,
        cx: &mut Context<'_>,
        f: F,
    ) -> Poll<Result<Fut2::Ok, Fut2::Error>>
        where F: FnOnce(Result<Fut1::Ok, Fut1::Error>, Data) -> TryChainAction<Fut2>,
    {
        let mut f = Some(f);

        // Safe to call `get_unchecked_mut` because we won't move the futures.
        let this = unsafe { self.get_unchecked_mut() };

        loop {
            let (output, data) = match this {
                TryChain::First(fut1, data) => {
                    // Poll the first future
                    let output = ready!(unsafe { Pin::new_unchecked(fut1) }.try_poll(cx));
                    (output, data.take().unwrap())
                }
                TryChain::Second(fut2) => {
                    // Poll the second future
                    return unsafe { Pin::new_unchecked(fut2) }
                        .try_poll(cx)
                        .map(|res| {
                            *this = TryChain::Empty; // Drop fut2.
                            res
                        });
                }
                TryChain::Empty => {
                    panic!("future must not be polled after it returned `Poll::Ready`");
                }
            };

            *this = TryChain::Empty; // Drop fut1
            let f = f.take().unwrap();
            match f(output, data) {
                TryChainAction::Future(fut2) => *this = TryChain::Second(fut2),
                TryChainAction::Output(output) => return Poll::Ready(output),
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use std::pin::Pin;
    use std::task::Poll;

    use futures_test::task::noop_context;

    use crate::future::ready;

    use super::{TryChain, TryChainAction};

    #[test]
    fn try_chain_is_terminated() {
        let mut cx = noop_context();

        let mut future = TryChain::new(ready(Ok(1)), ());
        assert!(!future.is_terminated());

        let res = Pin::new(&mut future).poll(
            &mut cx,
            |res: Result<usize, ()>, ()| {
                assert!(res.is_ok());
                TryChainAction::Future(ready(Ok(2)))
            },
        );
        assert_eq!(res, Poll::Ready::<Result<usize, ()>>(Ok(2)));
        assert!(future.is_terminated());
    }
}
