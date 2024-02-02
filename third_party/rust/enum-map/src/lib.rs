// SPDX-FileCopyrightText: 2017 - 2023 Kamila Borowska <kamila@borowska.pw>
// SPDX-FileCopyrightText: 2019 Riey <creeper844@gmail.com>
// SPDX-FileCopyrightText: 2021 Alex Sayers <alex@asayers.com>
// SPDX-FileCopyrightText: 2021 Bruno Corrêa Zimmermann <brunoczim@gmail.com>
// SPDX-FileCopyrightText: 2022 Cass Fridkin <cass@cloudflare.com>
// SPDX-FileCopyrightText: 2022 Mateusz Kowalczyk <fuuzetsu@fuuzetsu.co.uk>
//
// SPDX-License-Identifier: MIT OR Apache-2.0

//! An enum mapping type.
//!
//! It is implemented using an array type, so using it is as fast as using Rust
//! arrays.
//!
//! # Examples
//!
//! ```
//! use enum_map::{enum_map, Enum, EnumMap};
//!
//! #[derive(Debug, Enum)]
//! enum Example {
//!     A(bool),
//!     B,
//!     C,
//! }
//!
//! let mut map = enum_map! {
//!     Example::A(false) => 0,
//!     Example::A(true) => 1,
//!     Example::B => 2,
//!     Example::C => 3,
//! };
//! map[Example::C] = 4;
//!
//! assert_eq!(map[Example::A(true)], 1);
//!
//! for (key, &value) in &map {
//!     println!("{:?} has {} as value.", key, value);
//! }
//! ```

#![no_std]
#![deny(missing_docs)]
#![warn(clippy::pedantic)]

#[cfg(feature = "arbitrary")]
mod arbitrary;
mod enum_map_impls;
mod internal;
mod iter;
#[cfg(feature = "serde")]
mod serde;

#[doc(hidden)]
pub use core::mem::{self, ManuallyDrop, MaybeUninit};
#[doc(hidden)]
pub use core::primitive::usize;
use core::slice;
#[doc(hidden)]
// unreachable needs to be exported for compatibility with older versions of enum-map-derive
pub use core::{panic, ptr, unreachable};
pub use enum_map_derive::Enum;
#[doc(hidden)]
pub use internal::out_of_bounds;
use internal::Array;
pub use internal::{Enum, EnumArray};
pub use iter::{IntoIter, IntoValues, Iter, IterMut, Values, ValuesMut};

// SAFETY: initialized needs to represent number of initialized elements
#[doc(hidden)]
pub struct Guard<'a, K, V>
where
    K: EnumArray<V>,
{
    array_mut: &'a mut MaybeUninit<K::Array>,
    initialized: usize,
}

impl<K, V> Drop for Guard<'_, K, V>
where
    K: EnumArray<V>,
{
    fn drop(&mut self) {
        // This is safe as arr[..len] is initialized due to
        // Guard's type invariant.
        unsafe {
            ptr::slice_from_raw_parts_mut(self.as_mut_ptr(), self.initialized).drop_in_place();
        }
    }
}

impl<'a, K, V> Guard<'a, K, V>
where
    K: EnumArray<V>,
{
    #[doc(hidden)]
    pub fn as_mut_ptr(&mut self) -> *mut V {
        self.array_mut.as_mut_ptr().cast::<V>()
    }

    #[doc(hidden)]
    #[must_use]
    pub fn new(array_mut: &'a mut MaybeUninit<K::Array>) -> Self {
        Self {
            array_mut,
            initialized: 0,
        }
    }

    #[doc(hidden)]
    #[must_use]
    #[allow(clippy::unused_self)]
    pub fn storage_length(&self) -> usize {
        // SAFETY: We need to use LENGTH from K::Array, as K::LENGTH is
        // untrustworthy.
        K::Array::LENGTH
    }

    #[doc(hidden)]
    #[must_use]
    pub fn get_key(&self) -> K {
        K::from_usize(self.initialized)
    }

    #[doc(hidden)]
    // Unsafe as it can write out of bounds.
    pub unsafe fn push(&mut self, value: V) {
        self.as_mut_ptr().add(self.initialized).write(value);
        self.initialized += 1;
    }
}

#[doc(hidden)]
pub struct TypeEqualizer<'a, K, V>
where
    K: EnumArray<V>,
{
    pub enum_map: [EnumMap<K, V>; 0],
    pub guard: Guard<'a, K, V>,
}

/// Enum map constructor.
///
/// This macro allows to create a new enum map in a type safe way. It takes
/// a list of `,` separated pairs separated by `=>`. Left side is `|`
/// separated list of enum keys, or `_` to match all unmatched enum keys,
/// while right side is a value.
///
/// The iteration order when using this macro is not guaranteed to be
/// consistent. Future releases of this crate may change it, and this is not
/// considered to be a breaking change.
///
/// # Examples
///
/// ```
/// use enum_map::{enum_map, Enum};
///
/// #[derive(Enum)]
/// enum Example {
///     A,
///     B,
///     C,
///     D,
/// }
///
/// let enum_map = enum_map! {
///     Example::A | Example::B => 1,
///     Example::C => 2,
///     _ => 3,
/// };
/// assert_eq!(enum_map[Example::A], 1);
/// assert_eq!(enum_map[Example::B], 1);
/// assert_eq!(enum_map[Example::C], 2);
/// assert_eq!(enum_map[Example::D], 3);
/// ```
#[macro_export]
macro_rules! enum_map {
    {$($t:tt)*} => {{
        let mut uninit = $crate::MaybeUninit::uninit();
        let mut eq = $crate::TypeEqualizer {
            enum_map: [],
            guard: $crate::Guard::new(&mut uninit),
        };
        if false {
            // Safe because this code is unreachable
            unsafe { (&mut eq.enum_map).as_mut_ptr().read() }
        } else {
            for _ in 0..(&eq.guard).storage_length() {
                struct __PleaseDoNotUseBreakWithoutLabel;
                let _please_do_not_use_continue_without_label;
                let value;
                #[allow(unreachable_code)]
                loop {
                    _please_do_not_use_continue_without_label = ();
                    value = match (&eq.guard).get_key() { $($t)* };
                    break __PleaseDoNotUseBreakWithoutLabel;
                };

                unsafe { (&mut eq.guard).push(value); }
            }
            $crate::mem::forget(eq);
            // Safe because the array was fully initialized.
            $crate::EnumMap::from_array(unsafe { uninit.assume_init() })
        }
    }};
}

/// An enum mapping.
///
/// This internally uses an array which stores a value for each possible
/// enum value. To work, it requires implementation of internal (private,
/// although public due to macro limitations) trait which allows extracting
/// information about an enum, which can be automatically generated using
/// `#[derive(Enum)]` macro.
///
/// Additionally, `bool` and `u8` automatically derives from `Enum`. While
/// `u8` is not technically an enum, it's convenient to consider it like one.
/// In particular, [reverse-complement in benchmark game] could be using `u8`
/// as an enum.
///
/// # Examples
///
/// ```
/// use enum_map::{enum_map, Enum, EnumMap};
///
/// #[derive(Enum)]
/// enum Example {
///     A,
///     B,
///     C,
/// }
///
/// let mut map = EnumMap::default();
/// // new initializes map with default values
/// assert_eq!(map[Example::A], 0);
/// map[Example::A] = 3;
/// assert_eq!(map[Example::A], 3);
/// ```
///
/// [reverse-complement in benchmark game]:
///     http://benchmarksgame.alioth.debian.org/u64q/program.php?test=revcomp&lang=rust&id=2
pub struct EnumMap<K: EnumArray<V>, V> {
    array: K::Array,
}

impl<K: EnumArray<V>, V: Default> EnumMap<K, V> {
    /// Clear enum map with default values.
    ///
    /// # Examples
    ///
    /// ```
    /// use enum_map::{Enum, EnumMap};
    ///
    /// #[derive(Enum)]
    /// enum Example {
    ///     A,
    ///     B,
    /// }
    ///
    /// let mut enum_map = EnumMap::<_, String>::default();
    /// enum_map[Example::B] = "foo".into();
    /// enum_map.clear();
    /// assert_eq!(enum_map[Example::A], "");
    /// assert_eq!(enum_map[Example::B], "");
    /// ```
    #[inline]
    pub fn clear(&mut self) {
        for v in self.as_mut_slice() {
            *v = V::default();
        }
    }
}

#[allow(clippy::len_without_is_empty)]
impl<K: EnumArray<V>, V> EnumMap<K, V> {
    /// Creates an enum map from array.
    #[inline]
    pub const fn from_array(array: K::Array) -> EnumMap<K, V> {
        EnumMap { array }
    }

    /// Create an enum map, where each value is the returned value from `cb`
    /// using provided enum key.
    ///
    /// ```
    /// # use enum_map_derive::*;
    /// use enum_map::{enum_map, Enum, EnumMap};
    ///
    /// #[derive(Enum, PartialEq, Debug)]
    /// enum Example {
    ///     A,
    ///     B,
    /// }
    ///
    /// let map = EnumMap::from_fn(|k| k == Example::A);
    /// assert_eq!(map, enum_map! { Example::A => true, Example::B => false })
    /// ```
    pub fn from_fn<F>(mut cb: F) -> Self
    where
        F: FnMut(K) -> V,
    {
        enum_map! { k => cb(k) }
    }

    /// Returns an iterator over enum map.
    ///
    /// The iteration order is deterministic, and when using [macro@Enum] derive
    /// it will be the order in which enum variants are declared.
    ///
    /// # Examples
    ///
    /// ```
    /// use enum_map::{enum_map, Enum};
    ///
    /// #[derive(Enum, PartialEq)]
    /// enum E {
    ///     A,
    ///     B,
    ///     C,
    /// }
    ///
    /// let map = enum_map! { E::A => 1, E::B => 2, E::C => 3};
    /// assert!(map.iter().eq([(E::A, &1), (E::B, &2), (E::C, &3)]));
    /// ```
    #[inline]
    pub fn iter(&self) -> Iter<K, V> {
        self.into_iter()
    }

    /// Returns a mutable iterator over enum map.
    #[inline]
    pub fn iter_mut(&mut self) -> IterMut<K, V> {
        self.into_iter()
    }

    /// Returns number of elements in enum map.
    #[inline]
    #[allow(clippy::unused_self)]
    pub const fn len(&self) -> usize {
        K::Array::LENGTH
    }

    /// Swaps two indexes.
    ///
    /// # Examples
    ///
    /// ```
    /// use enum_map::enum_map;
    ///
    /// let mut map = enum_map! { false => 0, true => 1 };
    /// map.swap(false, true);
    /// assert_eq!(map[false], 1);
    /// assert_eq!(map[true], 0);
    /// ```
    #[inline]
    pub fn swap(&mut self, a: K, b: K) {
        self.as_mut_slice().swap(a.into_usize(), b.into_usize());
    }

    /// Consumes an enum map and returns the underlying array.
    ///
    /// The order of elements is deterministic, and when using [macro@Enum]
    /// derive it will be the order in which enum variants are declared.
    ///
    /// # Examples
    ///
    /// ```
    /// use enum_map::{enum_map, Enum};
    ///
    /// #[derive(Enum, PartialEq)]
    /// enum E {
    ///     A,
    ///     B,
    ///     C,
    /// }
    ///
    /// let map = enum_map! { E::A => 1, E::B => 2, E::C => 3};
    /// assert_eq!(map.into_array(), [1, 2, 3]);
    /// ```
    pub fn into_array(self) -> K::Array {
        self.array
    }

    /// Returns a reference to the underlying array.
    ///
    /// The order of elements is deterministic, and when using [macro@Enum]
    /// derive it will be the order in which enum variants are declared.
    ///
    /// # Examples
    ///
    /// ```
    /// use enum_map::{enum_map, Enum};
    ///
    /// #[derive(Enum, PartialEq)]
    /// enum E {
    ///     A,
    ///     B,
    ///     C,
    /// }
    ///
    /// let map = enum_map! { E::A => 1, E::B => 2, E::C => 3};
    /// assert_eq!(map.as_array(), &[1, 2, 3]);
    /// ```
    pub const fn as_array(&self) -> &K::Array {
        &self.array
    }

    /// Returns a mutable reference to the underlying array.
    ///
    /// The order of elements is deterministic, and when using [macro@Enum]
    /// derive it will be the order in which enum variants are declared.
    ///
    /// # Examples
    ///
    /// ```
    /// use enum_map::{enum_map, Enum};
    ///
    /// #[derive(Enum, PartialEq)]
    /// enum E {
    ///     A,
    ///     B,
    ///     C,
    /// }
    ///
    /// let mut map = enum_map! { E::A => 1, E::B => 2, E::C => 3};
    /// map.as_mut_array()[1] = 42;
    /// assert_eq!(map.as_array(), &[1, 42, 3]);
    /// ```
    pub fn as_mut_array(&mut self) -> &mut K::Array {
        &mut self.array
    }

    /// Converts an enum map to a slice representing values.
    ///
    /// The order of elements is deterministic, and when using [macro@Enum]
    /// derive it will be the order in which enum variants are declared.
    ///
    /// # Examples
    ///
    /// ```
    /// use enum_map::{enum_map, Enum};
    ///
    /// #[derive(Enum, PartialEq)]
    /// enum E {
    ///     A,
    ///     B,
    ///     C,
    /// }
    ///
    /// let map = enum_map! { E::A => 1, E::B => 2, E::C => 3};
    /// assert_eq!(map.as_slice(), &[1, 2, 3]);
    /// ```
    #[inline]
    pub fn as_slice(&self) -> &[V] {
        unsafe { slice::from_raw_parts(ptr::addr_of!(self.array).cast(), K::Array::LENGTH) }
    }

    /// Converts a mutable enum map to a mutable slice representing values.
    #[inline]
    pub fn as_mut_slice(&mut self) -> &mut [V] {
        unsafe { slice::from_raw_parts_mut(ptr::addr_of_mut!(self.array).cast(), K::Array::LENGTH) }
    }

    /// Returns an enum map with function `f` applied to each element in order.
    ///
    /// # Examples
    ///
    /// ```
    /// use enum_map::enum_map;
    ///
    /// let a = enum_map! { false => 0, true => 1 };
    /// let b = a.map(|_, x| f64::from(x) + 0.5);
    /// assert_eq!(b, enum_map! { false => 0.5, true => 1.5 });
    /// ```
    pub fn map<F, T>(self, mut f: F) -> EnumMap<K, T>
    where
        F: FnMut(K, V) -> T,
        K: EnumArray<T>,
    {
        struct DropOnPanic<K, V>
        where
            K: EnumArray<V>,
        {
            position: usize,
            map: ManuallyDrop<EnumMap<K, V>>,
        }
        impl<K, V> Drop for DropOnPanic<K, V>
        where
            K: EnumArray<V>,
        {
            fn drop(&mut self) {
                unsafe {
                    ptr::drop_in_place(&mut self.map.as_mut_slice()[self.position..]);
                }
            }
        }
        let mut drop_protect = DropOnPanic {
            position: 0,
            map: ManuallyDrop::new(self),
        };
        enum_map! {
            k => {
                let value = unsafe { ptr::read(&drop_protect.map.as_slice()[drop_protect.position]) };
                drop_protect.position += 1;
                f(k, value)
            }
        }
    }
}
