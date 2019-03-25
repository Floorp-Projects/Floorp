//! Minimal API of pointer vectors.

macro_rules! impl_minimal_p {
    ([$elem_ty:ty; $elem_count:expr]: $id:ident, $mask_ty:ident,
     $usize_ty:ident, $isize_ty:ident | $ref:ident | $test_tt:tt
     | $($elem_name:ident),+ | ($true:expr, $false:expr) |
     $(#[$doc:meta])*) => {

        $(#[$doc])*
        pub type $id<T> = Simd<[$elem_ty; $elem_count]>;

        impl<T> sealed::Simd for $id<T> {
            type Element = $elem_ty;
            const LANES: usize = $elem_count;
            type LanesType = [u32; $elem_count];
        }

        impl<T> $id<T> {
            /// Creates a new instance with each vector elements initialized
            /// with the provided values.
            #[inline]
            #[allow(clippy::too_many_arguments)]
            pub const fn new($($elem_name: $elem_ty),*) -> Self {
                Simd(codegen::$id($($elem_name),*))
            }

            /// Returns the number of vector lanes.
            #[inline]
            pub const fn lanes() -> usize {
                $elem_count
            }

            /// Constructs a new instance with each element initialized to
            /// `value`.
            #[inline]
            pub const fn splat(value: $elem_ty) -> Self {
                Simd(codegen::$id($({
                    #[allow(non_camel_case_types, dead_code)]
                    struct $elem_name;
                    value
                }),*))
            }

            /// Constructs a new instance with each element initialized to
            /// `null`.
            #[inline]
            pub const fn null() -> Self {
                Self::splat(crate::ptr::null_mut() as $elem_ty)
            }

            /// Returns a mask that selects those lanes that contain `null`
            /// pointers.
            #[inline]
            pub fn is_null(self) -> $mask_ty {
                self.eq(Self::null())
            }

            /// Extracts the value at `index`.
            ///
            /// # Panics
            ///
            /// If `index >= Self::lanes()`.
            #[inline]
            pub fn extract(self, index: usize) -> $elem_ty {
                assert!(index < $elem_count);
                unsafe { self.extract_unchecked(index) }
            }

            /// Extracts the value at `index`.
            ///
            /// # Precondition
            ///
            /// If `index >= Self::lanes()` the behavior is undefined.
            #[inline]
            pub unsafe fn extract_unchecked(self, index: usize) -> $elem_ty {
                use crate::llvm::simd_extract;
                simd_extract(self.0, index as u32)
            }

            /// Returns a new vector where the value at `index` is replaced by
            /// `new_value`.
            ///
            /// # Panics
            ///
            /// If `index >= Self::lanes()`.
            #[inline]
            #[must_use = "replace does not modify the original value - \
                          it returns a new vector with the value at `index` \
                          replaced by `new_value`d"
            ]
            #[allow(clippy::not_unsafe_ptr_arg_deref)]
            pub fn replace(self, index: usize, new_value: $elem_ty) -> Self {
                assert!(index < $elem_count);
                unsafe { self.replace_unchecked(index, new_value) }
            }

            /// Returns a new vector where the value at `index` is replaced by `new_value`.
            ///
            /// # Precondition
            ///
            /// If `index >= Self::lanes()` the behavior is undefined.
            #[inline]
            #[must_use = "replace_unchecked does not modify the original value - \
                          it returns a new vector with the value at `index` \
                          replaced by `new_value`d"
            ]
            pub unsafe fn replace_unchecked(
                self,
                index: usize,
                new_value: $elem_ty,
            ) -> Self {
                use crate::llvm::simd_insert;
                Simd(simd_insert(self.0, index as u32, new_value))
            }
        }


        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _minimal>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn minimal() {
                        // lanes:
                        assert_eq!($elem_count, $id::<i32>::lanes());

                        // splat and extract / extract_unchecked:
                        let VAL7: <$id<i32> as sealed::Simd>::Element
                            = $ref!(7);
                        let VAL42: <$id<i32> as sealed::Simd>::Element
                            = $ref!(42);
                        let VEC: $id<i32> = $id::splat(VAL7);
                        for i in 0..$id::<i32>::lanes() {
                            assert_eq!(VAL7, VEC.extract(i));
                            assert_eq!(
                                VAL7, unsafe { VEC.extract_unchecked(i) }
                            );
                        }

                        // replace / replace_unchecked
                        let new_vec = VEC.replace(0, VAL42);
                        for i in 0..$id::<i32>::lanes() {
                            if i == 0 {
                                assert_eq!(VAL42, new_vec.extract(i));
                            } else {
                                assert_eq!(VAL7, new_vec.extract(i));
                            }
                        }
                        let new_vec = unsafe {
                            VEC.replace_unchecked(0, VAL42)
                        };
                        for i in 0..$id::<i32>::lanes() {
                            if i == 0 {
                                assert_eq!(VAL42, new_vec.extract(i));
                            } else {
                                assert_eq!(VAL7, new_vec.extract(i));
                            }
                        }

                        let mut n = $id::<i32>::null();
                        assert_eq!(
                            n,
                            $id::<i32>::splat(unsafe { crate::mem::zeroed() })
                        );
                        assert!(n.is_null().all());
                        n = n.replace(
                            0, unsafe { crate::mem::transmute(1_isize) }
                        );
                        assert!(!n.is_null().all());
                        if $id::<i32>::lanes() > 1 {
                            assert!(n.is_null().any());
                        } else {
                            assert!(!n.is_null().any());
                        }
                    }

                    // FIXME: wasm-bindgen-test does not support #[should_panic]
                    // #[cfg_attr(not(target_arch = "wasm32"), test)]
                    // #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    #[cfg(not(target_arch = "wasm32"))]
                    #[test]
                    #[should_panic]
                    fn extract_panic_oob() {
                        let VAL: <$id<i32> as sealed::Simd>::Element
                            = $ref!(7);
                        let VEC: $id<i32> = $id::splat(VAL);
                        let _ = VEC.extract($id::<i32>::lanes());
                    }

                    // FIXME: wasm-bindgen-test does not support #[should_panic]
                    // #[cfg_attr(not(target_arch = "wasm32"), test)]
                    // #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    #[cfg(not(target_arch = "wasm32"))]
                    #[test]
                    #[should_panic]
                    fn replace_panic_oob() {
                        let VAL: <$id<i32> as sealed::Simd>::Element
                            = $ref!(7);
                        let VAL42: <$id<i32> as sealed::Simd>::Element
                            = $ref!(42);
                        let VEC: $id<i32> = $id::splat(VAL);
                        let _ = VEC.replace($id::<i32>::lanes(), VAL42);
                    }
                }
            }
        }

        impl<T> crate::fmt::Debug for $id<T> {
            #[allow(clippy::missing_inline_in_public_items)]
            fn fmt(&self, f: &mut crate::fmt::Formatter<'_>)
                   -> crate::fmt::Result {
                write!(
                    f,
                    "{}<{}>(",
                    stringify!($id),
                    unsafe { crate::intrinsics::type_name::<T>() }
                )?;
                for i in 0..$elem_count {
                    if i > 0 {
                        write!(f, ", ")?;
                    }
                    self.extract(i).fmt(f)?;
                }
                write!(f, ")")
            }
        }

         test_if!{
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
                        let v = $id::<i32>::default();
                        let mut s = TinyString::new();
                        write!(&mut s, "{:?}", v).unwrap();

                        let mut beg = TinyString::new();
                        write!(&mut beg, "{}<i32>(", stringify!($id)).unwrap();
                        assert!(
                            s.starts_with(beg.as_str()),
                            "s = {} (should start with = {})", s, beg
                        );
                        assert!(s.ends_with(")"));
                        let s: ArrayVec<[TinyString; 64]>
                            = s.replace(beg.as_str(), "")
                            .replace(")", "").split(",")
                            .map(|v| TinyString::from(v.trim()).unwrap())
                            .collect();
                        assert_eq!(s.len(), $id::<i32>::lanes());
                        for (index, ss) in s.into_iter().enumerate() {
                            let mut e = TinyString::new();
                            write!(&mut e, "{:?}", v.extract(index)).unwrap();
                            assert_eq!(ss, e);
                        }
                    }
                }
            }
         }

        impl<T> Default for $id<T> {
            #[inline]
            fn default() -> Self {
                // FIXME: ptrs do not implement default
                Self::null()
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _default>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn default() {
                        let a = $id::<i32>::default();
                        for i in 0..$id::<i32>::lanes() {
                            assert_eq!(
                                a.extract(i), unsafe { crate::mem::zeroed() }
                            );
                        }
                    }
                }
            }
        }

        impl<T> $id<T> {
            /// Lane-wise equality comparison.
            #[inline]
            pub fn eq(self, other: Self) -> $mask_ty {
                unsafe {
                    use crate::llvm::simd_eq;
                    let a: $usize_ty = crate::mem::transmute(self);
                    let b: $usize_ty = crate::mem::transmute(other);
                    Simd(simd_eq(a.0, b.0))
                }
            }

            /// Lane-wise inequality comparison.
            #[inline]
            pub fn ne(self, other: Self) -> $mask_ty {
                unsafe {
                    use crate::llvm::simd_ne;
                    let a: $usize_ty = crate::mem::transmute(self);
                    let b: $usize_ty = crate::mem::transmute(other);
                    Simd(simd_ne(a.0, b.0))
                }
            }

            /// Lane-wise less-than comparison.
            #[inline]
            pub fn lt(self, other: Self) -> $mask_ty {
                unsafe {
                    use crate::llvm::simd_lt;
                    let a: $usize_ty = crate::mem::transmute(self);
                    let b: $usize_ty = crate::mem::transmute(other);
                    Simd(simd_lt(a.0, b.0))
                }
            }

            /// Lane-wise less-than-or-equals comparison.
            #[inline]
            pub fn le(self, other: Self) -> $mask_ty {
                unsafe {
                    use crate::llvm::simd_le;
                    let a: $usize_ty = crate::mem::transmute(self);
                    let b: $usize_ty = crate::mem::transmute(other);
                    Simd(simd_le(a.0, b.0))
                }
            }

            /// Lane-wise greater-than comparison.
            #[inline]
            pub fn gt(self, other: Self) -> $mask_ty {
                unsafe {
                    use crate::llvm::simd_gt;
                    let a: $usize_ty = crate::mem::transmute(self);
                    let b: $usize_ty = crate::mem::transmute(other);
                    Simd(simd_gt(a.0, b.0))
                }
            }

            /// Lane-wise greater-than-or-equals comparison.
            #[inline]
            pub fn ge(self, other: Self) -> $mask_ty {
                unsafe {
                    use crate::llvm::simd_ge;
                    let a: $usize_ty = crate::mem::transmute(self);
                    let b: $usize_ty = crate::mem::transmute(other);
                    Simd(simd_ge(a.0, b.0))
                }
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _cmp_vertical>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn cmp() {
                        let a = $id::<i32>::null();
                        let b = $id::<i32>::splat(unsafe {
                            crate::mem::transmute(1_isize)
                        });

                        let r = a.lt(b);
                        let e = $mask_ty::splat(true);
                        assert!(r == e);
                        let r = a.le(b);
                        assert!(r == e);

                        let e = $mask_ty::splat(false);
                        let r = a.gt(b);
                        assert!(r == e);
                        let r = a.ge(b);
                        assert!(r == e);
                        let r = a.eq(b);
                        assert!(r == e);

                        let mut a = a;
                        let mut b = b;
                        let mut e = e;
                        for i in 0..$id::<i32>::lanes() {
                            if i % 2 == 0 {
                                a = a.replace(
                                    i,
                                    unsafe { crate::mem::transmute(0_isize) }
                                );
                                b = b.replace(
                                    i,
                                    unsafe { crate::mem::transmute(1_isize) }
                                );
                                e = e.replace(i, true);
                            } else {
                                a = a.replace(
                                    i,
                                    unsafe { crate::mem::transmute(1_isize) }
                                );
                                b = b.replace(
                                    i,
                                    unsafe { crate::mem::transmute(0_isize) }
                                );
                                e = e.replace(i, false);
                            }
                        }
                        let r = a.lt(b);
                        assert!(r == e);
                    }
                }
            }
        }

        #[allow(clippy::partialeq_ne_impl)]
        impl<T> crate::cmp::PartialEq<$id<T>> for $id<T> {
            #[inline]
            fn eq(&self, other: &Self) -> bool {
                $id::<T>::eq(*self, *other).all()
            }
            #[inline]
            fn ne(&self, other: &Self) -> bool {
                $id::<T>::ne(*self, *other).any()
            }
        }

        // FIXME: https://github.com/rust-lang-nursery/rust-clippy/issues/2892
        #[allow(clippy::partialeq_ne_impl)]
        impl<T> crate::cmp::PartialEq<LexicographicallyOrdered<$id<T>>>
            for LexicographicallyOrdered<$id<T>>
        {
            #[inline]
            fn eq(&self, other: &Self) -> bool {
                self.0 == other.0
            }
            #[inline]
            fn ne(&self, other: &Self) -> bool {
                self.0 != other.0
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _cmp_PartialEq>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn partial_eq() {
                        let a = $id::<i32>::null();
                        let b = $id::<i32>::splat(unsafe {
                            crate::mem::transmute(1_isize)
                        });

                        assert!(a != b);
                        assert!(!(a == b));
                        assert!(a == a);
                        assert!(!(a != a));

                        if $id::<i32>::lanes() > 1 {
                            let a = $id::<i32>::null().replace(0, unsafe {
                                crate::mem::transmute(1_isize)
                            });
                            let b = $id::<i32>::splat(unsafe {
                                crate::mem::transmute(1_isize)
                            });

                            assert!(a != b);
                            assert!(!(a == b));
                            assert!(a == a);
                            assert!(!(a != a));
                        }
                    }
                }
            }
        }

        impl<T> crate::cmp::Eq for $id<T> {}
        impl<T> crate::cmp::Eq for LexicographicallyOrdered<$id<T>> {}

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _cmp_eq>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn eq() {
                        fn foo<E: crate::cmp::Eq>(_: E) {}
                        let a = $id::<i32>::null();
                        foo(a);
                    }
                }
            }
        }

        impl<T> From<[$elem_ty; $elem_count]> for $id<T> {
            #[inline]
            fn from(array: [$elem_ty; $elem_count]) -> Self {
                unsafe {
                    // FIXME: unnecessary zeroing; better than UB.
                    let mut u: Self = crate::mem::zeroed();
                    crate::ptr::copy_nonoverlapping(
                        &array as *const [$elem_ty; $elem_count] as *const u8,
                        &mut u as *mut Self as *mut u8,
                        crate::mem::size_of::<Self>()
                    );
                    u
                }
            }
        }
        impl<T> Into<[$elem_ty; $elem_count]> for $id<T> {
            #[inline]
            fn into(self) -> [$elem_ty; $elem_count] {
                unsafe {
                    // FIXME: unnecessary zeroing; better than UB.
                    let mut u: [$elem_ty; $elem_count] = crate::mem::zeroed();
                    crate::ptr::copy_nonoverlapping(
                        &self as *const $id<T> as *const u8,
                        &mut u as *mut [$elem_ty; $elem_count] as *mut u8,
                        crate::mem::size_of::<Self>()
                    );
                    u
                }
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _from>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn array() {
                        let values = [1_i32; $elem_count];

                        let mut vec: $id<i32> = Default::default();
                        let mut array = [
                            $id::<i32>::null().extract(0); $elem_count
                        ];

                        for i in 0..$elem_count {
                            let ptr = unsafe {
                                crate::mem::transmute(
                                    &values[i] as *const i32
                                )
                            };
                            vec = vec.replace(i, ptr);
                            array[i] = ptr;
                        }

                        // FIXME: there is no impl of From<$id<T>> for [$elem_ty; N]
                        // let a0 = From::from(vec);
                        // assert_eq!(a0, array);
                        #[allow(unused_assignments)]
                        let mut a1 = array;
                        a1 = vec.into();
                        assert_eq!(a1, array);

                        let v0: $id<i32> = From::from(array);
                        assert_eq!(v0, vec);
                        let v1: $id<i32> = array.into();
                        assert_eq!(v1, vec);
                    }
                }
            }
        }

        impl<T> $id<T> {
            /// Instantiates a new vector with the values of the `slice`.
            ///
            /// # Panics
            ///
            /// If `slice.len() < Self::lanes()` or `&slice[0]` is not aligned
            /// to an `align_of::<Self>()` boundary.
            #[inline]
            pub fn from_slice_aligned(slice: &[$elem_ty]) -> Self {
                unsafe {
                    assert!(slice.len() >= $elem_count);
                    let target_ptr = slice.get_unchecked(0) as *const $elem_ty;
                    assert!(
                        target_ptr.align_offset(crate::mem::align_of::<Self>())
                            == 0
                    );
                    Self::from_slice_aligned_unchecked(slice)
                }
            }

            /// Instantiates a new vector with the values of the `slice`.
            ///
            /// # Panics
            ///
            /// If `slice.len() < Self::lanes()`.
            #[inline]
            pub fn from_slice_unaligned(slice: &[$elem_ty]) -> Self {
                unsafe {
                    assert!(slice.len() >= $elem_count);
                    Self::from_slice_unaligned_unchecked(slice)
                }
            }

            /// Instantiates a new vector with the values of the `slice`.
            ///
            /// # Precondition
            ///
            /// If `slice.len() < Self::lanes()` or `&slice[0]` is not aligned
            /// to an `align_of::<Self>()` boundary, the behavior is undefined.
            #[inline]
            pub unsafe fn from_slice_aligned_unchecked(slice: &[$elem_ty])
                                                       -> Self {
                #[allow(clippy::cast_ptr_alignment)]
                *(slice.get_unchecked(0) as *const $elem_ty as *const Self)
            }

            /// Instantiates a new vector with the values of the `slice`.
            ///
            /// # Precondition
            ///
            /// If `slice.len() < Self::lanes()` the behavior is undefined.
            #[inline]
            pub unsafe fn from_slice_unaligned_unchecked(
                slice: &[$elem_ty],
            ) -> Self {
                use crate::mem::size_of;
                let target_ptr =
                    slice.get_unchecked(0) as *const $elem_ty as *const u8;
                let mut x = Self::splat(crate::ptr::null_mut() as $elem_ty);
                let self_ptr = &mut x as *mut Self as *mut u8;
                crate::ptr::copy_nonoverlapping(
                    target_ptr,
                    self_ptr,
                    size_of::<Self>(),
                );
                x
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _slice_from_slice>] {
                    use super::*;
                    use crate::iter::Iterator;

                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn from_slice_unaligned() {
                        let (null, non_null) = ptr_vals!($id<i32>);

                        let mut unaligned = [
                            non_null; $id::<i32>::lanes() + 1
                        ];
                        unaligned[0] = null;
                        let vec = $id::<i32>::from_slice_unaligned(
                            &unaligned[1..]
                        );
                        for (index, &b) in unaligned.iter().enumerate() {
                            if index == 0 {
                                assert_eq!(b, null);
                            } else {
                                assert_eq!(b, non_null);
                                assert_eq!(b, vec.extract(index - 1));
                            }
                        }
                    }

                    // FIXME: wasm-bindgen-test does not support #[should_panic]
                    // #[cfg_attr(not(target_arch = "wasm32"), test)]
                    // #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    #[cfg(not(target_arch = "wasm32"))]
                    #[test]
                    #[should_panic]
                    fn from_slice_unaligned_fail() {
                        let (_null, non_null) = ptr_vals!($id<i32>);
                        let unaligned = [non_null; $id::<i32>::lanes() + 1];
                        // the slice is not large enough => panic
                        let _vec = $id::<i32>::from_slice_unaligned(
                            &unaligned[2..]
                        );
                    }

                    union A {
                        data: [<$id<i32> as sealed::Simd>::Element;
                               2 * $id::<i32>::lanes()],
                        _vec: $id<i32>,
                    }

                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn from_slice_aligned() {
                        let (null, non_null) = ptr_vals!($id<i32>);
                        let mut aligned = A {
                            data: [null; 2 * $id::<i32>::lanes()],
                        };
                        for i in
                            $id::<i32>::lanes()..(2 * $id::<i32>::lanes()) {
                            unsafe {
                                aligned.data[i] = non_null;
                            }
                        }

                        let vec = unsafe {
                            $id::<i32>::from_slice_aligned(
                                &aligned.data[$id::<i32>::lanes()..]
                            )
                        };
                        for (index, &b) in unsafe {
                            aligned.data.iter().enumerate()
                        } {
                            if index < $id::<i32>::lanes() {
                                assert_eq!(b, null);
                            } else {
                                assert_eq!(b, non_null);
                                assert_eq!(
                                    b, vec.extract(index - $id::<i32>::lanes())
                                );
                            }
                        }
                    }

                    // FIXME: wasm-bindgen-test does not support #[should_panic]
                    // #[cfg_attr(not(target_arch = "wasm32"), test)]
                    // #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    #[cfg(not(target_arch = "wasm32"))]
                    #[test]
                    #[should_panic]
                    fn from_slice_aligned_fail_lanes() {
                        let (_null, non_null) = ptr_vals!($id<i32>);
                        let aligned = A {
                            data: [non_null; 2 * $id::<i32>::lanes()],
                        };
                        // the slice is not large enough => panic
                        let _vec = unsafe {
                            $id::<i32>::from_slice_aligned(
                                &aligned.data[2 * $id::<i32>::lanes()..]
                            )
                        };
                    }

                    // FIXME: wasm-bindgen-test does not support #[should_panic]
                    // #[cfg_attr(not(target_arch = "wasm32"), test)]
                    // #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    #[cfg(not(target_arch = "wasm32"))]
                    #[test]
                    #[should_panic]
                    fn from_slice_aligned_fail_align() {
                        unsafe {
                            let (null, _non_null) = ptr_vals!($id<i32>);
                            let aligned = A {
                                data: [null; 2 * $id::<i32>::lanes()],
                            };

                            // get a pointer to the front of data
                            let ptr = aligned.data.as_ptr();
                            // offset pointer by one element
                            let ptr = ptr.wrapping_add(1);

                            if ptr.align_offset(
                                crate::mem::align_of::<$id<i32>>()
                            ) == 0 {
                                // the pointer is properly aligned, so
                                // from_slice_aligned won't fail here (e.g. this
                                // can happen for i128x1). So we panic to make
                                // the "should_fail" test pass:
                                panic!("ok");
                            }

                            // create a slice - this is safe, because the
                            // elements of the slice exist, are properly
                            // initialized, and properly aligned:
                            let s = slice::from_raw_parts(
                                ptr, $id::<i32>::lanes()
                            );
                            // this should always panic because the slice
                            // alignment does not match the alignment
                            // requirements for the vector type:
                            let _vec = $id::<i32>::from_slice_aligned(s);
                        }
                    }
                }
            }
        }

        impl<T> $id<T> {
            /// Writes the values of the vector to the `slice`.
            ///
            /// # Panics
            ///
            /// If `slice.len() < Self::lanes()` or `&slice[0]` is not
            /// aligned to an `align_of::<Self>()` boundary.
            #[inline]
            pub fn write_to_slice_aligned(self, slice: &mut [$elem_ty]) {
                unsafe {
                    assert!(slice.len() >= $elem_count);
                    let target_ptr =
                        slice.get_unchecked_mut(0) as *mut $elem_ty;
                    assert!(
                        target_ptr.align_offset(crate::mem::align_of::<Self>())
                            == 0
                    );
                    self.write_to_slice_aligned_unchecked(slice);
                }
            }

            /// Writes the values of the vector to the `slice`.
            ///
            /// # Panics
            ///
            /// If `slice.len() < Self::lanes()`.
            #[inline]
            pub fn write_to_slice_unaligned(self, slice: &mut [$elem_ty]) {
                unsafe {
                    assert!(slice.len() >= $elem_count);
                    self.write_to_slice_unaligned_unchecked(slice);
                }
            }

            /// Writes the values of the vector to the `slice`.
            ///
            /// # Precondition
            ///
            /// If `slice.len() < Self::lanes()` or `&slice[0]` is not
            /// aligned to an `align_of::<Self>()` boundary, the behavior is
            /// undefined.
            #[inline]
            pub unsafe fn write_to_slice_aligned_unchecked(
                self, slice: &mut [$elem_ty],
            ) {
                #[allow(clippy::cast_ptr_alignment)]
                *(slice.get_unchecked_mut(0) as *mut $elem_ty as *mut Self) =
                    self;
            }

            /// Writes the values of the vector to the `slice`.
            ///
            /// # Precondition
            ///
            /// If `slice.len() < Self::lanes()` the behavior is undefined.
            #[inline]
            pub unsafe fn write_to_slice_unaligned_unchecked(
                self, slice: &mut [$elem_ty],
            ) {
                let target_ptr =
                    slice.get_unchecked_mut(0) as *mut $elem_ty as *mut u8;
                let self_ptr = &self as *const Self as *const u8;
                crate::ptr::copy_nonoverlapping(
                    self_ptr,
                    target_ptr,
                    crate::mem::size_of::<Self>(),
                );
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _slice_write_to_slice>] {
                    use super::*;
                    use crate::iter::Iterator;

                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn write_to_slice_unaligned() {
                        let (null, non_null) = ptr_vals!($id<i32>);
                        let mut unaligned = [null; $id::<i32>::lanes() + 1];
                        let vec = $id::<i32>::splat(non_null);
                        vec.write_to_slice_unaligned(&mut unaligned[1..]);
                        for (index, &b) in unaligned.iter().enumerate() {
                            if index == 0 {
                                assert_eq!(b, null);
                            } else {
                                assert_eq!(b, non_null);
                                assert_eq!(b, vec.extract(index - 1));
                            }
                        }
                    }

                    // FIXME: wasm-bindgen-test does not support #[should_panic]
                    // #[cfg_attr(not(target_arch = "wasm32"), test)]
                    // #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    #[cfg(not(target_arch = "wasm32"))]
                    #[test]
                    #[should_panic]
                    fn write_to_slice_unaligned_fail() {
                        let (null, non_null) = ptr_vals!($id<i32>);
                        let mut unaligned = [null; $id::<i32>::lanes() + 1];
                        let vec = $id::<i32>::splat(non_null);
                        // the slice is not large enough => panic
                        vec.write_to_slice_unaligned(&mut unaligned[2..]);
                    }

                    union A {
                        data: [<$id<i32> as sealed::Simd>::Element;
                               2 * $id::<i32>::lanes()],
                        _vec: $id<i32>,
                    }

                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn write_to_slice_aligned() {
                        let (null, non_null) = ptr_vals!($id<i32>);
                        let mut aligned = A {
                            data: [null; 2 * $id::<i32>::lanes()],
                        };
                        let vec = $id::<i32>::splat(non_null);
                        unsafe {
                            vec.write_to_slice_aligned(
                                &mut aligned.data[$id::<i32>::lanes()..]
                            )
                        };
                        for (index, &b) in
                            unsafe { aligned.data.iter().enumerate() } {
                            if index < $id::<i32>::lanes() {
                                assert_eq!(b, null);
                            } else {
                                assert_eq!(b, non_null);
                                assert_eq!(
                                    b, vec.extract(index - $id::<i32>::lanes())
                                );
                            }
                        }
                    }

                    // FIXME: wasm-bindgen-test does not support #[should_panic]
                    // #[cfg_attr(not(target_arch = "wasm32"), test)]
                    // #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    #[cfg(not(target_arch = "wasm32"))]
                    #[test]
                    #[should_panic]
                    fn write_to_slice_aligned_fail_lanes() {
                        let (null, non_null) = ptr_vals!($id<i32>);
                        let mut aligned = A {
                            data: [null; 2 * $id::<i32>::lanes()],
                        };
                        let vec = $id::<i32>::splat(non_null);
                        // the slice is not large enough => panic
                        unsafe {
                            vec.write_to_slice_aligned(
                                &mut aligned.data[2 * $id::<i32>::lanes()..]
                            )
                        };
                    }

                    // FIXME: wasm-bindgen-test does not support #[should_panic]
                    // #[cfg_attr(not(target_arch = "wasm32"), test)]
                    // #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    #[cfg(not(target_arch = "wasm32"))]
                    #[test]
                    #[should_panic]
                    fn write_to_slice_aligned_fail_align() {
                        let (null, non_null) = ptr_vals!($id<i32>);
                        unsafe {
                            let mut aligned = A {
                                data: [null; 2 * $id::<i32>::lanes()],
                            };

                            // get a pointer to the front of data
                            let ptr = aligned.data.as_mut_ptr();
                            // offset pointer by one element
                            let ptr = ptr.wrapping_add(1);

                            if ptr.align_offset(
                                crate::mem::align_of::<$id<i32>>()
                            ) == 0 {
                                // the pointer is properly aligned, so
                                // write_to_slice_aligned won't fail here (e.g.
                                // this can happen for i128x1). So we panic to
                                // make the "should_fail" test pass:
                                panic!("ok");
                            }

                            // create a slice - this is safe, because the
                            // elements of the slice exist, are properly
                            // initialized, and properly aligned:
                            let s = slice::from_raw_parts_mut(
                                ptr, $id::<i32>::lanes()
                            );
                            // this should always panic because the slice
                            // alignment does not match the alignment
                            // requirements for the vector type:
                            let vec = $id::<i32>::splat(non_null);
                            vec.write_to_slice_aligned(s);
                        }
                    }
                }
            }
        }

        impl<T> crate::hash::Hash for $id<T> {
            #[inline]
            fn hash<H: crate::hash::Hasher>(&self, state: &mut H) {
                let s: $usize_ty = unsafe { crate::mem::transmute(*self) };
                s.hash(state)
            }
        }

        test_if! {
            $test_tt:
            paste::item! {
                pub mod [<$id _hash>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn hash() {
                        use crate::hash::{Hash, Hasher};
                        #[allow(deprecated)]
                        use crate::hash::{SipHasher13};

                        let values = [1_i32; $elem_count];

                        let mut vec: $id<i32> = Default::default();
                        let mut array = [
                            $id::<i32>::null().extract(0);
                            $elem_count
                        ];

                        for i in 0..$elem_count {
                            let ptr = unsafe {
                                crate::mem::transmute(
                                    &values[i] as *const i32
                                )
                            };
                            vec = vec.replace(i, ptr);
                            array[i] = ptr;
                        }

                        #[allow(deprecated)]
                        let mut a_hash = SipHasher13::new();
                        let mut v_hash = a_hash.clone();
                        array.hash(&mut a_hash);
                        vec.hash(&mut v_hash);
                        assert_eq!(a_hash.finish(), v_hash.finish());
                    }
                }
            }
        }

        impl<T> $id<T> {
            /// Calculates the offset from a pointer.
            ///
            /// `count` is in units of `T`; e.g. a count of `3` represents a
            /// pointer offset of `3 * size_of::<T>()` bytes.
            ///
            /// # Safety
            ///
            /// If any of the following conditions are violated, the result is
            /// Undefined Behavior:
            ///
            /// * Both the starting and resulting pointer must be either in
            /// bounds or one byte past the end of an allocated object.
            ///
            /// * The computed offset, in bytes, cannot overflow an `isize`.
            ///
            /// * The offset being in bounds cannot rely on "wrapping around"
            /// the address space. That is, the infinite-precision sum, in bytes
            /// must fit in a `usize`.
            ///
            /// The compiler and standard library generally tries to ensure
            /// allocations never reach a size where an offset is a concern. For
            /// instance, `Vec` and `Box` ensure they never allocate more than
            /// `isize::MAX` bytes, so `vec.as_ptr().offset(vec.len() as isize)`
            /// is always safe.
            ///
            /// Most platforms fundamentally can't even construct such an
            /// allocation. For instance, no known 64-bit platform can ever
            /// serve a request for 263 bytes due to page-table limitations or
            /// splitting the address space. However, some 32-bit and 16-bit
            /// platforms may successfully serve a request for more than
            /// `isize::MAX` bytes with things like Physical Address Extension.
            /// As such, memory acquired directly from allocators or memory
            /// mapped files may be too large to handle with this function.
            ///
            /// Consider using `wrapping_offset` instead if these constraints
            /// are difficult to satisfy. The only advantage of this method is
            /// that it enables more aggressive compiler optimizations.
            #[inline]
            pub unsafe fn offset(self, count: $isize_ty) -> Self {
                // FIXME: should use LLVM's `add nsw nuw`
                self.wrapping_offset(count)
            }

            /// Calculates the offset from a pointer using wrapping arithmetic.
            ///
            /// `count` is in units of `T`; e.g. a count of `3` represents a
            /// pointer offset of `3 * size_of::<T>()` bytes.
            ///
            /// # Safety
            ///
            /// The resulting pointer does not need to be in bounds, but it is
            /// potentially hazardous to dereference (which requires unsafe).
            ///
            /// Always use `.offset(count)` instead when possible, because
            /// offset allows the compiler to optimize better.
            #[inline]
            pub fn wrapping_offset(self, count: $isize_ty) -> Self {
                unsafe {
                    let x: $isize_ty = crate::mem::transmute(self);
                    // note: {+,*} currently performs a `wrapping_{add, mul}`
                    crate::mem::transmute(
                        x + (count * crate::mem::size_of::<T>() as isize)
                    )
                }
            }

            /// Calculates the distance between two pointers.
            ///
            /// The returned value is in units of `T`: the distance in bytes is
            /// divided by `mem::size_of::<T>()`.
            ///
            /// This function is the inverse of offset.
            ///
            /// # Safety
            ///
            /// If any of the following conditions are violated, the result is
            /// Undefined Behavior:
            ///
            /// * Both the starting and other pointer must be either in bounds
            /// or one byte past the end of the same allocated object.
            ///
            /// * The distance between the pointers, in bytes, cannot overflow
            /// an `isize`.
            ///
            /// * The distance between the pointers, in bytes, must be an exact
            /// multiple of the size of `T`.
            ///
            /// * The distance being in bounds cannot rely on "wrapping around"
            /// the address space.
            ///
            /// The compiler and standard library generally try to ensure
            /// allocations never reach a size where an offset is a concern. For
            /// instance, `Vec` and `Box` ensure they never allocate more than
            /// `isize::MAX` bytes, so `ptr_into_vec.offset_from(vec.as_ptr())`
            /// is always safe.
            ///
            /// Most platforms fundamentally can't even construct such an
            /// allocation. For instance, no known 64-bit platform can ever
            /// serve a request for 263 bytes due to page-table limitations or
            /// splitting the address space. However, some 32-bit and 16-bit
            /// platforms may successfully serve a request for more than
            /// `isize::MAX` bytes with things like Physical Address Extension.
            /// As such, memory acquired directly from allocators or memory
            /// mapped files may be too large to handle with this function.
            ///
            /// Consider using wrapping_offset_from instead if these constraints
            /// are difficult to satisfy. The only advantage of this method is
            /// that it enables more aggressive compiler optimizations.
            #[inline]
            pub unsafe fn offset_from(self, origin: Self) -> $isize_ty {
                // FIXME: should use LLVM's `sub nsw nuw`.
                self.wrapping_offset_from(origin)
            }

            /// Calculates the distance between two pointers.
            ///
            /// The returned value is in units of `T`: the distance in bytes is
            /// divided by `mem::size_of::<T>()`.
            ///
            /// If the address different between the two pointers is not a
            /// multiple of `mem::size_of::<T>()` then the result of the
            /// division is rounded towards zero.
            ///
            /// Though this method is safe for any two pointers, note that its
            /// result will be mostly useless if the two pointers aren't into
            /// the same allocated object, for example if they point to two
            /// different local variables.
            #[inline]
            pub fn wrapping_offset_from(self, origin: Self) -> $isize_ty {
                let x: $isize_ty = unsafe { crate::mem::transmute(self) };
                let y: $isize_ty = unsafe { crate::mem::transmute(origin) };
                // note: {-,/} currently perform wrapping_{sub, div}
                (y - x) / (crate::mem::size_of::<T>() as isize)
            }

            /// Calculates the offset from a pointer (convenience for
            /// `.offset(count as isize)`).
            ///
            /// `count` is in units of `T`; e.g. a count of 3 represents a
            /// pointer offset of `3 * size_of::<T>()` bytes.
            ///
            /// # Safety
            ///
            /// If any of the following conditions are violated, the result is
            /// Undefined Behavior:
            ///
            /// * Both the starting and resulting pointer must be either in
            /// bounds or one byte past the end of an allocated object.
            ///
            /// * The computed offset, in bytes, cannot overflow an `isize`.
            ///
            /// * The offset being in bounds cannot rely on "wrapping around"
            /// the address space. That is, the infinite-precision sum must fit
            /// in a `usize`.
            ///
            /// The compiler and standard library generally tries to ensure
            /// allocations never reach a size where an offset is a concern. For
            /// instance, `Vec` and `Box` ensure they never allocate more than
            /// `isize::MAX` bytes, so `vec.as_ptr().add(vec.len())` is always
            /// safe.
            ///
            /// Most platforms fundamentally can't even construct such an
            /// allocation. For instance, no known 64-bit platform can ever
            /// serve a request for 263 bytes due to page-table limitations or
            /// splitting the address space. However, some 32-bit and 16-bit
            /// platforms may successfully serve a request for more than
            /// `isize::MAX` bytes with things like Physical Address Extension.
            /// As such, memory acquired directly from allocators or memory
            /// mapped files may be too large to handle with this function.
            ///
            /// Consider using `wrapping_offset` instead if these constraints
            /// are difficult to satisfy. The only advantage of this method is
            /// that it enables more aggressive compiler optimizations.
            #[inline]
            #[allow(clippy::should_implement_trait)]
            pub unsafe fn add(self, count: $usize_ty) -> Self {
                self.offset(count.cast())
            }

            /// Calculates the offset from a pointer (convenience for
            /// `.offset((count as isize).wrapping_neg())`).
            ///
            /// `count` is in units of T; e.g. a `count` of 3 represents a
            /// pointer offset of `3 * size_of::<T>()` bytes.
            ///
            /// # Safety
            ///
            /// If any of the following conditions are violated, the result is
            /// Undefined Behavior:
            ///
            /// * Both the starting and resulting pointer must be either in
            /// bounds or one byte past the end of an allocated object.
            ///
            /// * The computed offset cannot exceed `isize::MAX` **bytes**.
            ///
            /// * The offset being in bounds cannot rely on "wrapping around"
            /// the address space. That is, the infinite-precision sum must fit
            /// in a usize.
            ///
            /// The compiler and standard library generally tries to ensure
            /// allocations never reach a size where an offset is a concern. For
            /// instance, `Vec` and `Box` ensure they never allocate more than
            /// `isize::MAX` bytes, so
            /// `vec.as_ptr().add(vec.len()).sub(vec.len())` is always safe.
            ///
            /// Most platforms fundamentally can't even construct such an
            /// allocation. For instance, no known 64-bit platform can ever
            /// serve a request for 2<sup>63</sup> bytes due to page-table
            /// limitations or splitting the address space. However, some 32-bit
            /// and 16-bit platforms may successfully serve a request for more
            /// than `isize::MAX` bytes with things like Physical Address
            /// Extension. As such, memory acquired directly from allocators or
            /// memory mapped files *may* be too large to handle with this
            /// function.
            ///
            /// Consider using `wrapping_offset` instead if these constraints
            /// are difficult to satisfy. The only advantage of this method is
            /// that it enables more aggressive compiler optimizations.
            #[inline]
            #[allow(clippy::should_implement_trait)]
            pub unsafe fn sub(self, count: $usize_ty) -> Self {
                let x: $isize_ty = count.cast();
                // note: - is currently wrapping_neg
                self.offset(-x)
            }

            /// Calculates the offset from a pointer using wrapping arithmetic.
            /// (convenience for `.wrapping_offset(count as isize)`)
            ///
            /// `count` is in units of T; e.g. a `count` of 3 represents a
            /// pointer offset of `3 * size_of::<T>()` bytes.
            ///
            /// # Safety
            ///
            /// The resulting pointer does not need to be in bounds, but it is
            /// potentially hazardous to dereference (which requires `unsafe`).
            ///
            /// Always use `.add(count)` instead when possible, because `add`
            /// allows the compiler to optimize better.
            #[inline]
            pub fn wrapping_add(self, count: $usize_ty) -> Self {
                self.wrapping_offset(count.cast())
            }

            /// Calculates the offset from a pointer using wrapping arithmetic.
            /// (convenience for `.wrapping_offset((count as
            /// isize).wrapping_sub())`)
            ///
            /// `count` is in units of T; e.g. a `count` of 3 represents a
            /// pointer offset of `3 * size_of::<T>()` bytes.
            ///
            /// # Safety
            ///
            /// The resulting pointer does not need to be in bounds, but it is
            /// potentially hazardous to dereference (which requires `unsafe`).
            ///
            /// Always use `.sub(count)` instead when possible, because `sub`
            /// allows the compiler to optimize better.
            #[inline]
            pub fn wrapping_sub(self, count: $usize_ty) -> Self {
                let x: $isize_ty = count.cast();
                self.wrapping_offset(-1 * x)
            }
        }

        impl<T> $id<T> {
            /// Shuffle vector elements according to `indices`.
            #[inline]
            pub fn shuffle1_dyn<I>(self, indices: I) -> Self
                where
                Self: codegen::shuffle1_dyn::Shuffle1Dyn<Indices = I>,
            {
                codegen::shuffle1_dyn::Shuffle1Dyn::shuffle1_dyn(self, indices)
            }
        }

        test_if! {
                $test_tt:
            paste::item! {
                pub mod [<$id _shuffle1_dyn>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn shuffle1_dyn() {
                        let (null, non_null) = ptr_vals!($id<i32>);

                        // alternating = [non_null, null, non_null, null, ...]
                        let mut alternating = $id::<i32>::splat(null);
                        for i in 0..$id::<i32>::lanes() {
                            if i % 2 == 0 {
                                alternating = alternating.replace(i, non_null);
                            }
                        }

                        type Indices = <$id<i32>
                            as codegen::shuffle1_dyn::Shuffle1Dyn>::Indices;
                        // even = [0, 0, 2, 2, 4, 4, ..]
                        let even = {
                            let mut v = Indices::splat(0);
                            for i in 0..$id::<i32>::lanes() {
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
                            for i in 0..$id::<i32>::lanes() {
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
                            $id::<i32>::splat(non_null)
                        );
                        if $id::<i32>::lanes() > 1 {
                            assert_eq!(
                                alternating.shuffle1_dyn(odd),
                                $id::<i32>::splat(null)
                            );
                        }
                    }
                }
            }
        }
    };
}
