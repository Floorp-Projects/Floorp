use core::pin::Pin;
use futures_core::stream::{FusedStream, Stream, TryStream};
use futures_core::task::{Context, Poll};
#[cfg(feature = "sink")]
use futures_sink::Sink;
use pin_utils::unsafe_pinned;

/// Stream for the [`try_flatten`](super::TryStreamExt::try_flatten) method.
#[derive(Debug)]
#[must_use = "streams do nothing unless polled"]
pub struct TryFlatten<St>
where
    St: TryStream,
{
    stream: St,
    next: Option<St::Ok>,
}

impl<St> Unpin for TryFlatten<St>
where
    St: TryStream + Unpin,
    St::Ok: Unpin,
{
}

impl<St> TryFlatten<St>
where
    St: TryStream,
{
    unsafe_pinned!(stream: St);
    unsafe_pinned!(next: Option<St::Ok>);
}

impl<St> TryFlatten<St>
where
    St: TryStream,
    St::Ok: TryStream,
    <St::Ok as TryStream>::Error: From<St::Error>,
{
    pub(super) fn new(stream: St) -> Self {
        Self { stream, next: None }
    }

    /// Acquires a reference to the underlying stream that this combinator is
    /// pulling from.
    pub fn get_ref(&self) -> &St {
        &self.stream
    }

    /// Acquires a mutable reference to the underlying stream that this
    /// combinator is pulling from.
    ///
    /// Note that care must be taken to avoid tampering with the state of the
    /// stream which may otherwise confuse this combinator.
    pub fn get_mut(&mut self) -> &mut St {
        &mut self.stream
    }

    /// Acquires a pinned mutable reference to the underlying stream that this
    /// combinator is pulling from.
    ///
    /// Note that care must be taken to avoid tampering with the state of the
    /// stream which may otherwise confuse this combinator.
    pub fn get_pin_mut(self: Pin<&mut Self>) -> Pin<&mut St> {
        self.stream()
    }

    /// Consumes this combinator, returning the underlying stream.
    ///
    /// Note that this may discard intermediate state of this combinator, so
    /// care should be taken to avoid losing resources when this is called.
    pub fn into_inner(self) -> St {
        self.stream
    }
}

impl<St> FusedStream for TryFlatten<St>
where
    St: TryStream + FusedStream,
    St::Ok: TryStream,
    <St::Ok as TryStream>::Error: From<St::Error>,
{
    fn is_terminated(&self) -> bool {
        self.next.is_none() && self.stream.is_terminated()
    }
}

impl<St> Stream for TryFlatten<St>
where
    St: TryStream,
    St::Ok: TryStream,
    <St::Ok as TryStream>::Error: From<St::Error>,
{
    type Item = Result<<St::Ok as TryStream>::Ok, <St::Ok as TryStream>::Error>;

    fn poll_next(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Option<Self::Item>> {
        loop {
            if self.next.is_none() {
                match ready!(self.as_mut().stream().try_poll_next(cx)?) {
                    Some(e) => self.as_mut().next().set(Some(e)),
                    None => return Poll::Ready(None),
                }
            }

            if let Some(item) = ready!(self
                .as_mut()
                .next()
                .as_pin_mut()
                .unwrap()
                .try_poll_next(cx)?)
            {
                return Poll::Ready(Some(Ok(item)));
            } else {
                self.as_mut().next().set(None);
            }
        }
    }
}

// Forwarding impl of Sink from the underlying stream
#[cfg(feature = "sink")]
impl<S, Item> Sink<Item> for TryFlatten<S>
where
    S: TryStream + Sink<Item>,
{
    type Error = <S as Sink<Item>>::Error;

    delegate_sink!(stream, Item);
}
