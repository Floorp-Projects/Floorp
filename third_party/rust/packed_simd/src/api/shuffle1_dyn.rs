//! Shuffle vector elements according to a dynamic vector of indices.

macro_rules! impl_shuffle1_dyn {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl $id {
            /// Shuffle vector elements according to `indices`.
            #[inline]
            pub fn shuffle1_dyn<I>(self, indices: I) -> Self
            where
                Self: codegen::shuffle1_dyn::Shuffle1Dyn<Indices = I>,
            {
                codegen::shuffle1_dyn::Shuffle1Dyn::shuffle1_dyn(self, indices)
            }
        }
    };
}

macro_rules! test_shuffle1_dyn {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        test_if! {
            $test_tt:
            paste::item! {
                pub mod [<$id _shuffle1_dyn>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn shuffle1_dyn() {
                        let increasing = {
                            let mut v = $id::splat(0 as $elem_ty);
                            for i in 0..$id::lanes() {
                                v = v.replace(i, i as $elem_ty);
                            }
                            v
                        };
                        let decreasing = {
                            let mut v = $id::splat(0 as $elem_ty);
                            for i in 0..$id::lanes() {
                                v = v.replace(
                                    i,
                                    ($id::lanes() - 1 - i) as $elem_ty
                                );
                            }
                            v
                        };

                        type Indices = <
                            $id as codegen::shuffle1_dyn::Shuffle1Dyn
                            >::Indices;
                        let increasing_ids: Indices = increasing.cast();
                        let decreasing_ids: Indices = decreasing.cast();

                        assert_eq!(
                            increasing.shuffle1_dyn(increasing_ids),
                            increasing,
                            "(i,i)=>i"
                        );
                        assert_eq!(
                            decreasing.shuffle1_dyn(increasing_ids),
                            decreasing,
                            "(d,i)=>d"
                        );
                        assert_eq!(
                            increasing.shuffle1_dyn(decreasing_ids),
                            decreasing,
                            "(i,d)=>d"
                        );
                        assert_eq!(
                            decreasing.shuffle1_dyn(decreasing_ids),
                            increasing,
                            "(d,d)=>i"
                        );

                        for i in 0..$id::lanes() {
                            let v_ids: Indices
                                = $id::splat(i as $elem_ty).cast();
                            assert_eq!(increasing.shuffle1_dyn(v_ids),
                                       $id::splat(increasing.extract(i))
                            );
                            assert_eq!(decreasing.shuffle1_dyn(v_ids),
                                       $id::splat(decreasing.extract(i))
                            );
                            assert_eq!(
                                $id::splat(i as $elem_ty)
                                    .shuffle1_dyn(increasing_ids),
                                $id::splat(i as $elem_ty)
                            );
                            assert_eq!(
                                $id::splat(i as $elem_ty)
                                    .shuffle1_dyn(decreasing_ids),
                                $id::splat(i as $elem_ty)
                            );
                        }
                    }
                }
            }
        }
    };
}

macro_rules! test_shuffle1_dyn_mask {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        test_if! {
            $test_tt:
            paste::item! {
                pub mod [<$id _shuffle1_dyn>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn shuffle1_dyn() {
                        // alternating = [true, false, true, false, ...]
                        let mut alternating = $id::splat(false);
                        for i in 0..$id::lanes() {
                            if i % 2 == 0 {
                                alternating = alternating.replace(i, true);
                            }
                        }

                        type Indices = <
                            $id as codegen::shuffle1_dyn::Shuffle1Dyn
                            >::Indices;
                        // even = [0, 0, 2, 2, 4, 4, ..]
                        let even = {
                            let mut v = Indices::splat(0);
                            for i in 0..$id::lanes() {
                                if i % 2 == 0 {
                                    v = v.replace(i, (i as u8).into());
                                } else {
                                    v = v.replace(i, (i as u8 - 1).into());
                                }
                            }
                            v
                        };
                        // odd = [1, 1, 3, 3, 5, 5, ...]
                        let odd = {
                            let mut v = Indices::splat(0);
                            for i in 0..$id::lanes() {
                                if i % 2 != 0 {
                                    v = v.replace(i, (i as u8).into());
                                } else {
                                    v = v.replace(i, (i as u8 + 1).into());
                                }
                            }
                            v
                        };

                        assert_eq!(
                            alternating.shuffle1_dyn(even),
                            $id::splat(true)
                        );
                        if $id::lanes() > 1 {
                            assert_eq!(
                                alternating.shuffle1_dyn(odd),
                                $id::splat(false)
                            );
                        }
                    }
                }
            }
        }
    };
}
