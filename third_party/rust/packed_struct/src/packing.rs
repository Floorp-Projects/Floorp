use internal_prelude::v1::*;

/// A structure that can be packed and unpacked from a byte array.
/// 
/// In case the structure occupies less bits than there are in the byte array,
/// the packed that should be aligned to the end of the array, with leading bits
/// being ignored.
/// 
/// 10 bits packs into: [0b00000011, 0b11111111]
pub trait PackedStruct<B> where Self: Sized {
    /// Packs the structure into a byte array.
    fn pack(&self) -> B;
    /// Unpacks the structure from a byte array.
    fn unpack(src: &B) -> Result<Self, PackingError>;
}

/// Infos about a particular type that can be packaged.
pub trait PackedStructInfo {
    /// Number of bits that this structure occupies when being packed.
    fn packed_bits() -> usize;
}

/// A structure that can be packed and unpacked from a slice of bytes.
pub trait PackedStructSlice where Self: Sized {
    /// Pack the structure into an output buffer.
    fn pack_to_slice(&self, output: &mut [u8]) -> Result<(), PackingError>;
    /// Unpack the structure from a buffer.
    fn unpack_from_slice(src: &[u8]) -> Result<Self, PackingError>;
    /// Number of bytes that this structure demands for packing or unpacking.
    fn packed_bytes() -> usize;

    #[cfg(any(feature="alloc", feature="std"))]
    /// Pack the structure into a new byte vector.
    fn pack_to_vec(&self) -> Result<Vec<u8>, PackingError> {
        let mut buf = vec![0; Self::packed_bytes()];
        self.pack_to_slice(&mut buf)?;
        Ok(buf)
    }
}



#[derive(Debug, Copy, Clone, PartialEq, Serialize)]
/// Packing errors that might occur during packing or unpacking
pub enum PackingError {
    InvalidValue,
    BitsError,
    BufferTooSmall,
    NotImplemented,
    BufferSizeMismatch { expected: usize, actual: usize }
}

impl Display for PackingError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:?}", self)
    }    
}

#[cfg(feature="std")]
impl ::std::error::Error for PackingError {
    fn description(&self) -> &str {
        match *self {
            PackingError::InvalidValue => "Invalid value",
            PackingError::BitsError => "Bits error",
            PackingError::BufferTooSmall => "Buffer too small",            
            PackingError::BufferSizeMismatch { .. } => "Buffer size mismatched",
            PackingError::NotImplemented => "Not implemented"
        }
    }
}


macro_rules! packing_slice {
    ($T: path; $num_bytes: expr) => (

        impl PackedStructSlice for $T {
            #[inline]
            fn pack_to_slice(&self, output: &mut [u8]) -> Result<(), PackingError> {
                if output.len() != $num_bytes {
                    return Err(PackingError::BufferTooSmall);
                }
                let packed = self.pack();                
                &mut output[..].copy_from_slice(&packed[..]);
                Ok(())
            }

            #[inline]
            fn unpack_from_slice(src: &[u8]) -> Result<Self, PackingError> {
                if src.len() != $num_bytes {
                    return Err(PackingError::BufferTooSmall);
                }
                let mut s = [0; $num_bytes];
                &mut s[..].copy_from_slice(src);
                Self::unpack(&s)
            }

            #[inline]
            fn packed_bytes() -> usize {
                $num_bytes
            }
        }

    )
}

