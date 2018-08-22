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

// Returns (lo, hi).
#[cfg_attr(feature = "no-panic", inline)]
pub fn umul128(a: u64, b: u64) -> (u64, u64) {
    let a_lo = a as u32;
    let a_hi = (a >> 32) as u32;
    let b_lo = b as u32;
    let b_hi = (b >> 32) as u32;

    let b00 = a_lo as u64 * b_lo as u64;
    let b01 = a_lo as u64 * b_hi as u64;
    let b10 = a_hi as u64 * b_lo as u64;
    let b11 = a_hi as u64 * b_hi as u64;

    let b00_lo = b00 as u32;
    let b00_hi = (b00 >> 32) as u32;

    let mid1 = b10 + b00_hi as u64;
    let mid1_lo = mid1 as u32;
    let mid1_hi = (mid1 >> 32) as u32;

    let mid2 = b01 + mid1_lo as u64;
    let mid2_lo = mid2 as u32;
    let mid2_hi = (mid2 >> 32) as u32;

    let p_hi = b11 + mid1_hi as u64 + mid2_hi as u64;
    let p_lo = ((mid2_lo as u64) << 32) + b00_lo as u64;

    (p_lo, p_hi)
}

#[cfg_attr(feature = "no-panic", inline)]
pub fn shiftright128(lo: u64, hi: u64, dist: u32) -> u64 {
    // We don't need to handle the case dist >= 64 here (see above).
    debug_assert!(dist > 0);
    debug_assert!(dist < 64);
    (hi << (64 - dist)) | (lo >> dist)
}
