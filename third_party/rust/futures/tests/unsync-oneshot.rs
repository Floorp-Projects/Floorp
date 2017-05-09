extern crate futures;

use futures::Future;
use futures::future;
use futures::unsync::oneshot::{channel, Canceled};

#[test]
fn smoke() {
    let (tx, rx) = channel();
    tx.send(33).unwrap();
    assert_eq!(rx.wait().unwrap(), 33);
}

#[test]
fn canceled() {
    let (_, rx) = channel::<()>();
    assert_eq!(rx.wait().unwrap_err(), Canceled);
}

#[test]
fn poll_cancel() {
    let (mut tx, _) = channel::<()>();
    assert!(tx.poll_cancel().unwrap().is_ready());
}

#[test]
fn tx_complete_rx_unparked() {
    let (tx, rx) = channel();

    let res = rx.join(future::lazy(move || {
        tx.send(55).unwrap();
        Ok(11)
    }));
    assert_eq!(res.wait().unwrap(), (55, 11));
}

#[test]
fn tx_dropped_rx_unparked() {
    let (tx, rx) = channel::<i32>();

    let res = rx.join(future::lazy(move || {
        let _tx = tx;
        Ok(11)
    }));
    assert_eq!(res.wait().unwrap_err(), Canceled);
}
