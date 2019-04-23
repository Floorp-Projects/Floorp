//! Shared mathematical utility functions.

/// Cut value to be inside given range
///
/// ```
/// use image::math::utils;
///
/// assert_eq!(utils::clamp(-5, 0, 10),  0);
/// assert_eq!(utils::clamp( 6, 0, 10),  6);
/// assert_eq!(utils::clamp(15, 0, 10), 10);
/// ```
#[inline]
pub fn clamp<N>(a: N, min: N, max: N) -> N
where
    N: PartialOrd,
{
    if a < min {
        return min;
    }
    if a > max {
        return max;
    }
    a
}
