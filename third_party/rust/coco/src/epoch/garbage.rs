//! Garbage collection.
//!
//! # Bags and garbage queues
//!
//! Objects that get removed from concurrent data structures must be stashed away until the global
//! epoch sufficiently advances so that they become safe for destruction. Pointers to such garbage
//! objects are kept in bags.
//!
//! When a bag becomes full, it is marked with the current global epoch pushed into a `Garbage`
//! queue. Usually each instance of concurrent data structure has it's own `Garbage` queue that
//! gets fully destroyed as soon as the data structure gets dropped.
//!
//! Whenever a bag is pushed into the queue, some garbage is collected and destroyed along the way.
//! Garbage collection can also be manually triggered by calling method `collect`.
//!
//! # The global garbage queue
//!
//! Some data structures don't own objects but merely transfer them between threads, e.g. queues.
//! As such, queues don't execute destructors - they only allocate and free some memory. it would
//! be costly for each queue to handle it's own `Garbage`, so there is a special global queue all
//! data structures can share.
//!
//! The global garbage queue is very efficient. Each thread has a thread-local bag that is
//! populated with garbage, and when it becomes full, it is finally pushed into queue. This design
//! reduces contention on data structures. The global queue cannot be explicitly accessed - the
//! only way to interact with it is by calling function `defer_free`.

use std::cell::UnsafeCell;
use std::cmp;
use std::fmt;
use std::mem;
use std::sync::atomic::{AtomicUsize, ATOMIC_USIZE_INIT};
use std::sync::atomic::Ordering::{AcqRel, Acquire, Relaxed, SeqCst};

use epoch::{self, Atomic, Pin, Ptr};

/// Maximum number of objects a bag can contain.
#[cfg(not(feature = "strict_gc"))]
const MAX_OBJECTS: usize = 64;
#[cfg(feature = "strict_gc")]
const MAX_OBJECTS: usize = 4;

/// The global epoch.
///
/// The last bit in this number is unused and is always zero. Every so often the global epoch is
/// incremented, i.e. we say it "advances". A pinned thread may advance the global epoch only if
/// all currently pinned threads have been pinned in the current epoch.
///
/// If an object became garbage in some epoch, then we can be sure that after two advancements no
/// thread will hold a reference to it. That is the crux of safe memory reclamation.
pub static EPOCH: AtomicUsize = ATOMIC_USIZE_INIT;

/// Holds removed objects that will be eventually destroyed.
pub struct Bag {
    /// Number of objects in the bag.
    len: AtomicUsize,
    /// Removed objects.
    objects: [UnsafeCell<(unsafe fn(*mut u8, usize), *mut u8, usize)>; MAX_OBJECTS],
    /// The global epoch at the moment when this bag got pushed into the queue.
    epoch: usize,
    /// The next bag in the queue.
    next: Atomic<Bag>,
}

impl Bag {
    /// Returns a new, empty bag.
    pub fn new() -> Self {
        Bag {
            len: AtomicUsize::new(0),
            objects: unsafe { mem::zeroed() },
            epoch: unsafe { mem::uninitialized() },
            next: Atomic::null(0),
        }
    }

    /// Returns `true` if the bag is empty.
    pub fn is_empty(&self) -> bool {
        self.len.load(Relaxed) == 0
    }

    /// Attempts to insert a garbage object into the bag and returns `true` if succeeded.
    pub fn try_insert<T>(&self, destroy: unsafe fn(*mut T, usize), object: *mut T, count: usize)
                         -> bool {
        // Erase type `*mut T` and use `*mut u8` instead.
        let destroy: unsafe fn(*mut u8, usize) = unsafe { mem::transmute(destroy) };
        let object = object as *mut u8;

        let mut len = self.len.load(Acquire);
        loop {
            // Is the bag full?
            if len == self.objects.len() {
                return false;
            }

            // Try incrementing `len`.
            match self.len.compare_exchange(len, len + 1, AcqRel, Acquire) {
                Ok(_) => {
                    // Success! Now store the garbage object into the array. The current thread
                    // will synchronize with the thread that destroys it through epoch advancement.
                    unsafe { *self.objects[len].get() = (destroy, object, count) }
                    return true;
                }
                Err(l) => len = l,
            }
        }
    }

    /// Destroys all objects in the bag.
    ///
    /// Note: can be called only once!
    unsafe fn destroy_all_objects(&self) {
        for cell in self.objects.iter().take(self.len.load(Relaxed)) {
            let (destroy, object, count) = *cell.get();
            destroy(object, count);
        }
    }
}

/// A garbage queue.
///
/// This is where a concurrent data structure can store removed objects for deferred destruction.
///
/// Stored garbage objects are first kept in the garbage buffer. When the buffer becomes full, it's
/// objects are flushed into the garbage queue. Flushing can be manually triggered by calling
/// [`flush`].
///
/// Some garbage in the queue can be manually collected by calling [`collect`].
///
/// [`flush`]: method.flush.html
/// [`collect`]: method.collect.html
pub struct Garbage {
    /// Head of the queue.
    head: Atomic<Bag>,
    /// Tail of the queue.
    tail: Atomic<Bag>,
    /// The next bag that will be pushed into the queue as soon as it gets full.
    pending: Atomic<Bag>,
}

unsafe impl Send for Garbage {}
unsafe impl Sync for Garbage {}

impl Garbage {
    /// Returns a new, empty garbage queue.
    pub fn new() -> Self {
        let garbage = Garbage {
            head: Atomic::null(0),
            tail: Atomic::null(0),
            pending: Atomic::null(0),
        };

        // This code may be executing while a thread harness is initializing, so normal pinning
        // would try to access it while it is being initialized. Such accesses fail with a panic.
        // We must therefore cheat by creating a fake pin.
        let pin = unsafe { &mem::zeroed::<Pin>() };

        // The head of the queue is always a sentinel entry.
        let sentinel = garbage.head.store_box(Box::new(Bag::new()), 0, pin);
        garbage.tail.store(sentinel);

        garbage
    }

    /// Attempts to compare-and-swap the pending bag `old` with a new, empty one.
    ///
    /// The return value is a result indicating whether the compare-and-swap successfully installed
    /// a new bag. On success the new bag is returned. On failure the current more up-to-date
    /// pending bag is returned.
    fn replace_pending<'p>(&self, old: Ptr<'p, Bag>, pin: &'p Pin)
                           -> Result<Ptr<'p, Bag>, Ptr<'p, Bag>> {
        match self.pending.cas_box(old, Box::new(Bag::new()), 0) {
            Ok(new) => {
                if !old.is_null() {
                    // Push the old bag into the queue.
                    let bag = unsafe { Box::from_raw(old.as_raw()) };
                    self.push(bag, pin);
                }

                // Spare some cycles on garbage collection.
                // Note: This may itself produce garbage and allocate new bags.
                epoch::thread::try_advance(pin);
                self.collect(pin);

                Ok(new)
            }
            Err((pending, _)) => Err(pending),
        }
    }

    /// Adds an object that will later be freed.
    ///
    /// The specified object is an array allocated at address `object` and consists of `count`
    /// elements of type `T`.
    ///
    /// This method inserts the object into the garbage buffer. When the buffers becomes full, it's
    /// objects are flushed into the garbage queue.
    pub unsafe fn defer_free<T>(&self, object: *mut T, count: usize, pin: &Pin) {
        unsafe fn free<T>(ptr: *mut T, count: usize) {
            // Free the memory, but don't run the destructors.
            drop(Vec::from_raw_parts(ptr, 0, count));
        }
        self.defer_destroy(free, object, count, pin);
    }

    /// Adds an object that will later be dropped and freed.
    ///
    /// The specified object is an array allocated at address `object` and consists of `count`
    /// elements of type `T`.
    ///
    /// This method inserts the object into the garbage buffer. When the buffers becomes full, it's
    /// objects are flushed into the garbage queue.
    ///
    /// Note: The object must be `Send + 'self`.
    pub unsafe fn defer_drop<T>(&self, object: *mut T, count: usize, pin: &Pin) {
        unsafe fn destruct<T>(ptr: *mut T, count: usize) {
            // Run the destructors and free the memory.
            drop(Vec::from_raw_parts(ptr, count, count));
        }
        self.defer_destroy(destruct, object, count, pin);
    }

    /// Adds an object that will later be destroyed using `destroy`.
    ///
    /// The specified object is an array allocated at address `object` and consists of `count`
    /// elements of type `T`.
    ///
    /// This method inserts the object into the garbage buffer. When the buffers becomes full, it's
    /// objects are flushed into the garbage queue.
    ///
    /// Note: The object must be `Send + 'self`.
    pub unsafe fn defer_destroy<T>(
        &self,
        destroy: unsafe fn(*mut T, usize),
        object: *mut T,
        count: usize,
        pin: &Pin
    ) {
        let mut pending = self.pending.load(pin);
        loop {
            match pending.as_ref() {
                Some(p) if p.try_insert(destroy, object, count) => break,
                _ => {
                    match self.replace_pending(pending, pin) {
                        Ok(p) => pending = p,
                        Err(p) => pending = p,
                    }
                }
            }
        }
    }

    /// Flushes the buffered garbage.
    ///
    /// It is wise to flush the garbage just after passing a very large object to one of the
    /// `defer_*` methods, so that it isn't sitting in the buffer for a long time.
    pub fn flush(&self, pin: &Pin) {
        let mut pending = self.pending.load(pin);
        loop {
            match pending.as_ref() {
                None => break,
                Some(p) => {
                    if p.is_empty() {
                        // The bag is already empty.
                        break;
                    } else {
                        match self.replace_pending(pending, pin) {
                            Ok(_) => break,
                            Err(p) => pending = p,
                        }
                    }
                }
            }
        }
    }

    /// Collects some garbage from the queue and destroys it.
    ///
    /// Generally speaking, it's not necessary to call this method because garbage production
    /// already triggers garbage destruction. However, if there are long periods without garbage
    /// production, it might be a good idea to call this method from time to time.
    ///
    /// This method collects several buffers worth of garbage objects.
    pub fn collect(&self, pin: &Pin) {
        /// Number of bags to destroy.
        const COLLECT_STEPS: usize = 8;

        let epoch = EPOCH.load(SeqCst);
        let condition = |bag: &Bag| {
            // A pinned thread can witness at most one epoch advancement. Therefore, any bag that
            // is within one epoch of the current one cannot be destroyed yet.
            let diff = epoch.wrapping_sub(bag.epoch);
            cmp::min(diff, 0usize.wrapping_sub(diff)) > 2
        };

        for _ in 0..COLLECT_STEPS {
            match self.try_pop_if(&condition, pin) {
                None => break,
                Some(bag) => unsafe { bag.destroy_all_objects() },
            }
        }
    }

    /// Pushes a bag into the queue.
    fn push(&self, mut bag: Box<Bag>, pin: &Pin) {
        // Mark the bag with the current epoch.
        bag.epoch = EPOCH.load(SeqCst);

        let mut tail = self.tail.load(pin);
        loop {
            let next = tail.unwrap().next.load(pin);
            if next.is_null() {
                // Try installing the new bag.
                match tail.unwrap().next.cas_box(next, bag, 0) {
                    Ok(bag) => {
                        // Tail pointer shouldn't fall behind. Let's move it forward.
                        let _ = self.tail.cas(tail, bag);
                        break;
                    }
                    Err((t, b)) => {
                        tail = t;
                        bag = b;
                    }
                }
            } else {
                // This is not the actual tail. Move the tail pointer forward.
                match self.tail.cas(tail, next) {
                    Ok(()) => tail = next,
                    Err(t) => tail = t,
                }
            }
        }
    }

    /// Attempts to pop a bag from the front of the queue and returns it if `condition` is met.
    ///
    /// If the bag in the front doesn't meet it or if the queue is empty, `None` is returned.
    fn try_pop_if<'p, F>(&self, condition: F, pin: &'p Pin) -> Option<&'p Bag>
        where F: Fn(&Bag) -> bool
    {
        let mut head = self.head.load(pin);
        loop {
            let next = head.unwrap().next.load(pin);
            match next.as_ref() {
                Some(n) if condition(n) => {
                    // Try moving the head forward.
                    match self.head.cas(head, next) {
                        Ok(()) => {
                            // The old head may be later freed.
                            unsafe { epoch::defer_free(head.as_raw(), 1, pin) }
                            // The new head holds the popped value (heads are sentinels!).
                            return Some(n);
                        }
                        Err(h) => head = h,
                    }
                }
                None | Some(_) => return None,
            }
        }
    }
}

impl Drop for Garbage {
    fn drop(&mut self) {
        unsafe {
            // Load the pending bag, then destroy it and all it's objects.
            let pending = self.pending.load_raw(Relaxed).0;
            if !pending.is_null() {
                (*pending).destroy_all_objects();
                drop(Vec::from_raw_parts(pending, 0, 1));
            }

            // Destroy all bags and objects in the queue.
            let mut head = self.head.load_raw(Relaxed).0;
            loop {
                // Load the next bag and destroy the current head.
                let next = (*head).next.load_raw(Relaxed).0;
                drop(Vec::from_raw_parts(head, 0, 1));

                // If the next node is null, we've reached the end of the queue.
                if next.is_null() {
                    break;
                }

                // Move one step forward.
                head = next;

                // Destroy all objects in this bag.
                // The bag itself will be destroyed in the next iteration of the loop.
                (*head).destroy_all_objects();
            }
        }
    }
}

impl fmt::Debug for Garbage {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Garbage {{ ... }}")
    }
}

/// Returns a reference to a global garbage, which is lazily initialized.
fn global() -> &'static Garbage {
    static GLOBAL: AtomicUsize = ATOMIC_USIZE_INIT;

    let current = GLOBAL.load(Acquire);

    let garbage = if current == 0 {
        // Initialize the singleton.
        let raw = Box::into_raw(Box::new(Garbage::new()));
        let new = raw as usize;
        let previous = GLOBAL.compare_and_swap(0, new, AcqRel);

        if previous == 0 {
            // Ok, we initialized it.
            new
        } else {
            // Another thread has already initialized it.
            unsafe { drop(Box::from_raw(raw)); }
            previous
        }
    } else {
        current
    };

    unsafe { &*(garbage as *const Garbage) }
}

/// Pushes a bag into the global garbage.
pub fn push(bag: Box<Bag>, pin: &Pin) {
    global().push(bag, pin);
}

/// Collects several bags from the global queue and destroys their objects.
pub fn collect(pin: &Pin) {
    global().collect(pin);
}

/// Destroys the global garbage.
///
/// # Safety
///
/// This function may only be called at the very end of the main thread, and only if the main
/// thread has never been pinned.
#[cfg(feature = "internals")]
pub unsafe fn destroy_global() {
    let global = global() as *const Garbage as *mut Garbage;
    drop(Box::from_raw(global));
}

#[cfg(test)]
mod tests {
    extern crate rand;

    use std::mem;
    use std::sync::atomic::{AtomicUsize, ATOMIC_USIZE_INIT};
    use std::sync::atomic::Ordering::SeqCst;
    use std::sync::Arc;
    use std::thread;

    use self::rand::{Rng, thread_rng};

    use super::Garbage;
    use ::epoch;

    #[test]
    fn smoke() {
        let g = Garbage::new();
        epoch::pin(|pin| {
            let a = Box::into_raw(Box::new(7));
            unsafe { g.defer_free(a, 1, pin) }
            assert!(!g.pending.load(pin).unwrap().is_empty());
        });
    }

    #[test]
    fn flush_pending() {
        let g = Garbage::new();
        let mut rng = thread_rng();

        for _ in 0..100_000 {
            epoch::pin(|pin| unsafe {
                let a = Box::into_raw(Box::new(7));
                g.defer_drop(a, 1, pin);

                if rng.gen_range(0, 100) == 0 {
                    g.flush(pin);
                    assert!(g.pending.load(pin).unwrap().is_empty());
                }
            });
        }
    }

    #[test]
    fn incremental() {
        const COUNT: usize = 100_000;
        static DESTROYS: AtomicUsize = ATOMIC_USIZE_INIT;

        let g = Garbage::new();

        epoch::pin(|pin| unsafe {
            for _ in 0..COUNT {
                let a = Box::into_raw(Box::new(7i32));
                unsafe fn destroy(ptr: *mut i32, count: usize) {
                    drop(Box::from_raw(ptr));
                    DESTROYS.fetch_add(count, SeqCst);
                }
                g.defer_destroy(destroy, a, 1, pin);
            }
            g.flush(pin);
        });

        let mut last = 0;

        while last < COUNT {
            let curr = DESTROYS.load(SeqCst);
            assert!(curr - last < 1000);
            last = curr;

            epoch::pin(|pin| g.collect(pin));
        }
        assert!(DESTROYS.load(SeqCst) == COUNT);
    }

    #[test]
    fn buffering() {
        const COUNT: usize = 10;
        static DESTROYS: AtomicUsize = ATOMIC_USIZE_INIT;

        let g = Garbage::new();

        epoch::pin(|pin| unsafe {
            for _ in 0..COUNT {
                let a = Box::into_raw(Box::new(7i32));
                unsafe fn destroy(ptr: *mut i32, count: usize) {
                    drop(Box::from_raw(ptr));
                    DESTROYS.fetch_add(count, SeqCst);
                }
                g.defer_destroy(destroy, a, 1, pin);
            }
        });

        for _ in 0..100_000 {
            epoch::pin(|pin| {
                g.collect(pin);
                assert!(DESTROYS.load(SeqCst) < COUNT);
            });
        }
        epoch::pin(|pin| g.flush(pin));

        while DESTROYS.load(SeqCst) < COUNT {
            epoch::pin(|pin| g.collect(pin));
        }
        assert_eq!(DESTROYS.load(SeqCst), COUNT);
    }

    #[test]
    fn count_drops() {
        const COUNT: usize = 100_000;
        static DROPS: AtomicUsize = ATOMIC_USIZE_INIT;

        struct Elem(i32);

        impl Drop for Elem {
            fn drop(&mut self) {
                DROPS.fetch_add(1, SeqCst);
            }
        }

        let g = Garbage::new();

        epoch::pin(|pin| unsafe {
            for _ in 0..COUNT {
                let a = Box::into_raw(Box::new(Elem(7i32)));
                g.defer_drop(a, 1, pin);
            }
            g.flush(pin);
        });

        while DROPS.load(SeqCst) < COUNT {
            epoch::pin(|pin| g.collect(pin));
        }
        assert_eq!(DROPS.load(SeqCst), COUNT);
    }

    #[test]
    fn count_destroy() {
        const COUNT: usize = 100_000;
        static DESTROYS: AtomicUsize = ATOMIC_USIZE_INIT;

        let g = Garbage::new();

        epoch::pin(|pin| unsafe {
            for _ in 0..COUNT {
                let a = Box::into_raw(Box::new(7i32));
                unsafe fn destroy(ptr: *mut i32, count: usize) {
                    drop(Box::from_raw(ptr));
                    DESTROYS.fetch_add(count, SeqCst);
                }
                g.defer_destroy(destroy, a, 1, pin);
            }
            g.flush(pin);
        });

        while DESTROYS.load(SeqCst) < COUNT {
            epoch::pin(|pin| g.collect(pin));
        }
        assert_eq!(DESTROYS.load(SeqCst), COUNT);
    }

    #[test]
    fn drop_array() {
        const COUNT: usize = 700;
        static DROPS: AtomicUsize = ATOMIC_USIZE_INIT;

        struct Elem(i32);

        impl Drop for Elem {
            fn drop(&mut self) {
                DROPS.fetch_add(1, SeqCst);
            }
        }

        let g = Garbage::new();

        epoch::pin(|pin| unsafe {
            let mut v = Vec::with_capacity(COUNT);
            for i in 0..COUNT {
                v.push(Elem(i as i32));
            }

            g.defer_drop(v.as_mut_ptr(), v.len(), pin);
            g.flush(pin);

            mem::forget(v);
        });

        while DROPS.load(SeqCst) < COUNT {
            epoch::pin(|pin| g.collect(pin));
        }
        assert_eq!(DROPS.load(SeqCst), COUNT);
    }

    #[test]
    fn destroy_array() {
        const COUNT: usize = 100_000;
        static DESTROYS: AtomicUsize = ATOMIC_USIZE_INIT;

        let g = Garbage::new();

        epoch::pin(|pin| unsafe {
            let mut v = Vec::with_capacity(COUNT);
            for i in 0..COUNT {
                v.push(i as i32);
            }

            unsafe fn destroy(ptr: *mut i32, count: usize) {
                assert!(count == COUNT);
                drop(Vec::from_raw_parts(ptr, count, count));
                DESTROYS.fetch_add(count, SeqCst);
            }
            g.defer_destroy(destroy, v.as_mut_ptr(), v.len(), pin);
            g.flush(pin);

            mem::forget(v);
        });

        while DESTROYS.load(SeqCst) < COUNT {
            epoch::pin(|pin| g.collect(pin));
        }
        assert_eq!(DESTROYS.load(SeqCst), COUNT);
    }

    #[test]
    fn drop_garbage() {
        const COUNT: usize = 100_000;
        static DROPS: AtomicUsize = ATOMIC_USIZE_INIT;

        struct Elem(i32);

        impl Drop for Elem {
            fn drop(&mut self) {
                DROPS.fetch_add(1, SeqCst);
            }
        }

        let g = Garbage::new();

        epoch::pin(|pin| unsafe {
            for _ in 0..COUNT {
                let a = Box::into_raw(Box::new(Elem(7i32)));
                g.defer_drop(a, 1, pin);
            }
            g.flush(pin);
        });

        drop(g);
        assert_eq!(DROPS.load(SeqCst), COUNT);
    }

    #[test]
    fn stress() {
        const THREADS: usize = 8;
        const COUNT: usize = 100_000;
        static DROPS: AtomicUsize = ATOMIC_USIZE_INIT;

        struct Elem(i32);

        impl Drop for Elem {
            fn drop(&mut self) {
                DROPS.fetch_add(1, SeqCst);
            }
        }

        let g = Arc::new(Garbage::new());

        let threads = (0..THREADS).map(|_| {
            let g = g.clone();

            thread::spawn(move || {
                for _ in 0..COUNT {
                    epoch::pin(|pin| unsafe {
                        let a = Box::into_raw(Box::new(Elem(7i32)));
                        g.defer_drop(a, 1, pin);
                    });
                }
            })
        }).collect::<Vec<_>>();

        for t in threads {
            t.join().unwrap();
        }

        drop(g);
        assert_eq!(DROPS.load(SeqCst), COUNT * THREADS);
    }
}
