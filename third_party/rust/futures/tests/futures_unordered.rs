extern crate futures;

use std::any::Any;

use futures::sync::oneshot;
use futures::stream::futures_unordered;
use futures::Future;

mod support;

#[test]
fn works_1() {
    let (a_tx, a_rx) = oneshot::channel::<u32>();
    let (b_tx, b_rx) = oneshot::channel::<u32>();
    let (c_tx, c_rx) = oneshot::channel::<u32>();

    let stream = futures_unordered(vec![a_rx, b_rx, c_rx]);

    let mut spawn = futures::executor::spawn(stream);
    b_tx.send(99).unwrap();
    assert_eq!(Some(Ok(99)), spawn.wait_stream());

    a_tx.send(33).unwrap();
    c_tx.send(33).unwrap();
    assert_eq!(Some(Ok(33)), spawn.wait_stream());
    assert_eq!(Some(Ok(33)), spawn.wait_stream());
    assert_eq!(None, spawn.wait_stream());
}

#[test]
fn works_2() {
    let (a_tx, a_rx) = oneshot::channel::<u32>();
    let (b_tx, b_rx) = oneshot::channel::<u32>();
    let (c_tx, c_rx) = oneshot::channel::<u32>();

    let stream = futures_unordered(vec![a_rx.boxed(), b_rx.join(c_rx).map(|(a, b)| a + b).boxed()]);

    let mut spawn = futures::executor::spawn(stream);
    a_tx.send(33).unwrap();
    b_tx.send(33).unwrap();
    assert!(spawn.poll_stream(support::unpark_noop()).unwrap().is_ready());
    c_tx.send(33).unwrap();
    assert!(spawn.poll_stream(support::unpark_noop()).unwrap().is_ready());
}

#[test]
fn finished_future_ok() {
    let (_a_tx, a_rx) = oneshot::channel::<Box<Any+Send>>();
    let (b_tx, b_rx) = oneshot::channel::<Box<Any+Send>>();
    let (c_tx, c_rx) = oneshot::channel::<Box<Any+Send>>();

    let stream = futures_unordered(vec![
        a_rx.boxed(),
        b_rx.select(c_rx).then(|res| Ok(Box::new(res) as Box<Any+Send>)).boxed(),
    ]);

    let mut spawn = futures::executor::spawn(stream);
    for _ in 0..10 {
        assert!(spawn.poll_stream(support::unpark_noop()).unwrap().is_not_ready());
    }

    b_tx.send(Box::new(())).unwrap();
    let next = spawn.poll_stream(support::unpark_noop()).unwrap();
    assert!(next.is_ready());
    c_tx.send(Box::new(())).unwrap();
    assert!(spawn.poll_stream(support::unpark_noop()).unwrap().is_not_ready());
    assert!(spawn.poll_stream(support::unpark_noop()).unwrap().is_not_ready());
}
