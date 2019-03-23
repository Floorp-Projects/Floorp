//! Mask types

macro_rules! impl_mask_ty {
    ($id:ident : $elem_ty:ident | #[$doc:meta]) => {
        #[$doc]
        #[derive(Copy, Clone)]
        pub struct $id($elem_ty);

        impl crate::sealed::Mask for $id {
            fn test(&self) -> bool {
                $id::test(self)
            }
        }

        impl $id {
            /// Instantiate a mask with `value`
            #[inline]
            pub fn new(x: bool) -> Self {
                if x {
                    $id(!0)
                } else {
                    $id(0)
                }
            }
            /// Test if the mask is set
            #[inline]
            pub fn test(&self) -> bool {
                self.0 != 0
            }
        }

        impl Default for $id {
            #[inline]
            fn default() -> Self {
                $id(0)
            }
        }

        #[allow(clippy::partialeq_ne_impl)]
        impl PartialEq<$id> for $id {
            #[inline]
            fn eq(&self, other: &Self) -> bool {
                self.0 == other.0
            }
            #[inline]
            fn ne(&self, other: &Self) -> bool {
                self.0 != other.0
            }
        }

        impl Eq for $id {}

        impl PartialOrd<$id> for $id {
            #[inline]
            fn partial_cmp(
                &self, other: &Self,
            ) -> Option<crate::cmp::Ordering> {
                use crate::cmp::Ordering;
                if self == other {
                    Some(Ordering::Equal)
                } else if self.0 > other.0 {
                    // Note:
                    //  * false = 0_i
                    //  * true == !0_i == -1_i
                    Some(Ordering::Less)
                } else {
                    Some(Ordering::Greater)
                }
            }

            #[inline]
            fn lt(&self, other: &Self) -> bool {
                self.0 > other.0
            }
            #[inline]
            fn gt(&self, other: &Self) -> bool {
                self.0 < other.0
            }
            #[inline]
            fn le(&self, other: &Self) -> bool {
                self.0 >= other.0
            }
            #[inline]
            fn ge(&self, other: &Self) -> bool {
                self.0 <= other.0
            }
        }

        impl Ord for $id {
            #[inline]
            fn cmp(&self, other: &Self) -> crate::cmp::Ordering {
                match self.partial_cmp(other) {
                    Some(x) => x,
                    None => unsafe { crate::hint::unreachable_unchecked() },
                }
            }
        }

        impl crate::hash::Hash for $id {
            #[inline]
            fn hash<H: crate::hash::Hasher>(&self, state: &mut H) {
                (self.0 != 0).hash(state);
            }
        }

        impl crate::fmt::Debug for $id {
            #[inline]
            fn fmt(
                &self, fmtter: &mut crate::fmt::Formatter<'_>,
            ) -> Result<(), crate::fmt::Error> {
                write!(fmtter, "{}({})", stringify!($id), self.0 != 0)
            }
        }
    };
}

impl_mask_ty!(m8: i8 | /// 8-bit wide mask.
);
impl_mask_ty!(m16: i16 | /// 16-bit wide mask.
);
impl_mask_ty!(m32: i32 | /// 32-bit wide mask.
);
impl_mask_ty!(m64: i64 | /// 64-bit wide mask.
);
impl_mask_ty!(m128: i128 | /// 128-bit wide mask.
);
impl_mask_ty!(msize: isize | /// isize-wide mask.
);
