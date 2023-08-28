//! Vertical (lane-wise) vector `min` and `max` for integer vectors.

macro_rules! impl_ops_vector_int_min_max {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl $id {
            /// Minimum of two vectors.
            ///
            /// Returns a new vector containing the minimum value of each of
            /// the input vector lanes.
            #[inline]
            pub fn min(self, x: Self) -> Self {
                self.lt(x).select(self, x)
            }

            /// Maximum of two vectors.
            ///
            /// Returns a new vector containing the maximum value of each of
            /// the input vector lanes.
            #[inline]
            pub fn max(self, x: Self) -> Self {
                self.gt(x).select(self, x)
            }
        }
        test_if!{$test_tt:
        paste::item! {
            pub mod [<$id _ops_vector_min_max>] {
                use super::*;
                #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                fn min_max() {
                    let o = $id::splat(1 as $elem_ty);
                    let t = $id::splat(2 as $elem_ty);

                    let mut m = o;
                    for i in 0..$id::lanes() {
                        if i % 2 == 0 {
                            m = m.replace(i, 2 as $elem_ty);
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
                }
            }
        }
        }
    };
}
