//! Implements vertical (lane-wise) floating-point `rsqrte`.

macro_rules! impl_math_float_rsqrte {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl $id {
            /// Reciprocal square-root estimate: `~= 1. / self.sqrt()`.
            ///
            /// FIXME: The precision of the estimate is currently unspecified.
            #[inline]
            pub fn rsqrte(self) -> Self {
                unsafe {
                    use crate::llvm::simd_fsqrt;
                    $id::splat(1.) / Simd(simd_fsqrt(self.0))
                }
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _math_rsqrte>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn rsqrte() {
                        use crate::$elem_ty::consts::SQRT_2;
                        let tol = $id::splat(2.4e-4 as $elem_ty);
                        let o = $id::splat(1 as $elem_ty);
                        let error = (o - o.rsqrte()).abs();
                        assert!(error.le(tol).all());

                        let t = $id::splat(2 as $elem_ty);
                        let e = 1. / SQRT_2;
                        let error = (e - t.rsqrte()).abs();
                        assert!(error.le(tol).all());
                    }
                }
            }
        }
    };
}
