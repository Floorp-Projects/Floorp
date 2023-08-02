#![cfg_attr(not(feature = "std"), no_std)]
#![allow(dead_code, unused_imports)]

#[cfg(not(feature = "std"))]
extern crate alloc;

#[cfg(not(feature = "std"))]
use alloc::vec::Vec;

use derive_more::IntoIterator;

#[derive(IntoIterator)]
#[into_iterator(owned, ref, ref_mut)]
struct MyVec(Vec<i32>);

#[derive(IntoIterator)]
#[into_iterator(owned, ref, ref_mut)]
struct Numbers {
    numbers: Vec<i32>,
}

#[derive(IntoIterator)]
struct Numbers2 {
    #[into_iterator(owned, ref, ref_mut)]
    numbers: Vec<i32>,
    useless: bool,
    useless2: bool,
}

#[derive(IntoIterator)]
struct Numbers3 {
    #[into_iterator(ref, ref_mut)]
    numbers: Vec<i32>,
    useless: bool,
    useless2: bool,
}

// Test that owned is not enabled when ref/ref_mut are enabled without owned
impl ::core::iter::IntoIterator for Numbers3 {
    type Item = <Vec<i32> as ::core::iter::IntoIterator>::Item;
    type IntoIter = <Vec<i32> as ::core::iter::IntoIterator>::IntoIter;

    #[inline]
    fn into_iter(self) -> Self::IntoIter {
        <Vec<i32> as ::core::iter::IntoIterator>::into_iter(self.numbers)
    }
}
