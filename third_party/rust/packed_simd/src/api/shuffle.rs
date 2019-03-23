//! Implements portable vector shuffles with immediate indices.

// FIXME: comprehensive tests
// https://github.com/rust-lang-nursery/packed_simd/issues/20

/// Shuffles vector elements.
///
/// This macro returns a new vector that contains a shuffle of the elements in
/// one (`shuffle!(vec, [indices...])`) or two (`shuffle!(vec0, vec1,
/// [indices...])`) input vectors.
///
/// The type of `vec0` and `vec1` must be equal, and the element type of the
/// resulting vector is the element type of the input vector.
///
/// The number of `indices` must be a power-of-two in range `[0, 64)`, since
/// currently, the largest vector supported by the library has 64 lanes. The
/// length of the resulting vector equals the number of indices provided.
///
/// The indices must be in range `[0, M * N)` where `M` is the number of input
/// vectors (`1` or `2`) and `N` is the number of lanes of the input vectors.
/// The indices `i` in range `[0, N)` refer to the `i`-th element of `vec0`,
/// while the indices in range `[N, 2*N)` refer to the `i - N`-th element of
/// `vec1`.
///
/// # Examples
///
/// Shuffling elements of two vectors:
///
/// ```
/// # #[macro_use]
/// # extern crate packed_simd;
/// # use packed_simd::*;
/// # fn main() {
/// // Shuffle allows reordering the elements:
/// let x = i32x4::new(1, 2, 3, 4);
/// let y = i32x4::new(5, 6, 7, 8);
/// let r = shuffle!(x, y, [4, 0, 5, 1]);
/// assert_eq!(r, i32x4::new(5, 1, 6, 2));
///
/// // The resulting vector can als be smaller than the input:
/// let r = shuffle!(x, y, [1, 6]);
/// assert_eq!(r, i32x2::new(2, 7));
///
/// // Or larger:
/// let r = shuffle!(x, y, [1, 3, 4, 2, 1, 7, 2, 2]);
/// assert_eq!(r, i32x8::new(2, 4, 5, 3, 2, 8, 3, 3));
/// // At most 2 * the number of lanes in the input vector.
/// # }
/// ```
///
/// Shuffling elements of one vector:
///
/// ```
/// # #[macro_use]
/// # extern crate packed_simd;
/// # use packed_simd::*;
/// # fn main() {
/// // Shuffle allows reordering the elements of a vector:
/// let x = i32x4::new(1, 2, 3, 4);
/// let r = shuffle!(x, [2, 1, 3, 0]);
/// assert_eq!(r, i32x4::new(3, 2, 4, 1));
///
/// // The resulting vector can be smaller than the input:
/// let r = shuffle!(x, [1, 3]);
/// assert_eq!(r, i32x2::new(2, 4));
///
/// // Equal:
/// let r = shuffle!(x, [1, 3, 2, 0]);
/// assert_eq!(r, i32x4::new(2, 4, 3, 1));
///
/// // Or larger:
/// let r = shuffle!(x, [1, 3, 2, 2, 1, 3, 2, 2]);
/// assert_eq!(r, i32x8::new(2, 4, 3, 3, 2, 4, 3, 3));
/// // At most 2 * the number of lanes in the input vector.
/// # }
/// ```
#[macro_export]
macro_rules! shuffle {
    ($vec0:expr, $vec1:expr, [$l0:expr, $l1:expr]) => {{
        #[allow(unused_unsafe)]
        unsafe {
            $crate::Simd($crate::__shuffle_vector2(
                $vec0.0,
                $vec1.0,
                [$l0, $l1],
            ))
        }
    }};
    ($vec0:expr, $vec1:expr, [$l0:expr, $l1:expr, $l2:expr, $l3:expr]) => {{
        #[allow(unused_unsafe)]
        unsafe {
            $crate::Simd($crate::__shuffle_vector4(
                $vec0.0,
                $vec1.0,
                [$l0, $l1, $l2, $l3],
            ))
        }
    }};
    ($vec0:expr, $vec1:expr,
     [$l0:expr, $l1:expr, $l2:expr, $l3:expr,
      $l4:expr, $l5:expr, $l6:expr, $l7:expr]) => {{
        #[allow(unused_unsafe)]
        unsafe {
            $crate::Simd($crate::__shuffle_vector8(
                $vec0.0,
                $vec1.0,
                [$l0, $l1, $l2, $l3, $l4, $l5, $l6, $l7],
            ))
        }
    }};
    ($vec0:expr, $vec1:expr,
     [$l0:expr, $l1:expr, $l2:expr, $l3:expr,
      $l4:expr, $l5:expr, $l6:expr, $l7:expr,
      $l8:expr, $l9:expr, $l10:expr, $l11:expr,
      $l12:expr, $l13:expr, $l14:expr, $l15:expr]) => {{
        #[allow(unused_unsafe)]
        unsafe {
            $crate::Simd($crate::__shuffle_vector16(
                $vec0.0,
                $vec1.0,
                [
                    $l0, $l1, $l2, $l3, $l4, $l5, $l6, $l7, $l8, $l9, $l10,
                    $l11, $l12, $l13, $l14, $l15,
                ],
            ))
        }
    }};
    ($vec0:expr, $vec1:expr,
     [$l0:expr, $l1:expr, $l2:expr, $l3:expr,
      $l4:expr, $l5:expr, $l6:expr, $l7:expr,
      $l8:expr, $l9:expr, $l10:expr, $l11:expr,
      $l12:expr, $l13:expr, $l14:expr, $l15:expr,
      $l16:expr, $l17:expr, $l18:expr, $l19:expr,
      $l20:expr, $l21:expr, $l22:expr, $l23:expr,
      $l24:expr, $l25:expr, $l26:expr, $l27:expr,
      $l28:expr, $l29:expr, $l30:expr, $l31:expr]) => {{
        #[allow(unused_unsafe)]
        unsafe {
            $crate::Simd($crate::__shuffle_vector32(
                $vec0.0,
                $vec1.0,
                [
                    $l0, $l1, $l2, $l3, $l4, $l5, $l6, $l7, $l8, $l9, $l10,
                    $l11, $l12, $l13, $l14, $l15, $l16, $l17, $l18, $l19,
                    $l20, $l21, $l22, $l23, $l24, $l25, $l26, $l27, $l28,
                    $l29, $l30, $l31,
                ],
            ))
        }
    }};
    ($vec0:expr, $vec1:expr,
     [$l0:expr, $l1:expr, $l2:expr, $l3:expr,
      $l4:expr, $l5:expr, $l6:expr, $l7:expr,
      $l8:expr, $l9:expr, $l10:expr, $l11:expr,
      $l12:expr, $l13:expr, $l14:expr, $l15:expr,
      $l16:expr, $l17:expr, $l18:expr, $l19:expr,
      $l20:expr, $l21:expr, $l22:expr, $l23:expr,
      $l24:expr, $l25:expr, $l26:expr, $l27:expr,
      $l28:expr, $l29:expr, $l30:expr, $l31:expr,
      $l32:expr, $l33:expr, $l34:expr, $l35:expr,
      $l36:expr, $l37:expr, $l38:expr, $l39:expr,
      $l40:expr, $l41:expr, $l42:expr, $l43:expr,
      $l44:expr, $l45:expr, $l46:expr, $l47:expr,
      $l48:expr, $l49:expr, $l50:expr, $l51:expr,
      $l52:expr, $l53:expr, $l54:expr, $l55:expr,
      $l56:expr, $l57:expr, $l58:expr, $l59:expr,
      $l60:expr, $l61:expr, $l62:expr, $l63:expr]) => {{
        #[allow(unused_unsafe)]
        unsafe {
            $crate::Simd($crate::__shuffle_vector64(
                $vec0.0,
                $vec1.0,
                [
                    $l0, $l1, $l2, $l3, $l4, $l5, $l6, $l7, $l8, $l9, $l10,
                    $l11, $l12, $l13, $l14, $l15, $l16, $l17, $l18, $l19,
                    $l20, $l21, $l22, $l23, $l24, $l25, $l26, $l27, $l28,
                    $l29, $l30, $l31, $l32, $l33, $l34, $l35, $l36, $l37,
                    $l38, $l39, $l40, $l41, $l42, $l43, $l44, $l45, $l46,
                    $l47, $l48, $l49, $l50, $l51, $l52, $l53, $l54, $l55,
                    $l56, $l57, $l58, $l59, $l60, $l61, $l62, $l63,
                ],
            ))
        }
     }};
    ($vec:expr, [$($l:expr),*]) => {
        match $vec {
            v => shuffle!(v, v, [$($l),*])
        }
    };
}
