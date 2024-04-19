//! Mask reductions implementation for `x86` and `x86_64` targets

#[cfg(target_feature = "sse")]
#[macro_use]
mod sse;

#[cfg(target_feature = "sse2")]
#[macro_use]
mod sse2;

#[cfg(target_feature = "avx")]
#[macro_use]
mod avx;

#[cfg(target_feature = "avx2")]
#[macro_use]
mod avx2;

/// x86 64-bit m8x8 implementation
macro_rules! x86_m8x8_impl {
    ($id:ident) => {
        fallback_impl!($id);
    };
}

/// x86 128-bit m8x16 implementation
macro_rules! x86_m8x16_impl {
    ($id:ident) => {
        cfg_if! {
            if #[cfg(target_feature = "sse2")] {
                x86_m8x16_sse2_impl!($id);
            } else {
                fallback_impl!($id);
            }
        }
    };
}

/// x86 128-bit m32x4 implementation
macro_rules! x86_m32x4_impl {
    ($id:ident) => {
        cfg_if! {
            if #[cfg(target_feature = "sse")] {
                x86_m32x4_sse_impl!($id);
            } else {
                fallback_impl!($id);
            }
        }
    };
}

/// x86 128-bit m64x2 implementation
macro_rules! x86_m64x2_impl {
    ($id:ident) => {
        cfg_if! {
            if #[cfg(target_feature = "sse2")] {
                x86_m64x2_sse2_impl!($id);
            } else if #[cfg(target_feature = "sse")] {
                x86_m32x4_sse_impl!($id);
            } else {
                fallback_impl!($id);
            }
        }
    };
}

/// x86 256-bit m8x32 implementation
macro_rules! x86_m8x32_impl {
    ($id:ident, $half_id:ident) => {
        cfg_if! {
            if #[cfg(target_feature = "avx2")] {
                x86_m8x32_avx2_impl!($id);
            } else if #[cfg(target_feature = "avx")] {
                x86_m8x32_avx_impl!($id);
            } else if #[cfg(target_feature = "sse2")] {
                recurse_half!($id, $half_id);
            } else {
                fallback_impl!($id);
            }
        }
    };
}

/// x86 256-bit m32x8 implementation
macro_rules! x86_m32x8_impl {
    ($id:ident, $half_id:ident) => {
        cfg_if! {
            if #[cfg(target_feature = "avx")] {
                x86_m32x8_avx_impl!($id);
            } else if #[cfg(target_feature = "sse")] {
                recurse_half!($id, $half_id);
            } else {
                fallback_impl!($id);
            }
        }
    };
}

/// x86 256-bit m64x4 implementation
macro_rules! x86_m64x4_impl {
    ($id:ident, $half_id:ident) => {
        cfg_if! {
            if #[cfg(target_feature = "avx")] {
                x86_m64x4_avx_impl!($id);
            } else if #[cfg(target_feature = "sse")] {
                recurse_half!($id, $half_id);
            } else {
                fallback_impl!($id);
            }
        }
    };
}

/// Fallback implementation.
macro_rules! x86_intr_impl {
    ($id:ident) => {
        impl All for $id {
            #[inline]
            unsafe fn all(self) -> bool {
                use crate::llvm::simd_reduce_all;
                simd_reduce_all(self.0)
            }
        }
        impl Any for $id {
            #[inline]
            unsafe fn any(self) -> bool {
                use crate::llvm::simd_reduce_any;
                simd_reduce_any(self.0)
            }
        }
    };
}

/// Mask reduction implementation for `x86` and `x86_64` targets
macro_rules! impl_mask_reductions {
    // 64-bit wide masks
    (m8x8) => {
        x86_m8x8_impl!(m8x8);
    };
    (m16x4) => {
        x86_m8x8_impl!(m16x4);
    };
    (m32x2) => {
        x86_m8x8_impl!(m32x2);
    };
    // 128-bit wide masks
    (m8x16) => {
        x86_m8x16_impl!(m8x16);
    };
    (m16x8) => {
        x86_m8x16_impl!(m16x8);
    };
    (m32x4) => {
        x86_m32x4_impl!(m32x4);
    };
    (m64x2) => {
        x86_m64x2_impl!(m64x2);
    };
    (m128x1) => {
        x86_intr_impl!(m128x1);
    };
    // 256-bit wide masks:
    (m8x32) => {
        x86_m8x32_impl!(m8x32, m8x16);
    };
    (m16x16) => {
        x86_m8x32_impl!(m16x16, m16x8);
    };
    (m32x8) => {
        x86_m32x8_impl!(m32x8, m32x4);
    };
    (m64x4) => {
        x86_m64x4_impl!(m64x4, m64x2);
    };
    (m128x2) => {
        x86_intr_impl!(m128x2);
    };
    (msizex2) => {
        cfg_if! {
            if #[cfg(target_pointer_width = "64")] {
                fallback_to_other_impl!(msizex2, m64x2);
            } else if #[cfg(target_pointer_width = "32")] {
                fallback_to_other_impl!(msizex2, m32x2);
            } else {
                compile_error!("unsupported target_pointer_width");
            }
        }
    };
    (msizex4) => {
        cfg_if! {
            if #[cfg(target_pointer_width = "64")] {
                fallback_to_other_impl!(msizex4, m64x4);
            } else if #[cfg(target_pointer_width = "32")] {
                fallback_to_other_impl!(msizex4, m32x4);
            } else {
                compile_error!("unsupported target_pointer_width");
            }
        }
    };
    (msizex8) => {
        cfg_if! {
            if #[cfg(target_pointer_width = "64")] {
                fallback_to_other_impl!(msizex8, m64x8);
            } else if #[cfg(target_pointer_width = "32")] {
                fallback_to_other_impl!(msizex8, m32x8);
            } else {
                compile_error!("unsupported target_pointer_width");
            }
        }
    };

    // Fallback to LLVM's default code-generation:
    ($id:ident) => {
        fallback_impl!($id);
    };
}
