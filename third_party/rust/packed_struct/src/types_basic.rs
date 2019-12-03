use super::packing::*;

impl PackedStruct<[u8; 1]> for bool {
    #[inline]
    fn pack(&self) -> [u8; 1] {
        if *self { [1] } else { [0] }
    }

    #[inline]
    fn unpack(src: &[u8; 1]) -> Result<bool, PackingError> {
        match src[0] {
            1 => Ok(true),
            0 => Ok(false),
            _ => Err(PackingError::InvalidValue)
        }
    }
}

impl PackedStructInfo for bool {
    #[inline]
    fn packed_bits() -> usize {
        1
    }
}

packing_slice!(bool; 1);




impl PackedStruct<[u8; 1]> for u8 {
    #[inline]
    fn pack(&self) -> [u8; 1] {
        [*self]
    }

    #[inline]
    fn unpack(src: &[u8; 1]) -> Result<u8, PackingError> {
        Ok(src[0])
    }
}

impl PackedStructInfo for u8 {
    #[inline]
    fn packed_bits() -> usize {
        8
    }
}
packing_slice!(u8; 1);




impl PackedStruct<[u8; 1]> for i8 {
    #[inline]
    fn pack(&self) -> [u8; 1] {
        [*self as u8]
    }

    #[inline]
    fn unpack(src: &[u8; 1]) -> Result<i8, PackingError> {
        Ok(src[0] as i8)
    }
}

impl PackedStructInfo for i8 {
    #[inline]
    fn packed_bits() -> usize {
        8
    }
}
packing_slice!(i8; 1);




impl PackedStruct<[u8; 0]> for () {
    #[inline]
    fn pack(&self) -> [u8; 0] {
        []
    }

    #[inline]
    fn unpack(_src: &[u8; 0]) -> Result<(), PackingError> {
        Ok(())
    }
}

impl PackedStructInfo for () {
    #[inline]
    fn packed_bits() -> usize {
        0
    }
}

