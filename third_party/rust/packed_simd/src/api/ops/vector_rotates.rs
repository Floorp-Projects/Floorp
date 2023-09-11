//! Vertical (lane-wise) vector rotates operations.
#![allow(unused)]

macro_rules! impl_ops_vector_rotates {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl $id {
            /// Shifts the bits of each lane to the left by the specified
            /// amount in the corresponding lane of `n`, wrapping the
            /// truncated bits to the end of the resulting integer.
            ///
            /// Note: this is neither the same operation as `<<` nor equivalent
            /// to `slice::rotate_left`.
            #[inline]
            pub fn rotate_left(self, n: $id) -> $id {
                const LANE_WIDTH: $elem_ty =
                    crate::mem::size_of::<$elem_ty>() as $elem_ty * 8;
                // Protect against undefined behavior for over-long bit shifts
                let n = n % LANE_WIDTH;
                (self << n) | (self >> ((LANE_WIDTH - n) % LANE_WIDTH))
            }

            /// Shifts the bits of each lane to the right by the specified
            /// amount in the corresponding lane of `n`, wrapping the
            /// truncated bits to the beginning of the resulting integer.
            ///
            /// Note: this is neither the same operation as `>>` nor equivalent
            /// to `slice::rotate_right`.
            #[inline]
            pub fn rotate_right(self, n: $id) -> $id {
                const LANE_WIDTH: $elem_ty =
                    crate::mem::size_of::<$elem_ty>() as $elem_ty * 8;
                // Protect against undefined behavior for over-long bit shifts
                let n = n % LANE_WIDTH;
                (self >> n) | (self << ((LANE_WIDTH - n) % LANE_WIDTH))
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                // FIXME:
                // https://github.com/rust-lang-nursery/packed_simd/issues/75
                #[cfg(not(any(
                    target_arch = "s390x",
                    target_arch = "sparc64",
                )))]
                pub mod [<$id _ops_vector_rotate>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    #[cfg(not(target_arch = "aarch64"))]
                    //~^ FIXME: https://github.com/rust-lang/packed_simd/issues/317
                    fn rotate_ops() {
                        let z = $id::splat(0 as $elem_ty);
                        let o = $id::splat(1 as $elem_ty);
                        let t = $id::splat(2 as $elem_ty);
                        let f = $id::splat(4 as $elem_ty);

                        let max = $id::splat(
                            (mem::size_of::<$elem_ty>() * 8 - 1) as $elem_ty);

                        // rotate_right
                        assert_eq!(z.rotate_right(z), z);
                        assert_eq!(z.rotate_right(o), z);
                        assert_eq!(z.rotate_right(t), z);

                        assert_eq!(o.rotate_right(z), o);
                        assert_eq!(t.rotate_right(z), t);
                        assert_eq!(f.rotate_right(z), f);
                        assert_eq!(f.rotate_right(max), f << 1);

                        assert_eq!(o.rotate_right(o), o << max);
                        assert_eq!(t.rotate_right(o), o);
                        assert_eq!(t.rotate_right(t), o << max);
                        assert_eq!(f.rotate_right(o), t);
                        assert_eq!(f.rotate_right(t), o);

                        // rotate_left
                        assert_eq!(z.rotate_left(z), z);
                        assert_eq!(o.rotate_left(z), o);
                        assert_eq!(t.rotate_left(z), t);
                        assert_eq!(f.rotate_left(z), f);
                        assert_eq!(f.rotate_left(max), t);

                        assert_eq!(o.rotate_left(o), t);
                        assert_eq!(o.rotate_left(t), f);
                        assert_eq!(t.rotate_left(o), f);
                    }
                }
            }
        }
    };
}
