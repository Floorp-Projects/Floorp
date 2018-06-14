// Copyright 2016 lazy-static.rs Developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

extern crate std;
extern crate core;

use self::std::prelude::v1::*;
use self::std::sync::Once;
pub use self::std::sync::ONCE_INIT;

pub struct Lazy<T: Sync>(pub Option<T>, pub Once);

impl<T: Sync> Lazy<T> {
    #[inline(always)]
    pub fn get<F>(&'static mut self, f: F) -> &T
        where F: FnOnce() -> T
    {
        {
            let r = &mut self.0;
            self.1.call_once(|| {
                *r = Some(f());
            });
        }
        unsafe {
            match self.0 {
                Some(ref x) => x,
                None => core::hint::unreachable_unchecked(),
            }
        }
    }
}

unsafe impl<T: Sync> Sync for Lazy<T> {}

#[macro_export]
#[doc(hidden)]
macro_rules! __lazy_static_create {
    ($NAME:ident, $T:ty) => {
        static mut $NAME: $crate::lazy::Lazy<$T> = $crate::lazy::Lazy(None, $crate::lazy::ONCE_INIT);
    }
}
