// Copyright (c) 2023 ISRG
// SPDX-License-Identifier: MPL-2.0

//! Finite field arithmetic for `GF(2^255 - 19)`.

use crate::{
    codec::{CodecError, Decode, Encode},
    field::{FieldElement, FieldElementVisitor, FieldError},
};
use fiat_crypto::curve25519_64::{
    fiat_25519_add, fiat_25519_carry, fiat_25519_carry_mul, fiat_25519_from_bytes,
    fiat_25519_loose_field_element, fiat_25519_opp, fiat_25519_relax, fiat_25519_selectznz,
    fiat_25519_sub, fiat_25519_tight_field_element, fiat_25519_to_bytes,
};
use serde::{Deserialize, Deserializer, Serialize, Serializer};
use std::{
    convert::TryFrom,
    fmt::{self, Debug, Display, Formatter},
    io::{Cursor, Read},
    marker::PhantomData,
    mem::size_of,
    ops::{Add, AddAssign, Div, DivAssign, Mul, MulAssign, Neg, Sub, SubAssign},
};
use subtle::{
    Choice, ConditionallySelectable, ConstantTimeEq, ConstantTimeGreater, ConstantTimeLess,
};

// `python3 -c "print(', '.join(hex(x) for x in (2**255-19).to_bytes(32, 'little')))"`
const MODULUS_LITTLE_ENDIAN: [u8; 32] = [
    0xed, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f,
];

/// `GF(2^255 - 19)`, a 255-bit field.
#[derive(Clone, Copy)]
#[cfg_attr(docsrs, doc(cfg(feature = "experimental")))]
pub struct Field255(fiat_25519_tight_field_element);

impl Field255 {
    /// Attempts to instantiate a `Field255` from the first `Self::ENCODED_SIZE` bytes in the
    /// provided slice.
    ///
    /// # Errors
    ///
    /// An error is returned if the provided slice is not long enough to encode a field element or
    /// if the decoded value is greater than the field prime.
    fn try_from_bytes(bytes: &[u8], mask_top_bit: bool) -> Result<Self, FieldError> {
        if Self::ENCODED_SIZE > bytes.len() {
            return Err(FieldError::ShortRead);
        }

        let mut value = [0u8; Self::ENCODED_SIZE];
        value.copy_from_slice(&bytes[..Self::ENCODED_SIZE]);

        if mask_top_bit {
            value[31] &= 0b0111_1111;
        }

        // Walk through the bytes of the provided value from most significant to least,
        // and identify whether the first byte that differs from the field's modulus is less than
        // the corresponding byte or greater than the corresponding byte, in constant time. (Or
        // whether the provided value is equal to the field modulus.)
        let mut less_than_modulus = Choice::from(0u8);
        let mut greater_than_modulus = Choice::from(0u8);
        for (value_byte, modulus_byte) in value.iter().rev().zip(MODULUS_LITTLE_ENDIAN.iter().rev())
        {
            less_than_modulus |= value_byte.ct_lt(modulus_byte) & !greater_than_modulus;
            greater_than_modulus |= value_byte.ct_gt(modulus_byte) & !less_than_modulus;
        }

        if bool::from(!less_than_modulus) {
            return Err(FieldError::ModulusOverflow);
        }

        let mut output = fiat_25519_tight_field_element([0; 5]);
        fiat_25519_from_bytes(&mut output, &value);

        Ok(Field255(output))
    }
}

impl ConstantTimeEq for Field255 {
    fn ct_eq(&self, rhs: &Self) -> Choice {
        // The internal representation used by fiat-crypto is not 1-1 with the field, so it is
        // necessary to compare field elements via their canonical encodings.

        let mut self_encoded = [0; 32];
        fiat_25519_to_bytes(&mut self_encoded, &self.0);
        let mut rhs_encoded = [0; 32];
        fiat_25519_to_bytes(&mut rhs_encoded, &rhs.0);

        self_encoded.ct_eq(&rhs_encoded)
    }
}

impl ConditionallySelectable for Field255 {
    fn conditional_select(a: &Self, b: &Self, choice: Choice) -> Self {
        let mut output = [0; 5];
        fiat_25519_selectznz(&mut output, choice.unwrap_u8(), &(a.0).0, &(b.0).0);
        Field255(fiat_25519_tight_field_element(output))
    }
}

impl PartialEq for Field255 {
    fn eq(&self, rhs: &Self) -> bool {
        self.ct_eq(rhs).into()
    }
}

impl Eq for Field255 {}

impl Add for Field255 {
    type Output = Field255;

    fn add(self, rhs: Self) -> Field255 {
        let mut loose_output = fiat_25519_loose_field_element([0; 5]);
        fiat_25519_add(&mut loose_output, &self.0, &rhs.0);
        let mut output = fiat_25519_tight_field_element([0; 5]);
        fiat_25519_carry(&mut output, &loose_output);
        Field255(output)
    }
}

impl AddAssign for Field255 {
    fn add_assign(&mut self, rhs: Self) {
        let mut loose_output = fiat_25519_loose_field_element([0; 5]);
        fiat_25519_add(&mut loose_output, &self.0, &rhs.0);
        fiat_25519_carry(&mut self.0, &loose_output);
    }
}

impl Sub for Field255 {
    type Output = Field255;

    fn sub(self, rhs: Self) -> Field255 {
        let mut loose_output = fiat_25519_loose_field_element([0; 5]);
        fiat_25519_sub(&mut loose_output, &self.0, &rhs.0);
        let mut output = fiat_25519_tight_field_element([0; 5]);
        fiat_25519_carry(&mut output, &loose_output);
        Field255(output)
    }
}

impl SubAssign for Field255 {
    fn sub_assign(&mut self, rhs: Self) {
        let mut loose_output = fiat_25519_loose_field_element([0; 5]);
        fiat_25519_sub(&mut loose_output, &self.0, &rhs.0);
        fiat_25519_carry(&mut self.0, &loose_output);
    }
}

impl Mul for Field255 {
    type Output = Field255;

    fn mul(self, rhs: Self) -> Field255 {
        let mut self_relaxed = fiat_25519_loose_field_element([0; 5]);
        fiat_25519_relax(&mut self_relaxed, &self.0);
        let mut rhs_relaxed = fiat_25519_loose_field_element([0; 5]);
        fiat_25519_relax(&mut rhs_relaxed, &rhs.0);
        let mut output = fiat_25519_tight_field_element([0; 5]);
        fiat_25519_carry_mul(&mut output, &self_relaxed, &rhs_relaxed);
        Field255(output)
    }
}

impl MulAssign for Field255 {
    fn mul_assign(&mut self, rhs: Self) {
        let mut self_relaxed = fiat_25519_loose_field_element([0; 5]);
        fiat_25519_relax(&mut self_relaxed, &self.0);
        let mut rhs_relaxed = fiat_25519_loose_field_element([0; 5]);
        fiat_25519_relax(&mut rhs_relaxed, &rhs.0);
        fiat_25519_carry_mul(&mut self.0, &self_relaxed, &rhs_relaxed);
    }
}

impl Div for Field255 {
    type Output = Field255;

    fn div(self, _rhs: Self) -> Self::Output {
        unimplemented!("Div is not implemented for Field255 because it's not needed yet")
    }
}

impl DivAssign for Field255 {
    fn div_assign(&mut self, _rhs: Self) {
        unimplemented!("DivAssign is not implemented for Field255 because it's not needed yet")
    }
}

impl Neg for Field255 {
    type Output = Field255;

    fn neg(self) -> Field255 {
        -&self
    }
}

impl<'a> Neg for &'a Field255 {
    type Output = Field255;

    fn neg(self) -> Field255 {
        let mut loose_output = fiat_25519_loose_field_element([0; 5]);
        fiat_25519_opp(&mut loose_output, &self.0);
        let mut output = fiat_25519_tight_field_element([0; 5]);
        fiat_25519_carry(&mut output, &loose_output);
        Field255(output)
    }
}

impl From<u64> for Field255 {
    fn from(value: u64) -> Self {
        let input_bytes = value.to_le_bytes();
        let mut field_bytes = [0u8; Self::ENCODED_SIZE];
        field_bytes[..input_bytes.len()].copy_from_slice(&input_bytes);
        Self::try_from_bytes(&field_bytes, false).unwrap()
    }
}

impl<'a> TryFrom<&'a [u8]> for Field255 {
    type Error = FieldError;

    fn try_from(bytes: &[u8]) -> Result<Self, FieldError> {
        Self::try_from_bytes(bytes, false)
    }
}

impl From<Field255> for [u8; Field255::ENCODED_SIZE] {
    fn from(element: Field255) -> Self {
        let mut array = [0; Field255::ENCODED_SIZE];
        fiat_25519_to_bytes(&mut array, &element.0);
        array
    }
}

impl From<Field255> for Vec<u8> {
    fn from(elem: Field255) -> Vec<u8> {
        <[u8; Field255::ENCODED_SIZE]>::from(elem).to_vec()
    }
}

impl Display for Field255 {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        let encoded: [u8; Self::ENCODED_SIZE] = (*self).into();
        write!(f, "0x")?;
        for byte in encoded {
            write!(f, "{byte:02x}")?;
        }
        Ok(())
    }
}

impl Debug for Field255 {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        <Self as Display>::fmt(self, f)
    }
}

impl Serialize for Field255 {
    fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        let bytes: [u8; Self::ENCODED_SIZE] = (*self).into();
        serializer.serialize_bytes(&bytes)
    }
}

impl<'de> Deserialize<'de> for Field255 {
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Field255, D::Error> {
        deserializer.deserialize_bytes(FieldElementVisitor {
            phantom: PhantomData,
        })
    }
}

impl Encode for Field255 {
    fn encode(&self, bytes: &mut Vec<u8>) {
        bytes.extend_from_slice(&<[u8; Self::ENCODED_SIZE]>::from(*self));
    }

    fn encoded_len(&self) -> Option<usize> {
        Some(Self::ENCODED_SIZE)
    }
}

impl Decode for Field255 {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        let mut value = [0u8; Self::ENCODED_SIZE];
        bytes.read_exact(&mut value)?;
        Field255::try_from_bytes(&value, false).map_err(|e| {
            CodecError::Other(Box::new(e) as Box<dyn std::error::Error + 'static + Send + Sync>)
        })
    }
}

impl FieldElement for Field255 {
    const ENCODED_SIZE: usize = 32;

    fn inv(&self) -> Self {
        unimplemented!("Field255::inv() is not implemented because it's not needed yet")
    }

    fn try_from_random(bytes: &[u8]) -> Result<Self, FieldError> {
        Field255::try_from_bytes(bytes, true)
    }

    fn zero() -> Self {
        Field255(fiat_25519_tight_field_element([0, 0, 0, 0, 0]))
    }

    fn one() -> Self {
        Field255(fiat_25519_tight_field_element([1, 0, 0, 0, 0]))
    }
}

impl Default for Field255 {
    fn default() -> Self {
        Field255::zero()
    }
}

impl TryFrom<Field255> for u64 {
    type Error = FieldError;

    fn try_from(elem: Field255) -> Result<u64, FieldError> {
        const PREFIX_LEN: usize = size_of::<u64>();
        let mut le_bytes = [0; 32];

        fiat_25519_to_bytes(&mut le_bytes, &elem.0);
        if !bool::from(le_bytes[PREFIX_LEN..].ct_eq(&[0_u8; 32 - PREFIX_LEN])) {
            return Err(FieldError::IntegerTryFrom);
        }

        Ok(u64::from_le_bytes(
            le_bytes[..PREFIX_LEN].try_into().unwrap(),
        ))
    }
}

#[cfg(test)]
mod tests {
    use super::{Field255, MODULUS_LITTLE_ENDIAN};
    use crate::{
        codec::Encode,
        field::{
            test_utils::{field_element_test_common, TestFieldElementWithInteger},
            FieldElement, FieldError,
        },
    };
    use assert_matches::assert_matches;
    use fiat_crypto::curve25519_64::{
        fiat_25519_from_bytes, fiat_25519_tight_field_element, fiat_25519_to_bytes,
    };
    use num_bigint::BigUint;
    use once_cell::sync::Lazy;
    use std::convert::{TryFrom, TryInto};

    static MODULUS: Lazy<BigUint> = Lazy::new(|| BigUint::from_bytes_le(&MODULUS_LITTLE_ENDIAN));

    impl From<BigUint> for Field255 {
        fn from(value: BigUint) -> Self {
            let le_bytes_vec = (value % &*MODULUS).to_bytes_le();

            let mut le_bytes_array = [0u8; 32];
            le_bytes_array[..le_bytes_vec.len()].copy_from_slice(&le_bytes_vec);

            let mut output = fiat_25519_tight_field_element([0; 5]);
            fiat_25519_from_bytes(&mut output, &le_bytes_array);
            Field255(output)
        }
    }

    impl From<Field255> for BigUint {
        fn from(value: Field255) -> Self {
            let mut le_bytes = [0u8; 32];
            fiat_25519_to_bytes(&mut le_bytes, &value.0);
            BigUint::from_bytes_le(&le_bytes)
        }
    }

    impl TestFieldElementWithInteger for Field255 {
        type Integer = BigUint;
        type IntegerTryFromError = <Self::Integer as TryFrom<usize>>::Error;
        type TryIntoU64Error = <Self::Integer as TryInto<u64>>::Error;

        fn pow(&self, _exp: Self::Integer) -> Self {
            unimplemented!("Field255::pow() is not implemented because it's not needed yet")
        }

        fn modulus() -> Self::Integer {
            MODULUS.clone()
        }
    }

    #[test]
    fn check_modulus() {
        let modulus = Field255::modulus();
        let element = Field255::from(modulus);
        // Note that these two objects represent the same field element, they encode to the same
        // canonical value (32 zero bytes), but they do not have the same internal representation.
        assert_eq!(element, Field255::zero());
    }

    #[test]
    fn check_identities() {
        let zero_bytes: [u8; 32] = Field255::zero().into();
        assert_eq!(zero_bytes, [0; 32]);
        let one_bytes: [u8; 32] = Field255::one().into();
        assert_eq!(
            one_bytes,
            [
                1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0
            ]
        );
    }

    #[test]
    fn encode_endianness() {
        let mut one_encoded = Vec::new();
        Field255::one().encode(&mut one_encoded);
        assert_eq!(
            one_encoded,
            [
                1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0
            ]
        );
    }

    #[test]
    fn test_field255() {
        field_element_test_common::<Field255>();
    }

    #[test]
    fn try_from_bytes() {
        assert_matches!(
            Field255::try_from_bytes(
                &[
                    0xed, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                    0xff, 0xff, 0xff, 0xff, 0xff, 0x7f,
                ],
                false,
            ),
            Err(FieldError::ModulusOverflow)
        );
        assert_matches!(
            Field255::try_from_bytes(
                &[
                    0xee, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
                    0xff, 0xff, 0xff, 0xff, 0xff, 0x7f,
                ],
                false,
            ),
            Ok(_)
        );
        assert_matches!(
            Field255::try_from_bytes(
                &[
                    0xec, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                    0xff, 0xff, 0xff, 0xff, 0xff, 0x7f,
                ],
                true,
            ),
            Ok(element) => assert_eq!(element + Field255::one(), Field255::zero())
        );
        assert_matches!(
            Field255::try_from_bytes(
                &[
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
                ],
                false
            ),
            Err(FieldError::ModulusOverflow)
        );
        assert_matches!(
            Field255::try_from_bytes(
                &[
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
                ],
                true
            ),
            Ok(element) => assert_eq!(element, Field255::zero())
        );
    }

    #[test]
    fn u64_conversion() {
        assert_eq!(Field255::from(0u64), Field255::zero());
        assert_eq!(Field255::from(1u64), Field255::one());

        let max_bytes: [u8; 32] = Field255::from(u64::MAX).into();
        assert_eq!(
            max_bytes,
            [
                0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00
            ]
        );

        let want: u64 = 0xffffffffffffffff;
        assert_eq!(u64::try_from(Field255::from(want)).unwrap(), want);

        let want: u64 = 0x7000000000000001;
        assert_eq!(u64::try_from(Field255::from(want)).unwrap(), want);

        let want: u64 = 0x1234123412341234;
        assert_eq!(u64::try_from(Field255::from(want)).unwrap(), want);

        assert!(u64::try_from(Field255::try_from_bytes(&[1; 32], false).unwrap()).is_err());
        assert!(u64::try_from(Field255::try_from_bytes(&[2; 32], false).unwrap()).is_err());
    }

    #[test]
    fn formatting() {
        assert_eq!(
            format!("{}", Field255::zero()),
            "0x0000000000000000000000000000000000000000000000000000000000000000"
        );
        assert_eq!(
            format!("{}", Field255::one()),
            "0x0100000000000000000000000000000000000000000000000000000000000000"
        );
        assert_eq!(
            format!("{}", -Field255::one()),
            "0xecffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f"
        );
        assert_eq!(
            format!("{:?}", Field255::zero()),
            "0x0000000000000000000000000000000000000000000000000000000000000000"
        );
        assert_eq!(
            format!("{:?}", Field255::one()),
            "0x0100000000000000000000000000000000000000000000000000000000000000"
        );
    }
}
