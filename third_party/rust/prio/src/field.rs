// Copyright (c) 2020 Apple Inc.
// SPDX-License-Identifier: MPL-2.0

//! Finite field arithmetic.
//!
//! Basic field arithmetic is captured in the [`FieldElement`] trait. Fields used in Prio implement
//! [`FftFriendlyFieldElement`], and have an associated element called the "generator" that
//! generates a multiplicative subgroup of order `2^n` for some `n`.

#[cfg(feature = "crypto-dependencies")]
use crate::prng::{Prng, PrngError};
use crate::{
    codec::{CodecError, Decode, Encode},
    fp::{FP128, FP32, FP64},
};
use serde::{
    de::{DeserializeOwned, Visitor},
    Deserialize, Deserializer, Serialize, Serializer,
};
use std::{
    cmp::min,
    convert::{TryFrom, TryInto},
    fmt::{self, Debug, Display, Formatter},
    hash::{Hash, Hasher},
    io::{Cursor, Read},
    marker::PhantomData,
    ops::{
        Add, AddAssign, BitAnd, ControlFlow, Div, DivAssign, Mul, MulAssign, Neg, Shl, Shr, Sub,
        SubAssign,
    },
};
use subtle::{Choice, ConditionallyNegatable, ConditionallySelectable, ConstantTimeEq};

#[cfg(feature = "experimental")]
mod field255;

#[cfg(feature = "experimental")]
pub use field255::Field255;

/// Possible errors from finite field operations.
#[derive(Debug, thiserror::Error)]
pub enum FieldError {
    /// Input sizes do not match.
    #[error("input sizes do not match")]
    InputSizeMismatch,
    /// Returned when decoding a [`FieldElement`] from a too-short byte string.
    #[error("short read from bytes")]
    ShortRead,
    /// Returned when decoding a [`FieldElement`] from a byte string that encodes an integer greater
    /// than or equal to the field modulus.
    #[error("read from byte slice exceeds modulus")]
    ModulusOverflow,
    /// Error while performing I/O.
    #[error("I/O error")]
    Io(#[from] std::io::Error),
    /// Error encoding or decoding a field.
    #[error("Codec error")]
    Codec(#[from] CodecError),
    /// Error converting to [`FieldElementWithInteger::Integer`].
    #[error("Integer TryFrom error")]
    IntegerTryFrom,
}

/// Objects with this trait represent an element of `GF(p)` for some prime `p`.
pub trait FieldElement:
    Sized
    + Debug
    + Copy
    + PartialEq
    + Eq
    + ConstantTimeEq
    + ConditionallySelectable
    + ConditionallyNegatable
    + Add<Output = Self>
    + AddAssign
    + Sub<Output = Self>
    + SubAssign
    + Mul<Output = Self>
    + MulAssign
    + Div<Output = Self>
    + DivAssign
    + Neg<Output = Self>
    + Display
    + for<'a> TryFrom<&'a [u8], Error = FieldError>
    // NOTE Ideally we would require `Into<[u8; Self::ENCODED_SIZE]>` instead of `Into<Vec<u8>>`,
    // since the former avoids a heap allocation and can easily be converted into Vec<u8>, but that
    // isn't possible yet[1]. However we can provide the impl on FieldElement implementations.
    // [1]: https://github.com/rust-lang/rust/issues/60551
    + Into<Vec<u8>>
    + Serialize
    + DeserializeOwned
    + Encode
    + Decode
    + 'static // NOTE This bound is needed for downcasting a `dyn Gadget<F>>` to a concrete type.
{
    /// Size in bytes of an encoded field element.
    const ENCODED_SIZE: usize;

    /// Modular inversion, i.e., `self^-1 (mod p)`. If `self` is 0, then the output is undefined.
    fn inv(&self) -> Self;

    /// Interprets the next [`Self::ENCODED_SIZE`] bytes from the input slice as an element of the
    /// field. The `m` most significant bits are cleared, where `m` is equal to the length of
    /// [`Self::Integer`] in bits minus the length of the modulus in bits.
    ///
    /// # Errors
    ///
    /// An error is returned if the provided slice is too small to encode a field element or if the
    /// result encodes an integer larger than or equal to the field modulus.
    ///
    /// # Warnings
    ///
    /// This function should only be used within [`prng::Prng`] to convert a random byte string into
    /// a field element. Use [`Self::decode`] to deserialize field elements. Use
    /// [`field::rand`] or [`prng::Prng`] to randomly generate field elements.
    #[doc(hidden)]
    fn try_from_random(bytes: &[u8]) -> Result<Self, FieldError>;

    /// Returns the additive identity.
    fn zero() -> Self;

    /// Returns the multiplicative identity.
    fn one() -> Self;

    /// Convert a slice of field elements into a vector of bytes.
    ///
    /// # Notes
    ///
    /// Ideally we would implement `From<&[F: FieldElement]> for Vec<u8>` or the corresponding
    /// `Into`, but the orphan rule and the stdlib's blanket implementations of `Into` make this
    /// impossible.
    fn slice_into_byte_vec(values: &[Self]) -> Vec<u8> {
        let mut vec = Vec::with_capacity(values.len() * Self::ENCODED_SIZE);
        encode_fieldvec(values, &mut vec);
        vec
    }

    /// Convert a slice of bytes into a vector of field elements. The slice is interpreted as a
    /// sequence of [`Self::ENCODED_SIZE`]-byte sequences.
    ///
    /// # Errors
    ///
    /// Returns an error if the length of the provided byte slice is not a multiple of the size of a
    /// field element, or if any of the values in the byte slice are invalid encodings of a field
    /// element, because the encoded integer is larger than or equal to the field modulus.
    ///
    /// # Notes
    ///
    /// Ideally we would implement `From<&[u8]> for Vec<F: FieldElement>` or the corresponding
    /// `Into`, but the orphan rule and the stdlib's blanket implementations of `Into` make this
    /// impossible.
    fn byte_slice_into_vec(bytes: &[u8]) -> Result<Vec<Self>, FieldError> {
        if bytes.len() % Self::ENCODED_SIZE != 0 {
            return Err(FieldError::ShortRead);
        }
        let mut vec = Vec::with_capacity(bytes.len() / Self::ENCODED_SIZE);
        for chunk in bytes.chunks_exact(Self::ENCODED_SIZE) {
            vec.push(Self::get_decoded(chunk)?);
        }
        Ok(vec)
    }
}

/// Extension trait for field elements that can be converted back and forth to an integer type.
///
/// The `Integer` associated type is an integer (primitive or otherwise) that supports various
/// arithmetic operations. The order of the field is guaranteed to fit inside the range of the
/// integer type. This trait also defines methods on field elements, `pow` and `modulus`, that make
/// use of the associated integer type.
pub trait FieldElementWithInteger: FieldElement + From<Self::Integer> {
    /// The error returned if converting `usize` to an `Integer` fails.
    type IntegerTryFromError: std::error::Error;

    /// The error returned if converting an `Integer` to a `u64` fails.
    type TryIntoU64Error: std::error::Error;

    /// The integer representation of a field element.
    type Integer: Copy
        + Debug
        + Eq
        + Ord
        + BitAnd<Output = Self::Integer>
        + Div<Output = Self::Integer>
        + Shl<usize, Output = Self::Integer>
        + Shr<usize, Output = Self::Integer>
        + Add<Output = Self::Integer>
        + Sub<Output = Self::Integer>
        + From<Self>
        + TryFrom<usize, Error = Self::IntegerTryFromError>
        + TryInto<u64, Error = Self::TryIntoU64Error>;

    /// Modular exponentation, i.e., `self^exp (mod p)`.
    fn pow(&self, exp: Self::Integer) -> Self;

    /// Returns the prime modulus `p`.
    fn modulus() -> Self::Integer;
}

/// Methods common to all `FieldElementWithInteger` implementations that are private to the crate.
pub(crate) trait FieldElementWithIntegerExt: FieldElementWithInteger {
    /// Encode `input` as bitvector of elements of `Self`. Output is written into the `output` slice.
    /// If `output.len()` is smaller than the number of bits required to respresent `input`,
    /// an error is returned.
    ///
    /// # Arguments
    ///
    /// * `input` - The field element to encode
    /// * `output` - The slice to write the encoded bits into. Least signicant bit comes first
    fn fill_with_bitvector_representation(
        input: &Self::Integer,
        output: &mut [Self],
    ) -> Result<(), FieldError> {
        // Create a mutable copy of `input`. In each iteration of the following loop we take the
        // least significant bit, and shift input to the right by one bit.
        let mut i = *input;

        let one = Self::Integer::from(Self::one());
        for bit in output.iter_mut() {
            let w = Self::from(i & one);
            *bit = w;
            i = i >> 1;
        }

        // If `i` is still not zero, this means that it cannot be encoded by `bits` bits.
        if i != Self::Integer::from(Self::zero()) {
            return Err(FieldError::InputSizeMismatch);
        }

        Ok(())
    }

    /// Encode `input` as `bits`-bit vector of elements of `Self` if it's small enough
    /// to be represented with that many bits.
    ///
    /// # Arguments
    ///
    /// * `input` - The field element to encode
    /// * `bits` - The number of bits to use for the encoding
    fn encode_into_bitvector_representation(
        input: &Self::Integer,
        bits: usize,
    ) -> Result<Vec<Self>, FieldError> {
        let mut result = vec![Self::zero(); bits];
        Self::fill_with_bitvector_representation(input, &mut result)?;
        Ok(result)
    }

    /// Decode the bitvector-represented value `input` into a simple representation as a single
    /// field element.
    ///
    /// # Errors
    ///
    /// This function errors if `2^input.len() - 1` does not fit into the field `Self`.
    fn decode_from_bitvector_representation(input: &[Self]) -> Result<Self, FieldError> {
        let fi_one = Self::Integer::from(Self::one());

        if !Self::valid_integer_bitlength(input.len()) {
            return Err(FieldError::ModulusOverflow);
        }

        let mut decoded = Self::zero();
        for (l, bit) in input.iter().enumerate() {
            let w = fi_one << l;
            decoded += Self::from(w) * *bit;
        }
        Ok(decoded)
    }

    /// Interpret `i` as [`Self::Integer`] if it's representable in that type and smaller than the
    /// field modulus.
    fn valid_integer_try_from<N>(i: N) -> Result<Self::Integer, FieldError>
    where
        Self::Integer: TryFrom<N>,
    {
        let i_int = Self::Integer::try_from(i).map_err(|_| FieldError::IntegerTryFrom)?;
        if Self::modulus() <= i_int {
            return Err(FieldError::ModulusOverflow);
        }
        Ok(i_int)
    }

    /// Check if the largest number representable with `bits` bits (i.e. 2^bits - 1) is
    /// representable in this field.
    fn valid_integer_bitlength(bits: usize) -> bool {
        if bits >= 8 * Self::ENCODED_SIZE {
            return false;
        }
        if Self::modulus() >> bits != Self::Integer::from(Self::zero()) {
            return true;
        }
        false
    }
}

impl<F: FieldElementWithInteger> FieldElementWithIntegerExt for F {}

/// Methods common to all `FieldElement` implementations that are private to the crate.
pub(crate) trait FieldElementExt: FieldElement {
    /// Try to interpret a slice of [`Self::ENCODED_SIZE`] random bytes as an element in the field. If
    /// the input represents an integer greater than or equal to the field modulus, then
    /// [`ControlFlow::Continue`] is returned instead, to indicate that an enclosing rejection sampling
    /// loop should try again with different random bytes.
    ///
    /// # Panics
    ///
    /// Panics if `bytes` is not of length [`Self::ENCODED_SIZE`].
    fn from_random_rejection(bytes: &[u8]) -> ControlFlow<Self, ()> {
        match Self::try_from_random(bytes) {
            Ok(x) => ControlFlow::Break(x),
            Err(FieldError::ModulusOverflow) => ControlFlow::Continue(()),
            Err(err) => panic!("unexpected error: {err}"),
        }
    }
}

impl<F: FieldElement> FieldElementExt for F {}

/// serde Visitor implementation used to generically deserialize `FieldElement`
/// values from byte arrays.
pub(crate) struct FieldElementVisitor<F: FieldElement> {
    pub(crate) phantom: PhantomData<F>,
}

impl<'de, F: FieldElement> Visitor<'de> for FieldElementVisitor<F> {
    type Value = F;

    fn expecting(&self, formatter: &mut Formatter) -> fmt::Result {
        formatter.write_fmt(format_args!("an array of {} bytes", F::ENCODED_SIZE))
    }

    fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        Self::Value::try_from(v).map_err(E::custom)
    }

    fn visit_seq<A>(self, mut seq: A) -> Result<Self::Value, A::Error>
    where
        A: serde::de::SeqAccess<'de>,
    {
        let mut bytes = vec![];
        while let Some(byte) = seq.next_element()? {
            bytes.push(byte);
        }

        self.visit_bytes(&bytes)
    }
}

/// Objects with this trait represent an element of `GF(p)`, where `p` is some prime and the
/// field's multiplicative group has a subgroup with an order that is a power of 2, and at least
/// `2^20`.
pub trait FftFriendlyFieldElement: FieldElementWithInteger {
    /// Returns the size of the multiplicative subgroup generated by
    /// [`FftFriendlyFieldElement::generator`].
    fn generator_order() -> Self::Integer;

    /// Returns the generator of the multiplicative subgroup of size
    /// [`FftFriendlyFieldElement::generator_order`].
    fn generator() -> Self;

    /// Returns the `2^l`-th principal root of unity for any `l <= 20`. Note that the `2^0`-th
    /// prinicpal root of unity is `1` by definition.
    fn root(l: usize) -> Option<Self>;
}

macro_rules! make_field {
    (
        $(#[$meta:meta])*
        $elem:ident, $int:ident, $fp:ident, $encoding_size:literal,
    ) => {
        $(#[$meta])*
        ///
        /// This structure represents a field element in a prime order field. The concrete
        /// representation of the element is via the Montgomery domain. For an element `n` in
        /// `GF(p)`, we store `n * R^-1 mod p` (where `R` is a given power of two). This
        /// representation enables using a more efficient (and branchless) multiplication algorithm,
        /// at the expense of having to convert elements between their Montgomery domain
        /// representation and natural representation. For calculations with many multiplications or
        /// exponentiations, this is worthwhile.
        ///
        /// As an invariant, this integer representing the field element in the Montgomery domain
        /// must be less than the field modulus, `p`.
        #[derive(Clone, Copy, PartialOrd, Ord, Default)]
        pub struct $elem(u128);

        impl $elem {
            /// Attempts to instantiate an `$elem` from the first `Self::ENCODED_SIZE` bytes in the
            /// provided slice. The decoded value will be bitwise-ANDed with `mask` before reducing
            /// it using the field modulus.
            ///
            /// # Errors
            ///
            /// An error is returned if the provided slice is not long enough to encode a field
            /// element or if the decoded value is greater than the field prime.
            ///
            /// # Notes
            ///
            /// We cannot use `u128::from_le_bytes` or `u128::from_be_bytes` because those functions
            /// expect inputs to be exactly 16 bytes long. Our encoding of most field elements is
            /// more compact.
            fn try_from_bytes(bytes: &[u8], mask: u128) -> Result<Self, FieldError> {
                if Self::ENCODED_SIZE > bytes.len() {
                    return Err(FieldError::ShortRead);
                }

                let mut int = 0;
                for i in 0..Self::ENCODED_SIZE {
                    int |= (bytes[i] as u128) << (i << 3);
                }

                int &= mask;

                if int >= $fp.p {
                    return Err(FieldError::ModulusOverflow);
                }
                // FieldParameters::montgomery() will return a value that has been fully reduced
                // mod p, satisfying the invariant on Self.
                Ok(Self($fp.montgomery(int)))
            }
        }

        impl PartialEq for $elem {
            fn eq(&self, rhs: &Self) -> bool {
                // The fields included in this comparison MUST match the fields
                // used in Hash::hash
                // https://doc.rust-lang.org/std/hash/trait.Hash.html#hash-and-eq

                // Check the invariant that the integer representation is fully reduced.
                debug_assert!(self.0 < $fp.p);
                debug_assert!(rhs.0 < $fp.p);

                self.0 == rhs.0
            }
        }

        impl ConstantTimeEq for $elem {
            fn ct_eq(&self, rhs: &Self) -> Choice {
                self.0.ct_eq(&rhs.0)
            }
        }

        impl ConditionallySelectable for $elem {
            fn conditional_select(a: &Self, b: &Self, choice: subtle::Choice) -> Self {
                Self(u128::conditional_select(&a.0, &b.0, choice))
            }
        }

        impl Hash for $elem {
            fn hash<H: Hasher>(&self, state: &mut H) {
                // The fields included in this hash MUST match the fields used
                // in PartialEq::eq
                // https://doc.rust-lang.org/std/hash/trait.Hash.html#hash-and-eq

                // Check the invariant that the integer representation is fully reduced.
                debug_assert!(self.0 < $fp.p);

                self.0.hash(state);
            }
        }

        impl Eq for $elem {}

        impl Add for $elem {
            type Output = $elem;
            fn add(self, rhs: Self) -> Self {
                // FieldParameters::add() returns a value that has been fully reduced
                // mod p, satisfying the invariant on Self.
                Self($fp.add(self.0, rhs.0))
            }
        }

        impl Add for &$elem {
            type Output = $elem;
            fn add(self, rhs: Self) -> $elem {
                *self + *rhs
            }
        }

        impl AddAssign for $elem {
            fn add_assign(&mut self, rhs: Self) {
                *self = *self + rhs;
            }
        }

        impl Sub for $elem {
            type Output = $elem;
            fn sub(self, rhs: Self) -> Self {
                // We know that self.0 and rhs.0 are both less than p, thus FieldParameters::sub()
                // returns a value less than p, satisfying the invariant on Self.
                Self($fp.sub(self.0, rhs.0))
            }
        }

        impl Sub for &$elem {
            type Output = $elem;
            fn sub(self, rhs: Self) -> $elem {
                *self - *rhs
            }
        }

        impl SubAssign for $elem {
            fn sub_assign(&mut self, rhs: Self) {
                *self = *self - rhs;
            }
        }

        impl Mul for $elem {
            type Output = $elem;
            fn mul(self, rhs: Self) -> Self {
                // FieldParameters::mul() always returns a value less than p, so the invariant on
                // Self is satisfied.
                Self($fp.mul(self.0, rhs.0))
            }
        }

        impl Mul for &$elem {
            type Output = $elem;
            fn mul(self, rhs: Self) -> $elem {
                *self * *rhs
            }
        }

        impl MulAssign for $elem {
            fn mul_assign(&mut self, rhs: Self) {
                *self = *self * rhs;
            }
        }

        impl Div for $elem {
            type Output = $elem;
            #[allow(clippy::suspicious_arithmetic_impl)]
            fn div(self, rhs: Self) -> Self {
                self * rhs.inv()
            }
        }

        impl Div for &$elem {
            type Output = $elem;
            fn div(self, rhs: Self) -> $elem {
                *self / *rhs
            }
        }

        impl DivAssign for $elem {
            fn div_assign(&mut self, rhs: Self) {
                *self = *self / rhs;
            }
        }

        impl Neg for $elem {
            type Output = $elem;
            fn neg(self) -> Self {
                // FieldParameters::neg() will return a value less than p because self.0 is less
                // than p, and neg() dispatches to sub().
                Self($fp.neg(self.0))
            }
        }

        impl Neg for &$elem {
            type Output = $elem;
            fn neg(self) -> $elem {
                -(*self)
            }
        }

        impl From<$int> for $elem {
            fn from(x: $int) -> Self {
                // FieldParameters::montgomery() will return a value that has been fully reduced
                // mod p, satisfying the invariant on Self.
                Self($fp.montgomery(u128::try_from(x).unwrap()))
            }
        }

        impl From<$elem> for $int {
            fn from(x: $elem) -> Self {
                $int::try_from($fp.residue(x.0)).unwrap()
            }
        }

        impl PartialEq<$int> for $elem {
            fn eq(&self, rhs: &$int) -> bool {
                $fp.residue(self.0) == u128::try_from(*rhs).unwrap()
            }
        }

        impl<'a> TryFrom<&'a [u8]> for $elem {
            type Error = FieldError;

            fn try_from(bytes: &[u8]) -> Result<Self, FieldError> {
                Self::try_from_bytes(bytes, u128::MAX)
            }
        }

        impl From<$elem> for [u8; $elem::ENCODED_SIZE] {
            fn from(elem: $elem) -> Self {
                let int = $fp.residue(elem.0);
                let mut slice = [0; $elem::ENCODED_SIZE];
                for i in 0..$elem::ENCODED_SIZE {
                    slice[i] = ((int >> (i << 3)) & 0xff) as u8;
                }
                slice
            }
        }

        impl From<$elem> for Vec<u8> {
            fn from(elem: $elem) -> Self {
                <[u8; $elem::ENCODED_SIZE]>::from(elem).to_vec()
            }
        }

        impl Display for $elem {
            fn fmt(&self, f: &mut Formatter) -> std::fmt::Result {
                write!(f, "{}", $fp.residue(self.0))
            }
        }

        impl Debug for $elem {
            fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
                write!(f, "{}", $fp.residue(self.0))
            }
        }

        // We provide custom [`serde::Serialize`] and [`serde::Deserialize`] implementations because
        // the derived implementations would represent `FieldElement` values as the backing `u128`,
        // which is not what we want because (1) we can be more efficient in all cases and (2) in
        // some circumstances, [some serializers don't support `u128`](https://github.com/serde-rs/json/issues/625).
        impl Serialize for $elem {
            fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
                let bytes: [u8; $elem::ENCODED_SIZE] = (*self).into();
                serializer.serialize_bytes(&bytes)
            }
        }

        impl<'de> Deserialize<'de> for $elem {
            fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<$elem, D::Error> {
                deserializer.deserialize_bytes(FieldElementVisitor { phantom: PhantomData })
            }
        }

        impl Encode for $elem {
            fn encode(&self, bytes: &mut Vec<u8>) {
                let slice = <[u8; $elem::ENCODED_SIZE]>::from(*self);
                bytes.extend_from_slice(&slice);
            }

            fn encoded_len(&self) -> Option<usize> {
                Some(Self::ENCODED_SIZE)
            }
        }

        impl Decode for $elem {
            fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
                let mut value = [0u8; $elem::ENCODED_SIZE];
                bytes.read_exact(&mut value)?;
                $elem::try_from_bytes(&value, u128::MAX).map_err(|e| {
                    CodecError::Other(Box::new(e) as Box<dyn std::error::Error + 'static + Send + Sync>)
                })
            }
        }

        impl FieldElement for $elem {
            const ENCODED_SIZE: usize = $encoding_size;
            fn inv(&self) -> Self {
                // FieldParameters::inv() ultimately relies on mul(), and will always return a
                // value less than p.
                Self($fp.inv(self.0))
            }

            fn try_from_random(bytes: &[u8]) -> Result<Self, FieldError> {
                $elem::try_from_bytes(bytes, $fp.bit_mask)
            }

            fn zero() -> Self {
                Self(0)
            }

            fn one() -> Self {
                Self($fp.roots[0])
            }
        }

        impl FieldElementWithInteger for $elem {
            type Integer = $int;
            type IntegerTryFromError = <Self::Integer as TryFrom<usize>>::Error;
            type TryIntoU64Error = <Self::Integer as TryInto<u64>>::Error;

            fn pow(&self, exp: Self::Integer) -> Self {
                // FieldParameters::pow() relies on mul(), and will always return a value less
                // than p.
                Self($fp.pow(self.0, u128::try_from(exp).unwrap()))
            }

            fn modulus() -> Self::Integer {
                $fp.p as $int
            }
        }

        impl FftFriendlyFieldElement for $elem {
            fn generator() -> Self {
                Self($fp.g)
            }

            fn generator_order() -> Self::Integer {
                1 << (Self::Integer::try_from($fp.num_roots).unwrap())
            }

            fn root(l: usize) -> Option<Self> {
                if l < min($fp.roots.len(), $fp.num_roots+1) {
                    Some(Self($fp.roots[l]))
                } else {
                    None
                }
            }
        }
    };
}

make_field!(
    /// Same as Field32, but encoded in little endian for compatibility with Prio v2.
    FieldPrio2,
    u32,
    FP32,
    4,
);

make_field!(
    /// `GF(18446744069414584321)`, a 64-bit field.
    Field64,
    u64,
    FP64,
    8,
);

make_field!(
    /// `GF(340282366920938462946865773367900766209)`, a 128-bit field.
    Field128,
    u128,
    FP128,
    16,
);

/// Merge two vectors of fields by summing other_vector into accumulator.
///
/// # Errors
///
/// Fails if the two vectors do not have the same length.
pub(crate) fn merge_vector<F: FieldElement>(
    accumulator: &mut [F],
    other_vector: &[F],
) -> Result<(), FieldError> {
    if accumulator.len() != other_vector.len() {
        return Err(FieldError::InputSizeMismatch);
    }
    for (a, o) in accumulator.iter_mut().zip(other_vector.iter()) {
        *a += *o;
    }

    Ok(())
}

/// Outputs an additive secret sharing of the input.
#[cfg(all(feature = "crypto-dependencies", test))]
pub(crate) fn split_vector<F: FieldElement>(
    inp: &[F],
    num_shares: usize,
) -> Result<Vec<Vec<F>>, PrngError> {
    if num_shares == 0 {
        return Ok(vec![]);
    }

    let mut outp = Vec::with_capacity(num_shares);
    outp.push(inp.to_vec());

    for _ in 1..num_shares {
        let share: Vec<F> = random_vector(inp.len())?;
        for (x, y) in outp[0].iter_mut().zip(&share) {
            *x -= *y;
        }
        outp.push(share);
    }

    Ok(outp)
}

/// Generate a vector of uniformly distributed random field elements.
#[cfg(feature = "crypto-dependencies")]
#[cfg_attr(docsrs, doc(cfg(feature = "crypto-dependencies")))]
pub fn random_vector<F: FieldElement>(len: usize) -> Result<Vec<F>, PrngError> {
    Ok(Prng::new()?.take(len).collect())
}

/// `encode_fieldvec` serializes a type that is equivalent to a vector of field elements.
#[inline(always)]
pub(crate) fn encode_fieldvec<F: FieldElement, T: AsRef<[F]>>(val: T, bytes: &mut Vec<u8>) {
    for elem in val.as_ref() {
        elem.encode(bytes);
    }
}

/// `decode_fieldvec` deserializes some number of field elements from a cursor, and advances the
/// cursor's position.
pub(crate) fn decode_fieldvec<F: FieldElement>(
    count: usize,
    input: &mut Cursor<&[u8]>,
) -> Result<Vec<F>, CodecError> {
    let mut vec = Vec::with_capacity(count);
    let mut buffer = [0u8; 64];
    assert!(
        buffer.len() >= F::ENCODED_SIZE,
        "field is too big for buffer"
    );
    for _ in 0..count {
        input.read_exact(&mut buffer[..F::ENCODED_SIZE])?;
        vec.push(
            F::try_from(&buffer[..F::ENCODED_SIZE]).map_err(|e| CodecError::Other(Box::new(e)))?,
        );
    }
    Ok(vec)
}

#[cfg(test)]
pub(crate) mod test_utils {
    use super::{FieldElement, FieldElementWithInteger};
    use crate::{codec::CodecError, field::FieldError, prng::Prng};
    use assert_matches::assert_matches;
    use std::{
        collections::hash_map::DefaultHasher,
        convert::{TryFrom, TryInto},
        fmt::Debug,
        hash::{Hash, Hasher},
        io::Cursor,
        ops::{Add, BitAnd, Div, Shl, Shr, Sub},
    };

    /// A test-only copy of `FieldElementWithInteger`.
    ///
    /// This trait is only used in tests, and it is implemented on some fields that do not have
    /// `FieldElementWithInteger` implementations. This separate trait is used in order to avoid
    /// affecting trait resolution with conditional compilation. Additionally, this trait only
    /// requires the `Integer` associated type satisfy `Clone`, not `Copy`, so that it may be used
    /// with arbitrary precision integer implementations.
    pub(crate) trait TestFieldElementWithInteger:
        FieldElement + From<Self::Integer>
    {
        type IntegerTryFromError: std::error::Error;
        type TryIntoU64Error: std::error::Error;
        type Integer: Clone
            + Debug
            + Eq
            + Ord
            + BitAnd<Output = Self::Integer>
            + Div<Output = Self::Integer>
            + Shl<usize, Output = Self::Integer>
            + Shr<usize, Output = Self::Integer>
            + Add<Output = Self::Integer>
            + Sub<Output = Self::Integer>
            + From<Self>
            + TryFrom<usize, Error = Self::IntegerTryFromError>
            + TryInto<u64, Error = Self::TryIntoU64Error>;

        fn pow(&self, exp: Self::Integer) -> Self;

        fn modulus() -> Self::Integer;
    }

    impl<F> TestFieldElementWithInteger for F
    where
        F: FieldElementWithInteger,
    {
        type IntegerTryFromError = <F as FieldElementWithInteger>::IntegerTryFromError;
        type TryIntoU64Error = <F as FieldElementWithInteger>::TryIntoU64Error;
        type Integer = <F as FieldElementWithInteger>::Integer;

        fn pow(&self, exp: Self::Integer) -> Self {
            <F as FieldElementWithInteger>::pow(self, exp)
        }

        fn modulus() -> Self::Integer {
            <F as FieldElementWithInteger>::modulus()
        }
    }

    pub(crate) fn field_element_test_common<F: TestFieldElementWithInteger>() {
        let mut prng: Prng<F, _> = Prng::new().unwrap();
        let int_modulus = F::modulus();
        let int_one = F::Integer::try_from(1).unwrap();
        let zero = F::zero();
        let one = F::one();
        let two = F::from(F::Integer::try_from(2).unwrap());
        let four = F::from(F::Integer::try_from(4).unwrap());

        // add
        assert_eq!(F::from(int_modulus.clone() - int_one.clone()) + one, zero);
        assert_eq!(one + one, two);
        assert_eq!(two + F::from(int_modulus.clone()), two);

        // add w/ assignment
        let mut a = prng.get();
        let b = prng.get();
        let c = a + b;
        a += b;
        assert_eq!(a, c);

        // sub
        assert_eq!(zero - one, F::from(int_modulus.clone() - int_one.clone()));
        #[allow(clippy::eq_op)]
        {
            assert_eq!(one - one, zero);
        }
        assert_eq!(one + (-one), zero);
        assert_eq!(two - F::from(int_modulus.clone()), two);
        assert_eq!(one - F::from(int_modulus.clone() - int_one.clone()), two);

        // sub w/ assignment
        let mut a = prng.get();
        let b = prng.get();
        let c = a - b;
        a -= b;
        assert_eq!(a, c);

        // add + sub
        for _ in 0..100 {
            let f = prng.get();
            let g = prng.get();
            assert_eq!(f + g - f - g, zero);
            assert_eq!(f + g - g, f);
            assert_eq!(f + g - f, g);
        }

        // mul
        assert_eq!(two * two, four);
        assert_eq!(two * one, two);
        assert_eq!(two * zero, zero);
        assert_eq!(one * F::from(int_modulus.clone()), zero);

        // mul w/ assignment
        let mut a = prng.get();
        let b = prng.get();
        let c = a * b;
        a *= b;
        assert_eq!(a, c);

        // integer conversion
        assert_eq!(F::Integer::from(zero), F::Integer::try_from(0).unwrap());
        assert_eq!(F::Integer::from(one), F::Integer::try_from(1).unwrap());
        assert_eq!(F::Integer::from(two), F::Integer::try_from(2).unwrap());
        assert_eq!(F::Integer::from(four), F::Integer::try_from(4).unwrap());

        // serialization
        let test_inputs = vec![
            zero,
            one,
            prng.get(),
            F::from(int_modulus.clone() - int_one.clone()),
        ];
        for want in test_inputs.iter() {
            let mut bytes = vec![];
            want.encode(&mut bytes);

            assert_eq!(bytes.len(), F::ENCODED_SIZE);
            assert_eq!(want.encoded_len().unwrap(), F::ENCODED_SIZE);

            let got = F::get_decoded(&bytes).unwrap();
            assert_eq!(got, *want);
        }

        let serialized_vec = F::slice_into_byte_vec(&test_inputs);
        let deserialized = F::byte_slice_into_vec(&serialized_vec).unwrap();
        assert_eq!(deserialized, test_inputs);

        let test_input = prng.get();
        let json = serde_json::to_string(&test_input).unwrap();
        let deserialized = serde_json::from_str::<F>(&json).unwrap();
        assert_eq!(deserialized, test_input);

        let value = serde_json::from_str::<serde_json::Value>(&json).unwrap();
        let array = value.as_array().unwrap();
        for element in array {
            element.as_u64().unwrap();
        }

        let err = F::byte_slice_into_vec(&[0]).unwrap_err();
        assert_matches!(err, FieldError::ShortRead);

        let err = F::byte_slice_into_vec(&vec![0xffu8; F::ENCODED_SIZE]).unwrap_err();
        assert_matches!(err, FieldError::Codec(CodecError::Other(err)) => {
            assert_matches!(err.downcast_ref::<FieldError>(), Some(FieldError::ModulusOverflow));
        });

        let insufficient = vec![0u8; F::ENCODED_SIZE - 1];
        let err = F::try_from(insufficient.as_ref()).unwrap_err();
        assert_matches!(err, FieldError::ShortRead);
        let err = F::decode(&mut Cursor::new(&insufficient)).unwrap_err();
        assert_matches!(err, CodecError::Io(_));

        let err = F::decode(&mut Cursor::new(&vec![0xffu8; F::ENCODED_SIZE])).unwrap_err();
        assert_matches!(err, CodecError::Other(err) => {
            assert_matches!(err.downcast_ref::<FieldError>(), Some(FieldError::ModulusOverflow));
        });

        // equality and hash: Generate many elements, confirm they are not equal, and confirm
        // various products that should be equal have the same hash. Three is chosen as a generator
        // here because it happens to generate fairly large subgroups of (Z/pZ)* for all four
        // primes.
        let three = F::from(F::Integer::try_from(3).unwrap());
        let mut powers_of_three = Vec::with_capacity(500);
        let mut power = one;
        for _ in 0..500 {
            powers_of_three.push(power);
            power *= three;
        }
        // Check all these elements are mutually not equal.
        for i in 0..powers_of_three.len() {
            let first = &powers_of_three[i];
            for second in &powers_of_three[0..i] {
                assert_ne!(first, second);
            }
        }

        // Construct an element from a number that needs to be reduced, and test comparisons on it,
        // confirming that it is reduced correctly.
        let p = F::from(int_modulus.clone());
        assert_eq!(p, zero);
        let p_plus_one = F::from(int_modulus + int_one);
        assert_eq!(p_plus_one, one);
    }

    pub(super) fn hash_helper<H: Hash>(input: H) -> u64 {
        let mut hasher = DefaultHasher::new();
        input.hash(&mut hasher);
        hasher.finish()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::field::test_utils::{field_element_test_common, hash_helper};
    use crate::fp::MAX_ROOTS;
    use crate::prng::Prng;
    use assert_matches::assert_matches;

    #[test]
    fn test_accumulate() {
        let mut lhs = vec![FieldPrio2(1); 10];
        let rhs = vec![FieldPrio2(2); 10];

        merge_vector(&mut lhs, &rhs).unwrap();

        lhs.iter().for_each(|f| assert_eq!(*f, FieldPrio2(3)));
        rhs.iter().for_each(|f| assert_eq!(*f, FieldPrio2(2)));

        let wrong_len = vec![FieldPrio2::zero(); 9];
        let result = merge_vector(&mut lhs, &wrong_len);
        assert_matches!(result, Err(FieldError::InputSizeMismatch));
    }

    fn field_element_test<F: FftFriendlyFieldElement + Hash>() {
        field_element_test_common::<F>();

        let mut prng: Prng<F, _> = Prng::new().unwrap();
        let int_modulus = F::modulus();
        let int_one = F::Integer::try_from(1).unwrap();
        let zero = F::zero();
        let one = F::one();
        let two = F::from(F::Integer::try_from(2).unwrap());
        let four = F::from(F::Integer::try_from(4).unwrap());

        // div
        assert_eq!(four / two, two);
        #[allow(clippy::eq_op)]
        {
            assert_eq!(two / two, one);
        }
        assert_eq!(zero / two, zero);
        assert_eq!(two / zero, zero); // Undefined behavior
        assert_eq!(zero.inv(), zero); // Undefined behavior

        // div w/ assignment
        let mut a = prng.get();
        let b = prng.get();
        let c = a / b;
        a /= b;
        assert_eq!(a, c);
        assert_eq!(hash_helper(a), hash_helper(c));

        // mul + div
        for _ in 0..100 {
            let f = prng.get();
            if f == zero {
                continue;
            }
            assert_eq!(f * f.inv(), one);
            assert_eq!(f.inv() * f, one);
        }

        // pow
        assert_eq!(two.pow(F::Integer::try_from(0).unwrap()), one);
        assert_eq!(two.pow(int_one), two);
        assert_eq!(two.pow(F::Integer::try_from(2).unwrap()), four);
        assert_eq!(two.pow(int_modulus - int_one), one);
        assert_eq!(two.pow(int_modulus), two);

        // roots
        let mut int_order = F::generator_order();
        for l in 0..MAX_ROOTS + 1 {
            assert_eq!(
                F::generator().pow(int_order),
                F::root(l).unwrap(),
                "failure for F::root({l})"
            );
            int_order = int_order >> 1;
        }

        // formatting
        assert_eq!(format!("{zero}"), "0");
        assert_eq!(format!("{one}"), "1");
        assert_eq!(format!("{zero:?}"), "0");
        assert_eq!(format!("{one:?}"), "1");

        let three = F::from(F::Integer::try_from(3).unwrap());
        let mut powers_of_three = Vec::with_capacity(500);
        let mut power = one;
        for _ in 0..500 {
            powers_of_three.push(power);
            power *= three;
        }

        // Check that 3^i is the same whether it's calculated with pow() or repeated
        // multiplication, with both equality and hash equality.
        for (i, power) in powers_of_three.iter().enumerate() {
            let result = three.pow(F::Integer::try_from(i).unwrap());
            assert_eq!(result, *power);
            let hash1 = hash_helper(power);
            let hash2 = hash_helper(result);
            assert_eq!(hash1, hash2);
        }

        // Check that 3^n = (3^i)*(3^(n-i)), via both equality and hash equality.
        let expected_product = powers_of_three[powers_of_three.len() - 1];
        let expected_hash = hash_helper(expected_product);
        for i in 0..powers_of_three.len() {
            let a = powers_of_three[i];
            let b = powers_of_three[powers_of_three.len() - 1 - i];
            let product = a * b;
            assert_eq!(product, expected_product);
            assert_eq!(hash_helper(product), expected_hash);
        }
    }

    #[test]
    fn test_field_prio2() {
        field_element_test::<FieldPrio2>();
    }

    #[test]
    fn test_field64() {
        field_element_test::<Field64>();
    }

    #[test]
    fn test_field128() {
        field_element_test::<Field128>();
    }

    #[test]
    fn test_encode_into_bitvector() {
        let zero = Field128::zero();
        let one = Field128::one();
        let zero_enc = Field128::encode_into_bitvector_representation(&0, 4).unwrap();
        let one_enc = Field128::encode_into_bitvector_representation(&1, 4).unwrap();
        let fifteen_enc = Field128::encode_into_bitvector_representation(&15, 4).unwrap();
        assert_eq!(zero_enc, [zero; 4]);
        assert_eq!(one_enc, [one, zero, zero, zero]);
        assert_eq!(fifteen_enc, [one; 4]);
        Field128::encode_into_bitvector_representation(&16, 4).unwrap_err();
    }

    #[test]
    fn test_fill_bitvector() {
        let zero = Field128::zero();
        let one = Field128::one();
        let mut output: Vec<Field128> = vec![zero; 6];
        Field128::fill_with_bitvector_representation(&9, &mut output[1..5]).unwrap();
        assert_eq!(output, [zero, one, zero, zero, one, zero]);
        Field128::fill_with_bitvector_representation(&16, &mut output[1..5]).unwrap_err();
    }
}
