// Copyright 2016 Amanieu d'Antras
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use crate::thread_parker::{ThreadParker, ThreadParkerT, UnparkHandleT};
use crate::util::UncheckedOptionExt;
use crate::word_lock::WordLock;
use core::{
    cell::{Cell, UnsafeCell},
    ptr,
    sync::atomic::{AtomicPtr, AtomicUsize, Ordering},
};
use smallvec::SmallVec;
use std::time::{Duration, Instant};

static NUM_THREADS: AtomicUsize = AtomicUsize::new(0);
static HASHTABLE: AtomicPtr<HashTable> = AtomicPtr::new(ptr::null_mut());

// Even with 3x more buckets than threads, the memory overhead per thread is
// still only a few hundred bytes per thread.
const LOAD_FACTOR: usize = 3;

struct HashTable {
    // Hash buckets for the table
    entries: Box<[Bucket]>,

    // Number of bits used for the hash function
    hash_bits: u32,

    // Previous table. This is only kept to keep leak detectors happy.
    _prev: *const HashTable,
}

impl HashTable {
    #[inline]
    fn new(num_threads: usize, prev: *const HashTable) -> Box<HashTable> {
        let new_size = (num_threads * LOAD_FACTOR).next_power_of_two();
        let hash_bits = 0usize.leading_zeros() - new_size.leading_zeros() - 1;

        let now = Instant::now();
        let mut entries = Vec::with_capacity(new_size);
        for i in 0..new_size {
            // We must ensure the seed is not zero
            entries.push(Bucket::new(now, i as u32 + 1));
        }

        Box::new(HashTable {
            entries: entries.into_boxed_slice(),
            hash_bits,
            _prev: prev,
        })
    }
}

#[repr(align(64))]
struct Bucket {
    // Lock protecting the queue
    mutex: WordLock,

    // Linked list of threads waiting on this bucket
    queue_head: Cell<*const ThreadData>,
    queue_tail: Cell<*const ThreadData>,

    // Next time at which point be_fair should be set
    fair_timeout: UnsafeCell<FairTimeout>,
}

impl Bucket {
    #[inline]
    pub fn new(timeout: Instant, seed: u32) -> Self {
        Self {
            mutex: WordLock::INIT,
            queue_head: Cell::new(ptr::null()),
            queue_tail: Cell::new(ptr::null()),
            fair_timeout: UnsafeCell::new(FairTimeout::new(timeout, seed)),
        }
    }
}

struct FairTimeout {
    // Next time at which point be_fair should be set
    timeout: Instant,

    // the PRNG state for calculating the next timeout
    seed: u32,
}

impl FairTimeout {
    #[inline]
    fn new(timeout: Instant, seed: u32) -> FairTimeout {
        FairTimeout { timeout, seed }
    }

    // Determine whether we should force a fair unlock, and update the timeout
    #[inline]
    fn should_timeout(&mut self) -> bool {
        let now = Instant::now();
        if now > self.timeout {
            // Time between 0 and 1ms.
            let nanos = self.gen_u32() % 1_000_000;
            self.timeout = now + Duration::new(0, nanos);
            true
        } else {
            false
        }
    }

    // Pseudorandom number generator from the "Xorshift RNGs" paper by George Marsaglia.
    fn gen_u32(&mut self) -> u32 {
        self.seed ^= self.seed << 13;
        self.seed ^= self.seed >> 17;
        self.seed ^= self.seed << 5;
        self.seed
    }
}

struct ThreadData {
    parker: ThreadParker,

    // Key that this thread is sleeping on. This may change if the thread is
    // requeued to a different key.
    key: AtomicUsize,

    // Linked list of parked threads in a bucket
    next_in_queue: Cell<*const ThreadData>,

    // UnparkToken passed to this thread when it is unparked
    unpark_token: Cell<UnparkToken>,

    // ParkToken value set by the thread when it was parked
    park_token: Cell<ParkToken>,

    // Is the thread parked with a timeout?
    parked_with_timeout: Cell<bool>,

    // Extra data for deadlock detection
    #[cfg(feature = "deadlock_detection")]
    deadlock_data: deadlock::DeadlockData,
}

impl ThreadData {
    fn new() -> ThreadData {
        // Keep track of the total number of live ThreadData objects and resize
        // the hash table accordingly.
        let num_threads = NUM_THREADS.fetch_add(1, Ordering::Relaxed) + 1;
        unsafe {
            grow_hashtable(num_threads);
        }

        ThreadData {
            parker: ThreadParker::new(),
            key: AtomicUsize::new(0),
            next_in_queue: Cell::new(ptr::null()),
            unpark_token: Cell::new(DEFAULT_UNPARK_TOKEN),
            park_token: Cell::new(DEFAULT_PARK_TOKEN),
            parked_with_timeout: Cell::new(false),
            #[cfg(feature = "deadlock_detection")]
            deadlock_data: deadlock::DeadlockData::new(),
        }
    }
}

// Invokes the given closure with a reference to the current thread `ThreadData`.
#[inline(always)]
fn with_thread_data<T>(f: impl FnOnce(&ThreadData) -> T) -> T {
    // Unlike word_lock::ThreadData, parking_lot::ThreadData is always expensive
    // to construct. Try to use a thread-local version if possible. Otherwise just
    // create a ThreadData on the stack
    let mut thread_data_storage = None;
    thread_local!(static THREAD_DATA: ThreadData = ThreadData::new());
    let thread_data_ptr = THREAD_DATA
        .try_with(|x| x as *const ThreadData)
        .unwrap_or_else(|_| thread_data_storage.get_or_insert_with(ThreadData::new));

    f(unsafe { &*thread_data_ptr })
}

impl Drop for ThreadData {
    fn drop(&mut self) {
        NUM_THREADS.fetch_sub(1, Ordering::Relaxed);
    }
}

// Get a pointer to the latest hash table, creating one if it doesn't exist yet.
#[inline]
fn get_hashtable() -> *mut HashTable {
    let table = HASHTABLE.load(Ordering::Acquire);

    // If there is no table, create one
    if table.is_null() {
        create_hashtable()
    } else {
        table
    }
}

// Get a pointer to the latest hash table, creating one if it doesn't exist yet.
#[cold]
fn create_hashtable() -> *mut HashTable {
    let new_table = Box::into_raw(HashTable::new(LOAD_FACTOR, ptr::null()));

    // If this fails then it means some other thread created the hash
    // table first.
    match HASHTABLE.compare_exchange(
        ptr::null_mut(),
        new_table,
        Ordering::Release,
        Ordering::Relaxed,
    ) {
        Ok(_) => new_table,
        Err(old_table) => {
            // Free the table we created
            unsafe {
                Box::from_raw(new_table);
            }
            old_table
        }
    }
}

// Grow the hash table so that it is big enough for the given number of threads.
// This isn't performance-critical since it is only done when a ThreadData is
// created, which only happens once per thread.
unsafe fn grow_hashtable(num_threads: usize) {
    // If there is no table, create one
    if HASHTABLE.load(Ordering::Relaxed).is_null() {
        let new_table = Box::into_raw(HashTable::new(num_threads, ptr::null()));

        // If this fails then it means some other thread created the hash
        // table first.
        if HASHTABLE
            .compare_exchange(
                ptr::null_mut(),
                new_table,
                Ordering::Release,
                Ordering::Relaxed,
            )
            .is_ok()
        {
            return;
        }

        // Free the table we created
        Box::from_raw(new_table);
    }

    let mut old_table;
    loop {
        old_table = HASHTABLE.load(Ordering::Acquire);

        // Check if we need to resize the existing table
        if (*old_table).entries.len() >= LOAD_FACTOR * num_threads {
            return;
        }

        // Lock all buckets in the old table
        for b in &(*old_table).entries[..] {
            b.mutex.lock();
        }

        // Now check if our table is still the latest one. Another thread could
        // have grown the hash table between us reading HASHTABLE and locking
        // the buckets.
        if HASHTABLE.load(Ordering::Relaxed) == old_table {
            break;
        }

        // Unlock buckets and try again
        for b in &(*old_table).entries[..] {
            b.mutex.unlock();
        }
    }

    // Create the new table
    let new_table = HashTable::new(num_threads, old_table);

    // Move the entries from the old table to the new one
    for b in &(*old_table).entries[..] {
        let mut current = b.queue_head.get();
        while !current.is_null() {
            let next = (*current).next_in_queue.get();
            let hash = hash((*current).key.load(Ordering::Relaxed), new_table.hash_bits);
            if new_table.entries[hash].queue_tail.get().is_null() {
                new_table.entries[hash].queue_head.set(current);
            } else {
                (*new_table.entries[hash].queue_tail.get())
                    .next_in_queue
                    .set(current);
            }
            new_table.entries[hash].queue_tail.set(current);
            (*current).next_in_queue.set(ptr::null());
            current = next;
        }
    }

    // Publish the new table. No races are possible at this point because
    // any other thread trying to grow the hash table is blocked on the bucket
    // locks in the old table.
    HASHTABLE.store(Box::into_raw(new_table), Ordering::Release);

    // Unlock all buckets in the old table
    for b in &(*old_table).entries[..] {
        b.mutex.unlock();
    }
}

// Hash function for addresses
#[cfg(target_pointer_width = "32")]
#[inline]
fn hash(key: usize, bits: u32) -> usize {
    key.wrapping_mul(0x9E3779B9) >> (32 - bits)
}
#[cfg(target_pointer_width = "64")]
#[inline]
fn hash(key: usize, bits: u32) -> usize {
    key.wrapping_mul(0x9E3779B97F4A7C15) >> (64 - bits)
}

// Lock the bucket for the given key
#[inline]
unsafe fn lock_bucket<'a>(key: usize) -> &'a Bucket {
    let mut bucket;
    loop {
        let hashtable = get_hashtable();

        let hash = hash(key, (*hashtable).hash_bits);
        bucket = &(*hashtable).entries[hash];

        // Lock the bucket
        bucket.mutex.lock();

        // If no other thread has rehashed the table before we grabbed the lock
        // then we are good to go! The lock we grabbed prevents any rehashes.
        if HASHTABLE.load(Ordering::Relaxed) == hashtable {
            return bucket;
        }

        // Unlock the bucket and try again
        bucket.mutex.unlock();
    }
}

// Lock the bucket for the given key, but check that the key hasn't been changed
// in the meantime due to a requeue.
#[inline]
unsafe fn lock_bucket_checked<'a>(key: &AtomicUsize) -> (usize, &'a Bucket) {
    let mut bucket;
    loop {
        let hashtable = get_hashtable();
        let current_key = key.load(Ordering::Relaxed);

        let hash = hash(current_key, (*hashtable).hash_bits);
        bucket = &(*hashtable).entries[hash];

        // Lock the bucket
        bucket.mutex.lock();

        // Check that both the hash table and key are correct while the bucket
        // is locked. Note that the key can't change once we locked the proper
        // bucket for it, so we just keep trying until we have the correct key.
        if HASHTABLE.load(Ordering::Relaxed) == hashtable
            && key.load(Ordering::Relaxed) == current_key
        {
            return (current_key, bucket);
        }

        // Unlock the bucket and try again
        bucket.mutex.unlock();
    }
}

// Lock the two buckets for the given pair of keys
#[inline]
unsafe fn lock_bucket_pair<'a>(key1: usize, key2: usize) -> (&'a Bucket, &'a Bucket) {
    let mut bucket1;
    loop {
        let hashtable = get_hashtable();

        // Get the lowest bucket first
        let hash1 = hash(key1, (*hashtable).hash_bits);
        let hash2 = hash(key2, (*hashtable).hash_bits);
        if hash1 <= hash2 {
            bucket1 = &(*hashtable).entries[hash1];
        } else {
            bucket1 = &(*hashtable).entries[hash2];
        }

        // Lock the first bucket
        bucket1.mutex.lock();

        // If no other thread has rehashed the table before we grabbed the lock
        // then we are good to go! The lock we grabbed prevents any rehashes.
        if HASHTABLE.load(Ordering::Relaxed) == hashtable {
            // Now lock the second bucket and return the two buckets
            if hash1 == hash2 {
                return (bucket1, bucket1);
            } else if hash1 < hash2 {
                let bucket2 = &(*hashtable).entries[hash2];
                bucket2.mutex.lock();
                return (bucket1, bucket2);
            } else {
                let bucket2 = &(*hashtable).entries[hash1];
                bucket2.mutex.lock();
                return (bucket2, bucket1);
            }
        }

        // Unlock the bucket and try again
        bucket1.mutex.unlock();
    }
}

// Unlock a pair of buckets
#[inline]
unsafe fn unlock_bucket_pair(bucket1: &Bucket, bucket2: &Bucket) {
    bucket1.mutex.unlock();
    if !ptr::eq(bucket1, bucket2) {
        bucket2.mutex.unlock();
    }
}

/// Result of a park operation.
#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub enum ParkResult {
    /// We were unparked by another thread with the given token.
    Unparked(UnparkToken),

    /// The validation callback returned false.
    Invalid,

    /// The timeout expired.
    TimedOut,
}

impl ParkResult {
    /// Returns true if we were unparked by another thread.
    #[inline]
    pub fn is_unparked(self) -> bool {
        if let ParkResult::Unparked(_) = self {
            true
        } else {
            false
        }
    }
}

/// Result of an unpark operation.
#[derive(Copy, Clone, Default, Eq, PartialEq, Debug)]
pub struct UnparkResult {
    /// The number of threads that were unparked.
    pub unparked_threads: usize,

    /// The number of threads that were requeued.
    pub requeued_threads: usize,

    /// Whether there are any threads remaining in the queue. This only returns
    /// true if a thread was unparked.
    pub have_more_threads: bool,

    /// This is set to true on average once every 0.5ms for any given key. It
    /// should be used to switch to a fair unlocking mechanism for a particular
    /// unlock.
    pub be_fair: bool,

    /// Private field so new fields can be added without breakage.
    _sealed: (),
}

/// Operation that `unpark_requeue` should perform.
#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub enum RequeueOp {
    /// Abort the operation without doing anything.
    Abort,

    /// Unpark one thread and requeue the rest onto the target queue.
    UnparkOneRequeueRest,

    /// Requeue all threads onto the target queue.
    RequeueAll,

    /// Unpark one thread and leave the rest parked. No requeuing is done.
    UnparkOne,

    /// Requeue one thread and leave the rest parked on the original queue.
    RequeueOne,
}

/// Operation that `unpark_filter` should perform for each thread.
#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub enum FilterOp {
    /// Unpark the thread and continue scanning the list of parked threads.
    Unpark,

    /// Don't unpark the thread and continue scanning the list of parked threads.
    Skip,

    /// Don't unpark the thread and stop scanning the list of parked threads.
    Stop,
}

/// A value which is passed from an unparker to a parked thread.
#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub struct UnparkToken(pub usize);

/// A value associated with a parked thread which can be used by `unpark_filter`.
#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub struct ParkToken(pub usize);

/// A default unpark token to use.
pub const DEFAULT_UNPARK_TOKEN: UnparkToken = UnparkToken(0);

/// A default park token to use.
pub const DEFAULT_PARK_TOKEN: ParkToken = ParkToken(0);

/// Parks the current thread in the queue associated with the given key.
///
/// The `validate` function is called while the queue is locked and can abort
/// the operation by returning false. If `validate` returns true then the
/// current thread is appended to the queue and the queue is unlocked.
///
/// The `before_sleep` function is called after the queue is unlocked but before
/// the thread is put to sleep. The thread will then sleep until it is unparked
/// or the given timeout is reached.
///
/// The `timed_out` function is also called while the queue is locked, but only
/// if the timeout was reached. It is passed the key of the queue it was in when
/// it timed out, which may be different from the original key if
/// `unpark_requeue` was called. It is also passed a bool which indicates
/// whether it was the last thread in the queue.
///
/// # Safety
///
/// You should only call this function with an address that you control, since
/// you could otherwise interfere with the operation of other synchronization
/// primitives.
///
/// The `validate` and `timed_out` functions are called while the queue is
/// locked and must not panic or call into any function in `parking_lot`.
///
/// The `before_sleep` function is called outside the queue lock and is allowed
/// to call `unpark_one`, `unpark_all`, `unpark_requeue` or `unpark_filter`, but
/// it is not allowed to call `park` or panic.
#[inline]
pub unsafe fn park(
    key: usize,
    validate: impl FnOnce() -> bool,
    before_sleep: impl FnOnce(),
    timed_out: impl FnOnce(usize, bool),
    park_token: ParkToken,
    timeout: Option<Instant>,
) -> ParkResult {
    // Grab our thread data, this also ensures that the hash table exists
    with_thread_data(|thread_data| {
        // Lock the bucket for the given key
        let bucket = lock_bucket(key);

        // If the validation function fails, just return
        if !validate() {
            bucket.mutex.unlock();
            return ParkResult::Invalid;
        }

        // Append our thread data to the queue and unlock the bucket
        thread_data.parked_with_timeout.set(timeout.is_some());
        thread_data.next_in_queue.set(ptr::null());
        thread_data.key.store(key, Ordering::Relaxed);
        thread_data.park_token.set(park_token);
        thread_data.parker.prepare_park();
        if !bucket.queue_head.get().is_null() {
            (*bucket.queue_tail.get()).next_in_queue.set(thread_data);
        } else {
            bucket.queue_head.set(thread_data);
        }
        bucket.queue_tail.set(thread_data);
        bucket.mutex.unlock();

        // Invoke the pre-sleep callback
        before_sleep();

        // Park our thread and determine whether we were woken up by an unpark
        // or by our timeout. Note that this isn't precise: we can still be
        // unparked since we are still in the queue.
        let unparked = match timeout {
            Some(timeout) => thread_data.parker.park_until(timeout),
            None => {
                thread_data.parker.park();
                // call deadlock detection on_unpark hook
                deadlock::on_unpark(thread_data);
                true
            }
        };

        // If we were unparked, return now
        if unparked {
            return ParkResult::Unparked(thread_data.unpark_token.get());
        }

        // Lock our bucket again. Note that the hashtable may have been rehashed in
        // the meantime. Our key may also have changed if we were requeued.
        let (key, bucket) = lock_bucket_checked(&thread_data.key);

        // Now we need to check again if we were unparked or timed out. Unlike the
        // last check this is precise because we hold the bucket lock.
        if !thread_data.parker.timed_out() {
            bucket.mutex.unlock();
            return ParkResult::Unparked(thread_data.unpark_token.get());
        }

        // We timed out, so we now need to remove our thread from the queue
        let mut link = &bucket.queue_head;
        let mut current = bucket.queue_head.get();
        let mut previous = ptr::null();
        let mut was_last_thread = true;
        while !current.is_null() {
            if current == thread_data {
                let next = (*current).next_in_queue.get();
                link.set(next);
                if bucket.queue_tail.get() == current {
                    bucket.queue_tail.set(previous);
                } else {
                    // Scan the rest of the queue to see if there are any other
                    // entries with the given key.
                    let mut scan = next;
                    while !scan.is_null() {
                        if (*scan).key.load(Ordering::Relaxed) == key {
                            was_last_thread = false;
                            break;
                        }
                        scan = (*scan).next_in_queue.get();
                    }
                }

                // Callback to indicate that we timed out, and whether we were the
                // last thread on the queue.
                timed_out(key, was_last_thread);
                break;
            } else {
                if (*current).key.load(Ordering::Relaxed) == key {
                    was_last_thread = false;
                }
                link = &(*current).next_in_queue;
                previous = current;
                current = link.get();
            }
        }

        // There should be no way for our thread to have been removed from the queue
        // if we timed out.
        debug_assert!(!current.is_null());

        // Unlock the bucket, we are done
        bucket.mutex.unlock();
        ParkResult::TimedOut
    })
}

/// Unparks one thread from the queue associated with the given key.
///
/// The `callback` function is called while the queue is locked and before the
/// target thread is woken up. The `UnparkResult` argument to the function
/// indicates whether a thread was found in the queue and whether this was the
/// last thread in the queue. This value is also returned by `unpark_one`.
///
/// The `callback` function should return an `UnparkToken` value which will be
/// passed to the thread that is unparked. If no thread is unparked then the
/// returned value is ignored.
///
/// # Safety
///
/// You should only call this function with an address that you control, since
/// you could otherwise interfere with the operation of other synchronization
/// primitives.
///
/// The `callback` function is called while the queue is locked and must not
/// panic or call into any function in `parking_lot`.
#[inline]
pub unsafe fn unpark_one(
    key: usize,
    callback: impl FnOnce(UnparkResult) -> UnparkToken,
) -> UnparkResult {
    // Lock the bucket for the given key
    let bucket = lock_bucket(key);

    // Find a thread with a matching key and remove it from the queue
    let mut link = &bucket.queue_head;
    let mut current = bucket.queue_head.get();
    let mut previous = ptr::null();
    let mut result = UnparkResult::default();
    while !current.is_null() {
        if (*current).key.load(Ordering::Relaxed) == key {
            // Remove the thread from the queue
            let next = (*current).next_in_queue.get();
            link.set(next);
            if bucket.queue_tail.get() == current {
                bucket.queue_tail.set(previous);
            } else {
                // Scan the rest of the queue to see if there are any other
                // entries with the given key.
                let mut scan = next;
                while !scan.is_null() {
                    if (*scan).key.load(Ordering::Relaxed) == key {
                        result.have_more_threads = true;
                        break;
                    }
                    scan = (*scan).next_in_queue.get();
                }
            }

            // Invoke the callback before waking up the thread
            result.unparked_threads = 1;
            result.be_fair = (*bucket.fair_timeout.get()).should_timeout();
            let token = callback(result);

            // Set the token for the target thread
            (*current).unpark_token.set(token);

            // This is a bit tricky: we first lock the ThreadParker to prevent
            // the thread from exiting and freeing its ThreadData if its wait
            // times out. Then we unlock the queue since we don't want to keep
            // the queue locked while we perform a system call. Finally we wake
            // up the parked thread.
            let handle = (*current).parker.unpark_lock();
            bucket.mutex.unlock();
            handle.unpark();

            return result;
        } else {
            link = &(*current).next_in_queue;
            previous = current;
            current = link.get();
        }
    }

    // No threads with a matching key were found in the bucket
    callback(result);
    bucket.mutex.unlock();
    result
}

/// Unparks all threads in the queue associated with the given key.
///
/// The given `UnparkToken` is passed to all unparked threads.
///
/// This function returns the number of threads that were unparked.
///
/// # Safety
///
/// You should only call this function with an address that you control, since
/// you could otherwise interfere with the operation of other synchronization
/// primitives.
#[inline]
pub unsafe fn unpark_all(key: usize, unpark_token: UnparkToken) -> usize {
    // Lock the bucket for the given key
    let bucket = lock_bucket(key);

    // Remove all threads with the given key in the bucket
    let mut link = &bucket.queue_head;
    let mut current = bucket.queue_head.get();
    let mut previous = ptr::null();
    let mut threads = SmallVec::<[_; 8]>::new();
    while !current.is_null() {
        if (*current).key.load(Ordering::Relaxed) == key {
            // Remove the thread from the queue
            let next = (*current).next_in_queue.get();
            link.set(next);
            if bucket.queue_tail.get() == current {
                bucket.queue_tail.set(previous);
            }

            // Set the token for the target thread
            (*current).unpark_token.set(unpark_token);

            // Don't wake up threads while holding the queue lock. See comment
            // in unpark_one. For now just record which threads we need to wake
            // up.
            threads.push((*current).parker.unpark_lock());
            current = next;
        } else {
            link = &(*current).next_in_queue;
            previous = current;
            current = link.get();
        }
    }

    // Unlock the bucket
    bucket.mutex.unlock();

    // Now that we are outside the lock, wake up all the threads that we removed
    // from the queue.
    let num_threads = threads.len();
    for handle in threads.into_iter() {
        handle.unpark();
    }

    num_threads
}

/// Removes all threads from the queue associated with `key_from`, optionally
/// unparks the first one and requeues the rest onto the queue associated with
/// `key_to`.
///
/// The `validate` function is called while both queues are locked. Its return
/// value will determine which operation is performed, or whether the operation
/// should be aborted. See `RequeueOp` for details about the different possible
/// return values.
///
/// The `callback` function is also called while both queues are locked. It is
/// passed the `RequeueOp` returned by `validate` and an `UnparkResult`
/// indicating whether a thread was unparked and whether there are threads still
/// parked in the new queue. This `UnparkResult` value is also returned by
/// `unpark_requeue`.
///
/// The `callback` function should return an `UnparkToken` value which will be
/// passed to the thread that is unparked. If no thread is unparked then the
/// returned value is ignored.
///
/// # Safety
///
/// You should only call this function with an address that you control, since
/// you could otherwise interfere with the operation of other synchronization
/// primitives.
///
/// The `validate` and `callback` functions are called while the queue is locked
/// and must not panic or call into any function in `parking_lot`.
#[inline]
pub unsafe fn unpark_requeue(
    key_from: usize,
    key_to: usize,
    validate: impl FnOnce() -> RequeueOp,
    callback: impl FnOnce(RequeueOp, UnparkResult) -> UnparkToken,
) -> UnparkResult {
    // Lock the two buckets for the given key
    let (bucket_from, bucket_to) = lock_bucket_pair(key_from, key_to);

    // If the validation function fails, just return
    let mut result = UnparkResult::default();
    let op = validate();
    if op == RequeueOp::Abort {
        unlock_bucket_pair(bucket_from, bucket_to);
        return result;
    }

    // Remove all threads with the given key in the source bucket
    let mut link = &bucket_from.queue_head;
    let mut current = bucket_from.queue_head.get();
    let mut previous = ptr::null();
    let mut requeue_threads: *const ThreadData = ptr::null();
    let mut requeue_threads_tail: *const ThreadData = ptr::null();
    let mut wakeup_thread = None;
    while !current.is_null() {
        if (*current).key.load(Ordering::Relaxed) == key_from {
            // Remove the thread from the queue
            let next = (*current).next_in_queue.get();
            link.set(next);
            if bucket_from.queue_tail.get() == current {
                bucket_from.queue_tail.set(previous);
            }

            // Prepare the first thread for wakeup and requeue the rest.
            if (op == RequeueOp::UnparkOneRequeueRest || op == RequeueOp::UnparkOne)
                && wakeup_thread.is_none()
            {
                wakeup_thread = Some(current);
                result.unparked_threads = 1;
            } else {
                if !requeue_threads.is_null() {
                    (*requeue_threads_tail).next_in_queue.set(current);
                } else {
                    requeue_threads = current;
                }
                requeue_threads_tail = current;
                (*current).key.store(key_to, Ordering::Relaxed);
                result.requeued_threads += 1;
            }
            if op == RequeueOp::UnparkOne || op == RequeueOp::RequeueOne {
                // Scan the rest of the queue to see if there are any other
                // entries with the given key.
                let mut scan = next;
                while !scan.is_null() {
                    if (*scan).key.load(Ordering::Relaxed) == key_from {
                        result.have_more_threads = true;
                        break;
                    }
                    scan = (*scan).next_in_queue.get();
                }
                break;
            }
            current = next;
        } else {
            link = &(*current).next_in_queue;
            previous = current;
            current = link.get();
        }
    }

    // Add the requeued threads to the destination bucket
    if !requeue_threads.is_null() {
        (*requeue_threads_tail).next_in_queue.set(ptr::null());
        if !bucket_to.queue_head.get().is_null() {
            (*bucket_to.queue_tail.get())
                .next_in_queue
                .set(requeue_threads);
        } else {
            bucket_to.queue_head.set(requeue_threads);
        }
        bucket_to.queue_tail.set(requeue_threads_tail);
    }

    // Invoke the callback before waking up the thread
    if result.unparked_threads != 0 {
        result.be_fair = (*bucket_from.fair_timeout.get()).should_timeout();
    }
    let token = callback(op, result);

    // See comment in unpark_one for why we mess with the locking
    if let Some(wakeup_thread) = wakeup_thread {
        (*wakeup_thread).unpark_token.set(token);
        let handle = (*wakeup_thread).parker.unpark_lock();
        unlock_bucket_pair(bucket_from, bucket_to);
        handle.unpark();
    } else {
        unlock_bucket_pair(bucket_from, bucket_to);
    }

    result
}

/// Unparks a number of threads from the front of the queue associated with
/// `key` depending on the results of a filter function which inspects the
/// `ParkToken` associated with each thread.
///
/// The `filter` function is called for each thread in the queue or until
/// `FilterOp::Stop` is returned. This function is passed the `ParkToken`
/// associated with a particular thread, which is unparked if `FilterOp::Unpark`
/// is returned.
///
/// The `callback` function is also called while both queues are locked. It is
/// passed an `UnparkResult` indicating the number of threads that were unparked
/// and whether there are still parked threads in the queue. This `UnparkResult`
/// value is also returned by `unpark_filter`.
///
/// The `callback` function should return an `UnparkToken` value which will be
/// passed to all threads that are unparked. If no thread is unparked then the
/// returned value is ignored.
///
/// # Safety
///
/// You should only call this function with an address that you control, since
/// you could otherwise interfere with the operation of other synchronization
/// primitives.
///
/// The `filter` and `callback` functions are called while the queue is locked
/// and must not panic or call into any function in `parking_lot`.
#[inline]
pub unsafe fn unpark_filter(
    key: usize,
    mut filter: impl FnMut(ParkToken) -> FilterOp,
    callback: impl FnOnce(UnparkResult) -> UnparkToken,
) -> UnparkResult {
    // Lock the bucket for the given key
    let bucket = lock_bucket(key);

    // Go through the queue looking for threads with a matching key
    let mut link = &bucket.queue_head;
    let mut current = bucket.queue_head.get();
    let mut previous = ptr::null();
    let mut threads = SmallVec::<[_; 8]>::new();
    let mut result = UnparkResult::default();
    while !current.is_null() {
        if (*current).key.load(Ordering::Relaxed) == key {
            // Call the filter function with the thread's ParkToken
            let next = (*current).next_in_queue.get();
            match filter((*current).park_token.get()) {
                FilterOp::Unpark => {
                    // Remove the thread from the queue
                    link.set(next);
                    if bucket.queue_tail.get() == current {
                        bucket.queue_tail.set(previous);
                    }

                    // Add the thread to our list of threads to unpark
                    threads.push((current, None));

                    current = next;
                }
                FilterOp::Skip => {
                    result.have_more_threads = true;
                    link = &(*current).next_in_queue;
                    previous = current;
                    current = link.get();
                }
                FilterOp::Stop => {
                    result.have_more_threads = true;
                    break;
                }
            }
        } else {
            link = &(*current).next_in_queue;
            previous = current;
            current = link.get();
        }
    }

    // Invoke the callback before waking up the threads
    result.unparked_threads = threads.len();
    if result.unparked_threads != 0 {
        result.be_fair = (*bucket.fair_timeout.get()).should_timeout();
    }
    let token = callback(result);

    // Pass the token to all threads that are going to be unparked and prepare
    // them for unparking.
    for t in threads.iter_mut() {
        (*t.0).unpark_token.set(token);
        t.1 = Some((*t.0).parker.unpark_lock());
    }

    bucket.mutex.unlock();

    // Now that we are outside the lock, wake up all the threads that we removed
    // from the queue.
    for (_, handle) in threads.into_iter() {
        handle.unchecked_unwrap().unpark();
    }

    result
}

/// \[Experimental\] Deadlock detection
///
/// Enabled via the `deadlock_detection` feature flag.
pub mod deadlock {
    #[cfg(feature = "deadlock_detection")]
    use super::deadlock_impl;

    #[cfg(feature = "deadlock_detection")]
    pub(super) use super::deadlock_impl::DeadlockData;

    /// Acquire a resource identified by key in the deadlock detector
    /// Noop if deadlock_detection feature isn't enabled.
    /// Note: Call after the resource is acquired
    #[inline]
    pub unsafe fn acquire_resource(_key: usize) {
        #[cfg(feature = "deadlock_detection")]
        deadlock_impl::acquire_resource(_key);
    }

    /// Release a resource identified by key in the deadlock detector.
    /// Noop if deadlock_detection feature isn't enabled.
    /// Note: Call before the resource is released
    /// # Panics
    /// Panics if the resource was already released or wasn't acquired in this thread.
    #[inline]
    pub unsafe fn release_resource(_key: usize) {
        #[cfg(feature = "deadlock_detection")]
        deadlock_impl::release_resource(_key);
    }

    /// Returns all deadlocks detected *since* the last call.
    /// Each cycle consist of a vector of `DeadlockedThread`.
    #[cfg(feature = "deadlock_detection")]
    #[inline]
    pub fn check_deadlock() -> Vec<Vec<deadlock_impl::DeadlockedThread>> {
        deadlock_impl::check_deadlock()
    }

    #[inline]
    pub(super) unsafe fn on_unpark(_td: &super::ThreadData) {
        #[cfg(feature = "deadlock_detection")]
        deadlock_impl::on_unpark(_td);
    }
}

#[cfg(feature = "deadlock_detection")]
mod deadlock_impl {
    use super::{get_hashtable, lock_bucket, with_thread_data, ThreadData, NUM_THREADS};
    use crate::thread_parker::{ThreadParkerT, UnparkHandleT};
    use crate::word_lock::WordLock;
    use backtrace::Backtrace;
    use petgraph;
    use petgraph::graphmap::DiGraphMap;
    use std::cell::{Cell, UnsafeCell};
    use std::collections::HashSet;
    use std::sync::atomic::Ordering;
    use std::sync::mpsc;
    use thread_id;

    /// Representation of a deadlocked thread
    pub struct DeadlockedThread {
        thread_id: usize,
        backtrace: Backtrace,
    }

    impl DeadlockedThread {
        /// The system thread id
        pub fn thread_id(&self) -> usize {
            self.thread_id
        }

        /// The thread backtrace
        pub fn backtrace(&self) -> &Backtrace {
            &self.backtrace
        }
    }

    pub struct DeadlockData {
        // Currently owned resources (keys)
        resources: UnsafeCell<Vec<usize>>,

        // Set when there's a pending callstack request
        deadlocked: Cell<bool>,

        // Sender used to report the backtrace
        backtrace_sender: UnsafeCell<Option<mpsc::Sender<DeadlockedThread>>>,

        // System thread id
        thread_id: usize,
    }

    impl DeadlockData {
        pub fn new() -> Self {
            DeadlockData {
                resources: UnsafeCell::new(Vec::new()),
                deadlocked: Cell::new(false),
                backtrace_sender: UnsafeCell::new(None),
                thread_id: thread_id::get(),
            }
        }
    }

    pub(super) unsafe fn on_unpark(td: &ThreadData) {
        if td.deadlock_data.deadlocked.get() {
            let sender = (*td.deadlock_data.backtrace_sender.get()).take().unwrap();
            sender
                .send(DeadlockedThread {
                    thread_id: td.deadlock_data.thread_id,
                    backtrace: Backtrace::new(),
                })
                .unwrap();
            // make sure to close this sender
            drop(sender);

            // park until the end of the time
            td.parker.prepare_park();
            td.parker.park();
            unreachable!("unparked deadlocked thread!");
        }
    }

    pub unsafe fn acquire_resource(key: usize) {
        with_thread_data(|thread_data| {
            (*thread_data.deadlock_data.resources.get()).push(key);
        });
    }

    pub unsafe fn release_resource(key: usize) {
        with_thread_data(|thread_data| {
            let resources = &mut (*thread_data.deadlock_data.resources.get());
            match resources.iter().rposition(|x| *x == key) {
                Some(p) => resources.swap_remove(p),
                None => panic!("key {} not found in thread resources", key),
            };
        });
    }

    pub fn check_deadlock() -> Vec<Vec<DeadlockedThread>> {
        unsafe {
            // fast pass
            if check_wait_graph_fast() {
                // double check
                check_wait_graph_slow()
            } else {
                Vec::new()
            }
        }
    }

    // Simple algorithm that builds a wait graph f the threads and the resources,
    // then checks for the presence of cycles (deadlocks).
    // This variant isn't precise as it doesn't lock the entire table before checking
    unsafe fn check_wait_graph_fast() -> bool {
        let table = get_hashtable();
        let thread_count = NUM_THREADS.load(Ordering::Relaxed);
        let mut graph = DiGraphMap::<usize, ()>::with_capacity(thread_count * 2, thread_count * 2);

        for b in &(*table).entries[..] {
            b.mutex.lock();
            let mut current = b.queue_head.get();
            while !current.is_null() {
                if !(*current).parked_with_timeout.get()
                    && !(*current).deadlock_data.deadlocked.get()
                {
                    // .resources are waiting for their owner
                    for &resource in &(*(*current).deadlock_data.resources.get()) {
                        graph.add_edge(resource, current as usize, ());
                    }
                    // owner waits for resource .key
                    graph.add_edge(current as usize, (*current).key.load(Ordering::Relaxed), ());
                }
                current = (*current).next_in_queue.get();
            }
            b.mutex.unlock();
        }

        petgraph::algo::is_cyclic_directed(&graph)
    }

    #[derive(Hash, PartialEq, Eq, PartialOrd, Ord, Copy, Clone)]
    enum WaitGraphNode {
        Thread(*const ThreadData),
        Resource(usize),
    }

    use self::WaitGraphNode::*;

    // Contrary to the _fast variant this locks the entries table before looking for cycles.
    // Returns all detected thread wait cycles.
    // Note that once a cycle is reported it's never reported again.
    unsafe fn check_wait_graph_slow() -> Vec<Vec<DeadlockedThread>> {
        static DEADLOCK_DETECTION_LOCK: WordLock = WordLock::INIT;
        DEADLOCK_DETECTION_LOCK.lock();

        let mut table = get_hashtable();
        loop {
            // Lock all buckets in the old table
            for b in &(*table).entries[..] {
                b.mutex.lock();
            }

            // Now check if our table is still the latest one. Another thread could
            // have grown the hash table between us getting and locking the hash table.
            let new_table = get_hashtable();
            if new_table == table {
                break;
            }

            // Unlock buckets and try again
            for b in &(*table).entries[..] {
                b.mutex.unlock();
            }

            table = new_table;
        }

        let thread_count = NUM_THREADS.load(Ordering::Relaxed);
        let mut graph =
            DiGraphMap::<WaitGraphNode, ()>::with_capacity(thread_count * 2, thread_count * 2);

        for b in &(*table).entries[..] {
            let mut current = b.queue_head.get();
            while !current.is_null() {
                if !(*current).parked_with_timeout.get()
                    && !(*current).deadlock_data.deadlocked.get()
                {
                    // .resources are waiting for their owner
                    for &resource in &(*(*current).deadlock_data.resources.get()) {
                        graph.add_edge(Resource(resource), Thread(current), ());
                    }
                    // owner waits for resource .key
                    graph.add_edge(
                        Thread(current),
                        Resource((*current).key.load(Ordering::Relaxed)),
                        (),
                    );
                }
                current = (*current).next_in_queue.get();
            }
        }

        for b in &(*table).entries[..] {
            b.mutex.unlock();
        }

        // find cycles
        let cycles = graph_cycles(&graph);

        let mut results = Vec::with_capacity(cycles.len());

        for cycle in cycles {
            let (sender, receiver) = mpsc::channel();
            for td in cycle {
                let bucket = lock_bucket((*td).key.load(Ordering::Relaxed));
                (*td).deadlock_data.deadlocked.set(true);
                *(*td).deadlock_data.backtrace_sender.get() = Some(sender.clone());
                let handle = (*td).parker.unpark_lock();
                bucket.mutex.unlock();
                // unpark the deadlocked thread!
                // on unpark it'll notice the deadlocked flag and report back
                handle.unpark();
            }
            // make sure to drop our sender before collecting results
            drop(sender);
            results.push(receiver.iter().collect());
        }

        DEADLOCK_DETECTION_LOCK.unlock();

        results
    }

    // normalize a cycle to start with the "smallest" node
    fn normalize_cycle<T: Ord + Copy + Clone>(input: &[T]) -> Vec<T> {
        let min_pos = input
            .iter()
            .enumerate()
            .min_by_key(|&(_, &t)| t)
            .map(|(p, _)| p)
            .unwrap_or(0);
        input
            .iter()
            .cycle()
            .skip(min_pos)
            .take(input.len())
            .cloned()
            .collect()
    }

    // returns all thread cycles in the wait graph
    fn graph_cycles(g: &DiGraphMap<WaitGraphNode, ()>) -> Vec<Vec<*const ThreadData>> {
        use petgraph::visit::depth_first_search;
        use petgraph::visit::DfsEvent;
        use petgraph::visit::NodeIndexable;

        let mut cycles = HashSet::new();
        let mut path = Vec::with_capacity(g.node_bound());
        // start from threads to get the correct threads cycle
        let threads = g
            .nodes()
            .filter(|n| if let &Thread(_) = n { true } else { false });

        depth_first_search(g, threads, |e| match e {
            DfsEvent::Discover(Thread(n), _) => path.push(n),
            DfsEvent::Finish(Thread(_), _) => {
                path.pop();
            }
            DfsEvent::BackEdge(_, Thread(n)) => {
                let from = path.iter().rposition(|&i| i == n).unwrap();
                cycles.insert(normalize_cycle(&path[from..]));
            }
            _ => (),
        });

        cycles.iter().cloned().collect()
    }
}
