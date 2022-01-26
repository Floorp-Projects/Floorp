/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{get_current_thread, DispatchOptions, RunnableBuilder};
use std::{
    cell::Cell,
    fmt::Debug,
    future::Future,
    pin::Pin,
    ptr,
    sync::Arc,
    task::{Context, Poll},
};
use xpcom::interfaces::{nsIEventTarget, nsIRunnablePriority};
use xpcom::RefPtr;

/// A spawned task.
///
/// A [`AsyncTask`] can be awaited to retrieve the output of its future.
///
/// Dropping an [`AsyncTask`] cancels it, which means its future won't be polled
/// again. To drop the [`AsyncTask`] handle without canceling it, use
/// [`detach()`][`AsyncTask::detach()`] instead. To cancel a task gracefully and
/// wait until it is fully destroyed, use the [`cancel()`][AsyncTask::cancel()]
/// method.
///
/// A task which is cancelled due to the nsIEventTarget it was dispatched to no
/// longer accepting events will never be resolved.
#[derive(Debug)]
#[must_use = "tasks get canceled when dropped, use `.detach()` to run them in the background"]
pub struct AsyncTask<T> {
    task: async_task::FallibleTask<T>,
}

impl<T> AsyncTask<T> {
    fn new(task: async_task::Task<T>) -> Self {
        AsyncTask {
            task: task.fallible(),
        }
    }

    /// Detaches the task to let it keep running in the background.
    pub fn detach(self) {
        self.task.detach()
    }

    /// Cancels the task and waits for it to stop running.
    ///
    /// Returns the task's output if it was completed just before it got canceled, or [`None`] if
    /// it didn't complete.
    ///
    /// While it's possible to simply drop the [`Task`] to cancel it, this is a cleaner way of
    /// canceling because it also waits for the task to stop running.
    pub async fn cancel(self) -> Option<T> {
        self.task.cancel().await
    }
}

impl<T> Future for AsyncTask<T> {
    type Output = T;

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        // Wrap the future produced by `AsyncTask` to never resolve if the
        // Runnable was dropped, and the task was cancelled.
        match Pin::new(&mut self.task).poll(cx) {
            Poll::Ready(Some(t)) => Poll::Ready(t),
            Poll::Ready(None) | Poll::Pending => Poll::Pending,
        }
    }
}

enum SpawnTarget {
    BackgroundTask,
    EventTarget(RefPtr<nsIEventTarget>),
}

// SAFETY: All XPCOM interfaces are considered !Send + !Sync, however all
// well-behaved nsIEventTarget instances must be threadsafe.
unsafe impl Send for SpawnTarget {}
unsafe impl Sync for SpawnTarget {}

/// Information used by tasks as they are spawned. Stored in an Arc such that
/// their identity can be used for `POLLING_TASK`.
struct TaskSpawnConfig {
    name: &'static str,
    priority: u32,
    options: DispatchOptions,
    target: SpawnTarget,
}

thread_local! {
    /// Raw pointer to the TaskSpawnConfig for the currently polling task. Used
    /// to detect scheduling callbacks for a runnable while it is polled, to set
    /// `DISPATCH_AT_END` on the notification.
    static POLLING_TASK: Cell<*const TaskSpawnConfig> = Cell::new(ptr::null());
}

fn schedule(config: Arc<TaskSpawnConfig>, runnable: async_task::Runnable) {
    // If we're dispatching this task while it is currently running on the same
    // thread, set the `DISPATCH_AT_END` flag in the dispatch options to tell
    // our threadpool target to not bother to spin up another thread.
    let currently_polling = POLLING_TASK.with(|t| t.get() == Arc::as_ptr(&config));

    // SAFETY: We use the POLLING_TASK thread local to check if we meet the
    // requirements for `at_end`.
    let options = unsafe { config.options.at_end(currently_polling) };

    // Build the RunnableBuilder for our task to be dispatched.
    let config2 = config.clone();
    let builder = RunnableBuilder::new(config.name, move || {
        // Record the pointer for the currently executing task in the
        // POLLING_TASK thread-local so that nested dispatches can detect it.
        POLLING_TASK.with(|t| {
            let prev = t.get();
            t.set(Arc::as_ptr(&config2));
            runnable.run();
            t.set(prev);
        });
    })
    .priority(config.priority)
    .options(options);

    let rv = match &config.target {
        SpawnTarget::BackgroundTask => builder.dispatch_background_task(),
        SpawnTarget::EventTarget(target) => builder.dispatch(&*target),
    };
    if let Err(err) = rv {
        log::warn!(
            "dispatch for spawned task '{}' failed: {:?}",
            config.name,
            err
        );
    }
}

/// Helper for starting an async task which will run a future to completion.
#[derive(Debug)]
pub struct TaskBuilder<F> {
    name: &'static str,
    future: F,
    priority: u32,
    options: DispatchOptions,
}

impl<F> TaskBuilder<F> {
    pub fn new(name: &'static str, future: F) -> TaskBuilder<F> {
        TaskBuilder {
            name,
            future,
            priority: nsIRunnablePriority::PRIORITY_NORMAL,
            options: DispatchOptions::default(),
        }
    }

    /// Specify the priority of the task's runnables.
    pub fn priority(mut self, priority: u32) -> Self {
        self.priority = priority;
        self
    }

    /// Specify options to use when dispatching the task.
    pub fn options(mut self, options: DispatchOptions) -> Self {
        self.options = options;
        self
    }

    /// Set whether or not the event may block, and should be run on the IO
    /// thread pool.
    pub fn may_block(mut self, may_block: bool) -> Self {
        self.options = self.options.may_block(may_block);
        self
    }
}

impl<F> TaskBuilder<F>
where
    F: Future + Send + 'static,
    F::Output: Send + 'static,
{
    /// Run the future on the background task pool.
    pub fn spawn(self) -> AsyncTask<F::Output> {
        let config = Arc::new(TaskSpawnConfig {
            name: self.name,
            priority: self.priority,
            options: self.options,
            target: SpawnTarget::BackgroundTask,
        });
        let (runnable, task) = async_task::spawn(self.future, move |runnable| {
            schedule(config.clone(), runnable)
        });
        runnable.schedule();
        AsyncTask::new(task)
    }

    /// Run the future on the specified nsIEventTarget.
    pub fn spawn_onto(self, target: &nsIEventTarget) -> AsyncTask<F::Output> {
        let config = Arc::new(TaskSpawnConfig {
            name: self.name,
            priority: self.priority,
            options: self.options,
            target: SpawnTarget::EventTarget(RefPtr::new(target)),
        });
        let (runnable, task) = async_task::spawn(self.future, move |runnable| {
            schedule(config.clone(), runnable)
        });
        runnable.schedule();
        AsyncTask::new(task)
    }
}

impl<F> TaskBuilder<F>
where
    F: Future + 'static,
    F::Output: 'static,
{
    /// Run the future on the current thread.
    ///
    /// Unlike the other `spawn` methods, this method supports non-Send futures.
    ///
    /// # Panics
    ///
    /// This method may panic if run on a thread which cannot run local futures
    /// (e.g. due to it is not being an XPCOM thread, or if we are very late
    /// during shutdown).
    pub fn spawn_local(self) -> AsyncTask<F::Output> {
        let current_thread = get_current_thread().expect("cannot get current thread");
        let config = Arc::new(TaskSpawnConfig {
            name: self.name,
            priority: self.priority,
            options: self.options,
            target: SpawnTarget::EventTarget(RefPtr::new(current_thread.coerce())),
        });
        let (runnable, task) = async_task::spawn_local(self.future, move |runnable| {
            schedule(config.clone(), runnable)
        });
        runnable.schedule();
        AsyncTask::new(task)
    }
}

/// Spawn a future onto the background task pool. The future will not be run on
/// the main thread.
pub fn spawn<F>(name: &'static str, future: F) -> AsyncTask<F::Output>
where
    F: Future + Send + 'static,
    F::Output: Send + 'static,
{
    TaskBuilder::new(name, future).spawn()
}

/// Spawn a potentially-blocking future onto the background task pool. The
/// future will not be run on the main thread.
pub fn spawn_blocking<F>(name: &'static str, future: F) -> AsyncTask<F::Output>
where
    F: Future + Send + 'static,
    F::Output: Send + 'static,
{
    TaskBuilder::new(name, future).may_block(true).spawn()
}

/// Spawn a local future onto the current thread.
pub fn spawn_local<F>(name: &'static str, future: F) -> AsyncTask<F::Output>
where
    F: Future + 'static,
    F::Output: 'static,
{
    TaskBuilder::new(name, future).spawn_local()
}

pub fn spawn_onto<F>(name: &'static str, target: &nsIEventTarget, future: F) -> AsyncTask<F::Output>
where
    F: Future + Send + 'static,
    F::Output: Send + 'static,
{
    TaskBuilder::new(name, future).spawn_onto(target)
}

pub fn spawn_onto_blocking<F>(
    name: &'static str,
    target: &nsIEventTarget,
    future: F,
) -> AsyncTask<F::Output>
where
    F: Future + Send + 'static,
    F::Output: Send + 'static,
{
    TaskBuilder::new(name, future)
        .may_block(true)
        .spawn_onto(target)
}
