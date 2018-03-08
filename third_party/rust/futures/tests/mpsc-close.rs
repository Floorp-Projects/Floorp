extern crate futures;

use std::thread;

use futures::prelude::*;
use futures::sync::mpsc::*;

#[test]
fn smoke() {
    let (mut sender, receiver) = channel(1);

    let t = thread::spawn(move ||{
        while let Ok(s) = sender.send(42).wait() {
            sender = s;
        }
    });

    receiver.take(3).for_each(|_| Ok(())).wait().unwrap();

    t.join().unwrap()
}
