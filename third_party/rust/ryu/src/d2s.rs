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

use core::{mem, ptr};

use common::*;
#[cfg(not(feature = "small"))]
use d2s_full_table::*;
#[cfg(feature = "small")]
use d2s_small_table::*;
use digit_table::*;
#[cfg(not(integer128))]
use mulshift128::*;

#[cfg(feature = "no-panic")]
use no_panic::no_panic;

pub const DOUBLE_MANTISSA_BITS: u32 = 52;
pub const DOUBLE_EXPONENT_BITS: u32 = 11;

const DOUBLE_POW5_INV_BITCOUNT: i32 = 122;
const DOUBLE_POW5_BITCOUNT: i32 = 121;

#[cfg_attr(feature = "no-panic", inline)]
fn pow5_factor(mut value: u64) -> u32 {
    let mut count = 0u32;
    loop {
        debug_assert!(value != 0);
        let q = value / 5;
        let r = value % 5;
        if r != 0 {
            break;
        }
        value = q;
        count += 1;
    }
    count
}

// Returns true if value is divisible by 5^p.
#[cfg_attr(feature = "no-panic", inline)]
fn multiple_of_power_of_5(value: u64, p: u32) -> bool {
    // I tried a case distinction on p, but there was no performance difference.
    pow5_factor(value) >= p
}

// Returns true if value is divisible by 2^p.
#[cfg_attr(feature = "no-panic", inline)]
fn multiple_of_power_of_2(value: u64, p: u32) -> bool {
    // return __builtin_ctzll(value) >= p;
    (value & ((1u64 << p) - 1)) == 0
}

#[cfg(integer128)]
#[cfg_attr(feature = "no-panic", inline)]
fn mul_shift(m: u64, mul: &(u64, u64), j: u32) -> u64 {
    let b0 = m as u128 * mul.0 as u128;
    let b2 = m as u128 * mul.1 as u128;
    (((b0 >> 64) + b2) >> (j - 64)) as u64
}

#[cfg(integer128)]
#[cfg_attr(feature = "no-panic", inline)]
fn mul_shift_all(
    m: u64,
    mul: &(u64, u64),
    j: u32,
    vp: &mut u64,
    vm: &mut u64,
    mm_shift: u32,
) -> u64 {
    *vp = mul_shift(4 * m + 2, mul, j);
    *vm = mul_shift(4 * m - 1 - mm_shift as u64, mul, j);
    mul_shift(4 * m, mul, j)
}

#[cfg(not(integer128))]
#[cfg_attr(feature = "no-panic", inline)]
fn mul_shift_all(
    mut m: u64,
    mul: &(u64, u64),
    j: u32,
    vp: &mut u64,
    vm: &mut u64,
    mm_shift: u32,
) -> u64 {
    m <<= 1;
    // m is maximum 55 bits
    let (lo, tmp) = umul128(m, mul.0);
    let (mut mid, mut hi) = umul128(m, mul.1);
    mid = mid.wrapping_add(tmp);
    hi = hi.wrapping_add((mid < tmp) as u64); // overflow into hi

    let lo2 = lo.wrapping_add(mul.0);
    let mid2 = mid.wrapping_add(mul.1).wrapping_add((lo2 < lo) as u64);
    let hi2 = hi.wrapping_add((mid2 < mid) as u64);
    *vp = shiftright128(mid2, hi2, j - 64 - 1);

    if mm_shift == 1 {
        let lo3 = lo.wrapping_sub(mul.0);
        let mid3 = mid.wrapping_sub(mul.1).wrapping_sub((lo3 > lo) as u64);
        let hi3 = hi.wrapping_sub((mid3 > mid) as u64);
        *vm = shiftright128(mid3, hi3, j - 64 - 1);
    } else {
        let lo3 = lo + lo;
        let mid3 = mid.wrapping_add(mid).wrapping_add((lo3 < lo) as u64);
        let hi3 = hi.wrapping_add(hi).wrapping_add((mid3 < mid) as u64);
        let lo4 = lo3.wrapping_sub(mul.0);
        let mid4 = mid3.wrapping_sub(mul.1).wrapping_sub((lo4 > lo3) as u64);
        let hi4 = hi3.wrapping_sub((mid4 > mid3) as u64);
        *vm = shiftright128(mid4, hi4, j - 64);
    }

    shiftright128(mid, hi, j - 64 - 1)
}

#[cfg_attr(feature = "no-panic", inline)]
pub fn decimal_length(v: u64) -> u32 {
    // This is slightly faster than a loop.
    // The average output length is 16.38 digits, so we check high-to-low.
    // Function precondition: v is not an 18, 19, or 20-digit number.
    // (17 digits are sufficient for round-tripping.)
    debug_assert!(v < 100000000000000000);

    if v >= 10000000000000000 {
        17
    } else if v >= 1000000000000000 {
        16
    } else if v >= 100000000000000 {
        15
    } else if v >= 10000000000000 {
        14
    } else if v >= 1000000000000 {
        13
    } else if v >= 100000000000 {
        12
    } else if v >= 10000000000 {
        11
    } else if v >= 1000000000 {
        10
    } else if v >= 100000000 {
        9
    } else if v >= 10000000 {
        8
    } else if v >= 1000000 {
        7
    } else if v >= 100000 {
        6
    } else if v >= 10000 {
        5
    } else if v >= 1000 {
        4
    } else if v >= 100 {
        3
    } else if v >= 10 {
        2
    } else {
        1
    }
}

// A floating decimal representing m * 10^e.
pub struct FloatingDecimal64 {
    pub mantissa: u64,
    pub exponent: i32,
}

#[cfg_attr(feature = "no-panic", inline)]
pub fn d2d(ieee_mantissa: u64, ieee_exponent: u32) -> FloatingDecimal64 {
    let bias = (1u32 << (DOUBLE_EXPONENT_BITS - 1)) - 1;

    let (e2, m2) = if ieee_exponent == 0 {
        (
            // We subtract 2 so that the bounds computation has 2 additional bits.
            1 - bias as i32 - DOUBLE_MANTISSA_BITS as i32 - 2,
            ieee_mantissa,
        )
    } else {
        (
            ieee_exponent as i32 - bias as i32 - DOUBLE_MANTISSA_BITS as i32 - 2,
            (1u64 << DOUBLE_MANTISSA_BITS) | ieee_mantissa,
        )
    };
    let even = (m2 & 1) == 0;
    let accept_bounds = even;

    // Step 2: Determine the interval of legal decimal representations.
    let mv = 4 * m2;
    // Implicit bool -> int conversion. True is 1, false is 0.
    let mm_shift = (ieee_mantissa != 0 || ieee_exponent <= 1) as u32;
    // We would compute mp and mm like this:
    // uint64_t mp = 4 * m2 + 2;
    // uint64_t mm = mv - 1 - mm_shift;

    // Step 3: Convert to a decimal power base using 128-bit arithmetic.
    let mut vr: u64;
    let mut vp: u64 = unsafe { mem::uninitialized() };
    let mut vm: u64 = unsafe { mem::uninitialized() };
    let e10: i32;
    let mut vm_is_trailing_zeros = false;
    let mut vr_is_trailing_zeros = false;
    if e2 >= 0 {
        // I tried special-casing q == 0, but there was no effect on performance.
        // This expression is slightly faster than max(0, log10_pow2(e2) - 1).
        let q = (log10_pow2(e2) - (e2 > 3) as i32) as u32;
        e10 = q as i32;
        let k = DOUBLE_POW5_INV_BITCOUNT + pow5bits(q as i32) as i32 - 1;
        let i = -e2 + q as i32 + k;
        vr = mul_shift_all(
            m2,
            #[cfg(feature = "small")]
            unsafe {
                &compute_inv_pow5(q)
            },
            #[cfg(not(feature = "small"))]
            unsafe {
                debug_assert!(q < DOUBLE_POW5_INV_SPLIT.len() as u32);
                DOUBLE_POW5_INV_SPLIT.get_unchecked(q as usize)
            },
            i as u32,
            &mut vp,
            &mut vm,
            mm_shift,
        );
        if q <= 21 {
            // This should use q <= 22, but I think 21 is also safe. Smaller values
            // may still be safe, but it's more difficult to reason about them.
            // Only one of mp, mv, and mm can be a multiple of 5, if any.
            if mv % 5 == 0 {
                vr_is_trailing_zeros = multiple_of_power_of_5(mv, q);
            } else if accept_bounds {
                // Same as min(e2 + (~mm & 1), pow5_factor(mm)) >= q
                // <=> e2 + (~mm & 1) >= q && pow5_factor(mm) >= q
                // <=> true && pow5_factor(mm) >= q, since e2 >= q.
                vm_is_trailing_zeros = multiple_of_power_of_5(mv - 1 - mm_shift as u64, q);
            } else {
                // Same as min(e2 + 1, pow5_factor(mp)) >= q.
                vp -= multiple_of_power_of_5(mv + 2, q) as u64;
            }
        }
    } else {
        // This expression is slightly faster than max(0, log10_pow5(-e2) - 1).
        let q = (log10_pow5(-e2) - (-e2 > 1) as i32) as u32;
        e10 = q as i32 + e2;
        let i = -e2 - q as i32;
        let k = pow5bits(i) as i32 - DOUBLE_POW5_BITCOUNT;
        let j = q as i32 - k;
        vr = mul_shift_all(
            m2,
            #[cfg(feature = "small")]
            unsafe {
                &compute_pow5(i as u32)
            },
            #[cfg(not(feature = "small"))]
            unsafe {
                debug_assert!(i < DOUBLE_POW5_SPLIT.len() as i32);
                DOUBLE_POW5_SPLIT.get_unchecked(i as usize)
            },
            j as u32,
            &mut vp,
            &mut vm,
            mm_shift,
        );
        if q <= 1 {
            // {vr,vp,vm} is trailing zeros if {mv,mp,mm} has at least q trailing 0 bits.
            // mv = 4 * m2, so it always has at least two trailing 0 bits.
            vr_is_trailing_zeros = true;
            if accept_bounds {
                // mm = mv - 1 - mm_shift, so it has 1 trailing 0 bit iff mm_shift == 1.
                vm_is_trailing_zeros = mm_shift == 1;
            } else {
                // mp = mv + 2, so it always has at least one trailing 0 bit.
                vp -= 1;
            }
        } else if q < 63 {
            // TODO(ulfjack): Use a tighter bound here.
            // We need to compute min(ntz(mv), pow5_factor(mv) - e2) >= q - 1
            // <=> ntz(mv) >= q - 1  &&  pow5_factor(mv) - e2 >= q - 1
            // <=> ntz(mv) >= q - 1    (e2 is negative and -e2 >= q)
            // <=> (mv & ((1 << (q - 1)) - 1)) == 0
            // We also need to make sure that the left shift does not overflow.
            vr_is_trailing_zeros = multiple_of_power_of_2(mv, q - 1);
        }
    }

    // Step 4: Find the shortest decimal representation in the interval of legal representations.
    let mut removed = 0u32;
    let mut last_removed_digit = 0u8;
    // On average, we remove ~2 digits.
    let output = if vm_is_trailing_zeros || vr_is_trailing_zeros {
        // General case, which happens rarely (~0.7%).
        while vp / 10 > vm / 10 {
            vm_is_trailing_zeros &= vm - (vm / 10) * 10 == 0;
            vr_is_trailing_zeros &= last_removed_digit == 0;
            last_removed_digit = (vr % 10) as u8;
            vr /= 10;
            vp /= 10;
            vm /= 10;
            removed += 1;
        }
        if vm_is_trailing_zeros {
            while vm % 10 == 0 {
                vr_is_trailing_zeros &= last_removed_digit == 0;
                last_removed_digit = (vr % 10) as u8;
                vr /= 10;
                vp /= 10;
                vm /= 10;
                removed += 1;
            }
        }
        if vr_is_trailing_zeros && last_removed_digit == 5 && vr % 2 == 0 {
            // Round even if the exact number is .....50..0.
            last_removed_digit = 4;
        }
        // We need to take vr + 1 if vr is outside bounds or we need to round up.
        vr + ((vr == vm && (!accept_bounds || !vm_is_trailing_zeros)) || last_removed_digit >= 5)
            as u64
    } else {
        // Specialized for the common case (~99.3%). Percentages below are relative to this.
        let mut round_up = false;
        // Optimization: remove two digits at a time (~86.2%).
        if vp / 100 > vm / 100 {
            round_up = vr % 100 >= 50;
            vr /= 100;
            vp /= 100;
            vm /= 100;
            removed += 2;
        }
        // Loop iterations below (approximately), without optimization above:
        // 0: 0.03%, 1: 13.8%, 2: 70.6%, 3: 14.0%, 4: 1.40%, 5: 0.14%, 6+: 0.02%
        // Loop iterations below (approximately), with optimization above:
        // 0: 70.6%, 1: 27.8%, 2: 1.40%, 3: 0.14%, 4+: 0.02%
        while vp / 10 > vm / 10 {
            round_up = vr % 10 >= 5;
            vr /= 10;
            vp /= 10;
            vm /= 10;
            removed += 1;
        }
        // We need to take vr + 1 if vr is outside bounds or we need to round up.
        vr + (vr == vm || round_up) as u64
    };
    let exp = e10 + removed as i32;

    FloatingDecimal64 {
        exponent: exp,
        mantissa: output,
    }
}

#[cfg_attr(feature = "no-panic", inline)]
unsafe fn to_chars(v: FloatingDecimal64, sign: bool, result: *mut u8) -> usize {
    // Step 5: Print the decimal representation.
    let mut index = 0isize;
    if sign {
        *result.offset(index) = b'-';
        index += 1;
    }

    let mut output = v.mantissa;
    let olength = decimal_length(output);

    // Print the decimal digits.
    // The following code is equivalent to:
    // for (uint32_t i = 0; i < olength - 1; ++i) {
    //   const uint32_t c = output % 10; output /= 10;
    //   result[index + olength - i] = (char) ('0' + c);
    // }
    // result[index] = '0' + output % 10;

    let mut i = 0isize;
    // We prefer 32-bit operations, even on 64-bit platforms.
    // We have at most 17 digits, and uint32_t can store 9 digits.
    // If output doesn't fit into uint32_t, we cut off 8 digits,
    // so the rest will fit into uint32_t.
    if (output >> 32) != 0 {
        // Expensive 64-bit division.
        let mut output2 = (output - 100000000 * (output / 100000000)) as u32;
        output /= 100000000;

        let c = output2 % 10000;
        output2 /= 10000;
        let d = output2 % 10000;
        let c0 = (c % 100) << 1;
        let c1 = (c / 100) << 1;
        let d0 = (d % 100) << 1;
        let d1 = (d / 100) << 1;
        ptr::copy_nonoverlapping(
            DIGIT_TABLE.get_unchecked(c0 as usize),
            result.offset(index + olength as isize - i - 1),
            2,
        );
        ptr::copy_nonoverlapping(
            DIGIT_TABLE.get_unchecked(c1 as usize),
            result.offset(index + olength as isize - i - 3),
            2,
        );
        ptr::copy_nonoverlapping(
            DIGIT_TABLE.get_unchecked(d0 as usize),
            result.offset(index + olength as isize - i - 5),
            2,
        );
        ptr::copy_nonoverlapping(
            DIGIT_TABLE.get_unchecked(d1 as usize),
            result.offset(index + olength as isize - i - 7),
            2,
        );
        i += 8;
    }
    let mut output2 = output as u32;
    while output2 >= 10000 {
        let c = (output2 - 10000 * (output2 / 10000)) as u32;
        output2 /= 10000;
        let c0 = (c % 100) << 1;
        let c1 = (c / 100) << 1;
        ptr::copy_nonoverlapping(
            DIGIT_TABLE.get_unchecked(c0 as usize),
            result.offset(index + olength as isize - i - 1),
            2,
        );
        ptr::copy_nonoverlapping(
            DIGIT_TABLE.get_unchecked(c1 as usize),
            result.offset(index + olength as isize - i - 3),
            2,
        );
        i += 4;
    }
    if output2 >= 100 {
        let c = ((output2 % 100) << 1) as u32;
        output2 /= 100;
        ptr::copy_nonoverlapping(
            DIGIT_TABLE.get_unchecked(c as usize),
            result.offset(index + olength as isize - i - 1),
            2,
        );
        i += 2;
    }
    if output2 >= 10 {
        let c = (output2 << 1) as u32;
        // We can't use memcpy here: the decimal dot goes between these two digits.
        *result.offset(index + olength as isize - i) = *DIGIT_TABLE.get_unchecked(c as usize + 1);
        *result.offset(index) = *DIGIT_TABLE.get_unchecked(c as usize);
    } else {
        *result.offset(index) = b'0' + output2 as u8;
    }

    // Print decimal point if needed.
    if olength > 1 {
        *result.offset(index + 1) = b'.';
        index += olength as isize + 1;
    } else {
        index += 1;
    }

    // Print the exponent.
    *result.offset(index) = b'E';
    index += 1;
    let mut exp = v.exponent as i32 + olength as i32 - 1;
    if exp < 0 {
        *result.offset(index) = b'-';
        index += 1;
        exp = -exp;
    }

    if exp >= 100 {
        let c = exp % 10;
        ptr::copy_nonoverlapping(
            DIGIT_TABLE.get_unchecked((2 * (exp / 10)) as usize),
            result.offset(index),
            2,
        );
        *result.offset(index + 2) = b'0' + c as u8;
        index += 3;
    } else if exp >= 10 {
        ptr::copy_nonoverlapping(
            DIGIT_TABLE.get_unchecked((2 * exp) as usize),
            result.offset(index),
            2,
        );
        index += 2;
    } else {
        *result.offset(index) = b'0' + exp as u8;
        index += 1;
    }

    debug_assert!(index <= 24);
    index as usize
}

/// Print f64 to the given buffer and return number of bytes written.
///
/// At most 24 bytes will be written.
///
/// ## Special cases
///
/// This function represents any NaN as `NaN`, positive infinity as `Infinity`,
/// and negative infinity as `-Infinity`.
///
/// ## Safety
///
/// The `result` pointer argument must point to sufficiently many writable bytes
/// to hold Ryū's representation of `f`.
///
/// ## Example
///
/// ```rust
/// let f = 1.234f64;
///
/// unsafe {
///     let mut buffer: [u8; 24] = std::mem::uninitialized();
///     let n = ryu::raw::d2s_buffered_n(f, &mut buffer[0]);
///     let s = std::str::from_utf8_unchecked(&buffer[..n]);
///     assert_eq!(s, "1.234E0");
/// }
/// ```
#[cfg_attr(must_use_return, must_use)]
#[cfg_attr(feature = "no-panic", no_panic)]
pub unsafe fn d2s_buffered_n(f: f64, result: *mut u8) -> usize {
    // Step 1: Decode the floating-point number, and unify normalized and subnormal cases.
    let bits = mem::transmute::<f64, u64>(f);

    // Decode bits into sign, mantissa, and exponent.
    let ieee_sign = ((bits >> (DOUBLE_MANTISSA_BITS + DOUBLE_EXPONENT_BITS)) & 1) != 0;
    let ieee_mantissa = bits & ((1u64 << DOUBLE_MANTISSA_BITS) - 1);
    let ieee_exponent =
        (bits >> DOUBLE_MANTISSA_BITS) as u32 & ((1u32 << DOUBLE_EXPONENT_BITS) - 1);
    // Case distinction; exit early for the easy cases.
    if ieee_exponent == ((1u32 << DOUBLE_EXPONENT_BITS) - 1)
        || (ieee_exponent == 0 && ieee_mantissa == 0)
    {
        return copy_special_str(result, ieee_sign, ieee_exponent != 0, ieee_mantissa != 0);
    }

    let v = d2d(ieee_mantissa, ieee_exponent);
    to_chars(v, ieee_sign, result)
}
