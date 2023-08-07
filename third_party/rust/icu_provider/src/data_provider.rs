// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::error::DataError;
use crate::key::DataKey;
use crate::marker::{DataMarker, KeyedDataMarker};
use crate::request::DataRequest;
use crate::response::DataResponse;

/// A data provider that loads data for a specific data type.
///
/// Unlike [`DataProvider`], there may be multiple keys corresponding to the same data type.
/// This is often the case when returning `dyn` trait objects such as [`AnyMarker`].
///
/// [`AnyMarker`]: crate::any::AnyMarker
pub trait DynamicDataProvider<M>
where
    M: DataMarker,
{
    /// Query the provider for data, returning the result.
    ///
    /// Returns [`Ok`] if the request successfully loaded data. If data failed to load, returns an
    /// Error with more information.
    fn load_data(&self, key: DataKey, req: DataRequest) -> Result<DataResponse<M>, DataError>;
}

/// A data provider that loads data for a specific [`DataKey`].
pub trait DataProvider<M>
where
    M: KeyedDataMarker,
{
    /// Query the provider for data, returning the result.
    ///
    /// Returns [`Ok`] if the request successfully loaded data. If data failed to load, returns an
    /// Error with more information.
    fn load(&self, req: DataRequest) -> Result<DataResponse<M>, DataError>;
}

impl<M, P> DynamicDataProvider<M> for alloc::boxed::Box<P>
where
    M: DataMarker,
    P: DynamicDataProvider<M> + ?Sized,
{
    fn load_data(&self, key: DataKey, req: DataRequest) -> Result<DataResponse<M>, DataError> {
        (**self).load_data(key, req)
    }
}

#[cfg(test)]
mod test {

    use super::*;
    use crate::hello_world::*;
    use crate::prelude::*;
    use alloc::borrow::Cow;
    use alloc::string::String;
    use core::fmt::Debug;
    use serde::{Deserialize, Serialize};

    // This tests DataProvider borrow semantics with a dummy data provider based on a
    // JSON string. It also exercises most of the data provider code paths.

    /// Key for HelloAlt, used for testing mismatched types
    const HELLO_ALT_KEY: DataKey = crate::data_key!("core/helloalt@1");

    /// A data struct serialization-compatible with HelloWorldV1 used for testing mismatched types
    #[derive(
        Serialize, Deserialize, Debug, Clone, Default, PartialEq, yoke::Yokeable, zerofrom::ZeroFrom,
    )]
    struct HelloAlt {
        #[zerofrom(clone)]
        message: String,
    }

    /// Marker type for [`HelloAlt`].
    struct HelloAltMarker {}

    impl DataMarker for HelloAltMarker {
        type Yokeable = HelloAlt;
    }

    impl KeyedDataMarker for HelloAltMarker {
        const KEY: DataKey = HELLO_ALT_KEY;
    }

    #[derive(Deserialize, Debug, Clone, Default, PartialEq)]
    struct HelloCombined<'data> {
        #[serde(borrow)]
        pub hello_v1: HelloWorldV1<'data>,
        pub hello_alt: HelloAlt,
    }

    /// A DataProvider that owns its data, returning an Rc-variant DataPayload.
    /// Supports only key::HELLO_WORLD_V1. Uses `impl_dynamic_data_provider!()`.
    #[derive(Debug)]
    struct DataWarehouse {
        hello_v1: HelloWorldV1<'static>,
        hello_alt: HelloAlt,
    }

    impl DataProvider<HelloWorldV1Marker> for DataWarehouse {
        fn load(&self, _: DataRequest) -> Result<DataResponse<HelloWorldV1Marker>, DataError> {
            Ok(DataResponse {
                metadata: DataResponseMetadata::default(),
                payload: Some(DataPayload::from_owned(self.hello_v1.clone())),
            })
        }
    }

    crate::impl_dynamic_data_provider!(DataWarehouse, [HelloWorldV1Marker,], AnyMarker);

    /// A DataProvider that supports both key::HELLO_WORLD_V1 and HELLO_ALT.
    #[derive(Debug)]
    struct DataProvider2 {
        data: DataWarehouse,
    }

    impl From<DataWarehouse> for DataProvider2 {
        fn from(warehouse: DataWarehouse) -> Self {
            DataProvider2 { data: warehouse }
        }
    }

    impl DataProvider<HelloWorldV1Marker> for DataProvider2 {
        fn load(&self, _: DataRequest) -> Result<DataResponse<HelloWorldV1Marker>, DataError> {
            Ok(DataResponse {
                metadata: DataResponseMetadata::default(),
                payload: Some(DataPayload::from_owned(self.data.hello_v1.clone())),
            })
        }
    }

    impl DataProvider<HelloAltMarker> for DataProvider2 {
        fn load(&self, _: DataRequest) -> Result<DataResponse<HelloAltMarker>, DataError> {
            Ok(DataResponse {
                metadata: DataResponseMetadata::default(),
                payload: Some(DataPayload::from_owned(self.data.hello_alt.clone())),
            })
        }
    }

    crate::impl_dynamic_data_provider!(
        DataProvider2,
        [HelloWorldV1Marker, HelloAltMarker,],
        AnyMarker
    );

    const DATA: &str = r#"{
        "hello_v1": {
            "message": "Hello V1"
        },
        "hello_alt": {
            "message": "Hello Alt"
        }
    }"#;

    fn get_warehouse(data: &'static str) -> DataWarehouse {
        let data: HelloCombined = serde_json::from_str(data).expect("Well-formed data");
        DataWarehouse {
            hello_v1: data.hello_v1,
            hello_alt: data.hello_alt,
        }
    }

    fn get_payload_v1<P: DataProvider<HelloWorldV1Marker> + ?Sized>(
        provider: &P,
    ) -> Result<DataPayload<HelloWorldV1Marker>, DataError> {
        provider.load(Default::default())?.take_payload()
    }

    fn get_payload_alt<P: DataProvider<HelloAltMarker> + ?Sized>(
        provider: &P,
    ) -> Result<DataPayload<HelloAltMarker>, DataError> {
        provider.load(Default::default())?.take_payload()
    }

    #[test]
    fn test_warehouse_owned() {
        let warehouse = get_warehouse(DATA);
        let hello_data = get_payload_v1(&warehouse).unwrap();
        assert!(matches!(
            hello_data.get(),
            HelloWorldV1 {
                message: Cow::Borrowed(_),
            }
        ));
    }

    #[test]
    fn test_warehouse_owned_dyn_erased() {
        let warehouse = get_warehouse(DATA);
        let hello_data = get_payload_v1(&warehouse.as_any_provider().as_downcasting()).unwrap();
        assert!(matches!(
            hello_data.get(),
            HelloWorldV1 {
                message: Cow::Borrowed(_),
            }
        ));
    }

    #[test]
    fn test_warehouse_owned_dyn_generic() {
        let warehouse = get_warehouse(DATA);
        let hello_data =
            get_payload_v1(&warehouse as &dyn DataProvider<HelloWorldV1Marker>).unwrap();
        assert!(matches!(
            hello_data.get(),
            HelloWorldV1 {
                message: Cow::Borrowed(_),
            }
        ));
    }

    #[test]
    fn test_warehouse_owned_dyn_erased_alt() {
        let warehouse = get_warehouse(DATA);
        let response = get_payload_alt(&warehouse.as_any_provider().as_downcasting());
        assert!(matches!(
            response,
            Err(DataError {
                kind: DataErrorKind::MissingDataKey,
                ..
            })
        ));
    }

    #[test]
    fn test_provider2() {
        let warehouse = get_warehouse(DATA);
        let provider = DataProvider2::from(warehouse);
        let hello_data = get_payload_v1(&provider).unwrap();
        assert!(matches!(
            hello_data.get(),
            HelloWorldV1 {
                message: Cow::Borrowed(_),
            }
        ));
    }

    #[test]
    fn test_provider2_dyn_erased() {
        let warehouse = get_warehouse(DATA);
        let provider = DataProvider2::from(warehouse);
        let hello_data = get_payload_v1(&provider.as_any_provider().as_downcasting()).unwrap();
        assert!(matches!(
            hello_data.get(),
            HelloWorldV1 {
                message: Cow::Borrowed(_),
            }
        ));
    }

    #[test]
    fn test_provider2_dyn_erased_alt() {
        let warehouse = get_warehouse(DATA);
        let provider = DataProvider2::from(warehouse);
        let hello_data = get_payload_alt(&provider.as_any_provider().as_downcasting()).unwrap();
        assert!(matches!(hello_data.get(), HelloAlt { .. }));
    }

    #[test]
    fn test_provider2_dyn_generic() {
        let warehouse = get_warehouse(DATA);
        let provider = DataProvider2::from(warehouse);
        let hello_data =
            get_payload_v1(&provider as &dyn DataProvider<HelloWorldV1Marker>).unwrap();
        assert!(matches!(
            hello_data.get(),
            HelloWorldV1 {
                message: Cow::Borrowed(_),
            }
        ));
    }

    #[test]
    fn test_provider2_dyn_generic_alt() {
        let warehouse = get_warehouse(DATA);
        let provider = DataProvider2::from(warehouse);
        let hello_data = get_payload_alt(&provider as &dyn DataProvider<HelloAltMarker>).unwrap();
        assert!(matches!(hello_data.get(), HelloAlt { .. }));
    }

    #[test]
    fn test_mismatched_types() {
        let warehouse = get_warehouse(DATA);
        let provider = DataProvider2::from(warehouse);
        // Request is for v2, but type argument is for v1
        let response: Result<DataResponse<HelloWorldV1Marker>, DataError> = AnyProvider::load_any(
            &provider.as_any_provider(),
            HELLO_ALT_KEY,
            Default::default(),
        )
        .unwrap()
        .downcast();
        assert!(matches!(
            response,
            Err(DataError {
                kind: DataErrorKind::MismatchedType(_),
                ..
            })
        ));
    }

    fn check_v1_v2<P>(d: &P)
    where
        P: DataProvider<HelloWorldV1Marker> + DataProvider<HelloAltMarker> + ?Sized,
    {
        let v1: DataPayload<HelloWorldV1Marker> =
            d.load(Default::default()).unwrap().take_payload().unwrap();
        let v2: DataPayload<HelloAltMarker> =
            d.load(Default::default()).unwrap().take_payload().unwrap();
        if v1.get().message == v2.get().message {
            panic!()
        }
    }

    #[test]
    fn test_v1_v2_generic() {
        let warehouse = get_warehouse(DATA);
        let provider = DataProvider2::from(warehouse);
        check_v1_v2(&provider);
    }

    #[test]
    fn test_v1_v2_dyn_erased() {
        let warehouse = get_warehouse(DATA);
        let provider = DataProvider2::from(warehouse);
        check_v1_v2(&provider.as_any_provider().as_downcasting());
    }
}
