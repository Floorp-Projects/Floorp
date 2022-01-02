//! Implement debug formatting

macro_rules! impl_fmt_debug_tests {
    ([$elem_ty:ty; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        test_if! {
            $test_tt:
            paste::item! {
                pub mod [<$id _fmt_debug>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn debug() {
                        use arrayvec::{ArrayString,ArrayVec};
                        type TinyString = ArrayString<[u8; 512]>;

                        use crate::fmt::Write;
                        let v = $id::default();
                        let mut s = TinyString::new();
                        write!(&mut s, "{:?}", v).unwrap();

                        let mut beg = TinyString::new();
                        write!(&mut beg, "{}(", stringify!($id)).unwrap();
                        assert!(s.starts_with(beg.as_str()));
                        assert!(s.ends_with(")"));
                        let s: ArrayVec<[TinyString; 64]>
                            = s.replace(beg.as_str(), "")
                            .replace(")", "").split(",")
                            .map(|v| TinyString::from(v.trim()).unwrap())
                            .collect();
                        assert_eq!(s.len(), $id::lanes());
                        for (index, ss) in s.into_iter().enumerate() {
                            let mut e = TinyString::new();
                            write!(&mut e, "{:?}", v.extract(index)).unwrap();
                            assert_eq!(ss, e);
                        }
                    }
                }
            }
        }
    };
}

macro_rules! impl_fmt_debug {
    ([$elem_ty:ty; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl crate::fmt::Debug for $id {
            #[allow(clippy::missing_inline_in_public_items)]
            fn fmt(
                &self, f: &mut crate::fmt::Formatter<'_>,
            ) -> crate::fmt::Result {
                write!(f, "{}(", stringify!($id))?;
                for i in 0..$elem_count {
                    if i > 0 {
                        write!(f, ", ")?;
                    }
                    self.extract(i).fmt(f)?;
                }
                write!(f, ")")
            }
        }
        impl_fmt_debug_tests!([$elem_ty; $elem_count]: $id | $test_tt);
    };
}
