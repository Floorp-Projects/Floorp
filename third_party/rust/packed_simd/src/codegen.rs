//! Code-generation utilities

pub(crate) mod bit_manip;
pub(crate) mod llvm;
pub(crate) mod math;
pub(crate) mod reductions;
pub(crate) mod shuffle;
pub(crate) mod shuffle1_dyn;
pub(crate) mod swap_bytes;

macro_rules! impl_simd_array {
    ([$elem_ty:ident; $elem_count:expr]:
     $tuple_id:ident | $($elem_tys:ident),*) => {
        #[derive(Copy, Clone)]
        #[repr(simd)]
        pub struct $tuple_id($(pub(crate) $elem_tys),*);
        //^^^^^^^ leaked through SimdArray

        impl crate::sealed::Seal for [$elem_ty; $elem_count] {}

        impl crate::sealed::SimdArray for [$elem_ty; $elem_count] {
            type Tuple = $tuple_id;
            type T = $elem_ty;
            const N: usize = $elem_count;
            type NT = [u32; $elem_count];
        }

        impl crate::sealed::Seal for $tuple_id {}
        impl crate::sealed::Simd for $tuple_id {
            type Element = $elem_ty;
            const LANES: usize = $elem_count;
            type LanesType = [u32; $elem_count];
        }

    }
}

pub(crate) mod pointer_sized_int;

pub(crate) mod v16;
pub(crate) use self::v16::*;

pub(crate) mod v32;
pub(crate) use self::v32::*;

pub(crate) mod v64;
pub(crate) use self::v64::*;

pub(crate) mod v128;
pub(crate) use self::v128::*;

pub(crate) mod v256;
pub(crate) use self::v256::*;

pub(crate) mod v512;
pub(crate) use self::v512::*;

pub(crate) mod vSize;
pub(crate) use self::vSize::*;

pub(crate) mod vPtr;
pub(crate) use self::vPtr::*;
