//! Implements `Hash` for vector types.

macro_rules! impl_hash {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl crate::hash::Hash for $id {
            #[inline]
            fn hash<H: crate::hash::Hasher>(&self, state: &mut H) {
                unsafe {
                    union A {
                        data: [$elem_ty; $id::lanes()],
                        vec: $id,
                    }
                    A { vec: *self }.data.hash(state)
                }
            }
        }

        test_if! {
            $test_tt:
            paste::item! {
                pub mod [<$id _hash>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn hash() {
                        use crate::hash::{Hash, Hasher};
                        #[allow(deprecated)]
                        use crate::hash::{SipHasher13};
                        type A = [$elem_ty; $id::lanes()];
                        let a: A = [42 as $elem_ty; $id::lanes()];
                        assert_eq!(
                            crate::mem::size_of::<A>(),
                            crate::mem::size_of::<$id>()
                        );
                        #[allow(deprecated)]
                        let mut a_hash = SipHasher13::new();
                        let mut v_hash = a_hash.clone();
                        a.hash(&mut a_hash);

                        let v = $id::splat(42 as $elem_ty);
                        v.hash(&mut v_hash);
                        assert_eq!(a_hash.finish(), v_hash.finish());
                    }
                }
            }
        }
    };
}
