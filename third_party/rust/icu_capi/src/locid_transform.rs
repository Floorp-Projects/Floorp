// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use alloc::boxed::Box;
    use icu_locid_transform::{LocaleCanonicalizer, LocaleExpander, TransformResult};

    use crate::{locale::ffi::ICU4XLocale, provider::ffi::ICU4XDataProvider};

    use crate::errors::ffi::ICU4XError;

    /// FFI version of `TransformResult`.
    #[diplomat::rust_link(icu::locid_transform::TransformResult, Enum)]
    #[diplomat::enum_convert(TransformResult)]
    pub enum ICU4XTransformResult {
        Modified,
        Unmodified,
    }

    /// A locale canonicalizer.
    #[diplomat::rust_link(icu::locid_transform::LocaleCanonicalizer, Struct)]
    #[diplomat::opaque]
    pub struct ICU4XLocaleCanonicalizer(LocaleCanonicalizer);

    impl ICU4XLocaleCanonicalizer {
        /// Create a new [`ICU4XLocaleCanonicalizer`].
        #[diplomat::rust_link(
            icu::locid_transform::LocaleCanonicalizer::try_new_unstable,
            FnInStruct
        )]
        pub fn create(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XLocaleCanonicalizer>, ICU4XError> {
            Ok(Box::new(ICU4XLocaleCanonicalizer(
                LocaleCanonicalizer::try_new_unstable(&provider.0)?,
            )))
        }

        /// Create a new [`ICU4XLocaleCanonicalizer`] with extended data.
        #[diplomat::rust_link(
            icu::locid_transform::LocaleCanonicalizer::try_new_with_expander_unstable,
            FnInStruct
        )]
        pub fn create_extended(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XLocaleCanonicalizer>, ICU4XError> {
            let expander = LocaleExpander::try_new_extended_unstable(&provider.0)?;
            Ok(Box::new(ICU4XLocaleCanonicalizer(
                LocaleCanonicalizer::try_new_with_expander_unstable(&provider.0, expander)?,
            )))
        }

        /// FFI version of `LocaleCanonicalizer::canonicalize()`.
        #[diplomat::rust_link(icu::locid_transform::LocaleCanonicalizer::canonicalize, FnInStruct)]
        pub fn canonicalize(&self, locale: &mut ICU4XLocale) -> ICU4XTransformResult {
            self.0.canonicalize(&mut locale.0).into()
        }
    }

    /// A locale expander.
    #[diplomat::rust_link(icu::locid_transform::LocaleExpander, Struct)]
    #[diplomat::opaque]
    pub struct ICU4XLocaleExpander(LocaleExpander);

    impl ICU4XLocaleExpander {
        /// Create a new [`ICU4XLocaleExpander`].
        #[diplomat::rust_link(icu::locid_transform::LocaleExpander::try_new_unstable, FnInStruct)]
        pub fn create(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XLocaleExpander>, ICU4XError> {
            Ok(Box::new(ICU4XLocaleExpander(
                LocaleExpander::try_new_unstable(&provider.0)?,
            )))
        }

        /// Create a new [`ICU4XLocaleExpander`] with extended data.
        #[diplomat::rust_link(
            icu::locid_transform::LocaleExpander::try_new_extended_unstable,
            FnInStruct
        )]
        pub fn create_extended(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XLocaleExpander>, ICU4XError> {
            Ok(Box::new(ICU4XLocaleExpander(
                LocaleExpander::try_new_extended_unstable(&provider.0)?,
            )))
        }

        /// FFI version of `LocaleExpander::maximize()`.
        #[diplomat::rust_link(icu::locid_transform::LocaleExpander::maximize, FnInStruct)]
        pub fn maximize(&self, locale: &mut ICU4XLocale) -> ICU4XTransformResult {
            self.0.maximize(&mut locale.0).into()
        }

        /// FFI version of `LocaleExpander::minimize()`.
        #[diplomat::rust_link(icu::locid_transform::LocaleExpander::minimize, FnInStruct)]
        pub fn minimize(&self, locale: &mut ICU4XLocale) -> ICU4XTransformResult {
            self.0.minimize(&mut locale.0).into()
        }
    }
}
