//! Fluent is a modern localization system designed to improve how software is translated.
//!
//! The Rust implementation provides the low level components for syntax operations, like parser
//! and AST, and the core localization struct - [`FluentBundle`].
//!
//! [`FluentBundle`] is the low level container for storing and formatting localization messages
//! in a single locale.
//!
//! This crate provides also a number of structures needed for a localization API such as [`FluentResource`],
//! [`FluentMessage`], [`FluentArgs`], and [`FluentValue`].
//!
//! Together, they allow implementations to build higher-level APIs that use [`FluentBundle`]
//! and add user friendly helpers, framework bindings, error fallbacking,
//! language negotiation between user requested languages and available resources,
//! and I/O for loading selected resources.
//!
//! # Example
//!
//! ```
//! use fluent_bundle::{FluentBundle, FluentValue, FluentResource, FluentArgs};
//!
//! // Used to provide a locale for the bundle.
//! use unic_langid::langid;
//!
//! let ftl_string = String::from("
//! hello-world = Hello, world!
//! intro = Welcome, { $name }.
//! ");
//! let res = FluentResource::try_new(ftl_string)
//!     .expect("Failed to parse an FTL string.");
//!
//! let langid_en = langid!("en-US");
//! let mut bundle = FluentBundle::new(vec![langid_en]);
//!
//! bundle
//!     .add_resource(res)
//!     .expect("Failed to add FTL resources to the bundle.");
//!
//! let msg = bundle.get_message("hello-world")
//!     .expect("Message doesn't exist.");
//! let mut errors = vec![];
//! let pattern = msg.value
//!     .expect("Message has no value.");
//! let value = bundle.format_pattern(&pattern, None, &mut errors);
//!
//! assert_eq!(&value, "Hello, world!");
//!
//! let mut args = FluentArgs::new();
//! args.add("name", FluentValue::from("John"));
//!
//! let msg = bundle.get_message("intro")
//!     .expect("Message doesn't exist.");
//! let mut errors = vec![];
//! let pattern = msg.value.expect("Message has no value.");
//! let value = bundle.format_pattern(&pattern, Some(&args), &mut errors);
//!
//! // The FSI/PDI isolation marks ensure that the direction of
//! // the text from the variable is not affected by the translation.
//! assert_eq!(value, "Welcome, \u{2068}John\u{2069}.");
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
//!
//! [`FluentBundle`]: ./type.FluentBundle.html
//! [`FluentResource`]: ./struct.FluentResource.html
//! [`FluentMessage`]: ./struct.FluentMessage.html
//! [`FluentValue`]: ./types/enum.FluentValue.html
//! [`FluentArgs`]: ./struct.FluentArgs.html

use intl_memoizer::{IntlLangMemoizer, Memoizable};
use unic_langid::LanguageIdentifier;

mod args;
pub mod bundle;
pub mod concurrent;
mod entry;
mod errors;
pub mod memoizer;
mod message;
pub mod resolver;
mod resource;
pub mod types;

pub use args::FluentArgs;
pub use errors::FluentError;
pub use message::{FluentAttribute, FluentMessage};
pub use resource::FluentResource;
pub use types::FluentValue;

/// A collection of localization messages for a single locale, which are meant
/// to be used together in a single view, widget or any other UI abstraction.
///
/// # Examples
///
/// ```
/// use fluent_bundle::{FluentBundle, FluentResource, FluentValue, FluentArgs};
/// use unic_langid::langid;
///
/// let ftl_string = String::from("intro = Welcome, { $name }.");
/// let resource = FluentResource::try_new(ftl_string)
///     .expect("Could not parse an FTL string.");
///
/// let langid_en = langid!("en-US");
/// let mut bundle = FluentBundle::new(vec![langid_en]);
///
/// bundle.add_resource(&resource)
///     .expect("Failed to add FTL resources to the bundle.");
///
/// let mut args = FluentArgs::new();
/// args.add("name", FluentValue::from("Rustacean"));
///
/// let msg = bundle.get_message("intro").expect("Message doesn't exist.");
/// let mut errors = vec![];
/// let pattern = msg.value.expect("Message has no value.");
/// let value = bundle.format_pattern(&pattern, Some(&args), &mut errors);
/// assert_eq!(&value, "Welcome, \u{2068}Rustacean\u{2069}.");
///
/// ```
///
/// # `FluentBundle` Life Cycle
///
/// ## Create a bundle
///
/// To create a bundle, call [`FluentBundle::new`] with a locale list that represents the best
/// possible fallback chain for a given locale. The simplest case is a one-locale list.
///
/// Fluent uses [`LanguageIdentifier`] which can be created using `langid!` macro.
///
/// ## Add Resources
///
/// Next, call [`add_resource`] one or more times, supplying translations in the FTL syntax.
///
/// Since [`FluentBundle`] is generic over anything that can borrow a [`FluentResource`],
/// one can use [`FluentBundle`] to own its resources, store references to them,
/// or even [`Rc<FluentResource>`] or [`Arc<FluentResource>`].
///
/// The [`FluentBundle`] instance is now ready to be used for localization.
///
/// ## Format
///
/// To format a translation, call [`get_message`] to retrieve a [`FluentMessage`],
/// and then call [`format_pattern`] on the message value or attribute in order to
/// retrieve the translated string.
///
/// The result of [`format_pattern`] is an [`Cow<str>`]. It is
/// recommended to treat the result as opaque from the perspective of the program and use it only
/// to display localized messages. Do not examine it or alter in any way before displaying.  This
/// is a general good practice as far as all internationalization operations are concerned.
///
/// If errors were encountered during formatting, they will be
/// accumulated in the [`Vec<FluentError>`] passed as the third argument.
///
/// While they are not fatal, they usually indicate problems with the translation,
/// and should be logged or reported in a way that allows the developer to notice
/// and fix them.
///
///
/// # Locale Fallback Chain
///
/// [`FluentBundle`] stores messages in a single locale, but keeps a locale fallback chain for the
/// purpose of language negotiation with i18n formatters. For instance, if date and time formatting
/// are not available in the first locale, [`FluentBundle`] will use its `locales` fallback chain
/// to negotiate a sensible fallback for date and time formatting.
///
/// # Concurrency
///
/// As you may have noticed, `FluentBundle` is a specialization of [`FluentBundleBase`]
/// which works with an [`IntlMemoizer`][] over `RefCell`.
/// In scenarios where the memoizer must work concurrently, there's an implementation of
/// `IntlMemoizer` that uses `Mutex` and there's [`concurrent::FluentBundle`] which works with that.
///
/// [`add_resource`]: ./bundle/struct.FluentBundleBase.html#method.add_resource
/// [`FluentBundle::new`]: ./bundle/struct.FluentBundleBase.html#method.new
/// [`FluentMessage`]: ./struct.FluentMessage.html
/// [`FluentBundle`]: ./type.FluentBundle.html
/// [`FluentResource`]: ./struct.FluentResource.html
/// [`get_message`]: ./bundle/struct.FluentBundleBase.html#method.get_message
/// [`format_pattern`]: ./bundle/struct.FluentBundleBase.html#method.format_pattern
/// [`Cow<str>`]: http://doc.rust-lang.org/std/borrow/enum.Cow.html
/// [`Rc<FluentResource>`]: https://doc.rust-lang.org/std/rc/struct.Rc.html
/// [`Arc<FluentResource>`]: https://doc.rust-lang.org/std/sync/struct.Arc.html
/// [`LanguageIdentifier`]: https://crates.io/crates/unic-langid
/// [`IntlMemoizer`]: https://github.com/projectfluent/fluent-rs/tree/master/intl-memoizer
/// [`Vec<FluentError>`]: ./enum.FluentError.html
/// [`concurrent::FluentBundle`]: ./concurrent/type.FluentBundle.html
pub type FluentBundle<R> = bundle::FluentBundleBase<R, IntlLangMemoizer>;

impl memoizer::MemoizerKind for IntlLangMemoizer {
    fn new(lang: LanguageIdentifier) -> Self
    where
        Self: Sized,
    {
        Self::new(lang)
    }

    fn with_try_get_threadsafe<I, R, U>(&self, args: I::Args, cb: U) -> Result<R, I::Error>
    where
        Self: Sized,
        I: Memoizable + Send + Sync + 'static,
        I::Args: Send + Sync + 'static,
        U: FnOnce(&I) -> R,
    {
        self.with_try_get(args, cb)
    }

    fn stringify_value(&self, value: &dyn types::FluentType) -> std::borrow::Cow<'static, str> {
        value.as_string(self)
    }
}
