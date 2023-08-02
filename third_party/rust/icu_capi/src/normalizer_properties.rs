// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use crate::{errors::ffi::ICU4XError, provider::ffi::ICU4XDataProvider};
    use alloc::boxed::Box;
    use icu_normalizer::properties::{
        CanonicalCombiningClassMap, CanonicalComposition, CanonicalDecomposition, Decomposed,
    };

    /// Lookup of the Canonical_Combining_Class Unicode property
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::normalizer::properties::CanonicalCombiningClassMap, Struct)]
    pub struct ICU4XCanonicalCombiningClassMap(pub CanonicalCombiningClassMap);

    impl ICU4XCanonicalCombiningClassMap {
        /// Construct a new ICU4XCanonicalCombiningClassMap instance for NFC
        #[diplomat::rust_link(
            icu::normalizer::properties::CanonicalCombiningClassMap::try_new_unstable,
            FnInStruct
        )]
        pub fn create(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCanonicalCombiningClassMap>, ICU4XError> {
            Ok(Box::new(ICU4XCanonicalCombiningClassMap(
                CanonicalCombiningClassMap::try_new_unstable(&provider.0)?,
            )))
        }

        #[diplomat::rust_link(
            icu::normalizer::properties::CanonicalCombiningClassMap::get,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::properties::properties::CanonicalCombiningClass,
            Struct,
            compact
        )]
        pub fn get(&self, ch: char) -> u8 {
            self.0.get(ch).0
        }
        #[diplomat::rust_link(
            icu::normalizer::properties::CanonicalCombiningClassMap::get32,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::properties::properties::CanonicalCombiningClass,
            Struct,
            compact
        )]
        pub fn get32(&self, ch: u32) -> u8 {
            self.0.get32(ch).0
        }
    }

    /// The raw canonical composition operation.
    ///
    /// Callers should generally use ICU4XComposingNormalizer unless they specifically need raw composition operations
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::normalizer::properties::CanonicalComposition, Struct)]
    pub struct ICU4XCanonicalComposition(pub CanonicalComposition);

    impl ICU4XCanonicalComposition {
        /// Construct a new ICU4XCanonicalComposition instance for NFC
        #[diplomat::rust_link(
            icu::normalizer::properties::CanonicalComposition::try_new_unstable,
            FnInStruct
        )]
        pub fn create(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCanonicalComposition>, ICU4XError> {
            Ok(Box::new(ICU4XCanonicalComposition(
                CanonicalComposition::try_new_unstable(&provider.0)?,
            )))
        }

        /// Performs canonical composition (including Hangul) on a pair of characters
        /// or returns NUL if these characters don’t compose. Composition exclusions are taken into account.
        #[diplomat::rust_link(
            icu::normalizer::properties::CanonicalComposition::compose,
            FnInStruct
        )]
        pub fn compose(&self, starter: char, second: char) -> char {
            self.0.compose(starter, second).unwrap_or('\0')
        }
    }

    /// The outcome of non-recursive canonical decomposition of a character.
    /// `second` will be NUL when the decomposition expands to a single character
    /// (which may or may not be the original one)
    #[diplomat::rust_link(icu::normalizer::properties::Decomposed, Enum)]
    pub struct ICU4XDecomposed {
        first: char,
        second: char,
    }

    /// The raw (non-recursive) canonical decomposition operation.
    ///
    /// Callers should generally use ICU4XDecomposingNormalizer unless they specifically need raw composition operations
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::normalizer::properties::CanonicalDecomposition, Struct)]
    pub struct ICU4XCanonicalDecomposition(pub CanonicalDecomposition);

    impl ICU4XCanonicalDecomposition {
        /// Construct a new ICU4XCanonicalDecomposition instance for NFC
        #[diplomat::rust_link(
            icu::normalizer::properties::CanonicalDecomposition::try_new_unstable,
            FnInStruct
        )]
        pub fn create(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XCanonicalDecomposition>, ICU4XError> {
            Ok(Box::new(ICU4XCanonicalDecomposition(
                CanonicalDecomposition::try_new_unstable(&provider.0)?,
            )))
        }

        /// Performs non-recursive canonical decomposition (including for Hangul).
        #[diplomat::rust_link(
            icu::normalizer::properties::CanonicalDecomposition::decompose,
            FnInStruct
        )]
        pub fn decompose(&self, c: char) -> ICU4XDecomposed {
            match self.0.decompose(c) {
                Decomposed::Default => ICU4XDecomposed {
                    first: c,
                    second: '\0',
                },
                Decomposed::Singleton(s) => ICU4XDecomposed {
                    first: s,
                    second: '\0',
                },
                Decomposed::Expansion(first, second) => ICU4XDecomposed { first, second },
            }
        }
    }
}
