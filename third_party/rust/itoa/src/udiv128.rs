/// Multiply unsigned 128 bit integers, return upper 128 bits of the result
#[inline]
fn u128_mulhi(x: u128, y: u128) -> u128 {
    let x_lo = x as u64;
    let x_hi = (x >> 64) as u64;
    let y_lo = y as u64;
    let y_hi = (y >> 64) as u64;

    // handle possibility of overflow
    let carry = (x_lo as u128 * y_lo as u128) >> 64;
    let m = x_lo as u128 * y_hi as u128 + carry;
    let high1 = m >> 64;

    let m_lo = m as u64;
    let high2 = x_hi as u128 * y_lo as u128 + m_lo as u128 >> 64;

    x_hi as u128 * y_hi as u128 + high1 + high2
}

/// Divide `n` by 1e19 and return quotient and remainder
///
/// Integer division algorithm is based on the following paper:
///
///   T. Granlund and P. Montgomery, “Division by Invariant Integers Using Multiplication”
///   in Proc. of the SIGPLAN94 Conference on Programming Language Design and
///   Implementation, 1994, pp. 61–72
///
#[inline]
pub fn udivmod_1e19(n: u128) -> (u128, u64) {
    let d = 10_000_000_000_000_000_000_u64; // 10^19

    let quot = if n < 1 << 83 {
        ((n >> 19) as u64 / (d >> 19)) as u128
    } else {
        let factor =
            (8507059173023461586_u64 as u128) << 64 | 10779635027931437427 as u128;
        u128_mulhi(n, factor) >> 62
    };

    let rem = (n - quot * d as u128) as u64;
    debug_assert_eq!(quot, n / d as u128);
    debug_assert_eq!(rem as u128, n % d as u128);

    (quot, rem)
}
