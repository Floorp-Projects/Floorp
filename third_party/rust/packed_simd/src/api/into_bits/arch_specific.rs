//! `FromBits` and `IntoBits` between portable vector types and the
//! architecture-specific vector types.
#[rustfmt::skip]

// FIXME: MIPS FromBits/IntoBits

#[allow(unused)]
use crate::*;

/// This macro implements FromBits for the portable and the architecture
/// specific vector types.
///
/// The "leaf" case is at the bottom, and the most generic case is at the top.
/// The generic case is split into smaller cases recursively.
macro_rules! impl_arch {
    ([$arch_head_i:ident[$arch_head_tt:tt]: $($arch_head_ty:ident),*],
     $([$arch_tail_i:ident[$arch_tail_tt:tt]: $($arch_tail_ty:ident),*]),* |
     from: $($from_ty:ident),* | into: $($into_ty:ident),* |
     test: $test_tt:tt) => {
        impl_arch!(
            [$arch_head_i[$arch_head_tt]: $($arch_head_ty),*] |
            from: $($from_ty),* |
            into: $($into_ty),* |
            test: $test_tt
        );
        impl_arch!(
            $([$arch_tail_i[$arch_tail_tt]: $($arch_tail_ty),*]),* |
            from: $($from_ty),* |
            into: $($into_ty),* |
            test: $test_tt
        );
    };
    ([$arch:ident[$arch_tt:tt]: $($arch_ty:ident),*] |
     from: $($from_ty:ident),* | into: $($into_ty:ident),* |
     test: $test_tt:tt) => {
        // note: if target is "arm", "+v7,+neon" must be enabled
        // and the std library must be recompiled with them
        #[cfg(any(
            not(target_arch = "arm"),
            all(target_feature = "v7", target_feature = "neon",
                any(feature = "core_arch", libcore_neon)))
        )]
        // note: if target is "powerpc", "altivec" must be enabled
        // and the std library must be recompiled with it
        #[cfg(any(
            not(target_arch = "powerpc"),
            all(target_feature = "altivec", feature = "core_arch"),
        ))]
        #[cfg(target_arch = $arch_tt)]
        use crate::arch::$arch::{
            $($arch_ty),*
        };

        #[cfg(any(
            not(target_arch = "arm"),
            all(target_feature = "v7", target_feature = "neon",
                any(feature = "core_arch", libcore_neon)))
        )]
        #[cfg(any(
            not(target_arch = "powerpc"),
            all(target_feature = "altivec", feature = "core_arch"),
        ))]
        #[cfg(target_arch = $arch_tt)]
        impl_arch!($($arch_ty),* | $($from_ty),* | $($into_ty),* |
                   test: $test_tt);
    };
    ($arch_head:ident, $($arch_tail:ident),* | $($from_ty:ident),*
     | $($into_ty:ident),* | test: $test_tt:tt) => {
        impl_arch!($arch_head | $($from_ty),* | $($into_ty),* |
                   test: $test_tt);
        impl_arch!($($arch_tail),* | $($from_ty),* | $($into_ty),* |
                   test: $test_tt);
    };
    ($arch_head:ident | $($from_ty:ident),* | $($into_ty:ident),* |
     test: $test_tt:tt) => {
        impl_from_bits!($arch_head[$test_tt]: $($from_ty),*);
        impl_into_bits!($arch_head[$test_tt]: $($into_ty),*);
    };
}

////////////////////////////////////////////////////////////////////////////////
// Implementations for the 64-bit wide vector types:

// FIXME: 64-bit single element types
// FIXME: arm/aarch float16x4_t missing
impl_arch!(
    [
        arm["arm"]: int8x8_t,
        uint8x8_t,
        poly8x8_t,
        int16x4_t,
        uint16x4_t,
        poly16x4_t,
        int32x2_t,
        uint32x2_t,
        float32x2_t,
        int64x1_t,
        uint64x1_t
    ],
    [
        aarch64["aarch64"]: int8x8_t,
        uint8x8_t,
        poly8x8_t,
        int16x4_t,
        uint16x4_t,
        poly16x4_t,
        int32x2_t,
        uint32x2_t,
        float32x2_t,
        int64x1_t,
        uint64x1_t,
        float64x1_t
    ] | from: i8x8,
    u8x8,
    m8x8,
    i16x4,
    u16x4,
    m16x4,
    i32x2,
    u32x2,
    f32x2,
    m32x2 | into: i8x8,
    u8x8,
    i16x4,
    u16x4,
    i32x2,
    u32x2,
    f32x2 | test: test_v64
);

////////////////////////////////////////////////////////////////////////////////
// Implementations for the 128-bit wide vector types:

// FIXME: arm/aarch float16x8_t missing
// FIXME: ppc vector_pixel missing
// FIXME: ppc64 vector_Float16 missing
// FIXME: ppc64 vector_signed_long_long missing
// FIXME: ppc64 vector_unsigned_long_long missing
// FIXME: ppc64 vector_bool_long_long missing
// FIXME: ppc64 vector_signed___int128 missing
// FIXME: ppc64 vector_unsigned___int128 missing
impl_arch!(
    [x86["x86"]: __m128, __m128i, __m128d],
    [x86_64["x86_64"]: __m128, __m128i, __m128d],
    [
        arm["arm"]: int8x16_t,
        uint8x16_t,
        poly8x16_t,
        int16x8_t,
        uint16x8_t,
        poly16x8_t,
        int32x4_t,
        uint32x4_t,
        float32x4_t,
        int64x2_t,
        uint64x2_t
    ],
    [
        aarch64["aarch64"]: int8x16_t,
        uint8x16_t,
        poly8x16_t,
        int16x8_t,
        uint16x8_t,
        poly16x8_t,
        int32x4_t,
        uint32x4_t,
        float32x4_t,
        int64x2_t,
        uint64x2_t,
        float64x2_t
    ],
    [
        powerpc["powerpc"]: vector_signed_char,
        vector_unsigned_char,
        vector_signed_short,
        vector_unsigned_short,
        vector_signed_int,
        vector_unsigned_int,
        vector_float
    ],
    [
        powerpc64["powerpc64"]: vector_signed_char,
        vector_unsigned_char,
        vector_signed_short,
        vector_unsigned_short,
        vector_signed_int,
        vector_unsigned_int,
        vector_float,
        vector_signed_long,
        vector_unsigned_long,
        vector_double
    ] | from: i8x16,
    u8x16,
    m8x16,
    i16x8,
    u16x8,
    m16x8,
    i32x4,
    u32x4,
    f32x4,
    m32x4,
    i64x2,
    u64x2,
    f64x2,
    m64x2,
    i128x1,
    u128x1,
    m128x1 | into: i8x16,
    u8x16,
    i16x8,
    u16x8,
    i32x4,
    u32x4,
    f32x4,
    i64x2,
    u64x2,
    f64x2,
    i128x1,
    u128x1 | test: test_v128
);

impl_arch!(
    [powerpc["powerpc"]: vector_bool_char],
    [powerpc64["powerpc64"]: vector_bool_char] | from: m8x16,
    m16x8,
    m32x4,
    m64x2,
    m128x1 | into: i8x16,
    u8x16,
    i16x8,
    u16x8,
    i32x4,
    u32x4,
    f32x4,
    i64x2,
    u64x2,
    f64x2,
    i128x1,
    u128x1,
    // Masks:
    m8x16 | test: test_v128
);

impl_arch!(
    [powerpc["powerpc"]: vector_bool_short],
    [powerpc64["powerpc64"]: vector_bool_short] | from: m16x8,
    m32x4,
    m64x2,
    m128x1 | into: i8x16,
    u8x16,
    i16x8,
    u16x8,
    i32x4,
    u32x4,
    f32x4,
    i64x2,
    u64x2,
    f64x2,
    i128x1,
    u128x1,
    // Masks:
    m8x16,
    m16x8 | test: test_v128
);

impl_arch!(
    [powerpc["powerpc"]: vector_bool_int],
    [powerpc64["powerpc64"]: vector_bool_int] | from: m32x4,
    m64x2,
    m128x1 | into: i8x16,
    u8x16,
    i16x8,
    u16x8,
    i32x4,
    u32x4,
    f32x4,
    i64x2,
    u64x2,
    f64x2,
    i128x1,
    u128x1,
    // Masks:
    m8x16,
    m16x8,
    m32x4 | test: test_v128
);

impl_arch!(
    [powerpc64["powerpc64"]: vector_bool_long] | from: m64x2,
    m128x1 | into: i8x16,
    u8x16,
    i16x8,
    u16x8,
    i32x4,
    u32x4,
    f32x4,
    i64x2,
    u64x2,
    f64x2,
    i128x1,
    u128x1,
    // Masks:
    m8x16,
    m16x8,
    m32x4,
    m64x2 | test: test_v128
);

////////////////////////////////////////////////////////////////////////////////
// Implementations for the 256-bit wide vector types

impl_arch!(
    [x86["x86"]: __m256, __m256i, __m256d],
    [x86_64["x86_64"]: __m256, __m256i, __m256d] | from: i8x32,
    u8x32,
    m8x32,
    i16x16,
    u16x16,
    m16x16,
    i32x8,
    u32x8,
    f32x8,
    m32x8,
    i64x4,
    u64x4,
    f64x4,
    m64x4,
    i128x2,
    u128x2,
    m128x2 | into: i8x32,
    u8x32,
    i16x16,
    u16x16,
    i32x8,
    u32x8,
    f32x8,
    i64x4,
    u64x4,
    f64x4,
    i128x2,
    u128x2 | test: test_v256
);

////////////////////////////////////////////////////////////////////////////////
// FIXME: Implementations for the 512-bit wide vector types
