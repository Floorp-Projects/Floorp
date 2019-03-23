//! Provides `isize` and `usize`

use cfg_if::cfg_if;

cfg_if! {
    if #[cfg(target_pointer_width = "8")] {
        crate type isize_ = i8;
        crate type usize_ = u8;
    } else if #[cfg(target_pointer_width = "16")] {
        crate type isize_ = i16;
        crate type usize_ = u16;
    } else if #[cfg(target_pointer_width = "32")] {
        crate type isize_ = i32;
        crate type usize_ = u32;

    } else if #[cfg(target_pointer_width = "64")] {
        crate type isize_ = i64;
        crate type usize_ = u64;
    } else if #[cfg(target_pointer_width = "64")] {
        crate type isize_ = i64;
        crate type usize_ = u64;
    } else if #[cfg(target_pointer_width = "128")] {
        crate type isize_ = i128;
        crate type usize_ = u128;
    } else {
        compile_error!("unsupported target_pointer_width");
    }
}
