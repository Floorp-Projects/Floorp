// Copyright 2016 Amanieu d'Antras
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

#![allow(dead_code)]

use std::sync::atomic;

// Re-export this for convenience
pub use std::sync::atomic::{Ordering, fence};

// Wrapper around AtomicUsize for non-nightly which has usable compare_exchange
// and compare_exchange_weak methods.
pub struct AtomicUsize(atomic::AtomicUsize);
pub use self::AtomicUsize as AtomicU8;

// Constants for static initialization
pub const ATOMIC_USIZE_INIT: AtomicUsize = AtomicUsize(atomic::ATOMIC_USIZE_INIT);
pub use self::ATOMIC_USIZE_INIT as ATOMIC_U8_INIT;

impl AtomicUsize {
    #[inline]
    pub fn new(val: usize) -> AtomicUsize {
        AtomicUsize(atomic::AtomicUsize::new(val))
    }
    #[inline]
    pub fn load(&self, order: Ordering) -> usize {
        self.0.load(order)
    }
    #[inline]
    pub fn store(&self, val: usize, order: Ordering) {
        self.0.store(val, order);
    }
    #[inline]
    pub fn swap(&self, val: usize, order: Ordering) -> usize {
        self.0.swap(val, order)
    }
    #[inline]
    pub fn fetch_add(&self, val: usize, order: Ordering) -> usize {
        self.0.fetch_add(val, order)
    }
    #[inline]
    pub fn fetch_sub(&self, val: usize, order: Ordering) -> usize {
        self.0.fetch_sub(val, order)
    }
    #[inline]
    pub fn fetch_and(&self, val: usize, order: Ordering) -> usize {
        self.0.fetch_and(val, order)
    }
    #[inline]
    pub fn fetch_or(&self, val: usize, order: Ordering) -> usize {
        self.0.fetch_or(val, order)
    }
    #[inline]
    pub fn compare_exchange(&self,
                            old: usize,
                            new: usize,
                            order: Ordering,
                            _: Ordering)
                            -> Result<usize, usize> {
        let res = self.0.compare_and_swap(old, new, order);
        if res == old { Ok(res) } else { Err(res) }
    }
    #[inline]
    pub fn compare_exchange_weak(&self,
                                 old: usize,
                                 new: usize,
                                 order: Ordering,
                                 _: Ordering)
                                 -> Result<usize, usize> {
        let res = self.0.compare_and_swap(old, new, order);
        if res == old { Ok(res) } else { Err(res) }
    }
}
