//! Mask reductions implementation for `aarch64` targets

/// 128-bit wide vectors
macro_rules! aarch64_128_neon_impl {
    ($id:ident, $vmin:ident, $vmax:ident) => {
        impl All for $id {
            #[inline]
            #[target_feature(enable = "neon")]
            unsafe fn all(self) -> bool {
                use crate::arch::aarch64::$vmin;
                $vmin(crate::mem::transmute(self)) != 0
            }
        }
        impl Any for $id {
            #[inline]
            #[target_feature(enable = "neon")]
            unsafe fn any(self) -> bool {
                use crate::arch::aarch64::$vmax;
                $vmax(crate::mem::transmute(self)) != 0
            }
        }
    };
}

/// 64-bit wide vectors
macro_rules! aarch64_64_neon_impl {
    ($id:ident, $vec128:ident) => {
        impl All for $id {
            #[inline]
            #[target_feature(enable = "neon")]
            unsafe fn all(self) -> bool {
                // Duplicates the 64-bit vector into a 128-bit one and
                // calls all on that.
                union U {
                    halves: ($id, $id),
                    vec: $vec128,
                }
                U { halves: (self, self) }.vec.all()
            }
        }
        impl Any for $id {
            #[inline]
            #[target_feature(enable = "neon")]
            unsafe fn any(self) -> bool {
                union U {
                    halves: ($id, $id),
                    vec: $vec128,
                }
                U { halves: (self, self) }.vec.any()
            }
        }
    };
}

/// Mask reduction implementation for `aarch64` targets
macro_rules! impl_mask_reductions {
    // 64-bit wide masks
    (m8x8) => {
        aarch64_64_neon_impl!(m8x8, m8x16);
    };
    (m16x4) => {
        aarch64_64_neon_impl!(m16x4, m16x8);
    };
    (m32x2) => {
        aarch64_64_neon_impl!(m32x2, m32x4);
    };
    // 128-bit wide masks
    (m8x16) => {
        aarch64_128_neon_impl!(m8x16, vminvq_u8, vmaxvq_u8);
    };
    (m16x8) => {
        aarch64_128_neon_impl!(m16x8, vminvq_u16, vmaxvq_u16);
    };
    (m32x4) => {
        aarch64_128_neon_impl!(m32x4, vminvq_u32, vmaxvq_u32);
    };
    // Fallback to LLVM's default code-generation:
    ($id:ident) => {
        fallback_impl!($id);
    };
}
