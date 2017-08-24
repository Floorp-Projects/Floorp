extern crate std;

use self::std::prelude::v1::*;
use self::std::sync::Once;

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
        use std::sync::ONCE_INIT;
        static mut $NAME: $crate::lazy::Lazy<$T> = $crate::lazy::Lazy(0 as *const $T, ONCE_INIT);
    }
}
