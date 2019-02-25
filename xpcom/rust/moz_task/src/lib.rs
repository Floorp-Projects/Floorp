/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module wraps XPCOM threading functions with Rust functions
//! to make it safer and more convenient to call the XPCOM functions.
//! It also provides the Task trait and TaskRunnable struct,
//! which make it easier to dispatch tasks to threads.

extern crate nserror;
extern crate nsstring;
#[macro_use]
extern crate xpcom;

use nserror::{nsresult, NsresultExt, NS_OK};
use nsstring::{nsACString, nsCString};
use std::{
    ptr,
    sync::atomic::{AtomicBool, Ordering},
};
use xpcom::{
    getter_addrefs,
    interfaces::{nsIEventTarget, nsIRunnable, nsIThread},
    RefPtr,
};

extern "C" {
    fn NS_GetCurrentThreadEventTarget(result: *mut *const nsIThread) -> nsresult;
    fn NS_GetMainThreadEventTarget(result: *mut *const nsIThread) -> nsresult;
    fn NS_IsMainThread() -> bool;
    fn NS_NewNamedThreadWithDefaultStackSize(
        name: *const nsACString,
        result: *mut *const nsIThread,
        event: *const nsIRunnable,
    ) -> nsresult;
}

pub fn get_current_thread() -> Result<RefPtr<nsIThread>, nsresult> {
    getter_addrefs(|p| unsafe { NS_GetCurrentThreadEventTarget(p) })
}

pub fn get_main_thread() -> Result<RefPtr<nsIThread>, nsresult> {
    getter_addrefs(|p| unsafe { NS_GetMainThreadEventTarget(p) })
}

pub fn is_main_thread() -> bool {
    unsafe { NS_IsMainThread() }
}

pub fn create_thread(name: &str) -> Result<RefPtr<nsIThread>, nsresult> {
    getter_addrefs(|p| unsafe {
        NS_NewNamedThreadWithDefaultStackSize(&*nsCString::from(name), p, ptr::null())
    })
}

/// A database operation that is executed asynchronously on a database thread
/// and returns its result to the original thread from which it was dispatched.
pub trait Task {
    fn run(&self);
    fn done(&self) -> Result<(), nsresult>;
}

/// The struct responsible for dispatching a Task by calling its run() method
/// on the target thread and returning its result by calling its done() method
/// on the original thread.
///
/// The struct uses its has_run field to determine whether it should call
/// run() or done().  It could instead check if task.result is Some or None,
/// but if run() failed to set task.result, then it would loop infinitely.
#[derive(xpcom)]
#[xpimplements(nsIRunnable, nsINamed)]
#[refcnt = "atomic"]
pub struct InitTaskRunnable {
    name: &'static str,
    task: Box<dyn Task + Send + Sync>,
    has_run: AtomicBool,
}

impl TaskRunnable {
    pub fn new(
        name: &'static str,
        task: Box<dyn Task + Send + Sync>,
    ) -> Result<RefPtr<TaskRunnable>, nsresult> {
        assert!(is_main_thread());
        Ok(TaskRunnable::allocate(InitTaskRunnable {
            name,
            task,
            has_run: AtomicBool::new(false),
        }))
    }
    pub fn dispatch(&self, target_thread: RefPtr<nsIThread>) -> Result<(), nsresult> {
        unsafe {
            target_thread.DispatchFromScript(self.coerce(), nsIEventTarget::DISPATCH_NORMAL as u32)
        }
        .to_result()
    }

    xpcom_method!(run => Run());
    fn run(&self) -> Result<(), nsresult> {
        match self
            .has_run
            .compare_exchange(false, true, Ordering::AcqRel, Ordering::Acquire)
        {
            Ok(_) => {
                assert!(!is_main_thread());
                self.task.run();
                self.dispatch(get_main_thread()?)
            }
            Err(_) => {
                assert!(is_main_thread());
                self.task.done()
            }
        }
    }

    xpcom_method!(get_name => GetName() -> nsACString);
    fn get_name(&self) -> Result<nsCString, nsresult> {
        Ok(nsCString::from(self.name))
    }
}
