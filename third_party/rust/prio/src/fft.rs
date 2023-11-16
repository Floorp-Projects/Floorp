// SPDX-License-Identifier: MPL-2.0

//! This module implements an iterative FFT algorithm for computing the (inverse) Discrete Fourier
//! Transform (DFT) over a slice of field elements.

use crate::field::FftFriendlyFieldElement;
use crate::fp::{log2, MAX_ROOTS};

use std::convert::TryFrom;

/// An error returned by an FFT operation.
#[derive(Debug, PartialEq, Eq, thiserror::Error)]
pub enum FftError {
    /// The output is too small.
    #[error("output slice is smaller than specified size")]
    OutputTooSmall,
    /// The specified size is too large.
    #[error("size is larger than than maximum permitted")]
    SizeTooLarge,
    /// The specified size is not a power of 2.
    #[error("size is not a power of 2")]
    SizeInvalid,
}

/// Sets `outp` to the DFT of `inp`.
///
/// Interpreting the input as the coefficients of a polynomial, the output is equal to the input
/// evaluated at points `p^0, p^1, ... p^(size-1)`, where `p` is the `2^size`-th principal root of
/// unity.
#[allow(clippy::many_single_char_names)]
pub fn discrete_fourier_transform<F: FftFriendlyFieldElement>(
    outp: &mut [F],
    inp: &[F],
    size: usize,
) -> Result<(), FftError> {
    let d = usize::try_from(log2(size as u128)).map_err(|_| FftError::SizeTooLarge)?;

    if size > outp.len() {
        return Err(FftError::OutputTooSmall);
    }

    if size > 1 << MAX_ROOTS {
        return Err(FftError::SizeTooLarge);
    }

    if size != 1 << d {
        return Err(FftError::SizeInvalid);
    }

    for (i, outp_val) in outp[..size].iter_mut().enumerate() {
        let j = bitrev(d, i);
        *outp_val = if j < inp.len() { inp[j] } else { F::zero() };
    }

    let mut w: F;
    for l in 1..d + 1 {
        w = F::one();
        let r = F::root(l).unwrap();
        let y = 1 << (l - 1);
        let chunk = (size / y) >> 1;

        // unrolling first iteration of i-loop.
        for j in 0..chunk {
            let x = j << l;
            let u = outp[x];
            let v = outp[x + y];
            outp[x] = u + v;
            outp[x + y] = u - v;
        }

        for i in 1..y {
            w *= r;
            for j in 0..chunk {
                let x = (j << l) + i;
                let u = outp[x];
                let v = w * outp[x + y];
                outp[x] = u + v;
                outp[x + y] = u - v;
            }
        }
    }

    Ok(())
}

/// Sets `outp` to the inverse of the DFT of `inp`.
#[cfg(test)]
pub(crate) fn discrete_fourier_transform_inv<F: FftFriendlyFieldElement>(
    outp: &mut [F],
    inp: &[F],
    size: usize,
) -> Result<(), FftError> {
    let size_inv = F::from(F::Integer::try_from(size).unwrap()).inv();
    discrete_fourier_transform(outp, inp, size)?;
    discrete_fourier_transform_inv_finish(outp, size, size_inv);
    Ok(())
}

/// An intermediate step in the computation of the inverse DFT. Exposing this function allows us to
/// amortize the cost the modular inverse across multiple inverse DFT operations.
pub(crate) fn discrete_fourier_transform_inv_finish<F: FftFriendlyFieldElement>(
    outp: &mut [F],
    size: usize,
    size_inv: F,
) {
    let mut tmp: F;
    outp[0] *= size_inv;
    outp[size >> 1] *= size_inv;
    for i in 1..size >> 1 {
        tmp = outp[i] * size_inv;
        outp[i] = outp[size - i] * size_inv;
        outp[size - i] = tmp;
    }
}

// bitrev returns the first d bits of x in reverse order. (Thanks, OEIS! https://oeis.org/A030109)
fn bitrev(d: usize, x: usize) -> usize {
    let mut y = 0;
    for i in 0..d {
        y += ((x >> i) & 1) << (d - i);
    }
    y >> 1
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::field::{random_vector, split_vector, Field128, Field64, FieldElement, FieldPrio2};
    use crate::polynomial::{poly_fft, PolyAuxMemory};

    fn discrete_fourier_transform_then_inv_test<F: FftFriendlyFieldElement>() -> Result<(), FftError>
    {
        let test_sizes = [1, 2, 4, 8, 16, 256, 1024, 2048];

        for size in test_sizes.iter() {
            let mut tmp = vec![F::zero(); *size];
            let mut got = vec![F::zero(); *size];
            let want = random_vector(*size).unwrap();

            discrete_fourier_transform(&mut tmp, &want, want.len())?;
            discrete_fourier_transform_inv(&mut got, &tmp, tmp.len())?;
            assert_eq!(got, want);
        }

        Ok(())
    }

    #[test]
    fn test_priov2_field32() {
        discrete_fourier_transform_then_inv_test::<FieldPrio2>().expect("unexpected error");
    }

    #[test]
    fn test_field64() {
        discrete_fourier_transform_then_inv_test::<Field64>().expect("unexpected error");
    }

    #[test]
    fn test_field128() {
        discrete_fourier_transform_then_inv_test::<Field128>().expect("unexpected error");
    }

    #[test]
    fn test_recursive_fft() {
        let size = 128;
        let mut mem = PolyAuxMemory::new(size / 2);

        let inp = random_vector(size).unwrap();
        let mut want = vec![FieldPrio2::zero(); size];
        let mut got = vec![FieldPrio2::zero(); size];

        discrete_fourier_transform::<FieldPrio2>(&mut want, &inp, inp.len()).unwrap();

        poly_fft(
            &mut got,
            &inp,
            &mem.roots_2n,
            size,
            false,
            &mut mem.fft_memory,
        );

        assert_eq!(got, want);
    }

    // This test demonstrates a consequence of \[BBG+19, Fact 4.4\]: interpolating a polynomial
    // over secret shares and summing up the coefficients is equivalent to interpolating a
    // polynomial over the plaintext data.
    #[test]
    fn test_fft_linearity() {
        let len = 16;
        let num_shares = 3;
        let x: Vec<Field64> = random_vector(len).unwrap();
        let mut x_shares = split_vector(&x, num_shares).unwrap();

        // Just for fun, let's do something different with a subset of the inputs. For the first
        // share, every odd element is set to the plaintext value. For all shares but the first,
        // every odd element is set to 0.
        for (i, x_val) in x.iter().enumerate() {
            if i % 2 != 0 {
                x_shares[0][i] = *x_val;
                for x_share in x_shares[1..num_shares].iter_mut() {
                    x_share[i] = Field64::zero();
                }
            }
        }

        let mut got = vec![Field64::zero(); len];
        let mut buf = vec![Field64::zero(); len];
        for share in x_shares {
            discrete_fourier_transform_inv(&mut buf, &share, len).unwrap();
            for i in 0..len {
                got[i] += buf[i];
            }
        }

        let mut want = vec![Field64::zero(); len];
        discrete_fourier_transform_inv(&mut want, &x, len).unwrap();

        assert_eq!(got, want);
    }
}
