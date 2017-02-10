// Copyright 2016 Amanieu d'Antras
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

#[cfg(feature = "nightly")]
use std::sync::atomic::{AtomicUsize, Ordering, fence};
#[cfg(not(feature = "nightly"))]
use stable::{AtomicUsize, Ordering, fence};
use std::ptr;
use std::mem;
use std::cell::Cell;
use spinwait::SpinWait;
use thread_parker::ThreadParker;

struct ThreadData {
    parker: ThreadParker,

    // Linked list of threads in the queue. The queue is split into two parts:
    // the processed part and the unprocessed part. When new nodes are added to
    // the list, they only have the next pointer set, and queue_tail is null.
    //
    // Nodes are processed with the queue lock held, which consists of setting
    // the prev pointer for each node and setting the queue_tail pointer on the
    // first processed node of the list.
    //
    // This setup allows nodes to be added to the queue without a lock, while
    // still allowing O(1) removal of nodes from the processed part of the list.
    // The only cost is the O(n) processing, but this only needs to be done
    // once for each node, and therefore isn't too expensive.
    queue_tail: Cell<*const ThreadData>,
    prev: Cell<*const ThreadData>,
    next: Cell<*const ThreadData>,
}

impl ThreadData {
    fn new() -> ThreadData {
        ThreadData {
            parker: ThreadParker::new(),
            queue_tail: Cell::new(ptr::null()),
            prev: Cell::new(ptr::null()),
            next: Cell::new(ptr::null()),
        }
    }
}

thread_local!(static THREAD_DATA: ThreadData = ThreadData::new());

const LOCKED_BIT: usize = 1;
const QUEUE_LOCKED_BIT: usize = 2;
const QUEUE_MASK: usize = !3;

// Word-sized lock that is used to implement the parking_lot API. Since this
// can't use parking_lot, it instead manages its own queue of waiting threads.
pub struct WordLock {
    state: AtomicUsize,
}

impl WordLock {
    #[inline]
    pub fn new() -> WordLock {
        WordLock { state: AtomicUsize::new(0) }
    }

    #[inline]
    pub unsafe fn lock(&self) {
        if self.state
            .compare_exchange_weak(0, LOCKED_BIT, Ordering::Acquire, Ordering::Relaxed)
            .is_ok() {
            return;
        }
        self.lock_slow();
    }

    #[inline]
    pub unsafe fn unlock(&self) {
        let state = self.state.fetch_sub(LOCKED_BIT, Ordering::Release);
        if state & QUEUE_LOCKED_BIT != 0 || state & QUEUE_MASK == 0 {
            return;
        }
        self.unlock_slow();
    }

    #[cold]
    #[inline(never)]
    unsafe fn lock_slow(&self) {
        let mut spinwait = SpinWait::new();
        let mut state = self.state.load(Ordering::Relaxed);
        loop {
            // Grab the lock if it isn't locked, even if there is a queue on it
            if state & LOCKED_BIT == 0 {
                match self.state
                    .compare_exchange_weak(state,
                                           state | LOCKED_BIT,
                                           Ordering::Acquire,
                                           Ordering::Relaxed) {
                    Ok(_) => return,
                    Err(x) => state = x,
                }
                continue;
            }

            // If there is no queue, try spinning a few times
            if state & QUEUE_MASK == 0 && spinwait.spin() {
                state = self.state.load(Ordering::Relaxed);
                continue;
            }

            // Get our thread data and prepare it for parking
            let thread_data = &*THREAD_DATA.with(|x| x as *const ThreadData);
            assert!(mem::align_of_val(thread_data) > !QUEUE_MASK);
            thread_data.parker.prepare_park();

            // Add our thread to the front of the queue
            let queue_head = (state & QUEUE_MASK) as *const ThreadData;
            if queue_head.is_null() {
                thread_data.queue_tail.set(thread_data);
                thread_data.prev.set(ptr::null());
            } else {
                thread_data.queue_tail.set(ptr::null());
                thread_data.prev.set(ptr::null());
                thread_data.next.set(queue_head);
            }
            if let Err(x) = self.state
                .compare_exchange_weak(state,
                                       (state & !QUEUE_MASK) | thread_data as *const _ as usize,
                                       Ordering::Release,
                                       Ordering::Relaxed) {
                state = x;
                continue;
            }

            // Sleep until we are woken up by an unlock
            thread_data.parker.park();

            // Loop back and try locking again
            spinwait.reset();
            self.state.load(Ordering::Relaxed);
        }
    }

    #[cold]
    #[inline(never)]
    unsafe fn unlock_slow(&self) {
        let mut state = self.state.load(Ordering::Relaxed);
        loop {
            // We just unlocked the WordLock. Just check if there is a thread
            // to wake up. If the queue is locked then another thread is already
            // taking care of waking up a thread.
            if state & QUEUE_LOCKED_BIT != 0 || state & QUEUE_MASK == 0 {
                return;
            }

            // Try to grab the queue lock
            match self.state.compare_exchange_weak(state,
                                                   state | QUEUE_LOCKED_BIT,
                                                   Ordering::Acquire,
                                                   Ordering::Relaxed) {
                Ok(_) => break,
                Err(x) => state = x,
            }
        }

        // Now we have the queue lock and the queue is non-empty
        'outer: loop {
            // First, we need to fill in the prev pointers for any newly added
            // threads. We do this until we reach a node that we previously
            // processed, which has a non-null queue_tail pointer.
            let queue_head = (state & QUEUE_MASK) as *const ThreadData;
            let mut queue_tail;
            let mut current = queue_head;
            loop {
                queue_tail = (*current).queue_tail.get();
                if !queue_tail.is_null() {
                    break;
                }
                let next = (*current).next.get();
                (*next).prev.set(current);
                current = next;
            }

            // Set queue_tail on the queue head to indicate that the whole list
            // has prev pointers set correctly.
            (*queue_head).queue_tail.set(queue_tail);

            // If the WordLock is locked, then there is no point waking up a
            // thread now. Instead we let the next unlocker take care of waking
            // up a thread.
            if state & LOCKED_BIT != 0 {
                match self.state.compare_exchange_weak(state,
                                                       state & !QUEUE_LOCKED_BIT,
                                                       Ordering::Release,
                                                       Ordering::Relaxed) {
                    Ok(_) => return,
                    Err(x) => state = x,
                }

                // Need an acquire fence before reading the new queue
                fence(Ordering::Acquire);
                continue;
            }

            // Remove the last thread from the queue and unlock the queue
            let new_tail = (*queue_tail).prev.get();
            if new_tail.is_null() {
                loop {
                    match self.state.compare_exchange_weak(state,
                                                           state & LOCKED_BIT,
                                                           Ordering::Release,
                                                           Ordering::Relaxed) {
                        Ok(_) => break,
                        Err(x) => state = x,
                    }

                    // If the compare_exchange failed because a new thread was
                    // added to the queue then we need to re-scan the queue to
                    // find the previous element.
                    if state & QUEUE_MASK == 0 {
                        continue;
                    } else {
                        // Need an acquire fence before reading the new queue
                        fence(Ordering::Acquire);
                        continue 'outer;
                    }
                }
            } else {
                (*queue_head).queue_tail.set(new_tail);
                self.state.fetch_and(!QUEUE_LOCKED_BIT, Ordering::Release);
            }

            // Finally, wake up the thread we removed from the queue. Note that
            // we don't need to worry about any races here since the thread is
            // guaranteed to be sleeping right now and we are the only one who
            // can wake it up.
            (*queue_tail).parker.unpark_lock().unpark();
            break;
        }
    }
}
