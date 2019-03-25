//! Pointer vector types

macro_rules! impl_simd_ptr {
    ([$ptr_ty:ty; $elem_count:expr]: $tuple_id:ident | $ty:ident
     | $($tys:ty),*) => {
        #[derive(Copy, Clone)]
        #[repr(simd)]
        pub struct $tuple_id<$ty>($(crate $tys),*);
        //^^^^^^^ leaked through SimdArray

        impl<$ty> crate::sealed::SimdArray for [$ptr_ty; $elem_count] {
            type Tuple = $tuple_id<$ptr_ty>;
            type T = $ptr_ty;
            const N: usize = $elem_count;
            type NT = [u32; $elem_count];
        }

        impl<$ty> crate::sealed::Simd for $tuple_id<$ptr_ty> {
            type Element = $ptr_ty;
            const LANES: usize = $elem_count;
            type LanesType = [u32; $elem_count];
        }

    }
}

impl_simd_ptr!([*const T; 2]: cptrx2 | T | T, T);
impl_simd_ptr!([*const T; 4]: cptrx4 | T | T, T, T, T);
impl_simd_ptr!([*const T; 8]: cptrx8 | T | T, T, T, T, T, T, T, T);

impl_simd_ptr!([*mut T; 2]: mptrx2 | T | T, T);
impl_simd_ptr!([*mut T; 4]: mptrx4 | T | T, T, T, T);
impl_simd_ptr!([*mut T; 8]: mptrx8 | T | T, T, T, T, T, T, T, T);
