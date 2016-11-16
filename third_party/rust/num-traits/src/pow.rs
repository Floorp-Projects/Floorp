use std::ops::Mul;
use {One, CheckedMul};

/// Raises a value to the power of exp, using exponentiation by squaring.
///
/// # Example
///
/// ```rust
/// use num_traits::pow;
///
/// assert_eq!(pow(2i8, 4), 16);
/// assert_eq!(pow(6u8, 3), 216);
/// ```
#[inline]
pub fn pow<T: Clone + One + Mul<T, Output = T>>(mut base: T, mut exp: usize) -> T {
    if exp == 0 { return T::one() }

    while exp & 1 == 0 {
        base = base.clone() * base;
        exp >>= 1;
    }
    if exp == 1 { return base }

    let mut acc = base.clone();
    while exp > 1 {
        exp >>= 1;
        base = base.clone() * base;
        if exp & 1 == 1 {
            acc = acc * base.clone();
        }
    }
    acc
}

/// Raises a value to the power of exp, returning `None` if an overflow occurred.
///
/// Otherwise same as the `pow` function.
///
/// # Example
///
/// ```rust
/// use num_traits::checked_pow;
///
/// assert_eq!(checked_pow(2i8, 4), Some(16));
/// assert_eq!(checked_pow(7i8, 8), None);
/// assert_eq!(checked_pow(7u32, 8), Some(5_764_801));
/// ```
#[inline]
pub fn checked_pow<T: Clone + One + CheckedMul>(mut base: T, mut exp: usize) -> Option<T> {
    if exp == 0 { return Some(T::one()) }

    macro_rules! optry {
        ( $ expr : expr ) => {
            if let Some(val) = $expr { val } else { return None }
        }
    }

    while exp & 1 == 0 {
        base = optry!(base.checked_mul(&base));
        exp >>= 1;
    }
    if exp == 1 { return Some(base) }

    let mut acc = base.clone();
    while exp > 1 {
        exp >>= 1;
        base = optry!(base.checked_mul(&base));
        if exp & 1 == 1 {
            acc = optry!(acc.checked_mul(&base));
        }
    }
    Some(acc)
}
