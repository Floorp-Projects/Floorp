// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Collection of predicate traits and functions for forking providers.

use icu_provider::prelude::*;

/// The predicate trait used by [`ForkByErrorProvider`].
///
/// [`ForkByErrorProvider`]: super::ForkByErrorProvider
pub trait ForkByErrorPredicate {
    /// This function is called when a data request fails and there are additional providers
    /// that could possibly fulfill the request.
    ///
    /// Arguments:
    ///
    /// - `&self` = Reference to the struct implementing the trait (for data capture)
    /// - `key` = The [`DataKey`] associated with the request
    /// - `req` = The [`DataRequest`]. This may be `None` if there is no request, such as
    ///           inside [`IterableDynamicDataProvider`].
    /// - `err` = The error that occurred.
    ///
    /// Return value:
    ///
    /// - `true` to discard the error and attempt the request with the next provider.
    /// - `false` to return the error and not perform any additional requests.
    ///
    /// [`DataKey`]: icu_provider::DataKey
    /// [`DataRequest`]: icu_provider::DataRequest
    /// [`IterableDynamicDataProvider`]: icu_provider::datagen::IterableDynamicDataProvider
    fn test(&self, key: DataKey, req: Option<DataRequest>, err: DataError) -> bool;
}

/// A predicate that allows forking providers to search for a provider that supports a
/// particular data key.
///
/// This is normally used implicitly by [`ForkByKeyProvider`].
///
/// [`ForkByKeyProvider`]: super::ForkByKeyProvider
#[derive(Debug, PartialEq, Eq)]
#[non_exhaustive] // Not intended to be constructed
pub struct MissingDataKeyPredicate;

impl ForkByErrorPredicate for MissingDataKeyPredicate {
    #[inline]
    fn test(&self, _: DataKey, _: Option<DataRequest>, err: DataError) -> bool {
        matches!(
            err,
            DataError {
                kind: DataErrorKind::MissingDataKey,
                ..
            }
        )
    }
}

/// A predicate that allows forking providers to search for a provider that supports a
/// particular locale, based on whether it returns [`DataErrorKind::MissingLocale`].
///
/// # Examples
///
/// Configure a multi-language data provider pointing at two language packs:
///
/// ```
/// use icu_provider_adapters::fork::ForkByErrorProvider;
/// use icu_provider_adapters::fork::predicates::MissingLocalePredicate;
/// use icu_provider_fs::FsDataProvider;
/// use icu_provider::prelude::*;
/// use icu_provider::hello_world::HelloWorldV1Marker;
/// use icu_locid::locale;
///
/// // The `tests` directory contains two separate "language packs" for Hello World data.
/// let base_dir = std::path::PathBuf::from(std::env!("CARGO_MANIFEST_DIR"))
///     .join("tests/data/langtest");
/// let provider_de = FsDataProvider::try_new(base_dir.join("de")).unwrap();
/// let provider_ro = FsDataProvider::try_new(base_dir.join("ro")).unwrap();
///
/// // Create the forking provider:
/// let provider = ForkByErrorProvider::new_with_predicate(
///     provider_de,
///     provider_ro,
///     MissingLocalePredicate
/// );
///
/// // Test that we can load both "de" and "ro" data:
///
/// let german_hello_world: DataPayload<HelloWorldV1Marker> = provider
///     .as_deserializing()
///     .load(DataRequest {
///         locale: &locale!("de").into(),
///         metadata: Default::default(),
///     })
///     .expect("Loading should succeed")
///     .take_payload()
///     .expect("Data should be present");
///
/// assert_eq!("Hallo Welt", german_hello_world.get().message);
///
/// let romanian_hello_world: DataPayload<HelloWorldV1Marker> = provider
///     .as_deserializing()
///     .load(DataRequest {
///         locale: &locale!("ro").into(),
///         metadata: Default::default(),
///     })
///     .expect("Loading should succeed")
///     .take_payload()
///     .expect("Data should be present");
///
/// assert_eq!("Salut, lume", romanian_hello_world.get().message);
///
/// // We should not be able to load "en" data because it is not in the provider:
///
/// DataProvider::<HelloWorldV1Marker>::load(
///     &provider.as_deserializing(),
///     DataRequest {
///         locale: &locale!("en").into(),
///         metadata: Default::default(),
///     }
/// )
/// .expect_err("No English data");
/// ```
#[derive(Debug, PartialEq, Eq)]
#[allow(clippy::exhaustive_structs)] // empty type
pub struct MissingLocalePredicate;

impl ForkByErrorPredicate for MissingLocalePredicate {
    #[inline]
    fn test(&self, _: DataKey, _: Option<DataRequest>, err: DataError) -> bool {
        matches!(
            err,
            DataError {
                kind: DataErrorKind::MissingLocale,
                ..
            }
        )
    }
}
