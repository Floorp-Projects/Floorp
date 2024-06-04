#![allow(dead_code)]

extern crate alloc;

#[cfg(not(loom))]
use alloc::sync::Arc;
#[cfg(not(loom))]
use core::sync::atomic::{AtomicUsize, Ordering::SeqCst};
#[cfg(loom)]
use loom::sync::{
    atomic::{AtomicUsize, Ordering::SeqCst},
    Arc,
};

#[cfg(loom)]
pub mod waker;

pub fn maybe_loom_model(test: impl Fn() + Sync + Send + 'static) {
    #[cfg(loom)]
    loom::model(test);
    #[cfg(not(loom))]
    test();
}

pub struct DropCounter<T> {
    drop_count: Arc<AtomicUsize>,
    value: Option<T>,
}

pub struct DropCounterHandle(Arc<AtomicUsize>);

impl<T> DropCounter<T> {
    pub fn new(value: T) -> (Self, DropCounterHandle) {
        let drop_count = Arc::new(AtomicUsize::new(0));
        (
            Self {
                drop_count: drop_count.clone(),
                value: Some(value),
            },
            DropCounterHandle(drop_count),
        )
    }

    pub fn value(&self) -> &T {
        self.value.as_ref().unwrap()
    }

    pub fn into_value(mut self) -> T {
        self.value.take().unwrap()
    }
}

impl DropCounterHandle {
    pub fn count(&self) -> usize {
        self.0.load(SeqCst)
    }
}

impl<T> Drop for DropCounter<T> {
    fn drop(&mut self) {
        self.drop_count.fetch_add(1, SeqCst);
    }
}
