use crate::prelude::*;
use num_traits::pow::Pow;

const TWO: Decimal = Decimal::from_parts_raw(2, 0, 0, 0);
const PI: Decimal = Decimal::from_parts_raw(1102470953, 185874565, 1703060790, 1835008);
const EXP_TOLERANCE: Decimal = Decimal::from_parts(2, 0, 0, false, 7);

// Table representing {index}!
const FACTORIAL: [Decimal; 28] = [
    Decimal::from_parts(1, 0, 0, false, 0),
    Decimal::from_parts(1, 0, 0, false, 0),
    Decimal::from_parts(2, 0, 0, false, 0),
    Decimal::from_parts(6, 0, 0, false, 0),
    Decimal::from_parts(24, 0, 0, false, 0),
    // 5!
    Decimal::from_parts(120, 0, 0, false, 0),
    Decimal::from_parts(720, 0, 0, false, 0),
    Decimal::from_parts(5040, 0, 0, false, 0),
    Decimal::from_parts(40320, 0, 0, false, 0),
    Decimal::from_parts(362880, 0, 0, false, 0),
    // 10!
    Decimal::from_parts(3628800, 0, 0, false, 0),
    Decimal::from_parts(39916800, 0, 0, false, 0),
    Decimal::from_parts(479001600, 0, 0, false, 0),
    Decimal::from_parts(1932053504, 1, 0, false, 0),
    Decimal::from_parts(1278945280, 20, 0, false, 0),
    // 15!
    Decimal::from_parts(2004310016, 304, 0, false, 0),
    Decimal::from_parts(2004189184, 4871, 0, false, 0),
    Decimal::from_parts(4006445056, 82814, 0, false, 0),
    Decimal::from_parts(3396534272, 1490668, 0, false, 0),
    Decimal::from_parts(109641728, 28322707, 0, false, 0),
    // 20!
    Decimal::from_parts(2192834560, 566454140, 0, false, 0),
    Decimal::from_parts(3099852800, 3305602358, 2, false, 0),
    Decimal::from_parts(3772252160, 4003775155, 60, false, 0),
    Decimal::from_parts(862453760, 1892515369, 1401, false, 0),
    Decimal::from_parts(3519021056, 2470695900, 33634, false, 0),
    // 25!
    Decimal::from_parts(2076180480, 1637855376, 840864, false, 0),
    Decimal::from_parts(2441084928, 3929534124, 21862473, false, 0),
    Decimal::from_parts(1484783616, 3018206259, 590286795, false, 0),
];

/// Trait exposing various mathematical operations that can be applied using a Decimal. This is only
/// present when the `maths` feature has been enabled.
pub trait MathematicalOps {
    /// The estimated exponential function, e<sup>x</sup>. Stops calculating when it is within
    /// tolerance of roughly `0.0000002`.
    fn exp(&self) -> Decimal;

    /// The estimated exponential function, e<sup>x</sup>. Stops calculating when it is within
    /// tolerance of roughly `0.0000002`. Returns `None` on overflow.
    fn checked_exp(&self) -> Option<Decimal>;

    /// The estimated exponential function, e<sup>x</sup> using the `tolerance` provided as a hint
    /// as to when to stop calculating. A larger tolerance will cause the number to stop calculating
    /// sooner at the potential cost of a slightly less accurate result.
    fn exp_with_tolerance(&self, tolerance: Decimal) -> Decimal;

    /// The estimated exponential function, e<sup>x</sup> using the `tolerance` provided as a hint
    /// as to when to stop calculating. A larger tolerance will cause the number to stop calculating
    /// sooner at the potential cost of a slightly less accurate result.
    /// Returns `None` on overflow.
    fn checked_exp_with_tolerance(&self, tolerance: Decimal) -> Option<Decimal>;

    /// Raise self to the given integer exponent: x<sup>y</sup>
    fn powi(&self, exp: i64) -> Decimal;

    /// Raise self to the given integer exponent x<sup>y</sup> returning `None` on overflow.
    fn checked_powi(&self, exp: i64) -> Option<Decimal>;

    /// Raise self to the given unsigned integer exponent: x<sup>y</sup>
    fn powu(&self, exp: u64) -> Decimal;

    /// Raise self to the given unsigned integer exponent x<sup>y</sup> returning `None` on overflow.
    fn checked_powu(&self, exp: u64) -> Option<Decimal>;

    /// Raise self to the given floating point exponent: x<sup>y</sup>
    fn powf(&self, exp: f64) -> Decimal;

    /// Raise self to the given floating point exponent x<sup>y</sup> returning `None` on overflow.
    fn checked_powf(&self, exp: f64) -> Option<Decimal>;

    /// Raise self to the given Decimal exponent: x<sup>y</sup>. If `exp` is not whole then the approximation
    /// e<sup>y*ln(x)</sup> is used.
    fn powd(&self, exp: Decimal) -> Decimal;

    /// Raise self to the given Decimal exponent x<sup>y</sup> returning `None` on overflow.
    /// If `exp` is not whole then the approximation e<sup>y*ln(x)</sup> is used.
    fn checked_powd(&self, exp: Decimal) -> Option<Decimal>;

    /// The square root of a Decimal. Uses a standard Babylonian method.
    fn sqrt(&self) -> Option<Decimal>;

    /// The natural logarithm for a Decimal. Uses a [fast estimation algorithm](https://en.wikipedia.org/wiki/Natural_logarithm#High_precision)
    /// This is more accurate on larger numbers and less on numbers less than 1.
    fn ln(&self) -> Decimal;

    /// Abramowitz Approximation of Error Function from [wikipedia](https://en.wikipedia.org/wiki/Error_function#Numerical_approximations)
    fn erf(&self) -> Decimal;

    /// The Cumulative distribution function for a Normal distribution
    fn norm_cdf(&self) -> Decimal;

    /// The Probability density function for a Normal distribution.
    fn norm_pdf(&self) -> Decimal;

    /// The Probability density function for a Normal distribution returning `None` on overflow.
    fn checked_norm_pdf(&self) -> Option<Decimal>;
}

impl MathematicalOps for Decimal {
    fn exp(&self) -> Decimal {
        self.exp_with_tolerance(EXP_TOLERANCE)
    }

    fn checked_exp(&self) -> Option<Decimal> {
        self.checked_exp_with_tolerance(EXP_TOLERANCE)
    }

    fn exp_with_tolerance(&self, tolerance: Decimal) -> Decimal {
        match self.checked_exp_with_tolerance(tolerance) {
            Some(d) => d,
            None => {
                if self.is_sign_negative() {
                    panic!("Exp underflowed")
                } else {
                    panic!("Exp overflowed")
                }
            }
        }
    }

    fn checked_exp_with_tolerance(&self, tolerance: Decimal) -> Option<Decimal> {
        if self.is_zero() {
            return Some(Decimal::ONE);
        }
        if self.is_sign_negative() {
            let mut flipped = *self;
            flipped.set_sign_positive(true);
            let exp = flipped.checked_exp_with_tolerance(tolerance)?;
            return Decimal::ONE.checked_div(exp);
        }

        let mut term = *self;
        let mut result = self + Decimal::ONE;

        for factorial in FACTORIAL.iter().skip(2) {
            term = self.checked_mul(term)?;
            let next = result + (term / factorial);
            let diff = (next - result).abs();
            result = next;
            if diff <= tolerance {
                break;
            }
        }

        Some(result)
    }

    fn powi(&self, exp: i64) -> Decimal {
        match self.checked_powi(exp) {
            Some(result) => result,
            None => panic!("Pow overflowed"),
        }
    }

    fn checked_powi(&self, exp: i64) -> Option<Decimal> {
        // For negative exponents we change x^-y into 1 / x^y.
        // Otherwise, we calculate a standard unsigned exponent
        if exp >= 0 {
            return self.checked_powu(exp as u64);
        }

        // Get the unsigned exponent
        let exp = exp.unsigned_abs();
        let pow = match self.checked_powu(exp) {
            Some(v) => v,
            None => return None,
        };
        Decimal::ONE.checked_div(pow)
    }

    fn powu(&self, exp: u64) -> Decimal {
        match self.checked_powu(exp) {
            Some(result) => result,
            None => panic!("Pow overflowed"),
        }
    }

    fn checked_powu(&self, exp: u64) -> Option<Decimal> {
        match exp {
            0 => Some(Decimal::ONE),
            1 => Some(*self),
            2 => self.checked_mul(*self),
            _ => {
                // Get the squared value
                let squared = match self.checked_mul(*self) {
                    Some(s) => s,
                    None => return None,
                };
                // Square self once and make an infinite sized iterator of the square.
                let iter = core::iter::repeat(squared);

                // We then take half of the exponent to create a finite iterator and then multiply those together.
                let mut product = Decimal::ONE;
                for x in iter.take((exp >> 1) as usize) {
                    match product.checked_mul(x) {
                        Some(r) => product = r,
                        None => return None,
                    };
                }

                // If the exponent is odd we still need to multiply once more
                if exp & 0x1 > 0 {
                    match self.checked_mul(product) {
                        Some(p) => product = p,
                        None => return None,
                    }
                }
                product.normalize_assign();
                Some(product)
            }
        }
    }

    fn powf(&self, exp: f64) -> Decimal {
        match self.checked_powf(exp) {
            Some(result) => result,
            None => panic!("Pow overflowed"),
        }
    }

    fn checked_powf(&self, exp: f64) -> Option<Decimal> {
        let exp = match Decimal::from_f64(exp) {
            Some(f) => f,
            None => return None,
        };
        self.checked_powd(exp)
    }

    fn powd(&self, exp: Decimal) -> Decimal {
        match self.checked_powd(exp) {
            Some(result) => result,
            None => panic!("Pow overflowed"),
        }
    }

    fn checked_powd(&self, exp: Decimal) -> Option<Decimal> {
        if exp.is_zero() {
            return Some(Decimal::ONE);
        }
        if self.is_zero() {
            return Some(Decimal::ZERO);
        }
        if self.is_one() {
            return Some(Decimal::ONE);
        }
        if exp.is_one() {
            return Some(*self);
        }

        // If the scale is 0 then it's a trivial calculation
        let exp = exp.normalize();
        if exp.scale() == 0 {
            if exp.mid() != 0 || exp.hi() != 0 {
                // Exponent way too big
                return None;
            }

            if exp.is_sign_negative() {
                return self.checked_powi(-(exp.lo() as i64));
            } else {
                return self.checked_powu(exp.lo() as u64);
            }
        }

        // We do some approximations since we've got a decimal exponent.
        // For positive bases: a^b = exp(b*ln(a))
        let negative = self.is_sign_negative();
        let e = match self.abs().ln().checked_mul(exp) {
            Some(e) => e,
            None => return None,
        };
        let mut result = e.checked_exp()?;
        result.set_sign_negative(negative);
        Some(result)
    }

    fn sqrt(&self) -> Option<Decimal> {
        if self.is_sign_negative() {
            return None;
        }

        if self.is_zero() {
            return Some(Decimal::ZERO);
        }

        // Start with an arbitrary number as the first guess
        let mut result = self / TWO;
        // Too small to represent, so we start with self
        // Future iterations could actually avoid using a decimal altogether and use a buffered
        // vector, only combining back into a decimal on return
        if result.is_zero() {
            result = *self;
        }
        let mut last = result + Decimal::ONE;

        // Keep going while the difference is larger than the tolerance
        let mut circuit_breaker = 0;
        while last != result {
            circuit_breaker += 1;
            assert!(circuit_breaker < 1000, "geo mean circuit breaker");

            last = result;
            result = (result + self / result) / TWO;
        }

        Some(result)
    }

    fn ln(&self) -> Decimal {
        const C4: Decimal = Decimal::from_parts_raw(4, 0, 0, 0);
        const C256: Decimal = Decimal::from_parts_raw(256, 0, 0, 0);
        const EIGHT_LN2: Decimal = Decimal::from_parts(1406348788, 262764557, 3006046716, false, 28);

        if self.is_sign_positive() {
            if *self == Decimal::ONE {
                Decimal::ZERO
            } else {
                let rhs = C4 / (self * C256);
                let arith_geo_mean = arithmetic_geo_mean_of_2(&Decimal::ONE, &rhs);
                (PI / (arith_geo_mean * TWO)) - EIGHT_LN2
            }
        } else {
            Decimal::ZERO
        }
    }

    fn erf(&self) -> Decimal {
        if self.is_sign_positive() {
            let one = &Decimal::ONE;

            let xa1 = self * Decimal::from_parts(705230784, 0, 0, false, 10);
            let xa2 = self.powi(2) * Decimal::from_parts(422820123, 0, 0, false, 10);
            let xa3 = self.powi(3) * Decimal::from_parts(92705272, 0, 0, false, 10);
            let xa4 = self.powi(4) * Decimal::from_parts(1520143, 0, 0, false, 10);
            let xa5 = self.powi(5) * Decimal::from_parts(2765672, 0, 0, false, 10);
            let xa6 = self.powi(6) * Decimal::from_parts(430638, 0, 0, false, 10);

            let sum = one + xa1 + xa2 + xa3 + xa4 + xa5 + xa6;
            one - (one / sum.powi(16))
        } else {
            -self.abs().erf()
        }
    }

    fn norm_cdf(&self) -> Decimal {
        (Decimal::ONE + (self / Decimal::from_parts(2318911239, 3292722, 0, false, 16)).erf()) / TWO
    }

    fn norm_pdf(&self) -> Decimal {
        match self.checked_norm_pdf() {
            Some(d) => d,
            None => panic!("Norm Pdf overflowed"),
        }
    }

    fn checked_norm_pdf(&self) -> Option<Decimal> {
        let sqrt2pi = Decimal::from_parts_raw(2133383024, 2079885984, 1358845910, 1835008);
        let factor = -self.checked_powi(2)?;
        let factor = factor.checked_div(TWO)?;
        factor.checked_exp()?.checked_div(sqrt2pi)
    }
}

impl Pow<Decimal> for Decimal {
    type Output = Decimal;

    fn pow(self, rhs: Decimal) -> Self::Output {
        MathematicalOps::powd(&self, rhs)
    }
}

impl Pow<u64> for Decimal {
    type Output = Decimal;

    fn pow(self, rhs: u64) -> Self::Output {
        MathematicalOps::powu(&self, rhs)
    }
}

impl Pow<i64> for Decimal {
    type Output = Decimal;

    fn pow(self, rhs: i64) -> Self::Output {
        MathematicalOps::powi(&self, rhs)
    }
}

impl Pow<f64> for Decimal {
    type Output = Decimal;

    fn pow(self, rhs: f64) -> Self::Output {
        MathematicalOps::powf(&self, rhs)
    }
}

/// Returns the convergence of both the arithmetic and geometric mean.
/// Used internally.
fn arithmetic_geo_mean_of_2(a: &Decimal, b: &Decimal) -> Decimal {
    const TOLERANCE: Decimal = Decimal::from_parts(5, 0, 0, false, 7);
    let diff = (a - b).abs();

    if diff < TOLERANCE {
        *a
    } else {
        arithmetic_geo_mean_of_2(&mean_of_2(a, b), &geo_mean_of_2(a, b))
    }
}

/// The Arithmetic mean. Used internally.
fn mean_of_2(a: &Decimal, b: &Decimal) -> Decimal {
    (a + b) / TWO
}

/// The geometric mean. Used internally.
fn geo_mean_of_2(a: &Decimal, b: &Decimal) -> Decimal {
    // TODO: This can overflow unnecessarily. We should keep this in an internal representation until
    //       absolutely necessary to convert back.
    (a * b).sqrt().unwrap()
}

#[cfg(test)]
mod test {
    use super::*;

    use std::str::FromStr;

    #[test]
    fn factorials() {
        assert_eq!("1", FACTORIAL[0].to_string(), "0!");
        assert_eq!("1", FACTORIAL[1].to_string(), "1!");
        assert_eq!("2", FACTORIAL[2].to_string(), "2!");
        assert_eq!("6", FACTORIAL[3].to_string(), "3!");
        assert_eq!("24", FACTORIAL[4].to_string(), "4!");
        assert_eq!("120", FACTORIAL[5].to_string(), "5!");
        assert_eq!("720", FACTORIAL[6].to_string(), "6!");
        assert_eq!("5040", FACTORIAL[7].to_string(), "7!");
        assert_eq!("40320", FACTORIAL[8].to_string(), "8!");
        assert_eq!("362880", FACTORIAL[9].to_string(), "9!");
        assert_eq!("3628800", FACTORIAL[10].to_string(), "10!");
        assert_eq!("39916800", FACTORIAL[11].to_string(), "11!");
        assert_eq!("479001600", FACTORIAL[12].to_string(), "12!");
        assert_eq!("6227020800", FACTORIAL[13].to_string(), "13!");
        assert_eq!("87178291200", FACTORIAL[14].to_string(), "14!");
        assert_eq!("1307674368000", FACTORIAL[15].to_string(), "15!");
        assert_eq!("20922789888000", FACTORIAL[16].to_string(), "16!");
        assert_eq!("355687428096000", FACTORIAL[17].to_string(), "17!");
        assert_eq!("6402373705728000", FACTORIAL[18].to_string(), "18!");
        assert_eq!("121645100408832000", FACTORIAL[19].to_string(), "19!");
        assert_eq!("2432902008176640000", FACTORIAL[20].to_string(), "20!");
        assert_eq!("51090942171709440000", FACTORIAL[21].to_string(), "21!");
        assert_eq!("1124000727777607680000", FACTORIAL[22].to_string(), "22!");
        assert_eq!("25852016738884976640000", FACTORIAL[23].to_string(), "23!");
        assert_eq!("620448401733239439360000", FACTORIAL[24].to_string(), "24!");
        assert_eq!("15511210043330985984000000", FACTORIAL[25].to_string(), "25!");
        assert_eq!("403291461126605635584000000", FACTORIAL[26].to_string(), "26!");
        assert_eq!("10888869450418352160768000000", FACTORIAL[27].to_string(), "27!");
    }

    #[test]
    fn test_geo_mean_of_2() {
        let test_cases = &[
            (
                Decimal::from_str("2").unwrap(),
                Decimal::from_str("2").unwrap(),
                Decimal::from_str("2").unwrap(),
            ),
            (
                Decimal::from_str("4").unwrap(),
                Decimal::from_str("3").unwrap(),
                Decimal::from_str("3.4641016151377545870548926830").unwrap(),
            ),
            (
                Decimal::from_str("12").unwrap(),
                Decimal::from_str("3").unwrap(),
                Decimal::from_str("6.000000000000000000000000000").unwrap(),
            ),
        ];

        for case in test_cases {
            assert_eq!(case.2, geo_mean_of_2(&case.0, &case.1));
        }
    }

    #[test]
    fn test_mean_of_2() {
        let test_cases = &[
            (
                Decimal::from_str("2").unwrap(),
                Decimal::from_str("2").unwrap(),
                Decimal::from_str("2").unwrap(),
            ),
            (
                Decimal::from_str("4").unwrap(),
                Decimal::from_str("3").unwrap(),
                Decimal::from_str("3.5").unwrap(),
            ),
            (
                Decimal::from_str("12").unwrap(),
                Decimal::from_str("3").unwrap(),
                Decimal::from_str("7.5").unwrap(),
            ),
        ];

        for case in test_cases {
            assert_eq!(case.2, mean_of_2(&case.0, &case.1));
        }
    }
}
