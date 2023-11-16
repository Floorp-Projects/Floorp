// Copyright (c) 2020 Apple Inc.
// SPDX-License-Identifier: MPL-2.0

//! Functions for polynomial interpolation and evaluation

#[cfg(feature = "prio2")]
use crate::fft::{discrete_fourier_transform, discrete_fourier_transform_inv_finish};
use crate::field::FftFriendlyFieldElement;

use std::convert::TryFrom;

/// Temporary memory used for FFT
#[derive(Clone, Debug)]
pub struct PolyFFTTempMemory<F> {
    fft_tmp: Vec<F>,
    fft_y_sub: Vec<F>,
    fft_roots_sub: Vec<F>,
}

impl<F: FftFriendlyFieldElement> PolyFFTTempMemory<F> {
    fn new(length: usize) -> Self {
        PolyFFTTempMemory {
            fft_tmp: vec![F::zero(); length],
            fft_y_sub: vec![F::zero(); length],
            fft_roots_sub: vec![F::zero(); length],
        }
    }
}

/// Auxiliary memory for polynomial interpolation and evaluation
#[derive(Clone, Debug)]
pub struct PolyAuxMemory<F> {
    pub roots_2n: Vec<F>,
    pub roots_2n_inverted: Vec<F>,
    pub roots_n: Vec<F>,
    pub roots_n_inverted: Vec<F>,
    pub coeffs: Vec<F>,
    pub fft_memory: PolyFFTTempMemory<F>,
}

impl<F: FftFriendlyFieldElement> PolyAuxMemory<F> {
    pub fn new(n: usize) -> Self {
        PolyAuxMemory {
            roots_2n: fft_get_roots(2 * n, false),
            roots_2n_inverted: fft_get_roots(2 * n, true),
            roots_n: fft_get_roots(n, false),
            roots_n_inverted: fft_get_roots(n, true),
            coeffs: vec![F::zero(); 2 * n],
            fft_memory: PolyFFTTempMemory::new(2 * n),
        }
    }
}

fn fft_recurse<F: FftFriendlyFieldElement>(
    out: &mut [F],
    n: usize,
    roots: &[F],
    ys: &[F],
    tmp: &mut [F],
    y_sub: &mut [F],
    roots_sub: &mut [F],
) {
    if n == 1 {
        out[0] = ys[0];
        return;
    }

    let half_n = n / 2;

    let (tmp_first, tmp_second) = tmp.split_at_mut(half_n);
    let (y_sub_first, y_sub_second) = y_sub.split_at_mut(half_n);
    let (roots_sub_first, roots_sub_second) = roots_sub.split_at_mut(half_n);

    // Recurse on the first half
    for i in 0..half_n {
        y_sub_first[i] = ys[i] + ys[i + half_n];
        roots_sub_first[i] = roots[2 * i];
    }
    fft_recurse(
        tmp_first,
        half_n,
        roots_sub_first,
        y_sub_first,
        tmp_second,
        y_sub_second,
        roots_sub_second,
    );
    for i in 0..half_n {
        out[2 * i] = tmp_first[i];
    }

    // Recurse on the second half
    for i in 0..half_n {
        y_sub_first[i] = ys[i] - ys[i + half_n];
        y_sub_first[i] *= roots[i];
    }
    fft_recurse(
        tmp_first,
        half_n,
        roots_sub_first,
        y_sub_first,
        tmp_second,
        y_sub_second,
        roots_sub_second,
    );
    for i in 0..half_n {
        out[2 * i + 1] = tmp[i];
    }
}

/// Calculate `count` number of roots of unity of order `count`
fn fft_get_roots<F: FftFriendlyFieldElement>(count: usize, invert: bool) -> Vec<F> {
    let mut roots = vec![F::zero(); count];
    let mut gen = F::generator();
    if invert {
        gen = gen.inv();
    }

    roots[0] = F::one();
    let step_size = F::generator_order() / F::Integer::try_from(count).unwrap();
    // generator for subgroup of order count
    gen = gen.pow(step_size);

    roots[1] = gen;

    for i in 2..count {
        roots[i] = gen * roots[i - 1];
    }

    roots
}

fn fft_interpolate_raw<F: FftFriendlyFieldElement>(
    out: &mut [F],
    ys: &[F],
    n_points: usize,
    roots: &[F],
    invert: bool,
    mem: &mut PolyFFTTempMemory<F>,
) {
    fft_recurse(
        out,
        n_points,
        roots,
        ys,
        &mut mem.fft_tmp,
        &mut mem.fft_y_sub,
        &mut mem.fft_roots_sub,
    );
    if invert {
        let n_inverse = F::from(F::Integer::try_from(n_points).unwrap()).inv();
        for out_val in out[0..n_points].iter_mut() {
            *out_val *= n_inverse;
        }
    }
}

pub fn poly_fft<F: FftFriendlyFieldElement>(
    points_out: &mut [F],
    points_in: &[F],
    scaled_roots: &[F],
    n_points: usize,
    invert: bool,
    mem: &mut PolyFFTTempMemory<F>,
) {
    fft_interpolate_raw(points_out, points_in, n_points, scaled_roots, invert, mem)
}

// Evaluate a polynomial using Horner's method.
pub fn poly_eval<F: FftFriendlyFieldElement>(poly: &[F], eval_at: F) -> F {
    if poly.is_empty() {
        return F::zero();
    }

    let mut result = poly[poly.len() - 1];
    for i in (0..poly.len() - 1).rev() {
        result *= eval_at;
        result += poly[i];
    }

    result
}

// Returns the degree of polynomial `p`.
pub fn poly_deg<F: FftFriendlyFieldElement>(p: &[F]) -> usize {
    let mut d = p.len();
    while d > 0 && p[d - 1] == F::zero() {
        d -= 1;
    }
    d.saturating_sub(1)
}

// Multiplies polynomials `p` and `q` and returns the result.
pub fn poly_mul<F: FftFriendlyFieldElement>(p: &[F], q: &[F]) -> Vec<F> {
    let p_size = poly_deg(p) + 1;
    let q_size = poly_deg(q) + 1;
    let mut out = vec![F::zero(); p_size + q_size];
    for i in 0..p_size {
        for j in 0..q_size {
            out[i + j] += p[i] * q[j];
        }
    }
    out.truncate(poly_deg(&out) + 1);
    out
}

#[cfg(feature = "prio2")]
#[inline]
pub fn poly_interpret_eval<F: FftFriendlyFieldElement>(
    points: &[F],
    eval_at: F,
    tmp_coeffs: &mut [F],
) -> F {
    let size_inv = F::from(F::Integer::try_from(points.len()).unwrap()).inv();
    discrete_fourier_transform(tmp_coeffs, points, points.len()).unwrap();
    discrete_fourier_transform_inv_finish(tmp_coeffs, points.len(), size_inv);
    poly_eval(&tmp_coeffs[..points.len()], eval_at)
}

// Returns a polynomial that evaluates to `0` if the input is in range `[start, end)`. Otherwise,
// the output is not `0`.
pub(crate) fn poly_range_check<F: FftFriendlyFieldElement>(start: usize, end: usize) -> Vec<F> {
    let mut p = vec![F::one()];
    let mut q = [F::zero(), F::one()];
    for i in start..end {
        q[0] = -F::from(F::Integer::try_from(i).unwrap());
        p = poly_mul(&p, &q);
    }
    p
}

#[cfg(test)]
mod tests {
    use crate::{
        field::{
            FftFriendlyFieldElement, Field64, FieldElement, FieldElementWithInteger, FieldPrio2,
        },
        polynomial::{
            fft_get_roots, poly_deg, poly_eval, poly_fft, poly_mul, poly_range_check, PolyAuxMemory,
        },
    };
    use rand::prelude::*;
    use std::convert::TryFrom;

    #[test]
    fn test_roots() {
        let count = 128;
        let roots = fft_get_roots::<FieldPrio2>(count, false);
        let roots_inv = fft_get_roots::<FieldPrio2>(count, true);

        for i in 0..count {
            assert_eq!(roots[i] * roots_inv[i], 1);
            assert_eq!(roots[i].pow(u32::try_from(count).unwrap()), 1);
            assert_eq!(roots_inv[i].pow(u32::try_from(count).unwrap()), 1);
        }
    }

    #[test]
    fn test_eval() {
        let mut poly = [FieldPrio2::from(0); 4];
        poly[0] = 2.into();
        poly[1] = 1.into();
        poly[2] = 5.into();
        // 5*3^2 + 3 + 2 = 50
        assert_eq!(poly_eval(&poly[..3], 3.into()), 50);
        poly[3] = 4.into();
        // 4*3^3 + 5*3^2 + 3 + 2 = 158
        assert_eq!(poly_eval(&poly[..4], 3.into()), 158);
    }

    #[test]
    fn test_poly_deg() {
        let zero = FieldPrio2::zero();
        let one = FieldPrio2::root(0).unwrap();
        assert_eq!(poly_deg(&[zero]), 0);
        assert_eq!(poly_deg(&[one]), 0);
        assert_eq!(poly_deg(&[zero, one]), 1);
        assert_eq!(poly_deg(&[zero, zero, one]), 2);
        assert_eq!(poly_deg(&[zero, one, one]), 2);
        assert_eq!(poly_deg(&[zero, one, one, one]), 3);
        assert_eq!(poly_deg(&[zero, one, one, one, zero]), 3);
        assert_eq!(poly_deg(&[zero, one, one, one, zero, zero]), 3);
    }

    #[test]
    fn test_poly_mul() {
        let p = [
            Field64::from(u64::try_from(2).unwrap()),
            Field64::from(u64::try_from(3).unwrap()),
        ];

        let q = [
            Field64::one(),
            Field64::zero(),
            Field64::from(u64::try_from(5).unwrap()),
        ];

        let want = [
            Field64::from(u64::try_from(2).unwrap()),
            Field64::from(u64::try_from(3).unwrap()),
            Field64::from(u64::try_from(10).unwrap()),
            Field64::from(u64::try_from(15).unwrap()),
        ];

        let got = poly_mul(&p, &q);
        assert_eq!(&got, &want);
    }

    #[test]
    fn test_poly_range_check() {
        let start = 74;
        let end = 112;
        let p = poly_range_check(start, end);

        // Check each number in the range.
        for i in start..end {
            let x = Field64::from(i as u64);
            let y = poly_eval(&p, x);
            assert_eq!(y, Field64::zero(), "range check failed for {i}");
        }

        // Check the number below the range.
        let x = Field64::from((start - 1) as u64);
        let y = poly_eval(&p, x);
        assert_ne!(y, Field64::zero());

        // Check a number above the range.
        let x = Field64::from(end as u64);
        let y = poly_eval(&p, x);
        assert_ne!(y, Field64::zero());
    }

    #[test]
    fn test_fft() {
        let count = 128;
        let mut mem = PolyAuxMemory::new(count / 2);

        let mut poly = vec![FieldPrio2::from(0); count];
        let mut points2 = vec![FieldPrio2::from(0); count];

        let points = (0..count)
            .map(|_| FieldPrio2::from(random::<u32>()))
            .collect::<Vec<FieldPrio2>>();

        // From points to coeffs and back
        poly_fft(
            &mut poly,
            &points,
            &mem.roots_2n,
            count,
            false,
            &mut mem.fft_memory,
        );
        poly_fft(
            &mut points2,
            &poly,
            &mem.roots_2n_inverted,
            count,
            true,
            &mut mem.fft_memory,
        );

        assert_eq!(points, points2);

        // interpolation
        poly_fft(
            &mut poly,
            &points,
            &mem.roots_2n,
            count,
            false,
            &mut mem.fft_memory,
        );

        for (poly_coeff, root) in poly[..count].iter().zip(mem.roots_2n[..count].iter()) {
            let mut should_be = FieldPrio2::from(0);
            for (j, point_j) in points[..count].iter().enumerate() {
                should_be = root.pow(u32::try_from(j).unwrap()) * *point_j + should_be;
            }
            assert_eq!(should_be, *poly_coeff);
        }
    }
}
