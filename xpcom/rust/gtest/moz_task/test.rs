/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use moz_task;
use std::{
    future::Future,
    pin::Pin,
    task::{Context, Poll, Waker},
};

/// Demo `Future` to demonstrate executing futures to completion via `nsIEventTarget`.
struct MyFuture {
    poll_count: u32,
    waker: Option<Waker>,
    expect_main_thread: bool,
}

impl MyFuture {
    fn new(expect_main_thread: bool) -> Self {
        Self {
            poll_count: 0,
            waker: None,
            expect_main_thread,
        }
    }
}

impl Future for MyFuture {
    type Output = u32;

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<u32> {
        assert_eq!(moz_task::is_main_thread(), self.expect_main_thread);

        self.poll_count += 1;

        if let Some(waker) = &mut self.waker {
            if !waker.will_wake(cx.waker()) {
                *waker = cx.waker().clone();
            }
        } else {
            let waker = cx.waker().clone();
            self.waker = Some(waker);
        }

        println!("Poll count = {}", self.poll_count);
        if self.poll_count > 5 {
            Poll::Ready(self.poll_count)
        } else {
            // Just notify the task that we need to re-polled.
            if let Some(waker) = &self.waker {
                waker.wake_by_ref();
            }
            Poll::Pending
        }
    }
}

#[no_mangle]
pub extern "C" fn Rust_Future(it_worked: *mut bool) {
    let future = async move {
        assert_eq!(MyFuture::new(true).await, 6);
        assert_eq!(
            moz_task::spawn_local("Rust_Future inner spawn_local", MyFuture::new(true)).await,
            6
        );
        assert_eq!(
            moz_task::spawn("Rust_Future inner spawn", MyFuture::new(false)).await,
            6
        );
        unsafe {
            *it_worked = true;
        }
    };
    unsafe {
        moz_task::gtest_only::spin_event_loop_until("Rust_Future", future).unwrap();
    };
}
