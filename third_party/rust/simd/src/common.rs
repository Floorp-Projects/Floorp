use super::*;
#[allow(unused_imports)]
use super::{
    simd_eq, simd_ne, simd_lt, simd_le, simd_gt, simd_ge,
    simd_shuffle2, simd_shuffle4, simd_shuffle8, simd_shuffle16,
    simd_insert, simd_extract,
    simd_cast,
    simd_add, simd_sub, simd_mul, simd_div, simd_shl, simd_shr, simd_and, simd_or, simd_xor,

    Unalign, bitcast,
};
use std::mem;
use std::ops;

#[cfg(any(target_arch = "x86",
          target_arch = "x86_64"))]
use x86::sse2::common;
#[cfg(any(target_arch = "arm"))]
use arm::neon::common;
#[cfg(any(target_arch = "aarch64"))]
use aarch64::neon::common;

macro_rules! basic_impls {
    ($(
        $name: ident:
        $elem: ident, $bool: ident, $shuffle: ident, $length: expr, $($first: ident),* | $($last: ident),*;
        )*) => {
        $(impl $name {
            /// Create a new instance.
            #[inline]
            pub const fn new($($first: $elem),*, $($last: $elem),*) -> $name {
                $name($($first),*, $($last),*)
            }

            /// Create a new instance where every lane has value `x`.
            #[inline]
            pub const fn splat(x: $elem) -> $name {
                $name($({ #[allow(dead_code)] struct $first; x }),*,
                      $({ #[allow(dead_code)] struct $last; x }),*)
            }

            /// Compare for equality.
            #[inline]
            pub fn eq(self, other: Self) -> $bool {
                unsafe {simd_eq(self, other)}
            }
            /// Compare for equality.
            #[inline]
            pub fn ne(self, other: Self) -> $bool {
                unsafe {simd_ne(self, other)}
            }
            /// Compare for equality.
            #[inline]
            pub fn lt(self, other: Self) -> $bool {
                unsafe {simd_lt(self, other)}
            }
            /// Compare for equality.
            #[inline]
            pub fn le(self, other: Self) -> $bool {
                unsafe {simd_le(self, other)}
            }
            /// Compare for equality.
            #[inline]
            pub fn gt(self, other: Self) -> $bool {
                unsafe {simd_gt(self, other)}
            }
            /// Compare for equality.
            #[inline]
            pub fn ge(self, other: Self) -> $bool {
                unsafe {simd_ge(self, other)}
            }

            /// Extract the value of the `idx`th lane of `self`.
            ///
            /// # Panics
            ///
            /// `extract` will panic if `idx` is out of bounds.
            #[inline]
            pub fn extract(self, idx: u32) -> $elem {
                assert!(idx < $length);
                unsafe {simd_extract(self, idx)}
            }
            /// Return a new vector where the `idx`th lane is replaced
            /// by `elem`.
            ///
            /// # Panics
            ///
            /// `replace` will panic if `idx` is out of bounds.
            #[inline]
            pub fn replace(self, idx: u32, elem: $elem) -> Self {
                assert!(idx < $length);
                unsafe {simd_insert(self, idx, elem)}
            }

            /// Load a new value from the `idx`th position of `array`.
            ///
            /// This is equivalent to the following, but is possibly
            /// more efficient:
            ///
            /// ```rust,ignore
            /// Self::new(array[idx], array[idx + 1], ...)
            /// ```
            ///
            /// # Panics
            ///
            /// `load` will panic if `idx` is out of bounds in
            /// `array`, or if `array[idx..]` is too short.
            #[inline]
            pub fn load(array: &[$elem], idx: usize) -> Self {
                let data = &array[idx..idx + $length];
                let loaded = unsafe {
                    *(data.as_ptr() as *const Unalign<Self>)
                };
                loaded.0
            }

            /// Store the elements of `self` to `array`, starting at
            /// the `idx`th position.
            ///
            /// This is equivalent to the following, but is possibly
            /// more efficient:
            ///
            /// ```rust,ignore
            /// array[i] = self.extract(0);
            /// array[i + 1] = self.extract(1);
            /// // ...
            /// ```
            ///
            /// # Panics
            ///
            /// `store` will panic if `idx` is out of bounds in
            /// `array`, or if `array[idx...]` is too short.
            #[inline]
            pub fn store(self, array: &mut [$elem], idx: usize) {
                let place = &mut array[idx..idx + $length];
                unsafe {
                    *(place.as_mut_ptr() as *mut Unalign<Self>) = Unalign(self)
                }
            }
        })*
    }
}

basic_impls! {
    u32x4: u32, bool32ix4, simd_shuffle4, 4, x0, x1 | x2, x3;
    i32x4: i32, bool32ix4, simd_shuffle4, 4, x0, x1 | x2, x3;
    f32x4: f32, bool32fx4, simd_shuffle4, 4, x0, x1 | x2, x3;

    u16x8: u16, bool16ix8, simd_shuffle8, 8, x0, x1, x2, x3 | x4, x5, x6, x7;
    i16x8: i16, bool16ix8, simd_shuffle8, 8, x0, x1, x2, x3 | x4, x5, x6, x7;

    u8x16: u8, bool8ix16, simd_shuffle16, 16, x0, x1, x2, x3, x4, x5, x6, x7 | x8, x9, x10, x11, x12, x13, x14, x15;
    i8x16: i8, bool8ix16, simd_shuffle16, 16, x0, x1, x2, x3, x4, x5, x6, x7 | x8, x9, x10, x11, x12, x13, x14, x15;
}

macro_rules! bool_impls {
    ($(
        $name: ident:
        $elem: ident, $repr: ident, $repr_elem: ident, $length: expr, $all: ident, $any: ident,
        $($first: ident),* | $($last: ident),*
        [$(#[$cvt_meta: meta] $cvt: ident -> $cvt_to: ident),*];
        )*) => {
        $(impl $name {
            /// Convert to integer representation.
            #[inline]
            pub fn to_repr(self) -> $repr {
                unsafe {mem::transmute(self)}
            }
            /// Convert from integer representation.
            #[inline]
            #[inline]
            pub fn from_repr(x: $repr) -> Self {
                unsafe {mem::transmute(x)}
            }

            /// Create a new instance.
            #[inline]
            pub fn new($($first: bool),*, $($last: bool),*) -> $name {
                unsafe {
                    // negate everything together
                    simd_sub($name::splat(false),
                             $name($( ($first as $repr_elem) ),*,
                                   $( ($last as $repr_elem) ),*))
                }
            }

            /// Create a new instance where every lane has value `x`.
            #[allow(unused_variables)]
            #[inline]
            pub fn splat(x: bool) -> $name {
                let x = if x {!(0 as $repr_elem)} else {0};
                $name($({ let $first = (); x}),*,
                      $({ let $last = (); x}),*)
            }

            /// Extract the value of the `idx`th lane of `self`.
            ///
            /// # Panics
            ///
            /// `extract` will panic if `idx` is out of bounds.
            #[inline]
            pub fn extract(self, idx: u32) -> bool {
                assert!(idx < $length);
                unsafe {simd_extract(self.to_repr(), idx) != 0}
            }
            /// Return a new vector where the `idx`th lane is replaced
            /// by `elem`.
            ///
            /// # Panics
            ///
            /// `replace` will panic if `idx` is out of bounds.
            #[inline]
            pub fn replace(self, idx: u32, elem: bool) -> Self {
                assert!(idx < $length);
                let x = if elem {!(0 as $repr_elem)} else {0};
                unsafe {Self::from_repr(simd_insert(self.to_repr(), idx, x))}
            }
            /// Select between elements of `then` and `else_`, based on
            /// the corresponding element of `self`.
            ///
            /// This is equivalent to the following, but is possibly
            /// more efficient:
            ///
            /// ```rust,ignore
            /// T::new(if self.extract(0) { then.extract(0) } else { else_.extract(0) },
            ///        if self.extract(1) { then.extract(1) } else { else_.extract(1) },
            ///        ...)
            /// ```
            #[inline]
            pub fn select<T: Simd<Bool = $name>>(self, then: T, else_: T) -> T {
                let then: $repr = bitcast(then);
                let else_: $repr = bitcast(else_);
                bitcast((then & self.to_repr()) | (else_ & (!self).to_repr()))
            }

            /// Check if every element of `self` is true.
            ///
            /// This is equivalent to the following, but is possibly
            /// more efficient:
            ///
            /// ```rust,ignore
            /// self.extract(0) && self.extract(1) && ...
            /// ```
            #[inline]
            pub fn all(self) -> bool {
                common::$all(self)
            }
            /// Check if any element of `self` is true.
            ///
            /// This is equivalent to the following, but is possibly
            /// more efficient:
            ///
            /// ```rust,ignore
            /// self.extract(0) || self.extract(1) || ...
            /// ```
            #[inline]
            pub fn any(self) -> bool {
                common::$any(self)
            }

            $(
                #[$cvt_meta]
                #[inline]
                pub fn $cvt(self) -> $cvt_to {
                    bitcast(self)
                }
                )*
        }
          impl ops::Not for $name {
              type Output = Self;

              #[inline]
              fn not(self) -> Self {
                  Self::from_repr($repr::splat(!(0 as $repr_elem)) ^ self.to_repr())
              }
          }
          )*
    }
}

bool_impls! {
    bool32ix4: bool32i, i32x4, i32, 4, bool32ix4_all, bool32ix4_any, x0, x1 | x2, x3
        [/// Convert `self` to a boolean vector for interacting with floating point vectors.
         to_f -> bool32fx4];
    bool32fx4: bool32f, i32x4, i32, 4, bool32fx4_all, bool32fx4_any, x0, x1 | x2, x3
        [/// Convert `self` to a boolean vector for interacting with integer vectors.
         to_i -> bool32ix4];

    bool16ix8: bool16i, i16x8, i16, 8, bool16ix8_all, bool16ix8_any, x0, x1, x2, x3 | x4, x5, x6, x7 [];

    bool8ix16: bool8i, i8x16, i8, 16, bool8ix16_all, bool8ix16_any, x0, x1, x2, x3, x4, x5, x6, x7 | x8, x9, x10, x11, x12, x13, x14, x15 [];
}

impl u32x4 {
    /// Convert each lane to a signed integer.
    #[inline]
    pub fn to_i32(self) -> i32x4 {
        unsafe {simd_cast(self)}
    }
    /// Convert each lane to a 32-bit float.
    #[inline]
    pub fn to_f32(self) -> f32x4 {
        unsafe {simd_cast(self)}
    }
}
impl i32x4 {
    /// Convert each lane to an unsigned integer.
    #[inline]
    pub fn to_u32(self) -> u32x4 {
        unsafe {simd_cast(self)}
    }
    /// Convert each lane to a 32-bit float.
    #[inline]
    pub fn to_f32(self) -> f32x4 {
        unsafe {simd_cast(self)}
    }
}
impl f32x4 {
    /// Compute the square root of each lane.
    #[inline]
    pub fn sqrt(self) -> Self {
        common::f32x4_sqrt(self)
    }
    /// Compute an approximation to the reciprocal of the square root
    /// of `self`, that is, `f32::splat(1.0) / self.sqrt()`.
    ///
    /// The accuracy of this approximation is platform dependent.
    #[inline]
    pub fn approx_rsqrt(self) -> Self {
        common::f32x4_approx_rsqrt(self)
    }
    /// Compute an approximation to the reciprocal of `self`, that is,
    /// `f32::splat(1.0) / self`.
    ///
    /// The accuracy of this approximation is platform dependent.
    #[inline]
    pub fn approx_reciprocal(self) -> Self {
        common::f32x4_approx_reciprocal(self)
    }
    /// Compute the lane-wise maximum of `self` and `other`.
    ///
    /// This is equivalent to the following, but is possibly more
    /// efficient:
    ///
    /// ```rust,ignore
    /// f32x4::new(self.extract(0).max(other.extract(0)),
    ///            self.extract(1).max(other.extract(1)),
    ///            ...)
    /// ```
    #[inline]
    pub fn max(self, other: Self) -> Self {
        common::f32x4_max(self, other)
    }
    /// Compute the lane-wise minimum of `self` and `other`.
    ///
    /// This is equivalent to the following, but is possibly more
    /// efficient:
    ///
    /// ```rust,ignore
    /// f32x4::new(self.extract(0).min(other.extract(0)),
    ///            self.extract(1).min(other.extract(1)),
    ///            ...)
    /// ```
    #[inline]
    pub fn min(self, other: Self) -> Self {
        common::f32x4_min(self, other)
    }
    /// Convert each lane to a signed integer.
    #[inline]
    pub fn to_i32(self) -> i32x4 {
        unsafe {simd_cast(self)}
    }
    /// Convert each lane to an unsigned integer.
    #[inline]
    pub fn to_u32(self) -> u32x4 {
        unsafe {simd_cast(self)}
    }
}

impl i16x8 {
    /// Convert each lane to an unsigned integer.
    #[inline]
    pub fn to_u16(self) -> u16x8 {
        unsafe {simd_cast(self)}
    }
}
impl u16x8 {
    /// Convert each lane to a signed integer.
    #[inline]
    pub fn to_i16(self) -> i16x8 {
        unsafe {simd_cast(self)}
    }
}

impl i8x16 {
    /// Convert each lane to an unsigned integer.
    #[inline]
    pub fn to_u8(self) -> u8x16 {
        unsafe {simd_cast(self)}
    }
}
impl u8x16 {
    /// Convert each lane to a signed integer.
    #[inline]
    pub fn to_i8(self) -> i8x16 {
        unsafe {simd_cast(self)}
    }
}


macro_rules! neg_impls {
    ($zero: expr, $($ty: ident,)*) => {
        $(impl ops::Neg for $ty {
            type Output = Self;
            fn neg(self) -> Self {
                $ty::splat($zero) - self
            }
        })*
    }
}
neg_impls!{
    0,
    i32x4,
    i16x8,
    i8x16,
}
neg_impls! {
    0.0,
    f32x4,
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
    i32x4,
    i16x8,
    i8x16,
    u32x4,
    u16x8,
    u8x16,
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
        i8x16, u8x16, i16x8, u16x8, i32x4, u32x4,
        f32x4;
    Sub (simd_sub, sub):
        i8x16, u8x16, i16x8, u16x8, i32x4, u32x4,
        f32x4;
    Mul (simd_mul, mul):
        i8x16, u8x16, i16x8, u16x8, i32x4, u32x4,
        f32x4;
    Div (simd_div, div): f32x4;

    BitAnd (simd_and, bitand):
        i8x16, u8x16, i16x8, u16x8, i32x4, u32x4,
        bool8ix16, bool16ix8, bool32ix4,
        bool32fx4;
    BitOr (simd_or, bitor):
        i8x16, u8x16, i16x8, u16x8, i32x4, u32x4,
        bool8ix16, bool16ix8, bool32ix4,
        bool32fx4;
    BitXor (simd_xor, bitxor):
        i8x16, u8x16, i16x8, u16x8, i32x4, u32x4,
        bool8ix16, bool16ix8, bool32ix4,
        bool32fx4;
}

macro_rules! shift_one {
    ($ty: ident, $($by: ident),*) => {
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
    i8x16, u8x16, i16x8, u16x8, i32x4, u32x4
}
