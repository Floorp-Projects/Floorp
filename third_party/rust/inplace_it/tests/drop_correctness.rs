use std::cell::Cell;
use inplace_it::*;
use std::mem::MaybeUninit;

struct DropCounter {
    count: Cell<usize>,
}

impl DropCounter {
    fn with_current<F: FnOnce(&DropCounter) -> R, R>(f: F) -> R {
        thread_local!(
            static COUNTER: DropCounter = DropCounter {count: Cell::new(0)};
        );
        COUNTER.with(f)
    }

    #[inline]
    fn get() -> usize {
        DropCounter::with_current(|c| c.count.get())
    }

    #[inline]
    fn inc() {
        DropCounter::with_current(|c| c.count.set(c.count.get() + 1));
    }

    #[inline]
    fn clear() {
        DropCounter::with_current(|c| c.count.set(0));
    }
}

struct DropCounterTrigger(u8 /* One byte to avoid zero-sized types optimizations */);

impl DropCounterTrigger {
    fn new() -> Self {
        Self(228)
    }
}

impl Drop for DropCounterTrigger {
    #[inline]
    fn drop(&mut self) {
        DropCounter::inc();
    }
}

#[test]
fn maybe_uninit_works_as_expected() {
    DropCounter::clear();
    drop(MaybeUninit::<DropCounterTrigger>::uninit());
    assert_eq!(DropCounter::get(), 0);
    DropCounter::clear();
    drop(MaybeUninit::<DropCounterTrigger>::new(DropCounterTrigger::new()));
    assert_eq!(DropCounter::get(), 0);
    DropCounter::clear();
    let mut memory = MaybeUninit::<DropCounterTrigger>::new(DropCounterTrigger::new());
    unsafe { core::ptr::drop_in_place(memory.as_mut_ptr()); }
    drop(memory);
    assert_eq!(DropCounter::get(), 1);
}

#[test]
fn inplace_array_should_correctly_drop_values() {
    for i in (0..4096).step_by(8) {
        DropCounter::clear();
        try_inplace_array(i, |guard: UninitializedSliceMemoryGuard<DropCounterTrigger>| {
            assert!(guard.len() >= i);
        }).map_err(|_| format!("Cannot inplace array of {} size", i)).unwrap();
        assert_eq!(DropCounter::get(), 0);
        DropCounter::clear();
        let len = try_inplace_array(i, |guard| {
            let len = guard.len();
            guard.init(|_| DropCounterTrigger::new());
            len
        }).map_err(|_| format!("Cannot inplace array of {} size", i)).unwrap();
        assert_eq!(DropCounter::get(), len);
        DropCounter::clear();
        try_inplace_array(i, |guard| {
            let guard = guard.slice(..i);
            assert_eq!(guard.len(), i);
            guard.init(|_| DropCounterTrigger::new());
        }).map_err(|_| format!("Cannot inplace array of {} size", i)).unwrap();
        assert_eq!(DropCounter::get(), i);
    }
}

#[test]
fn alloc_array_should_correctly_drop_values() {
    for i in (0..4096).step_by(8) {
        DropCounter::clear();
        alloc_array(i, |_guard: UninitializedSliceMemoryGuard<DropCounterTrigger>| {});
        assert_eq!(DropCounter::get(), 0);
        DropCounter::clear();
        alloc_array(i, |guard| {
            guard.init(|_| DropCounterTrigger::new());
        });
        assert_eq!(DropCounter::get(), i);
    }
}
