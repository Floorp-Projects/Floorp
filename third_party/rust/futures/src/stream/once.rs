use core;

use Poll;
use stream;
use stream::Stream;

/// A stream which emits single element and then EOF.
///
/// This stream will never block and is always ready.
#[derive(Debug)]
#[must_use = "streams do nothing unless polled"]
pub struct Once<T, E>(stream::Iter<core::iter::Once<Result<T, E>>>);

/// Creates a stream of single element
///
/// ```rust
/// use futures::*;
///
/// let mut stream = stream::once::<(), _>(Err(17));
/// assert_eq!(Err(17), stream.poll());
/// assert_eq!(Ok(Async::Ready(None)), stream.poll());
/// ```
pub fn once<T, E>(item: Result<T, E>) -> Once<T, E> {
    Once(stream::iter(core::iter::once(item)))
}

impl<T, E> Stream for Once<T, E> {
    type Item = T;
    type Error = E;

    fn poll(&mut self) -> Poll<Option<T>, E> {
        self.0.poll()
    }
}
