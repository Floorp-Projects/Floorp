//! Code generation workaround for `all()` mask horizontal reduction.
//!
//! Works arround [LLVM bug 36702].
//!
//! [LLVM bug 36702]: https://bugs.llvm.org/show_bug.cgi?id=36702
#![allow(unused_macros)]

use crate::*;

crate trait All: crate::marker::Sized {
    unsafe fn all(self) -> bool;
}

crate trait Any: crate::marker::Sized {
    unsafe fn any(self) -> bool;
}

#[macro_use]
mod fallback_impl;

cfg_if! {
    if #[cfg(any(target_arch = "x86", target_arch = "x86_64"))] {
        #[macro_use]
        mod x86;
    } else if #[cfg(all(target_arch = "arm", target_feature = "v7",
                        target_feature = "neon",
                        any(feature = "core_arch", libcore_neon)))] {
        #[macro_use]
        mod arm;
    } else if #[cfg(all(target_arch = "aarch64", target_feature = "neon"))] {
        #[macro_use]
        mod aarch64;
    } else {
        #[macro_use]
        mod fallback;
    }
}

impl_mask_reductions!(m8x2);
impl_mask_reductions!(m8x4);
impl_mask_reductions!(m8x8);
impl_mask_reductions!(m8x16);
impl_mask_reductions!(m8x32);
impl_mask_reductions!(m8x64);

impl_mask_reductions!(m16x2);
impl_mask_reductions!(m16x4);
impl_mask_reductions!(m16x8);
impl_mask_reductions!(m16x16);
impl_mask_reductions!(m16x32);

impl_mask_reductions!(m32x2);
impl_mask_reductions!(m32x4);
impl_mask_reductions!(m32x8);
impl_mask_reductions!(m32x16);

// FIXME: 64-bit single element vector
// impl_mask_reductions!(m64x1);
impl_mask_reductions!(m64x2);
impl_mask_reductions!(m64x4);
impl_mask_reductions!(m64x8);

impl_mask_reductions!(m128x1);
impl_mask_reductions!(m128x2);
impl_mask_reductions!(m128x4);

impl_mask_reductions!(msizex2);
impl_mask_reductions!(msizex4);
impl_mask_reductions!(msizex8);
