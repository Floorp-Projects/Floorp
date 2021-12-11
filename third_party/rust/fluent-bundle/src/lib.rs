//! Fluent is a modern localization system designed to improve how software is translated.
//!
//! `fluent-bundle` is a mid-level component of the [Fluent Localization
//! System](https://www.projectfluent.org).
//!
//! The crate builds on top of the low level [`fluent-syntax`](../fluent-syntax) package, and provides
//! foundational types and structures required for executing localization at runtime.
//!
//! There are four core concepts to understand Fluent runtime:
//!
//! * [`FluentMessage`] - A single translation unit
//! * [`FluentResource`] - A list of [`FluentMessage`] units
//! * [`FluentBundle`](crate::bundle::FluentBundle) - A collection of [`FluentResource`] lists
//! * [`FluentArgs`] - A list of elements used to resolve a [`FluentMessage`] value
//!
//! ## Example
//!
//! ```
//! use fluent_bundle::{FluentBundle, FluentValue, FluentResource, FluentArgs};
//! // Used to provide a locale for the bundle.
//! use unic_langid::langid;
//!
//! // 1. Crate a FluentResource
//!
//! let ftl_string = r#"
//!
//! hello-world = Hello, world!
//! intro = Welcome, { $name }.
//!
//! "#.to_string();
//!
//! let res = FluentResource::try_new(ftl_string)
//!     .expect("Failed to parse an FTL string.");
//!
//!
//! // 2. Crate a FluentBundle
//!
//! let langid_en = langid!("en-US");
//! let mut bundle = FluentBundle::new(vec![langid_en]);
//!
//!
//! // 3. Add the resource to the bundle
//!
//! bundle
//!     .add_resource(res)
//!     .expect("Failed to add FTL resources to the bundle.");
//!
//!
//! // 4. Retrieve a FluentMessage from the bundle
//!
//! let msg = bundle.get_message("hello-world")
//!     .expect("Message doesn't exist.");
//!
//!
//! // 5. Format the value of the simple message
//!
//! let mut errors = vec![];
//!
//! let pattern = msg.value()
//!     .expect("Message has no value.");
//!
//! let value = bundle.format_pattern(&pattern, None, &mut errors);
//!
//! assert_eq!(
//!     bundle.format_pattern(&pattern, None, &mut errors),
//!     "Hello, world!"
//! );
//!
//! // 6. Format the value of the message with arguments
//!
//! let mut args = FluentArgs::new();
//! args.set("name", "John");
//!
//! let msg = bundle.get_message("intro")
//!     .expect("Message doesn't exist.");
//!
//! let pattern = msg.value()
//!     .expect("Message has no value.");
//!
//! // The FSI/PDI isolation marks ensure that the direction of
//! // the text from the variable is not affected by the translation.
//! assert_eq!(
//!     bundle.format_pattern(&pattern, Some(&args), &mut errors),
//!     "Welcome, \u{2068}John\u{2069}."
//! );
//! ```
//!
//! # Ergonomics & Higher Level APIs
//!
//! Reading the example, you may notice how verbose it feels.
//! Many core methods are fallible, others accumulate errors, and there
//! are intermediate structures used in operations.
//!
//! This is intentional as it serves as building blocks for variety of different
//! scenarios allowing implementations to handle errors, cache and
//! optimize results.
//!
//! At the moment it is expected that users will use
//! the `fluent-bundle` crate directly, while the ecosystem
//! matures and higher level APIs are being developed.
mod args;
pub mod bundle;
mod concurrent;
mod entry;
mod errors;
#[doc(hidden)]
pub mod memoizer;
mod message;
#[doc(hidden)]
pub mod resolver;
mod resource;
pub mod types;

pub use args::FluentArgs;
/// Specialized [`FluentBundle`](crate::bundle::FluentBundle) over
/// non-concurrent [`IntlLangMemoizer`](intl_memoizer::IntlLangMemoizer).
///
/// This is the basic variant of the [`FluentBundle`](crate::bundle::FluentBundle).
///
/// The concurrent specialization, can be constructed with
/// [`FluentBundle::new_concurrent`](crate::bundle::FluentBundle::new_concurrent).
pub type FluentBundle<R> = bundle::FluentBundle<R, intl_memoizer::IntlLangMemoizer>;
pub use errors::FluentError;
pub use message::{FluentAttribute, FluentMessage};
pub use resource::FluentResource;
#[doc(inline)]
pub use types::FluentValue;
