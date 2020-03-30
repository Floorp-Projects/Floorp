/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module wraps XPCOM threading functions with Rust functions
//! to make it safer and more convenient to call the XPCOM functions.
//! It also provides the Task trait and TaskRunnable struct,
//! which make it easier to dispatch tasks to threads.

extern crate libc;
extern crate nserror;
extern crate nsstring;
extern crate xpcom;

use nserror::{nsresult, NS_OK};
use nsstring::{nsACString, nsCString};
use std::{
    ffi::CStr,
    marker::PhantomData,
    mem, ptr,
    sync::atomic::{AtomicBool, Ordering},
};
use xpcom::{
    getter_addrefs,
    interfaces::{nsIEventTarget, nsIRunnable, nsISerialEventTarget, nsISupports, nsIThread},
    xpcom, xpcom_method, AtomicRefcnt, RefCounted, RefPtr, XpCom,
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
    fn NS_IsCurrentThread(thread: *const nsIEventTarget) -> bool;
    fn NS_ProxyReleaseISupports(
        name: *const libc::c_char,
        target: *const nsIEventTarget,
        doomed: *const nsISupports,
        always_proxy: bool,
    );
    fn NS_CreateBackgroundTaskQueue(
        name: *const libc::c_char,
        target: *mut *const nsISerialEventTarget,
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

pub fn is_current_thread(thread: &nsIThread) -> bool {
    unsafe { NS_IsCurrentThread(thread.coerce()) }
}

pub fn create_background_task_queue(
    name: &'static CStr,
) -> Result<RefPtr<nsISerialEventTarget>, nsresult> {
    getter_addrefs(|p| unsafe { NS_CreateBackgroundTaskQueue(name.as_ptr(), p) })
}

/// Options to control how task runnables are dispatched.
#[derive(Copy, Clone, Debug, Eq, Hash, PartialEq)]
pub struct DispatchOptions(u32);

impl Default for DispatchOptions {
    #[inline]
    fn default() -> Self {
        DispatchOptions(nsIEventTarget::DISPATCH_NORMAL as u32)
    }
}

impl DispatchOptions {
    /// Creates a blank set of options. The runnable will be dispatched using
    /// the default mode.
    #[inline]
    pub fn new() -> Self {
        DispatchOptions::default()
    }

    /// Indicates whether or not the dispatched runnable may block its target
    /// thread by waiting on I/O. If `true`, the runnable may be dispatched to a
    /// dedicated thread pool, leaving the main pool free for CPU-bound tasks.
    #[inline]
    pub fn may_block(self, may_block: bool) -> DispatchOptions {
        const FLAG: u32 = nsIEventTarget::DISPATCH_EVENT_MAY_BLOCK as u32;
        if may_block {
            DispatchOptions(self.flags() | FLAG)
        } else {
            DispatchOptions(self.flags() & !FLAG)
        }
    }

    /// Returns the set of bitflags to pass to `DispatchFromScript`.
    #[inline]
    fn flags(self) -> u32 {
        self.0
    }
}

/// A task represents an operation that asynchronously executes on a target
/// thread, and returns its result to the original thread.
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
    original_thread: RefPtr<nsIThread>,
    task: Box<dyn Task + Send + Sync>,
    has_run: AtomicBool,
}

impl TaskRunnable {
    pub fn new(
        name: &'static str,
        task: Box<dyn Task + Send + Sync>,
    ) -> Result<RefPtr<TaskRunnable>, nsresult> {
        Ok(TaskRunnable::allocate(InitTaskRunnable {
            name,
            original_thread: get_current_thread()?,
            task,
            has_run: AtomicBool::new(false),
        }))
    }

    #[inline]
    pub fn dispatch(&self, target_thread: &nsIEventTarget) -> Result<(), nsresult> {
        self.dispatch_with_options(target_thread, DispatchOptions::default())
    }

    pub fn dispatch_with_options(
        &self,
        target_thread: &nsIEventTarget,
        options: DispatchOptions,
    ) -> Result<(), nsresult> {
        unsafe { target_thread.DispatchFromScript(self.coerce(), options.flags()) }.to_result()
    }

    xpcom_method!(run => Run());
    fn run(&self) -> Result<(), nsresult> {
        match self
            .has_run
            .compare_exchange(false, true, Ordering::AcqRel, Ordering::Acquire)
        {
            Ok(_) => {
                assert!(!is_current_thread(&self.original_thread));
                self.task.run();
                self.dispatch(&self.original_thread)
            }
            Err(_) => {
                assert!(is_current_thread(&self.original_thread));
                self.task.done()
            }
        }
    }

    xpcom_method!(get_name => GetName() -> nsACString);
    fn get_name(&self) -> Result<nsCString, nsresult> {
        Ok(nsCString::from(self.name))
    }
}

pub type ThreadPtrHandle<T> = RefPtr<ThreadPtrHolder<T>>;

/// A Rust analog to `nsMainThreadPtrHolder` that wraps an `nsISupports` object
/// with thread-safe refcounting. The holder keeps one reference to the wrapped
/// object that's released when the holder's refcount reaches zero.
pub struct ThreadPtrHolder<T: XpCom + 'static> {
    ptr: *const T,
    marker: PhantomData<T>,
    name: &'static CStr,
    owning_thread: RefPtr<nsIThread>,
    refcnt: AtomicRefcnt,
}

unsafe impl<T: XpCom + 'static> Send for ThreadPtrHolder<T> {}
unsafe impl<T: XpCom + 'static> Sync for ThreadPtrHolder<T> {}

unsafe impl<T: XpCom + 'static> RefCounted for ThreadPtrHolder<T> {
    unsafe fn addref(&self) {
        self.refcnt.inc();
    }

    unsafe fn release(&self) {
        let rc = self.refcnt.dec();
        if rc == 0 {
            // Once the holder's count reaches zero, release the wrapped
            // object...
            if !self.ptr.is_null() {
                // The holder can be released on any thread. If we're on the
                // owning thread, we can release the object directly. Otherwise,
                // we need to post a proxy release event to release the object
                // on the owning thread.
                if is_current_thread(&self.owning_thread) {
                    (*self.ptr).release()
                } else {
                    NS_ProxyReleaseISupports(
                        self.name.as_ptr(),
                        self.owning_thread.coerce(),
                        self.ptr as *const T as *const nsISupports,
                        false,
                    );
                }
            }
            // ...And deallocate the holder.
            Box::from_raw(self as *const Self as *mut Self);
        }
    }
}

impl<T: XpCom + 'static> ThreadPtrHolder<T> {
    /// Creates a new owning thread pointer holder. Returns an error if the
    /// thread manager has shut down. Panics if `name` isn't a valid C string.
    pub fn new(name: &'static CStr, ptr: RefPtr<T>) -> Result<RefPtr<Self>, nsresult> {
        let owning_thread = get_current_thread()?;
        // Take ownership of the `RefPtr`. This does _not_ decrement its
        // refcount, which is what we want. Once we've released all references
        // to the holder, we'll release the wrapped `RefPtr`.
        let raw: *const T = &*ptr;
        mem::forget(ptr);
        unsafe {
            let boxed = Box::new(ThreadPtrHolder {
                name,
                ptr: raw,
                marker: PhantomData,
                owning_thread,
                refcnt: AtomicRefcnt::new(),
            });
            Ok(RefPtr::from_raw(Box::into_raw(boxed)).unwrap())
        }
    }

    /// Returns the wrapped object's owning thread.
    pub fn owning_thread(&self) -> &nsIThread {
        &self.owning_thread
    }

    /// Returns the wrapped object if called from the owning thread, or
    /// `None` if called from any other thread.
    pub fn get(&self) -> Option<&T> {
        if is_current_thread(&self.owning_thread) && !self.ptr.is_null() {
            unsafe { Some(&*self.ptr) }
        } else {
            None
        }
    }
}
