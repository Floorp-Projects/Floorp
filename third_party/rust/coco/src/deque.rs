//! A lock-free work-stealing deque.
//!
//! There is one worker and possibly multiple stealers per deque. The worker has exclusive access
//! to one side of the deque and may push and pop elements. Stealers can only steal (i.e. pop)
//! elements from the other side.
//!
//! The implementation is based on the following papers:
//!
//! 1. Dynamic Circular Work-Stealing Deque
//!    <sup>[pdf][chase-lev]</sup>
//! 2. Correct and Efficient Work-Stealing for Weak Memory Models
//!    <sup>[pdf][weak-mem]</sup>
//! 3. CDSChecker: Checking Concurrent Data Structures Written with C/C++ Atomics
//!    <sup>[pdf][checker] [code][code]</sup>
//!
//! [chase-lev]: https://pdfs.semanticscholar.org/3771/77bb82105c35e6e26ebad1698a20688473bd.pdf
//! [weak-mem]: http://www.di.ens.fr/~zappa/readings/ppopp13.pdf
//! [checker]: http://plrg.eecs.uci.edu/publications/c11modelcheck.pdf
//! [code]: https://github.com/computersforpeace/model-checker-benchmarks/tree/master/chase-lev-deque-bugfix
//!
//! # Examples
//!
//! ```
//! use coco::deque;
//!
//! let (w, s) = deque::new();
//!
//! // Create some work.
//! for i in 0..1000 {
//!     w.push(i);
//! }
//!
//! let threads = (0..4).map(|_| {
//!     let s = s.clone();
//!     std::thread::spawn(move || {
//!         while let Some(x) = s.steal() {
//!             // Do something with `x`...
//!         }
//!     })
//! }).collect::<Vec<_>>();
//!
//! while let Some(x) = w.pop() {
//!     // Do something with `x`...
//!     // Or create even more work...
//!     if x > 1 {
//!         w.push(x / 2);
//!         w.push(x / 2);
//!     }
//! }
//!
//! for t in threads {
//!     t.join().unwrap();
//! }
//! ```

use std::cmp;
use std::fmt;
use std::marker::PhantomData;
use std::mem;
use std::ptr;
use std::sync::Arc;
use std::sync::atomic::{AtomicIsize, fence};
use std::sync::atomic::Ordering::{Acquire, Release, Relaxed, SeqCst};

use epoch::{self, Atomic};

/// Minimum buffer capacity for a deque.
const MIN_CAP: usize = 16;

/// A buffer where deque elements are stored.
struct Buffer<T> {
    /// Pointer to the allocated memory.
    ptr: *mut T,
    /// Capacity of the buffer. Always a power of two.
    cap: usize,
}

impl<T> Buffer<T> {
    /// Returns a new buffe with the specified capacity.
    fn new(cap: usize) -> Self {
        let mut v = Vec::with_capacity(cap);
        let ptr = v.as_mut_ptr();
        mem::forget(v);
        Buffer {
            ptr: ptr,
            cap: cap,
        }
    }

    /// Returns a pointer to the element at the specified `index`.
    unsafe fn at(&self, index: isize) -> *mut T {
        // `self.len` is always a power of two.
        self.ptr.offset(index & (self.cap - 1) as isize)
    }

    /// Writes `value` into the specified `index`.
    unsafe fn write(&self, index: isize, value: T) {
        ptr::write(self.at(index), value)
    }

    /// Reads the value from the specified `index`.
    unsafe fn read(&self, index: isize) -> T {
        ptr::read(self.at(index))
    }
}

struct Deque<T> {
    bottom: AtomicIsize,
    top: AtomicIsize,
    buffer: Atomic<Buffer<T>>,
}

/// A work-stealing deque.
impl<T> Deque<T> {
    /// Returns a new, empty deque.
    fn new() -> Self {
        Deque {
            bottom: AtomicIsize::new(0),
            top: AtomicIsize::new(0),
            buffer: Atomic::new(Buffer::new(MIN_CAP), 0),
        }
    }

    /// Returns the number of elements in the deque.
    ///
    /// If used concurrently with other operations, the returned number is just an estimate.
    fn len(&self) -> usize {
        let b = self.bottom.load(Relaxed);
        let t = self.top.load(Relaxed);
        // The length can be negative because `b` and `t` were loaded without synchronization.
        cmp::max(b.wrapping_sub(t), 0) as usize
    }

    /// Resizes the buffer with new capacity of `new_cap`.
    #[cold]
    unsafe fn resize(&self, new_cap: usize) {
        // Load the bottom, top, and buffer.
        let b = self.bottom.load(Relaxed);
        let t = self.top.load(Relaxed);
        let buffer = self.buffer.load_raw(Relaxed).0;

        // Allocate a new buffer.
        let new = Buffer::new(new_cap);

        // Copy data from the old buffer to the new one.
        let mut i = t;
        while i != b {
            ptr::copy_nonoverlapping((*buffer).at(i), new.at(i), 1);
            i = i.wrapping_add(1);
        }

        epoch::pin(|pin| {
            // Replace the old buffer with the new one.
            self.buffer.store_box(Box::new(new), 0, pin).as_raw();

            let ptr = (*buffer).ptr;
            let cap = (*buffer).cap;

            // Destroy the old buffer later.
            epoch::defer_free(ptr, cap, pin);
            epoch::defer_free(buffer, 1, pin);

            // If the size of the buffer at least than 1KB, then flush the thread-local garbage in
            // order to destroy it sooner.
            if mem::size_of::<T>() * cap >= 1 << 10 {
                epoch::flush(pin);
            }
        })
    }

    /// Pushes an element onto the bottom of the deque.
    fn push(&self, value: T) {
        unsafe {
            // Load the bottom, top, and buffer. The buffer doesn't have to be epoch-protected
            // because the current thread (the worker) is the only one that grows and shrinks it.
            let b = self.bottom.load(Relaxed);
            let t = self.top.load(Acquire);
            let mut buffer = self.buffer.load_raw(Relaxed).0;

            // Calculate the length of the deque.
            let len = b.wrapping_sub(t);

            // Is the deque full?
            let cap = (*buffer).cap;
            if len >= cap as isize {
                // Yes. Grow the underlying buffer.
                self.resize(2 * cap);
                buffer = self.buffer.load_raw(Relaxed).0;
            }

            // Write `value` into the right slot and increment `b`.
            (*buffer).write(b, value);
            fence(Release);
            self.bottom.store(b.wrapping_add(1), Relaxed);
        }
    }

    /// Pops an element from the bottom of the deque.
    fn pop(&self) -> Option<T> {
        // Load the bottom.
        let b = self.bottom.load(Relaxed);

        // If the deque is empty, return early without incurring the cost of a SeqCst fence.
        let t = self.top.load(Relaxed);
        if b.wrapping_sub(t) <= 0 {
            return None;
        }

        // Decrement the bottom.
        let b = b.wrapping_sub(1);
        self.bottom.store(b, Relaxed);

        // Load the buffer. The buffer doesn't have to be epoch-protected because the current
        // thread (the worker) is the only one that grows and shrinks it.
        let buffer = self.buffer.load_raw(Relaxed).0;

        fence(SeqCst);

        // Load the top.
        let t = self.top.load(Relaxed);

        // Compute the length after the bottom was decremented.
        let len = b.wrapping_sub(t);

        if len < 0 {
            // The deque is empty. Restore the bottom back to the original value.
            self.bottom.store(b.wrapping_add(1), Relaxed);
            None
        } else {
            // Read the value to be popped.
            let mut value = unsafe { Some((*buffer).read(b)) };

            // Are we popping the last element from the deque?
            if len == 0 {
                // Try incrementing the top.
                if self.top.compare_exchange(t, t.wrapping_add(1), SeqCst, Relaxed).is_err() {
                    // Failed. We didn't pop anything.
                    mem::forget(value.take());
                }

                // Restore the bottom back to the original value.
                self.bottom.store(b.wrapping_add(1), Relaxed);
            } else {
                // Shrink the buffer if `len` is less than one fourth of `cap`.
                unsafe {
                    let cap = (*buffer).cap;
                    if cap > MIN_CAP && len < cap as isize / 4 {
                        self.resize(cap / 2);
                    }
                }
            }

            value
        }
    }

    /// Steals an element from the top of the deque.
    fn steal(&self) -> Option<T> {
        // Load the top.
        let mut t = self.top.load(Acquire);

        // A SeqCst fence is needed here.
        // If the current thread is already pinned (reentrantly), we must manually issue the fence.
        // Otherwise, the following pinning will issue the fence anyway, so we don't have to.
        if epoch::is_pinned() {
            fence(SeqCst);
        }

        epoch::pin(|pin| {
            // Loop until we successfully steal an element or find the deque empty.
            loop {
                // Load the bottom.
                let b = self.bottom.load(Acquire);

                // Is the deque empty?
                if b.wrapping_sub(t) <= 0 {
                    return None;
                }

                // Load the buffer and read the value at the top.
                let a = self.buffer.load(pin).unwrap();
                let value = unsafe { a.read(t) };

                // Try incrementing the top to steal the value.
                if self.top.compare_exchange(t, t.wrapping_add(1), SeqCst, Relaxed).is_ok() {
                    return Some(value);
                }

                // We didn't steal this value, forget it.
                mem::forget(value);

                // Before every iteration of the loop we must load the top, issue a SeqCst fence,
                // and then load the bottom. Now reload the top and issue the fence.
                t = self.top.load(Acquire);
                fence(SeqCst);
            }
        })
    }

    /// Steals an element from the top of the deque, but only the worker may call this method.
    fn steal_as_worker(&self) -> Option<T> {
        let b = self.bottom.load(Relaxed);
        let a = self.buffer.load_raw(Relaxed).0;

        // Loop until we successfully steal an element or find the deque empty.
        loop {
            let t = self.top.load(Relaxed);

            // Is the deque empty?
            if b.wrapping_sub(t) <= 0 {
                return None;
            }

            // Try incrementing the top to steal the value.
            if self.top.compare_exchange(t, t.wrapping_add(1), SeqCst, Relaxed).is_ok() {
                return unsafe { Some((*a).read(t)) };
            }
        }
    }
}

impl<T> Drop for Deque<T> {
    fn drop(&mut self) {
        // Load the bottom, top, and buffer.
        let b = self.bottom.load(Relaxed);
        let t = self.top.load(Relaxed);
        let buffer = self.buffer.load_raw(Relaxed).0;

        unsafe {
            // Go through the buffer from top to bottom and drop all elements in the deque.
            let mut i = t;
            while i != b {
                ptr::drop_in_place((*buffer).at(i));
                i = i.wrapping_add(1);
            }

            // Free the memory allocated by the buffer.
            drop(Vec::from_raw_parts((*buffer).ptr, 0, (*buffer).cap));
            drop(Vec::from_raw_parts(buffer, 0, 1));
        }
    }
}

/// Worker side of a work-stealing deque.
///
/// There is only one worker per deque.
pub struct Worker<T> {
    deque: Arc<Deque<T>>,
    _marker: PhantomData<*mut ()>, // !Send + !Sync
}

unsafe impl<T: Send> Send for Worker<T> {}

impl<T> Worker<T> {
    /// Returns the number of elements in the deque.
    ///
    /// If used concurrently with other operations, the returned number is just an estimate.
    ///
    /// # Examples
    ///
    /// ```
    /// use coco::deque;
    ///
    /// let (w, _) = deque::new();
    /// for i in 0..30 {
    ///     w.push(i);
    /// }
    /// assert_eq!(w.len(), 30);
    /// ```
    pub fn len(&self) -> usize {
        self.deque.len()
    }

    /// Pushes an element onto the bottom of the deque.
    ///
    /// # Examples
    ///
    /// ```
    /// use coco::deque;
    ///
    /// let (w, _) = deque::new();
    /// w.push(1);
    /// w.push(2);
    /// ```
    pub fn push(&self, value: T) {
        self.deque.push(value);
    }

    /// Pops an element from the bottom of the deque.
    ///
    /// # Examples
    ///
    /// ```
    /// use coco::deque;
    ///
    /// let (w, _) = deque::new();
    /// w.push(1);
    /// w.push(2);
    ///
    /// assert_eq!(w.pop(), Some(2));
    /// assert_eq!(w.pop(), Some(1));
    /// assert_eq!(w.pop(), None);
    /// ```
    pub fn pop(&self) -> Option<T> {
        self.deque.pop()
    }

    /// Steals an element from the top of the deque.
    ///
    /// # Examples
    ///
    /// ```
    /// use coco::deque;
    ///
    /// let (w, _) = deque::new();
    /// w.push(1);
    /// w.push(2);
    ///
    /// assert_eq!(w.steal(), Some(1));
    /// assert_eq!(w.steal(), Some(2));
    /// assert_eq!(w.steal(), None);
    /// ```
    pub fn steal(&self) -> Option<T> {
        self.deque.steal_as_worker()
    }
}

impl<T> fmt::Debug for Worker<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Worker {{ ... }}")
    }
}

/// Stealer side of a work-stealing deque.
///
/// Stealers may be cloned in order to create more stealers for the same deque.
pub struct Stealer<T> {
    deque: Arc<Deque<T>>,
    _marker: PhantomData<*mut ()>, // !Send + !Sync
}

unsafe impl<T: Send> Send for Stealer<T> {}
unsafe impl<T: Send> Sync for Stealer<T> {}

impl<T> Stealer<T> {
    /// Returns the number of elements in the deque.
    ///
    /// If used concurrently with other operations, the returned number is just an estimate.
    ///
    /// # Examples
    ///
    /// ```
    /// use coco::deque;
    ///
    /// let (w, _) = deque::new();
    /// for i in 0..30 {
    ///     w.push(i);
    /// }
    /// assert_eq!(w.len(), 30);
    /// ```
    pub fn len(&self) -> usize {
        self.deque.len()
    }

    /// Steals an element from the top of the deque.
    ///
    /// # Examples
    ///
    /// ```
    /// use coco::deque;
    ///
    /// let (w, s) = deque::new();
    /// w.push(1);
    /// w.push(2);
    ///
    /// assert_eq!(s.steal(), Some(1));
    /// assert_eq!(s.steal(), Some(2));
    /// assert_eq!(s.steal(), None);
    /// ```
    pub fn steal(&self) -> Option<T> {
        self.deque.steal()
    }
}

impl<T> Clone for Stealer<T> {
    fn clone(&self) -> Self {
        Stealer {
            deque: self.deque.clone(),
            _marker: PhantomData,
        }
    }
}

impl<T> fmt::Debug for Stealer<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Stealer {{ ... }}")
    }
}

/// Returns a new work-stealing deque.
///
/// The worker is unique, while stealers can be cloned and distributed among multiple threads.
///
/// The deque will be destructed as soon as it's worker and all it's stealers get dropped.
///
/// # Examples
///
/// ```
/// use coco::deque;
///
/// let (w, s1) = deque::new();
/// let s2 = s1.clone();
///
/// w.push('a');
/// w.push('b');
/// w.push('c');
///
/// assert_eq!(w.pop(), Some('c'));
/// assert_eq!(s1.steal(), Some('a'));
/// assert_eq!(s2.steal(), Some('b'));
/// ```
pub fn new<T>() -> (Worker<T>, Stealer<T>) {
    let d = Arc::new(Deque::new());
    let worker = Worker {
        deque: d.clone(),
        _marker: PhantomData,
    };
    let stealer = Stealer {
        deque: d,
        _marker: PhantomData,
    };
    (worker, stealer)
}

#[cfg(test)]
mod tests {
    extern crate rand;

    use std::sync::{Arc, Mutex};
    use std::sync::atomic::{AtomicBool, AtomicUsize};
    use std::sync::atomic::Ordering::SeqCst;
    use std::thread;

    use epoch;
    use self::rand::Rng;

    #[test]
    fn smoke() {
        let (w, s) = super::new();
        assert_eq!(w.pop(), None);
        assert_eq!(s.steal(), None);
        assert_eq!(w.len(), 0);
        assert_eq!(s.len(), 0);

        w.push(1);
        assert_eq!(w.len(), 1);
        assert_eq!(s.len(), 1);
        assert_eq!(w.pop(), Some(1));
        assert_eq!(w.pop(), None);
        assert_eq!(s.steal(), None);
        assert_eq!(w.len(), 0);
        assert_eq!(s.len(), 0);

        w.push(2);
        assert_eq!(s.steal(), Some(2));
        assert_eq!(s.steal(), None);
        assert_eq!(w.pop(), None);

        w.push(3);
        w.push(4);
        w.push(5);
        assert_eq!(w.steal(), Some(3));
        assert_eq!(s.steal(), Some(4));
        assert_eq!(w.steal(), Some(5));
        assert_eq!(w.steal(), None);
    }

    #[test]
    fn steal_push() {
        const STEPS: usize = 50_000;

        let (w, s) = super::new();
        let t = thread::spawn(move || {
            for i in 0..STEPS {
                loop {
                    if let Some(v) = s.steal() {
                        assert_eq!(i, v);
                        break;
                    }
                }
            }
        });

        for i in 0..STEPS {
            w.push(i);
        }
        t.join().unwrap();
    }

    #[test]
    fn stampede() {
        const COUNT: usize = 50_000;

        let (w, s) = super::new();

        for i in 0..COUNT {
            w.push(Box::new(i + 1));
        }
        let remaining = Arc::new(AtomicUsize::new(COUNT));

        let threads = (0..8).map(|_| {
            let s = s.clone();
            let remaining = remaining.clone();

            thread::spawn(move || {
                let mut last = 0;
                while remaining.load(SeqCst) > 0 {
                    if let Some(x) = s.steal() {
                        assert!(last < *x);
                        last = *x;
                        remaining.fetch_sub(1, SeqCst);
                    }
                }
            })
        }).collect::<Vec<_>>();

        let mut last = COUNT + 1;
        while remaining.load(SeqCst) > 0 {
            if let Some(x) = w.pop() {
                assert!(last > *x);
                last = *x;
                remaining.fetch_sub(1, SeqCst);
            }
        }

        for t in threads {
            t.join().unwrap();
        }
    }

    fn run_stress() {
        const COUNT: usize = 50_000;

        let (w, s) = super::new();
        let done = Arc::new(AtomicBool::new(false));
        let hits = Arc::new(AtomicUsize::new(0));

        let threads = (0..8).map(|_| {
            let s = s.clone();
            let done = done.clone();
            let hits = hits.clone();

            thread::spawn(move || {
                while !done.load(SeqCst) {
                    if let Some(_) = s.steal() {
                        hits.fetch_add(1, SeqCst);
                    }
                }
            })
        }).collect::<Vec<_>>();

        let mut rng = rand::thread_rng();
        let mut expected = 0;
        while expected < COUNT {
            if rng.gen_range(0, 3) == 0 {
                if w.pop().is_some() {
                    hits.fetch_add(1, SeqCst);
                }
            } else {
                w.push(expected);
                expected += 1;
            }
        }

        while hits.load(SeqCst) < COUNT {
            if w.pop().is_some() {
                hits.fetch_add(1, SeqCst);
            }
        }
        done.store(true, SeqCst);

        for t in threads {
            t.join().unwrap();
        }
    }

    #[test]
    fn stress() {
        run_stress();
    }

    #[test]
    fn stress_pinned() {
        epoch::pin(|_| run_stress());
    }

    #[test]
    fn no_starvation() {
        const COUNT: usize = 50_000;

        let (w, s) = super::new();
        let done = Arc::new(AtomicBool::new(false));

        let (threads, hits): (Vec<_>, Vec<_>) = (0..8).map(|_| {
            let s = s.clone();
            let done = done.clone();
            let hits = Arc::new(AtomicUsize::new(0));

            let t = {
                let hits = hits.clone();
                thread::spawn(move || {
                    while !done.load(SeqCst) {
                        if let Some(_) = s.steal() {
                            hits.fetch_add(1, SeqCst);
                        }
                    }
                })
            };

            (t, hits)
        }).unzip();

        let mut rng = rand::thread_rng();
        let mut my_hits = 0;
        loop {
            for i in 0..rng.gen_range(0, COUNT) {
                if rng.gen_range(0, 3) == 0 && my_hits == 0 {
                    if w.pop().is_some() {
                        my_hits += 1;
                    }
                } else {
                    w.push(i);
                }
            }

            if my_hits > 0 && hits.iter().all(|h| h.load(SeqCst) > 0) {
                break;
            }
        }
        done.store(true, SeqCst);

        for t in threads {
            t.join().unwrap();
        }
    }

    #[test]
    fn destructors() {
        const COUNT: usize = 50_000;

        struct Elem(usize, Arc<Mutex<Vec<usize>>>);

        impl Drop for Elem {
            fn drop(&mut self) {
                self.1.lock().unwrap().push(self.0);
            }
        }

        let (w, s) = super::new();

        let dropped = Arc::new(Mutex::new(Vec::new()));
        let remaining = Arc::new(AtomicUsize::new(COUNT));
        for i in 0..COUNT {
            w.push(Elem(i, dropped.clone()));
        }

        let threads = (0..8).map(|_| {
            let s = s.clone();
            let remaining = remaining.clone();

            thread::spawn(move || {
                for _ in 0..1000 {
                    if s.steal().is_some() {
                        remaining.fetch_sub(1, SeqCst);
                    }
                }
            })
        }).collect::<Vec<_>>();

        for _ in 0..1000 {
            if w.pop().is_some() {
                remaining.fetch_sub(1, SeqCst);
            }
        }

        for t in threads {
            t.join().unwrap();
        }

        let rem = remaining.load(SeqCst);
        assert!(rem > 0);
        assert_eq!(w.len(), rem);
        assert_eq!(s.len(), rem);

        {
            let mut v = dropped.lock().unwrap();
            assert_eq!(v.len(), COUNT - rem);
            v.clear();
        }

        drop(w);
        drop(s);

        {
            let mut v = dropped.lock().unwrap();
            assert_eq!(v.len(), rem);
            v.sort();
            for w in v.windows(2) {
                assert_eq!(w[0] + 1, w[1]);
            }
        }
    }
}
