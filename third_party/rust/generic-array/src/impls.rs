use super::{ArrayLength, GenericArray};
use core::borrow::{Borrow, BorrowMut};
use core::cmp::Ordering;
use core::fmt::{self, Debug};
use core::hash::{Hash, Hasher};
use functional::*;
use sequence::*;

impl<T: Default, N> Default for GenericArray<T, N>
where
    N: ArrayLength<T>,
{
    #[inline]
    fn default() -> Self {
        Self::generate(|_| T::default())
    }
}

impl<T: Clone, N> Clone for GenericArray<T, N>
where
    N: ArrayLength<T>,
{
    fn clone(&self) -> GenericArray<T, N> {
        self.map(Clone::clone)
    }
}

impl<T: Copy, N> Copy for GenericArray<T, N>
where
    N: ArrayLength<T>,
    N::ArrayType: Copy,
{
}

impl<T: PartialEq, N> PartialEq for GenericArray<T, N>
where
    N: ArrayLength<T>,
{
    fn eq(&self, other: &Self) -> bool {
        **self == **other
    }
}
impl<T: Eq, N> Eq for GenericArray<T, N>
where
    N: ArrayLength<T>,
{
}

impl<T: PartialOrd, N> PartialOrd for GenericArray<T, N>
where
    N: ArrayLength<T>,
{
    fn partial_cmp(&self, other: &GenericArray<T, N>) -> Option<Ordering> {
        PartialOrd::partial_cmp(self.as_slice(), other.as_slice())
    }
}

impl<T: Ord, N> Ord for GenericArray<T, N>
where
    N: ArrayLength<T>,
{
    fn cmp(&self, other: &GenericArray<T, N>) -> Ordering {
        Ord::cmp(self.as_slice(), other.as_slice())
    }
}

impl<T: Debug, N> Debug for GenericArray<T, N>
where
    N: ArrayLength<T>,
{
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        self[..].fmt(fmt)
    }
}

impl<T, N> Borrow<[T]> for GenericArray<T, N>
where
    N: ArrayLength<T>,
{
    #[inline(always)]
    fn borrow(&self) -> &[T] {
        &self[..]
    }
}

impl<T, N> BorrowMut<[T]> for GenericArray<T, N>
where
    N: ArrayLength<T>,
{
    #[inline(always)]
    fn borrow_mut(&mut self) -> &mut [T] {
        &mut self[..]
    }
}

impl<T, N> AsRef<[T]> for GenericArray<T, N>
where
    N: ArrayLength<T>,
{
    #[inline(always)]
    fn as_ref(&self) -> &[T] {
        &self[..]
    }
}

impl<T, N> AsMut<[T]> for GenericArray<T, N>
where
    N: ArrayLength<T>,
{
    #[inline(always)]
    fn as_mut(&mut self) -> &mut [T] {
        &mut self[..]
    }
}

impl<T: Hash, N> Hash for GenericArray<T, N>
where
    N: ArrayLength<T>,
{
    fn hash<H>(&self, state: &mut H)
    where
        H: Hasher,
    {
        Hash::hash(&self[..], state)
    }
}

macro_rules! impl_from {
    ($($n: expr => $ty: ty),*) => {
        $(
            impl<T> From<[T; $n]> for GenericArray<T, $ty> {
                #[inline(always)]
                fn from(arr: [T; $n]) -> Self {
                    unsafe { $crate::transmute(arr) }
                }
            }

            impl<T> Into<[T; $n]> for GenericArray<T, $ty> {
                #[inline(always)]
                fn into(self) -> [T; $n] {
                    unsafe { $crate::transmute(self) }
                }
            }
        )*

    }
}

impl_from! {
    1  => ::typenum::U1,
    2  => ::typenum::U2,
    3  => ::typenum::U3,
    4  => ::typenum::U4,
    5  => ::typenum::U5,
    6  => ::typenum::U6,
    7  => ::typenum::U7,
    8  => ::typenum::U8,
    9  => ::typenum::U9,
    10 => ::typenum::U10,
    11 => ::typenum::U11,
    12 => ::typenum::U12,
    13 => ::typenum::U13,
    14 => ::typenum::U14,
    15 => ::typenum::U15,
    16 => ::typenum::U16,
    17 => ::typenum::U17,
    18 => ::typenum::U18,
    19 => ::typenum::U19,
    20 => ::typenum::U20,
    21 => ::typenum::U21,
    22 => ::typenum::U22,
    23 => ::typenum::U23,
    24 => ::typenum::U24,
    25 => ::typenum::U25,
    26 => ::typenum::U26,
    27 => ::typenum::U27,
    28 => ::typenum::U28,
    29 => ::typenum::U29,
    30 => ::typenum::U30,
    31 => ::typenum::U31,
    32 => ::typenum::U32
}
