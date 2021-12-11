//! Vertical (lane-wise) vector `Neg`.

macro_rules! impl_ops_vector_neg {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl crate::ops::Neg for $id {
            type Output = Self;
            #[inline]
            fn neg(self) -> Self {
                Self::splat(-1 as $elem_ty) * self
            }
        }
        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _ops_vector_neg>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn neg() {
                        let z = $id::splat(0 as $elem_ty);
                        let o = $id::splat(1 as $elem_ty);
                        let t = $id::splat(2 as $elem_ty);
                        let f = $id::splat(4 as $elem_ty);

                        let nz = $id::splat(-(0 as $elem_ty));
                        let no = $id::splat(-(1 as $elem_ty));
                        let nt = $id::splat(-(2 as $elem_ty));
                        let nf = $id::splat(-(4 as $elem_ty));

                        assert_eq!(-z, nz);
                        assert_eq!(-o, no);
                        assert_eq!(-t, nt);
                        assert_eq!(-f, nf);

                        assert_eq!(z, -nz);
                        assert_eq!(o, -no);
                        assert_eq!(t, -nt);
                        assert_eq!(f, -nf);
                    }
                }
            }
        }
    };
}
