use stream::Stream;
use executor;

/// A stream combinator which converts an asynchronous stream to a **blocking
/// iterator**.
///
/// Created by the `Stream::wait` method, this function transforms any stream
/// into a standard iterator. This is implemented by blocking the current thread
/// while items on the underlying stream aren't ready yet.
#[must_use = "iterators do nothing unless advanced"]
#[derive(Debug)]
pub struct Wait<S> {
    stream: executor::Spawn<S>,
}

pub fn new<S: Stream>(s: S) -> Wait<S> {
    Wait {
        stream: executor::spawn(s),
    }
}

impl<S: Stream> Iterator for Wait<S> {
    type Item = Result<S::Item, S::Error>;

    fn next(&mut self) -> Option<Self::Item> {
        self.stream.wait_stream()
    }
}
