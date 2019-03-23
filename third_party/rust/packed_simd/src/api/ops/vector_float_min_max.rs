//! Vertical (lane-wise) vector `min` and `max` for floating-point vectors.

macro_rules! impl_ops_vector_float_min_max {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl $id {
            /// Minimum of two vectors.
            ///
            /// Returns a new vector containing the minimum value of each of
            /// the input vector lanes.
            #[inline]
            pub fn min(self, x: Self) -> Self {
                use crate::llvm::simd_fmin;
                unsafe { Simd(simd_fmin(self.0, x.0)) }
            }

            /// Maximum of two vectors.
            ///
            /// Returns a new vector containing the maximum value of each of
            /// the input vector lanes.
            #[inline]
            pub fn max(self, x: Self) -> Self {
                use crate::llvm::simd_fmax;
                unsafe { Simd(simd_fmax(self.0, x.0)) }
            }
        }
        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _ops_vector_min_max>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn min_max() {
                        let n = crate::$elem_ty::NAN;
                        let o = $id::splat(1. as $elem_ty);
                        let t = $id::splat(2. as $elem_ty);

                        let mut m = o; // [1., 2., 1., 2., ...]
                        let mut on = o;
                        for i in 0..$id::lanes() {
                            if i % 2 == 0 {
                                m = m.replace(i, 2. as $elem_ty);
                                on = on.replace(i, n);
                            }
                        }

                        assert_eq!(o.min(t), o);
                        assert_eq!(t.min(o), o);
                        assert_eq!(m.min(o), o);
                        assert_eq!(o.min(m), o);
                        assert_eq!(m.min(t), m);
                        assert_eq!(t.min(m), m);

                        assert_eq!(o.max(t), t);
                        assert_eq!(t.max(o), t);
                        assert_eq!(m.max(o), m);
                        assert_eq!(o.max(m), m);
                        assert_eq!(m.max(t), t);
                        assert_eq!(t.max(m), t);

                        assert_eq!(on.min(o), o);
                        assert_eq!(o.min(on), o);
                        assert_eq!(on.max(o), o);
                        assert_eq!(o.max(on), o);
                    }
                }
            }
        }
    };
}
