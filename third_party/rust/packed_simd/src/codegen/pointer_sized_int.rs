//! Provides `isize` and `usize`

use cfg_if::cfg_if;

cfg_if! {
    if #[cfg(target_pointer_width = "8")] {
        pub(crate) type isize_ = i8;
        pub(crate) type usize_ = u8;
    } else if #[cfg(target_pointer_width = "16")] {
        pub(crate) type isize_ = i16;
        pub(crate) type usize_ = u16;
    } else if #[cfg(target_pointer_width = "32")] {
        pub(crate) type isize_ = i32;
        pub(crate) type usize_ = u32;

    } else if #[cfg(target_pointer_width = "64")] {
        pub(crate) type isize_ = i64;
        pub(crate) type usize_ = u64;
    } else if #[cfg(target_pointer_width = "64")] {
        pub(crate) type isize_ = i64;
        pub(crate) type usize_ = u64;
    } else if #[cfg(target_pointer_width = "128")] {
        pub(crate) type isize_ = i128;
        pub(crate) type usize_ = u128;
    } else {
        compile_error!("unsupported target_pointer_width");
    }
}
