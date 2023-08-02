// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Helpers for switching between multiple providers.

use icu_provider::prelude::*;

#[cfg(feature = "datagen")]
use icu_provider::datagen;

/// A provider that is one of two types determined at runtime.
///
/// Data provider traits implemented by both `P0` and `P1` are implemented on
/// `EitherProvider<P0, P1>`.
#[allow(clippy::exhaustive_enums)] // this is stable
#[derive(Debug)]
pub enum EitherProvider<P0, P1> {
    /// A value of type `P0`.
    A(P0),
    /// A value of type `P1`.
    B(P1),
}

impl<P0: AnyProvider, P1: AnyProvider> AnyProvider for EitherProvider<P0, P1> {
    #[inline]
    fn load_any(&self, key: DataKey, req: DataRequest) -> Result<AnyResponse, DataError> {
        use EitherProvider::*;
        match self {
            A(p) => p.load_any(key, req),
            B(p) => p.load_any(key, req),
        }
    }
}

impl<P0: BufferProvider, P1: BufferProvider> BufferProvider for EitherProvider<P0, P1> {
    #[inline]
    fn load_buffer(
        &self,
        key: DataKey,
        req: DataRequest,
    ) -> Result<DataResponse<BufferMarker>, DataError> {
        use EitherProvider::*;
        match self {
            A(p) => p.load_buffer(key, req),
            B(p) => p.load_buffer(key, req),
        }
    }
}

impl<M: DataMarker, P0: DynamicDataProvider<M>, P1: DynamicDataProvider<M>> DynamicDataProvider<M>
    for EitherProvider<P0, P1>
{
    #[inline]
    fn load_data(&self, key: DataKey, req: DataRequest) -> Result<DataResponse<M>, DataError> {
        use EitherProvider::*;
        match self {
            A(p) => p.load_data(key, req),
            B(p) => p.load_data(key, req),
        }
    }
}

impl<M: KeyedDataMarker, P0: DataProvider<M>, P1: DataProvider<M>> DataProvider<M>
    for EitherProvider<P0, P1>
{
    #[inline]
    fn load(&self, req: DataRequest) -> Result<DataResponse<M>, DataError> {
        use EitherProvider::*;
        match self {
            A(p) => p.load(req),
            B(p) => p.load(req),
        }
    }
}

#[cfg(feature = "datagen")]
impl<
        M: DataMarker,
        P0: datagen::IterableDynamicDataProvider<M>,
        P1: datagen::IterableDynamicDataProvider<M>,
    > datagen::IterableDynamicDataProvider<M> for EitherProvider<P0, P1>
{
    #[inline]
    fn supported_locales_for_key(
        &self,
        key: DataKey,
    ) -> Result<alloc::vec::Vec<DataLocale>, DataError> {
        use EitherProvider::*;
        match self {
            A(p) => p.supported_locales_for_key(key),
            B(p) => p.supported_locales_for_key(key),
        }
    }
}

#[cfg(feature = "datagen")]
impl<
        M: KeyedDataMarker,
        P0: datagen::IterableDataProvider<M>,
        P1: datagen::IterableDataProvider<M>,
    > datagen::IterableDataProvider<M> for EitherProvider<P0, P1>
{
    #[inline]
    fn supported_locales(&self) -> Result<alloc::vec::Vec<DataLocale>, DataError> {
        use EitherProvider::*;
        match self {
            A(p) => p.supported_locales(),
            B(p) => p.supported_locales(),
        }
    }
}
