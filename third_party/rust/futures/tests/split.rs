extern crate futures;

use futures::{Future, StartSend, Sink, Stream, Poll};
use futures::stream::iter;

struct Join<T, U>(T, U);

impl<T: Stream, U> Stream for Join<T, U> {
    type Item = T::Item;
    type Error = T::Error;

    fn poll(&mut self) -> Poll<Option<T::Item>, T::Error> {
        self.0.poll()
    }
}

impl<T, U: Sink> Sink for Join<T, U> {
    type SinkItem = U::SinkItem;
    type SinkError = U::SinkError;

    fn start_send(&mut self, item: U::SinkItem)
        -> StartSend<U::SinkItem, U::SinkError>
    {
        self.1.start_send(item)
    }

    fn poll_complete(&mut self) -> Poll<(), U::SinkError> {
        self.1.poll_complete()
    }

    fn close(&mut self) -> Poll<(), U::SinkError> {
        self.1.close()
    }
}

#[test]
fn test_split() {
    let mut dest = Vec::new();
    {
        let j = Join(iter(vec![Ok(10), Ok(20), Ok(30)]), &mut dest);
        let (sink, stream) = j.split();
        sink.send_all(stream).wait().unwrap();
    }
    assert_eq!(dest, vec![10, 20, 30]);
}
