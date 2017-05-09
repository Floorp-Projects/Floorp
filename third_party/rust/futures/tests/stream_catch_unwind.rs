extern crate futures;

use futures::stream;
use futures::stream::Stream;

#[test]
fn panic_in_the_middle_of_the_stream() {
    let stream = stream::iter::<_, Option<i32>, bool>(vec![
        Some(10), None, Some(11)].into_iter().map(Ok));

    // panic on second element
    let stream_panicking = stream.map(|o| o.unwrap());
    let mut iter = stream_panicking.catch_unwind().wait();

    assert_eq!(Ok(10), iter.next().unwrap().ok().unwrap());
    assert!(iter.next().unwrap().is_err());
    assert!(iter.next().is_none());
}

#[test]
fn no_panic() {
    let stream = stream::iter::<_, _, bool>(vec![
        10, 11, 12].into_iter().map(Ok));

    let mut iter = stream.catch_unwind().wait();

    assert_eq!(Ok(10), iter.next().unwrap().ok().unwrap());
    assert_eq!(Ok(11), iter.next().unwrap().ok().unwrap());
    assert_eq!(Ok(12), iter.next().unwrap().ok().unwrap());
    assert!(iter.next().is_none());
}
