// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::data_struct_impl;
use proc_macro2::TokenStream as TokenStream2;
use quote::quote;
use syn::{DeriveInput, NestedMeta};

fn check(attr: Vec<TokenStream2>, item: TokenStream2, expected: TokenStream2) {
    let actual = data_struct_impl(
        attr.into_iter()
            .map(syn::parse2)
            .collect::<syn::parse::Result<Vec<NestedMeta>>>()
            .unwrap(),
        syn::parse2::<DeriveInput>(item).unwrap(),
    );
    assert_eq!(expected.to_string(), actual.to_string());
}

#[test]
#[rustfmt::skip] // inserts a comma
fn test_basic() {
    // #[data_struct]
    check(
        vec![],
        quote!(
            pub struct FooV1;
        ),
        quote!(
            #[derive(icu_provider::prelude::yoke::Yokeable, icu_provider::prelude::zerofrom::ZeroFrom)]
            pub struct FooV1;
        ),
    );
}

#[test]
fn test_data_marker() {
    // #[data_struct(FooV1Marker)]
    check(
        vec![quote!(FooV1Marker)],
        quote!(
            pub struct FooV1;
        ),
        quote!(
            #[doc = "Marker type for [`FooV1`]"]
            pub struct FooV1Marker;
            impl icu_provider::DataMarker for FooV1Marker {
                type Yokeable = FooV1;
            }
            #[derive(icu_provider::prelude::yoke::Yokeable, icu_provider::prelude::zerofrom::ZeroFrom)]
            pub struct FooV1;
        ),
    );
}

#[test]
fn test_keyed_data_marker() {
    // #[data_struct(BarV1Marker = "demo/bar@1")]
    check(
        vec![quote!(BarV1Marker = "demo/bar@1")],
        quote!(
            pub struct FooV1;
        ),
        quote!(
            #[doc = "Marker type for [`FooV1`]: \"demo/bar@1\"\n\n- Fallback priority: language (default)\n- Extension keyword: none (default)"]
            pub struct BarV1Marker;
            impl icu_provider::DataMarker for BarV1Marker {
                type Yokeable = FooV1;
            }
            impl icu_provider::KeyedDataMarker for BarV1Marker {
                const KEY: icu_provider::DataKey = icu_provider::data_key!(
                    "demo/bar@1",
                    icu_provider::DataKeyMetadata::construct_internal(
                        icu_provider::FallbackPriority::const_default(),
                        None,
                        None
                    ));
            }
            #[derive(icu_provider::prelude::yoke::Yokeable, icu_provider::prelude::zerofrom::ZeroFrom)]
            pub struct FooV1;
        ),
    );
}

#[test]
fn test_multi_named_keyed_data_marker() {
    // #[data_struct(FooV1Marker, BarV1Marker = "demo/bar@1", BazV1Marker = "demo/baz@1")]
    check(
        vec![
            quote!(FooV1Marker),
            quote!(BarV1Marker = "demo/bar@1"),
            quote!(BazV1Marker = "demo/baz@1"),
        ],
        quote!(
            pub struct FooV1<'data>;
        ),
        quote!(
            #[doc = "Marker type for [`FooV1`]"]
            pub struct FooV1Marker;
            impl icu_provider::DataMarker for FooV1Marker {
                type Yokeable = FooV1<'static>;
            }
            #[doc = "Marker type for [`FooV1`]: \"demo/bar@1\"\n\n- Fallback priority: language (default)\n- Extension keyword: none (default)"]
            pub struct BarV1Marker;
            impl icu_provider::DataMarker for BarV1Marker {
                type Yokeable = FooV1<'static>;
            }
            impl icu_provider::KeyedDataMarker for BarV1Marker {
                const KEY: icu_provider::DataKey = icu_provider::data_key!(
                    "demo/bar@1",
                    icu_provider::DataKeyMetadata::construct_internal(
                        icu_provider::FallbackPriority::const_default(),
                        None,
                        None
                    ));
            }
            #[doc = "Marker type for [`FooV1`]: \"demo/baz@1\"\n\n- Fallback priority: language (default)\n- Extension keyword: none (default)"]
            pub struct BazV1Marker;
            impl icu_provider::DataMarker for BazV1Marker {
                type Yokeable = FooV1<'static>;
            }
            impl icu_provider::KeyedDataMarker for BazV1Marker {
                const KEY: icu_provider::DataKey = icu_provider::data_key!(
                    "demo/baz@1",
                    icu_provider::DataKeyMetadata::construct_internal(
                        icu_provider::FallbackPriority::const_default(),
                        None,
                        None
                    ));
            }
            #[derive(icu_provider::prelude::yoke::Yokeable, icu_provider::prelude::zerofrom::ZeroFrom)]
            pub struct FooV1<'data>;
        ),
    );
}

#[test]
fn test_databake() {
    check(
        vec![quote!(BarV1Marker = "demo/bar@1")],
        quote!(
            #[databake(path = test::path)]
            pub struct FooV1;
        ),
        quote!(
            #[doc = "Marker type for [`FooV1`]: \"demo/bar@1\"\n\n- Fallback priority: language (default)\n- Extension keyword: none (default)"]
            #[derive(databake::Bake)]
            #[databake(path = test::path)]
            pub struct BarV1Marker;
            impl icu_provider::DataMarker for BarV1Marker {
                type Yokeable = FooV1;
            }
            impl icu_provider::KeyedDataMarker for BarV1Marker {
                const KEY: icu_provider::DataKey = icu_provider::data_key!(
                    "demo/bar@1",
                    icu_provider::DataKeyMetadata::construct_internal(
                        icu_provider::FallbackPriority::const_default(),
                        None,
                        None
                    ));
            }
            #[derive(icu_provider::prelude::yoke::Yokeable, icu_provider::prelude::zerofrom::ZeroFrom)]
            #[databake(path = test::path)]
            pub struct FooV1;
        ),
    );
}

#[test]
fn test_attributes() {
    // #[data_struct(FooV1Marker, marker(BarV1Marker, "demo/bar@1", fallback_by = "region", extension_kw = "ca"))]
    check(
        vec![
            quote!(FooV1Marker),
            quote!(marker(
                BarV1Marker,
                "demo/bar@1",
                fallback_by = "region",
                extension_key = "ca",
                fallback_supplement = "collation"
            )),
        ],
        quote!(
            pub struct FooV1<'data>;
        ),
        quote!(
            #[doc = "Marker type for [`FooV1`]"]
            pub struct FooV1Marker;
            impl icu_provider::DataMarker for FooV1Marker {
                type Yokeable = FooV1<'static>;
            }
            #[doc = "Marker type for [`FooV1`]: \"demo/bar@1\"\n\n- Fallback priority: region\n- Extension keyword: ca"]
            pub struct BarV1Marker;
            impl icu_provider::DataMarker for BarV1Marker {
                type Yokeable = FooV1<'static>;
            }
            impl icu_provider::KeyedDataMarker for BarV1Marker {
                const KEY: icu_provider::DataKey = icu_provider::data_key!(
                    "demo/bar@1",
                    icu_provider::DataKeyMetadata::construct_internal(
                        icu_provider::FallbackPriority::Region,
                        Some(icu_provider::_internal::extensions_unicode_key!("ca")),
                        Some(icu_provider::FallbackSupplement::Collation)
                    ));
            }
            #[derive(icu_provider::prelude::yoke::Yokeable, icu_provider::prelude::zerofrom::ZeroFrom)]
            pub struct FooV1<'data>;
        ),
    );
}
