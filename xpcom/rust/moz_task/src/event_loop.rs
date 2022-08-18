/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate nsstring;

use cstr::cstr;
use nserror::{nsresult, NS_ERROR_SERVICE_NOT_AVAILABLE, NS_ERROR_UNEXPECTED, NS_OK};
use nsstring::*;
use std::cell::RefCell;
use std::future::Future;
use xpcom::{interfaces::nsIThreadManager, xpcom, xpcom_method};

#[xpcom(implement(nsINestedEventLoopCondition), nonatomic)]
struct FutureCompleteCondition<T: 'static> {
    value: RefCell<Option<T>>,
}

impl<T: 'static> FutureCompleteCondition<T> {
    xpcom_method!(is_done => IsDone() -> bool);
    fn is_done(&self) -> Result<bool, nsresult> {
        Ok(self.value.borrow().is_some())
    }
}

/// Spin the event loop on the current thread until `future` is resolved.
///
/// # Safety
///
/// Spinning a nested event loop should always be avoided when possible, as it
/// can cause hangs, break JS run-to-completion guarantees, and break other C++
/// code currently on the stack relying on heap invariants. While in a pure-rust
/// codebase this method would only be ill-advised and not technically "unsafe",
/// it is marked as unsafe due to the potential for triggering unsafety in
/// unrelated C++ code.
pub unsafe fn spin_event_loop_until<F>(
    reason: &'static str,
    future: F,
) -> Result<F::Output, nsresult>
where
    F: Future + 'static,
    F::Output: 'static,
{
    let thread_manager =
        xpcom::get_service::<nsIThreadManager>(cstr!("@mozilla.org/thread-manager;1"))
            .ok_or(NS_ERROR_SERVICE_NOT_AVAILABLE)?;

    let cond = FutureCompleteCondition::<F::Output>::allocate(InitFutureCompleteCondition {
        value: RefCell::new(None),
    });

    // Spawn our future onto the current thread event loop, and record the
    // completed value as it completes.
    let cond2 = cond.clone();
    crate::spawn_local(reason, async move {
        let rv = future.await;
        *cond2.value.borrow_mut() = Some(rv);
    })
    .detach();

    thread_manager
        .SpinEventLoopUntil(&*nsCStr::from(reason), cond.coerce())
        .to_result()?;
    let rv = cond.value.borrow_mut().take();
    rv.ok_or(NS_ERROR_UNEXPECTED)
}
