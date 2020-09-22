/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use moz_task;
use std::{
    future::Future,
    pin::Pin,
    sync::atomic::{AtomicBool, Ordering::Relaxed},
    sync::Arc,
    task::{Context, Poll, Waker},
};

/// Demo `Future` to demonstrate executing futures to completion via `nsIEventTarget`.
struct MyFuture {
    poll_count: u32,
    waker: Option<Waker>,
}

impl Default for MyFuture {
    fn default() -> Self {
        Self {
            poll_count: 0,
            waker: None,
        }
    }
}

impl Future for MyFuture {
    type Output = ();

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<()> {
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
                Poll::Ready(())
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
    let done = Arc::new(AtomicBool::new(false));
    let done2 = done.clone();

    moz_task::spawn_current_thread(async move {
        MyFuture::default().await;
        done.store(true, Relaxed);
    })
    .unwrap();

    unsafe {
        moz_task::gtest_only::spin_event_loop_until(move || done2.load(Relaxed)).unwrap();
        *it_worked = true;
    };
}
