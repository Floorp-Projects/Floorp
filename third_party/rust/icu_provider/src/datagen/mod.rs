// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains various utilities required to generate ICU4X data files, typically
//! via the `icu_datagen` reference crate. End users should not need to consume anything in
//! this module as a library unless defining new types that integrate with `icu_datagen`.
//!
//! This module can be enabled with the `datagen` Cargo feature on `icu_provider`.

mod data_conversion;
mod iter;
mod payload;
pub use data_conversion::DataConverter;
pub use iter::IterableDataProvider;

#[doc(hidden)] // exposed for make_exportable_provider
pub use iter::IterableDynamicDataProvider;
#[doc(hidden)] // exposed for make_exportable_provider
pub use payload::{ExportBox, ExportMarker};

use crate::prelude::*;

/// An object capable of exporting data payloads in some form.
pub trait DataExporter: Sync {
    /// Save a `payload` corresponding to the given key and locale.
    /// Takes non-mut self as it can be called concurrently.
    fn put_payload(
        &self,
        key: DataKey,
        locale: &DataLocale,
        payload: &DataPayload<ExportMarker>,
    ) -> Result<(), DataError>;

    /// Function called after all keys have been fully dumped.
    /// Takes non-mut self as it can be called concurrently.
    fn flush(&self, _key: DataKey) -> Result<(), DataError> {
        Ok(())
    }

    /// This function has to be called before the object is dropped (after all
    /// keys have been fully dumped). This conceptually takes ownership, so
    /// clients *may not* interact with this object after close has been called.
    fn close(&mut self) -> Result<(), DataError> {
        Ok(())
    }
}

/// A [`DynamicDataProvider`] that can be used for exporting data.
///
/// Use [`make_exportable_provider`](crate::make_exportable_provider) to implement this.
pub trait ExportableProvider: IterableDynamicDataProvider<ExportMarker> + Sync {}
impl<T> ExportableProvider for T where T: IterableDynamicDataProvider<ExportMarker> + Sync {}

/// This macro can be used on a data provider to allow it to be used for data generation.
///
/// Data generation 'compiles' data by using this data provider (which usually translates data from
/// different sources and doesn't have to be efficient) to generate data structs, and then writing
/// them to an efficient format like [`BlobDataProvider`] or [`BakedDataProvider`]. The requirements
/// for `make_exportable_provider` are:
/// * The data struct has to implement [`serde::Serialize`](::serde::Serialize) and [`databake::Bake`]
/// * The provider needs to implement [`IterableDataProvider`] for all specified [`KeyedDataMarker`]s.
///   This allows the generating code to know which [`DataLocale`] to collect.
///
/// [`BlobDataProvider`]: ../../icu_provider_blob/struct.BlobDataProvider.html
/// [`BakedDataProvider`]: ../../icu_datagen/index.html
#[macro_export]
macro_rules! make_exportable_provider {
    ($provider:ty, [ $($struct_m:ident),+, ]) => {
        $crate::impl_dynamic_data_provider!(
            $provider,
            [ $($struct_m),+, ],
            $crate::datagen::ExportMarker
        );
        $crate::impl_dynamic_data_provider!(
            $provider,
            [ $($struct_m),+, ],
            $crate::any::AnyMarker
        );

        impl $crate::datagen::IterableDynamicDataProvider<$crate::datagen::ExportMarker> for $provider {
            fn supported_locales_for_key(&self, key: $crate::DataKey) -> Result<Vec<$crate::DataLocale>, $crate::DataError> {
                #![allow(non_upper_case_globals)]
                // Reusing the struct names as identifiers
                $(
                    const $struct_m: $crate::DataKeyHash = <$struct_m as $crate::KeyedDataMarker>::KEY.hashed();
                )+
                match key.hashed() {
                    $(
                        $struct_m => {
                            $crate::datagen::IterableDataProvider::<$struct_m>::supported_locales(self)
                        }
                    )+,
                    _ => Err($crate::DataErrorKind::MissingDataKey.with_key(key))
                }
            }
        }
    };
}
