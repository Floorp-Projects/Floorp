// Copyright 2016 lazy-static.rs Developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

extern crate std;

use self::std::prelude::v1::*;
use self::std::cell::UnsafeCell;
use self::std::sync::{Once, ONCE_INIT};

pub struct Lazy<T: Sync>(UnsafeCell<Option<T>>, Once);

impl<T: Sync> Lazy<T> {
    #[inline(always)]
    pub const fn new() -> Self {
        Lazy(UnsafeCell::new(None), ONCE_INIT)
    }

    #[inline(always)]
    pub fn get<F>(&'static self, f: F) -> &T
        where F: FnOnce() -> T
    {
        unsafe {
            self.1.call_once(|| {
                *self.0.get() = Some(f());
            });

            match *self.0.get() {
                Some(ref x) => x,
                None => std::intrinsics::unreachable(),
            }
        }
    }
}

unsafe impl<T: Sync> Sync for Lazy<T> {}

#[macro_export]
#[allow_internal_unstable]
#[doc(hidden)]
macro_rules! __lazy_static_create {
    ($NAME:ident, $T:ty) => {
        static $NAME: $crate::lazy::Lazy<$T> = $crate::lazy::Lazy::new();
    }
}
