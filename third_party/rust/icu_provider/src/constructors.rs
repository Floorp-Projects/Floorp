// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! 📚 *This module documents ICU4X constructor signatures.*
//!
//! One of the key differences between ICU4X and its parent projects, ICU4C and ICU4J, is in how
//! it deals with locale data.
//!
//! In ICU4X, the data provider is an *explicit argument* whenever it is required by the library.
//! This enables ICU4X to achieve the following value propositions:
//!
//! 1. Configurable data sources (machine-readable data file, baked into code, JSON, etc).
//! 2. Dynamic data loading at runtime (load data on demand).
//! 3. Reduced overhead and code size (data is resolved locally at each call site).
//! 4. Explicit support for multiple ICU4X instances sharing data.
//!
//! In order to achieve these goals, there are 3 versions of all Rust ICU4X functions that
//! take a data provider argument:
//!
//! 1. `*_unstable`
//! 2. `*_with_any_provider`
//! 3. `*_with_buffer_provider`
//!
//! # Which constructor should I use?
//!
//! ## When to use `*_unstable`
//!
//! Use this constructor if your data provider implements the [`DataProvider`] trait for all
//! data structs in *current and future* ICU4X versions. Examples:
//!
//! 1. `BakedDataProvider` auto-regenerated on new ICU4X versions
//! 2. Anything with a _blanket_ [`DataProvider`] impl
//!
//! Since the exact set of bounds may change at any time, including in minor SemVer releases,
//! it is the client's responsibility to guarantee that the requirement is upheld.
//!
//! ## When to use `*_with_any_provider`
//!
//! Use this constructor if you need to use a provider that implements [`AnyProvider`] but not
//! [`DataProvider`]. Examples:
//!
//! 1. [`AnyPayloadProvider`]
//! 2. [`ForkByKeyProvider`] between two providers implementing [`AnyProvider`]
//! 3. Providers that cache or override certain keys but not others and therefore
//!    can't implement [`DataProvider`]
//!
//! ## When to use `*_with_buffer_provider`
//!
//! Use this constructor if your data originates as byte buffers that need to be deserialized.
//! All such providers should implement [`BufferProvider`]. Examples:
//!
//! 1. [`BlobDataProvider`]
//! 2. [`FsDataProvider`]
//! 3. [`ForkByKeyProvider`] between any of the above
//!
//! Please note that you must enable the `"serde"` Cargo feature on each crate in which you use the
//! `*_with_buffer_provider` constructor.
//!
//! # Data Versioning Policy
//!
//! The `*_with_any_provider` and `*_with_buffer_provider` functions will succeed to compile and
//! run if given a data provider supporting all of the keys required for the object being
//! constructed, either the current or any previous version within the same SemVer major release.
//! For example, if a data file is built to support FooFormatter version 1.1, then FooFormatter
//! version 1.2 will be able to read the same data file. Likewise, backwards-compatible keys can
//! always be included by `icu_datagen` to support older library versions.
//!
//! The `*_unstable` functions are only guaranteed to work on data built for the exact same version
//! of ICU4X. The advantage of the `*_unstable` functions is that they result in the smallest code
//! size and allow for automatic data slicing when `BakedDataProvider` is used. However, the type
//! bounds of this function may change over time, breaking SemVer guarantees. These functions
//! should therefore only be used when you have full control over your data lifecycle at compile
//! time.
//!
//! # Data Providers Over FFI
//!
//! Over FFI, there is only one data provider type: [`ICU4XDataProvider`]. Internally, it is an
//! `enum` between `dyn `[`AnyProvider`] and `dyn `[`BufferProvider`].
//!
//! To control for code size, there are two Cargo features, `any_provider` and `buffer_provider`,
//! that enable the corresponding items in the enum.
//!
//! In Rust ICU4X, a similar buffer/any enum approach was not taken because:
//!
//! 1. Feature-gating the enum branches gets complex across crates.
//! 2. Without feature gating, users need to carry Serde code even if they're not using it,
//!    violating one of the core value propositions of ICU4X.
//! 3. We could reduce the number of constructors from 3 to 2 but not to 1, so the educational
//!    benefit is limited.
//!
//!
//! [`DataProvider`]: crate::DataProvider
//! [`BufferProvider`]: crate::BufferProvider
//! [`AnyProvider`]: crate::AnyProvider
//! [`AnyPayloadProvider`]: ../../icu_provider_adapters/any_payload/struct.AnyPayloadProvider.html
//! [`ForkByKeyProvider`]: ../../icu_provider_adapters/fork/struct.ForkByKeyProvider.html
//! [`BlobDataProvider`]: ../../icu_provider_blob/struct.BlobDataProvider.html
//! [`StaticDataProvider`]: ../../icu_provider_blob/struct.StaticDataProvider.html
//! [`FsDataProvider`]: ../../icu_provider_blob/struct.FsDataProvider.html
//! [`ICU4XDataProvider`]: ../../icu_capi/provider/ffi/struct.ICU4XDataProvider.html
