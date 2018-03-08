extern crate futures;

use futures::prelude::*;
use futures::future::ok;
use futures::executor;

mod support;
use support::*;

#[test]
fn fuse() {
    let mut future = executor::spawn(ok::<i32, u32>(2).fuse());
    assert!(future.poll_future_notify(&notify_panic(), 0).unwrap().is_ready());
    assert!(future.poll_future_notify(&notify_panic(), 0).unwrap().is_not_ready());
}
