//! Code-generation utilities

crate mod bit_manip;
crate mod llvm;
crate mod math;
crate mod reductions;
crate mod shuffle;
crate mod shuffle1_dyn;
crate mod swap_bytes;

macro_rules! impl_simd_array {
    ([$elem_ty:ident; $elem_count:expr]:
     $tuple_id:ident | $($elem_tys:ident),*) => {
        #[derive(Copy, Clone)]
        #[repr(simd)]
        pub struct $tuple_id($(crate $elem_tys),*);
        //^^^^^^^ leaked through SimdArray

        impl crate::sealed::SimdArray for [$elem_ty; $elem_count] {
            type Tuple = $tuple_id;
            type T = $elem_ty;
            const N: usize = $elem_count;
            type NT = [u32; $elem_count];
        }

        impl crate::sealed::Simd for $tuple_id {
            type Element = $elem_ty;
            const LANES: usize = $elem_count;
            type LanesType = [u32; $elem_count];
        }

    }
}

crate mod pointer_sized_int;

crate mod v16;
crate use self::v16::*;

crate mod v32;
crate use self::v32::*;

crate mod v64;
crate use self::v64::*;

crate mod v128;
crate use self::v128::*;

crate mod v256;
crate use self::v256::*;

crate mod v512;
crate use self::v512::*;

crate mod vSize;
crate use self::vSize::*;

crate mod vPtr;
crate use self::vPtr::*;
