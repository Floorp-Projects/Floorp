// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use crate::errors::ffi::ICU4XError;
    use crate::provider::ffi::ICU4XDataProvider;
    use alloc::boxed::Box;
    use icu_timezone::MetazoneCalculator;

    /// An object capable of computing the metazone from a timezone.
    ///
    /// This can be used via `maybe_calculate_metazone()` on [`ICU4XCustomTimeZone`].
    ///
    /// [`ICU4XCustomTimeZone`]: crate::timezone::ffi::ICU4XCustomTimeZone;
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::timezone::MetazoneCalculator, Struct)]
    pub struct ICU4XMetazoneCalculator(pub MetazoneCalculator);

    impl ICU4XMetazoneCalculator {
        #[diplomat::rust_link(icu::timezone::MetazoneCalculator::try_new_unstable, FnInStruct)]
        pub fn create(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XMetazoneCalculator>, ICU4XError> {
            Ok(Box::new(ICU4XMetazoneCalculator(
                MetazoneCalculator::try_new_unstable(&provider.0)?,
            )))
        }
    }
}
