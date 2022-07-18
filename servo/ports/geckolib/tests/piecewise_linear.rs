/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use euclid::approxeq::ApproxEq;
use style::piecewise_linear::{PiecewiseLinearFunction, PiecewiseLinearFunctionBuilder};

fn get_linear_keyword_equivalent() -> PiecewiseLinearFunction {
    PiecewiseLinearFunctionBuilder::new().build()
}
#[test]
fn linear_keyword_equivalent_in_bound() {
    let function = get_linear_keyword_equivalent();
    assert!(function.at(0.).approx_eq(&0.));
    assert!(function.at(0.5).approx_eq(&0.5));
    assert!(function.at(1.0).approx_eq(&1.0));
}

#[test]
fn linear_keyword_equivalent_out_of_bounds() {
    let function = get_linear_keyword_equivalent();
    assert!(function.at(-0.1).approx_eq(&-0.1));
    assert!(function.at(1.1).approx_eq(&1.1));
}

fn get_const_function() -> PiecewiseLinearFunction {
    PiecewiseLinearFunctionBuilder::new()
        .push(CONST_VALUE as f32, None)
        .build()
}

const CONST_VALUE: f32 = 0.2;

#[test]
fn const_function() {
    let function = get_const_function();
    assert!(function.at(0.).approx_eq(&CONST_VALUE));
    assert!(function.at(0.5).approx_eq(&CONST_VALUE));
    assert!(function.at(1.0).approx_eq(&CONST_VALUE));
}

#[test]
fn const_function_out_of_bounds() {
    let function = get_const_function();
    assert!(function.at(-0.1).approx_eq(&CONST_VALUE));
    assert!(function.at(1.1).approx_eq(&CONST_VALUE));
}

#[test]
fn implied_input_spacing() {
    let explicit_spacing = PiecewiseLinearFunctionBuilder::new()
        .push(0.0, Some(0.0))
        .push(1.0, Some(1.0))
        .build();
    let implied_spacing = PiecewiseLinearFunctionBuilder::new()
        .push(0.0, None)
        .push(1.0, None)
        .build();
    assert!(implied_spacing
        .at(0.)
        .approx_eq(&explicit_spacing.at(0.)));
    assert!(implied_spacing
        .at(0.5)
        .approx_eq(&explicit_spacing.at(0.5)));
    assert!(implied_spacing
        .at(1.0)
        .approx_eq(&explicit_spacing.at(1.0)));
}

#[test]
fn interpolation() {
    let interpolate = PiecewiseLinearFunctionBuilder::new()
        .push(0.0, None)
        .push(0.7, None)
        .push(1.0, None)
        .build();
    assert!(interpolate.at(0.1).approx_eq(&0.14));
    assert!(interpolate.at(0.25).approx_eq(&0.35));
    assert!(interpolate.at(0.45).approx_eq(&0.63));
    assert!(interpolate.at(0.7).approx_eq(&0.82));
    assert!(interpolate.at(0.75).approx_eq(&0.85));
    assert!(interpolate.at(0.95).approx_eq(&0.97));
}

#[test]
fn implied_multiple_input_spacing() {
    let multiple_implied = PiecewiseLinearFunctionBuilder::new()
        .push(0.0, None)
        .push(0.8, None)
        .push(0.6, None)
        .push(0.4, None)
        .push(0.5, Some(0.4))
        .push(0.1, None)
        .push(0.9, None)
        .push(1.0, None)
        .build();
    assert!(multiple_implied.at(0.1).approx_eq(&0.8));
    assert!(multiple_implied.at(0.2).approx_eq(&0.6));
    assert!(multiple_implied.at(0.3).approx_eq(&0.4));
    assert!(multiple_implied.at(0.4).approx_eq(&0.5));
    assert!(multiple_implied.at(0.6).approx_eq(&0.1));
    assert!(multiple_implied.at(0.8).approx_eq(&0.9));
    assert!(multiple_implied.at(1.).approx_eq(&1.));
}

#[test]
fn nonzero_edge_values() {
    let nonzero_edges = PiecewiseLinearFunctionBuilder::new()
        .push(0.1, Some(0.0))
        .push(0.7, Some(1.0))
        .build();
    assert!(nonzero_edges.at(0.).approx_eq(&0.1));
    assert!(nonzero_edges.at(0.5).approx_eq(&0.4));
    assert!(nonzero_edges.at(1.0).approx_eq(&0.7));
}

#[test]
fn out_of_bounds_extrapolate() {
    // General case: extrapolate from the edges' slope
    let oob_extend = PiecewiseLinearFunctionBuilder::new()
        .push(0.0, None)
        .push(0.7, None)
        .push(1.0, None)
        .build();
    assert!(oob_extend.at(-0.25).approx_eq(&-0.35));
    assert!(oob_extend.at(1.25).approx_eq(&1.15));
}

#[test]
fn out_of_bounds_flat() {
    // Repeated endpoints: flat extrapolation out-of-bounds
    let oob_flat = PiecewiseLinearFunctionBuilder::new()
        .push(0.0, Some(0.0))
        .push(0.0, Some(0.0))
        .push(0.7, None)
        .push(1.0, Some(1.0))
        .push(1.0, Some(1.0))
        .build();
    assert!(oob_flat
        .at(0.0)
        .approx_eq(&oob_flat.at(-0.25)));
    assert!(oob_flat.at(1.0).approx_eq(&oob_flat.at(1.25)));
}

#[test]
fn flat_region() {
    let flat = PiecewiseLinearFunctionBuilder::new()
        .push(0.0, Some(0.0))
        .push(0.5, Some(0.25))
        .push(0.5, Some(0.7))
        .push(1.0, Some(1.0))
        .build();
    assert!(flat.at(0.125).approx_eq(&0.25));
    assert!(flat.at(0.5).approx_eq(&0.5));
    assert!(flat.at(0.85).approx_eq(&0.75));
}

#[test]
fn step() {
    let step = PiecewiseLinearFunctionBuilder::new()
        .push(0.0, Some(0.0))
        .push(0.0, Some(0.5))
        .push(1.0, Some(0.5))
        .push(1.0, Some(1.0))
        .build();
    assert!(step.at(0.25).approx_eq(&0.0));
    // At the discontinuity, take the right hand side value
    assert!(step.at(0.5).approx_eq(&1.0));
    assert!(step.at(0.75).approx_eq(&1.0));
}

#[test]
fn step_multiple_conflicting() {
    let step = PiecewiseLinearFunctionBuilder::new()
        .push(0.0, Some(0.0))
        .push(0.0, Some(0.5))
        .push(0.75, Some(0.5))
        .push(0.75, Some(0.5))
        .push(1.0, Some(0.5))
        .push(1.0, Some(1.0))
        .build();
    assert!(step.at(0.25).approx_eq(&0.0));
    assert!(step.at(0.5).approx_eq(&1.0));
    assert!(step.at(0.75).approx_eq(&1.0));
}

#[test]
fn always_monotonic() {
    let monotonic = PiecewiseLinearFunctionBuilder::new()
        .push(0.0, Some(0.0))
        .push(0.3, Some(0.5))
        .push(0.4, Some(0.4))
        .push(1.0, Some(1.0))
        .build();
    assert!(monotonic.at(0.25).approx_eq(&0.15));
    // A discontinuity at x = 0.5 from y = 0.3 to 0.4
    assert!(monotonic.at(0.5).approx_eq(&0.4));
    assert!(monotonic.at(0.6).approx_eq(&0.52));
}

#[test]
fn always_monotonic_flat() {
    let monotonic = PiecewiseLinearFunctionBuilder::new()
        .push(0.0, Some(0.0))
        .push(0.2, Some(0.2))
        .push(0.4, Some(0.1))
        .push(0.4, Some(0.15))
        .push(1.0, Some(1.0))
        .build();
    assert!(monotonic.at(0.2).approx_eq(&0.4));
    // A discontinuity at x = 0.2 from y = 0.2 to 0.4
    assert!(monotonic.at(0.3).approx_eq(&0.475));
}

#[test]
fn always_monotonic_flat_backwards() {
    let monotonic = PiecewiseLinearFunctionBuilder::new()
        .push(0.0, Some(0.0))
        .push(0.2, Some(0.2))
        .push(0.3, Some(0.3))
        .push(0.3, Some(0.2))
        .push(1.0, Some(1.0))
        .build();
    assert!(monotonic.at(0.2).approx_eq(&0.2));
    assert!(monotonic.at(0.3).approx_eq(&0.3));
    assert!(monotonic.at(0.4).approx_eq(&0.4));
}

#[test]
fn input_out_of_bounds() {
    let oob = PiecewiseLinearFunctionBuilder::new()
        .push(0.0, Some(-0.5))
        .push(1.0, Some(1.5))
        .build();
    assert!(oob.at(-0.5).approx_eq(&0.0));
    assert!(oob.at(0.0).approx_eq(&0.25));
    assert!(oob.at(0.5).approx_eq(&0.5));
    assert!(oob.at(1.0).approx_eq(&0.75));
    assert!(oob.at(1.5).approx_eq(&1.0));
}

#[test]
fn invalid_builder_input() {
    let built_from_invalid = PiecewiseLinearFunctionBuilder::new()
        .push(0.0, Some(f32::NEG_INFINITY))
        .push(0.7, Some(f32::NAN))
        .push(1.0, Some(f32::INFINITY))
        .build();
    let equivalent = PiecewiseLinearFunctionBuilder::new()
        .push(0.0, None)
        .push(0.7, None)
        .push(1.0, None)
        .build();

    assert!(built_from_invalid.at(0.0).approx_eq(&equivalent.at(0.0)));
    assert!(built_from_invalid.at(0.25).approx_eq(&equivalent.at(0.25)));
    assert!(built_from_invalid.at(0.5).approx_eq(&equivalent.at(0.5)));
    assert!(built_from_invalid.at(0.75).approx_eq(&equivalent.at(0.75)));
    assert!(built_from_invalid.at(1.0).approx_eq(&equivalent.at(1.0)));
}

#[test]
fn input_domain_not_complete() {
    let not_covered = PiecewiseLinearFunctionBuilder::new()
        .push(0.2, Some(0.2))
        .push(0.8, Some(0.8))
        .build();
    assert!(not_covered.at(0.0).approx_eq(&0.0));
    assert!(not_covered.at(0.5).approx_eq(&0.5));
    assert!(not_covered.at(1.0).approx_eq(&1.0));
}

#[test]
fn input_second_negative() {
    let function = PiecewiseLinearFunctionBuilder::new()
        .push(0.0, None)
        .push(0.0, Some(-0.1))
        .push(0.3, Some(-0.05))
        .push(0.5, None)
        .push(0.2, Some(0.6))
        .push(1.0, None)
        .build();
    let equivalent = PiecewiseLinearFunctionBuilder::new()
        .push(0.0, Some(0.0))
        .push(0.0, Some(0.0))
        .push(0.3, Some(0.0))
        .push(0.5, Some(0.3))
        .push(0.2, Some(0.6))
        .push(1.0, Some(1.0))
        .build();
    assert!(function.at(-0.1).approx_eq(&equivalent.at(-0.1)));
    assert!(function.at(0.0).approx_eq(&equivalent.at(0.0)));
    assert!(function.at(0.3).approx_eq(&equivalent.at(0.3)));
    assert!(function.at(0.6).approx_eq(&equivalent.at(0.6)));
    assert!(function.at(1.0).approx_eq(&equivalent.at(1.0)));
}

#[test]
fn input_second_last_above_1() {
    let function = PiecewiseLinearFunctionBuilder::new()
        .push(0.0, Some(0.0))
        .push(1.0, Some(2.0))
        .push(1.0, None)
        .build();
    assert!(function.at(-0.5).approx_eq(&-0.25));
    assert!(function.at(0.0).approx_eq(&0.0));
    assert!(function.at(0.5).approx_eq(&0.25));
    assert!(function.at(1.0).approx_eq(&0.5));
    assert!(function.at(1.5).approx_eq(&0.75));
    assert!(function.at(2.0).approx_eq(&1.0));
    assert!(function.at(3.0).approx_eq(&1.0));
}
