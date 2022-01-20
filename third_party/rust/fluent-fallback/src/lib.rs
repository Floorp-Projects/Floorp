//! Fluent is a modern localization system designed to improve how software is translated.
//!
//! `fluent-fallback` is a high-level component of the [Fluent Localization
//! System](https://www.projectfluent.org).
//!
//! The crate builds on top of the mid-level [`fluent-bundle`](../fluent-bundle) package, and provides an ergonomic API for highly flexible localization.
//!
//! The functionality of this level is complete, but the API itself is in the
//! early stages and the goal of being ergonomic is yet to be achieved.
//!
//! If the user is willing to work through the challenge of setting up the
//! boiler-plate that will eventually go away, `fluent-fallback` provides
//! a powerful abstraction around [`FluentBundle`](fluent_bundle::FluentBundle) coupled
//! with a localization resource management system.
//!
//! The main struct, [`Localization`], is a long-lived, reactive, multi-lingual
//! struct which allows for strong error recovery and locale
//! fallbacking, exposing synchronous and asynchronous ergonomic methods
//! for [`L10nMessage`](types::L10nMessage) retrieval.
//!
//! [`Localization`] is also an API that is to be used when designing bindings
//! to user interface systems, such as DOM, React, and others.
//!
//! # Example
//!
//! ```
//! use fluent_fallback::{Localization, types::{ResourceType, ToResourceId}};
//! use fluent_resmgr::ResourceManager;
//! use unic_langid::langid;
//!
//! let res_mgr = ResourceManager::new("./tests/resources/{locale}/".to_string());
//!
//! let loc = Localization::with_env(
//!     vec![
//!         "test.ftl".into(),
//!         "test2.ftl".to_resource_id(ResourceType::Optional),
//!     ],
//!     true,
//!     vec![langid!("en-US")],
//!     res_mgr,
//! );
//! let bundles = loc.bundles();
//!
//! let mut errors = vec![];
//! let value = bundles.format_value_sync("hello-world", None, &mut errors)
//!     .expect("Failed to format a value");
//!
//! assert_eq!(value, Some("Hello World [en]".into()));
//! ```
//!
//! The above example is far from the ergonomical API style the Fluent project
//! is aiming for, but it represents the full scope of functionality intended
//! for the model.
//!
//! # Resource Management
//!
//! Resource management is one of the most complicated parts of a localization system.
//! In particular, modern software may have needs for both synchronous
//! and asynchronous I/O. That, in turn has a large impact on what can happen
//! in case of missing resources, or errors.
//!
//! Resource identifiers can refer to resources that are either required or optional.
//! In the above example, `"test.ftl"` is a required resource (the default using `.into()`),
//! and `"test2.ftl" is an optional resource, which you can create via the
//! [`ToResourceId`](fluent_fallback::types::ToResourceId) trait.
//!
//! A required resource must be present in order for the a bundle to be considered valid.
//! If a required resource is missing for a given locale, a bundle will not be generated for that locale.
//!
//! A bundle is still considered valid if an optional resource is missing. A bundle will still be generated
//! and the entries for the missing optional resource will simply be missing from the bundle. This should be
//! used sparingly in exceptional cases where you do not want `Localization` to fall back to the next
//! locale if there is a missing resource. Marking all resources as optional will increase the state space
//! that the solver has to search through, and will have a negative impact on performance.
//!
//! Currently, [`Localization`] can be specialized over an implementation of
//! [`generator::BundleGenerator`] trait which provides a method to generate an
//! [`Iterator`] and [`Stream`](futures::stream::Stream).
//!
//! This is not very elegant and will likely be improved in the future, but for the time being, if
//! the customer doesn't need one of the modes, the unnecessary method should use the
//! `unimplemented!()` macro as its body.
//!
//! `fluent-resmgr` provides a simple resource manager which handles synchronous I/O
//! and uses local file system to store resources in a directory structure.
//!
//! That model is often sufficient and the user can either use `fluent-resmgr` or write
//! a similar API to provide the generator for [`Localization`].
//!
//! Alternatively, a much more sophisticated resource manager can be used. Mozilla
//! for its needs in Firefox uses [`L10nRegistry`](https://github.com/zbraniecki/l10nregistry-rs)
//! library which implements [`BundleGenerator`](generator::BundleGenerator).
//!
//! # Locale Management
//!
//! As a long lived structure, the [`Localization`] is intended to handle runtime locale
//! management.
//!
//! In the example above, [`Vec<LagnuageIdentifier>`](unic_langid::LanguageIdentifier)
//! provides a static list of locales that the [`Localization`] handles, but that's just the
//! simplest implementation of the [`env::LocalesProvider`], and one can implement
//! a much more sophisticated one that reacts to user or environment driven changes, and
//! called [`Localization::on_change`] to trigger a new locales to be used for the
//! next translation request.
//!
//! See [`env::LocalesProvider`] trait for an example of a reactive system implementation.
mod bundles;
mod cache;
pub mod env;
mod errors;
pub mod generator;
mod localization;
mod pin_cell;
pub mod types;

pub use bundles::Bundles;
pub use errors::LocalizationError;
pub use localization::Localization;
