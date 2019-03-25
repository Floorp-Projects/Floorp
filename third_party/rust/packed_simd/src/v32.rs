//! 32-bit wide vector types

use crate::*;

impl_i!([i8; 4]: i8x4, m8x4 | i8 | test_v32 | x0, x1, x2, x3 |
        From: |
        /// A 32-bit vector with 4 `i8` lanes.
);
impl_u!([u8; 4]: u8x4, m8x4 | u8 | test_v32 | x0, x1, x2, x3 |
        From: |
        /// A 32-bit vector with 4 `u8` lanes.
);
impl_m!([m8; 4]: m8x4 | i8 | test_v32 | x0, x1, x2, x3 |
        From: m16x4, m32x4, m64x4 |
        /// A 32-bit vector mask with 4 `m8` lanes.
);

impl_i!([i16; 2]: i16x2, m16x2 | i16 | test_v32 | x0, x1 |
        From: i8x2, u8x2 |
        /// A 32-bit vector with 2 `i16` lanes.
);
impl_u!([u16; 2]: u16x2, m16x2 | u16 | test_v32 | x0, x1 |
        From: u8x2 |
        /// A 32-bit vector with 2 `u16` lanes.
);
impl_m!([m16; 2]: m16x2 | i16 | test_v32 | x0, x1 |
        From: m8x2, m32x2, m64x2, m128x2 |
        /// A 32-bit vector mask with 2 `m16` lanes.
);
