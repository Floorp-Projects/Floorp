// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! A data provider wrapper that performs locale fallback.

use crate::helpers::result_is_err_missing_locale;
use icu_locid_transform::provider::*;
use icu_provider::prelude::*;

#[doc(hidden)] // moved
pub use icu_locid_transform::fallback::{
    LocaleFallbackIterator, LocaleFallbacker, LocaleFallbackerWithConfig,
};
#[doc(hidden)] // moved
pub use icu_provider::fallback::LocaleFallbackConfig;

/// A data provider wrapper that performs locale fallback. This enables arbitrary locales to be
/// handled at runtime.
///
/// # Examples
///
/// ```
/// use icu_locid::locale;
/// use icu_provider::prelude::*;
/// use icu_provider::hello_world::*;
/// use icu_provider_adapters::fallback::LocaleFallbackProvider;
///
/// # let provider = icu_provider_blob::BlobDataProvider::try_new_from_static_blob(include_bytes!("../../tests/data/blob.postcard")).unwrap();
/// # let provider = provider.as_deserializing();
///
/// let req = DataRequest {
///     locale: &locale!("ja-JP").into(),
///     metadata: Default::default(),
/// };
///
/// // The provider does not have data for "ja-JP":
/// DataProvider::<HelloWorldV1Marker>::load(&provider, req).expect_err("No fallback");
///
/// // But if we wrap the provider in a fallback provider...
/// let provider = LocaleFallbackProvider::try_new_unstable(provider)
///     .expect("Fallback data present");
///
/// // ...then we can load "ja-JP" based on "ja" data
/// let response =
///   DataProvider::<HelloWorldV1Marker>::load(&provider, req).expect("successful with vertical fallback");
///
/// assert_eq!(
///     response.metadata.locale.unwrap(),
///     locale!("ja").into(),
/// );
/// assert_eq!(
///     response.payload.unwrap().get().message,
///     "こんにちは世界",
/// );
/// ```
#[derive(Clone, Debug)]
pub struct LocaleFallbackProvider<P> {
    inner: P,
    fallbacker: LocaleFallbacker,
}

impl<P> LocaleFallbackProvider<P>
where
    P: DataProvider<LocaleFallbackLikelySubtagsV1Marker>
        + DataProvider<LocaleFallbackParentsV1Marker>
        + DataProvider<CollationFallbackSupplementV1Marker>,
{
    /// Create a [`LocaleFallbackProvider`] by wrapping another data provider and then loading
    /// fallback data from it.
    ///
    /// If the data provider being wrapped does not contain fallback data, use
    /// [`LocaleFallbackProvider::new_with_fallbacker`].
    pub fn try_new_unstable(provider: P) -> Result<Self, DataError> {
        let fallbacker = LocaleFallbacker::try_new_unstable(&provider)?;
        Ok(Self {
            inner: provider,
            fallbacker,
        })
    }
}

impl<P> LocaleFallbackProvider<P>
where
    P: AnyProvider,
{
    /// Create a [`LocaleFallbackProvider`] by wrapping another data provider and then loading
    /// fallback data from it.
    ///
    /// If the data provider being wrapped does not contain fallback data, use
    /// [`LocaleFallbackProvider::new_with_fallbacker`].
    pub fn try_new_with_any_provider(provider: P) -> Result<Self, DataError> {
        let fallbacker = LocaleFallbacker::try_new_with_any_provider(&provider)?;
        Ok(Self {
            inner: provider,
            fallbacker,
        })
    }
}

#[cfg(feature = "serde")]
impl<P> LocaleFallbackProvider<P>
where
    P: BufferProvider,
{
    /// Create a [`LocaleFallbackProvider`] by wrapping another data provider and then loading
    /// fallback data from it.
    ///
    /// If the data provider being wrapped does not contain fallback data, use
    /// [`LocaleFallbackProvider::new_with_fallbacker`].
    pub fn try_new_with_buffer_provider(provider: P) -> Result<Self, DataError> {
        let fallbacker = LocaleFallbacker::try_new_with_buffer_provider(&provider)?;
        Ok(Self {
            inner: provider,
            fallbacker,
        })
    }
}

impl<P> LocaleFallbackProvider<P> {
    /// Wrap a provider with an arbitrary fallback engine.
    ///
    /// This relaxes the requirement that the wrapped provider contains its own fallback data.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_locid::locale;
    /// use icu_locid_transform::LocaleFallbacker;
    /// use icu_provider::hello_world::*;
    /// use icu_provider::prelude::*;
    /// use icu_provider_adapters::fallback::LocaleFallbackProvider;
    ///
    /// let provider = HelloWorldProvider;
    ///
    /// let req = DataRequest {
    ///     locale: &locale!("de-CH").into(),
    ///     metadata: Default::default(),
    /// };
    ///
    /// // There is no "de-CH" data in the `HelloWorldProvider`
    /// DataProvider::<HelloWorldV1Marker>::load(&provider, req)
    ///     .expect_err("No data for de-CH");
    ///
    /// // `HelloWorldProvider` does not contain fallback data,
    /// // but we can construct a fallbacker with `icu_locid_transform`'s
    /// // compiled data.
    /// let provider = LocaleFallbackProvider::new_with_fallbacker(
    ///     provider,
    ///     LocaleFallbacker::new().static_to_owned(),
    /// );
    ///
    /// // Now we can load the "de-CH" data via fallback to "de".
    /// let german_hello_world: DataPayload<HelloWorldV1Marker> = provider
    ///     .load(req)
    ///     .expect("Loading should succeed")
    ///     .take_payload()
    ///     .expect("Data should be present");
    ///
    /// assert_eq!("Hallo Welt", german_hello_world.get().message);
    /// ```
    pub fn new_with_fallbacker(provider: P, fallbacker: LocaleFallbacker) -> Self {
        Self {
            inner: provider,
            fallbacker,
        }
    }

    /// Returns a reference to the inner provider, bypassing fallback.
    pub fn inner(&self) -> &P {
        &self.inner
    }

    /// Returns a mutable reference to the inner provider.
    pub fn inner_mut(&mut self) -> &mut P {
        &mut self.inner
    }

    /// Returns ownership of the inner provider to the caller.
    pub fn into_inner(self) -> P {
        self.inner
    }

    /// Run the fallback algorithm with the data request using the inner data provider.
    /// Internal function; external clients should use one of the trait impls below.
    ///
    /// Function arguments:
    ///
    /// - F1 should perform a data load for a single DataRequest and return the result of it
    /// - F2 should map from the provider-specific response type to DataResponseMetadata
    fn run_fallback<F1, F2, R>(
        &self,
        key: DataKey,
        mut base_req: DataRequest,
        mut f1: F1,
        mut f2: F2,
    ) -> Result<R, DataError>
    where
        F1: FnMut(DataRequest) -> Result<R, DataError>,
        F2: FnMut(&mut R) -> &mut DataResponseMetadata,
    {
        if key.metadata().singleton {
            return f1(base_req);
        }
        let mut fallback_iterator = self
            .fallbacker
            .for_config(key.fallback_config())
            .fallback_for(base_req.locale.clone());
        let base_silent = core::mem::replace(&mut base_req.metadata.silent, true);
        loop {
            let result = f1(DataRequest {
                locale: fallback_iterator.get(),
                metadata: base_req.metadata,
            });
            if !result_is_err_missing_locale(&result) {
                return result
                    .map(|mut res| {
                        f2(&mut res).locale = Some(fallback_iterator.take());
                        res
                    })
                    // Log the original request rather than the fallback request
                    .map_err(|e| {
                        base_req.metadata.silent = base_silent;
                        e.with_req(key, base_req)
                    });
            }
            // If we just checked und, break out of the loop.
            if fallback_iterator.get().is_und() {
                break;
            }
            fallback_iterator.step();
        }
        base_req.metadata.silent = base_silent;
        Err(DataErrorKind::MissingLocale.with_req(key, base_req))
    }
}

impl<P> AnyProvider for LocaleFallbackProvider<P>
where
    P: AnyProvider,
{
    fn load_any(&self, key: DataKey, base_req: DataRequest) -> Result<AnyResponse, DataError> {
        self.run_fallback(
            key,
            base_req,
            |req| self.inner.load_any(key, req),
            |res| &mut res.metadata,
        )
    }
}

impl<P> BufferProvider for LocaleFallbackProvider<P>
where
    P: BufferProvider,
{
    fn load_buffer(
        &self,
        key: DataKey,
        base_req: DataRequest,
    ) -> Result<DataResponse<BufferMarker>, DataError> {
        self.run_fallback(
            key,
            base_req,
            |req| self.inner.load_buffer(key, req),
            |res| &mut res.metadata,
        )
    }
}

impl<P, M> DynamicDataProvider<M> for LocaleFallbackProvider<P>
where
    P: DynamicDataProvider<M>,
    M: DataMarker,
{
    fn load_data(&self, key: DataKey, base_req: DataRequest) -> Result<DataResponse<M>, DataError> {
        self.run_fallback(
            key,
            base_req,
            |req| self.inner.load_data(key, req),
            |res| &mut res.metadata,
        )
    }
}

impl<P, M> DataProvider<M> for LocaleFallbackProvider<P>
where
    P: DataProvider<M>,
    M: KeyedDataMarker,
{
    fn load(&self, base_req: DataRequest) -> Result<DataResponse<M>, DataError> {
        self.run_fallback(
            M::KEY,
            base_req,
            |req| self.inner.load(req),
            |res| &mut res.metadata,
        )
    }
}
