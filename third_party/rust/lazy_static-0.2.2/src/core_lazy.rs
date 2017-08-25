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
macro_rules! __lazy_static_create {
    ($NAME:ident, $T:ty) => {
        static $NAME: $crate::lazy::Lazy<$T> = $crate::lazy::Lazy::new();
    }
}
