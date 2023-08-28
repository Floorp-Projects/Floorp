//! Implements vertical (lane-wise) floating-point `mul_adde`.

macro_rules! impl_math_float_mul_adde {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl $id {
            /// Fused multiply add estimate: ~= `self * y + z`
            ///
            /// While fused multiply-add (`fma`) has infinite precision,
            /// `mul_adde` has _at worst_ the same precision of a multiply followed by an add.
            /// This might be more efficient on architectures that do not have an `fma` instruction.
            #[inline]
            pub fn mul_adde(self, y: Self, z: Self) -> Self {
                use crate::codegen::math::float::mul_adde::MulAddE;
                MulAddE::mul_adde(self, y, z)
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _math_mul_adde>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn mul_adde() {
                        let z = $id::splat(0 as $elem_ty);
                        let o = $id::splat(1 as $elem_ty);
                        let t = $id::splat(2 as $elem_ty);
                        let t3 = $id::splat(3 as $elem_ty);
                        let f = $id::splat(4 as $elem_ty);

                        assert_eq!(z, z.mul_adde(z, z));
                        assert_eq!(o, o.mul_adde(o, z));
                        assert_eq!(o, o.mul_adde(z, o));
                        assert_eq!(o, z.mul_adde(o, o));

                        assert_eq!(t, o.mul_adde(o, o));
                        assert_eq!(t, o.mul_adde(t, z));
                        assert_eq!(t, t.mul_adde(o, z));

                        assert_eq!(f, t.mul_adde(t, z));
                        assert_eq!(f, t.mul_adde(o, t));
                        assert_eq!(t3, t.mul_adde(o, o));
                    }
                }
            }
        }
    };
}
