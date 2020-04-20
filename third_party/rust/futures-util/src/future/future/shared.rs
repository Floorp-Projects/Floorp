use crate::task::{ArcWake, waker_ref};
use futures_core::future::{FusedFuture, Future};
use futures_core::task::{Context, Poll, Waker};
use slab::Slab;
use std::cell::UnsafeCell;
use std::fmt;
use std::pin::Pin;
use std::sync::atomic::AtomicUsize;
use std::sync::atomic::Ordering::SeqCst;
use std::sync::{Arc, Mutex};

/// Future for the [`shared`](super::FutureExt::shared) method.
#[must_use = "futures do nothing unless you `.await` or poll them"]
pub struct Shared<Fut: Future> {
    inner: Option<Arc<Inner<Fut>>>,
    waker_key: usize,
}

struct Inner<Fut: Future> {
    future_or_output: UnsafeCell<FutureOrOutput<Fut>>,
    notifier: Arc<Notifier>,
}

struct Notifier {
    state: AtomicUsize,
    wakers: Mutex<Option<Slab<Option<Waker>>>>,
}

// The future itself is polled behind the `Arc`, so it won't be moved
// when `Shared` is moved.
impl<Fut: Future> Unpin for Shared<Fut> {}

impl<Fut: Future> fmt::Debug for Shared<Fut> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Shared")
            .field("inner", &self.inner)
            .field("waker_key", &self.waker_key)
            .finish()
    }
}

impl<Fut: Future> fmt::Debug for Inner<Fut> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Inner").finish()
    }
}

enum FutureOrOutput<Fut: Future> {
    Future(Fut),
    Output(Fut::Output),
}

unsafe impl<Fut> Send for Inner<Fut>
where
    Fut: Future + Send,
    Fut::Output: Send + Sync,
{}

unsafe impl<Fut> Sync for Inner<Fut>
where
    Fut: Future + Send,
    Fut::Output: Send + Sync,
{}

const IDLE: usize = 0;
const POLLING: usize = 1;
const REPOLL: usize = 2;
const COMPLETE: usize = 3;
const POISONED: usize = 4;

const NULL_WAKER_KEY: usize = usize::max_value();

impl<Fut: Future> Shared<Fut> {
    pub(super) fn new(future: Fut) -> Shared<Fut> {
        let inner = Inner {
            future_or_output: UnsafeCell::new(FutureOrOutput::Future(future)),
            notifier: Arc::new(Notifier {
                state: AtomicUsize::new(IDLE),
                wakers: Mutex::new(Some(Slab::new())),
            }),
        };

        Shared {
            inner: Some(Arc::new(inner)),
            waker_key: NULL_WAKER_KEY,
        }
    }
}

impl<Fut> Shared<Fut>
where
    Fut: Future,
    Fut::Output: Clone,
{
    /// Returns [`Some`] containing a reference to this [`Shared`]'s output if
    /// it has already been computed by a clone or [`None`] if it hasn't been
    /// computed yet or this [`Shared`] already returned its output from
    /// [`poll`](Future::poll).
    pub fn peek(&self) -> Option<&Fut::Output> {
        if let Some(inner) = self.inner.as_ref() {
            match inner.notifier.state.load(SeqCst) {
                COMPLETE => unsafe { return Some(inner.output()) },
                POISONED => panic!("inner future panicked during poll"),
                _ => {}
            }
        }
        None
    }

    /// Registers the current task to receive a wakeup when `Inner` is awoken.
    fn set_waker(&mut self, cx: &mut Context<'_>) {
        // Acquire the lock first before checking COMPLETE to ensure there
        // isn't a race.
        let mut wakers_guard = if let Some(inner) = self.inner.as_ref() {
            inner.notifier.wakers.lock().unwrap()
        } else {
            return;
        };

        let wakers = if let Some(wakers) = wakers_guard.as_mut() {
            wakers
        } else {
            return;
        };

        if self.waker_key == NULL_WAKER_KEY {
            self.waker_key = wakers.insert(Some(cx.waker().clone()));
        } else {
            let waker_slot = &mut wakers[self.waker_key];
            let needs_replacement = if let Some(_old_waker) = waker_slot {
                // If there's still an unwoken waker in the slot, only replace
                // if the current one wouldn't wake the same task.
                // TODO: This API is currently not available, so replace always
                // !waker.will_wake_nonlocal(old_waker)
                true
            } else {
                true
            };
            if needs_replacement {
                *waker_slot = Some(cx.waker().clone());
            }
        }
        debug_assert!(self.waker_key != NULL_WAKER_KEY);
    }

    /// Safety: callers must first ensure that `self.inner.state`
    /// is `COMPLETE`
    unsafe fn take_or_clone_output(&mut self) -> Fut::Output {
        let inner = self.inner.take().unwrap();

        match Arc::try_unwrap(inner) {
            Ok(inner) => match inner.future_or_output.into_inner() {
                FutureOrOutput::Output(item) => item,
                FutureOrOutput::Future(_) => unreachable!(),
            },
            Err(inner) => inner.output().clone(),
        }
    }
}

impl<Fut> Inner<Fut>
where
    Fut: Future,
    Fut::Output: Clone,
{
    /// Safety: callers must first ensure that `self.inner.state`
    /// is `COMPLETE`
    unsafe fn output(&self) -> &Fut::Output {
        match &*self.future_or_output.get() {
            FutureOrOutput::Output(ref item) => &item,
            FutureOrOutput::Future(_) => unreachable!(),
        }
    }
}

impl<Fut> FusedFuture for Shared<Fut>
where
    Fut: Future,
    Fut::Output: Clone,
{
    fn is_terminated(&self) -> bool {
        self.inner.is_none()
    }
}

impl<Fut> Future for Shared<Fut>
where
    Fut: Future,
    Fut::Output: Clone,
{
    type Output = Fut::Output;

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        let this = &mut *self;

        this.set_waker(cx);

        let inner = if let Some(inner) = this.inner.as_ref() {
            inner
        } else {
            panic!("Shared future polled again after completion");
        };

        match inner.notifier.state.compare_and_swap(IDLE, POLLING, SeqCst) {
            IDLE => {
                // Lock acquired, fall through
            }
            POLLING | REPOLL => {
                // Another task is currently polling, at this point we just want
                // to ensure that the waker for this task is registered

                return Poll::Pending;
            }
            COMPLETE => {
                // Safety: We're in the COMPLETE state
                return unsafe { Poll::Ready(this.take_or_clone_output()) };
            }
            POISONED => panic!("inner future panicked during poll"),
            _ => unreachable!(),
        }

        let waker = waker_ref(&inner.notifier);
        let mut cx = Context::from_waker(&waker);

        struct Reset<'a>(&'a AtomicUsize);

        impl Drop for Reset<'_> {
            fn drop(&mut self) {
                use std::thread;

                if thread::panicking() {
                    self.0.store(POISONED, SeqCst);
                }
            }
        }

        let _reset = Reset(&inner.notifier.state);

        let output = loop {
            let future = unsafe {
                match &mut *inner.future_or_output.get() {
                    FutureOrOutput::Future(fut) => Pin::new_unchecked(fut),
                    _ => unreachable!(),
                }
            };

            let poll = future.poll(&mut cx);

            match poll {
                Poll::Pending => {
                    let state = &inner.notifier.state;
                    match state.compare_and_swap(POLLING, IDLE, SeqCst) {
                        POLLING => {
                            // Success
                            return Poll::Pending;
                        }
                        REPOLL => {
                            // Was woken since: Gotta poll again!
                            let prev = state.swap(POLLING, SeqCst);
                            assert_eq!(prev, REPOLL);
                        }
                        _ => unreachable!(),
                    }
                }
                Poll::Ready(output) => break output,
            }
        };

        unsafe {
            *inner.future_or_output.get() =
                FutureOrOutput::Output(output);
        }

        inner.notifier.state.store(COMPLETE, SeqCst);

        // Wake all tasks and drop the slab
        let mut wakers_guard = inner.notifier.wakers.lock().unwrap();
        let wakers = &mut wakers_guard.take().unwrap();
        for (_key, opt_waker) in wakers {
            if let Some(waker) = opt_waker.take() {
                waker.wake();
            }
        }

        drop(_reset); // Make borrow checker happy
        drop(wakers_guard);

        // Safety: We're in the COMPLETE state
        unsafe { Poll::Ready(this.take_or_clone_output()) }
    }
}

impl<Fut> Clone for Shared<Fut>
where
    Fut: Future,
{
    fn clone(&self) -> Self {
        Shared {
            inner: self.inner.clone(),
            waker_key: NULL_WAKER_KEY,
        }
    }
}

impl<Fut> Drop for Shared<Fut>
where
    Fut: Future,
{
    fn drop(&mut self) {
        if self.waker_key != NULL_WAKER_KEY {
            if let Some(ref inner) = self.inner {
                if let Ok(mut wakers) = inner.notifier.wakers.lock() {
                    if let Some(wakers) = wakers.as_mut() {
                        wakers.remove(self.waker_key);
                    }
                }
            }
        }
    }
}

impl ArcWake for Notifier {
    fn wake_by_ref(arc_self: &Arc<Self>) {
        arc_self.state.compare_and_swap(POLLING, REPOLL, SeqCst);

        let wakers = &mut *arc_self.wakers.lock().unwrap();
        if let Some(wakers) = wakers.as_mut() {
            for (_key, opt_waker) in wakers {
                if let Some(waker) = opt_waker.take() {
                    waker.wake();
                }
            }
        }
    }
}
