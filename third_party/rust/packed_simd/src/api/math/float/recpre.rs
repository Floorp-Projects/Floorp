//! Implements vertical (lane-wise) floating-point `recpre`.

macro_rules! impl_math_float_recpre {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl $id {
            /// Reciprocal estimate: `~= 1. / self`.
            ///
            /// FIXME: The precision of the estimate is currently unspecified.
            #[inline]
            pub fn recpre(self) -> Self {
                $id::splat(1.) / self
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _math_recpre>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn recpre() {
                        let tol = $id::splat(2.4e-4 as $elem_ty);
                        let o = $id::splat(1 as $elem_ty);
                        let error = (o - o.recpre()).abs();
                        assert!(error.le(tol).all());

                        let t = $id::splat(2 as $elem_ty);
                        let e = 0.5;
                        let error = (e - t.recpre()).abs();
                        assert!(error.le(tol).all());
                    }
                }
            }
        }
    };
}
