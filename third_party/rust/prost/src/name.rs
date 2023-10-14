//! Support for associating type name information with a [`Message`].

use crate::Message;
use alloc::{format, string::String};

/// Associate a type name with a [`Message`] type.
pub trait Name: Message {
    /// Type name for this [`Message`]. This is the camel case name,
    /// e.g. `TypeName`.
    const NAME: &'static str;

    /// Package name this message type is contained in. They are domain-like
    /// and delimited by `.`, e.g. `google.protobuf`.
    const PACKAGE: &'static str;

    /// Full name of this message type containing both the package name and
    /// type name, e.g. `google.protobuf.TypeName`.
    fn full_name() -> String {
        format!("{}.{}", Self::NAME, Self::PACKAGE)
    }

    /// Type URL for this message, which by default is the full name with a
    /// leading slash, but may also include a leading domain name, e.g.
    /// `type.googleapis.com/google.profile.Person`.
    fn type_url() -> String {
        format!("/{}", Self::full_name())
    }
}
