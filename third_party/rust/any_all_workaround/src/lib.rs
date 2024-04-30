// This code began as a fork of
// https://raw.githubusercontent.com/rust-lang/packed_simd/d938e39bee9bc5c222f5f2f2a0df9e53b5ce36ae/src/codegen/reductions/mask/arm.rs
// which didn't have a license header on the file, but Cargo.toml said "MIT OR Apache-2.0".
// See LICENSE-MIT and LICENSE-APACHE.

#![no_std]
#![feature(portable_simd)]
#![cfg_attr(
    all(
        stdsimd_split,
        target_arch = "arm",
        target_endian = "little",
        target_feature = "neon",
        target_feature = "v7"
    ),
    feature(stdarch_arm_neon_intrinsics)
)]
#![cfg_attr(
    all(
        not(stdsimd_split),
        target_arch = "arm",
        target_endian = "little",
        target_feature = "neon",
        target_feature = "v7"
    ),
    feature(stdsimd)
)]

use cfg_if::cfg_if;
use core::simd::mask16x8;
use core::simd::mask32x4;
use core::simd::mask8x16;

cfg_if! {
    if #[cfg(all(target_arch = "arm", target_endian = "little", target_feature = "neon", target_feature = "v7"))] {
        use core::simd::mask8x8;
        use core::simd::mask16x4;
        use core::simd::mask32x2;
        macro_rules! arm_128_v7_neon_impl {
            ($all:ident, $any:ident, $id:ident, $half:ident, $vpmin:ident, $vpmax:ident) => {
                #[inline]
                pub fn $all(s: $id) -> bool {
                    use core::arch::arm::$vpmin;
                    use core::mem::transmute;
                    unsafe {
                        union U {
                            halves: ($half, $half),
                            vec: $id,
                        }
                        let halves = U { vec: s }.halves;
                        let h: $half = transmute($vpmin(transmute(halves.0), transmute(halves.1)));
                        h.all()
                    }
                }
                #[inline]
                pub fn $any(s: $id) -> bool {
                    use core::arch::arm::$vpmax;
                    use core::mem::transmute;
                    unsafe {
                        union U {
                            halves: ($half, $half),
                            vec: $id,
                        }
                        let halves = U { vec: s }.halves;
                        let h: $half = transmute($vpmax(transmute(halves.0), transmute(halves.1)));
                        h.any()
                    }
                }
            }
        }
    } else {
        macro_rules! arm_128_v7_neon_impl {
            ($all:ident, $any:ident, $id:ident, $half:ident, $vpmin:ident, $vpmax:ident) => {
                #[inline(always)]
                pub fn $all(s: $id) -> bool {
                    s.all()
                }
                #[inline(always)]
                pub fn $any(s: $id) -> bool {
                    s.any()
                }
            }
        }
    }
}

arm_128_v7_neon_impl!(
    all_mask8x16,
    any_mask8x16,
    mask8x16,
    mask8x8,
    vpmin_u8,
    vpmax_u8
);
arm_128_v7_neon_impl!(
    all_mask16x8,
    any_mask16x8,
    mask16x8,
    mask16x4,
    vpmin_u16,
    vpmax_u16
);
arm_128_v7_neon_impl!(
    all_mask32x4,
    any_mask32x4,
    mask32x4,
    mask32x2,
    vpmin_u32,
    vpmax_u32
);
