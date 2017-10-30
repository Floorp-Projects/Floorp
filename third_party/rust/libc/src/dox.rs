pub use self::imp::*;

#[cfg(not(dox))]
mod imp {
    pub use core::option::Option;
    pub use core::clone::Clone;
    pub use core::marker::Copy;
    pub use core::mem;
}

#[cfg(dox)]
mod imp {
    pub enum Option<T> {
        Some(T),
        None,
    }
    impl<T: Copy> Copy for Option<T> {}
    impl<T: Clone> Clone for Option<T> {
        fn clone(&self) -> Option<T> { loop {} }
    }

    pub trait Clone {
        fn clone(&self) -> Self;
    }

    #[lang = "copy"]
    pub trait Copy {}

    #[lang = "freeze"]
    pub trait Freeze {}

    #[lang = "sync"]
    pub trait Sync {}
    impl<T> Sync for T {}

    #[lang = "sized"]
    pub trait Sized {}

    macro_rules! each_int {
        ($mac:ident) => (
            $mac!(u8);
            $mac!(u16);
            $mac!(u32);
            $mac!(u64);
            $mac!(usize);
            each_signed_int!($mac);
        )
    }

    macro_rules! each_signed_int {
        ($mac:ident) => (
            $mac!(i8);
            $mac!(i16);
            $mac!(i32);
            $mac!(i64);
            $mac!(isize);
        )
    }

    #[lang = "div"]
    pub trait Div<RHS> {
        type Output;
        fn div(self, rhs: RHS) -> Self::Output;
    }

    #[lang = "shl"]
    pub trait Shl<RHS> {
        type Output;
        fn shl(self, rhs: RHS) -> Self::Output;
    }

    #[lang = "mul"]
    pub trait Mul<RHS=Self> {
        type Output;
        fn mul(self, rhs: RHS) -> Self::Output;
    }

    #[lang = "sub"]
    pub trait Sub<RHS=Self> {
        type Output;
        fn sub(self, rhs: RHS) -> Self::Output;
    }

    #[lang = "bitor"]
    pub trait Bitor<RHS=Self> {
        type Output;
        fn bitor(self, rhs: RHS) -> Self::Output;
    }

    #[lang = "neg"]
    pub trait Neg {
        type Output;
        fn neg(self) -> Self::Output;
    }

    #[lang = "not"]
    pub trait Not {
        type Output;
        fn not(self) -> Self::Output;
    }

    #[lang = "add"]
    pub trait Add<RHS = Self> {
        type Output;
        fn add(self, r: RHS) -> Self::Output;
    }

    macro_rules! impl_traits {
        ($($i:ident)*) => ($(
            impl Div<$i> for $i {
                type Output = $i;
                fn div(self, rhs: $i) -> $i { self / rhs }
            }
            impl Shl<$i> for $i {
                type Output = $i;
                fn shl(self, rhs: $i) -> $i { self << rhs }
            }
            impl Mul for $i {
                type Output = $i;
                fn mul(self, rhs: $i) -> $i { self * rhs }
            }

            impl Sub for $i {
                type Output = $i;
                fn sub(self, rhs: $i) -> $i { self - rhs }
            }
            impl Bitor for $i {
                type Output = $i;
                fn bitor(self, rhs: $i) -> $i { self | rhs }
            }
            impl Neg for $i {
                type Output = $i;
                fn neg(self) -> $i { -self }
            }
            impl Not for $i {
                type Output = $i;
                fn not(self) -> $i { !self }
            }
            impl Add<$i> for $i {
                type Output = $i;
                fn add(self, other: $i) -> $i { self + other }
            }
        )*)
    }
    each_int!(impl_traits);

    pub mod mem {
        pub fn size_of_val<T>(_: &T) -> usize { 4 }
    }
}
