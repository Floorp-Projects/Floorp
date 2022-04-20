//! Implements vertical (lane-wise) floating-point `sqrte`.

macro_rules! impl_math_float_sqrte {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl $id {
            /// Square-root estimate.
            ///
            /// FIXME: The precision of the estimate is currently unspecified.
            #[inline]
            pub fn sqrte(self) -> Self {
                use crate::codegen::math::float::sqrte::Sqrte;
                Sqrte::sqrte(self)
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _math_sqrte>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn sqrte() {
                        use crate::$elem_ty::consts::SQRT_2;
                        let tol = $id::splat(2.4e-4 as $elem_ty);

                        let z = $id::splat(0 as $elem_ty);
                        let error = (z - z.sqrte()).abs();
                        assert!(error.le(tol).all());

                        let o = $id::splat(1 as $elem_ty);
                        let error = (o - o.sqrte()).abs();
                        assert!(error.le(tol).all());

                        let t = $id::splat(2 as $elem_ty);
                        let e = $id::splat(SQRT_2 as $elem_ty);
                        let error = (e - t.sqrte()).abs();

                        assert!(error.le(tol).all());
                    }
                }
            }
        }
    };
}
