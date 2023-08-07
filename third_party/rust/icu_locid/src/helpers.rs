// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::iter::FromIterator;

use alloc::boxed::Box;
use alloc::vec;
use alloc::vec::Vec;
use litemap::store::*;

/// Internal: A vector that supports no-allocation, constant values if length 0 or 1.
/// Using ZeroOne(Option<T>) saves 8 bytes in ShortSlice via niche optimization.
#[derive(Debug, Clone, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub(crate) enum ShortSlice<T> {
    ZeroOne(Option<T>),
    Multi(Box<[T]>),
}

impl<T> ShortSlice<T> {
    #[inline]
    pub const fn new() -> Self {
        Self::ZeroOne(None)
    }

    #[inline]
    pub const fn new_single(item: T) -> Self {
        Self::ZeroOne(Some(item))
    }

    pub fn push(&mut self, item: T) {
        *self = match core::mem::replace(self, Self::ZeroOne(None)) {
            ShortSlice::ZeroOne(None) => ShortSlice::ZeroOne(Some(item)),
            ShortSlice::ZeroOne(Some(prev_item)) => {
                ShortSlice::Multi(vec![prev_item, item].into_boxed_slice())
            }
            ShortSlice::Multi(items) => {
                let mut items = items.into_vec();
                items.push(item);
                ShortSlice::Multi(items.into_boxed_slice())
            }
        };
    }

    #[inline]
    pub fn as_slice(&self) -> &[T] {
        match self {
            ShortSlice::ZeroOne(None) => &[],
            ShortSlice::ZeroOne(Some(v)) => core::slice::from_ref(v),
            ShortSlice::Multi(v) => v,
        }
    }

    #[inline]
    pub fn as_mut_slice(&mut self) -> &mut [T] {
        match self {
            ShortSlice::ZeroOne(None) => &mut [],
            ShortSlice::ZeroOne(Some(v)) => core::slice::from_mut(v),
            ShortSlice::Multi(v) => v,
        }
    }

    #[inline]
    pub const fn single(&self) -> Option<&T> {
        match self {
            ShortSlice::ZeroOne(Some(v)) => Some(v),
            _ => None,
        }
    }

    #[inline]
    pub fn len(&self) -> usize {
        match self {
            ShortSlice::ZeroOne(None) => 0,
            ShortSlice::ZeroOne(_) => 1,
            ShortSlice::Multi(ref v) => v.len(),
        }
    }

    pub fn insert(&mut self, index: usize, elt: T) {
        assert!(
            index <= self.len(),
            "insertion index (is {}) should be <= len (is {})",
            index,
            self.len()
        );

        *self = match core::mem::replace(self, ShortSlice::ZeroOne(None)) {
            ShortSlice::ZeroOne(None) => ShortSlice::ZeroOne(Some(elt)),
            ShortSlice::ZeroOne(Some(item)) => {
                let items = if index == 0 {
                    vec![elt, item].into_boxed_slice()
                } else {
                    vec![item, elt].into_boxed_slice()
                };
                ShortSlice::Multi(items)
            }
            ShortSlice::Multi(items) => {
                let mut items = items.into_vec();
                items.insert(index, elt);
                ShortSlice::Multi(items.into_boxed_slice())
            }
        }
    }

    pub fn remove(&mut self, index: usize) -> T {
        assert!(
            index < self.len(),
            "removal index (is {}) should be < len (is {})",
            index,
            self.len()
        );

        let (replaced, removed_item) = match core::mem::replace(self, ShortSlice::ZeroOne(None)) {
            ShortSlice::ZeroOne(None) => unreachable!(),
            ShortSlice::ZeroOne(Some(v)) => (ShortSlice::ZeroOne(None), v),
            ShortSlice::Multi(v) => {
                let mut v = v.into_vec();
                let removed_item = v.remove(index);
                match v.len() {
                    #[allow(clippy::unwrap_used)]
                    // we know that the vec has exactly one element left
                    1 => (ShortSlice::ZeroOne(Some(v.pop().unwrap())), removed_item),
                    // v has at least 2 elements, create a Multi variant
                    _ => (ShortSlice::Multi(v.into_boxed_slice()), removed_item),
                }
            }
        };
        *self = replaced;
        removed_item
    }

    #[inline]
    pub fn clear(&mut self) {
        let _ = core::mem::replace(self, ShortSlice::ZeroOne(None));
    }
}

impl<T> From<Vec<T>> for ShortSlice<T> {
    fn from(v: Vec<T>) -> Self {
        match v.len() {
            0 => ShortSlice::ZeroOne(None),
            #[allow(clippy::unwrap_used)] // we know that the vec is not empty
            1 => ShortSlice::ZeroOne(Some(v.into_iter().next().unwrap())),
            _ => ShortSlice::Multi(v.into_boxed_slice()),
        }
    }
}

impl<T> Default for ShortSlice<T> {
    fn default() -> Self {
        ShortSlice::ZeroOne(None)
    }
}

impl<T> FromIterator<T> for ShortSlice<T> {
    fn from_iter<I: IntoIterator<Item = T>>(iter: I) -> Self {
        iter.into_iter().collect::<Vec<_>>().into()
    }
}

impl<K, V> StoreConstEmpty<K, V> for ShortSlice<(K, V)> {
    const EMPTY: ShortSlice<(K, V)> = ShortSlice::ZeroOne(None);
}

impl<K, V> Store<K, V> for ShortSlice<(K, V)> {
    #[inline]
    fn lm_len(&self) -> usize {
        self.len()
    }

    #[inline]
    fn lm_is_empty(&self) -> bool {
        matches!(self, ShortSlice::ZeroOne(None))
    }

    #[inline]
    fn lm_get(&self, index: usize) -> Option<(&K, &V)> {
        self.as_slice().get(index).map(|elt| (&elt.0, &elt.1))
    }

    #[inline]
    fn lm_last(&self) -> Option<(&K, &V)> {
        match self {
            ShortSlice::ZeroOne(v) => v.as_ref(),
            ShortSlice::Multi(v) => v.last(),
        }
        .map(|elt| (&elt.0, &elt.1))
    }

    #[inline]
    fn lm_binary_search_by<F>(&self, mut cmp: F) -> Result<usize, usize>
    where
        F: FnMut(&K) -> core::cmp::Ordering,
    {
        self.as_slice().binary_search_by(|(k, _)| cmp(k))
    }
}

impl<K: Ord, V> StoreFromIterable<K, V> for ShortSlice<(K, V)> {
    fn lm_sort_from_iter<I: IntoIterator<Item = (K, V)>>(iter: I) -> Self {
        let v: Vec<(K, V)> = Vec::lm_sort_from_iter(iter);
        v.into()
    }
}

impl<K, V> StoreMut<K, V> for ShortSlice<(K, V)> {
    fn lm_with_capacity(_capacity: usize) -> Self {
        ShortSlice::ZeroOne(None)
    }

    fn lm_reserve(&mut self, _additional: usize) {}

    fn lm_get_mut(&mut self, index: usize) -> Option<(&K, &mut V)> {
        self.as_mut_slice()
            .get_mut(index)
            .map(|elt| (&elt.0, &mut elt.1))
    }

    fn lm_push(&mut self, key: K, value: V) {
        self.push((key, value))
    }

    fn lm_insert(&mut self, index: usize, key: K, value: V) {
        self.insert(index, (key, value))
    }

    fn lm_remove(&mut self, index: usize) -> (K, V) {
        self.remove(index)
    }

    fn lm_clear(&mut self) {
        self.clear();
    }
}

impl<'a, K: 'a, V: 'a> StoreIterable<'a, K, V> for ShortSlice<(K, V)> {
    type KeyValueIter =
        core::iter::Map<core::slice::Iter<'a, (K, V)>, for<'r> fn(&'r (K, V)) -> (&'r K, &'r V)>;

    fn lm_iter(&'a self) -> Self::KeyValueIter {
        self.as_slice().iter().map(|elt| (&elt.0, &elt.1))
    }
}

impl<K, V> StoreFromIterator<K, V> for ShortSlice<(K, V)> {}

#[test]
fn test_shortvec_impl() {
    litemap::testing::check_store::<ShortSlice<(u32, u64)>>();
}

macro_rules! impl_tinystr_subtag {
    (
        $(#[$doc:meta])*
        $name:ident,
        $($full_name:ident)::+,
        $macro_name:ident,
        $len_start:literal..=$len_end:literal,
        $tinystr_ident:ident,
        $validate:expr,
        $normalize:expr,
        $is_normalized:expr,
        $error:ident,
        [$good_example:literal $(,$more_good_examples:literal)*],
        [$bad_example:literal $(, $more_bad_examples:literal)*],
    ) => {
        #[derive(Debug, PartialEq, Eq, Clone, Hash, PartialOrd, Ord, Copy)]
        #[cfg_attr(feature = "serde", derive(serde::Serialize))]
        #[repr(transparent)]
        $(#[$doc])*
        pub struct $name(tinystr::TinyAsciiStr<$len_end>);

        impl $name {
            /// A constructor which takes a UTF-8 slice, parses it and
            #[doc = concat!("produces a well-formed [`", stringify!($name), "`].")]
            ///
            /// # Examples
            ///
            /// ```
            #[doc = concat!("use icu_locid::", stringify!($($full_name)::+), ";")]
            ///
            #[doc = concat!("assert!(", stringify!($name), "::try_from_bytes(b", stringify!($good_example), ").is_ok());")]
            #[doc = concat!("assert!(", stringify!($name), "::try_from_bytes(b", stringify!($bad_example), ").is_err());")]
            /// ```
            pub const fn try_from_bytes(v: &[u8]) -> Result<Self, crate::parser::errors::ParserError> {
                Self::try_from_bytes_manual_slice(v, 0, v.len())
            }

            /// Equivalent to [`try_from_bytes(bytes[start..end])`](Self::try_from_bytes),
            /// but callable in a `const` context (which range indexing is not).
            pub const fn try_from_bytes_manual_slice(
                v: &[u8],
                start: usize,
                end: usize,
            ) -> Result<Self, crate::parser::errors::ParserError> {
                let slen = end - start;

                #[allow(clippy::double_comparisons)] // if len_start == len_end
                if slen < $len_start || slen > $len_end {
                    return Err(crate::parser::errors::ParserError::$error);
                }

                match tinystr::TinyAsciiStr::from_bytes_manual_slice(v, start, end) {
                    Ok($tinystr_ident) if $validate => Ok(Self($normalize)),
                    _ => Err(crate::parser::errors::ParserError::$error),
                }
            }

            #[doc = concat!("Safely creates a [`", stringify!($name), "`] from its raw format")]
            /// as returned by [`Self::into_raw`]. Unlike [`Self::try_from_bytes`],
            /// this constructor only takes normalized values.
            pub const fn try_from_raw(
                v: [u8; $len_end],
            ) -> Result<Self, crate::parser::errors::ParserError> {
                if let Ok($tinystr_ident) = tinystr::TinyAsciiStr::<$len_end>::try_from_raw(v) {
                    if $tinystr_ident.len() >= $len_start && $is_normalized {
                        Ok(Self($tinystr_ident))
                    } else {
                        Err(crate::parser::errors::ParserError::$error)
                    }
                } else {
                    Err(crate::parser::errors::ParserError::$error)
                }
            }

            #[doc = concat!("Unsafely creates a [`", stringify!($name), "`] from its raw format")]
            /// as returned by [`Self::into_raw`]. Unlike [`Self::try_from_bytes`],
            /// this constructor only takes normalized values.
            ///
            /// # Safety
            ///
            /// This function is safe iff [`Self::try_from_raw`] returns an `Ok`. This is the case
            /// for inputs that are correctly normalized.
            pub const unsafe fn from_raw_unchecked(v: [u8; $len_end]) -> Self {
                Self(tinystr::TinyAsciiStr::from_bytes_unchecked(v))
            }

            /// Deconstructs into a raw format to be consumed by
            /// [`from_raw_unchecked`](Self::from_raw_unchecked()) or
            /// [`try_from_raw`](Self::try_from_raw()).
            pub const fn into_raw(self) -> [u8; $len_end] {
                *self.0.all_bytes()
            }

            #[inline]
            /// A helper function for displaying as a `&str`.
            pub const fn as_str(&self) -> &str {
                self.0.as_str()
            }

            /// Compare with BCP-47 bytes.
            ///
            /// The return value is equivalent to what would happen if you first converted
            /// `self` to a BCP-47 string and then performed a byte comparison.
            ///
            /// This function is case-sensitive and results in a *total order*, so it is appropriate for
            /// binary search. The only argument producing [`Ordering::Equal`](core::cmp::Ordering::Equal)
            /// is `self.as_str().as_bytes()`.
            #[inline]
            pub fn strict_cmp(self, other: &[u8]) -> core::cmp::Ordering {
                self.as_str().as_bytes().cmp(other)
            }

            /// Compare with a potentially unnormalized BCP-47 string.
            ///
            /// The return value is equivalent to what would happen if you first parsed the
            /// BCP-47 string and then performed a structural comparison.
            ///
            #[inline]
            pub fn normalizing_eq(self, other: &str) -> bool {
                self.as_str().eq_ignore_ascii_case(other)
            }
        }

        impl core::str::FromStr for $name {
            type Err = crate::parser::errors::ParserError;

            fn from_str(source: &str) -> Result<Self, Self::Err> {
                Self::try_from_bytes(source.as_bytes())
            }
        }

        impl<'l> From<&'l $name> for &'l str {
            fn from(input: &'l $name) -> Self {
                input.as_str()
            }
        }

        impl From<$name> for tinystr::TinyAsciiStr<$len_end> {
            fn from(input: $name) -> Self {
                input.0
            }
        }

        impl writeable::Writeable for $name {
            #[inline]
            fn write_to<W: core::fmt::Write + ?Sized>(&self, sink: &mut W) -> core::fmt::Result {
                sink.write_str(self.as_str())
            }
            #[inline]
            fn writeable_length_hint(&self) -> writeable::LengthHint {
                writeable::LengthHint::exact(self.0.len())
            }
            #[inline]
            fn write_to_string(&self) -> alloc::borrow::Cow<str> {
                alloc::borrow::Cow::Borrowed(self.0.as_str())
            }
        }

        writeable::impl_display_with_writeable!($name);

        #[doc = concat!("A macro allowing for compile-time construction of valid [`", stringify!($name), "`] subtags.")]
        ///
        /// # Examples
        ///
        /// Parsing errors don't have to be handled at runtime:
        /// ```
        /// assert_eq!(
        #[doc = concat!("  icu_locid::", stringify!($macro_name), "!(", stringify!($good_example) ,"),")]
        #[doc = concat!("  ", stringify!($good_example), ".parse::<icu_locid::", stringify!($($full_name)::+),">().unwrap()")]
        /// );
        /// ```
        ///
        /// Invalid input is a compile failure:
        /// ```compile_fail,E0080
        #[doc = concat!("icu_locid::", stringify!($macro_name), "!(", stringify!($bad_example) ,");")]
        /// ```
        ///
        #[doc = concat!("[`", stringify!($name), "`]: crate::", stringify!($($full_name)::+))]
        #[macro_export]
        macro_rules! $macro_name {
            ($string:literal) => {{
                use $crate::$($full_name)::+;
                const R: $name =
                    match $name::try_from_bytes($string.as_bytes()) {
                        Ok(r) => r,
                        #[allow(clippy::panic)] // const context
                        _ => panic!(concat!("Invalid ", stringify!($name), ": ", $string)),
                    };
                R
            }};
        }

        #[cfg(feature = "databake")]
        impl databake::Bake for $name {
            fn bake(&self, env: &databake::CrateEnv) -> databake::TokenStream {
                env.insert("icu_locid");
                let string = self.as_str();
                databake::quote! {::icu_locid::$macro_name!(#string) }
            }
        }

        #[test]
        fn test_construction() {
            let maybe = $name::try_from_bytes($good_example.as_bytes());
            assert!(maybe.is_ok());
            assert_eq!(maybe, $name::try_from_raw(maybe.unwrap().into_raw()));
            assert_eq!(maybe.unwrap().as_str(), $good_example);
            $(
                let maybe = $name::try_from_bytes($more_good_examples.as_bytes());
                assert!(maybe.is_ok());
                assert_eq!(maybe, $name::try_from_raw(maybe.unwrap().into_raw()));
                assert_eq!(maybe.unwrap().as_str(), $more_good_examples);
            )*
            assert!($name::try_from_bytes($bad_example.as_bytes()).is_err());
            $(
                assert!($name::try_from_bytes($more_bad_examples.as_bytes()).is_err());
            )*
        }

        #[test]
        fn test_writeable() {
            writeable::assert_writeable_eq!(&$good_example.parse::<$name>().unwrap(), $good_example);
            $(
                writeable::assert_writeable_eq!($more_good_examples.parse::<$name>().unwrap(), $more_good_examples);
            )*
        }

        #[cfg(feature = "serde")]
        impl<'de> serde::Deserialize<'de> for $name {
            fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
            where
                D: serde::de::Deserializer<'de>,
            {
                struct Visitor;

                impl<'de> serde::de::Visitor<'de> for Visitor {
                    type Value = $name;

                    fn expecting(
                        &self,
                        formatter: &mut core::fmt::Formatter<'_>,
                    ) -> core::fmt::Result {
                        write!(formatter, "a valid BCP-47 {}", stringify!($name))
                    }

                    fn visit_str<E: serde::de::Error>(self, s: &str) -> Result<Self::Value, E> {
                        s.parse().map_err(serde::de::Error::custom)
                    }
                }

                if deserializer.is_human_readable() {
                    deserializer.deserialize_string(Visitor)
                } else {
                    Self::try_from_raw(serde::de::Deserialize::deserialize(deserializer)?)
                        .map_err(serde::de::Error::custom)
                }
            }
        }

        // Safety checklist for ULE:
        //
        // 1. Must not include any uninitialized or padding bytes (true since transparent over a ULE).
        // 2. Must have an alignment of 1 byte (true since transparent over a ULE).
        // 3. ULE::validate_byte_slice() checks that the given byte slice represents a valid slice.
        // 4. ULE::validate_byte_slice() checks that the given byte slice has a valid length.
        // 5. All other methods must be left with their default impl.
        // 6. Byte equality is semantic equality.
        #[cfg(feature = "zerovec")]
        unsafe impl zerovec::ule::ULE for $name {
            fn validate_byte_slice(bytes: &[u8]) -> Result<(), zerovec::ZeroVecError> {
                let it = bytes.chunks_exact(core::mem::size_of::<Self>());
                if !it.remainder().is_empty() {
                    return Err(zerovec::ZeroVecError::length::<Self>(bytes.len()));
                }
                for v in it {
                    // The following can be removed once `array_chunks` is stabilized.
                    let mut a = [0; core::mem::size_of::<Self>()];
                    a.copy_from_slice(v);
                    if Self::try_from_raw(a).is_err() {
                        return Err(zerovec::ZeroVecError::parse::<Self>());
                    }
                }
                Ok(())
            }
        }

        #[cfg(feature = "zerovec")]
        impl zerovec::ule::AsULE for $name {
            type ULE = Self;
            fn to_unaligned(self) -> Self::ULE {
                self
            }
            fn from_unaligned(unaligned: Self::ULE) -> Self {
                unaligned
            }
        }

        #[cfg(feature = "zerovec")]
        impl<'a> zerovec::maps::ZeroMapKV<'a> for $name {
            type Container = zerovec::ZeroVec<'a, $name>;
            type Slice = zerovec::ZeroSlice<$name>;
            type GetType = $name;
            type OwnedType = $name;
        }
    };
}

macro_rules! impl_writeable_for_each_subtag_str_no_test {
    ($type:tt $(, $self:ident, $borrow_cond:expr => $borrow:expr)?) => {
        impl writeable::Writeable for $type {
            fn write_to<W: core::fmt::Write + ?Sized>(&self, sink: &mut W) -> core::fmt::Result {
                let mut initial = true;
                self.for_each_subtag_str(&mut |subtag| {
                    if initial {
                        initial = false;
                    } else {
                        sink.write_char('-')?;
                    }
                    sink.write_str(subtag)
                })
            }

            #[inline]
            fn writeable_length_hint(&self) -> writeable::LengthHint {
                let mut result = writeable::LengthHint::exact(0);
                let mut initial = true;
                self.for_each_subtag_str::<core::convert::Infallible, _>(&mut |subtag| {
                    if initial {
                        initial = false;
                    } else {
                        result += 1;
                    }
                    result += subtag.len();
                    Ok(())
                })
                .expect("infallible");
                result
            }

            $(
                fn write_to_string(&self) -> alloc::borrow::Cow<str> {
                    #[allow(clippy::unwrap_used)] // impl_writeable_for_subtag_list's $borrow uses unwrap
                    let $self = self;
                    if $borrow_cond {
                        $borrow
                    } else {
                        let mut output = alloc::string::String::with_capacity(self.writeable_length_hint().capacity());
                        let _ = self.write_to(&mut output);
                        alloc::borrow::Cow::Owned(output)
                    }
                }
            )?
        }

        writeable::impl_display_with_writeable!($type);
    };
}

macro_rules! impl_writeable_for_subtag_list {
    ($type:tt, $sample1:literal, $sample2:literal) => {
        impl_writeable_for_each_subtag_str_no_test!($type, selff, selff.0.len() == 1 => alloc::borrow::Cow::Borrowed(selff.0.as_slice().get(0).unwrap().as_str()));

        #[test]
        fn test_writeable() {
            writeable::assert_writeable_eq!(&$type::default(), "");
            writeable::assert_writeable_eq!(
                &$type::from_vec_unchecked(alloc::vec![$sample1.parse().unwrap()]),
                $sample1,
            );
            writeable::assert_writeable_eq!(
                &$type::from_vec_unchecked(vec![
                    $sample1.parse().unwrap(),
                    $sample2.parse().unwrap()
                ]),
                core::concat!($sample1, "-", $sample2),
            );
        }
    };
}

macro_rules! impl_writeable_for_key_value {
    ($type:tt, $key1:literal, $value1:literal, $key2:literal, $expected2:literal) => {
        impl_writeable_for_each_subtag_str_no_test!($type);

        #[test]
        fn test_writeable() {
            writeable::assert_writeable_eq!(&$type::default(), "");
            writeable::assert_writeable_eq!(
                &$type::from_tuple_vec(vec![($key1.parse().unwrap(), $value1.parse().unwrap())]),
                core::concat!($key1, "-", $value1),
            );
            writeable::assert_writeable_eq!(
                &$type::from_tuple_vec(vec![
                    ($key1.parse().unwrap(), $value1.parse().unwrap()),
                    ($key2.parse().unwrap(), "true".parse().unwrap())
                ]),
                core::concat!($key1, "-", $value1, "-", $expected2),
            );
        }
    };
}
