//! Implements vertical floating-point math operations.

#[macro_use]
mod abs;

#[macro_use]
mod consts;

#[macro_use]
mod cos;

#[macro_use]
mod exp;

#[macro_use]
mod powf;

#[macro_use]
mod ln;

#[macro_use]
mod mul_add;

#[macro_use]
mod mul_adde;

#[macro_use]
mod recpre;

#[macro_use]
mod rsqrte;

#[macro_use]
mod sin;

#[macro_use]
mod sqrt;

#[macro_use]
mod sqrte;

#[macro_use]
mod tanh;

macro_rules! impl_float_category {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident, $mask_ty:ident) => {
        impl $id {
            #[inline]
            pub fn is_nan(self) -> $mask_ty {
                self.ne(self)
            }

            #[inline]
            pub fn is_infinite(self) -> $mask_ty {
                self.eq(Self::INFINITY) | self.eq(Self::NEG_INFINITY)
            }

            #[inline]
            pub fn is_finite(self) -> $mask_ty {
                !(self.is_nan() | self.is_infinite())
            }
        }
    };
}
