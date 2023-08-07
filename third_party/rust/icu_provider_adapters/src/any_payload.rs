// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data provider always serving the same struct.

use icu_provider::prelude::*;
use yoke::trait_hack::YokeTraitHack;
use yoke::Yokeable;
use zerofrom::ZeroFrom;

/// A data provider that returns clones of a fixed type-erased payload.
///
/// [`AnyPayloadProvider`] implements [`AnyProvider`], so it can be used in
/// `*_with_any_provider` constructors across ICU4X.
///
/// # Examples
///
/// ```
/// use icu_provider::hello_world::*;
/// use icu_provider::prelude::*;
/// use icu_provider_adapters::any_payload::AnyPayloadProvider;
/// use std::borrow::Cow;
/// use writeable::assert_writeable_eq;
///
/// let provider =
///     AnyPayloadProvider::from_static::<HelloWorldV1Marker>(&HelloWorldV1 {
///         message: Cow::Borrowed("custom hello world"),
///     });
///
/// // Check that it works:
/// let formatter = HelloWorldFormatter::try_new_with_any_provider(
///     &provider,
///     &icu_locid::Locale::UND.into(),
/// )
/// .expect("key matches");
/// assert_writeable_eq!(formatter.format(), "custom hello world");
///
/// // Requests for invalid keys get MissingDataKey
/// assert!(matches!(
///     provider.load_any(icu_provider::data_key!("foo@1"), Default::default()),
///     Err(DataError {
///         kind: DataErrorKind::MissingDataKey,
///         ..
///     })
/// ))
/// ```
#[derive(Debug)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct AnyPayloadProvider {
    /// The [`DataKey`] for which to provide data. All others will receive a
    /// [`DataErrorKind::MissingDataKey`].
    key: DataKey,
    /// The [`AnyPayload`] to return on matching requests.
    data: AnyPayload,
}

impl AnyPayloadProvider {
    /// Creates an `AnyPayloadProvider` with an owned (allocated) payload of the given data.
    pub fn from_owned<M: KeyedDataMarker>(data: M::Yokeable) -> Self
    where
        M::Yokeable: icu_provider::MaybeSendSync,
    {
        Self::from_payload::<M>(DataPayload::from_owned(data))
    }

    /// Creates an `AnyPayloadProvider` with a statically borrowed payload of the given data.
    pub fn from_static<M: KeyedDataMarker>(data: &'static M::Yokeable) -> Self {
        AnyPayloadProvider {
            key: M::KEY,
            data: AnyPayload::from_static_ref(data),
        }
    }

    /// Creates an `AnyPayloadProvider` from an existing [`DataPayload`].
    pub fn from_payload<M: KeyedDataMarker>(payload: DataPayload<M>) -> Self
    where
        M::Yokeable: icu_provider::MaybeSendSync,
    {
        AnyPayloadProvider {
            key: M::KEY,
            data: payload.wrap_into_any_payload(),
        }
    }

    /// Creates an `AnyPayloadProvider` from an existing [`AnyPayload`].
    pub fn from_any_payload<M: KeyedDataMarker>(payload: AnyPayload) -> Self {
        AnyPayloadProvider {
            key: M::KEY,
            data: payload,
        }
    }

    /// Creates an `AnyPayloadProvider` with the default (allocated) version of the data struct.
    pub fn new_default<M: KeyedDataMarker>() -> Self
    where
        M::Yokeable: Default,
        M::Yokeable: icu_provider::MaybeSendSync,
    {
        Self::from_owned::<M>(M::Yokeable::default())
    }
}

impl AnyProvider for AnyPayloadProvider {
    fn load_any(&self, key: DataKey, _: DataRequest) -> Result<AnyResponse, DataError> {
        key.match_key(self.key)?;
        Ok(AnyResponse {
            metadata: DataResponseMetadata::default(),
            payload: Some(self.data.clone()),
        })
    }
}

impl<M> DataProvider<M> for AnyPayloadProvider
where
    M: KeyedDataMarker,
    for<'a> YokeTraitHack<<M::Yokeable as Yokeable<'a>>::Output>: Clone,
    M::Yokeable: ZeroFrom<'static, M::Yokeable>,
    M::Yokeable: icu_provider::MaybeSendSync,
{
    fn load(&self, req: DataRequest) -> Result<DataResponse<M>, DataError> {
        self.as_downcasting().load(req)
    }
}
