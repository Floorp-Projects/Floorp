use crate::std_alloc::{Cow, Vec};
use core::cmp;
use core::cmp::Ordering::{self, Equal, Greater, Less};
use core::iter::repeat;
use core::mem;
use num_traits::{One, PrimInt, Zero};

use crate::biguint::biguint_from_vec;
use crate::biguint::BigUint;

use crate::bigint::BigInt;
use crate::bigint::Sign;
use crate::bigint::Sign::{Minus, NoSign, Plus};

use crate::big_digit::{self, BigDigit, DoubleBigDigit, SignedDoubleBigDigit};

// Generic functions for add/subtract/multiply with carry/borrow:

// Add with carry:
#[inline]
fn adc(a: BigDigit, b: BigDigit, acc: &mut DoubleBigDigit) -> BigDigit {
    *acc += DoubleBigDigit::from(a);
    *acc += DoubleBigDigit::from(b);
    let lo = *acc as BigDigit;
    *acc >>= big_digit::BITS;
    lo
}

// Subtract with borrow:
#[inline]
fn sbb(a: BigDigit, b: BigDigit, acc: &mut SignedDoubleBigDigit) -> BigDigit {
    *acc += SignedDoubleBigDigit::from(a);
    *acc -= SignedDoubleBigDigit::from(b);
    let lo = *acc as BigDigit;
    *acc >>= big_digit::BITS;
    lo
}

#[inline]
pub(crate) fn mac_with_carry(
    a: BigDigit,
    b: BigDigit,
    c: BigDigit,
    acc: &mut DoubleBigDigit,
) -> BigDigit {
    *acc += DoubleBigDigit::from(a);
    *acc += DoubleBigDigit::from(b) * DoubleBigDigit::from(c);
    let lo = *acc as BigDigit;
    *acc >>= big_digit::BITS;
    lo
}

#[inline]
pub(crate) fn mul_with_carry(a: BigDigit, b: BigDigit, acc: &mut DoubleBigDigit) -> BigDigit {
    *acc += DoubleBigDigit::from(a) * DoubleBigDigit::from(b);
    let lo = *acc as BigDigit;
    *acc >>= big_digit::BITS;
    lo
}

/// Divide a two digit numerator by a one digit divisor, returns quotient and remainder:
///
/// Note: the caller must ensure that both the quotient and remainder will fit into a single digit.
/// This is _not_ true for an arbitrary numerator/denominator.
///
/// (This function also matches what the x86 divide instruction does).
#[inline]
fn div_wide(hi: BigDigit, lo: BigDigit, divisor: BigDigit) -> (BigDigit, BigDigit) {
    debug_assert!(hi < divisor);

    let lhs = big_digit::to_doublebigdigit(hi, lo);
    let rhs = DoubleBigDigit::from(divisor);
    ((lhs / rhs) as BigDigit, (lhs % rhs) as BigDigit)
}

/// For small divisors, we can divide without promoting to `DoubleBigDigit` by
/// using half-size pieces of digit, like long-division.
#[inline]
fn div_half(rem: BigDigit, digit: BigDigit, divisor: BigDigit) -> (BigDigit, BigDigit) {
    use crate::big_digit::{HALF, HALF_BITS};
    use num_integer::Integer;

    debug_assert!(rem < divisor && divisor <= HALF);
    let (hi, rem) = ((rem << HALF_BITS) | (digit >> HALF_BITS)).div_rem(&divisor);
    let (lo, rem) = ((rem << HALF_BITS) | (digit & HALF)).div_rem(&divisor);
    ((hi << HALF_BITS) | lo, rem)
}

#[inline]
pub(crate) fn div_rem_digit(mut a: BigUint, b: BigDigit) -> (BigUint, BigDigit) {
    let mut rem = 0;

    if b <= big_digit::HALF {
        for d in a.data.iter_mut().rev() {
            let (q, r) = div_half(rem, *d, b);
            *d = q;
            rem = r;
        }
    } else {
        for d in a.data.iter_mut().rev() {
            let (q, r) = div_wide(rem, *d, b);
            *d = q;
            rem = r;
        }
    }

    (a.normalized(), rem)
}

#[inline]
pub(crate) fn rem_digit(a: &BigUint, b: BigDigit) -> BigDigit {
    let mut rem = 0;

    if b <= big_digit::HALF {
        for &digit in a.data.iter().rev() {
            let (_, r) = div_half(rem, digit, b);
            rem = r;
        }
    } else {
        for &digit in a.data.iter().rev() {
            let (_, r) = div_wide(rem, digit, b);
            rem = r;
        }
    }

    rem
}

/// Two argument addition of raw slices, `a += b`, returning the carry.
///
/// This is used when the data `Vec` might need to resize to push a non-zero carry, so we perform
/// the addition first hoping that it will fit.
///
/// The caller _must_ ensure that `a` is at least as long as `b`.
#[inline]
pub(crate) fn __add2(a: &mut [BigDigit], b: &[BigDigit]) -> BigDigit {
    debug_assert!(a.len() >= b.len());

    let mut carry = 0;
    let (a_lo, a_hi) = a.split_at_mut(b.len());

    for (a, b) in a_lo.iter_mut().zip(b) {
        *a = adc(*a, *b, &mut carry);
    }

    if carry != 0 {
        for a in a_hi {
            *a = adc(*a, 0, &mut carry);
            if carry == 0 {
                break;
            }
        }
    }

    carry as BigDigit
}

/// Two argument addition of raw slices:
/// a += b
///
/// The caller _must_ ensure that a is big enough to store the result - typically this means
/// resizing a to max(a.len(), b.len()) + 1, to fit a possible carry.
pub(crate) fn add2(a: &mut [BigDigit], b: &[BigDigit]) {
    let carry = __add2(a, b);

    debug_assert!(carry == 0);
}

pub(crate) fn sub2(a: &mut [BigDigit], b: &[BigDigit]) {
    let mut borrow = 0;

    let len = cmp::min(a.len(), b.len());
    let (a_lo, a_hi) = a.split_at_mut(len);
    let (b_lo, b_hi) = b.split_at(len);

    for (a, b) in a_lo.iter_mut().zip(b_lo) {
        *a = sbb(*a, *b, &mut borrow);
    }

    if borrow != 0 {
        for a in a_hi {
            *a = sbb(*a, 0, &mut borrow);
            if borrow == 0 {
                break;
            }
        }
    }

    // note: we're _required_ to fail on underflow
    assert!(
        borrow == 0 && b_hi.iter().all(|x| *x == 0),
        "Cannot subtract b from a because b is larger than a."
    );
}

// Only for the Sub impl. `a` and `b` must have same length.
#[inline]
pub(crate) fn __sub2rev(a: &[BigDigit], b: &mut [BigDigit]) -> BigDigit {
    debug_assert!(b.len() == a.len());

    let mut borrow = 0;

    for (ai, bi) in a.iter().zip(b) {
        *bi = sbb(*ai, *bi, &mut borrow);
    }

    borrow as BigDigit
}

pub(crate) fn sub2rev(a: &[BigDigit], b: &mut [BigDigit]) {
    debug_assert!(b.len() >= a.len());

    let len = cmp::min(a.len(), b.len());
    let (a_lo, a_hi) = a.split_at(len);
    let (b_lo, b_hi) = b.split_at_mut(len);

    let borrow = __sub2rev(a_lo, b_lo);

    assert!(a_hi.is_empty());

    // note: we're _required_ to fail on underflow
    assert!(
        borrow == 0 && b_hi.iter().all(|x| *x == 0),
        "Cannot subtract b from a because b is larger than a."
    );
}

pub(crate) fn sub_sign(a: &[BigDigit], b: &[BigDigit]) -> (Sign, BigUint) {
    // Normalize:
    let a = &a[..a.iter().rposition(|&x| x != 0).map_or(0, |i| i + 1)];
    let b = &b[..b.iter().rposition(|&x| x != 0).map_or(0, |i| i + 1)];

    match cmp_slice(a, b) {
        Greater => {
            let mut a = a.to_vec();
            sub2(&mut a, b);
            (Plus, biguint_from_vec(a))
        }
        Less => {
            let mut b = b.to_vec();
            sub2(&mut b, a);
            (Minus, biguint_from_vec(b))
        }
        _ => (NoSign, Zero::zero()),
    }
}

/// Three argument multiply accumulate:
/// acc += b * c
pub(crate) fn mac_digit(acc: &mut [BigDigit], b: &[BigDigit], c: BigDigit) {
    if c == 0 {
        return;
    }

    let mut carry = 0;
    let (a_lo, a_hi) = acc.split_at_mut(b.len());

    for (a, &b) in a_lo.iter_mut().zip(b) {
        *a = mac_with_carry(*a, b, c, &mut carry);
    }

    let mut a = a_hi.iter_mut();
    while carry != 0 {
        let a = a.next().expect("carry overflow during multiplication!");
        *a = adc(*a, 0, &mut carry);
    }
}

fn bigint_from_slice(slice: &[BigDigit]) -> BigInt {
    BigInt::from(biguint_from_vec(slice.to_vec()))
}

/// Three argument multiply accumulate:
/// acc += b * c
fn mac3(acc: &mut [BigDigit], b: &[BigDigit], c: &[BigDigit]) {
    let (x, y) = if b.len() < c.len() { (b, c) } else { (c, b) };

    // We use three algorithms for different input sizes.
    //
    // - For small inputs, long multiplication is fastest.
    // - Next we use Karatsuba multiplication (Toom-2), which we have optimized
    //   to avoid unnecessary allocations for intermediate values.
    // - For the largest inputs we use Toom-3, which better optimizes the
    //   number of operations, but uses more temporary allocations.
    //
    // The thresholds are somewhat arbitrary, chosen by evaluating the results
    // of `cargo bench --bench bigint multiply`.

    if x.len() <= 32 {
        // Long multiplication:
        for (i, xi) in x.iter().enumerate() {
            mac_digit(&mut acc[i..], y, *xi);
        }
    } else if x.len() <= 256 {
        /*
         * Karatsuba multiplication:
         *
         * The idea is that we break x and y up into two smaller numbers that each have about half
         * as many digits, like so (note that multiplying by b is just a shift):
         *
         * x = x0 + x1 * b
         * y = y0 + y1 * b
         *
         * With some algebra, we can compute x * y with three smaller products, where the inputs to
         * each of the smaller products have only about half as many digits as x and y:
         *
         * x * y = (x0 + x1 * b) * (y0 + y1 * b)
         *
         * x * y = x0 * y0
         *       + x0 * y1 * b
         *       + x1 * y0 * b
         *       + x1 * y1 * b^2
         *
         * Let p0 = x0 * y0 and p2 = x1 * y1:
         *
         * x * y = p0
         *       + (x0 * y1 + x1 * y0) * b
         *       + p2 * b^2
         *
         * The real trick is that middle term:
         *
         *         x0 * y1 + x1 * y0
         *
         *       = x0 * y1 + x1 * y0 - p0 + p0 - p2 + p2
         *
         *       = x0 * y1 + x1 * y0 - x0 * y0 - x1 * y1 + p0 + p2
         *
         * Now we complete the square:
         *
         *       = -(x0 * y0 - x0 * y1 - x1 * y0 + x1 * y1) + p0 + p2
         *
         *       = -((x1 - x0) * (y1 - y0)) + p0 + p2
         *
         * Let p1 = (x1 - x0) * (y1 - y0), and substitute back into our original formula:
         *
         * x * y = p0
         *       + (p0 + p2 - p1) * b
         *       + p2 * b^2
         *
         * Where the three intermediate products are:
         *
         * p0 = x0 * y0
         * p1 = (x1 - x0) * (y1 - y0)
         * p2 = x1 * y1
         *
         * In doing the computation, we take great care to avoid unnecessary temporary variables
         * (since creating a BigUint requires a heap allocation): thus, we rearrange the formula a
         * bit so we can use the same temporary variable for all the intermediate products:
         *
         * x * y = p2 * b^2 + p2 * b
         *       + p0 * b + p0
         *       - p1 * b
         *
         * The other trick we use is instead of doing explicit shifts, we slice acc at the
         * appropriate offset when doing the add.
         */

        /*
         * When x is smaller than y, it's significantly faster to pick b such that x is split in
         * half, not y:
         */
        let b = x.len() / 2;
        let (x0, x1) = x.split_at(b);
        let (y0, y1) = y.split_at(b);

        /*
         * We reuse the same BigUint for all the intermediate multiplies and have to size p
         * appropriately here: x1.len() >= x0.len and y1.len() >= y0.len():
         */
        let len = x1.len() + y1.len() + 1;
        let mut p = BigUint { data: vec![0; len] };

        // p2 = x1 * y1
        mac3(&mut p.data[..], x1, y1);

        // Not required, but the adds go faster if we drop any unneeded 0s from the end:
        p.normalize();

        add2(&mut acc[b..], &p.data[..]);
        add2(&mut acc[b * 2..], &p.data[..]);

        // Zero out p before the next multiply:
        p.data.truncate(0);
        p.data.extend(repeat(0).take(len));

        // p0 = x0 * y0
        mac3(&mut p.data[..], x0, y0);
        p.normalize();

        add2(&mut acc[..], &p.data[..]);
        add2(&mut acc[b..], &p.data[..]);

        // p1 = (x1 - x0) * (y1 - y0)
        // We do this one last, since it may be negative and acc can't ever be negative:
        let (j0_sign, j0) = sub_sign(x1, x0);
        let (j1_sign, j1) = sub_sign(y1, y0);

        match j0_sign * j1_sign {
            Plus => {
                p.data.truncate(0);
                p.data.extend(repeat(0).take(len));

                mac3(&mut p.data[..], &j0.data[..], &j1.data[..]);
                p.normalize();

                sub2(&mut acc[b..], &p.data[..]);
            }
            Minus => {
                mac3(&mut acc[b..], &j0.data[..], &j1.data[..]);
            }
            NoSign => (),
        }
    } else {
        // Toom-3 multiplication:
        //
        // Toom-3 is like Karatsuba above, but dividing the inputs into three parts.
        // Both are instances of Toom-Cook, using `k=3` and `k=2` respectively.
        //
        // The general idea is to treat the large integers digits as
        // polynomials of a certain degree and determine the coefficients/digits
        // of the product of the two via interpolation of the polynomial product.
        let i = y.len() / 3 + 1;

        let x0_len = cmp::min(x.len(), i);
        let x1_len = cmp::min(x.len() - x0_len, i);

        let y0_len = i;
        let y1_len = cmp::min(y.len() - y0_len, i);

        // Break x and y into three parts, representating an order two polynomial.
        // t is chosen to be the size of a digit so we can use faster shifts
        // in place of multiplications.
        //
        // x(t) = x2*t^2 + x1*t + x0
        let x0 = bigint_from_slice(&x[..x0_len]);
        let x1 = bigint_from_slice(&x[x0_len..x0_len + x1_len]);
        let x2 = bigint_from_slice(&x[x0_len + x1_len..]);

        // y(t) = y2*t^2 + y1*t + y0
        let y0 = bigint_from_slice(&y[..y0_len]);
        let y1 = bigint_from_slice(&y[y0_len..y0_len + y1_len]);
        let y2 = bigint_from_slice(&y[y0_len + y1_len..]);

        // Let w(t) = x(t) * y(t)
        //
        // This gives us the following order-4 polynomial.
        //
        // w(t) = w4*t^4 + w3*t^3 + w2*t^2 + w1*t + w0
        //
        // We need to find the coefficients w4, w3, w2, w1 and w0. Instead
        // of simply multiplying the x and y in total, we can evaluate w
        // at 5 points. An n-degree polynomial is uniquely identified by (n + 1)
        // points.
        //
        // It is arbitrary as to what points we evaluate w at but we use the
        // following.
        //
        // w(t) at t = 0, 1, -1, -2 and inf
        //
        // The values for w(t) in terms of x(t)*y(t) at these points are:
        //
        // let a = w(0)   = x0 * y0
        // let b = w(1)   = (x2 + x1 + x0) * (y2 + y1 + y0)
        // let c = w(-1)  = (x2 - x1 + x0) * (y2 - y1 + y0)
        // let d = w(-2)  = (4*x2 - 2*x1 + x0) * (4*y2 - 2*y1 + y0)
        // let e = w(inf) = x2 * y2 as t -> inf

        // x0 + x2, avoiding temporaries
        let p = &x0 + &x2;

        // y0 + y2, avoiding temporaries
        let q = &y0 + &y2;

        // x2 - x1 + x0, avoiding temporaries
        let p2 = &p - &x1;

        // y2 - y1 + y0, avoiding temporaries
        let q2 = &q - &y1;

        // w(0)
        let r0 = &x0 * &y0;

        // w(inf)
        let r4 = &x2 * &y2;

        // w(1)
        let r1 = (p + x1) * (q + y1);

        // w(-1)
        let r2 = &p2 * &q2;

        // w(-2)
        let r3 = ((p2 + x2) * 2 - x0) * ((q2 + y2) * 2 - y0);

        // Evaluating these points gives us the following system of linear equations.
        //
        //  0  0  0  0  1 | a
        //  1  1  1  1  1 | b
        //  1 -1  1 -1  1 | c
        // 16 -8  4 -2  1 | d
        //  1  0  0  0  0 | e
        //
        // The solved equation (after gaussian elimination or similar)
        // in terms of its coefficients:
        //
        // w0 = w(0)
        // w1 = w(0)/2 + w(1)/3 - w(-1) + w(2)/6 - 2*w(inf)
        // w2 = -w(0) + w(1)/2 + w(-1)/2 - w(inf)
        // w3 = -w(0)/2 + w(1)/6 + w(-1)/2 - w(1)/6
        // w4 = w(inf)
        //
        // This particular sequence is given by Bodrato and is an interpolation
        // of the above equations.
        let mut comp3: BigInt = (r3 - &r1) / 3;
        let mut comp1: BigInt = (r1 - &r2) / 2;
        let mut comp2: BigInt = r2 - &r0;
        comp3 = (&comp2 - comp3) / 2 + &r4 * 2;
        comp2 += &comp1 - &r4;
        comp1 -= &comp3;

        // Recomposition. The coefficients of the polynomial are now known.
        //
        // Evaluate at w(t) where t is our given base to get the result.
        let bits = u64::from(big_digit::BITS) * i as u64;
        let result = r0
            + (comp1 << bits)
            + (comp2 << (2 * bits))
            + (comp3 << (3 * bits))
            + (r4 << (4 * bits));
        let result_pos = result.to_biguint().unwrap();
        add2(&mut acc[..], &result_pos.data);
    }
}

pub(crate) fn mul3(x: &[BigDigit], y: &[BigDigit]) -> BigUint {
    let len = x.len() + y.len() + 1;
    let mut prod = BigUint { data: vec![0; len] };

    mac3(&mut prod.data[..], x, y);
    prod.normalized()
}

pub(crate) fn scalar_mul(a: &mut [BigDigit], b: BigDigit) -> BigDigit {
    let mut carry = 0;
    for a in a.iter_mut() {
        *a = mul_with_carry(*a, b, &mut carry);
    }
    carry as BigDigit
}

pub(crate) fn div_rem(mut u: BigUint, mut d: BigUint) -> (BigUint, BigUint) {
    if d.is_zero() {
        panic!("attempt to divide by zero")
    }
    if u.is_zero() {
        return (Zero::zero(), Zero::zero());
    }

    if d.data.len() == 1 {
        if d.data == [1] {
            return (u, Zero::zero());
        }
        let (div, rem) = div_rem_digit(u, d.data[0]);
        // reuse d
        d.data.clear();
        d += rem;
        return (div, d);
    }

    // Required or the q_len calculation below can underflow:
    match u.cmp(&d) {
        Less => return (Zero::zero(), u),
        Equal => {
            u.set_one();
            return (u, Zero::zero());
        }
        Greater => {} // Do nothing
    }

    // This algorithm is from Knuth, TAOCP vol 2 section 4.3, algorithm D:
    //
    // First, normalize the arguments so the highest bit in the highest digit of the divisor is
    // set: the main loop uses the highest digit of the divisor for generating guesses, so we
    // want it to be the largest number we can efficiently divide by.
    //
    let shift = d.data.last().unwrap().leading_zeros() as usize;

    let (q, r) = if shift == 0 {
        // no need to clone d
        div_rem_core(u, &d)
    } else {
        div_rem_core(u << shift, &(d << shift))
    };
    // renormalize the remainder
    (q, r >> shift)
}

pub(crate) fn div_rem_ref(u: &BigUint, d: &BigUint) -> (BigUint, BigUint) {
    if d.is_zero() {
        panic!("attempt to divide by zero")
    }
    if u.is_zero() {
        return (Zero::zero(), Zero::zero());
    }

    if d.data.len() == 1 {
        if d.data == [1] {
            return (u.clone(), Zero::zero());
        }

        let (div, rem) = div_rem_digit(u.clone(), d.data[0]);
        return (div, rem.into());
    }

    // Required or the q_len calculation below can underflow:
    match u.cmp(d) {
        Less => return (Zero::zero(), u.clone()),
        Equal => return (One::one(), Zero::zero()),
        Greater => {} // Do nothing
    }

    // This algorithm is from Knuth, TAOCP vol 2 section 4.3, algorithm D:
    //
    // First, normalize the arguments so the highest bit in the highest digit of the divisor is
    // set: the main loop uses the highest digit of the divisor for generating guesses, so we
    // want it to be the largest number we can efficiently divide by.
    //
    let shift = d.data.last().unwrap().leading_zeros() as usize;

    let (q, r) = if shift == 0 {
        // no need to clone d
        div_rem_core(u.clone(), d)
    } else {
        div_rem_core(u << shift, &(d << shift))
    };
    // renormalize the remainder
    (q, r >> shift)
}

/// an implementation of Knuth, TAOCP vol 2 section 4.3, algorithm D
///
/// # Correctness
///
/// This function requires the following conditions to run correctly and/or effectively
///
/// - `a > b`
/// - `d.data.len() > 1`
/// - `d.data.last().unwrap().leading_zeros() == 0`
fn div_rem_core(mut a: BigUint, b: &BigUint) -> (BigUint, BigUint) {
    // The algorithm works by incrementally calculating "guesses", q0, for part of the
    // remainder. Once we have any number q0 such that q0 * b <= a, we can set
    //
    //     q += q0
    //     a -= q0 * b
    //
    // and then iterate until a < b. Then, (q, a) will be our desired quotient and remainder.
    //
    // q0, our guess, is calculated by dividing the last few digits of a by the last digit of b
    // - this should give us a guess that is "close" to the actual quotient, but is possibly
    // greater than the actual quotient. If q0 * b > a, we simply use iterated subtraction
    // until we have a guess such that q0 * b <= a.
    //

    let bn = *b.data.last().unwrap();
    let q_len = a.data.len() - b.data.len() + 1;
    let mut q = BigUint {
        data: vec![0; q_len],
    };

    // We reuse the same temporary to avoid hitting the allocator in our inner loop - this is
    // sized to hold a0 (in the common case; if a particular digit of the quotient is zero a0
    // can be bigger).
    //
    let mut tmp = BigUint {
        data: Vec::with_capacity(2),
    };

    for j in (0..q_len).rev() {
        /*
         * When calculating our next guess q0, we don't need to consider the digits below j
         * + b.data.len() - 1: we're guessing digit j of the quotient (i.e. q0 << j) from
         * digit bn of the divisor (i.e. bn << (b.data.len() - 1) - so the product of those
         * two numbers will be zero in all digits up to (j + b.data.len() - 1).
         */
        let offset = j + b.data.len() - 1;
        if offset >= a.data.len() {
            continue;
        }

        /* just avoiding a heap allocation: */
        let mut a0 = tmp;
        a0.data.truncate(0);
        a0.data.extend(a.data[offset..].iter().cloned());

        /*
         * q0 << j * big_digit::BITS is our actual quotient estimate - we do the shifts
         * implicitly at the end, when adding and subtracting to a and q. Not only do we
         * save the cost of the shifts, the rest of the arithmetic gets to work with
         * smaller numbers.
         */
        let (mut q0, _) = div_rem_digit(a0, bn);
        let mut prod = b * &q0;

        while cmp_slice(&prod.data[..], &a.data[j..]) == Greater {
            q0 -= 1u32;
            prod -= b;
        }

        add2(&mut q.data[j..], &q0.data[..]);
        sub2(&mut a.data[j..], &prod.data[..]);
        a.normalize();

        tmp = q0;
    }

    debug_assert!(a < *b);

    (q.normalized(), a)
}

/// Find last set bit
/// fls(0) == 0, fls(u32::MAX) == 32
pub(crate) fn fls<T: PrimInt>(v: T) -> u8 {
    mem::size_of::<T>() as u8 * 8 - v.leading_zeros() as u8
}

pub(crate) fn ilog2<T: PrimInt>(v: T) -> u8 {
    fls(v) - 1
}

#[inline]
pub(crate) fn biguint_shl<T: PrimInt>(n: Cow<'_, BigUint>, shift: T) -> BigUint {
    if shift < T::zero() {
        panic!("attempt to shift left with negative");
    }
    if n.is_zero() {
        return n.into_owned();
    }
    let bits = T::from(big_digit::BITS).unwrap();
    let digits = (shift / bits).to_usize().expect("capacity overflow");
    let shift = (shift % bits).to_u8().unwrap();
    biguint_shl2(n, digits, shift)
}

fn biguint_shl2(n: Cow<'_, BigUint>, digits: usize, shift: u8) -> BigUint {
    let mut data = match digits {
        0 => n.into_owned().data,
        _ => {
            let len = digits.saturating_add(n.data.len() + 1);
            let mut data = Vec::with_capacity(len);
            data.extend(repeat(0).take(digits));
            data.extend(n.data.iter());
            data
        }
    };

    if shift > 0 {
        let mut carry = 0;
        let carry_shift = big_digit::BITS as u8 - shift;
        for elem in data[digits..].iter_mut() {
            let new_carry = *elem >> carry_shift;
            *elem = (*elem << shift) | carry;
            carry = new_carry;
        }
        if carry != 0 {
            data.push(carry);
        }
    }

    biguint_from_vec(data)
}

#[inline]
pub(crate) fn biguint_shr<T: PrimInt>(n: Cow<'_, BigUint>, shift: T) -> BigUint {
    if shift < T::zero() {
        panic!("attempt to shift right with negative");
    }
    if n.is_zero() {
        return n.into_owned();
    }
    let bits = T::from(big_digit::BITS).unwrap();
    let digits = (shift / bits).to_usize().unwrap_or(core::usize::MAX);
    let shift = (shift % bits).to_u8().unwrap();
    biguint_shr2(n, digits, shift)
}

fn biguint_shr2(n: Cow<'_, BigUint>, digits: usize, shift: u8) -> BigUint {
    if digits >= n.data.len() {
        let mut n = n.into_owned();
        n.set_zero();
        return n;
    }
    let mut data = match n {
        Cow::Borrowed(n) => n.data[digits..].to_vec(),
        Cow::Owned(mut n) => {
            n.data.drain(..digits);
            n.data
        }
    };

    if shift > 0 {
        let mut borrow = 0;
        let borrow_shift = big_digit::BITS as u8 - shift;
        for elem in data.iter_mut().rev() {
            let new_borrow = *elem << borrow_shift;
            *elem = (*elem >> shift) | borrow;
            borrow = new_borrow;
        }
    }

    biguint_from_vec(data)
}

pub(crate) fn cmp_slice(a: &[BigDigit], b: &[BigDigit]) -> Ordering {
    debug_assert!(a.last() != Some(&0));
    debug_assert!(b.last() != Some(&0));

    match Ord::cmp(&a.len(), &b.len()) {
        Equal => Iterator::cmp(a.iter().rev(), b.iter().rev()),
        other => other,
    }
}

#[cfg(test)]
mod algorithm_tests {
    use crate::big_digit::BigDigit;
    use crate::{BigInt, BigUint};
    use num_traits::Num;

    #[test]
    fn test_sub_sign() {
        use super::sub_sign;

        fn sub_sign_i(a: &[BigDigit], b: &[BigDigit]) -> BigInt {
            let (sign, val) = sub_sign(a, b);
            BigInt::from_biguint(sign, val)
        }

        let a = BigUint::from_str_radix("265252859812191058636308480000000", 10).unwrap();
        let b = BigUint::from_str_radix("26525285981219105863630848000000", 10).unwrap();
        let a_i = BigInt::from(a.clone());
        let b_i = BigInt::from(b.clone());

        assert_eq!(sub_sign_i(&a.data[..], &b.data[..]), &a_i - &b_i);
        assert_eq!(sub_sign_i(&b.data[..], &a.data[..]), &b_i - &a_i);
    }
}
