#![cfg_attr(test, deny(missing_docs))]
#![cfg_attr(test, deny(warnings))]
#![cfg_attr(feature = "heap_size", feature(custom_derive, plugin))]
#![cfg_attr(feature = "heap_size", plugin(heapsize_plugin))]

//! # Case
//!
//! Case provices a way of specifying strings that are case-insensitive.
//!
//! ## Example
//!
//! ```rust
//! use unicase::UniCase;
//!
//! let a = UniCase("foobar");
//! let b = UniCase("FoObAr");
//!
//! assert_eq!(a, b);
//! ```

#[cfg(feature = "heap_size")]
extern crate heapsize;

use std::ascii::AsciiExt;
#[cfg(__unicase__iter_cmp)]
use std::cmp::Ordering;
use std::fmt;
use std::hash::{Hash, Hasher};
use std::ops::{Deref, DerefMut};
use std::str::FromStr;

/// Case Insensitive wrapper of strings.
#[derive(Copy, Clone, Debug)]
#[cfg_attr(feature = "heap_size", derive(HeapSizeOf))]
pub struct UniCase<S>(pub S);

impl<S> Deref for UniCase<S> {
    type Target = S;
    #[inline]
    fn deref<'a>(&'a self) -> &'a S {
        &self.0
    }
}

impl<S> DerefMut for UniCase<S> {
    #[inline]
    fn deref_mut<'a>(&'a mut self) -> &'a mut S {
        &mut self.0
    }
}

#[cfg(__unicase__iter_cmp)]
impl<T: AsRef<str>> PartialOrd for UniCase<T> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

#[cfg(__unicase__iter_cmp)]
impl<T: AsRef<str>> Ord for UniCase<T> {
    fn cmp(&self, other: &Self) -> Ordering {
        let self_chars = self.as_ref().chars().map(|c| c.to_ascii_lowercase());
        let other_chars = other.as_ref().chars().map(|c| c.to_ascii_lowercase());
        self_chars.cmp(other_chars)
    }
}

impl<S: AsRef<str>> AsRef<str> for UniCase<S> {
    #[inline]
    fn as_ref(&self) -> &str {
        self.0.as_ref()
    }

}

impl<S: fmt::Display> fmt::Display for UniCase<S> {
    #[inline]
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt::Display::fmt(&self.0, fmt)
    }
}

impl<S1: AsRef<str>, S2: AsRef<str>> PartialEq<S2> for UniCase<S1> {
    #[inline]
    fn eq(&self, other: &S2) -> bool {
        self.as_ref().eq_ignore_ascii_case(other.as_ref())
    }
}


impl<S: AsRef<str>> Eq for UniCase<S> {}

impl<S: FromStr> FromStr for UniCase<S> {
    type Err = <S as FromStr>::Err;
    fn from_str(s: &str) -> Result<UniCase<S>, <S as FromStr>::Err> {
        s.parse().map(UniCase)
    }
}

impl<S: AsRef<str>> Hash for UniCase<S> {
    #[inline]
    fn hash<H: Hasher>(&self, hasher: &mut H) {
        for byte in self.as_ref().bytes().map(|b| b.to_ascii_lowercase()) {
            hasher.write(&[byte]);
        }
    }
}

macro_rules! from_impl {
    ($from:ty => $to:ty; $by:ident) => (
        impl<'a> From<$from> for UniCase<$to> {
            fn from(s: $from) -> Self {
                UniCase(s.$by())
            }
        }
    );
    ($from:ty => $to:ty) => ( from_impl!($from => $to; into); )
}

macro_rules! into_impl {
    ($to:ty) => (
        impl<'a> Into<$to> for UniCase<$to> {
            fn into(self) -> $to {
                self.0
            }
        }
    );
}

from_impl!(&'a str => &'a str);
from_impl!(&'a str => String);
from_impl!(&'a String => &'a str; as_ref);
from_impl!(String => String);

into_impl!(&'a str);
into_impl!(String);

#[cfg(test)]
mod test {
    use super::UniCase;
    use std::hash::{Hash, Hasher};
    #[cfg(not(__unicase__default_hasher))]
    use std::hash::SipHasher as DefaultHasher;
    #[cfg(__unicase__default_hasher)]
    use std::collections::hash_map::DefaultHasher;

    fn hash<T: Hash>(t: &T) -> u64 {
        let mut s = DefaultHasher::new();
        t.hash(&mut s);
        s.finish()
    }

    #[test]
    fn test_copy_for_refs() {
        fn foo<T>(_: UniCase<T>) {}

        let a = UniCase("foobar");
        foo(a);
        foo(a);
    }

    #[test]
    fn test_case_insensitive() {
        let a = UniCase("foobar");
        let b = UniCase("FOOBAR");

        assert_eq!(a, b);
        assert_eq!(hash(&a), hash(&b));
    }

    #[test]
    fn test_different_string_types() {
        let a = UniCase("foobar");
        let b = "FOOBAR".to_owned();
        assert_eq!(a, b);
        assert_eq!(UniCase(b), a);
    }

    #[cfg(__unicase__iter_cmp)]
    #[test]
    fn test_case_cmp() {
        assert!(UniCase("foobar") == UniCase("FOOBAR"));
        assert!(UniCase("a") < UniCase("B"));

        assert!(UniCase("A") < UniCase("b"));
        assert!(UniCase("aa") > UniCase("a"));

        assert!(UniCase("a") < UniCase("aa"));
        assert!(UniCase("a") < UniCase("AA"));
    }

    #[test]
    fn test_from_impls() {
        let view: &'static str = "foobar";
        let _: UniCase<&'static str> = view.into();
        let _: UniCase<&str> = view.into();
        let _: UniCase<String> = view.into();

        let owned: String = view.to_owned();
        let _: UniCase<&str> = (&owned).into();
        let _: UniCase<String> = owned.into();
    }

    #[test]
    fn test_into_impls() {
        let view: UniCase<&'static str> = UniCase("foobar");
        let _: &'static str = view.into();
        let _: &str = view.into();

        let owned: UniCase<String> = "foobar".into();
        let _: String = owned.clone().into();
        let _: &str = owned.as_ref();
    }
}
