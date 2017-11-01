// Copyright 2016 lazy-static.rs Developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

extern crate spin;

use self::spin::Once;

pub struct Lazy<T: Sync>(Once<T>);

impl<T: Sync> Lazy<T> {
    #[inline(always)]
    pub const fn new() -> Self {
        Lazy(Once::new())
    }

    #[inline(always)]
    pub fn get<F>(&'static self, builder: F) -> &T
        where F: FnOnce() -> T
    {
        self.0.call_once(builder)
    }
}

#[macro_export]
#[allow_internal_unstable]
#[doc(hidden)]
macro_rules! __lazy_static_create {
    ($NAME:ident, $T:ty) => {
        static $NAME: $crate::lazy::Lazy<$T> = $crate::lazy::Lazy::new();
    }
}
