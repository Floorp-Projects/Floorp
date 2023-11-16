// SPDX-License-Identifier: MPL-2.0

//! Implementations of encoding fixed point types as field elements and field elements as floats
//! for the [`FixedPointBoundedL2VecSum`](crate::flp::types::fixedpoint_l2::FixedPointBoundedL2VecSum) type.

use crate::field::{Field128, FieldElementWithInteger};
use fixed::types::extra::{U15, U31, U63};
use fixed::{FixedI16, FixedI32, FixedI64};

/// Assign a `Float` type to this type and describe how to represent this type as an integer of the
/// given field, and how to represent a field element as the assigned `Float` type.
pub trait CompatibleFloat {
    /// Represent a field element as `Float`, given the number of clients `c`.
    fn to_float(t: Field128, c: u128) -> f64;

    /// Represent a value of this type as an integer in the given field.
    fn to_field_integer(&self) -> <Field128 as FieldElementWithInteger>::Integer;
}

impl CompatibleFloat for FixedI16<U15> {
    fn to_float(d: Field128, c: u128) -> f64 {
        to_float_bits(d, c, 16)
    }

    fn to_field_integer(&self) -> <Field128 as FieldElementWithInteger>::Integer {
        //signed two's complement integer representation
        let i: i16 = self.to_bits();
        // reinterpret as unsigned
        let u = i as u16;
        // invert the left-most bit to de-two-complement
        u128::from(u ^ (1 << 15))
    }
}

impl CompatibleFloat for FixedI32<U31> {
    fn to_float(d: Field128, c: u128) -> f64 {
        to_float_bits(d, c, 32)
    }

    fn to_field_integer(&self) -> <Field128 as FieldElementWithInteger>::Integer {
        //signed two's complement integer representation
        let i: i32 = self.to_bits();
        // reinterpret as unsigned
        let u = i as u32;
        // invert the left-most bit to de-two-complement
        u128::from(u ^ (1 << 31))
    }
}

impl CompatibleFloat for FixedI64<U63> {
    fn to_float(d: Field128, c: u128) -> f64 {
        to_float_bits(d, c, 64)
    }

    fn to_field_integer(&self) -> <Field128 as FieldElementWithInteger>::Integer {
        //signed two's complement integer representation
        let i: i64 = self.to_bits();
        // reinterpret as unsigned
        let u = i as u64;
        // invert the left-most bit to de-two-complement
        u128::from(u ^ (1 << 63))
    }
}

/// Return an `f64` representation of the field element `s`, assuming it is the computation result
/// of a `c`-client fixed point vector summation with `n` fractional bits.
fn to_float_bits(s: Field128, c: u128, n: i32) -> f64 {
    // get integer representation of field element
    let s_int: u128 = <Field128 as FieldElementWithInteger>::Integer::from(s);

    // to decode a single integer, we'd use the function
    //   dec(y) = (y - 2^(n-1)) * 2^(1-n) = y * 2^(1-n) - 1
    // as s is the sum of c encoded vector entries where c is the number of
    // clients, we have to compute instead
    //   s * 2^(1-n) - c
    //
    // Furthermore, for better numerical stability, we reformulate this as
    //   = (s - c*2^(n-1)) * 2^(1-n)
    // where the subtraction of `c` is done on integers and only afterwards
    // the conversion to floats is done.
    //
    // Since the RHS of the substraction may be larger than the LHS
    // (when the number we are decoding is going to be negative),
    // yet we are dealing with unsigned 128-bit integers, we manually
    // check for the resulting sign while ensuring that the subtraction
    // does not underflow.
    let (a, b, sign) = match (s_int, c << (n - 1)) {
        (x, y) if x < y => (y, x, -1.0f64),
        (x, y) => (x, y, 1.0f64),
    };

    ((a - b) as f64) * sign * f64::powi(2.0, 1 - n)
}
