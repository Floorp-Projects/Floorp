//! Vectors of pointers
#[rustfmt::skip]

use crate::*;

impl_const_p!(
    [*const T; 2]: cptrx2, msizex2, usizex2, isizex2 | test_v128 | x0, x1 | From: |
    /// A vector with 2 `*const T` lanes
);

impl_mut_p!(
    [*mut T; 2]: mptrx2, msizex2, usizex2, isizex2 | test_v128 | x0, x1 | From: |
    /// A vector with 2 `*mut T` lanes
);

impl_const_p!(
    [*const T; 4]: cptrx4, msizex4, usizex4, isizex4 | test_v256 | x0, x1, x2, x3 | From: |
    /// A vector with 4 `*const T` lanes
);

impl_mut_p!(
    [*mut T; 4]: mptrx4, msizex4, usizex4, isizex4 | test_v256 | x0, x1, x2, x3 | From: |
    /// A vector with 4 `*mut T` lanes
);

impl_const_p!(
    [*const T; 8]: cptrx8, msizex8, usizex8, isizex8 | test_v512 | x0, x1, x2, x3, x4, x5, x6, x7 | From: |
    /// A vector with 8 `*const T` lanes
);

impl_mut_p!(
    [*mut T; 8]: mptrx8, msizex8, usizex8, isizex8 | test_v512 | x0, x1, x2, x3, x4, x5, x6, x7 | From: |
    /// A vector with 8 `*mut T` lanes
);
