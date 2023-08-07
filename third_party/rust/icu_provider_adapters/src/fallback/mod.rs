// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Tools for locale fallback, enabling arbitrary input locales to be mapped into the nearest
//! locale with data.
//!
//! The algorithm implemented in this module is called [Flexible Vertical Fallback](
//! https://docs.google.com/document/d/1Mp7EUyl-sFh_HZYgyeVwj88vJGpCBIWxzlCwGgLCDwM/edit).
//! Watch [#2243](https://github.com/unicode-org/icu4x/issues/2243) to track improvements to
//! this algorithm and steps to enshrine the algorithm in CLDR.
//!
//! # Examples
//!
//! Run the locale fallback algorithm:
//!
//! ```
//! use icu_locid::locale;
//! use icu_provider_adapters::fallback::LocaleFallbacker;
//! use icu_provider::prelude::*;
//!
//! // Set up a LocaleFallbacker with data.
//! let fallbacker = LocaleFallbacker::try_new_unstable(&icu_testdata::unstable()).expect("data");
//!
//! // Create a LocaleFallbackerWithConfig with a configuration for a specific key.
//! // By default, uses language priority with no additional extension keywords.
//! let key_fallbacker = fallbacker.for_config(Default::default());
//!
//! // Set up the fallback iterator.
//! let mut fallback_iterator = key_fallbacker.fallback_for(DataLocale::from(locale!("hi-Latn-IN")));
//!
//! // Run the algorithm and check the results.
//! assert_eq!(fallback_iterator.get(), &DataLocale::from(locale!("hi-Latn-IN")));
//! fallback_iterator.step();
//! assert_eq!(fallback_iterator.get(), &DataLocale::from(locale!("hi-Latn")));
//! fallback_iterator.step();
//! assert_eq!(fallback_iterator.get(), &DataLocale::from(locale!("en-IN")));
//! fallback_iterator.step();
//! assert_eq!(fallback_iterator.get(), &DataLocale::from(locale!("en-001")));
//! fallback_iterator.step();
//! assert_eq!(fallback_iterator.get(), &DataLocale::from(locale!("en")));
//! fallback_iterator.step();
//! assert_eq!(fallback_iterator.get(), &DataLocale::from(locale!("und")));
//! ```

use icu_locid::extensions::unicode::{Key, Value};
use icu_locid::subtags::Variants;
use icu_provider::prelude::*;
use icu_provider::FallbackPriority;
use icu_provider::FallbackSupplement;

mod adapter;
mod algorithms;
pub mod provider;

pub use adapter::LocaleFallbackProvider;

use provider::*;

/// Configuration settings for a particular fallback operation.
#[derive(Debug, Clone, PartialEq, Eq, Default)]
#[non_exhaustive]
pub struct LocaleFallbackConfig {
    /// Strategy for choosing which subtags to drop during locale fallback.
    ///
    /// # Examples
    ///
    /// Retain the language and script subtags until the final step:
    ///
    /// ```
    /// use icu_locid::locale;
    /// use icu_provider::prelude::*;
    /// use icu_provider::FallbackPriority;
    /// use icu_provider_adapters::fallback::LocaleFallbackConfig;
    /// use icu_provider_adapters::fallback::LocaleFallbacker;
    ///
    /// // Set up the fallback iterator.
    /// let fallbacker =
    ///     LocaleFallbacker::try_new_unstable(&icu_testdata::unstable())
    ///         .expect("data");
    /// let mut config = LocaleFallbackConfig::default();
    /// config.priority = FallbackPriority::Language;
    /// let key_fallbacker = fallbacker.for_config(config);
    /// let mut fallback_iterator = key_fallbacker
    ///     .fallback_for(DataLocale::from(locale!("ca-ES-valencia")));
    ///
    /// // Run the algorithm and check the results.
    /// assert_eq!(
    ///     fallback_iterator.get(),
    ///     &DataLocale::from(locale!("ca-ES-valencia"))
    /// );
    /// fallback_iterator.step();
    /// assert_eq!(fallback_iterator.get(), &DataLocale::from(locale!("ca-ES")));
    /// fallback_iterator.step();
    /// assert_eq!(fallback_iterator.get(), &DataLocale::from(locale!("ca")));
    /// fallback_iterator.step();
    /// assert_eq!(fallback_iterator.get(), &DataLocale::from(locale!("und")));
    /// ```
    ///
    /// Retain the region subtag until the final step:
    ///
    /// ```
    /// use icu_locid::locale;
    /// use icu_provider::prelude::*;
    /// use icu_provider::FallbackPriority;
    /// use icu_provider_adapters::fallback::LocaleFallbackConfig;
    /// use icu_provider_adapters::fallback::LocaleFallbacker;
    ///
    /// // Set up the fallback iterator.
    /// let fallbacker =
    ///     LocaleFallbacker::try_new_unstable(&icu_testdata::unstable())
    ///         .expect("data");
    /// let mut config = LocaleFallbackConfig::default();
    /// config.priority = FallbackPriority::Region;
    /// let key_fallbacker = fallbacker.for_config(config);
    /// let mut fallback_iterator = key_fallbacker
    ///     .fallback_for(DataLocale::from(locale!("ca-ES-valencia")));
    ///
    /// // Run the algorithm and check the results.
    /// assert_eq!(
    ///     fallback_iterator.get(),
    ///     &DataLocale::from(locale!("ca-ES-valencia"))
    /// );
    /// fallback_iterator.step();
    /// assert_eq!(fallback_iterator.get(), &DataLocale::from(locale!("ca-ES")));
    /// fallback_iterator.step();
    /// assert_eq!(
    ///     fallback_iterator.get(),
    ///     &DataLocale::from(locale!("und-ES-valencia"))
    /// );
    /// fallback_iterator.step();
    /// assert_eq!(
    ///     fallback_iterator.get(),
    ///     &DataLocale::from(locale!("und-ES"))
    /// );
    /// fallback_iterator.step();
    /// assert_eq!(fallback_iterator.get(), &DataLocale::from(locale!("und")));
    /// ```
    pub priority: FallbackPriority,
    /// An extension keyword to retain during locale fallback.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_locid::locale;
    /// use icu_provider::prelude::*;
    /// use icu_provider_adapters::fallback::LocaleFallbackConfig;
    /// use icu_provider_adapters::fallback::LocaleFallbacker;
    ///
    /// // Set up the fallback iterator.
    /// let fallbacker =
    ///     LocaleFallbacker::try_new_unstable(&icu_testdata::unstable())
    ///         .expect("data");
    /// let mut config = LocaleFallbackConfig::default();
    /// config.extension_key = Some(icu_locid::extensions_unicode_key!("nu"));
    /// let key_fallbacker = fallbacker.for_config(config);
    /// let mut fallback_iterator = key_fallbacker
    ///     .fallback_for(DataLocale::from(locale!("ar-EG-u-nu-latn")));
    ///
    /// // Run the algorithm and check the results.
    /// assert_eq!(
    ///     fallback_iterator.get(),
    ///     &DataLocale::from(locale!("ar-EG-u-nu-latn"))
    /// );
    /// fallback_iterator.step();
    /// assert_eq!(fallback_iterator.get(), &DataLocale::from(locale!("ar-EG")));
    /// fallback_iterator.step();
    /// assert_eq!(fallback_iterator.get(), &DataLocale::from(locale!("ar")));
    /// fallback_iterator.step();
    /// assert_eq!(fallback_iterator.get(), &DataLocale::from(locale!("und")));
    /// ```
    pub extension_key: Option<Key>,
    /// Fallback supplement data key to customize fallback rules.
    ///
    /// For example, most data keys for collation add additional parent locales, such as
    /// "yue" to "zh-Hant", and data used for the `"-u-co"` extension keyword fallback.
    ///
    /// Currently the only supported fallback supplement is `FallbackSupplement::Collation`, but more may be
    /// added in the future.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_locid::locale;
    /// use icu_provider::prelude::*;
    /// use icu_provider::FallbackPriority;
    /// use icu_provider::FallbackSupplement;
    /// use icu_provider_adapters::fallback::LocaleFallbackConfig;
    /// use icu_provider_adapters::fallback::LocaleFallbacker;
    /// use tinystr::tinystr;
    ///
    /// // Set up the fallback iterator.
    /// let fallbacker =
    ///     LocaleFallbacker::try_new_unstable(&icu_testdata::unstable())
    ///         .expect("data");
    /// let mut config = LocaleFallbackConfig::default();
    /// config.priority = FallbackPriority::Collation;
    /// config.fallback_supplement = Some(FallbackSupplement::Collation);
    /// let key_fallbacker = fallbacker.for_config(config);
    /// let mut fallback_iterator =
    ///     key_fallbacker.fallback_for(DataLocale::from(locale!("yue-HK")));
    ///
    /// // Run the algorithm and check the results.
    /// // TODO(#1964): add "zh" as a target.
    /// assert_eq!(
    ///     fallback_iterator.get(),
    ///     &DataLocale::from(locale!("yue-HK"))
    /// );
    /// fallback_iterator.step();
    /// assert_eq!(fallback_iterator.get(), &DataLocale::from(locale!("yue")));
    /// fallback_iterator.step();
    /// assert_eq!(
    ///     fallback_iterator.get(),
    ///     &DataLocale::from(locale!("zh-Hant"))
    /// );
    /// fallback_iterator.step();
    /// assert_eq!(fallback_iterator.get(), &DataLocale::from(locale!("und")));
    /// ```
    pub fallback_supplement: Option<FallbackSupplement>,
}

/// Entry type for locale fallbacking.
///
/// See the module-level documentation for an example.
#[derive(Debug, Clone, PartialEq)]
pub struct LocaleFallbacker {
    likely_subtags: DataPayload<LocaleFallbackLikelySubtagsV1Marker>,
    parents: DataPayload<LocaleFallbackParentsV1Marker>,
    collation_supplement: Option<DataPayload<CollationFallbackSupplementV1Marker>>,
}

/// Intermediate type for spawning locale fallback iterators based on a specific configuration.
///
/// See the module-level documentation for an example.
#[derive(Debug, Clone, PartialEq)]
pub struct LocaleFallbackerWithConfig<'a> {
    likely_subtags: &'a LocaleFallbackLikelySubtagsV1<'a>,
    parents: &'a LocaleFallbackParentsV1<'a>,
    supplement: Option<&'a LocaleFallbackSupplementV1<'a>>,
    config: LocaleFallbackConfig,
}

/// Inner iteration type. Does not own the item under fallback.
#[derive(Debug)]
struct LocaleFallbackIteratorInner<'a, 'b> {
    likely_subtags: &'a LocaleFallbackLikelySubtagsV1<'a>,
    parents: &'a LocaleFallbackParentsV1<'a>,
    supplement: Option<&'a LocaleFallbackSupplementV1<'a>>,
    config: &'b LocaleFallbackConfig,
    backup_extension: Option<Value>,
    backup_subdivision: Option<Value>,
    backup_variants: Option<Variants>,
}

/// Iteration type for locale fallback operations.
///
/// Because the `Iterator` trait does not allow items to borrow from the iterator, this class does
/// not implement that trait. Instead, use `.step()` and `.get()`.
#[derive(Debug)]
pub struct LocaleFallbackIterator<'a, 'b> {
    current: DataLocale,
    inner: LocaleFallbackIteratorInner<'a, 'b>,
}

impl LocaleFallbacker {
    /// Creates a [`LocaleFallbacker`] with fallback data (likely subtags and parent locales).
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    /// <div class="stab unstable">
    /// ⚠️ The bounds on this function may change over time, including in SemVer minor releases.
    /// </div>
    pub fn try_new_unstable<P>(provider: &P) -> Result<Self, DataError>
    where
        P: DataProvider<LocaleFallbackLikelySubtagsV1Marker>
            + DataProvider<LocaleFallbackParentsV1Marker>
            + DataProvider<CollationFallbackSupplementV1Marker>
            + ?Sized,
    {
        let likely_subtags = provider.load(Default::default())?.take_payload()?;
        let parents = provider.load(Default::default())?.take_payload()?;
        let collation_supplement = match DataProvider::<CollationFallbackSupplementV1Marker>::load(
            provider,
            Default::default(),
        ) {
            Ok(response) => Some(response.take_payload()?),
            // It is expected that not all keys are present
            Err(DataError {
                kind: DataErrorKind::MissingDataKey,
                ..
            }) => None,
            Err(e) => return Err(e),
        };
        Ok(LocaleFallbacker {
            likely_subtags,
            parents,
            collation_supplement,
        })
    }

    icu_provider::gen_any_buffer_constructors!(locale: skip, options: skip, error: DataError);

    /// Creates a [`LocaleFallbacker`] without fallback data. Using this constructor may result in
    /// surprising behavior, especially in multi-script languages.
    pub fn new_without_data() -> Self {
        LocaleFallbacker {
            likely_subtags: DataPayload::from_owned(Default::default()),
            parents: DataPayload::from_owned(Default::default()),
            collation_supplement: None,
        }
    }

    /// Creates the intermediate [`LocaleFallbackerWithConfig`] with configuration options.
    pub fn for_config(&self, config: LocaleFallbackConfig) -> LocaleFallbackerWithConfig {
        let supplement = match config.fallback_supplement {
            Some(FallbackSupplement::Collation) => {
                self.collation_supplement.as_ref().map(|p| p.get())
            }
            _ => None,
        };
        LocaleFallbackerWithConfig {
            likely_subtags: self.likely_subtags.get(),
            parents: self.parents.get(),
            supplement,
            config,
        }
    }

    /// Creates the intermediate [`LocaleFallbackerWithConfig`] based on a
    /// [`DataKey`] and a [`DataRequestMetadata`].
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_locid::locale;
    /// use icu_provider::prelude::*;
    /// use icu_provider_adapters::fallback::LocaleFallbacker;
    /// use std::borrow::Cow;
    ///
    /// // Define the data struct with key.
    /// #[icu_provider::data_struct(marker(
    ///     FooV1Marker,
    ///     "demo/foo@1",
    ///     fallback_by = "region"
    /// ))]
    /// pub struct FooV1<'data> {
    ///     message: Cow<'data, str>,
    /// };
    ///
    /// // Set up the fallback iterator.
    /// let fallbacker =
    ///     LocaleFallbacker::try_new_unstable(&icu_testdata::unstable())
    ///         .expect("data");
    /// let key_fallbacker = fallbacker.for_key(FooV1Marker::KEY);
    /// let mut fallback_iterator =
    ///     key_fallbacker.fallback_for(DataLocale::from(locale!("en-GB")));
    ///
    /// // Run the algorithm and check the results.
    /// assert_eq!(fallback_iterator.get(), &DataLocale::from(locale!("en-GB")));
    /// fallback_iterator.step();
    /// assert_eq!(
    ///     fallback_iterator.get(),
    ///     &DataLocale::from(locale!("und-GB"))
    /// );
    /// fallback_iterator.step();
    /// assert_eq!(fallback_iterator.get(), &DataLocale::from(locale!("und")));
    /// ```
    ///
    /// [`DataRequestMetadata`]: icu_provider::DataRequestMetadata
    pub fn for_key(&self, data_key: DataKey) -> LocaleFallbackerWithConfig {
        let priority = data_key.metadata().fallback_priority;
        let extension_key = data_key.metadata().extension_key;
        let fallback_supplement = data_key.metadata().fallback_supplement;
        self.for_config(LocaleFallbackConfig {
            priority,
            extension_key,
            fallback_supplement,
        })
    }
}

impl<'a> LocaleFallbackerWithConfig<'a> {
    /// Creates an iterator based on a [`DataLocale`] (which can be created from [`Locale`]).
    ///
    /// When first initialized, the locale is normalized according to the fallback algorithm.
    ///
    /// [`Locale`]: icu_locid::Locale
    pub fn fallback_for<'b>(&'b self, mut locale: DataLocale) -> LocaleFallbackIterator<'a, 'b> {
        self.normalize(&mut locale);
        LocaleFallbackIterator {
            current: locale,
            inner: LocaleFallbackIteratorInner {
                likely_subtags: self.likely_subtags,
                parents: self.parents,
                supplement: self.supplement,
                config: &self.config,
                backup_extension: None,
                backup_subdivision: None,
                backup_variants: None,
            },
        }
    }
}

impl LocaleFallbackIterator<'_, '_> {
    /// Borrows the current [`DataLocale`] under fallback.
    pub fn get(&self) -> &DataLocale {
        &self.current
    }

    /// Takes the current [`DataLocale`] under fallback.
    pub fn take(self) -> DataLocale {
        self.current
    }

    /// Performs one step of the locale fallback algorithm.
    ///
    /// The fallback is completed once the inner [`DataLocale`] becomes `und`.
    pub fn step(&mut self) -> &mut Self {
        self.inner.step(&mut self.current);
        self
    }
}
