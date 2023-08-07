// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use crate::provider::ffi::ICU4XDataProvider;
    use alloc::boxed::Box;
    use icu_properties::{names::PropertyValueNameToEnumMapper, provider, GeneralCategoryGroup};
    use icu_provider::prelude::{DataProvider, DataResponse, KeyedDataMarker};

    use crate::errors::ffi::ICU4XError;

    /// A type capable of looking up a property value from a string name.
    #[diplomat::opaque]
    #[diplomat::rust_link(icu::properties::names::PropertyValueNameToEnumMapper, Struct)]
    #[diplomat::rust_link(icu::properties::names::PropertyValueNameToEnumMapperBorrowed, Struct)]
    #[diplomat::rust_link(
        icu::properties::names::PropertyValueNameToEnumMapper::as_borrowed,
        FnInStruct,
        hidden
    )]
    #[diplomat::rust_link(
        icu::properties::names::PropertyValueNameToEnumMapper::from_data,
        FnInStruct,
        hidden
    )]
    pub struct ICU4XPropertyValueNameToEnumMapper(PropertyValueNameToEnumMapper<u16>);

    fn load<
        M: KeyedDataMarker<Yokeable = provider::names::PropertyValueNameToEnumMapV1<'static>>,
    >(
        provider: &ICU4XDataProvider,
    ) -> Result<Box<ICU4XPropertyValueNameToEnumMapper>, ICU4XError> {
        let data = DataProvider::<M>::load(&provider.0, Default::default())
            .and_then(DataResponse::take_payload)?;
        Ok(Box::new(ICU4XPropertyValueNameToEnumMapper(
            PropertyValueNameToEnumMapper::from_data(data),
        )))
    }

    impl ICU4XPropertyValueNameToEnumMapper {
        /// Get the property value matching the given name, using strict matching
        ///
        /// Returns -1 if the name is unknown for this property
        #[diplomat::rust_link(
            icu::properties::names::PropertyValueNameToEnumMapperBorrowed::get_strict,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::properties::names::PropertyValueNameToEnumMapperBorrowed::get_strict_u16,
            FnInStruct,
            hidden
        )]
        pub fn get_strict(&self, name: &str) -> i16 {
            self.0
                .as_borrowed()
                .get_strict(name)
                .map(|x| x as i16)
                .unwrap_or(-1)
        }

        /// Get the property value matching the given name, using loose matching
        ///
        /// Returns -1 if the name is unknown for this property
        #[diplomat::rust_link(
            icu::properties::names::PropertyValueNameToEnumMapperBorrowed::get_loose,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu::properties::names::PropertyValueNameToEnumMapperBorrowed::get_loose_u16,
            FnInStruct,
            hidden
        )]
        pub fn get_loose(&self, name: &str) -> i16 {
            self.0
                .as_borrowed()
                .get_loose(name)
                .map(|x| x as i16)
                .unwrap_or(-1)
        }

        #[diplomat::rust_link(
            icu::properties::GeneralCategory::get_name_to_enum_mapper,
            FnInStruct
        )]
        pub fn load_general_category(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XPropertyValueNameToEnumMapper>, ICU4XError> {
            load::<provider::GeneralCategoryNameToValueV1Marker>(provider)
        }

        #[diplomat::rust_link(icu::properties::BidiClass::get_name_to_enum_mapper, FnInStruct)]
        pub fn load_bidi_class(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XPropertyValueNameToEnumMapper>, ICU4XError> {
            load::<provider::BidiClassNameToValueV1Marker>(provider)
        }

        #[diplomat::rust_link(icu::properties::EastAsianWidth::get_name_to_enum_mapper, FnInStruct)]
        pub fn load_east_asian_width(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XPropertyValueNameToEnumMapper>, ICU4XError> {
            load::<provider::EastAsianWidthNameToValueV1Marker>(provider)
        }

        #[diplomat::rust_link(icu::properties::LineBreak::get_name_to_enum_mapper, FnInStruct)]
        pub fn load_line_break(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XPropertyValueNameToEnumMapper>, ICU4XError> {
            load::<provider::LineBreakNameToValueV1Marker>(provider)
        }

        #[diplomat::rust_link(
            icu::properties::GraphemeClusterBreak::get_name_to_enum_mapper,
            FnInStruct
        )]
        pub fn load_grapheme_cluster_break(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XPropertyValueNameToEnumMapper>, ICU4XError> {
            load::<provider::GraphemeClusterBreakNameToValueV1Marker>(provider)
        }

        #[diplomat::rust_link(icu::properties::WordBreak::get_name_to_enum_mapper, FnInStruct)]
        pub fn load_word_break(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XPropertyValueNameToEnumMapper>, ICU4XError> {
            load::<provider::WordBreakNameToValueV1Marker>(provider)
        }

        #[diplomat::rust_link(icu::properties::SentenceBreak::get_name_to_enum_mapper, FnInStruct)]
        pub fn load_sentence_break(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XPropertyValueNameToEnumMapper>, ICU4XError> {
            load::<provider::SentenceBreakNameToValueV1Marker>(provider)
        }

        #[diplomat::rust_link(icu::properties::Script::get_name_to_enum_mapper, FnInStruct)]
        pub fn load_script(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XPropertyValueNameToEnumMapper>, ICU4XError> {
            load::<provider::ScriptNameToValueV1Marker>(provider)
        }
    }

    /// A type capable of looking up General Category mask values from a string name.
    #[diplomat::opaque]
    #[diplomat::rust_link(
        icu::properties::GeneralCategoryGroup::get_name_to_enum_mapper,
        FnInStruct
    )]
    #[diplomat::rust_link(icu::properties::names::PropertyValueNameToEnumMapper, Struct)]
    #[diplomat::rust_link(
        icu::properties::names::PropertyValueNameToEnumMapper::as_borrowed,
        FnInStruct,
        hidden
    )]
    pub struct ICU4XGeneralCategoryNameToMaskMapper(
        PropertyValueNameToEnumMapper<GeneralCategoryGroup>,
    );

    impl ICU4XGeneralCategoryNameToMaskMapper {
        /// Get the mask value matching the given name, using strict matching
        ///
        /// Returns 0 if the name is unknown for this property
        // #[diplomat::rust_link(icu::properties::maps::PropertyValueNameToEnumMapperBorrowed::get_strict, FnInStruct)]
        // #[diplomat::rust_link(icu::properties::maps::PropertyValueNameToEnumMapperBorrowed::get_strict_u16, FnInStruct, hidden)]
        pub fn get_strict(&self, name: &str) -> u32 {
            self.0
                .as_borrowed()
                .get_strict(name)
                .map(Into::into)
                .unwrap_or(0)
        }

        /// Get the mask value matching the given name, using loose matching
        ///
        /// Returns 0 if the name is unknown for this property
        // #[diplomat::rust_link(icu::properties::maps::PropertyValueNameToEnumMapperBorrowed::get_loose, FnInStruct)]
        // #[diplomat::rust_link(icu::properties::maps::PropertyValueNameToEnumMapperBorrowed::get_loose_u16, FnInStruct, hidden)]
        pub fn get_loose(&self, name: &str) -> u32 {
            self.0
                .as_borrowed()
                .get_loose(name)
                .map(Into::into)
                .unwrap_or(0)
        }

        #[diplomat::rust_link(
            icu::properties::GeneralCategoryGroup::get_name_to_enum_mapper,
            FnInStruct
        )]
        pub fn load(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XGeneralCategoryNameToMaskMapper>, ICU4XError> {
            let data = DataProvider::<provider::GeneralCategoryMaskNameToValueV1Marker>::load(
                &provider.0,
                Default::default(),
            )
            .and_then(DataResponse::take_payload)?;
            Ok(Box::new(ICU4XGeneralCategoryNameToMaskMapper(
                PropertyValueNameToEnumMapper::from_data(data),
            )))
        }
    }
}
