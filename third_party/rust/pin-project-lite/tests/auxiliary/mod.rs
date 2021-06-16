#![allow(dead_code, unused_macros)]
#![allow(box_pointers, unreachable_pub)]
#![allow(clippy::restriction)]

macro_rules! assert_unpin {
    ($ty:ty) => {
        static_assertions::assert_impl_all!($ty: Unpin);
    };
}
macro_rules! assert_not_unpin {
    ($ty:ty) => {
        static_assertions::assert_not_impl_all!($ty: Unpin);
    };
}
