// SPDX-License-Identifier: MPL-2.0

//! Finite field arithmetic for any field GF(p) for which p < 2^128.

#[cfg(test)]
use rand::{prelude::*, Rng};

/// For each set of field parameters we pre-compute the 1st, 2nd, 4th, ..., 2^20-th principal roots
/// of unity. The largest of these is used to run the FFT algorithm on an input of size 2^20. This
/// is the largest input size we would ever need for the cryptographic applications in this crate.
pub(crate) const MAX_ROOTS: usize = 20;

/// This structure represents the parameters of a finite field GF(p) for which p < 2^128.
#[derive(Debug, PartialEq, Eq)]
pub(crate) struct FieldParameters {
    /// The prime modulus `p`.
    pub p: u128,
    /// `mu = -p^(-1) mod 2^64`.
    pub mu: u64,
    /// `r2 = (2^128)^2 mod p`.
    pub r2: u128,
    /// The `2^num_roots`-th -principal root of unity. This element is used to generate the
    /// elements of `roots`.
    pub g: u128,
    /// The number of principal roots of unity in `roots`.
    pub num_roots: usize,
    /// Equal to `2^b - 1`, where `b` is the length of `p` in bits.
    pub bit_mask: u128,
    /// `roots[l]` is the `2^l`-th principal root of unity, i.e., `roots[l]` has order `2^l` in the
    /// multiplicative group. `roots[0]` is equal to one by definition.
    pub roots: [u128; MAX_ROOTS + 1],
}

impl FieldParameters {
    /// Addition. The result will be in [0, p), so long as both x and y are as well.
    #[inline(always)]
    pub fn add(&self, x: u128, y: u128) -> u128 {
        //   0,x
        // + 0,y
        // =====
        //   c,z
        let (z, carry) = x.overflowing_add(y);
        //     c, z
        // -   0, p
        // ========
        // b1,s1,s0
        let (s0, b0) = z.overflowing_sub(self.p);
        let (_s1, b1) = (carry as u128).overflowing_sub(b0 as u128);
        // if b1 == 1: return z
        // else:       return s0
        let m = 0u128.wrapping_sub(b1 as u128);
        (z & m) | (s0 & !m)
    }

    /// Subtraction. The result will be in [0, p), so long as both x and y are as well.
    #[inline(always)]
    pub fn sub(&self, x: u128, y: u128) -> u128 {
        //     0, x
        // -   0, y
        // ========
        // b1,z1,z0
        let (z0, b0) = x.overflowing_sub(y);
        let (_z1, b1) = 0u128.overflowing_sub(b0 as u128);
        let m = 0u128.wrapping_sub(b1 as u128);
        //   z1,z0
        // +  0, p
        // ========
        //   s1,s0
        z0.wrapping_add(m & self.p)
        // if b1 == 1: return s0
        // else:       return z0
    }

    /// Multiplication of field elements in the Montgomery domain. This uses the REDC algorithm
    /// described
    /// [here](https://www.ams.org/journals/mcom/1985-44-170/S0025-5718-1985-0777282-X/S0025-5718-1985-0777282-X.pdf).
    /// The result will be in [0, p).
    ///
    /// # Example usage
    /// ```text
    /// assert_eq!(fp.residue(fp.mul(fp.montgomery(23), fp.montgomery(2))), 46);
    /// ```
    #[inline(always)]
    pub fn mul(&self, x: u128, y: u128) -> u128 {
        let x = [lo64(x), hi64(x)];
        let y = [lo64(y), hi64(y)];
        let p = [lo64(self.p), hi64(self.p)];
        let mut zz = [0; 4];

        // Integer multiplication
        // z = x * y

        //       x1,x0
        // *     y1,y0
        // ===========
        // z3,z2,z1,z0
        let mut result = x[0] * y[0];
        let mut carry = hi64(result);
        zz[0] = lo64(result);
        result = x[0] * y[1];
        let mut hi = hi64(result);
        let mut lo = lo64(result);
        result = lo + carry;
        zz[1] = lo64(result);
        let mut cc = hi64(result);
        result = hi + cc;
        zz[2] = lo64(result);

        result = x[1] * y[0];
        hi = hi64(result);
        lo = lo64(result);
        result = zz[1] + lo;
        zz[1] = lo64(result);
        cc = hi64(result);
        result = hi + cc;
        carry = lo64(result);

        result = x[1] * y[1];
        hi = hi64(result);
        lo = lo64(result);
        result = lo + carry;
        lo = lo64(result);
        cc = hi64(result);
        result = hi + cc;
        hi = lo64(result);
        result = zz[2] + lo;
        zz[2] = lo64(result);
        cc = hi64(result);
        result = hi + cc;
        zz[3] = lo64(result);

        // Montgomery Reduction
        // z = z + p * mu*(z mod 2^64), where mu = (-p)^(-1) mod 2^64.

        // z3,z2,z1,z0
        // +     p1,p0
        // *         w = mu*z0
        // ===========
        // z3,z2,z1, 0
        let w = self.mu.wrapping_mul(zz[0] as u64);
        result = p[0] * (w as u128);
        hi = hi64(result);
        lo = lo64(result);
        result = zz[0] + lo;
        zz[0] = lo64(result);
        cc = hi64(result);
        result = hi + cc;
        carry = lo64(result);

        result = p[1] * (w as u128);
        hi = hi64(result);
        lo = lo64(result);
        result = lo + carry;
        lo = lo64(result);
        cc = hi64(result);
        result = hi + cc;
        hi = lo64(result);
        result = zz[1] + lo;
        zz[1] = lo64(result);
        cc = hi64(result);
        result = zz[2] + hi + cc;
        zz[2] = lo64(result);
        cc = hi64(result);
        result = zz[3] + cc;
        zz[3] = lo64(result);

        //    z3,z2,z1
        // +     p1,p0
        // *         w = mu*z1
        // ===========
        //    z3,z2, 0
        let w = self.mu.wrapping_mul(zz[1] as u64);
        result = p[0] * (w as u128);
        hi = hi64(result);
        lo = lo64(result);
        result = zz[1] + lo;
        zz[1] = lo64(result);
        cc = hi64(result);
        result = hi + cc;
        carry = lo64(result);

        result = p[1] * (w as u128);
        hi = hi64(result);
        lo = lo64(result);
        result = lo + carry;
        lo = lo64(result);
        cc = hi64(result);
        result = hi + cc;
        hi = lo64(result);
        result = zz[2] + lo;
        zz[2] = lo64(result);
        cc = hi64(result);
        result = zz[3] + hi + cc;
        zz[3] = lo64(result);
        cc = hi64(result);

        // z = (z3,z2)
        let prod = zz[2] | (zz[3] << 64);

        // Final subtraction
        // If z >= p, then z = z - p

        //     0, z
        // -   0, p
        // ========
        // b1,s1,s0
        let (s0, b0) = prod.overflowing_sub(self.p);
        let (_s1, b1) = cc.overflowing_sub(b0 as u128);
        // if b1 == 1: return z
        // else:       return s0
        let mask = 0u128.wrapping_sub(b1 as u128);
        (prod & mask) | (s0 & !mask)
    }

    /// Modular exponentiation, i.e., `x^exp (mod p)` where `p` is the modulus. Note that the
    /// runtime of this algorithm is linear in the bit length of `exp`.
    pub fn pow(&self, x: u128, exp: u128) -> u128 {
        let mut t = self.montgomery(1);
        for i in (0..128 - exp.leading_zeros()).rev() {
            t = self.mul(t, t);
            if (exp >> i) & 1 != 0 {
                t = self.mul(t, x);
            }
        }
        t
    }

    /// Modular inversion, i.e., x^-1 (mod p) where `p` is the modulus. Note that the runtime of
    /// this algorithm is linear in the bit length of `p`.
    #[inline(always)]
    pub fn inv(&self, x: u128) -> u128 {
        self.pow(x, self.p - 2)
    }

    /// Negation, i.e., `-x (mod p)` where `p` is the modulus.
    #[inline(always)]
    pub fn neg(&self, x: u128) -> u128 {
        self.sub(0, x)
    }

    /// Maps an integer to its internal representation. Field elements are mapped to the Montgomery
    /// domain in order to carry out field arithmetic. The result will be in [0, p).
    ///
    /// # Example usage
    /// ```text
    /// let integer = 1; // Standard integer representation
    /// let elem = fp.montgomery(integer); // Internal representation in the Montgomery domain
    /// assert_eq!(elem, 2564090464);
    /// ```
    #[inline(always)]
    pub fn montgomery(&self, x: u128) -> u128 {
        modp(self.mul(x, self.r2), self.p)
    }

    /// Returns a random field element mapped.
    #[cfg(test)]
    pub fn rand_elem<R: Rng + ?Sized>(&self, rng: &mut R) -> u128 {
        let uniform = rand::distributions::Uniform::from(0..self.p);
        self.montgomery(uniform.sample(rng))
    }

    /// Maps a field element to its representation as an integer. The result will be in [0, p).
    ///
    /// #Example usage
    /// ```text
    /// let elem = 2564090464; // Internal representation in the Montgomery domain
    /// let integer = fp.residue(elem); // Standard integer representation
    /// assert_eq!(integer, 1);
    /// ```
    #[inline(always)]
    pub fn residue(&self, x: u128) -> u128 {
        modp(self.mul(x, 1), self.p)
    }

    #[cfg(test)]
    pub fn check(&self, p: u128, g: u128, order: u128) {
        use modinverse::modinverse;
        use num_bigint::{BigInt, ToBigInt};
        use std::cmp::max;

        assert_eq!(self.p, p, "p mismatch");

        let mu = match modinverse((-(p as i128)).rem_euclid(1 << 64), 1 << 64) {
            Some(mu) => mu as u64,
            None => panic!("inverse of -p (mod 2^64) is undefined"),
        };
        assert_eq!(self.mu, mu, "mu mismatch");

        let big_p = &p.to_bigint().unwrap();
        let big_r: &BigInt = &(&(BigInt::from(1) << 128) % big_p);
        let big_r2: &BigInt = &(&(big_r * big_r) % big_p);
        let mut it = big_r2.iter_u64_digits();
        let mut r2 = 0;
        r2 |= it.next().unwrap() as u128;
        if let Some(x) = it.next() {
            r2 |= (x as u128) << 64;
        }
        assert_eq!(self.r2, r2, "r2 mismatch");

        assert_eq!(self.g, self.montgomery(g), "g mismatch");
        assert_eq!(
            self.residue(self.pow(self.g, order)),
            1,
            "g order incorrect"
        );

        let num_roots = log2(order) as usize;
        assert_eq!(order, 1 << num_roots, "order not a power of 2");
        assert_eq!(self.num_roots, num_roots, "num_roots mismatch");

        let mut roots = vec![0; max(num_roots, MAX_ROOTS) + 1];
        roots[num_roots] = self.montgomery(g);
        for i in (0..num_roots).rev() {
            roots[i] = self.mul(roots[i + 1], roots[i + 1]);
        }
        assert_eq!(&self.roots, &roots[..MAX_ROOTS + 1], "roots mismatch");
        assert_eq!(self.residue(self.roots[0]), 1, "first root is not one");

        let bit_mask = (BigInt::from(1) << big_p.bits()) - BigInt::from(1);
        assert_eq!(
            self.bit_mask.to_bigint().unwrap(),
            bit_mask,
            "bit_mask mismatch"
        );
    }
}

#[inline(always)]
fn lo64(x: u128) -> u128 {
    x & ((1 << 64) - 1)
}

#[inline(always)]
fn hi64(x: u128) -> u128 {
    x >> 64
}

#[inline(always)]
fn modp(x: u128, p: u128) -> u128 {
    let (z, carry) = x.overflowing_sub(p);
    let m = 0u128.wrapping_sub(carry as u128);
    z.wrapping_add(m & p)
}

pub(crate) const FP32: FieldParameters = FieldParameters {
    p: 4293918721, // 32-bit prime
    mu: 17302828673139736575,
    r2: 1676699750,
    g: 1074114499,
    num_roots: 20,
    bit_mask: 4294967295,
    roots: [
        2564090464, 1729828257, 306605458, 2294308040, 1648889905, 57098624, 2788941825,
        2779858277, 368200145, 2760217336, 594450960, 4255832533, 1372848488, 721329415,
        3873251478, 1134002069, 7138597, 2004587313, 2989350643, 725214187, 1074114499,
    ],
};

pub(crate) const FP64: FieldParameters = FieldParameters {
    p: 18446744069414584321, // 64-bit prime
    mu: 18446744069414584319,
    r2: 4294967295,
    g: 959634606461954525,
    num_roots: 32,
    bit_mask: 18446744073709551615,
    roots: [
        18446744065119617025,
        4294967296,
        18446462594437939201,
        72057594037927936,
        1152921504338411520,
        16384,
        18446743519658770561,
        18446735273187346433,
        6519596376689022014,
        9996039020351967275,
        15452408553935940313,
        15855629130643256449,
        8619522106083987867,
        13036116919365988132,
        1033106119984023956,
        16593078884869787648,
        16980581328500004402,
        12245796497946355434,
        8709441440702798460,
        8611358103550827629,
        8120528636261052110,
    ],
};

pub(crate) const FP128: FieldParameters = FieldParameters {
    p: 340282366920938462946865773367900766209, // 128-bit prime
    mu: 18446744073709551615,
    r2: 403909908237944342183153,
    g: 107630958476043550189608038630704257141,
    num_roots: 66,
    bit_mask: 340282366920938463463374607431768211455,
    roots: [
        516508834063867445247,
        340282366920938462430356939304033320962,
        129526470195413442198896969089616959958,
        169031622068548287099117778531474117974,
        81612939378432101163303892927894236156,
        122401220764524715189382260548353967708,
        199453575871863981432000940507837456190,
        272368408887745135168960576051472383806,
        24863773656265022616993900367764287617,
        257882853788779266319541142124730662203,
        323732363244658673145040701829006542956,
        57532865270871759635014308631881743007,
        149571414409418047452773959687184934208,
        177018931070866797456844925926211239962,
        268896136799800963964749917185333891349,
        244556960591856046954834420512544511831,
        118945432085812380213390062516065622346,
        202007153998709986841225284843501908420,
        332677126194796691532164818746739771387,
        258279638927684931537542082169183965856,
        148221243758794364405224645520862378432,
    ],
};

// Compute the ceiling of the base-2 logarithm of `x`.
pub(crate) fn log2(x: u128) -> u128 {
    let y = (127 - x.leading_zeros()) as u128;
    y + ((x > 1 << y) as u128)
}

#[cfg(test)]
mod tests {
    use super::*;
    use num_bigint::ToBigInt;

    #[test]
    fn test_log2() {
        assert_eq!(log2(1), 0);
        assert_eq!(log2(2), 1);
        assert_eq!(log2(3), 2);
        assert_eq!(log2(4), 2);
        assert_eq!(log2(15), 4);
        assert_eq!(log2(16), 4);
        assert_eq!(log2(30), 5);
        assert_eq!(log2(32), 5);
        assert_eq!(log2(1 << 127), 127);
        assert_eq!(log2((1 << 127) + 13), 128);
    }

    struct TestFieldParametersData {
        fp: FieldParameters,  // The paramters being tested
        expected_p: u128,     // Expected fp.p
        expected_g: u128,     // Expected fp.residue(fp.g)
        expected_order: u128, // Expect fp.residue(fp.pow(fp.g, expected_order)) == 1
    }

    #[test]
    fn test_fp() {
        let test_fps = vec![
            TestFieldParametersData {
                fp: FP32,
                expected_p: 4293918721,
                expected_g: 3925978153,
                expected_order: 1 << 20,
            },
            TestFieldParametersData {
                fp: FP64,
                expected_p: 18446744069414584321,
                expected_g: 1753635133440165772,
                expected_order: 1 << 32,
            },
            TestFieldParametersData {
                fp: FP128,
                expected_p: 340282366920938462946865773367900766209,
                expected_g: 145091266659756586618791329697897684742,
                expected_order: 1 << 66,
            },
        ];

        for t in test_fps.into_iter() {
            //  Check that the field parameters have been constructed properly.
            t.fp.check(t.expected_p, t.expected_g, t.expected_order);

            // Check that the generator has the correct order.
            assert_eq!(t.fp.residue(t.fp.pow(t.fp.g, t.expected_order)), 1);
            assert_ne!(t.fp.residue(t.fp.pow(t.fp.g, t.expected_order / 2)), 1);

            // Test arithmetic using the field parameters.
            arithmetic_test(&t.fp);
        }
    }

    fn arithmetic_test(fp: &FieldParameters) {
        let mut rng = rand::thread_rng();
        let big_p = &fp.p.to_bigint().unwrap();

        for _ in 0..100 {
            let x = fp.rand_elem(&mut rng);
            let y = fp.rand_elem(&mut rng);
            let big_x = &fp.residue(x).to_bigint().unwrap();
            let big_y = &fp.residue(y).to_bigint().unwrap();

            // Test addition.
            let got = fp.add(x, y);
            let want = (big_x + big_y) % big_p;
            assert_eq!(fp.residue(got).to_bigint().unwrap(), want);

            // Test subtraction.
            let got = fp.sub(x, y);
            let want = if big_x >= big_y {
                big_x - big_y
            } else {
                big_p - big_y + big_x
            };
            assert_eq!(fp.residue(got).to_bigint().unwrap(), want);

            // Test multiplication.
            let got = fp.mul(x, y);
            let want = (big_x * big_y) % big_p;
            assert_eq!(fp.residue(got).to_bigint().unwrap(), want);

            // Test inversion.
            let got = fp.inv(x);
            let want = big_x.modpow(&(big_p - 2u128), big_p);
            assert_eq!(fp.residue(got).to_bigint().unwrap(), want);
            assert_eq!(fp.residue(fp.mul(got, x)), 1);

            // Test negation.
            let got = fp.neg(x);
            let want = (big_p - big_x) % big_p;
            assert_eq!(fp.residue(got).to_bigint().unwrap(), want);
            assert_eq!(fp.residue(fp.add(got, x)), 0);
        }
    }
}
