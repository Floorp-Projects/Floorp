// SPDX-License-Identifier: MPL-2.0

use num_bigint::{BigInt, BigUint};
use num_rational::Ratio;
use num_traits::FromPrimitive;
use prio::dp::distributions::DiscreteGaussian;
use prio::vdaf::xof::SeedStreamSha3;
use rand::distributions::Distribution;
use rand::SeedableRng;
use serde::Deserialize;

/// A test vector of discrete Gaussian samples, produced by the python reference
/// implementation for [[CKS20]]. The script used to generate the test vector can
/// be found in this gist:
/// https://gist.github.com/ooovi/529c00fc8a7eafd068cd076b78fc424e
/// The python reference implementation is here:
/// https://github.com/IBM/discrete-gaussian-differential-privacy
///
/// [CKS20]: https://arxiv.org/pdf/2004.00010.pdf
#[derive(Debug, Eq, PartialEq, Deserialize)]
pub struct DiscreteGaussTestVector {
    #[serde(with = "hex")]
    seed: [u8; 16],
    std_num: u128,
    std_denom: u128,
    samples: Vec<i128>,
}

#[test]
fn discrete_gauss_reference() {
    let test_vectors: Vec<DiscreteGaussTestVector> = vec![
        serde_json::from_str(include_str!(concat!("test_vectors/discrete_gauss_3.json"))).unwrap(),
        serde_json::from_str(include_str!(concat!("test_vectors/discrete_gauss_9.json"))).unwrap(),
        serde_json::from_str(include_str!(concat!(
            "test_vectors/discrete_gauss_100.json"
        )))
        .unwrap(),
        serde_json::from_str(include_str!(concat!(
            "test_vectors/discrete_gauss_41293847.json"
        )))
        .unwrap(),
        serde_json::from_str(include_str!(concat!(
            "test_vectors/discrete_gauss_9999999999999999999999.json"
        )))
        .unwrap(),
    ];

    for test_vector in test_vectors {
        let sampler = DiscreteGaussian::new(Ratio::<BigUint>::new(
            test_vector.std_num.into(),
            test_vector.std_denom.into(),
        ))
        .unwrap();

        // check samples are consistent
        let mut rng = SeedStreamSha3::from_seed(test_vector.seed);
        let samples: Vec<BigInt> = (0..test_vector.samples.len())
            .map(|_| sampler.sample(&mut rng))
            .collect();

        assert_eq!(
            samples,
            test_vector
                .samples
                .iter()
                .map(|&s| BigInt::from_i128(s).unwrap())
                .collect::<Vec::<BigInt>>()
        );
    }
}
