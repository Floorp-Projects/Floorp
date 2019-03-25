//! Default implementation of a mask reduction for any target.

macro_rules! fallback_to_other_impl {
    ($id:ident, $other:ident) => {
        impl All for $id {
            #[inline]
            unsafe fn all(self) -> bool {
                let m: $other = crate::mem::transmute(self);
                m.all()
            }
        }
        impl Any for $id {
            #[inline]
            unsafe fn any(self) -> bool {
                let m: $other = crate::mem::transmute(self);
                m.any()
            }
        }
    };
}

/// Fallback implementation.
macro_rules! fallback_impl {
    // 16-bit wide masks:
    (m8x2) => {
        impl All for m8x2 {
            #[inline]
            unsafe fn all(self) -> bool {
                let i: u16 = crate::mem::transmute(self);
                i == u16::max_value()
            }
        }
        impl Any for m8x2 {
            #[inline]
            unsafe fn any(self) -> bool {
                let i: u16 = crate::mem::transmute(self);
                i != 0
            }
        }
    };
    // 32-bit wide masks
    (m8x4) => {
        impl All for m8x4 {
            #[inline]
            unsafe fn all(self) -> bool {
                let i: u32 = crate::mem::transmute(self);
                i == u32::max_value()
            }
        }
        impl Any for m8x4 {
            #[inline]
            unsafe fn any(self) -> bool {
                let i: u32 = crate::mem::transmute(self);
                i != 0
            }
        }
    };
    (m16x2) => {
        fallback_to_other_impl!(m16x2, m8x4);
    };
    // 64-bit wide masks:
    (m8x8) => {
        impl All for m8x8 {
            #[inline]
            unsafe fn all(self) -> bool {
                let i: u64 = crate::mem::transmute(self);
                i == u64::max_value()
            }
        }
        impl Any for m8x8 {
            #[inline]
            unsafe fn any(self) -> bool {
                let i: u64 = crate::mem::transmute(self);
                i != 0
            }
        }
    };
    (m16x4) => {
        fallback_to_other_impl!(m16x4, m8x8);
    };
    (m32x2) => {
        fallback_to_other_impl!(m32x2, m16x4);
    };
    // FIXME: 64x1 maxk
    // 128-bit wide masks:
    (m8x16) => {
        impl All for m8x16 {
            #[inline]
            unsafe fn all(self) -> bool {
                let i: u128 = crate::mem::transmute(self);
                i == u128::max_value()
            }
        }
        impl Any for m8x16 {
            #[inline]
            unsafe fn any(self) -> bool {
                let i: u128 = crate::mem::transmute(self);
                i != 0
            }
        }
    };
    (m16x8) => {
        fallback_to_other_impl!(m16x8, m8x16);
    };
    (m32x4) => {
        fallback_to_other_impl!(m32x4, m16x8);
    };
    (m64x2) => {
        fallback_to_other_impl!(m64x2, m32x4);
    };
    (m128x1) => {
        fallback_to_other_impl!(m128x1, m64x2);
    };
    // 256-bit wide masks
    (m8x32) => {
        impl All for m8x32 {
            #[inline]
            unsafe fn all(self) -> bool {
                let i: [u128; 2] = crate::mem::transmute(self);
                let o: [u128; 2] = [u128::max_value(); 2];
                i == o
            }
        }
        impl Any for m8x32 {
            #[inline]
            unsafe fn any(self) -> bool {
                let i: [u128; 2] = crate::mem::transmute(self);
                let o: [u128; 2] = [0; 2];
                i != o
            }
        }
    };
    (m16x16) => {
        fallback_to_other_impl!(m16x16, m8x32);
    };
    (m32x8) => {
        fallback_to_other_impl!(m32x8, m16x16);
    };
    (m64x4) => {
        fallback_to_other_impl!(m64x4, m32x8);
    };
    (m128x2) => {
        fallback_to_other_impl!(m128x2, m64x4);
    };
    // 512-bit wide masks
    (m8x64) => {
        impl All for m8x64 {
            #[inline]
            unsafe fn all(self) -> bool {
                let i: [u128; 4] = crate::mem::transmute(self);
                let o: [u128; 4] = [u128::max_value(); 4];
                i == o
            }
        }
        impl Any for m8x64 {
            #[inline]
            unsafe fn any(self) -> bool {
                let i: [u128; 4] = crate::mem::transmute(self);
                let o: [u128; 4] = [0; 4];
                i != o
            }
        }
    };
    (m16x32) => {
        fallback_to_other_impl!(m16x32, m8x64);
    };
    (m32x16) => {
        fallback_to_other_impl!(m32x16, m16x32);
    };
    (m64x8) => {
        fallback_to_other_impl!(m64x8, m32x16);
    };
    (m128x4) => {
        fallback_to_other_impl!(m128x4, m64x8);
    };
    // Masks with pointer-sized elements64
    (msizex2) => {
        cfg_if! {
            if #[cfg(target_pointer_width = "64")] {
                fallback_to_other_impl!(msizex2, m64x2);
            } else if #[cfg(target_pointer_width = "32")] {
                fallback_to_other_impl!(msizex2, m32x2);
            } else {
                compile_error!("unsupported target_pointer_width");
            }
        }
    };
    (msizex4) => {
        cfg_if! {
            if #[cfg(target_pointer_width = "64")] {
                fallback_to_other_impl!(msizex4, m64x4);
            } else if #[cfg(target_pointer_width = "32")] {
                fallback_to_other_impl!(msizex4, m32x4);
            } else {
                compile_error!("unsupported target_pointer_width");
            }
        }
    };
    (msizex8) => {
        cfg_if! {
            if #[cfg(target_pointer_width = "64")] {
                fallback_to_other_impl!(msizex8, m64x8);
            } else if #[cfg(target_pointer_width = "32")] {
                fallback_to_other_impl!(msizex8, m32x8);
            } else {
                compile_error!("unsupported target_pointer_width");
            }
        }
    };
}

macro_rules! recurse_half {
    ($vid:ident, $vid_h:ident) => {
        impl All for $vid {
            #[inline]
            unsafe fn all(self) -> bool {
                union U {
                    halves: ($vid_h, $vid_h),
                    vec: $vid,
                }
                let halves = U { vec: self }.halves;
                halves.0.all() && halves.1.all()
            }
        }
        impl Any for $vid {
            #[inline]
            unsafe fn any(self) -> bool {
                union U {
                    halves: ($vid_h, $vid_h),
                    vec: $vid,
                }
                let halves = U { vec: self }.halves;
                halves.0.any() || halves.1.any()
            }
        }
    };
}
