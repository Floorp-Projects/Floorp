// Copyright (c) 2023 ISRG
// SPDX-License-Identifier: MPL-2.0
//
// This file contains code covered by the following copyright and permission notice
// and has been modified by ISRG and collaborators.
//
// Copyright (c) 2022 President and Fellows of Harvard College
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// This file incorporates work covered by the following copyright and
// permission notice:
//
//   Copyright 2020 Thomas Steinke
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.

//   The following code is adapted from the opendp implementation to reduce dependencies:
//       https://github.com/opendp/opendp/blob/main/rust/src/traits/samplers/cks20

//! Implementation of a sampler from the Discrete Gaussian Distribution.
//!
//! Follows
//!     Clément Canonne, Gautam Kamath, Thomas Steinke. The Discrete Gaussian for Differential Privacy. 2020.
//!     <https://arxiv.org/pdf/2004.00010.pdf>

use num_bigint::{BigInt, BigUint, UniformBigUint};
use num_integer::Integer;
use num_iter::range_inclusive;
use num_rational::Ratio;
use num_traits::{One, Zero};
use rand::{distributions::uniform::UniformSampler, distributions::Distribution, Rng};
use serde::{Deserialize, Serialize};

use super::{
    DifferentialPrivacyBudget, DifferentialPrivacyDistribution, DifferentialPrivacyStrategy,
    DpError, ZCdpBudget,
};

/// Sample from the Bernoulli(gamma) distribution, where $gamma /leq 1$.
///
/// `sample_bernoulli(gamma, rng)` returns numbers distributed as $Bernoulli(gamma)$.
/// using the given random number generator for base randomness. The procedure is as described
/// on page 30 of [[CKS20]].
///
/// [CKS20]: https://arxiv.org/pdf/2004.00010.pdf
fn sample_bernoulli<R: Rng + ?Sized>(gamma: &Ratio<BigUint>, rng: &mut R) -> bool {
    let d = gamma.denom();
    assert!(!d.is_zero());
    assert!(gamma <= &Ratio::<BigUint>::one());

    // sample uniform biguint in {1,...,d}
    // uses the implementation of rand::Uniform for num_bigint::BigUint
    let s = UniformBigUint::sample_single_inclusive(BigUint::one(), d, rng);

    s <= *gamma.numer()
}

/// Sample from the Bernoulli(exp(-gamma)) distribution where `gamma` is in `[0,1]`.
///
/// `sample_bernoulli_exp1(gamma, rng)` returns numbers distributed as $Bernoulli(exp(-gamma))$,
/// using the given random number generator for base randomness. Follows Algorithm 1 of [[CKS20]],
/// splitting the branches into two non-recursive functions. This is the `gamma in [0,1]` branch.
///
/// [CKS20]: https://arxiv.org/pdf/2004.00010.pdf
fn sample_bernoulli_exp1<R: Rng + ?Sized>(gamma: &Ratio<BigUint>, rng: &mut R) -> bool {
    assert!(!gamma.denom().is_zero());
    assert!(gamma <= &Ratio::<BigUint>::one());

    let mut k = BigUint::one();
    loop {
        if sample_bernoulli(&(gamma / k.clone()), rng) {
            k += 1u8;
        } else {
            return k.is_odd();
        }
    }
}

/// Sample from the Bernoulli(exp(-gamma)) distribution.
///
/// `sample_bernoulli_exp(gamma, rng)` returns numbers distributed as $Bernoulli(exp(-gamma))$,
/// using the given random number generator for base randomness. Follows Algorithm 1 of [[CKS20]],
/// splitting the branches into two non-recursive functions. This is the `gamma > 1` branch.
///
/// [CKS20]: https://arxiv.org/pdf/2004.00010.pdf
fn sample_bernoulli_exp<R: Rng + ?Sized>(gamma: &Ratio<BigUint>, rng: &mut R) -> bool {
    assert!(!gamma.denom().is_zero());
    for _ in range_inclusive(BigUint::one(), gamma.floor().to_integer()) {
        if !sample_bernoulli_exp1(&Ratio::<BigUint>::one(), rng) {
            return false;
        }
    }
    sample_bernoulli_exp1(&(gamma - gamma.floor()), rng)
}

/// Sample from the geometric distribution  with parameter 1 - exp(-gamma).
///
/// `sample_geometric_exp(gamma, rng)` returns numbers distributed according to
/// $Geometric(1 - exp(-gamma))$, using the given random number generator for base randomness.
/// The code follows all but the last three lines of Algorithm 2 in [[CKS20]].
///
/// [CKS20]: https://arxiv.org/pdf/2004.00010.pdf
fn sample_geometric_exp<R: Rng + ?Sized>(gamma: &Ratio<BigUint>, rng: &mut R) -> BigUint {
    let (s, t) = (gamma.numer(), gamma.denom());
    assert!(!t.is_zero());
    if gamma.is_zero() {
        return BigUint::zero();
    }

    // sampler for uniform biguint in {0...t-1}
    // uses the implementation of rand::Uniform for num_bigint::BigUint
    let usampler = UniformBigUint::new(BigUint::zero(), t);
    let mut u = usampler.sample(rng);

    while !sample_bernoulli_exp1(&Ratio::<BigUint>::new(u.clone(), t.clone()), rng) {
        u = usampler.sample(rng);
    }

    let mut v = BigUint::zero();
    loop {
        if sample_bernoulli_exp1(&Ratio::<BigUint>::one(), rng) {
            v += 1u8;
        } else {
            break;
        }
    }

    // we do integer division, so the following term equals floor((u + t*v)/s)
    (u + t * v) / s
}

/// Sample from the discrete Laplace distribution.
///
/// `sample_discrete_laplace(scale, rng)` returns numbers distributed according to
/// $\mathcal{L}_\mathbb{Z}(0, scale)$, using the given random number generator for base randomness.
/// This follows Algorithm 2 of [[CKS20]], using a subfunction for geometric sampling.
///
/// [CKS20]: https://arxiv.org/pdf/2004.00010.pdf
fn sample_discrete_laplace<R: Rng + ?Sized>(scale: &Ratio<BigUint>, rng: &mut R) -> BigInt {
    let (s, t) = (scale.numer(), scale.denom());
    assert!(!t.is_zero());
    if s.is_zero() {
        return BigInt::zero();
    }

    loop {
        let negative = sample_bernoulli(&Ratio::<BigUint>::new(BigUint::one(), 2u8.into()), rng);
        let y: BigInt = sample_geometric_exp(&scale.recip(), rng).into();
        if negative && y.is_zero() {
            continue;
        } else {
            return if negative { -y } else { y };
        }
    }
}

/// Sample from the discrete Gaussian distribution.
///
/// `sample_discrete_gaussian(sigma, rng)` returns `BigInt` numbers distributed as
/// $\mathcal{N}_\mathbb{Z}(0, sigma^2)$, using the given random number generator for base
/// randomness. Follows Algorithm 3 from [[CKS20]].
///
/// [CKS20]: https://arxiv.org/pdf/2004.00010.pdf
fn sample_discrete_gaussian<R: Rng + ?Sized>(sigma: &Ratio<BigUint>, rng: &mut R) -> BigInt {
    assert!(!sigma.denom().is_zero());
    if sigma.is_zero() {
        return 0.into();
    }
    let t = sigma.floor() + BigUint::one();

    // no need to compute these parts of the probability term every iteration
    let summand = sigma.pow(2) / t.clone();
    // compute probability of accepting the laplace sample y
    let prob = |term: Ratio<BigUint>| term.pow(2) * (sigma.pow(2) * BigUint::from(2u8)).recip();

    loop {
        let y = sample_discrete_laplace(&t, rng);

        // absolute value without type conversion
        let y_abs: Ratio<BigUint> = BigUint::new(y.to_u32_digits().1).into();

        // unsigned subtraction-followed-by-square
        let prob: Ratio<BigUint> = if y_abs < summand {
            prob(summand.clone() - y_abs)
        } else {
            prob(y_abs - summand.clone())
        };

        if sample_bernoulli_exp(&prob, rng) {
            return y;
        }
    }
}

/// Samples `BigInt` numbers according to the discrete Gaussian distribution with mean zero.
/// The distribution is defined over the integers, represented by arbitrary-precision integers.
/// The sampling procedure follows [[CKS20]].
///
/// [CKS20]: https://arxiv.org/pdf/2004.00010.pdf
#[derive(Clone, Debug)]
pub struct DiscreteGaussian {
    /// The standard deviation of the distribution.
    std: Ratio<BigUint>,
}

impl DiscreteGaussian {
    /// Create a new sampler from the Discrete Gaussian Distribution with the given
    /// standard deviation and mean zero. Errors if the input has denominator zero.
    pub fn new(std: Ratio<BigUint>) -> Result<DiscreteGaussian, DpError> {
        if std.denom().is_zero() {
            return Err(DpError::ZeroDenominator);
        }
        Ok(DiscreteGaussian { std })
    }
}

impl Distribution<BigInt> for DiscreteGaussian {
    fn sample<R>(&self, rng: &mut R) -> BigInt
    where
        R: Rng + ?Sized,
    {
        sample_discrete_gaussian(&self.std, rng)
    }
}

impl DifferentialPrivacyDistribution for DiscreteGaussian {}

/// A DP strategy using the discrete gaussian distribution.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq, Ord, PartialOrd)]
pub struct DiscreteGaussianDpStrategy<B>
where
    B: DifferentialPrivacyBudget,
{
    budget: B,
}

/// A DP strategy using the discrete gaussian distribution providing zero-concentrated DP.
pub type ZCdpDiscreteGaussian = DiscreteGaussianDpStrategy<ZCdpBudget>;

impl DifferentialPrivacyStrategy for DiscreteGaussianDpStrategy<ZCdpBudget> {
    type Budget = ZCdpBudget;
    type Distribution = DiscreteGaussian;
    type Sensitivity = Ratio<BigUint>;

    fn from_budget(budget: ZCdpBudget) -> DiscreteGaussianDpStrategy<ZCdpBudget> {
        DiscreteGaussianDpStrategy { budget }
    }

    /// Create a new sampler from the Discrete Gaussian Distribution with a standard
    /// deviation calibrated to provide `1/2 epsilon^2` zero-concentrated differential
    /// privacy when added to the result of an integer-valued function with sensitivity
    /// `sensitivity`, following Theorem 4 from [[CKS20]]
    ///
    /// [CKS20]: https://arxiv.org/pdf/2004.00010.pdf
    fn create_distribution(
        &self,
        sensitivity: Ratio<BigUint>,
    ) -> Result<DiscreteGaussian, DpError> {
        DiscreteGaussian::new(sensitivity / self.budget.epsilon.clone())
    }
}

#[cfg(test)]
mod tests {

    use super::*;
    use crate::dp::Rational;
    use crate::vdaf::xof::SeedStreamSha3;

    use num_bigint::{BigUint, Sign, ToBigInt, ToBigUint};
    use num_traits::{One, Signed, ToPrimitive};
    use rand::{distributions::Distribution, SeedableRng};
    use statrs::distribution::{ChiSquared, ContinuousCDF, Normal};
    use std::collections::HashMap;

    #[test]
    fn test_discrete_gaussian() {
        let sampler =
            DiscreteGaussian::new(Ratio::<BigUint>::from_integer(BigUint::from(5u8))).unwrap();

        // check samples are consistent
        let mut rng = SeedStreamSha3::from_seed([0u8; 16]);
        let samples: Vec<i8> = (0..10)
            .map(|_| i8::try_from(sampler.sample(&mut rng)).unwrap())
            .collect();
        let samples1: Vec<i8> = (0..10)
            .map(|_| i8::try_from(sampler.sample(&mut rng)).unwrap())
            .collect();
        assert_eq!(samples, vec![-3, -11, -3, 5, 1, 5, 2, 2, 1, 18]);
        assert_eq!(samples1, vec![4, -4, -5, -2, 0, -5, -3, 1, 1, -2]);
    }

    #[test]
    /// Make sure that the distribution created by `create_distribution`
    /// of `ZCdpDicreteGaussian` is the same one as manually creating one
    /// by using the constructor of `DiscreteGaussian` directly.
    fn test_zcdp_discrete_gaussian() {
        // sample from a manually created distribution
        let sampler1 =
            DiscreteGaussian::new(Ratio::<BigUint>::from_integer(BigUint::from(4u8))).unwrap();
        let mut rng = SeedStreamSha3::from_seed([0u8; 16]);
        let samples1: Vec<i8> = (0..10)
            .map(|_| i8::try_from(sampler1.sample(&mut rng)).unwrap())
            .collect();

        // sample from the distribution created by the `zcdp` strategy
        let zcdp = ZCdpDiscreteGaussian {
            budget: ZCdpBudget::new(Rational::try_from(0.25).unwrap()),
        };
        let sampler2 = zcdp
            .create_distribution(Ratio::<BigUint>::from_integer(1u8.into()))
            .unwrap();
        let mut rng2 = SeedStreamSha3::from_seed([0u8; 16]);
        let samples2: Vec<i8> = (0..10)
            .map(|_| i8::try_from(sampler2.sample(&mut rng2)).unwrap())
            .collect();

        assert_eq!(samples2, samples1);
    }

    pub fn test_mean<FS: FnMut() -> BigInt>(
        mut sampler: FS,
        hyp_mean: f64,
        hyp_var: f64,
        alpha: f64,
        n: u32,
    ) -> bool {
        // we test if the mean from our sampler is within the given error margin assuimng its
        // normally distributed with mean hyp_mean and variance sqrt(hyp_var/n)
        // this assumption is from the central limit theorem

        // inverse cdf (quantile function) is F s.t. P[X<=F(p)]=p for X ~ N(0,1)
        // (i.e. X from the standard normal distribution)
        let probit = |p| Normal::new(0.0, 1.0).unwrap().inverse_cdf(p);

        // x such that the probability of a N(0,1) variable attaining
        // a value outside of (-x, x) is alpha
        let z_stat = probit(alpha / 2.).abs();

        // confidence interval for the mean
        let abs_p_tol = Ratio::<BigInt>::from_float(z_stat * (hyp_var / n as f64).sqrt()).unwrap();

        // take n samples from the distribution, compute empirical mean
        let emp_mean = Ratio::<BigInt>::new((0..n).map(|_| sampler()).sum::<BigInt>(), n.into());

        (emp_mean - Ratio::<BigInt>::from_float(hyp_mean).unwrap()).abs() < abs_p_tol
    }

    fn histogram(
        d: &Vec<BigInt>,
        bin_bounds: &[Option<(BigInt, BigInt)>],
        smallest: BigInt,
        largest: BigInt,
    ) -> HashMap<Option<(BigInt, BigInt)>, u64> {
        // a binned histogram of the samples in `d`
        // used for chi_square test

        fn insert<T>(hist: &mut HashMap<T, u64>, key: &T, val: u64)
        where
            T: Eq + std::hash::Hash + Clone,
        {
            *hist.entry(key.clone()).or_default() += val;
        }

        // regular histogram
        let mut hist = HashMap::<BigInt, u64>::new();
        //binned histogram
        let mut bin_hist = HashMap::<Option<(BigInt, BigInt)>, u64>::new();

        for val in d {
            // throw outliers with bound bins
            if val < &smallest || val > &largest {
                insert(&mut bin_hist, &None, 1);
            } else {
                insert(&mut hist, val, 1);
            }
        }
        // sort values into their bins
        for (a, b) in bin_bounds.iter().flatten() {
            for i in range_inclusive(a.clone(), b.clone()) {
                if let Some(count) = hist.get(&i) {
                    insert(&mut bin_hist, &Some((a.clone(), b.clone())), *count);
                }
            }
        }
        bin_hist
    }

    fn discrete_gauss_cdf_approx(
        sigma: &BigUint,
        bin_bounds: &[Option<(BigInt, BigInt)>],
    ) -> HashMap<Option<(BigInt, BigInt)>, f64> {
        // approximate bin probabilties from theoretical distribution
        // formula is eq. (1) on page 3 of [[CKS20]]
        //
        // [CKS20]: https://arxiv.org/pdf/2004.00010.pdf
        let sigma = BigInt::from_biguint(Sign::Plus, sigma.clone());
        let exp_sum = |lower: &BigInt, upper: &BigInt| {
            range_inclusive(lower.clone(), upper.clone())
                .map(|x: BigInt| {
                    f64::exp(
                        Ratio::<BigInt>::new(-(x.pow(2)), 2 * sigma.pow(2))
                            .to_f64()
                            .unwrap(),
                    )
                })
                .sum::<f64>()
        };
        // denominator is approximate up to 10 times the variance
        // outside of that probabilities should be very small
        // so the error will be negligible for the test
        let denom = exp_sum(&(-10i8 * sigma.pow(2)), &(10i8 * sigma.pow(2)));

        // compute probabilities for each bin
        let mut cdf = HashMap::new();
        let mut p_outside = 1.0; // probability of not landing inside bin boundaries
        for (a, b) in bin_bounds.iter().flatten() {
            let entry = exp_sum(a, b) / denom;
            assert!(!entry.is_zero() && entry.is_finite());
            cdf.insert(Some((a.clone(), b.clone())), entry);
            p_outside -= entry;
        }
        cdf.insert(None, p_outside);
        cdf
    }

    fn chi_square(sigma: &BigUint, n_bins: usize, alpha: f64) -> bool {
        // perform pearsons chi-squared test on the discrete gaussian sampler

        let sigma_signed = BigInt::from_biguint(Sign::Plus, sigma.clone());

        // cut off at 3 times the std. and collect all outliers in a seperate bin
        let global_bound = 3u8 * sigma_signed;

        // bounds of bins
        let lower_bounds = range_inclusive(-global_bound.clone(), global_bound.clone()).step_by(
            ((2u8 * global_bound.clone()) / BigInt::from(n_bins))
                .try_into()
                .unwrap(),
        );
        let mut bin_bounds: Vec<Option<(BigInt, BigInt)>> = std::iter::zip(
            lower_bounds.clone().take(n_bins),
            lower_bounds.map(|x: BigInt| x - 1u8).skip(1),
        )
        .map(Some)
        .collect();
        bin_bounds.push(None); // bin for outliers

        // approximate bin probabilities
        let cdf = discrete_gauss_cdf_approx(sigma, &bin_bounds);

        // chi2 stat wants at least 5 expected entries per bin
        // so we choose n_samples in a way that gives us that
        let n_samples = cdf
            .values()
            .map(|val| f64::ceil(5.0 / *val) as u32)
            .max()
            .unwrap();

        // collect that number of samples
        let mut rng = SeedStreamSha3::from_seed([0u8; 16]);
        let samples: Vec<BigInt> = (1..n_samples)
            .map(|_| {
                sample_discrete_gaussian(&Ratio::<BigUint>::from_integer(sigma.clone()), &mut rng)
            })
            .collect();

        // make a histogram from the samples
        let hist = histogram(&samples, &bin_bounds, -global_bound.clone(), global_bound);

        // compute pearsons chi-squared test statistic
        let stat: f64 = bin_bounds
            .iter()
            .map(|key| {
                let expected = cdf.get(&(key.clone())).unwrap() * n_samples as f64;
                if let Some(val) = hist.get(&(key.clone())) {
                    (*val as f64 - expected).powf(2.) / expected
                } else {
                    0.0
                }
            })
            .sum::<f64>();

        let chi2 = ChiSquared::new((cdf.len() - 1) as f64).unwrap();
        // the probability of observing X >= stat for X ~ chi-squared
        // (the "p-value")
        let p = 1.0 - chi2.cdf(stat);

        p > alpha
    }

    #[test]
    fn empirical_test_gauss() {
        [100, 2000, 20000].iter().for_each(|p| {
            let mut rng = SeedStreamSha3::from_seed([0u8; 16]);
            let sampler = || {
                sample_discrete_gaussian(
                    &Ratio::<BigUint>::from_integer((*p).to_biguint().unwrap()),
                    &mut rng,
                )
            };
            let mean = 0.0;
            let var = (p * p) as f64;
            assert!(
                test_mean(sampler, mean, var, 0.00001, 1000),
                "Empirical evaluation of discrete Gaussian({:?}) sampler mean failed.",
                p
            );
        });
        // we only do chi square for std 100 because it's expensive
        assert!(chi_square(&(100u8.to_biguint().unwrap()), 10, 0.05));
    }

    #[test]
    fn empirical_test_bernoulli_mean() {
        [2u8, 5u8, 7u8, 9u8].iter().for_each(|p| {
            let mut rng = SeedStreamSha3::from_seed([0u8; 16]);
            let sampler = || {
                if sample_bernoulli(
                    &Ratio::<BigUint>::new(BigUint::one(), (*p).into()),
                    &mut rng,
                ) {
                    BigInt::one()
                } else {
                    BigInt::zero()
                }
            };
            let mean = 1. / (*p as f64);
            let var = mean * (1. - mean);
            assert!(
                test_mean(sampler, mean, var, 0.00001, 1000),
                "Empirical evaluation of the Bernoulli(1/{:?}) distribution mean failed",
                p
            );
        })
    }

    #[test]
    fn empirical_test_geometric_mean() {
        [2u8, 5u8, 7u8, 9u8].iter().for_each(|p| {
            let mut rng = SeedStreamSha3::from_seed([0u8; 16]);
            let sampler = || {
                sample_geometric_exp(
                    &Ratio::<BigUint>::new(BigUint::one(), (*p).into()),
                    &mut rng,
                )
                .to_bigint()
                .unwrap()
            };
            let p_prob = 1. - f64::exp(-(1. / *p as f64));
            let mean = (1. - p_prob) / p_prob;
            let var = (1. - p_prob) / p_prob.powi(2);
            assert!(
                test_mean(sampler, mean, var, 0.0001, 1000),
                "Empirical evaluation of the Geometric(1-exp(-1/{:?})) distribution mean failed",
                p
            );
        })
    }

    #[test]
    fn empirical_test_laplace_mean() {
        [2u8, 5u8, 7u8, 9u8].iter().for_each(|p| {
            let mut rng = SeedStreamSha3::from_seed([0u8; 16]);
            let sampler = || {
                sample_discrete_laplace(
                    &Ratio::<BigUint>::new(BigUint::one(), (*p).into()),
                    &mut rng,
                )
            };
            let mean = 0.0;
            let var = (1. / *p as f64).powi(2);
            assert!(
                test_mean(sampler, mean, var, 0.0001, 1000),
                "Empirical evaluation of the Laplace(0,1/{:?}) distribution mean failed",
                p
            );
        })
    }
}
