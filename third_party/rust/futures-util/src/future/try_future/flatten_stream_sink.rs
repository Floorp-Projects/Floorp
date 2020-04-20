use core::fmt;
use core::pin::Pin;
use futures_core::future::TryFuture;
use futures_core::stream::{FusedStream, Stream, TryStream};
use futures_core::task::{Context, Poll};
#[cfg(feature = "sink")]
use futures_sink::Sink;
use pin_utils::unsafe_pinned;

#[must_use = "streams do nothing unless polled"]
pub(crate) struct FlattenStreamSink<Fut>
where
    Fut: TryFuture,
{
    state: State<Fut, Fut::Ok>,
}

impl<Fut> Unpin for FlattenStreamSink<Fut>
where
    Fut: TryFuture + Unpin,
    Fut::Ok: Unpin,
{
}

impl<Fut> fmt::Debug for FlattenStreamSink<Fut>
where
    Fut: TryFuture + fmt::Debug,
    Fut::Ok: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("FlattenStreamSink")
            .field("state", &self.state)
            .finish()
    }
}

impl<Fut> FlattenStreamSink<Fut>
where
    Fut: TryFuture,
{
    unsafe_pinned!(state: State<Fut, Fut::Ok>);

    pub(crate) fn new(future: Fut) -> Self {
        Self {
            state: State::Future(future),
        }
    }
}

#[derive(Debug)]
enum State<Fut, S> {
    // future is not yet called or called and not ready
    Future(Fut),
    // future resolved to Stream or Sink
    StreamOrSink(S),
    // future resolved to error
    Done,
}

impl<Fut, S> State<Fut, S> {
    fn get_pin_mut(self: Pin<&mut Self>) -> State<Pin<&mut Fut>, Pin<&mut S>> {
        // safety: data is never moved via the resulting &mut reference
        match unsafe { self.get_unchecked_mut() } {
            // safety: the future we're re-pinning here will never be moved;
            // it will just be polled, then dropped in place
            State::Future(f) => State::Future(unsafe { Pin::new_unchecked(f) }),
            // safety: the stream we're repinning here will never be moved;
            // it will just be polled, then dropped in place
            State::StreamOrSink(s) => State::StreamOrSink(unsafe { Pin::new_unchecked(s) }),
            State::Done => State::Done,
        }
    }
}

impl<Fut> State<Fut, Fut::Ok>
where
    Fut: TryFuture,
{
    fn poll_future(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Result<(), Fut::Error>> {
        if let State::Future(f) = self.as_mut().get_pin_mut() {
            match ready!(f.try_poll(cx)) {
                Ok(s) => {
                    // Future resolved to stream.
                    // We do not return, but poll that
                    // stream in the next loop iteration.
                    self.set(State::StreamOrSink(s));
                }
                Err(e) => {
                    // Future resolved to error.
                    // We have neither a pollable stream nor a future.
                    self.set(State::Done);
                    return Poll::Ready(Err(e));
                }
            }
        }
        Poll::Ready(Ok(()))
    }
}

impl<Fut> FusedStream for FlattenStreamSink<Fut>
where
    Fut: TryFuture,
    Fut::Ok: TryStream<Error = Fut::Error> + FusedStream,
{
    fn is_terminated(&self) -> bool {
        match &self.state {
            State::Future(_) => false,
            State::StreamOrSink(stream) => stream.is_terminated(),
            State::Done => true,
        }
    }
}

impl<Fut> Stream for FlattenStreamSink<Fut>
where
    Fut: TryFuture,
    Fut::Ok: TryStream<Error = Fut::Error>,
{
    type Item = Result<<Fut::Ok as TryStream>::Ok, Fut::Error>;

    fn poll_next(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Option<Self::Item>> {
        ready!(self.as_mut().state().poll_future(cx)?);
        match self.as_mut().state().get_pin_mut() {
            State::StreamOrSink(s) => s.try_poll_next(cx),
            State::Done => Poll::Ready(None),
            State::Future(_) => unreachable!(),
        }
    }
}

#[cfg(feature = "sink")]
impl<Fut, Item> Sink<Item> for FlattenStreamSink<Fut>
where
    Fut: TryFuture,
    Fut::Ok: Sink<Item, Error = Fut::Error>,
{
    type Error = Fut::Error;

    fn poll_ready(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Result<(), Self::Error>> {
        ready!(self.as_mut().state().poll_future(cx)?);
        match self.as_mut().state().get_pin_mut() {
            State::StreamOrSink(s) => s.poll_ready(cx),
            State::Done => panic!("poll_ready called after eof"),
            State::Future(_) => unreachable!(),
        }
    }

    fn start_send(self: Pin<&mut Self>, item: Item) -> Result<(), Self::Error> {
        match self.state().get_pin_mut() {
            State::StreamOrSink(s) => s.start_send(item),
            State::Future(_) => panic!("poll_ready not called first"),
            State::Done => panic!("start_send called after eof"),
        }
    }

    fn poll_flush(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Result<(), Self::Error>> {
        match self.state().get_pin_mut() {
            State::StreamOrSink(s) => s.poll_flush(cx),
            // if sink not yet resolved, nothing written ==> everything flushed
            State::Future(_) => Poll::Ready(Ok(())),
            State::Done => panic!("poll_flush called after eof"),
        }
    }

    fn poll_close(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Result<(), Self::Error>> {
        let res = match self.as_mut().state().get_pin_mut() {
            State::StreamOrSink(s) => s.poll_close(cx),
            State::Future(_) | State::Done => Poll::Ready(Ok(())),
        };
        if res.is_ready() {
            self.as_mut().state().set(State::Done);
        }
        res
    }
}
