//! Unit bit sizes, used as a type parameter to concrete types to signify their
//! intended size.

use internal_prelude::v1::*;

/// Number of bits that the generic type should occupy.
pub trait NumberOfBits: Copy + Clone + Debug + Default {
    /// Minimal number of bytes that this bit width requires.
    type Bytes: NumberOfBytes;

    /// The numerical number of bits.
    fn number_of_bits() -> u8;
}

/// These bits are a multiple of 8
pub trait BitsFullBytes {} 

/// These bits are not a multiple of 8
pub trait BitsPartialBytes {}



/// Number of bytes that the generic type should occupy.
pub trait NumberOfBytes: Copy + Clone + Debug + Default {
    /// The byte array type that holds these bytes, for instance [u8; 2].
    type AsBytes: ByteArray;

    /// The numberical number of bytes.
    fn number_of_bytes() -> u8;
}

/// Helper that allows us to cast a fixed size array into a byte slice.
pub trait ByteArray: Default + Debug {
    fn len(&self) -> usize;
    fn as_bytes_slice(&self) -> &[u8];
    fn as_mut_bytes_slice(&mut self) -> &mut [u8];
    fn rotate_right(&mut self, bytes: usize);
    fn new(value: u8) -> Self;
}

macro_rules! bytes_type {
    ($T: ident, $N: expr) => {
        #[derive(Copy, Clone, Debug, Default, PartialEq)]
        pub struct $T;

        impl NumberOfBytes for $T {
            type AsBytes = [u8; $N];

            #[inline]
            fn number_of_bytes() -> u8 {
                $N
            }
        }

        impl ByteArray for [u8; $N] {
            #[inline]
            fn len(&self) -> usize {
                $N
            }

            #[inline]
            fn as_bytes_slice(&self) -> &[u8] {
                &self[..]
            }

            #[inline]
            fn as_mut_bytes_slice(&mut self) -> &mut [u8] {
                &mut self[..]
            }

            #[inline]
            fn rotate_right(&mut self, bytes: usize) {
                bytes_rotate_right(self, bytes)
            }

            fn new(value: u8) -> Self {
                [value; $N]
            }
        }
    }
}

macro_rules! bits_type {
    ($T: ident, $N: expr, $TB: ident, $TBK: ident) => {
        #[derive(Copy, Clone, Debug, Default, PartialEq)]
        pub struct $T;

        impl NumberOfBits for $T {
            type Bytes = $TB;

            #[inline]
            fn number_of_bits() -> u8 {
                $N
            }
        }

        impl $TBK for $T { }
    };
}

include!(concat!(env!("OUT_DIR"), "/generate_bytes_and_bits.rs"));

#[inline]
fn bytes_rotate_right(s: &mut [u8], bytes: usize) {
    {
        let mut i = s.len() - bytes - 1;
        loop {
            s[i+bytes] = s[i];            
            if i == 0 { break;}
            i -= 1;
        }
    }
    for i in 0..bytes {
        s[i] = 0;
    }
}

#[test]
fn test_byte_rotation() {
    let mut a = [0xCC, 0xBB, 0xAA, 0x00];
    bytes_rotate_right(&mut a, 1);
    assert_eq!([0x00, 0xCC, 0xBB, 0xAA], a);
}