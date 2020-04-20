use core::fmt;
use core::pin::Pin;
use futures_core::future::Future;
use futures_core::stream::{FusedStream, Stream};
use futures_core::task::{Context, Poll};
use pin_utils::unsafe_pinned;

/// Stream for the [`flatten_stream`](super::FutureExt::flatten_stream) method.
#[must_use = "streams do nothing unless polled"]
pub struct FlattenStream<Fut: Future> {
    state: State<Fut, Fut::Output>,
}

impl<Fut: Future> FlattenStream<Fut> {
    unsafe_pinned!(state: State<Fut, Fut::Output>);

    pub(super) fn new(future: Fut) -> FlattenStream<Fut> {
        FlattenStream {
            state: State::Future(future)
        }
    }
}

impl<Fut> fmt::Debug for FlattenStream<Fut>
    where Fut: Future + fmt::Debug,
          Fut::Output: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("FlattenStream")
            .field("state", &self.state)
            .finish()
    }
}

#[derive(Debug)]
enum State<Fut, St> {
    // future is not yet called or called and not ready
    Future(Fut),
    // future resolved to Stream
    Stream(St),
}

impl<Fut, St> State<Fut, St> {
    fn get_pin_mut(self: Pin<&mut Self>) -> State<Pin<&mut Fut>, Pin<&mut St>> {
        // safety: data is never moved via the resulting &mut reference
        match unsafe { self.get_unchecked_mut() } {
            // safety: the future we're re-pinning here will never be moved;
            // it will just be polled, then dropped in place
            State::Future(f) => State::Future(unsafe { Pin::new_unchecked(f) }),
            // safety: the stream we're repinning here will never be moved;
            // it will just be polled, then dropped in place
            State::Stream(s) => State::Stream(unsafe { Pin::new_unchecked(s) }),
        }
    }
}

impl<Fut> FusedStream for FlattenStream<Fut>
    where Fut: Future,
          Fut::Output: Stream + FusedStream,
{
    fn is_terminated(&self) -> bool {
        match &self.state {
            State::Future(_) => false,
            State::Stream(stream) => stream.is_terminated(),
        }
    }
}

impl<Fut> Stream for FlattenStream<Fut>
    where Fut: Future,
          Fut::Output: Stream,
{
    type Item = <Fut::Output as Stream>::Item;

    fn poll_next(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Option<Self::Item>> {
        loop {
            match self.as_mut().state().get_pin_mut() {
                State::Future(f) => {
                    let stream = ready!(f.poll(cx));
                    // Future resolved to stream.
                    // We do not return, but poll that
                    // stream in the next loop iteration.
                    self.as_mut().state().set(State::Stream(stream));
                }
                State::Stream(s) => return s.poll_next(cx),
            }
        }
    }
}
