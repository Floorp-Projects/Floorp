// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[allow(unused_imports)] // feature-specific
use alloc::boxed::Box;
use icu_provider::prelude::*;
#[allow(unused_imports)] // feature-specific
use icu_provider::MaybeSendSync;
use icu_provider_adapters::empty::EmptyDataProvider;
#[allow(unused_imports)] // feature-specific
use yoke::{trait_hack::YokeTraitHack, Yokeable};
#[allow(unused_imports)] // feature-specific
use zerofrom::ZeroFrom;

pub enum ICU4XDataProviderInner {
    Destroyed,
    Empty,
    #[cfg(feature = "any_provider")]
    Any(Box<dyn AnyProvider + 'static>),
    #[cfg(feature = "buffer_provider")]
    Buffer(Box<dyn BufferProvider + 'static>),
}

#[diplomat::bridge]
pub mod ffi {
    use super::ICU4XDataProviderInner;
    use crate::errors::ffi::ICU4XError;
    use crate::fallbacker::ffi::ICU4XLocaleFallbacker;
    use alloc::boxed::Box;
    #[allow(unused_imports)] // feature-gated
    use icu_provider_adapters::fallback::LocaleFallbackProvider;
    #[allow(unused_imports)] // feature-gated
    use icu_provider_adapters::fork::predicates::MissingLocalePredicate;

    #[diplomat::opaque]
    /// An ICU4X data provider, capable of loading ICU4X data keys from some source.
    #[diplomat::rust_link(icu_provider, Mod)]
    pub struct ICU4XDataProvider(pub ICU4XDataProviderInner);

    #[cfg(feature = "any_provider")]
    fn convert_any_provider<D: icu_provider::AnyProvider + 'static>(x: D) -> ICU4XDataProvider {
        ICU4XDataProvider(super::ICU4XDataProviderInner::Any(Box::new(x)))
    }

    #[cfg(feature = "buffer_provider")]
    fn convert_buffer_provider<D: icu_provider::BufferProvider + 'static>(
        x: D,
    ) -> ICU4XDataProvider {
        ICU4XDataProvider(super::ICU4XDataProviderInner::Buffer(Box::new(x)))
    }

    impl ICU4XDataProvider {
        /// Constructs an `FsDataProvider` and returns it as an [`ICU4XDataProvider`].
        /// Requires the `provider_fs` Cargo feature.
        /// Not supported in WASM.
        #[diplomat::rust_link(icu_provider_fs::FsDataProvider, Struct)]
        #[cfg(all(
            feature = "provider_fs",
            not(any(target_arch = "wasm32", target_os = "none"))
        ))]
        pub fn create_fs(path: &str) -> Result<Box<ICU4XDataProvider>, ICU4XError> {
            // #2520
            // In the future we can start using OsString APIs to support non-utf8 paths
            core::str::from_utf8(path.as_bytes())
                .map_err(|e| ICU4XError::DataIoError.log_original(&e))?;

            Ok(Box::new(convert_buffer_provider(
                icu_provider_fs::FsDataProvider::try_new(path)?,
            )))
        }

        /// Constructs a testdata provider and returns it as an [`ICU4XDataProvider`].
        /// Requires the `provider_test` and one of `any_provider` or `buffer_provider` Cargo features.
        #[diplomat::rust_link(icu_testdata, Mod)]
        #[cfg(all(
            feature = "provider_test",
            any(feature = "any_provider", feature = "buffer_provider")
        ))]
        pub fn create_test() -> Box<ICU4XDataProvider> {
            #[cfg(feature = "any_provider")]
            return Box::new(convert_any_provider(icu_testdata::any()));

            #[cfg(not(feature = "any_provider"))]
            return Box::new(convert_buffer_provider(icu_testdata::buffer()));
        }

        /// Constructs a `BlobDataProvider` and returns it as an [`ICU4XDataProvider`].
        #[diplomat::rust_link(icu_provider_blob::BlobDataProvider, Struct)]
        #[cfg(feature = "buffer_provider")]
        pub fn create_from_byte_slice(
            blob: &'static [u8],
        ) -> Result<Box<ICU4XDataProvider>, ICU4XError> {
            Ok(Box::new(convert_buffer_provider(
                icu_provider_blob::BlobDataProvider::try_new_from_static_blob(blob)?,
            )))
        }

        /// Constructs an empty [`ICU4XDataProvider`].
        #[diplomat::rust_link(icu_provider_adapters::empty::EmptyDataProvider, Struct)]
        #[diplomat::rust_link(
            icu_provider_adapters::empty::EmptyDataProvider::new,
            FnInStruct,
            hidden
        )]
        pub fn create_empty() -> Box<ICU4XDataProvider> {
            Box::new(ICU4XDataProvider(ICU4XDataProviderInner::Empty))
        }

        /// Creates a provider that tries the current provider and then, if the current provider
        /// doesn't support the data key, another provider `other`.
        ///
        /// This takes ownership of the `other` provider, leaving an empty provider in its place.
        ///
        /// The providers must be the same type (Any or Buffer). This condition is satisfied if
        /// both providers originate from the same constructor, such as `create_from_byte_slice`
        /// or `create_fs`. If the condition is not upheld, a runtime error occurs.
        #[diplomat::rust_link(icu_provider_adapters::fork::ForkByKeyProvider, Typedef)]
        #[diplomat::rust_link(
            icu_provider_adapters::fork::predicates::MissingDataKeyPredicate,
            Struct,
            hidden
        )]
        pub fn fork_by_key(&mut self, other: &mut ICU4XDataProvider) -> Result<(), ICU4XError> {
            #[allow(unused_imports)]
            use ICU4XDataProviderInner::*;
            *self = match (
                core::mem::replace(&mut self.0, Destroyed),
                core::mem::replace(&mut other.0, Destroyed),
            ) {
                (Destroyed, _) | (_, Destroyed) => Err(icu_provider::DataError::custom(
                    "This provider has been destroyed",
                ))?,
                (Empty, Empty) => ICU4XDataProvider(ICU4XDataProviderInner::Empty),
                #[cfg(any(feature = "buffer_provider", feature = "any_provider"))]
                (Empty, b) | (b, Empty) => ICU4XDataProvider(b),
                #[cfg(feature = "any_provider")]
                (Any(a), Any(b)) => {
                    convert_any_provider(icu_provider_adapters::fork::ForkByKeyProvider::new(a, b))
                }
                #[cfg(feature = "buffer_provider")]
                (Buffer(a), Buffer(b)) => convert_buffer_provider(
                    icu_provider_adapters::fork::ForkByKeyProvider::new(a, b),
                ),
                #[cfg(all(feature = "buffer_provider", feature = "any_provider"))]
                (Buffer(_), Any(_)) | (Any(_), Buffer(_)) => {
                    Err(ICU4XError::DataMismatchedAnyBufferError.log_original(
                        "fork_by_key must be passed the same type of provider (Any or Buffer)",
                    ))?
                }
            };
            Ok(())
        }

        /// Same as `fork_by_key` but forks by locale instead of key.
        #[diplomat::rust_link(
            icu_provider_adapters::fork::predicates::MissingLocalePredicate,
            Struct
        )]
        pub fn fork_by_locale(&mut self, other: &mut ICU4XDataProvider) -> Result<(), ICU4XError> {
            #[allow(unused_imports)]
            use ICU4XDataProviderInner::*;
            *self = match (
                core::mem::replace(&mut self.0, Destroyed),
                core::mem::replace(&mut other.0, Destroyed),
            ) {
                (Destroyed, _) | (_, Destroyed) => Err(icu_provider::DataError::custom(
                    "This provider has been destroyed",
                ))?,
                (Empty, Empty) => ICU4XDataProvider(ICU4XDataProviderInner::Empty),
                #[cfg(any(feature = "buffer_provider", feature = "any_provider"))]
                (Empty, b) | (b, Empty) => ICU4XDataProvider(b),
                #[cfg(feature = "any_provider")]
                (Any(a), Any(b)) => convert_any_provider(
                    icu_provider_adapters::fork::ForkByErrorProvider::new_with_predicate(
                        a,
                        b,
                        MissingLocalePredicate,
                    ),
                ),
                #[cfg(feature = "buffer_provider")]
                (Buffer(a), Buffer(b)) => convert_buffer_provider(
                    icu_provider_adapters::fork::ForkByErrorProvider::new_with_predicate(
                        a,
                        b,
                        MissingLocalePredicate,
                    ),
                ),
                #[cfg(all(feature = "buffer_provider", feature = "any_provider"))]
                (Buffer(_), Any(_)) | (Any(_), Buffer(_)) => {
                    Err(ICU4XError::DataMismatchedAnyBufferError.log_original(
                        "fork_by_locale must be passed the same type of provider (Any or Buffer)",
                    ))?
                }
            };
            Ok(())
        }

        /// Enables locale fallbacking for data requests made to this provider.
        ///
        /// Note that the test provider (from `create_test`) already has fallbacking enabled.
        #[diplomat::rust_link(
            icu_provider_adapters::fallback::LocaleFallbackProvider::try_new_unstable,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu_provider_adapters::fallback::LocaleFallbackProvider,
            Struct,
            compact
        )]
        pub fn enable_locale_fallback(&mut self) -> Result<(), ICU4XError> {
            use ICU4XDataProviderInner::*;
            *self = match core::mem::replace(&mut self.0, Destroyed) {
                Destroyed => Err(icu_provider::DataError::custom(
                    "This provider has been destroyed",
                ))?,
                Empty => Err(icu_provider::DataErrorKind::MissingDataKey.into_error())?,
                #[cfg(feature = "any_provider")]
                Any(inner) => {
                    convert_any_provider(LocaleFallbackProvider::try_new_with_any_provider(inner)?)
                }
                #[cfg(feature = "buffer_provider")]
                Buffer(inner) => convert_buffer_provider(
                    LocaleFallbackProvider::try_new_with_buffer_provider(inner)?,
                ),
            };
            Ok(())
        }

        #[diplomat::rust_link(
            icu_provider_adapters::fallback::LocaleFallbackProvider::new_with_fallbacker,
            FnInStruct
        )]
        #[diplomat::rust_link(
            icu_provider_adapters::fallback::LocaleFallbackProvider,
            Struct,
            compact
        )]
        #[allow(unused_variables)] // feature-gated
        pub fn enable_locale_fallback_with(
            &mut self,
            fallbacker: &ICU4XLocaleFallbacker,
        ) -> Result<(), ICU4XError> {
            use ICU4XDataProviderInner::*;
            *self = match core::mem::replace(&mut self.0, Destroyed) {
                Destroyed => Err(icu_provider::DataError::custom(
                    "This provider has been destroyed",
                ))?,
                Empty => Err(icu_provider::DataErrorKind::MissingDataKey.into_error())?,
                #[cfg(feature = "any_provider")]
                Any(inner) => convert_any_provider(LocaleFallbackProvider::new_with_fallbacker(
                    inner,
                    fallbacker.0.clone(),
                )),
                #[cfg(feature = "buffer_provider")]
                Buffer(inner) => convert_buffer_provider(
                    LocaleFallbackProvider::new_with_fallbacker(inner, fallbacker.0.clone()),
                ),
            };
            Ok(())
        }
    }
}

macro_rules! load {
    () => {
        fn load(&self, req: DataRequest) -> Result<DataResponse<M>, DataError> {
            use ICU4XDataProviderInner::*;
            match self {
                Destroyed => Err(icu_provider::DataError::custom(
                    "This provider has been destroyed",
                ))?,
                Empty => EmptyDataProvider::new().load(req),
                #[cfg(feature = "any_provider")]
                Any(any_provider) => any_provider.as_downcasting().load(req),
                #[cfg(feature = "buffer_provider")]
                Buffer(buffer_provider) => buffer_provider.as_deserializing().load(req),
            }
        }
    };
}

#[cfg(not(any(feature = "any_provider", feature = "buffer_provider")))]
impl<M> DataProvider<M> for ICU4XDataProviderInner
where
    M: KeyedDataMarker,
{
    load!();
}

#[cfg(all(feature = "buffer_provider", not(feature = "any_provider")))]
impl<M> DataProvider<M> for ICU4XDataProviderInner
where
    M: KeyedDataMarker,
    // Actual bound:
    //     for<'de> <M::Yokeable as Yokeable<'de>>::Output: Deserialize<'de>,
    // Necessary workaround bound (see `yoke::trait_hack` docs):
    for<'de> YokeTraitHack<<M::Yokeable as Yokeable<'de>>::Output>: serde::Deserialize<'de>,
{
    load!();
}

#[cfg(all(feature = "any_provider", not(feature = "buffer_provider")))]
impl<M> DataProvider<M> for ICU4XDataProviderInner
where
    M: KeyedDataMarker,
    for<'a> YokeTraitHack<<M::Yokeable as Yokeable<'a>>::Output>: Clone,
    M::Yokeable: ZeroFrom<'static, M::Yokeable>,
    M::Yokeable: MaybeSendSync,
{
    load!();
}

#[cfg(all(feature = "buffer_provider", feature = "any_provider"))]
impl<M> DataProvider<M> for ICU4XDataProviderInner
where
    M: KeyedDataMarker,
    for<'a> YokeTraitHack<<M::Yokeable as Yokeable<'a>>::Output>: Clone,
    M::Yokeable: ZeroFrom<'static, M::Yokeable>,
    M::Yokeable: MaybeSendSync,
    // Actual bound:
    //     for<'de> <M::Yokeable as Yokeable<'de>>::Output: Deserialize<'de>,
    // Necessary workaround bound (see `yoke::trait_hack` docs):
    for<'de> YokeTraitHack<<M::Yokeable as Yokeable<'de>>::Output>: serde::Deserialize<'de>,
{
    load!();
}
