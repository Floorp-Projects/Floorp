/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::dispatch;
use futures_task::{waker, ArcWake};
use nserror::{nsresult, NS_OK};
use std::{
    future::Future,
    pin::Pin,
    sync::{
        atomic::{AtomicUsize, Ordering::SeqCst},
        Arc, Mutex,
    },
    task::{Context, Poll},
};
use xpcom::{interfaces::nsIEventTarget, xpcom, xpcom_method, RefPtr, ThreadBoundRefPtr};

#[derive(xpcom)]
#[xpimplements(nsIRunnable)]
#[refcnt = "atomic"]
struct InitLocalTask<F: Future<Output = ()> + 'static> {
    future: Mutex<F>,
    event_target: RefPtr<nsIEventTarget>,
    state: TaskState,
}

impl<T> InitLocalTask<T>
where
    T: Future<Output = ()> + 'static,
{
    fn new(future: T, event_target: RefPtr<nsIEventTarget>) -> Self {
        InitLocalTask {
            future: Mutex::new(future),
            event_target,
            state: TaskState::default(),
        }
    }
}

impl<T> LocalTask<T>
where
    T: Future<Output = ()> + 'static,
{
    /// Runs a closure from the context of the task.
    ///
    /// Any wake notifications resulting from the execution of the closure are
    /// tracked.
    fn enter<F, R>(&self, f: F) -> R
    where
        F: FnOnce(&mut Context<'_>) -> R,
    {
        let task = ThreadBoundRefPtr::new(RefPtr::new(self));
        let wake_handle = Arc::new(LocalWakeHandle { task });
        let waker = waker(wake_handle);
        let mut cx = Context::from_waker(&waker);

        f(&mut cx)
    }

    xpcom_method!(run => Run());
    fn run(&self) -> Result<(), nsresult> {
        // # Safety
        //
        // Mutex ensures that future is polled serially.
        self.enter(|cx| {
            // The only way to have this `LocalTask` dispatched to the named
            // event target is for it to be dispatched by the Waker, which will
            // put the state into POLL before dispatching the runnable.
            assert!(self.state.is(POLL));
            loop {
                // # Safety
                //
                // LocalTask is a heap allocation due to being an XPCOM object,
                // so `fut` is effectively `Box`ed.
                //
                // Also the value is never moved value out of its owning `Mutex`.
                let mut lock = self.future.lock().expect("Failed to lock future");
                let fut = unsafe { Pin::new_unchecked(&mut *lock) };
                let res = fut.poll(cx);
                match res {
                    Poll::Pending => {}
                    Poll::Ready(()) => return unsafe { self.state.complete() },
                }
                if unsafe { !self.state.wait() } {
                    break;
                }
            }
        });
        Ok(())
    }

    fn wake_up(&self) {
        if self.state.wake_up() {
            unsafe { dispatch(self.coerce(), &self.event_target) }.unwrap()
        }
    }
}

// Task State Machine - This was heavily cribbed from futures-executor::ThreadPool
struct TaskState {
    state: AtomicUsize,
}

// There are four possible task states, listed below with their possible
// transitions:

// The task is blocked, waiting on an event
const IDLE: usize = 0; // --> POLL

// The task is actively being polled by a thread; arrival of additional events
// of interest should move it to the REPOLL state
const POLL: usize = 1; // --> IDLE, REPOLL, or COMPLETE

// The task is actively being polled, but will need to be re-polled upon
// completion to ensure that all events were observed.
const REPOLL: usize = 2; // --> POLL

// The task has finished executing (either successfully or with an error/panic)
const COMPLETE: usize = 3; // No transitions out

impl Default for TaskState {
    fn default() -> Self {
        Self {
            state: AtomicUsize::new(IDLE),
        }
    }
}

impl TaskState {
    fn is(&self, state: usize) -> bool {
        self.state.load(SeqCst) == state
    }

    /// Attempt to "wake up" the task and poll the future.
    ///
    /// A `true` result indicates that the `POLL` state has been entered, and
    /// the caller can proceed to poll the future.  A `false` result indicates
    /// that polling is not necessary (because the task is finished or the
    /// polling has been delegated).
    fn wake_up(&self) -> bool {
        let mut state = self.state.load(SeqCst);
        loop {
            match state {
                // The task is idle, so try to run it immediately.
                IDLE => match self.state.compare_exchange(IDLE, POLL, SeqCst, SeqCst) {
                    Ok(_) => {
                        return true;
                    }
                    Err(cur) => state = cur,
                },

                // The task is being polled, so we need to record that it should
                // be *repolled* when complete.
                POLL => match self.state.compare_exchange(POLL, REPOLL, SeqCst, SeqCst) {
                    Ok(_) => return false,
                    Err(cur) => state = cur,
                },

                // The task is already scheduled for polling, or is complete, so
                // we've got nothing to do.
                _ => return false,
            }
        }
    }

    /// Alert the Task that polling completed with `Pending`.
    ///
    /// Returns true if a `REPOLL` is pending.
    ///
    /// # Safety
    ///
    /// Callable only from the `POLL`/`REPOLL` states, i.e. between
    /// successful calls to `notify` and `wait`/`complete`.
    unsafe fn wait(&self) -> bool {
        debug_assert!(matches!(self.state.load(SeqCst), POLL | REPOLL));
        match self.state.compare_exchange(POLL, IDLE, SeqCst, SeqCst) {
            // no wakeups came in while we were running
            Ok(_) => false,

            // guaranteed to be in REPOLL state; just clobber the
            // state and run again.
            Err(state) => {
                assert_eq!(state, REPOLL);
                self.state.store(POLL, SeqCst);
                true
            }
        }
    }

    /// Alert the Task that it has completed execution and should not be
    /// notified again.
    ///
    /// # Safety
    ///
    /// Callable only from the `POLL`/`REPOLL` states, i.e. between
    /// successful calls to `wake_up` and `wait`/`complete`.
    unsafe fn complete(&self) {
        debug_assert!(matches!(self.state.load(SeqCst), POLL | REPOLL));
        self.state.store(COMPLETE, SeqCst);
    }
}

struct LocalWakeHandle<F: Future<Output = ()> + 'static> {
    task: ThreadBoundRefPtr<LocalTask<F>>,
}

impl<F> ArcWake for LocalWakeHandle<F>
where
    F: Future<Output = ()> + 'static,
{
    fn wake_by_ref(arc_self: &Arc<Self>) {
        if let Some(task) = arc_self.task.get_ref() {
            task.wake_up();
        } else {
            panic!("Attempting to wake task from the wrong thread!");
        }
    }
}

/// # Safety
///
/// There is no guarantee that `current_thread` is acutally the current thread.
pub unsafe fn local_task<T>(future: T, current_thread: &nsIEventTarget)
where
    T: Future<Output = ()> + 'static,
{
    let task = LocalTask::allocate(InitLocalTask::new(future, RefPtr::new(current_thread)));
    task.wake_up();
}
