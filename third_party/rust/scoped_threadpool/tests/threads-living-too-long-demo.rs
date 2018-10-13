extern crate scoped_threadpool;

use scoped_threadpool::Pool;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::atomic::{AtomicBool, ATOMIC_BOOL_INIT};
use std::panic::AssertUnwindSafe;

// The representation invariant for PositivelyAtomic is that it is
// always a positive integer. When we drop it, we store zero in
// its value; but one should never observe that.
pub struct PositivelyAtomic(AtomicUsize);
impl Drop for PositivelyAtomic {
    fn drop(&mut self) {
        // Since we are being dropped, we will now break the
        // representation invariant. (And then clients will
        // subsequently observe that breakage.)
        self.0.store(0, Ordering::Relaxed);
    }
}
impl PositivelyAtomic {
    pub fn new(x: usize) -> PositivelyAtomic {
        assert!(x > 0);
        PositivelyAtomic(AtomicUsize::new(x))
    }
    // Assuming the representation invariant holds, this should
    // always return a positive value.
    pub fn load(&self) -> usize {
        self.0.load(Ordering::Relaxed)
    }
    pub fn cas(&self, old: usize, new: usize) -> usize {
        assert!(new > 0);
        self.0.compare_and_swap(old, new, Ordering::Relaxed)
    }
}

#[test]
fn demo_stack_allocated() {
    static SAW_ZERO: AtomicBool = ATOMIC_BOOL_INIT;
    for _i in 0..100 {
        let saw_zero = &AssertUnwindSafe(&SAW_ZERO);
        let _p = ::std::panic::catch_unwind(move || {
            let p = PositivelyAtomic::new(1);
            kernel(&p, saw_zero);
        });

        if saw_zero.load(Ordering::Relaxed) {
            panic!("demo_stack_allocated saw zero!");
        }
    }
}

#[test]
fn demo_heap_allocated() {
    static SAW_ZERO: AtomicBool = ATOMIC_BOOL_INIT;
    for i in 0..100 {
        let saw_zero = &AssertUnwindSafe(&SAW_ZERO);
        let _p = ::std::panic::catch_unwind(move || {
            let mut v = Vec::with_capacity((i % 5)*1024 + i);
            v.push(PositivelyAtomic::new(1));
            kernel(&v[0], saw_zero);
        });

        if saw_zero.load(Ordering::Relaxed) {
            panic!("demo_heap_allocated saw zero!");
        }
    }
}

pub fn kernel(r: &PositivelyAtomic, saw_zero: &AtomicBool) {
    // Create a threadpool holding 4 threads
    let mut pool = Pool::new(4);

    // Use the threads as scoped threads that can
    // reference anything outside this closure
    pool.scoped(|scope| {
        // Create references to each element in the vector ...
        for _ in 0..4 {
            scope.execute(move || {
                for _ in 0..100000 {
                    let v = r.load();
                    if v == 0 {
                        saw_zero.store(true, Ordering::Relaxed);
                        panic!("SAW ZERO");
                    }
                    let v_new = (v % 100) + 1;
                    if v != r.cas(v, v_new) {

                        // this is not a true panic condition
                        // in the original scenario.
                        //
                        // it rather is a rare event, and I want to
                        // emulate a rare panic occurring from one
                        // thread (and then see how the overall
                        // computation proceeds from there).
                        panic!("interference");
                    } else {
                        // incremented successfully
                    }
                }
            });
        }
    });
}
