/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module wraps XPCOM threading functions with Rust functions
//! to make it safer and more convenient to call the XPCOM functions.
//! It also provides the Task trait and TaskRunnable struct,
//! which make it easier to dispatch tasks to threads.

mod dispatcher;
pub use dispatcher::{dispatch_background_task, dispatch_local, dispatch_onto, RunnableBuilder};
mod event_loop;
mod executor;
pub use executor::{
    spawn, spawn_blocking, spawn_local, spawn_onto, spawn_onto_blocking, AsyncTask, TaskBuilder,
};

// Expose functions intended to be used only in gtest via this module.
// We don't use a feature gate here to stop the need to compile all crates that
// depend upon `moz_task` twice.
pub mod gtest_only {
    pub use crate::event_loop::spin_event_loop_until;
}

use nserror::nsresult;
use nsstring::{nsACString, nsCString};
use std::{ffi::CStr, marker::PhantomData, mem, ptr};
use xpcom::{
    getter_addrefs,
    interfaces::{nsIEventTarget, nsIRunnable, nsISerialEventTarget, nsISupports, nsIThread},
    AtomicRefcnt, RefCounted, RefPtr, XpCom,
};

extern "C" {
    fn NS_GetCurrentThreadRust(result: *mut *const nsIThread) -> nsresult;
    fn NS_GetMainThreadRust(result: *mut *const nsIThread) -> nsresult;
    fn NS_IsMainThread() -> bool;
    fn NS_NewNamedThreadWithDefaultStackSize(
        name: *const nsACString,
        result: *mut *const nsIThread,
        event: *const nsIRunnable,
    ) -> nsresult;
    fn NS_IsOnCurrentThread(target: *const nsIEventTarget) -> bool;
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
    fn NS_DispatchBackgroundTask(event: *const nsIRunnable, flags: u32) -> nsresult;
}

pub fn get_current_thread() -> Result<RefPtr<nsIThread>, nsresult> {
    getter_addrefs(|p| unsafe { NS_GetCurrentThreadRust(p) })
}

pub fn get_main_thread() -> Result<RefPtr<nsIThread>, nsresult> {
    getter_addrefs(|p| unsafe { NS_GetMainThreadRust(p) })
}

pub fn is_main_thread() -> bool {
    unsafe { NS_IsMainThread() }
}

pub fn create_thread(name: &str) -> Result<RefPtr<nsIThread>, nsresult> {
    getter_addrefs(|p| unsafe {
        NS_NewNamedThreadWithDefaultStackSize(&*nsCString::from(name), p, ptr::null())
    })
}

pub fn is_on_current_thread(target: &nsIEventTarget) -> bool {
    unsafe { NS_IsOnCurrentThread(target) }
}

/// Creates a queue that runs tasks on the background thread pool. The tasks
/// will run in the order they're dispatched, one after the other.
pub fn create_background_task_queue(
    name: &'static CStr,
) -> Result<RefPtr<nsISerialEventTarget>, nsresult> {
    getter_addrefs(|p| unsafe { NS_CreateBackgroundTaskQueue(name.as_ptr(), p) })
}

/// Dispatches a one-shot runnable to an event target, like a thread or a
/// task queue, with the given options.
///
/// This function leaks the runnable if dispatch fails.
///
/// # Safety
///
/// As there is no guarantee that the runnable is actually `Send + Sync`, we
/// can't know that it's safe to dispatch an `nsIRunnable` to any
/// `nsIEventTarget`.
pub unsafe fn dispatch_runnable(
    runnable: &nsIRunnable,
    target: &nsIEventTarget,
    options: DispatchOptions,
) -> Result<(), nsresult> {
    // NOTE: DispatchFromScript performs an AddRef on `runnable` which is
    // why this function leaks on failure.
    target
        .DispatchFromScript(runnable, options.flags())
        .to_result()
}

/// Dispatches a one-shot task runnable to the background thread pool with the
/// given options. The task may run concurrently with other background tasks.
/// If you need tasks to run in a specific order, please create a background
/// task queue using `create_background_task_queue`, and dispatch tasks to it
/// instead.
///
/// This function leaks the runnable if dispatch fails. This avoids a race where
/// a runnable can be destroyed on either the original or target thread, which
/// is important if the runnable holds thread-unsafe members.
///
/// ### Safety
///
/// As there is no guarantee that the runnable is actually `Send + Sync`, we
/// can't know that it's safe to dispatch an `nsIRunnable` to any
/// `nsIEventTarget`.
pub unsafe fn dispatch_background_task_runnable(
    runnable: &nsIRunnable,
    options: DispatchOptions,
) -> Result<(), nsresult> {
    // This eventually calls the non-`already_AddRefed<nsIRunnable>` overload of
    // `nsIEventTarget::Dispatch` (see xpcom/threads/nsIEventTarget.idl#20-25),
    // which adds an owning reference and leaks if dispatch fails.
    NS_DispatchBackgroundTask(runnable, options.flags()).to_result()
}

/// Options to control how task runnables are dispatched.
///
/// NOTE: The `DISPATCH_SYNC` flag is intentionally not supported by this type.
#[derive(Copy, Clone, Debug, Eq, Hash, PartialEq)]
pub struct DispatchOptions(u32);

impl Default for DispatchOptions {
    #[inline]
    fn default() -> Self {
        DispatchOptions(nsIEventTarget::DISPATCH_NORMAL)
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
        const FLAG: u32 = nsIEventTarget::DISPATCH_EVENT_MAY_BLOCK;
        if may_block {
            DispatchOptions(self.flags() | FLAG)
        } else {
            DispatchOptions(self.flags() & !FLAG)
        }
    }

    /// Specifies that the dispatch is occurring from a running event that was
    /// dispatched to the same event target, and that event is about to finish.
    ///
    /// A thread pool can use this as an optimization hint to not spin up
    /// another thread, since the current thread is about to become idle.
    ///
    /// Setting this flag is unsafe, as it may only be used from the target
    /// event target when the event is about to finish.
    #[inline]
    pub unsafe fn at_end(self, may_block: bool) -> DispatchOptions {
        const FLAG: u32 = nsIEventTarget::DISPATCH_AT_END;
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
///
/// # Alternatives
///
/// This trait is no longer necessary for basic tasks to be dispatched to
/// another thread with a callback on the originating thread. `moz_task` now has
/// a series of more rust-like primitives which can be used instead. For
/// example, it may be preferable to use the async executor over `Task`:
///
/// ```ignore
/// // Spawn a task onto the background task pool, and capture the result of its
/// // execution.
/// let bg_task = moz_task::spawn("Example", async move {
///     do_background_work(captured_state)
/// });
///
/// // Spawn another task on the calling thread which will await on the result
/// // of the async operation, and invoke a non-Send callback. This task won't
/// // be awaited on, so needs to be `detach`-ed.
/// moz_task::spawn_local("Example", async move {
///     callback.completed(bg_task.await);
/// })
/// .detach();
/// ```
///
/// If no result is needed, the task returned from `spawn` may be also detached
/// directly.
pub trait Task {
    // FIXME: These could accept `&mut`.
    fn run(&self);
    fn done(&self) -> Result<(), nsresult>;
}

pub struct TaskRunnable {
    name: &'static str,
    task: Box<dyn Task + Send + Sync>,
}

impl TaskRunnable {
    // XXX: Fixme: clean up this old API. (bug 1744312)
    pub fn new(
        name: &'static str,
        task: Box<dyn Task + Send + Sync>,
    ) -> Result<TaskRunnable, nsresult> {
        Ok(TaskRunnable { name, task })
    }

    pub fn dispatch(self, target: &nsIEventTarget) -> Result<(), nsresult> {
        self.dispatch_with_options(target, DispatchOptions::default())
    }

    pub fn dispatch_with_options(
        self,
        target: &nsIEventTarget,
        options: DispatchOptions,
    ) -> Result<(), nsresult> {
        // Perform `task.run()` on a background thread.
        let task = self.task;
        let handle = TaskBuilder::new(self.name, async move {
            task.run();
            task
        })
        .options(options)
        .spawn_onto(target);

        // Run `task.done()` on the starting thread once the background thread
        // is done with the task.
        spawn_local(self.name, async move {
            let task = handle.await;
            let _ = task.done();
        })
        .detach();
        Ok(())
    }

    pub fn dispatch_background_task_with_options(
        self,
        options: DispatchOptions,
    ) -> Result<(), nsresult> {
        // Perform `task.run()` on a background thread.
        let task = self.task;
        let handle = TaskBuilder::new(self.name, async move {
            task.run();
            task
        })
        .options(options)
        .spawn();

        // Run `task.done()` on the starting thread once the background thread
        // is done with the task.
        spawn_local(self.name, async move {
            let task = handle.await;
            let _ = task.done();
        })
        .detach();
        Ok(())
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
                if is_on_current_thread(&self.owning_thread) {
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
            mem::drop(Box::from_raw(self as *const Self as *mut Self));
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
        if is_on_current_thread(&self.owning_thread) && !self.ptr.is_null() {
            unsafe { Some(&*self.ptr) }
        } else {
            None
        }
    }
}
