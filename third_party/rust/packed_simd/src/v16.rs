//! 16-bit wide vector types

use crate::*;

impl_i!([i8; 2]: i8x2, m8x2 | i8, u8 | test_v16 | x0, x1 |
        From: |
        /// A 16-bit vector with 2 `i8` lanes.
);
impl_u!([u8; 2]: u8x2, m8x2 | u8, u8 | test_v16 | x0, x1 |
        From: |
        /// A 16-bit vector with 2 `u8` lanes.
);
impl_m!([m8; 2]: m8x2 | i8, u8 | test_v16 | x0, x1 |
        From: m16x2, m32x2, m64x2, m128x2 |
        /// A 16-bit vector mask with 2 `m8` lanes.
);
