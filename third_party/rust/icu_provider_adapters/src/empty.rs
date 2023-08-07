// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Empty data provider implementations.
//!
//! Use [`EmptyDataProvider`] as a stand-in for a provider that always fails.

use icu_provider::prelude::*;

/// A data provider that always returns an error.
///
/// The returned error kind is configurable.
///
/// # Examples
///
/// ```
/// use icu_provider::hello_world::HelloWorldV1Marker;
/// use icu_provider::prelude::*;
/// use icu_provider_adapters::empty::EmptyDataProvider;
///
/// let provider = EmptyDataProvider::new();
///
/// assert!(matches!(
///     provider.load_any(HelloWorldV1Marker::KEY, Default::default()),
///     Err(DataError {
///         kind: DataErrorKind::MissingDataKey,
///         ..
///     })
/// ));
/// ```
#[derive(Debug)]
pub struct EmptyDataProvider {
    error_kind: DataErrorKind,
}

impl Default for EmptyDataProvider {
    fn default() -> Self {
        Self::new()
    }
}

impl EmptyDataProvider {
    /// Creates a data provider that always returns [`DataErrorKind::MissingDataKey`].
    pub fn new() -> Self {
        Self {
            error_kind: DataErrorKind::MissingDataKey,
        }
    }

    /// Creates a data provider that always returns the specified error kind.
    pub fn new_with_error_kind(error_kind: DataErrorKind) -> Self {
        Self { error_kind }
    }
}

impl AnyProvider for EmptyDataProvider {
    fn load_any(&self, key: DataKey, base_req: DataRequest) -> Result<AnyResponse, DataError> {
        Err(self.error_kind.with_req(key, base_req))
    }
}

impl BufferProvider for EmptyDataProvider {
    fn load_buffer(
        &self,
        key: DataKey,
        base_req: DataRequest,
    ) -> Result<DataResponse<BufferMarker>, DataError> {
        Err(self.error_kind.with_req(key, base_req))
    }
}

impl<M> DynamicDataProvider<M> for EmptyDataProvider
where
    M: DataMarker,
{
    fn load_data(&self, key: DataKey, base_req: DataRequest) -> Result<DataResponse<M>, DataError> {
        Err(self.error_kind.with_req(key, base_req))
    }
}

impl<M> DataProvider<M> for EmptyDataProvider
where
    M: KeyedDataMarker,
{
    fn load(&self, base_req: DataRequest) -> Result<DataResponse<M>, DataError> {
        Err(self.error_kind.with_req(M::KEY, base_req))
    }
}

#[cfg(feature = "datagen")]
impl<M> icu_provider::datagen::IterableDataProvider<M> for EmptyDataProvider
where
    M: KeyedDataMarker,
{
    fn supported_locales(&self) -> Result<alloc::vec::Vec<DataLocale>, DataError> {
        Ok(vec![])
    }
}

#[cfg(feature = "datagen")]
impl<M> icu_provider::datagen::IterableDynamicDataProvider<M> for EmptyDataProvider
where
    M: DataMarker,
{
    fn supported_locales_for_key(
        &self,
        _: DataKey,
    ) -> Result<alloc::vec::Vec<DataLocale>, DataError> {
        Ok(vec![])
    }
}
