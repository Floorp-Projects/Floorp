extern crate futures;

use std::thread;

use futures::{Sink, Stream, Future};
use futures::sync::mpsc::*;

#[test]
fn smoke() {
    let (mut sender, receiver) = channel(1);

    let t = thread::spawn(move ||{
        loop {
            match sender.send(42).wait() {
                Ok(s) => sender = s,
                Err(_) => break,
            }
        }
    });

    receiver.take(3).for_each(|_| Ok(())).wait().unwrap();

    t.join().unwrap()
}
