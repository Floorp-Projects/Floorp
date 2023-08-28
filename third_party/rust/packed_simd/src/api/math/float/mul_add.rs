//! Implements vertical (lane-wise) floating-point `mul_add`.

macro_rules! impl_math_float_mul_add {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl $id {
            /// Fused multiply add: `self * y + z`
            #[inline]
            pub fn mul_add(self, y: Self, z: Self) -> Self {
                use crate::codegen::math::float::mul_add::MulAdd;
                MulAdd::mul_add(self, y, z)
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _math_mul_add>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn mul_add() {
                        let z = $id::splat(0 as $elem_ty);
                        let o = $id::splat(1 as $elem_ty);
                        let t = $id::splat(2 as $elem_ty);
                        let t3 = $id::splat(3 as $elem_ty);
                        let f = $id::splat(4 as $elem_ty);

                        assert_eq!(z, z.mul_add(z, z));
                        assert_eq!(o, o.mul_add(o, z));
                        assert_eq!(o, o.mul_add(z, o));
                        assert_eq!(o, z.mul_add(o, o));

                        assert_eq!(t, o.mul_add(o, o));
                        assert_eq!(t, o.mul_add(t, z));
                        assert_eq!(t, t.mul_add(o, z));

                        assert_eq!(f, t.mul_add(t, z));
                        assert_eq!(f, t.mul_add(o, t));
                        assert_eq!(t3, t.mul_add(o, o));
                    }
                }
            }
        }
    };
}
