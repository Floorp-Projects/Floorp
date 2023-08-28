//! Macros implementing `FromCast`

macro_rules! impl_from_cast_ {
    ($id:ident[$test_tt:tt]: $from_ty:ident) => {
        impl crate::api::cast::FromCast<$from_ty> for $id {
            #[inline]
            fn from_cast(x: $from_ty) -> Self {
                use crate::llvm::simd_cast;
                debug_assert_eq!($from_ty::lanes(), $id::lanes());
                Simd(unsafe { simd_cast(x.0) })
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _from_cast_ $from_ty>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn test() {
                        assert_eq!($id::lanes(), $from_ty::lanes());
                    }
                }
            }
        }
    };
}

macro_rules! impl_from_cast {
    ($id:ident[$test_tt:tt]: $($from_ty:ident),*) => {
        $(
            impl_from_cast_!($id[$test_tt]: $from_ty);
        )*
    }
}

macro_rules! impl_from_cast_mask_ {
    ($id:ident[$test_tt:tt]: $from_ty:ident) => {
        impl crate::api::cast::FromCast<$from_ty> for $id {
            #[inline]
            fn from_cast(x: $from_ty) -> Self {
                debug_assert_eq!($from_ty::lanes(), $id::lanes());
                x.ne($from_ty::default())
                    .select($id::splat(true), $id::splat(false))
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _from_cast_ $from_ty>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn test() {
                        assert_eq!($id::lanes(), $from_ty::lanes());

                        let x = $from_ty::default();
                        let m: $id = x.cast();
                        assert!(m.none());
                    }
                }
            }
        }
    };
}

macro_rules! impl_from_cast_mask {
    ($id:ident[$test_tt:tt]: $($from_ty:ident),*) => {
        $(
            impl_from_cast_mask_!($id[$test_tt]: $from_ty);
        )*
    }
}

#[allow(unused)]
macro_rules! impl_into_cast {
    ($id:ident[$test_tt:tt]: $($from_ty:ident),*) => {
        $(
            impl_from_cast_!($from_ty[$test_tt]: $id);
        )*
    }
}
