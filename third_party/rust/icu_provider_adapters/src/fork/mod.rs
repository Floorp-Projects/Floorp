// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Providers that combine multiple other providers.
//!
//! # Types of Forking Providers
//!
//! ## Key-Based
//!
//! To fork between providers that support different data keys, see:
//!
//! - [`ForkByKeyProvider`]
//! - [`MultiForkByKeyProvider`]
//!
//! ## Locale-Based
//!
//! To fork between providers that support different locales, see:
//!
//! - [`ForkByErrorProvider`]`<`[`MissingLocalePredicate`]`>`
//! - [`MultiForkByErrorProvider`]`<`[`MissingLocalePredicate`]`>`
//!
//! [`MissingLocalePredicate`]: predicates::MissingLocalePredicate
//!
//! # Examples
//!
//! See:
//!
//! - [`ForkByKeyProvider`]
//! - [`MultiForkByKeyProvider`]
//! - [`MissingLocalePredicate`]

use alloc::vec::Vec;

mod by_error;

pub mod predicates;

#[macro_use]
mod macros;

pub use by_error::ForkByErrorProvider;
pub use by_error::MultiForkByErrorProvider;

use predicates::ForkByErrorPredicate;
use predicates::MissingDataKeyPredicate;

/// Create a provider that returns data from one of two child providers based on the key.
///
/// The result of the first provider that supports a particular [`DataKey`] will be returned,
/// even if the request failed for other reasons (such as an unsupported language). Therefore,
/// you should add child providers that support disjoint sets of keys.
///
/// [`ForkByKeyProvider`] does not support forking between [`DataProvider`]s. However, it
/// supports forking between [`AnyProvider`], [`BufferProvider`], and [`DynamicDataProvider`].
///
/// # Examples
///
/// Normal usage:
///
/// ```
/// use icu_locid::locale;
/// use icu_provider::hello_world::*;
/// use icu_provider::prelude::*;
/// use icu_provider_adapters::fork::ForkByKeyProvider;
///
/// struct DummyBufferProvider;
/// impl BufferProvider for DummyBufferProvider {
///     fn load_buffer(
///         &self,
///         key: DataKey,
///         req: DataRequest,
///     ) -> Result<DataResponse<BufferMarker>, DataError> {
///         Err(DataErrorKind::MissingDataKey.with_req(key, req))
///     }
/// }
///
/// let forking_provider = ForkByKeyProvider::new(
///     DummyBufferProvider,
///     HelloWorldProvider.into_json_provider(),
/// );
///
/// let provider = forking_provider.as_deserializing();
///
/// let german_hello_world: DataPayload<HelloWorldV1Marker> = provider
///     .load(DataRequest {
///         locale: &locale!("de").into(),
///         metadata: Default::default(),
///     })
///     .expect("Loading should succeed")
///     .take_payload()
///     .expect("Data should be present");
///
/// assert_eq!("Hallo Welt", german_hello_world.get().message);
/// ```
///
/// Stops at the first provider supporting a key, even if the locale is not supported:
///
/// ```
/// use icu_locid::{subtags::language, locale};
/// use icu_provider::hello_world::*;
/// use icu_provider::prelude::*;
/// use icu_provider_adapters::filter::Filterable;
/// use icu_provider_adapters::fork::ForkByKeyProvider;
///
/// let forking_provider = ForkByKeyProvider::new(
///     HelloWorldProvider
///         .into_json_provider()
///         .filterable("Chinese")
///         .filter_by_langid(|langid| langid.language == language!("zh")),
///     HelloWorldProvider
///         .into_json_provider()
///         .filterable("German")
///         .filter_by_langid(|langid| langid.language == language!("de")),
/// );
///
/// let provider: &dyn DataProvider<HelloWorldV1Marker> =
///     &forking_provider.as_deserializing();
///
/// // Chinese is the first provider, so this succeeds
/// let chinese_hello_world = provider
///     .load(DataRequest {
///         locale: &locale!("zh").into(),
///         metadata: Default::default(),
///     })
///     .expect("Loading should succeed")
///     .take_payload()
///     .expect("Data should be present");
///
/// assert_eq!("你好世界", chinese_hello_world.get().message);
///
/// // German is shadowed by Chinese, so this fails
/// provider
///     .load(DataRequest {
///         locale: &locale!("de").into(),
///         metadata: Default::default(),
///     })
///     .expect_err("Should stop at the first provider, even though the second has data");
/// ```
///
/// [`DataKey`]: icu_provider::DataKey
/// [`DataProvider`]: icu_provider::DataProvider
/// [`AnyProvider`]: icu_provider::AnyProvider
/// [`BufferProvider`]: icu_provider::BufferProvider
/// [`DynamicDataProvider`]: icu_provider::DynamicDataProvider
pub type ForkByKeyProvider<P0, P1> = ForkByErrorProvider<P0, P1, MissingDataKeyPredicate>;

impl<P0, P1> ForkByKeyProvider<P0, P1> {
    /// A provider that returns data from one of two child providers based on the key.
    ///
    /// See [`ForkByKeyProvider`].
    pub fn new(p0: P0, p1: P1) -> Self {
        ForkByErrorProvider::new_with_predicate(p0, p1, MissingDataKeyPredicate)
    }
}

/// A provider that returns data from the first child provider supporting the key.
///
/// The result of the first provider that supports a particular [`DataKey`] will be returned,
/// even if the request failed for other reasons (such as an unsupported language). Therefore,
/// you should add child providers that support disjoint sets of keys.
///
/// [`MultiForkByKeyProvider`] does not support forking between [`DataProvider`]s. However, it
/// supports forking between [`AnyProvider`], [`BufferProvider`], and [`DynamicDataProvider`].
///
/// # Examples
///
/// ```
/// use icu_locid::{subtags::language, locale};
/// use icu_provider::hello_world::*;
/// use icu_provider::prelude::*;
/// use icu_provider_adapters::filter::Filterable;
/// use icu_provider_adapters::fork::MultiForkByKeyProvider;
///
/// let forking_provider = MultiForkByKeyProvider::new(
///     vec![
///         HelloWorldProvider
///             .into_json_provider()
///             .filterable("Chinese")
///             .filter_by_langid(|langid| langid.language == language!("zh")),
///         HelloWorldProvider
///             .into_json_provider()
///             .filterable("German")
///             .filter_by_langid(|langid| langid.language == language!("de")),
///     ],
/// );
///
/// let provider: &dyn DataProvider<HelloWorldV1Marker> =
///     &forking_provider.as_deserializing();
///
/// // Chinese is the first provider, so this succeeds
/// let chinese_hello_world = provider
///     .load(DataRequest {
///         locale: &locale!("zh").into(),
///         metadata: Default::default(),
///     })
///     .expect("Loading should succeed")
///     .take_payload()
///     .expect("Data should be present");
///
/// assert_eq!("你好世界", chinese_hello_world.get().message);
///
/// // German is shadowed by Chinese, so this fails
/// provider
///     .load(DataRequest {
///         locale: &locale!("de").into(),
///         metadata: Default::default(),
///     })
///     .expect_err("Should stop at the first provider, even though the second has data");
/// ```
///
/// [`DataKey`]: icu_provider::DataKey
/// [`DataProvider`]: icu_provider::DataProvider
/// [`AnyProvider`]: icu_provider::AnyProvider
/// [`BufferProvider`]: icu_provider::BufferProvider
/// [`DynamicDataProvider`]: icu_provider::DynamicDataProvider
pub type MultiForkByKeyProvider<P> = MultiForkByErrorProvider<P, MissingDataKeyPredicate>;

impl<P> MultiForkByKeyProvider<P> {
    /// Create a provider that returns data from the first child provider supporting the key.
    ///
    /// See [`MultiForkByKeyProvider`].
    pub fn new(providers: Vec<P>) -> Self {
        MultiForkByErrorProvider::new_with_predicate(providers, MissingDataKeyPredicate)
    }
}
