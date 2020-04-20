use core::pin::Pin;
use futures_core::future::Future;
use futures_core::task::{Context, Poll};

#[must_use = "futures do nothing unless you `.await` or poll them"]
#[derive(Debug)]
pub(crate) enum Chain<Fut1, Fut2, Data> {
    First(Fut1, Option<Data>),
    Second(Fut2),
    Empty,
}

impl<Fut1: Unpin, Fut2: Unpin, Data> Unpin for Chain<Fut1, Fut2, Data> {}

impl<Fut1, Fut2, Data> Chain<Fut1, Fut2, Data> {
    pub(crate)fn is_terminated(&self) -> bool {
        if let Chain::Empty = *self { true } else { false }
    }
}

impl<Fut1, Fut2, Data> Chain<Fut1, Fut2, Data>
    where Fut1: Future,
          Fut2: Future,
{
    pub(crate) fn new(fut1: Fut1, data: Data) -> Chain<Fut1, Fut2, Data> {
        Chain::First(fut1, Some(data))
    }

    pub(crate) fn poll<F>(
        self: Pin<&mut Self>,
        cx: &mut Context<'_>,
        f: F,
    ) -> Poll<Fut2::Output>
        where F: FnOnce(Fut1::Output, Data) -> Fut2,
    {
        let mut f = Some(f);

        // Safe to call `get_unchecked_mut` because we won't move the futures.
        let this = unsafe { self.get_unchecked_mut() };

        loop {
            let (output, data) = match this {
                Chain::First(fut1, data) => {
                    let output = ready!(unsafe { Pin::new_unchecked(fut1) }.poll(cx));
                    (output, data.take().unwrap())
                }
                Chain::Second(fut2) => {
                    return unsafe { Pin::new_unchecked(fut2) }.poll(cx);
                }
                Chain::Empty => unreachable!()
            };

            *this = Chain::Empty; // Drop fut1
            let fut2 = (f.take().unwrap())(output, data);
            *this = Chain::Second(fut2)
        }
    }
}
