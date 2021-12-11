//! Implements mask's `select`.

/// Implements mask select method
macro_rules! impl_select {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl $id {
            /// Selects elements of `a` and `b` using mask.
            ///
            /// The lanes of the result for which the mask is `true` contain
            /// the values of `a`. The remaining lanes contain the values of
            /// `b`.
            #[inline]
            pub fn select<T>(self, a: Simd<T>, b: Simd<T>) -> Simd<T>
            where
                T: sealed::SimdArray<
                    NT = <[$elem_ty; $elem_count] as sealed::SimdArray>::NT,
                >,
            {
                use crate::llvm::simd_select;
                Simd(unsafe { simd_select(self.0, a.0, b.0) })
            }
        }

        test_select!(bool, $id, $id, (false, true) | $test_tt);
    };
}

macro_rules! test_select {
    (
        $elem_ty:ident,
        $mask_ty:ident,
        $vec_ty:ident,($small:expr, $large:expr) |
        $test_tt:tt
    ) => {
        test_if! {
            $test_tt:
            paste::item! {
                pub mod [<$vec_ty _select>] {
                    use super::*;

                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn select() {
                        let o = $small as $elem_ty;
                        let t = $large as $elem_ty;

                        let a = $vec_ty::splat(o);
                        let b = $vec_ty::splat(t);
                        let m = a.lt(b);
                        assert_eq!(m.select(a, b), a);

                        let m = b.lt(a);
                        assert_eq!(m.select(b, a), a);

                        let mut c = a;
                        let mut d = b;
                        let mut m_e = $mask_ty::splat(false);
                        for i in 0..$vec_ty::lanes() {
                            if i % 2 == 0 {
                                let c_tmp = c.extract(i);
                                c = c.replace(i, d.extract(i));
                                d = d.replace(i, c_tmp);
                            } else {
                                m_e = m_e.replace(i, true);
                            }
                        }

                        let m = c.lt(d);
                        assert_eq!(m_e, m);
                        assert_eq!(m.select(c, d), a);
                    }
                }
            }
        }
    };
}
