use core::fmt;
use core::future::Future;
use core::marker::{PhantomData, Unpin};
use core::mem;
use core::pin::Pin;
use core::ptr::NonNull;
use core::sync::atomic::Ordering;
use core::task::{Context, Poll};

use crate::header::Header;
use crate::state::*;

/// A spawned task.
///
/// A [`Task`] can be awaited to retrieve the output of its future.
///
/// Dropping a [`Task`] cancels it, which means its future won't be polled again. To drop the
/// [`Task`] handle without canceling it, use [`detach()`][`Task::detach()`] instead. To cancel a
/// task gracefully and wait until it is fully destroyed, use the [`cancel()`][Task::cancel()]
/// method.
///
/// Note that canceling a task actually wakes it and reschedules one last time. Then, the executor
/// can destroy the task by simply dropping its [`Runnable`][`super::Runnable`] or by invoking
/// [`run()`][`super::Runnable::run()`].
///
/// # Examples
///
/// ```
/// use smol::{future, Executor};
/// use std::thread;
///
/// let ex = Executor::new();
///
/// // Spawn a future onto the executor.
/// let task = ex.spawn(async {
///     println!("Hello from a task!");
///     1 + 2
/// });
///
/// // Run an executor thread.
/// thread::spawn(move || future::block_on(ex.run(future::pending::<()>())));
///
/// // Wait for the task's output.
/// assert_eq!(future::block_on(task), 3);
/// ```
#[must_use = "tasks get canceled when dropped, use `.detach()` to run them in the background"]
pub struct Task<T> {
    /// A raw task pointer.
    pub(crate) ptr: NonNull<()>,

    /// A marker capturing generic type `T`.
    pub(crate) _marker: PhantomData<T>,
}

unsafe impl<T: Send> Send for Task<T> {}
unsafe impl<T> Sync for Task<T> {}

impl<T> Unpin for Task<T> {}

#[cfg(feature = "std")]
impl<T> std::panic::UnwindSafe for Task<T> {}
#[cfg(feature = "std")]
impl<T> std::panic::RefUnwindSafe for Task<T> {}

impl<T> Task<T> {
    /// Detaches the task to let it keep running in the background.
    ///
    /// # Examples
    ///
    /// ```
    /// use smol::{Executor, Timer};
    /// use std::time::Duration;
    ///
    /// let ex = Executor::new();
    ///
    /// // Spawn a deamon future.
    /// ex.spawn(async {
    ///     loop {
    ///         println!("I'm a daemon task looping forever.");
    ///         Timer::after(Duration::from_secs(1)).await;
    ///     }
    /// })
    /// .detach();
    /// ```
    pub fn detach(self) {
        let mut this = self;
        let _out = this.set_detached();
        mem::forget(this);
    }

    /// Cancels the task and waits for it to stop running.
    ///
    /// Returns the task's output if it was completed just before it got canceled, or [`None`] if
    /// it didn't complete.
    ///
    /// While it's possible to simply drop the [`Task`] to cancel it, this is a cleaner way of
    /// canceling because it also waits for the task to stop running.
    ///
    /// # Examples
    ///
    /// ```
    /// use smol::{future, Executor, Timer};
    /// use std::thread;
    /// use std::time::Duration;
    ///
    /// let ex = Executor::new();
    ///
    /// // Spawn a deamon future.
    /// let task = ex.spawn(async {
    ///     loop {
    ///         println!("Even though I'm in an infinite loop, you can still cancel me!");
    ///         Timer::after(Duration::from_secs(1)).await;
    ///     }
    /// });
    ///
    /// // Run an executor thread.
    /// thread::spawn(move || future::block_on(ex.run(future::pending::<()>())));
    ///
    /// future::block_on(async {
    ///     Timer::after(Duration::from_secs(3)).await;
    ///     task.cancel().await;
    /// });
    /// ```
    pub async fn cancel(self) -> Option<T> {
        let mut this = self;
        this.set_canceled();

        struct Fut<T>(Task<T>);

        impl<T> Future for Fut<T> {
            type Output = Option<T>;

            fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
                self.0.poll_task(cx)
            }
        }

        Fut(this).await
    }

    /// Puts the task in canceled state.
    fn set_canceled(&mut self) {
        let ptr = self.ptr.as_ptr();
        let header = ptr as *const Header;

        unsafe {
            let mut state = (*header).state.load(Ordering::Acquire);

            loop {
                // If the task has been completed or closed, it can't be canceled.
                if state & (COMPLETED | CLOSED) != 0 {
                    break;
                }

                // If the task is not scheduled nor running, we'll need to schedule it.
                let new = if state & (SCHEDULED | RUNNING) == 0 {
                    (state | SCHEDULED | CLOSED) + REFERENCE
                } else {
                    state | CLOSED
                };

                // Mark the task as closed.
                match (*header).state.compare_exchange_weak(
                    state,
                    new,
                    Ordering::AcqRel,
                    Ordering::Acquire,
                ) {
                    Ok(_) => {
                        // If the task is not scheduled nor running, schedule it one more time so
                        // that its future gets dropped by the executor.
                        if state & (SCHEDULED | RUNNING) == 0 {
                            ((*header).vtable.schedule)(ptr);
                        }

                        // Notify the awaiter that the task has been closed.
                        if state & AWAITER != 0 {
                            (*header).notify(None);
                        }

                        break;
                    }
                    Err(s) => state = s,
                }
            }
        }
    }

    /// Puts the task in detached state.
    fn set_detached(&mut self) -> Option<T> {
        let ptr = self.ptr.as_ptr();
        let header = ptr as *const Header;

        unsafe {
            // A place where the output will be stored in case it needs to be dropped.
            let mut output = None;

            // Optimistically assume the `Task` is being detached just after creating the task.
            // This is a common case so if the `Task` is datached, the overhead of it is only one
            // compare-exchange operation.
            if let Err(mut state) = (*header).state.compare_exchange_weak(
                SCHEDULED | TASK | REFERENCE,
                SCHEDULED | REFERENCE,
                Ordering::AcqRel,
                Ordering::Acquire,
            ) {
                loop {
                    // If the task has been completed but not yet closed, that means its output
                    // must be dropped.
                    if state & COMPLETED != 0 && state & CLOSED == 0 {
                        // Mark the task as closed in order to grab its output.
                        match (*header).state.compare_exchange_weak(
                            state,
                            state | CLOSED,
                            Ordering::AcqRel,
                            Ordering::Acquire,
                        ) {
                            Ok(_) => {
                                // Read the output.
                                output =
                                    Some((((*header).vtable.get_output)(ptr) as *mut T).read());

                                // Update the state variable because we're continuing the loop.
                                state |= CLOSED;
                            }
                            Err(s) => state = s,
                        }
                    } else {
                        // If this is the last reference to the task and it's not closed, then
                        // close it and schedule one more time so that its future gets dropped by
                        // the executor.
                        let new = if state & (!(REFERENCE - 1) | CLOSED) == 0 {
                            SCHEDULED | CLOSED | REFERENCE
                        } else {
                            state & !TASK
                        };

                        // Unset the `TASK` flag.
                        match (*header).state.compare_exchange_weak(
                            state,
                            new,
                            Ordering::AcqRel,
                            Ordering::Acquire,
                        ) {
                            Ok(_) => {
                                // If this is the last reference to the task, we need to either
                                // schedule dropping its future or destroy it.
                                if state & !(REFERENCE - 1) == 0 {
                                    if state & CLOSED == 0 {
                                        ((*header).vtable.schedule)(ptr);
                                    } else {
                                        ((*header).vtable.destroy)(ptr);
                                    }
                                }

                                break;
                            }
                            Err(s) => state = s,
                        }
                    }
                }
            }

            output
        }
    }

    /// Polls the task to retrieve its output.
    ///
    /// Returns `Some` if the task has completed or `None` if it was closed.
    ///
    /// A task becomes closed in the following cases:
    ///
    /// 1. It gets canceled by `Runnable::drop()`, `Task::drop()`, or `Task::cancel()`.
    /// 2. Its output gets awaited by the `Task`.
    /// 3. It panics while polling the future.
    /// 4. It is completed and the `Task` gets dropped.
    fn poll_task(&mut self, cx: &mut Context<'_>) -> Poll<Option<T>> {
        let ptr = self.ptr.as_ptr();
        let header = ptr as *const Header;

        unsafe {
            let mut state = (*header).state.load(Ordering::Acquire);

            loop {
                // If the task has been closed, notify the awaiter and return `None`.
                if state & CLOSED != 0 {
                    // If the task is scheduled or running, we need to wait until its future is
                    // dropped.
                    if state & (SCHEDULED | RUNNING) != 0 {
                        // Replace the waker with one associated with the current task.
                        (*header).register(cx.waker());

                        // Reload the state after registering. It is possible changes occurred just
                        // before registration so we need to check for that.
                        state = (*header).state.load(Ordering::Acquire);

                        // If the task is still scheduled or running, we need to wait because its
                        // future is not dropped yet.
                        if state & (SCHEDULED | RUNNING) != 0 {
                            return Poll::Pending;
                        }
                    }

                    // Even though the awaiter is most likely the current task, it could also be
                    // another task.
                    (*header).notify(Some(cx.waker()));
                    return Poll::Ready(None);
                }

                // If the task is not completed, register the current task.
                if state & COMPLETED == 0 {
                    // Replace the waker with one associated with the current task.
                    (*header).register(cx.waker());

                    // Reload the state after registering. It is possible that the task became
                    // completed or closed just before registration so we need to check for that.
                    state = (*header).state.load(Ordering::Acquire);

                    // If the task has been closed, restart.
                    if state & CLOSED != 0 {
                        continue;
                    }

                    // If the task is still not completed, we're blocked on it.
                    if state & COMPLETED == 0 {
                        return Poll::Pending;
                    }
                }

                // Since the task is now completed, mark it as closed in order to grab its output.
                match (*header).state.compare_exchange(
                    state,
                    state | CLOSED,
                    Ordering::AcqRel,
                    Ordering::Acquire,
                ) {
                    Ok(_) => {
                        // Notify the awaiter. Even though the awaiter is most likely the current
                        // task, it could also be another task.
                        if state & AWAITER != 0 {
                            (*header).notify(Some(cx.waker()));
                        }

                        // Take the output from the task.
                        let output = ((*header).vtable.get_output)(ptr) as *mut T;
                        return Poll::Ready(Some(output.read()));
                    }
                    Err(s) => state = s,
                }
            }
        }
    }
}

impl<T> Drop for Task<T> {
    fn drop(&mut self) {
        self.set_canceled();
        self.set_detached();
    }
}

impl<T> Future for Task<T> {
    type Output = T;

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        match self.poll_task(cx) {
            Poll::Ready(t) => Poll::Ready(t.expect("task has failed")),
            Poll::Pending => Poll::Pending,
        }
    }
}

impl<T> fmt::Debug for Task<T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let ptr = self.ptr.as_ptr();
        let header = ptr as *const Header;

        f.debug_struct("Task")
            .field("header", unsafe { &(*header) })
            .finish()
    }
}
