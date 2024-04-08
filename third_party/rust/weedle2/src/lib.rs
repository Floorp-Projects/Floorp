//! Weedle - A WebIDL Parser
//!
//! Parses valid WebIDL definitions & produces a data structure starting from
//! [`Definitions`](struct.Definitions.html).
//!
//! ### Example
//!
//! ```
//! extern crate weedle;
//!
//! let parsed = weedle::parse("
//!     interface Window {
//!         readonly attribute Storage sessionStorage;
//!     };
//! ").unwrap();
//! println!("{:?}", parsed);
//! ```
//!
//! Note:
//! This parser follows the grammar given at [WebIDL](https://heycam.github.io/webidl).
//!
//! If any flaws found when parsing string with a valid grammar, create an issue.

use self::argument::ArgumentList;
use self::attribute::ExtendedAttributeList;
use self::common::{Braced, Docstring, Identifier, Parenthesized, PunctuatedNonEmpty};
use self::dictionary::DictionaryMembers;
use self::interface::{Inheritance, InterfaceMembers};
use self::literal::StringLit;
use self::mixin::MixinMembers;
use self::namespace::NamespaceMembers;
use self::types::{AttributedType, ReturnType};
pub use nom::{error::Error, Err, IResult};

#[macro_use]
mod macros;
#[macro_use]
mod whitespace;
#[macro_use]
pub mod term;
pub mod argument;
pub mod attribute;
pub mod common;
pub mod dictionary;
pub mod interface;
pub mod literal;
pub mod mixin;
pub mod namespace;
pub mod types;

/// A convenient parse function
///
/// ### Example
///
/// ```
/// extern crate weedle;
///
/// let parsed = weedle::parse("
///     interface Window {
///         readonly attribute Storage sessionStorage;
///     };
/// ").unwrap();
///
/// println!("{:?}", parsed);
/// ```
pub fn parse(raw: &str) -> Result<Definitions<'_>, Err<Error<&str>>> {
    let (remaining, parsed) = Definitions::parse(raw)?;
    assert!(
        remaining.is_empty(),
        "There is redundant raw data after parsing"
    );
    Ok(parsed)
}

pub trait Parse<'a>: Sized {
    fn parse(input: &'a str) -> IResult<&'a str, Self>;
}

/// Parses WebIDL definitions. It is the root struct for a complete WebIDL definition.
///
/// ### Example
/// ```
/// use weedle::{Definitions, Parse};
///
/// let (_, parsed) = Definitions::parse("
///     interface Window {
///         readonly attribute Storage sessionStorage;
///     };
/// ").unwrap();
///
/// println!("{:?}", parsed);
/// ```
///
/// It is recommended to use [`parse`](fn.parse.html) instead.
pub type Definitions<'a> = Vec<Definition<'a>>;

ast_types! {
    /// Parses a definition
    enum Definition<'a> {
        /// Parses `[attributes]? callback identifier = type ( (arg1, arg2, ..., argN)? );`
        Callback(struct CallbackDefinition<'a> {
            attributes: Option<ExtendedAttributeList<'a>>,
            callback: term!(callback),
            identifier: Identifier<'a>,
            assign: term!(=),
            return_type: ReturnType<'a>,
            arguments: Parenthesized<ArgumentList<'a>>,
            semi_colon: term!(;),
        }),
        /// Parses `[attributes]? callback interface identifier ( : inheritance )? { members };`
        CallbackInterface(struct CallbackInterfaceDefinition<'a> {
            docstring: Option<Docstring>,
            attributes: Option<ExtendedAttributeList<'a>>,
            callback: term!(callback),
            interface: term!(interface),
            identifier: Identifier<'a>,
            inheritance: Option<Inheritance<'a>>,
            members: Braced<InterfaceMembers<'a>>,
            semi_colon: term!(;),
        }),
        /// Parses `[attributes]? interface identifier ( : inheritance )? { members };`
        Interface(struct InterfaceDefinition<'a> {
            docstring: Option<Docstring>,
            attributes: Option<ExtendedAttributeList<'a>>,
            interface: term!(interface),
            identifier: Identifier<'a>,
            inheritance: Option<Inheritance<'a>>,
            members: Braced<InterfaceMembers<'a>>,
            semi_colon: term!(;),
        }),
        /// Parses `[attributes]? interface mixin identifier { members };`
        InterfaceMixin(struct InterfaceMixinDefinition<'a> {
            attributes: Option<ExtendedAttributeList<'a>>,
            interface: term!(interface),
            mixin: term!(mixin),
            identifier: Identifier<'a>,
            members: Braced<MixinMembers<'a>>,
            semi_colon: term!(;),
        }),
        /// Parses `[attributes]? namespace identifier { members };`
        Namespace(struct NamespaceDefinition<'a> {
            docstring: Option<Docstring>,
            attributes: Option<ExtendedAttributeList<'a>>,
            namespace: term!(namespace),
            identifier: Identifier<'a>,
            members: Braced<NamespaceMembers<'a>>,
            semi_colon: term!(;),
        }),
        /// Parses `[attributes]? dictionary identifier ( : inheritance )? { members };`
        Dictionary(struct DictionaryDefinition<'a> {
            docstring: Option<Docstring>,
            attributes: Option<ExtendedAttributeList<'a>>,
            dictionary: term!(dictionary),
            identifier: Identifier<'a>,
            inheritance: Option<Inheritance<'a>>,
            members: Braced<DictionaryMembers<'a>>,
            semi_colon: term!(;),
        }),
        /// Parses `[attributes]? partial interface identifier { members };`
        PartialInterface(struct PartialInterfaceDefinition<'a> {
            attributes: Option<ExtendedAttributeList<'a>>,
            partial: term!(partial),
            interface: term!(interface),
            identifier: Identifier<'a>,
            members: Braced<InterfaceMembers<'a>>,
            semi_colon: term!(;),
        }),
        /// Parses `[attributes]? partial interface mixin identifier { members };`
        PartialInterfaceMixin(struct PartialInterfaceMixinDefinition<'a> {
            attributes: Option<ExtendedAttributeList<'a>>,
            partial: term!(partial),
            interface: term!(interface),
            mixin: term!(mixin),
            identifier: Identifier<'a>,
            members: Braced<MixinMembers<'a>>,
            semi_colon: term!(;),
        }),
        /// Parses `[attributes]? partial dictionary identifier { members };`
        PartialDictionary(struct PartialDictionaryDefinition<'a> {
            attributes: Option<ExtendedAttributeList<'a>>,
            partial: term!(partial),
            dictionary: term!(dictionary),
            identifier: Identifier<'a>,
            members: Braced<DictionaryMembers<'a>>,
            semi_colon: term!(;),
        }),
        /// Parses `[attributes]? partial namespace identifier { members };`
        PartialNamespace(struct PartialNamespaceDefinition<'a> {
            attributes: Option<ExtendedAttributeList<'a>>,
            partial: term!(partial),
            namespace: term!(namespace),
            identifier: Identifier<'a>,
            members: Braced<NamespaceMembers<'a>>,
            semi_colon: term!(;),
        }),
        /// Parses `[attributes]? enum identifier { values };`
        Enum(struct EnumDefinition<'a> {
            docstring: Option<Docstring>,
            attributes: Option<ExtendedAttributeList<'a>>,
            enum_: term!(enum),
            identifier: Identifier<'a>,
            values: Braced<EnumValueList<'a>>,
            semi_colon: term!(;),
        }),
        /// Parses `[attributes]? typedef attributedtype identifier;`
        Typedef(struct TypedefDefinition<'a> {
            attributes: Option<ExtendedAttributeList<'a>>,
            typedef: term!(typedef),
            type_: AttributedType<'a>,
            identifier: Identifier<'a>,
            semi_colon: term!(;),
        }),
        /// Parses `[attributes]? identifier includes identifier;`
        IncludesStatement(struct IncludesStatementDefinition<'a> {
            attributes: Option<ExtendedAttributeList<'a>>,
            lhs_identifier: Identifier<'a>,
            includes: term!(includes),
            rhs_identifier: Identifier<'a>,
            semi_colon: term!(;),
        }),
        /// Parses `[attributes]? identifier implements identifier;`
        Implements(struct ImplementsDefinition<'a> {
            attributes: Option<ExtendedAttributeList<'a>>,
            lhs_identifier: Identifier<'a>,
            includes: term!(implements),
            rhs_identifier: Identifier<'a>,
            semi_colon: term!(;),
        }),
    }
}

ast_types! {
    struct EnumVariant<'a> {
        docstring: Option<Docstring>,
        value: StringLit<'a>,
    }
}

/// Parses a non-empty enum value list
pub type EnumValueList<'a> = PunctuatedNonEmpty<EnumVariant<'a>, term!(,)>;

#[cfg(test)]
mod test {
    use super::*;

    test!(should_parse_includes_statement { "first includes second;" =>
        "";
        IncludesStatementDefinition;
        attributes.is_none();
        lhs_identifier.0 == "first";
        rhs_identifier.0 == "second";
    });

    test!(should_parse_typedef { "typedef short Short;" =>
        "";
        TypedefDefinition;
        attributes.is_none();
        identifier.0 == "Short";
    });

    test!(should_parse_enum { r#"enum name { "first", "second" }; "# =>
        "";
        EnumDefinition;
        attributes.is_none();
        identifier.0 == "name";
        values.body.list.len() == 2;
    });

    test!(should_parse_dictionary { "dictionary A { long c; long g; };" =>
        "";
        DictionaryDefinition;
        attributes.is_none();
        identifier.0 == "A";
        inheritance.is_none();
        members.body.len() == 2;
    });

    test!(should_parse_dictionary_inherited { "dictionary C : B { long e; long f; };" =>
        "";
        DictionaryDefinition;
        attributes.is_none();
        identifier.0 == "C";
        inheritance.is_some();
        members.body.len() == 2;
    });

    test!(should_parse_partial_namespace { "
        partial namespace VectorUtils {
            readonly attribute Vector unit;
            double dotProduct(Vector x, Vector y);
            Vector crossProduct(Vector x, Vector y);
        };
    " =>
        "";
        PartialNamespaceDefinition;
        attributes.is_none();
        identifier.0 == "VectorUtils";
        members.body.len() == 3;
    });

    test!(should_parse_partial_dictionary { "partial dictionary C { long e; long f; };" =>
        "";
        PartialDictionaryDefinition;
        attributes.is_none();
        identifier.0 == "C";
        members.body.len() == 2;
    });

    test!(should_parse_partial_interface_mixin { "
        partial interface mixin WindowSessionStorage {
          readonly attribute Storage sessionStorage;
        };
    " =>
        "";
        PartialInterfaceMixinDefinition;
        attributes.is_none();
        identifier.0 == "WindowSessionStorage";
        members.body.len() == 1;
    });

    test!(should_parse_partial_interface { "
        partial interface Window {
          readonly attribute Storage sessionStorage;
        };
    " =>
        "";
        PartialInterfaceDefinition;
        attributes.is_none();
        identifier.0 == "Window";
        members.body.len() == 1;
    });

    test!(should_parse_namespace { "
        namespace VectorUtils {
          readonly attribute Vector unit;
          double dotProduct(Vector x, Vector y);
          Vector crossProduct(Vector x, Vector y);
        };
    " =>
        "";
        NamespaceDefinition;
        attributes.is_none();
        identifier.0 == "VectorUtils";
        members.body.len() == 3;
    });

    test!(should_parse_interface_mixin { "
        interface mixin WindowSessionStorage {
          readonly attribute Storage sessionStorage;
        };
    " =>
        "";
        InterfaceMixinDefinition;
        attributes.is_none();
        identifier.0 == "WindowSessionStorage";
        members.body.len() == 1;
    });

    test!(should_parse_interface { "
        interface Window {
          readonly attribute Storage sessionStorage;
        };
    " =>
        "";
        InterfaceDefinition;
        attributes.is_none();
        identifier.0 == "Window";
        members.body.len() == 1;
    });

    test!(should_parse_callback_interface {"
        callback interface Options {
          attribute DOMString? option1;
          attribute DOMString? option2;
          attribute long? option3;
        };
    " =>
        "";
        CallbackInterfaceDefinition;
        attributes.is_none();
        identifier.0 == "Options";
        members.body.len() == 3;
    });

    test!(should_parse_callback { "callback AsyncOperationCallback = undefined (DOMString status);" =>
        "";
        CallbackDefinition;
        attributes.is_none();
        identifier.0 == "AsyncOperationCallback";
        arguments.body.list.len() == 1;
    });

    test!(should_parse_with_line_comments { "
        // This is a comment
        callback AsyncOperationCallback = undefined (DOMString status);
    " =>
        "";
        CallbackDefinition;
    });

    test!(should_parse_with_block_comments { "
        /* This is a comment */
        callback AsyncOperationCallback = undefined (DOMString status);
    " =>
        "";
        CallbackDefinition;
    });

    test!(should_parse_with_multiple_comments { "
        // This is a comment
        // This is a comment
        // This is a comment

        // This is a comment
        callback AsyncOperationCallback = undefined (DOMString status);
    " =>
        "";
        CallbackDefinition;
    });
}
