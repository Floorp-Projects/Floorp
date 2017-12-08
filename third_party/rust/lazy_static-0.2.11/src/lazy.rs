// Copyright 2016 lazy-static.rs Developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

extern crate std;

use self::std::prelude::v1::*;
use self::std::sync::Once;
pub use self::std::sync::ONCE_INIT;

pub struct Lazy<T: Sync>(pub *const T, pub Once);

impl<T: Sync> Lazy<T> {
    #[inline(always)]
    pub fn get<F>(&'static mut self, f: F) -> &T
        where F: FnOnce() -> T
    {
        unsafe {
            let r = &mut self.0;
            self.1.call_once(|| {
                *r = Box::into_raw(Box::new(f()));
            });

            &*self.0
        }
    }
}

unsafe impl<T: Sync> Sync for Lazy<T> {}

#[macro_export]
macro_rules! __lazy_static_create {
    ($NAME:ident, $T:ty) => {
        static mut $NAME: $crate::lazy::Lazy<$T> = $crate::lazy::Lazy(0 as *const $T, $crate::lazy::ONCE_INIT);
    }
}
