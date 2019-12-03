use super::packing::*;

macro_rules! packable_u8_array {
    ($N: expr) => (

        impl PackedStruct<[u8; $N]> for [u8; $N] {
            #[inline]
            fn pack(&self) -> [u8; $N] {
                *self
            }

            #[inline]
            fn unpack(src: &[u8; $N]) -> Result<[u8; $N], PackingError> {
                Ok(*src)
            }
        }

        impl PackedStructInfo for [u8; $N] {
            #[inline]
            fn packed_bits() -> usize {
                $N * 8
            }
        }
    )
}

packable_u8_array!(1);
packable_u8_array!(2);
packable_u8_array!(3);
packable_u8_array!(4);
packable_u8_array!(5);
packable_u8_array!(6);
packable_u8_array!(7);
packable_u8_array!(8);
packable_u8_array!(9);
packable_u8_array!(10);
packable_u8_array!(11);
packable_u8_array!(12);
packable_u8_array!(13);
packable_u8_array!(14);
packable_u8_array!(15);
packable_u8_array!(16);
packable_u8_array!(17);
packable_u8_array!(18);
packable_u8_array!(19);
packable_u8_array!(20);
packable_u8_array!(21);
packable_u8_array!(22);
packable_u8_array!(23);
packable_u8_array!(24);
packable_u8_array!(25);
packable_u8_array!(26);
packable_u8_array!(27);
packable_u8_array!(28);
packable_u8_array!(29);
packable_u8_array!(30);
packable_u8_array!(31);
packable_u8_array!(32);
