#![allow(dead_code)]
use super::*;
#[allow(unused_imports)]
use super::{
    f32x2,
    simd_eq, simd_ne, simd_lt, simd_le, simd_gt, simd_ge,
    simd_shuffle2, simd_shuffle4, simd_shuffle8, simd_shuffle16,
    simd_insert, simd_extract,
    simd_cast,
    simd_add, simd_sub, simd_mul, simd_div, simd_shl, simd_shr, simd_and, simd_or, simd_xor,

    Unalign, bitcast,
};
use core::{mem,ops};

/// Boolean type for 64-bit integers.
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[derive(Debug, Copy, Clone)]
pub struct bool64i(i64);
/// Boolean type for 64-bit floats.
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[derive(Debug, Copy, Clone)]
pub struct bool64f(i64);
/// A SIMD vector of 2 `u64`s.
#[repr(simd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[derive(Debug, Copy)]
pub struct u64x2(u64, u64);
/// A SIMD vector of 2 `i64`s.
#[repr(simd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[derive(Debug, Copy)]
pub struct i64x2(i64, i64);
/// A SIMD vector of 2 `f64`s.
#[repr(simd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[derive(Debug, Copy)]
pub struct f64x2(f64, f64);
/// A SIMD boolean vector for length-2 vectors of 64-bit integers.
#[repr(simd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[derive(Debug, Copy)]
pub struct bool64ix2(i64, i64);
/// A SIMD boolean vector for length-2 vectors of 64-bit floats.
#[repr(simd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[derive(Debug, Copy)]
pub struct bool64fx2(i64, i64);

simd! {
    bool64ix2: i64x2 = i64, u64x2 = u64, bool64ix2 = bool64i;
    bool64fx2: f64x2 = f64, bool64fx2 = bool64f;
}
basic_impls! {
    u64x2: u64, bool64ix2, simd_shuffle2, 2, x0 | x1;
    i64x2: i64, bool64ix2, simd_shuffle2, 2, x0 | x1;
    f64x2: f64, bool64fx2, simd_shuffle2, 2, x0 | x1;
}

mod common {
    use super::*;
    // naive for now
    #[inline]
    pub fn bool64ix2_all(x: bool64ix2) -> bool {
        x.0 != 0 && x.1 != 0
    }
    #[inline]
    pub fn bool64ix2_any(x: bool64ix2) -> bool {
        x.0 != 0 || x.1 != 0
    }
    #[inline]
    pub fn bool64fx2_all(x: bool64fx2) -> bool {
        x.0 != 0 && x.1 != 0
    }
    #[inline]
    pub fn bool64fx2_any(x: bool64fx2) -> bool {
        x.0 != 0 || x.1 != 0
    }}
bool_impls! {
    bool64ix2: bool64i, i64x2, i64, 2, bool64ix2_all, bool64ix2_any, x0 | x1
        [/// Convert `self` to a boolean vector for interacting with floating point vectors.
         to_f -> bool64fx2];

    bool64fx2: bool64f, i64x2, i64, 2, bool64fx2_all, bool64fx2_any, x0 | x1
        [/// Convert `self` to a boolean vector for interacting with integer vectors.
         to_i -> bool64ix2];
}

impl u64x2 {
    /// Convert each lane to a signed integer.
    #[inline]
    pub fn to_i64(self) -> i64x2 {
        unsafe {simd_cast(self)}
    }
    /// Convert each lane to a 64-bit float.
    #[inline]
    pub fn to_f64(self) -> f64x2 {
        unsafe {simd_cast(self)}
    }
}
impl i64x2 {
    /// Convert each lane to an unsigned integer.
    #[inline]
    pub fn to_u64(self) -> u64x2 {
        unsafe {simd_cast(self)}
    }
    /// Convert each lane to a 64-bit float.
    #[inline]
    pub fn to_f64(self) -> f64x2 {
        unsafe {simd_cast(self)}
    }
}
impl f64x2 {
    /// Convert each lane to a signed integer.
    #[inline]
    pub fn to_i64(self) -> i64x2 {
        unsafe {simd_cast(self)}
    }
    /// Convert each lane to an unsigned integer.
    #[inline]
    pub fn to_u64(self) -> u64x2 {
        unsafe {simd_cast(self)}
    }

    /// Convert each lane to a 32-bit float.
    #[inline]
    pub fn to_f32(self) -> f32x4 {
        unsafe {
            let x: f32x2 = simd_cast(self);
            f32x4::new(x.0, x.1, 0.0, 0.0)
        }
    }
}

neg_impls!{
    0,
    i64x2,
}
neg_impls! {
    0.0,
    f64x2,
}
macro_rules! not_impls {
    ($($ty: ident,)*) => {
        $(impl ops::Not for $ty {
            type Output = Self;
            fn not(self) -> Self {
                $ty::splat(!0) ^ self
            }
        })*
    }
}
not_impls! {
    i64x2,
    u64x2,
}

macro_rules! operators {
    ($($trayt: ident ($func: ident, $method: ident): $($ty: ty),*;)*) => {
        $(
            $(impl ops::$trayt for $ty {
                type Output = Self;
                #[inline]
                fn $method(self, x: Self) -> Self {
                    unsafe {$func(self, x)}
                }
            })*
                )*
    }
}
operators! {
    Add (simd_add, add):
        i64x2, u64x2,
        f64x2;
    Sub (simd_sub, sub):
        i64x2, u64x2,
        f64x2;
    Mul (simd_mul, mul):
        i64x2, u64x2,
        f64x2;
    Div (simd_div, div): f64x2;

    BitAnd (simd_and, bitand):
        i64x2, u64x2,
        bool64ix2,
        bool64fx2;
    BitOr (simd_or, bitor):
        i64x2, u64x2,
        bool64ix2,
        bool64fx2;
    BitXor (simd_xor, bitxor):
        i64x2, u64x2,
        bool64ix2,
        bool64fx2;
}

macro_rules! shift_one { ($ty: ident, $($by: ident),*) => {
        $(
        impl ops::Shl<$by> for $ty {
            type Output = Self;
            #[inline]
            fn shl(self, other: $by) -> Self {
                unsafe { simd_shl(self, $ty::splat(other as <$ty as Simd>::Elem)) }
            }
        }
        impl ops::Shr<$by> for $ty {
            type Output = Self;
            #[inline]
            fn shr(self, other: $by) -> Self {
                unsafe {simd_shr(self, $ty::splat(other as <$ty as Simd>::Elem))}
            }
        }
            )*
    }
}

macro_rules! shift {
    ($($ty: ident),*) => {
        $(shift_one! {
            $ty,
            u8, u16, u32, u64, usize,
            i8, i16, i32, i64, isize
        })*
    }
}
shift! {
    i64x2, u64x2
}
