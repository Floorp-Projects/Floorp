// Translated from C to Rust. The original C code can be found at
// https://github.com/ulfjack/ryu and carries the following license:
//
// Copyright 2018 Ulf Adams
//
// The contents of this file may be used under the terms of the Apache License,
// Version 2.0.
//
//    (See accompanying file LICENSE-Apache or copy at
//     http://www.apache.org/licenses/LICENSE-2.0)
//
// Alternatively, the contents of this file may be used under the terms of
// the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE-Boost or copy at
//     https://www.boost.org/LICENSE_1_0.txt)
//
// Unless required by applicable law or agreed to in writing, this software
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.

use common::*;
#[cfg(not(integer128))]
use d2s_intrinsics::*;

pub static DOUBLE_POW5_TABLE: [u64; 26] = [
    1,
    5,
    25,
    125,
    625,
    3125,
    15625,
    78125,
    390625,
    1953125,
    9765625,
    48828125,
    244140625,
    1220703125,
    6103515625,
    30517578125,
    152587890625,
    762939453125,
    3814697265625,
    19073486328125,
    95367431640625,
    476837158203125,
    2384185791015625,
    11920928955078125,
    59604644775390625,
    298023223876953125,
];

pub static DOUBLE_POW5_SPLIT2: [(u64, u64); 13] = [
    (0, 72057594037927936),
    (10376293541461622784, 93132257461547851),
    (15052517733678820785, 120370621524202240),
    (6258995034005762182, 77787690973264271),
    (14893927168346708332, 100538234169297439),
    (4272820386026678563, 129942622070561240),
    (7330497575943398595, 83973451344588609),
    (18377130505971182927, 108533142064701048),
    (10038208235822497557, 140275798336537794),
    (7017903361312433648, 90651109995611182),
    (6366496589810271835, 117163813585596168),
    (9264989777501460624, 75715339914673581),
    (17074144231291089770, 97859783203563123),
];

// Unfortunately, the results are sometimes off by one. We use an additional
// lookup table to store those cases and adjust the result.
pub static POW5_OFFSETS: [u32; 13] = [
    0x00000000, 0x00000000, 0x00000000, 0x033c55be, 0x03db77d8, 0x0265ffb2, 0x00000800, 0x01a8ff56,
    0x00000000, 0x0037a200, 0x00004000, 0x03fffffc, 0x00003ffe,
];

pub static DOUBLE_POW5_INV_SPLIT2: [(u64, u64); 13] = [
    (1, 288230376151711744),
    (7661987648932456967, 223007451985306231),
    (12652048002903177473, 172543658669764094),
    (5522544058086115566, 266998379490113760),
    (3181575136763469022, 206579990246952687),
    (4551508647133041040, 159833525776178802),
    (1116074521063664381, 247330401473104534),
    (17400360011128145022, 191362629322552438),
    (9297997190148906106, 148059663038321393),
    (11720143854957885429, 229111231347799689),
    (15401709288678291155, 177266229209635622),
    (3003071137298187333, 274306203439684434),
    (17516772882021341108, 212234145163966538),
];

pub static POW5_INV_OFFSETS: [u32; 20] = [
    0x51505404, 0x55054514, 0x45555545, 0x05511411, 0x00505010, 0x00000004, 0x00000000, 0x00000000,
    0x55555040, 0x00505051, 0x00050040, 0x55554000, 0x51659559, 0x00001000, 0x15000010, 0x55455555,
    0x41404051, 0x00001010, 0x00000014, 0x00000000,
];

// Computes 5^i in the form required by Ryu.
#[cfg(integer128)]
#[cfg_attr(feature = "no-panic", inline)]
pub unsafe fn compute_pow5(i: u32) -> (u64, u64) {
    let base = i / DOUBLE_POW5_TABLE.len() as u32;
    let base2 = base * DOUBLE_POW5_TABLE.len() as u32;
    let offset = i - base2;
    debug_assert!(base < DOUBLE_POW5_SPLIT2.len() as u32);
    let mul = *DOUBLE_POW5_SPLIT2.get_unchecked(base as usize);
    if offset == 0 {
        return mul;
    }
    debug_assert!(offset < DOUBLE_POW5_TABLE.len() as u32);
    let m = *DOUBLE_POW5_TABLE.get_unchecked(offset as usize);
    let b0 = m as u128 * mul.0 as u128;
    let b2 = m as u128 * mul.1 as u128;
    let delta = pow5bits(i as i32) - pow5bits(base2 as i32);
    debug_assert!(base < POW5_OFFSETS.len() as u32);
    let shifted_sum = (b0 >> delta)
        + (b2 << (64 - delta))
        + ((*POW5_OFFSETS.get_unchecked(base as usize) >> offset) & 1) as u128;
    (shifted_sum as u64, (shifted_sum >> 64) as u64)
}

// Computes 5^-i in the form required by Ryu.
#[cfg(integer128)]
#[cfg_attr(feature = "no-panic", inline)]
pub unsafe fn compute_inv_pow5(i: u32) -> (u64, u64) {
    let base = (i + DOUBLE_POW5_TABLE.len() as u32 - 1) / DOUBLE_POW5_TABLE.len() as u32;
    let base2 = base * DOUBLE_POW5_TABLE.len() as u32;
    let offset = base2 - i;
    debug_assert!(base < DOUBLE_POW5_INV_SPLIT2.len() as u32);
    let mul = *DOUBLE_POW5_INV_SPLIT2.get_unchecked(base as usize); // 1/5^base2
    if offset == 0 {
        return mul;
    }
    debug_assert!(offset < DOUBLE_POW5_TABLE.len() as u32);
    let m = *DOUBLE_POW5_TABLE.get_unchecked(offset as usize); // 5^offset
    let b0 = m as u128 * (mul.0 - 1) as u128;
    let b2 = m as u128 * mul.1 as u128; // 1/5^base2 * 5^offset = 1/5^(base2-offset) = 1/5^i
    let delta = pow5bits(base2 as i32) - pow5bits(i as i32);
    debug_assert!(base < POW5_INV_OFFSETS.len() as u32);
    let shifted_sum = ((b0 >> delta) + (b2 << (64 - delta)))
        + 1
        + ((*POW5_INV_OFFSETS.get_unchecked((i / 16) as usize) >> ((i % 16) << 1)) & 3) as u128;
    (shifted_sum as u64, (shifted_sum >> 64) as u64)
}

// Computes 5^i in the form required by Ryu, and stores it in the given pointer.
#[cfg(not(integer128))]
#[cfg_attr(feature = "no-panic", inline)]
pub unsafe fn compute_pow5(i: u32) -> (u64, u64) {
    let base = i / DOUBLE_POW5_TABLE.len() as u32;
    let base2 = base * DOUBLE_POW5_TABLE.len() as u32;
    let offset = i - base2;
    debug_assert!(base < DOUBLE_POW5_SPLIT2.len() as u32);
    let mul = *DOUBLE_POW5_SPLIT2.get_unchecked(base as usize);
    if offset == 0 {
        return mul;
    }
    debug_assert!(offset < DOUBLE_POW5_TABLE.len() as u32);
    let m = *DOUBLE_POW5_TABLE.get_unchecked(offset as usize);
    let (low1, mut high1) = umul128(m, mul.1);
    let (low0, high0) = umul128(m, mul.0);
    let sum = high0 + low1;
    if sum < high0 {
        high1 += 1; // overflow into high1
    }
    // high1 | sum | low0
    let delta = pow5bits(i as i32) - pow5bits(base2 as i32);
    debug_assert!(base < POW5_OFFSETS.len() as u32);
    (
        shiftright128(low0, sum, delta as u32)
            + ((*POW5_OFFSETS.get_unchecked(base as usize) >> offset) & 1) as u64,
        shiftright128(sum, high1, delta as u32),
    )
}

// Computes 5^-i in the form required by Ryu, and stores it in the given pointer.
#[cfg(not(integer128))]
#[cfg_attr(feature = "no-panic", inline)]
pub unsafe fn compute_inv_pow5(i: u32) -> (u64, u64) {
    let base = (i + DOUBLE_POW5_TABLE.len() as u32 - 1) / DOUBLE_POW5_TABLE.len() as u32;
    let base2 = base * DOUBLE_POW5_TABLE.len() as u32;
    let offset = base2 - i;
    debug_assert!(base < DOUBLE_POW5_INV_SPLIT2.len() as u32);
    let mul = *DOUBLE_POW5_INV_SPLIT2.get_unchecked(base as usize); // 1/5^base2
    if offset == 0 {
        return mul;
    }
    debug_assert!(offset < DOUBLE_POW5_TABLE.len() as u32);
    let m = *DOUBLE_POW5_TABLE.get_unchecked(offset as usize);
    let (low1, mut high1) = umul128(m, mul.1);
    let (low0, high0) = umul128(m, mul.0 - 1);
    let sum = high0 + low1;
    if sum < high0 {
        high1 += 1; // overflow into high1
    }
    // high1 | sum | low0
    let delta = pow5bits(base2 as i32) - pow5bits(i as i32);
    debug_assert!(base < POW5_INV_OFFSETS.len() as u32);
    (
        shiftright128(low0, sum, delta as u32)
            + 1
            + ((*POW5_INV_OFFSETS.get_unchecked((i / 16) as usize) >> ((i % 16) << 1)) & 3) as u64,
        shiftright128(sum, high1, delta as u32),
    )
}
