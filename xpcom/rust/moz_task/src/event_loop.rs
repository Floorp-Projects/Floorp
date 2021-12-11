/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate nsstring;

use cstr::cstr;
use nserror::{nsresult, NS_ERROR_SERVICE_NOT_AVAILABLE, NS_OK};
use nsstring::*;
use std::cell::UnsafeCell;
use xpcom::{interfaces::nsIThreadManager, xpcom, xpcom_method};

type IsDoneClosure = dyn FnMut() -> bool + 'static;

#[derive(xpcom)]
#[xpimplements(nsINestedEventLoopCondition)]
#[refcnt = "atomic"]
struct InitEventLoopCondition {
    closure: UnsafeCell<Box<IsDoneClosure>>,
}

impl EventLoopCondition {
    xpcom_method!(is_done => IsDone() -> bool);
    fn is_done(&self) -> Result<bool, nsresult> {
        unsafe { Ok((&mut *self.closure.get())()) }
    }
}

/// Spin the event loop on the current thread until `pred` returns true.
///
/// # Safety
///
/// Spinning a nested event loop should always be avoided when possible, as it
/// can cause hangs, break JS run-to-completion guarantees, and break other C++
/// code currently on the stack relying on heap invariants. While in a pure-rust
/// codebase this method would only be ill-advised and not technically "unsafe",
/// it is marked as unsafe due to the potential for triggering unsafety in
/// unrelated C++ code.
pub unsafe fn spin_event_loop_until<P>(pred: P) -> Result<(), nsresult>
where
    P: FnMut() -> bool + 'static,
{
    let closure = Box::new(pred) as Box<IsDoneClosure>;
    let cond = EventLoopCondition::allocate(InitEventLoopCondition {
        closure: UnsafeCell::new(closure),
    });
    let thread_manager =
        xpcom::get_service::<nsIThreadManager>(cstr!("@mozilla.org/thread-manager;1"))
            .ok_or(NS_ERROR_SERVICE_NOT_AVAILABLE)?;

    // XXX: Pass in aVeryGoodReason from caller
    thread_manager
        .SpinEventLoopUntil(
            &*nsCStr::from("event_loop.rs: Rust is spinning the event loop."),
            cond.coerce(),
        )
        .to_result()
}
