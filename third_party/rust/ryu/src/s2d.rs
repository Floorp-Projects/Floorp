use crate::common::*;
use crate::d2s;
use crate::d2s_intrinsics::*;
use crate::parse::Error;
#[cfg(feature = "no-panic")]
use no_panic::no_panic;

const DOUBLE_EXPONENT_BIAS: usize = 1023;

fn floor_log2(value: u64) -> u32 {
    63_u32.wrapping_sub(value.leading_zeros())
}

#[cfg_attr(feature = "no-panic", no_panic)]
pub fn s2d(buffer: &[u8]) -> Result<f64, Error> {
    let len = buffer.len();
    if len == 0 {
        return Err(Error::InputTooShort);
    }

    let mut m10digits = 0;
    let mut e10digits = 0;
    let mut dot_index = len;
    let mut e_index = len;
    let mut m10 = 0u64;
    let mut e10 = 0i32;
    let mut signed_m = false;
    let mut signed_e = false;

    let mut i = 0;
    if unsafe { *buffer.get_unchecked(0) } == b'-' {
        signed_m = true;
        i += 1;
    }

    while let Some(c) = buffer.get(i).copied() {
        if c == b'.' {
            if dot_index != len {
                return Err(Error::MalformedInput);
            }
            dot_index = i;
            i += 1;
            continue;
        }
        if c < b'0' || c > b'9' {
            break;
        }
        if m10digits >= 17 {
            return Err(Error::InputTooLong);
        }
        m10 = 10 * m10 + (c - b'0') as u64;
        if m10 != 0 {
            m10digits += 1;
        }
        i += 1;
    }

    if let Some(b'e') | Some(b'E') = buffer.get(i) {
        e_index = i;
        i += 1;
        match buffer.get(i) {
            Some(b'-') => {
                signed_e = true;
                i += 1;
            }
            Some(b'+') => i += 1,
            _ => {}
        }
        while let Some(c) = buffer.get(i).copied() {
            if c < b'0' || c > b'9' {
                return Err(Error::MalformedInput);
            }
            if e10digits > 3 {
                // TODO: Be more lenient. Return +/-Infinity or +/-0 instead.
                return Err(Error::InputTooLong);
            }
            e10 = 10 * e10 + (c - b'0') as i32;
            if e10 != 0 {
                e10digits += 1;
            }
            i += 1;
        }
    }

    if i < len {
        return Err(Error::MalformedInput);
    }
    if signed_e {
        e10 = -e10;
    }
    e10 -= if dot_index < e_index {
        (e_index - dot_index - 1) as i32
    } else {
        0
    };
    if m10 == 0 {
        return Ok(if signed_m { -0.0 } else { 0.0 });
    }

    if m10digits + e10 <= -324 || m10 == 0 {
        // Number is less than 1e-324, which should be rounded down to 0; return
        // +/-0.0.
        let ieee = (signed_m as u64) << (d2s::DOUBLE_EXPONENT_BITS + d2s::DOUBLE_MANTISSA_BITS);
        return Ok(f64::from_bits(ieee));
    }
    if m10digits + e10 >= 310 {
        // Number is larger than 1e+309, which should be rounded to +/-Infinity.
        let ieee = ((signed_m as u64) << (d2s::DOUBLE_EXPONENT_BITS + d2s::DOUBLE_MANTISSA_BITS))
            | (0x7ff_u64 << d2s::DOUBLE_MANTISSA_BITS);
        return Ok(f64::from_bits(ieee));
    }

    // Convert to binary float m2 * 2^e2, while retaining information about
    // whether the conversion was exact (trailing_zeros).
    let e2: i32;
    let m2: u64;
    let mut trailing_zeros: bool;
    if e10 >= 0 {
        // The length of m * 10^e in bits is:
        //   log2(m10 * 10^e10) = log2(m10) + e10 log2(10) = log2(m10) + e10 + e10 * log2(5)
        //
        // We want to compute the DOUBLE_MANTISSA_BITS + 1 top-most bits (+1 for
        // the implicit leading one in IEEE format). We therefore choose a
        // binary output exponent of
        //   log2(m10 * 10^e10) - (DOUBLE_MANTISSA_BITS + 1).
        //
        // We use floor(log2(5^e10)) so that we get at least this many bits;
        // better to have an additional bit than to not have enough bits.
        e2 = floor_log2(m10)
            .wrapping_add(e10 as u32)
            .wrapping_add(log2_pow5(e10) as u32)
            .wrapping_sub(d2s::DOUBLE_MANTISSA_BITS + 1) as i32;

        // We now compute [m10 * 10^e10 / 2^e2] = [m10 * 5^e10 / 2^(e2-e10)].
        // To that end, we use the DOUBLE_POW5_SPLIT table.
        let j = e2
            .wrapping_sub(e10)
            .wrapping_sub(ceil_log2_pow5(e10))
            .wrapping_add(d2s::DOUBLE_POW5_BITCOUNT);
        debug_assert!(j >= 0);
        debug_assert!(e10 < d2s::DOUBLE_POW5_SPLIT.len() as i32);
        m2 = mul_shift_64(
            m10,
            unsafe { d2s::DOUBLE_POW5_SPLIT.get_unchecked(e10 as usize) },
            j as u32,
        );

        // We also compute if the result is exact, i.e.,
        //   [m10 * 10^e10 / 2^e2] == m10 * 10^e10 / 2^e2.
        // This can only be the case if 2^e2 divides m10 * 10^e10, which in turn
        // requires that the largest power of 2 that divides m10 + e10 is
        // greater than e2. If e2 is less than e10, then the result must be
        // exact. Otherwise we use the existing multiple_of_power_of_2 function.
        trailing_zeros =
            e2 < e10 || e2 - e10 < 64 && multiple_of_power_of_2(m10, (e2 - e10) as u32);
    } else {
        e2 = floor_log2(m10)
            .wrapping_add(e10 as u32)
            .wrapping_sub(ceil_log2_pow5(-e10) as u32)
            .wrapping_sub(d2s::DOUBLE_MANTISSA_BITS + 1) as i32;
        let j = e2
            .wrapping_sub(e10)
            .wrapping_add(ceil_log2_pow5(-e10))
            .wrapping_sub(1)
            .wrapping_add(d2s::DOUBLE_POW5_INV_BITCOUNT);
        debug_assert!(-e10 < d2s::DOUBLE_POW5_INV_SPLIT.len() as i32);
        m2 = mul_shift_64(
            m10,
            unsafe { d2s::DOUBLE_POW5_INV_SPLIT.get_unchecked(-e10 as usize) },
            j as u32,
        );
        trailing_zeros = multiple_of_power_of_5(m10, -e10 as u32);
    }

    // Compute the final IEEE exponent.
    let mut ieee_e2 = i32::max(0, e2 + DOUBLE_EXPONENT_BIAS as i32 + floor_log2(m2) as i32) as u32;

    if ieee_e2 > 0x7fe {
        // Final IEEE exponent is larger than the maximum representable; return +/-Infinity.
        let ieee = ((signed_m as u64) << (d2s::DOUBLE_EXPONENT_BITS + d2s::DOUBLE_MANTISSA_BITS))
            | (0x7ff_u64 << d2s::DOUBLE_MANTISSA_BITS);
        return Ok(f64::from_bits(ieee));
    }

    // We need to figure out how much we need to shift m2. The tricky part is
    // that we need to take the final IEEE exponent into account, so we need to
    // reverse the bias and also special-case the value 0.
    let shift = if ieee_e2 == 0 { 1 } else { ieee_e2 as i32 }
        .wrapping_sub(e2)
        .wrapping_sub(DOUBLE_EXPONENT_BIAS as i32)
        .wrapping_sub(d2s::DOUBLE_MANTISSA_BITS as i32);
    debug_assert!(shift >= 0);

    // We need to round up if the exact value is more than 0.5 above the value
    // we computed. That's equivalent to checking if the last removed bit was 1
    // and either the value was not just trailing zeros or the result would
    // otherwise be odd.
    //
    // We need to update trailing_zeros given that we have the exact output
    // exponent ieee_e2 now.
    trailing_zeros &= (m2 & ((1_u64 << (shift - 1)) - 1)) == 0;
    let last_removed_bit = (m2 >> (shift - 1)) & 1;
    let round_up = last_removed_bit != 0 && (!trailing_zeros || ((m2 >> shift) & 1) != 0);

    let mut ieee_m2 = (m2 >> shift).wrapping_add(round_up as u64);
    debug_assert!(ieee_m2 <= 1_u64 << (d2s::DOUBLE_MANTISSA_BITS + 1));
    ieee_m2 &= (1_u64 << d2s::DOUBLE_MANTISSA_BITS) - 1;
    if ieee_m2 == 0 && round_up {
        // Due to how the IEEE represents +/-Infinity, we don't need to check
        // for overflow here.
        ieee_e2 += 1;
    }
    let ieee = ((((signed_m as u64) << d2s::DOUBLE_EXPONENT_BITS) | ieee_e2 as u64)
        << d2s::DOUBLE_MANTISSA_BITS)
        | ieee_m2;
    Ok(f64::from_bits(ieee))
}
