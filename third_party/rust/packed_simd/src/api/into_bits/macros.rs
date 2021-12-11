//! Macros implementing `FromBits`

macro_rules! impl_from_bits_ {
    ($id:ident[$test_tt:tt]: $from_ty:ident) => {
        impl crate::api::into_bits::FromBits<$from_ty> for $id {
            #[inline]
            fn from_bits(x: $from_ty) -> Self {
                unsafe { crate::mem::transmute(x) }
            }
        }

        test_if! {
            $test_tt:
            paste::item! {
                pub mod [<$id _from_bits_ $from_ty>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn test() {
                        use crate::{
                            ptr::{read_unaligned},
                            mem::{size_of, zeroed}
                        };
                        use crate::IntoBits;
                        assert_eq!(size_of::<$id>(),
                                   size_of::<$from_ty>());
                        // This is safe becasue we never create a reference to
                        // uninitialized memory:
                        let a: $from_ty = unsafe { zeroed() };

                        let b_0: $id = crate::FromBits::from_bits(a);
                        let b_1: $id = a.into_bits();

                        // Check that these are byte-wise equal, that is,
                        // that the bit patterns are identical:
                        for i in 0..size_of::<$id>() {
                            // This is safe because we only read initialized
                            // memory in bounds. Also, taking a reference to
                            // `b_i` is ok because the fields are initialized.
                            unsafe {
                                let b_0_v: u8 = read_unaligned(
                                    (&b_0 as *const $id as *const u8)
                                        .wrapping_add(i)
                                );
                                let b_1_v: u8 = read_unaligned(
                                    (&b_1 as *const $id as *const u8)
                                        .wrapping_add(i)
                                );
                                assert_eq!(b_0_v, b_1_v);
                            }
                        }
                    }
                }
            }
        }
    };
}

macro_rules! impl_from_bits {
    ($id:ident[$test_tt:tt]: $($from_ty:ident),*) => {
        $(
            impl_from_bits_!($id[$test_tt]: $from_ty);
        )*
    }
}

#[allow(unused)]
macro_rules! impl_into_bits {
    ($id:ident[$test_tt:tt]: $($from_ty:ident),*) => {
        $(
            impl_from_bits_!($from_ty[$test_tt]: $id);
        )*
    }
}
