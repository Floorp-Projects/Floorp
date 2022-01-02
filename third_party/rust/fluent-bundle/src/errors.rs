use crate::resolver::ResolverError;
use fluent_syntax::parser::ParserError;
use std::error::Error;

#[derive(Debug, PartialEq, Clone)]
pub enum EntryKind {
    Message,
    Term,
    Function,
}

impl std::fmt::Display for EntryKind {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Message => f.write_str("message"),
            Self::Term => f.write_str("term"),
            Self::Function => f.write_str("function"),
        }
    }
}

/// Core error type for Fluent runtime system.
///
/// It contains three main types of errors that may come up
/// during runtime use of the fluent-bundle crate.
#[derive(Debug, PartialEq, Clone)]
pub enum FluentError {
    /// An error which occurs when
    /// [`FluentBundle::add_resource`](crate::bundle::FluentBundle::add_resource)
    /// adds entries that are already registered in a given [`FluentBundle`](crate::FluentBundle).
    ///
    /// # Example
    ///
    /// ```
    /// use fluent_bundle::{FluentBundle, FluentResource};
    /// use unic_langid::langid;
    ///
    /// let ftl_string = String::from("intro = Welcome, { $name }.");
    /// let res1 = FluentResource::try_new(ftl_string)
    ///     .expect("Could not parse an FTL string.");
    ///
    /// let ftl_string = String::from("intro = Hi, { $name }.");
    /// let res2 = FluentResource::try_new(ftl_string)
    ///     .expect("Could not parse an FTL string.");
    ///
    /// let langid_en = langid!("en-US");
    /// let mut bundle = FluentBundle::new(vec![langid_en]);
    ///
    /// bundle.add_resource(&res1)
    ///     .expect("Failed to add FTL resources to the bundle.");
    ///
    /// assert!(bundle.add_resource(&res2).is_err());
    /// ```
    Overriding {
        kind: EntryKind,
        id: String,
    },
    ParserError(ParserError),
    ResolverError(ResolverError),
}

impl std::fmt::Display for FluentError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Overriding { kind, id } => {
                write!(f, "Attempt to override an existing {}: \"{}\".", kind, id)
            }
            Self::ParserError(err) => write!(f, "Parser error: {}", err),
            Self::ResolverError(err) => write!(f, "Resolver error: {}", err),
        }
    }
}

impl Error for FluentError {}

impl From<ResolverError> for FluentError {
    fn from(error: ResolverError) -> Self {
        Self::ResolverError(error)
    }
}

impl From<ParserError> for FluentError {
    fn from(error: ParserError) -> Self {
        Self::ParserError(error)
    }
}
