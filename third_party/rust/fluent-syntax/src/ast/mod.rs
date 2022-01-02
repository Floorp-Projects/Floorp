//! Abstract Syntax Tree representation of the Fluent Translation List.
//!
//! The AST of Fluent contains all nodes structures to represent a complete
//! representation of the FTL resource.
//!
//! The tree preserves all semantic information and allow for round-trip
//! of a canonically written FTL resource.
//!
//! The root node is called [`Resource`] and contains a list of [`Entry`] nodes
//! representing all possible entries in the Fluent Translation List.
//!
//! # Example
//!
//! ```
//! use fluent_syntax::parser;
//! use fluent_syntax::ast;
//!
//! let ftl = r#"
//!
//! ## This is a message comment
//! hello-world = Hello World!
//!     .tooltip = Tooltip for you, { $userName }.
//!
//! "#;
//!
//! let resource = parser::parse(ftl)
//!     .expect("Failed to parse an FTL resource.");
//!
//! assert_eq!(
//!     resource.body[0],
//!     ast::Entry::Message(
//!         ast::Message {
//!             id: ast::Identifier {
//!                 name: "hello-world"
//!             },
//!             value: Some(ast::Pattern {
//!                 elements: vec![
//!                     ast::PatternElement::TextElement {
//!                         value: "Hello World!"
//!                     },
//!                 ]
//!             }),
//!             attributes: vec![
//!                 ast::Attribute {
//!                     id: ast::Identifier {
//!                         name: "tooltip"
//!                     },
//!                     value: ast::Pattern {
//!                         elements: vec![
//!                             ast::PatternElement::TextElement {
//!                                 value: "Tooltip for you, "
//!                             },
//!                             ast::PatternElement::Placeable {
//!                                 expression: ast::Expression::Inline(
//!                                     ast::InlineExpression::VariableReference {
//!                                         id: ast::Identifier {
//!                                             name: "userName"
//!                                         }
//!                                     }
//!                                 )
//!                             },
//!                             ast::PatternElement::TextElement {
//!                                 value: "."
//!                             },
//!                         ]
//!                     }
//!                 }
//!             ],
//!             comment: Some(
//!                 ast::Comment {
//!                     content: vec!["This is a message comment"]
//!                 }
//!             )
//!         }
//!     ),
//! );
//! ```
//!
//! ## Errors
//!
//! Fluent AST preserves blocks containing invaid syntax as [`Entry::Junk`].
//!
//! ## White space
//!
//! At the moment, AST does not preserve white space. In result only a
//! canonical form of the AST is suitable for a round-trip.
mod helper;

#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize};

/// Root node of a Fluent Translation List.
///
/// A [`Resource`] contains a body with a list of [`Entry`] nodes.
///
/// # Example
///
/// ```
/// use fluent_syntax::parser;
/// use fluent_syntax::ast;
///
/// let ftl = "";
///
/// let resource = parser::parse(ftl)
///     .expect("Failed to parse an FTL resource.");
///
/// assert_eq!(
///     resource,
///     ast::Resource {
///         body: vec![]
///     }
/// );
/// ```
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Resource<S> {
    pub body: Vec<Entry<S>>,
}

/// A top-level node representing an entry of a [`Resource`].
///
/// Every [`Entry`] is a standalone element and the parser is capable
/// of recovering from errors by identifying a beginning of a next entry.
///
/// # Example
///
/// ```
/// use fluent_syntax::parser;
/// use fluent_syntax::ast;
///
/// let ftl = r#"
///
/// key = Value
///
/// "#;
///
/// let resource = parser::parse(ftl)
///     .expect("Failed to parse an FTL resource.");
///
/// assert_eq!(
///     resource,
///     ast::Resource {
///         body: vec![
///             ast::Entry::Message(
///                 ast::Message {
///                     id: ast::Identifier {
///                         name: "key"
///                     },
///                     value: Some(ast::Pattern {
///                         elements: vec![
///                             ast::PatternElement::TextElement {
///                                 value: "Value"
///                             },
///                         ]
///                     }),
///                     attributes: vec![],
///                     comment: None,
///                 }
///             )
///         ]
///     }
/// );
/// ```
///
/// # Junk Entry
///
/// If FTL source contains invalid FTL content, it will be preserved
/// in form of [`Entry::Junk`] nodes.
///
/// # Example
///
/// ```
/// use fluent_syntax::parser;
/// use fluent_syntax::ast;
///
/// let ftl = r#"
///
/// g@rb@ge En!ry
///
/// "#;
///
/// let (resource, _) = parser::parse(ftl)
///     .expect_err("Failed to parse an FTL resource.");
///
/// assert_eq!(
///     resource,
///     ast::Resource {
///         body: vec![
///             ast::Entry::Junk {
///                 content: "g@rb@ge En!ry\n\n"
///             }
///         ]
///     }
/// );
/// ```
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[cfg_attr(feature = "serde", serde(tag = "type"))]
pub enum Entry<S> {
    Message(Message<S>),
    Term(Term<S>),
    Comment(Comment<S>),
    GroupComment(Comment<S>),
    ResourceComment(Comment<S>),
    Junk { content: S },
}

/// Message node represents the most common [`Entry`] in an FTL [`Resource`].
///
/// A message is a localization unit with a [`Identifier`] unique within a given
/// [`Resource`], and a value or attributes with associated [`Pattern`].
///
/// A message can contain a simple text value, or a compound combination of value
/// and attributes which together can be used to localize a complex User Interface
/// element.
///
/// Finally, each [`Message`] may have an associated [`Comment`].
///
/// # Example
///
/// ```
/// use fluent_syntax::parser;
/// use fluent_syntax::ast;
///
/// let ftl = r#"
///
/// hello-world = Hello, World!
///
/// "#;
///
/// let resource = parser::parse(ftl)
///     .expect("Failed to parse an FTL resource.");
///
/// assert_eq!(
///     resource,
///     ast::Resource {
///         body: vec![
///             ast::Entry::Message(ast::Message {
///                 id: ast::Identifier {
///                     name: "hello-world"
///                 },
///                 value: Some(ast::Pattern {
///                     elements: vec![
///                         ast::PatternElement::TextElement {
///                             value: "Hello, World!"
///                         }
///                     ]
///                 }),
///                 attributes: vec![],
///                 comment: None,
///             })
///         ]
///     }
/// );
/// ```
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Message<S> {
    pub id: Identifier<S>,
    pub value: Option<Pattern<S>>,
    pub attributes: Vec<Attribute<S>>,
    pub comment: Option<Comment<S>>,
}

/// A Fluent [`Term`].
///
/// Terms are semantically similar to [`Message`] nodes, but
/// they represent a separate concept in Fluent system.
///
/// Every term has to have a value, and the parser will
/// report errors when term references are used in wrong positions.
///
/// # Example
///
/// ```
/// use fluent_syntax::parser;
/// use fluent_syntax::ast;
///
/// let ftl = r#"
///
/// -brand-name = Nightly
///
/// "#;
///
/// let resource = parser::parse(ftl)
///     .expect("Failed to parse an FTL resource.");
///
/// assert_eq!(
///     resource,
///     ast::Resource {
///         body: vec![
///             ast::Entry::Term(ast::Term {
///                 id: ast::Identifier {
///                     name: "brand-name"
///                 },
///                 value: ast::Pattern {
///                     elements: vec![
///                         ast::PatternElement::TextElement {
///                             value: "Nightly"
///                         }
///                     ]
///                 },
///                 attributes: vec![],
///                 comment: None,
///             })
///         ]
///     }
/// );
/// ```
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Term<S> {
    pub id: Identifier<S>,
    pub value: Pattern<S>,
    pub attributes: Vec<Attribute<S>>,
    pub comment: Option<Comment<S>>,
}

/// Pattern contains a value of a [`Message`], [`Term`] or an [`Attribute`].
///
/// Each pattern is a list of [`PatternElement`] nodes representing
/// either a simple textual value, or a combination of text literals
/// and placeholder [`Expression`] nodes.
///
/// # Example
///
/// ```
/// use fluent_syntax::parser;
/// use fluent_syntax::ast;
///
/// let ftl = r#"
///
/// hello-world = Hello, World!
///
/// welcome = Welcome, { $userName }.
///
/// "#;
///
/// let resource = parser::parse(ftl)
///     .expect("Failed to parse an FTL resource.");
///
/// assert_eq!(
///     resource,
///     ast::Resource {
///         body: vec![
///             ast::Entry::Message(ast::Message {
///                 id: ast::Identifier {
///                     name: "hello-world"
///                 },
///                 value: Some(ast::Pattern {
///                     elements: vec![
///                         ast::PatternElement::TextElement {
///                             value: "Hello, World!"
///                         }
///                     ]
///                 }),
///                 attributes: vec![],
///                 comment: None,
///             }),
///             ast::Entry::Message(ast::Message {
///                 id: ast::Identifier {
///                     name: "welcome"
///                 },
///                 value: Some(ast::Pattern {
///                     elements: vec![
///                         ast::PatternElement::TextElement {
///                             value: "Welcome, "
///                         },
///                         ast::PatternElement::Placeable {
///                             expression: ast::Expression::Inline(
///                                 ast::InlineExpression::VariableReference {
///                                     id: ast::Identifier {
///                                         name: "userName"
///                                     }
///                                 }
///                             )
///                         },
///                         ast::PatternElement::TextElement {
///                             value: "."
///                         }
///                     ]
///                 }),
///                 attributes: vec![],
///                 comment: None,
///             }),
///         ]
///     }
/// );
/// ```
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Pattern<S> {
    pub elements: Vec<PatternElement<S>>,
}

/// PatternElement is an element of a [`Pattern`].
///
/// Each [`PatternElement`] node represents
/// either a simple textual value, or a combination of text literals
/// and placeholder [`Expression`] nodes.
///
/// # Example
///
/// ```
/// use fluent_syntax::parser;
/// use fluent_syntax::ast;
///
/// let ftl = r#"
///
/// hello-world = Hello, World!
///
/// welcome = Welcome, { $userName }.
///
/// "#;
///
/// let resource = parser::parse(ftl)
///     .expect("Failed to parse an FTL resource.");
///
/// assert_eq!(
///     resource,
///     ast::Resource {
///         body: vec![
///             ast::Entry::Message(ast::Message {
///                 id: ast::Identifier {
///                     name: "hello-world"
///                 },
///                 value: Some(ast::Pattern {
///                     elements: vec![
///                         ast::PatternElement::TextElement {
///                             value: "Hello, World!"
///                         }
///                     ]
///                 }),
///                 attributes: vec![],
///                 comment: None,
///             }),
///             ast::Entry::Message(ast::Message {
///                 id: ast::Identifier {
///                     name: "welcome"
///                 },
///                 value: Some(ast::Pattern {
///                     elements: vec![
///                         ast::PatternElement::TextElement {
///                             value: "Welcome, "
///                         },
///                         ast::PatternElement::Placeable {
///                             expression: ast::Expression::Inline(
///                                 ast::InlineExpression::VariableReference {
///                                     id: ast::Identifier {
///                                         name: "userName"
///                                     }
///                                 }
///                             )
///                         },
///                         ast::PatternElement::TextElement {
///                             value: "."
///                         }
///                     ]
///                 }),
///                 attributes: vec![],
///                 comment: None,
///             }),
///         ]
///     }
/// );
/// ```
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[cfg_attr(feature = "serde", serde(tag = "type"))]
pub enum PatternElement<S> {
    TextElement { value: S },
    Placeable { expression: Expression<S> },
}

/// Attribute represents a part of a [`Message`] or [`Term`].
///
/// Attributes are used to express a compound list of keyed
/// [`Pattern`] elements on an entry.
///
/// # Example
///
/// ```
/// use fluent_syntax::parser;
/// use fluent_syntax::ast;
///
/// let ftl = r#"
///
/// hello-world =
///     .title = This is a title
///     .accesskey = T
///
/// "#;
///
/// let resource = parser::parse(ftl)
///     .expect("Failed to parse an FTL resource.");
///
/// assert_eq!(
///     resource,
///     ast::Resource {
///         body: vec![
///             ast::Entry::Message(ast::Message {
///                 id: ast::Identifier {
///                     name: "hello-world"
///                 },
///                 value: None,
///                 attributes: vec![
///                     ast::Attribute {
///                         id: ast::Identifier {
///                             name: "title"
///                         },
///                         value: ast::Pattern {
///                             elements: vec![
///                                 ast::PatternElement::TextElement {
///                                     value: "This is a title"
///                                 },
///                             ]
///                         }
///                     },
///                     ast::Attribute {
///                         id: ast::Identifier {
///                             name: "accesskey"
///                         },
///                         value: ast::Pattern {
///                             elements: vec![
///                                 ast::PatternElement::TextElement {
///                                     value: "T"
///                                 },
///                             ]
///                         }
///                     }
///                 ],
///                 comment: None,
///             }),
///         ]
///     }
/// );
/// ```
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Attribute<S> {
    pub id: Identifier<S>,
    pub value: Pattern<S>,
}

/// Identifier is part of nodes such as [`Message`], [`Term`] and [`Attribute`].
///
/// It is used to associate a unique key with an [`Entry`] or an [`Attribute`]
/// and in [`Expression`] nodes to refer to another entry.
///
/// # Example
///
/// ```
/// use fluent_syntax::parser;
/// use fluent_syntax::ast;
///
/// let ftl = r#"
///
/// hello-world = Value
///
/// "#;
///
/// let resource = parser::parse(ftl)
///     .expect("Failed to parse an FTL resource.");
///
/// assert_eq!(
///     resource,
///     ast::Resource {
///         body: vec![
///             ast::Entry::Message(ast::Message {
///                 id: ast::Identifier {
///                     name: "hello-world"
///                 },
///                 value: Some(ast::Pattern {
///                     elements: vec![
///                         ast::PatternElement::TextElement {
///                             value: "Value"
///                         }
///                     ]
///                 }),
///                 attributes: vec![],
///                 comment: None,
///             }),
///         ]
///     }
/// );
/// ```
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Identifier<S> {
    pub name: S,
}

/// Variant is a single branch of a value in a [`Select`](Expression::Select) expression.
///
/// It's a pair of [`VariantKey`] and [`Pattern`]. If the selector match the
/// key, then the value of the variant is returned as the value of the expression.
///
/// # Example
///
/// ```
/// use fluent_syntax::parser;
/// use fluent_syntax::ast;
///
/// let ftl = r#"
///
/// hello-world = { $var ->
///     [key1] Value 1
///    *[other] Value 2
/// }
///
/// "#;
///
/// let resource = parser::parse(ftl)
///     .expect("Failed to parse an FTL resource.");
///
/// assert_eq!(
///     resource,
///     ast::Resource {
///         body: vec![
///             ast::Entry::Message(ast::Message {
///                 id: ast::Identifier {
///                     name: "hello-world"
///                 },
///                 value: Some(ast::Pattern {
///                     elements: vec![
///                         ast::PatternElement::Placeable {
///                             expression: ast::Expression::Select {
///                                 selector: ast::InlineExpression::VariableReference {
///                                     id: ast::Identifier { name: "var" },
///                                 },
///                                 variants: vec![
///                                     ast::Variant {
///                                         key: ast::VariantKey::Identifier {
///                                             name: "key1"
///                                         },
///                                         value: ast::Pattern {
///                                             elements: vec![
///                                                 ast::PatternElement::TextElement {
///                                                     value: "Value 1",
///                                                 }
///                                             ]
///                                         },
///                                         default: false,
///                                     },
///                                     ast::Variant {
///                                         key: ast::VariantKey::Identifier {
///                                             name: "other"
///                                         },
///                                         value: ast::Pattern {
///                                             elements: vec![
///                                                 ast::PatternElement::TextElement {
///                                                     value: "Value 2",
///                                                 }
///                                             ]
///                                         },
///                                         default: true,
///                                     },
///                                 ]
///                             }
///                         }
///                     ]
///                 }),
///                 attributes: vec![],
///                 comment: None,
///             }),
///         ]
///     }
/// );
/// ```
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[cfg_attr(feature = "serde", serde(tag = "type"))]
pub struct Variant<S> {
    pub key: VariantKey<S>,
    pub value: Pattern<S>,
    pub default: bool,
}

/// A key of a [`Variant`].
///
/// Variant key can be either an identifier or a number.
///
/// # Example
///
/// ```
/// use fluent_syntax::parser;
/// use fluent_syntax::ast;
///
/// let ftl = r#"
///
/// hello-world = { $var ->
///     [0] Value 1
///    *[other] Value 2
/// }
///
/// "#;
///
/// let resource = parser::parse(ftl)
///     .expect("Failed to parse an FTL resource.");
///
/// assert_eq!(
///     resource,
///     ast::Resource {
///         body: vec![
///             ast::Entry::Message(ast::Message {
///                 id: ast::Identifier {
///                     name: "hello-world"
///                 },
///                 value: Some(ast::Pattern {
///                     elements: vec![
///                         ast::PatternElement::Placeable {
///                             expression: ast::Expression::Select {
///                                 selector: ast::InlineExpression::VariableReference {
///                                     id: ast::Identifier { name: "var" },
///                                 },
///                                 variants: vec![
///                                     ast::Variant {
///                                         key: ast::VariantKey::NumberLiteral {
///                                             value: "0"
///                                         },
///                                         value: ast::Pattern {
///                                             elements: vec![
///                                                 ast::PatternElement::TextElement {
///                                                     value: "Value 1",
///                                                 }
///                                             ]
///                                         },
///                                         default: false,
///                                     },
///                                     ast::Variant {
///                                         key: ast::VariantKey::Identifier {
///                                             name: "other"
///                                         },
///                                         value: ast::Pattern {
///                                             elements: vec![
///                                                 ast::PatternElement::TextElement {
///                                                     value: "Value 2",
///                                                 }
///                                             ]
///                                         },
///                                         default: true,
///                                     },
///                                 ]
///                             }
///                         }
///                     ]
///                 }),
///                 attributes: vec![],
///                 comment: None,
///             }),
///         ]
///     }
/// );
/// ```
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[cfg_attr(feature = "serde", serde(tag = "type"))]
pub enum VariantKey<S> {
    Identifier { name: S },
    NumberLiteral { value: S },
}

/// Fluent [`Comment`].
///
/// In Fluent, comments may be standalone, or associated with
/// an entry such as [`Term`] or [`Message`].
///
/// When used as a standalone [`Entry`], comments may appear in one of
/// three levels:
///
/// * Standalone comment
/// * Group comment associated with a group of messages
/// * Resource comment associated with the whole resource
///
/// # Example
///
/// ```
/// use fluent_syntax::parser;
/// use fluent_syntax::ast;
///
/// let ftl = r#"
/// ## A standalone level comment
/// "#;
///
/// let resource = parser::parse(ftl)
///     .expect("Failed to parse an FTL resource.");
///
/// assert_eq!(
///     resource,
///     ast::Resource {
///         body: vec![
///             ast::Entry::Comment(ast::Comment {
///                 content: vec![
///                     "A standalone level comment"
///                 ]
///             })
///         ]
///     }
/// );
/// ```
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[cfg_attr(feature = "serde", serde(from = "helper::CommentDef<S>"))]
pub struct Comment<S> {
    pub content: Vec<S>,
}

/// List of arguments for a [`FunctionReference`](InlineExpression::FunctionReference) or a
/// [`TermReference`](InlineExpression::TermReference).
///
/// Function and Term reference may contain a list of positional and
/// named arguments passed to them.
///
/// # Example
///
/// ```
/// use fluent_syntax::parser;
/// use fluent_syntax::ast;
///
/// let ftl = r#"
///
/// key = { FUNC($var1, "literal", style: "long") }
///
/// "#;
///
/// let resource = parser::parse(ftl)
///     .expect("Failed to parse an FTL resource.");
///
/// assert_eq!(
///     resource,
///     ast::Resource {
///         body: vec![
///             ast::Entry::Message(
///                 ast::Message {
///                     id: ast::Identifier {
///                         name: "key"
///                     },
///                     value: Some(ast::Pattern {
///                         elements: vec![
///                             ast::PatternElement::Placeable {
///                                 expression: ast::Expression::Inline(
///                                     ast::InlineExpression::FunctionReference {
///                                         id: ast::Identifier {
///                                             name: "FUNC"
///                                         },
///                                         arguments: ast::CallArguments {
///                                             positional: vec![
///                                                 ast::InlineExpression::VariableReference {
///                                                     id: ast::Identifier {
///                                                         name: "var1"
///                                                     }
///                                                 },
///                                                 ast::InlineExpression::StringLiteral {
///                                                     value: "literal",
///                                                 }
///                                             ],
///                                             named: vec![
///                                                 ast::NamedArgument {
///                                                     name: ast::Identifier {
///                                                         name: "style"
///                                                     },
///                                                     value: ast::InlineExpression::StringLiteral
///                                                     {
///                                                         value: "long"
///                                                     }
///                                                 }
///                                             ],
///                                         }
///                                     }
///                                 )
///                             },
///                         ]
///                     }),
///                     attributes: vec![],
///                     comment: None,
///                 }
///             )
///         ]
///     }
/// );
/// ```
#[derive(Debug, PartialEq, Clone, Default)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[cfg_attr(feature = "serde", serde(tag = "type"))]
pub struct CallArguments<S> {
    pub positional: Vec<InlineExpression<S>>,
    pub named: Vec<NamedArgument<S>>,
}

/// A key-value pair used in [`CallArguments`].
///
/// # Example
///
/// ```
/// use fluent_syntax::parser;
/// use fluent_syntax::ast;
///
/// let ftl = r#"
///
/// key = { FUNC(style: "long") }
///
/// "#;
///
/// let resource = parser::parse(ftl)
///     .expect("Failed to parse an FTL resource.");
///
/// assert_eq!(
///     resource,
///     ast::Resource {
///         body: vec![
///             ast::Entry::Message(
///                 ast::Message {
///                     id: ast::Identifier {
///                         name: "key"
///                     },
///                     value: Some(ast::Pattern {
///                         elements: vec![
///                             ast::PatternElement::Placeable {
///                                 expression: ast::Expression::Inline(
///                                     ast::InlineExpression::FunctionReference {
///                                         id: ast::Identifier {
///                                             name: "FUNC"
///                                         },
///                                         arguments: ast::CallArguments {
///                                             positional: vec![],
///                                             named: vec![
///                                                 ast::NamedArgument {
///                                                     name: ast::Identifier {
///                                                         name: "style"
///                                                     },
///                                                     value: ast::InlineExpression::StringLiteral
///                                                     {
///                                                         value: "long"
///                                                     }
///                                                 }
///                                             ],
///                                         }
///                                     }
///                                 )
///                             },
///                         ]
///                     }),
///                     attributes: vec![],
///                     comment: None,
///                 }
///             )
///         ]
///     }
/// );
/// ```
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[cfg_attr(feature = "serde", serde(tag = "type"))]
pub struct NamedArgument<S> {
    pub name: Identifier<S>,
    pub value: InlineExpression<S>,
}

/// A subset of expressions which can be used as [`Placeable`](PatternElement::Placeable),
/// [`selector`](Expression::Select), or in [`CallArguments`].
///
/// # Example
///
/// ```
/// use fluent_syntax::parser;
/// use fluent_syntax::ast;
///
/// let ftl = r#"
///
/// key = { $emailCount }
///
/// "#;
///
/// let resource = parser::parse(ftl)
///     .expect("Failed to parse an FTL resource.");
///
/// assert_eq!(
///     resource,
///     ast::Resource {
///         body: vec![
///             ast::Entry::Message(
///                 ast::Message {
///                     id: ast::Identifier {
///                         name: "key"
///                     },
///                     value: Some(ast::Pattern {
///                         elements: vec![
///                             ast::PatternElement::Placeable {
///                                 expression: ast::Expression::Inline(
///                                     ast::InlineExpression::VariableReference {
///                                         id: ast::Identifier {
///                                             name: "emailCount"
///                                         },
///                                     }
///                                 )
///                             },
///                         ]
///                     }),
///                     attributes: vec![],
///                     comment: None,
///                 }
///             )
///         ]
///     }
/// );
/// ```
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[cfg_attr(feature = "serde", serde(tag = "type"))]
pub enum InlineExpression<S> {
    /// Single line string literal enclosed in `"`.
    ///
    /// # Example
    ///
    /// ```
    /// use fluent_syntax::parser;
    /// use fluent_syntax::ast;
    ///
    /// let ftl = r#"
    ///
    /// key = { "this is a literal" }
    ///
    /// "#;
    ///
    /// let resource = parser::parse(ftl)
    ///     .expect("Failed to parse an FTL resource.");
    ///
    /// assert_eq!(
    ///     resource,
    ///     ast::Resource {
    ///         body: vec![
    ///             ast::Entry::Message(
    ///                 ast::Message {
    ///                     id: ast::Identifier {
    ///                         name: "key"
    ///                     },
    ///                     value: Some(ast::Pattern {
    ///                         elements: vec![
    ///                             ast::PatternElement::Placeable {
    ///                                 expression: ast::Expression::Inline(
    ///                                     ast::InlineExpression::StringLiteral {
    ///                                         value: "this is a literal",
    ///                                     }
    ///                                 )
    ///                             },
    ///                         ]
    ///                     }),
    ///                     attributes: vec![],
    ///                     comment: None,
    ///                 }
    ///             )
    ///         ]
    ///     }
    /// );
    /// ```
    StringLiteral { value: S },
    /// A number literal.
    ///
    /// # Example
    ///
    /// ```
    /// use fluent_syntax::parser;
    /// use fluent_syntax::ast;
    ///
    /// let ftl = r#"
    ///
    /// key = { -0.5 }
    ///
    /// "#;
    ///
    /// let resource = parser::parse(ftl)
    ///     .expect("Failed to parse an FTL resource.");
    ///
    /// assert_eq!(
    ///     resource,
    ///     ast::Resource {
    ///         body: vec![
    ///             ast::Entry::Message(
    ///                 ast::Message {
    ///                     id: ast::Identifier {
    ///                         name: "key"
    ///                     },
    ///                     value: Some(ast::Pattern {
    ///                         elements: vec![
    ///                             ast::PatternElement::Placeable {
    ///                                 expression: ast::Expression::Inline(
    ///                                     ast::InlineExpression::NumberLiteral {
    ///                                         value: "-0.5",
    ///                                     }
    ///                                 )
    ///                             },
    ///                         ]
    ///                     }),
    ///                     attributes: vec![],
    ///                     comment: None,
    ///                 }
    ///             )
    ///         ]
    ///     }
    /// );
    /// ```
    NumberLiteral { value: S },
    /// A function reference.
    ///
    /// # Example
    ///
    /// ```
    /// use fluent_syntax::parser;
    /// use fluent_syntax::ast;
    ///
    /// let ftl = r#"
    ///
    /// key = { FUNC() }
    ///
    /// "#;
    ///
    /// let resource = parser::parse(ftl)
    ///     .expect("Failed to parse an FTL resource.");
    ///
    /// assert_eq!(
    ///     resource,
    ///     ast::Resource {
    ///         body: vec![
    ///             ast::Entry::Message(
    ///                 ast::Message {
    ///                     id: ast::Identifier {
    ///                         name: "key"
    ///                     },
    ///                     value: Some(ast::Pattern {
    ///                         elements: vec![
    ///                             ast::PatternElement::Placeable {
    ///                                 expression: ast::Expression::Inline(
    ///                                     ast::InlineExpression::FunctionReference {
    ///                                         id: ast::Identifier {
    ///                                             name: "FUNC"
    ///                                         },
    ///                                         arguments: ast::CallArguments::default(),
    ///                                     }
    ///                                 )
    ///                             },
    ///                         ]
    ///                     }),
    ///                     attributes: vec![],
    ///                     comment: None,
    ///                 }
    ///             )
    ///         ]
    ///     }
    /// );
    /// ```
    FunctionReference {
        id: Identifier<S>,
        arguments: CallArguments<S>,
    },
    /// A reference to another message.
    ///
    /// # Example
    ///
    /// ```
    /// use fluent_syntax::parser;
    /// use fluent_syntax::ast;
    ///
    /// let ftl = r#"
    ///
    /// key = { key2 }
    ///
    /// "#;
    ///
    /// let resource = parser::parse(ftl)
    ///     .expect("Failed to parse an FTL resource.");
    ///
    /// assert_eq!(
    ///     resource,
    ///     ast::Resource {
    ///         body: vec![
    ///             ast::Entry::Message(
    ///                 ast::Message {
    ///                     id: ast::Identifier {
    ///                         name: "key"
    ///                     },
    ///                     value: Some(ast::Pattern {
    ///                         elements: vec![
    ///                             ast::PatternElement::Placeable {
    ///                                 expression: ast::Expression::Inline(
    ///                                     ast::InlineExpression::MessageReference {
    ///                                         id: ast::Identifier {
    ///                                             name: "key2"
    ///                                         },
    ///                                         attribute: None,
    ///                                     }
    ///                                 )
    ///                             },
    ///                         ]
    ///                     }),
    ///                     attributes: vec![],
    ///                     comment: None,
    ///                 }
    ///             )
    ///         ]
    ///     }
    /// );
    /// ```
    MessageReference {
        id: Identifier<S>,
        attribute: Option<Identifier<S>>,
    },
    /// A reference to a term.
    ///
    /// # Example
    ///
    /// ```
    /// use fluent_syntax::parser;
    /// use fluent_syntax::ast;
    ///
    /// let ftl = r#"
    ///
    /// key = { -brand-name }
    ///
    /// "#;
    ///
    /// let resource = parser::parse(ftl)
    ///     .expect("Failed to parse an FTL resource.");
    ///
    /// assert_eq!(
    ///     resource,
    ///     ast::Resource {
    ///         body: vec![
    ///             ast::Entry::Message(
    ///                 ast::Message {
    ///                     id: ast::Identifier {
    ///                         name: "key"
    ///                     },
    ///                     value: Some(ast::Pattern {
    ///                         elements: vec![
    ///                             ast::PatternElement::Placeable {
    ///                                 expression: ast::Expression::Inline(
    ///                                     ast::InlineExpression::TermReference {
    ///                                         id: ast::Identifier {
    ///                                             name: "brand-name"
    ///                                         },
    ///                                         attribute: None,
    ///                                         arguments: None,
    ///                                     }
    ///                                 )
    ///                             },
    ///                         ]
    ///                     }),
    ///                     attributes: vec![],
    ///                     comment: None,
    ///                 }
    ///             )
    ///         ]
    ///     }
    /// );
    /// ```
    TermReference {
        id: Identifier<S>,
        attribute: Option<Identifier<S>>,
        arguments: Option<CallArguments<S>>,
    },
    /// A reference to a variable.
    ///
    /// # Example
    ///
    /// ```
    /// use fluent_syntax::parser;
    /// use fluent_syntax::ast;
    ///
    /// let ftl = r#"
    ///
    /// key = { $var1 }
    ///
    /// "#;
    ///
    /// let resource = parser::parse(ftl)
    ///     .expect("Failed to parse an FTL resource.");
    ///
    /// assert_eq!(
    ///     resource,
    ///     ast::Resource {
    ///         body: vec![
    ///             ast::Entry::Message(
    ///                 ast::Message {
    ///                     id: ast::Identifier {
    ///                         name: "key"
    ///                     },
    ///                     value: Some(ast::Pattern {
    ///                         elements: vec![
    ///                             ast::PatternElement::Placeable {
    ///                                 expression: ast::Expression::Inline(
    ///                                     ast::InlineExpression::VariableReference {
    ///                                         id: ast::Identifier {
    ///                                             name: "var1"
    ///                                         },
    ///                                     }
    ///                                 )
    ///                             },
    ///                         ]
    ///                     }),
    ///                     attributes: vec![],
    ///                     comment: None,
    ///                 }
    ///             )
    ///         ]
    ///     }
    /// );
    /// ```
    VariableReference { id: Identifier<S> },
    /// A placeable which may contain another expression.
    ///
    /// # Example
    ///
    /// ```
    /// use fluent_syntax::parser;
    /// use fluent_syntax::ast;
    ///
    /// let ftl = r#"
    ///
    /// key = { { "placeable" } }
    ///
    /// "#;
    ///
    /// let resource = parser::parse(ftl)
    ///     .expect("Failed to parse an FTL resource.");
    ///
    /// assert_eq!(
    ///     resource,
    ///     ast::Resource {
    ///         body: vec![
    ///             ast::Entry::Message(
    ///                 ast::Message {
    ///                     id: ast::Identifier {
    ///                         name: "key"
    ///                     },
    ///                     value: Some(ast::Pattern {
    ///                         elements: vec![
    ///                             ast::PatternElement::Placeable {
    ///                                 expression: ast::Expression::Inline(
    ///                                     ast::InlineExpression::Placeable {
    ///                                         expression: Box::new(
    ///                                             ast::Expression::Inline(
    ///                                                 ast::InlineExpression::StringLiteral {
    ///                                                     value: "placeable"
    ///                                                 }
    ///                                             )
    ///                                         )
    ///                                     }
    ///                                 )
    ///                             },
    ///                         ]
    ///                     }),
    ///                     attributes: vec![],
    ///                     comment: None,
    ///                 }
    ///             )
    ///         ]
    ///     }
    /// );
    /// ```
    Placeable { expression: Box<Expression<S>> },
}

/// An expression that is either a select expression or an inline expression.
///
/// # Example
///
/// ```
/// use fluent_syntax::parser;
/// use fluent_syntax::ast;
///
/// let ftl = r#"
///
/// key = { $var ->
///     [key1] Value 1
///    *[other] Value 2
/// }
///
/// "#;
///
/// let resource = parser::parse(ftl)
///     .expect("Failed to parse an FTL resource.");
///
/// assert_eq!(
///     resource,
///     ast::Resource {
///         body: vec![
///             ast::Entry::Message(ast::Message {
///                 id: ast::Identifier {
///                     name: "key"
///                 },
///                 value: Some(ast::Pattern {
///                     elements: vec![
///                         ast::PatternElement::Placeable {
///                             expression: ast::Expression::Select {
///                                 selector: ast::InlineExpression::VariableReference {
///                                     id: ast::Identifier { name: "var" },
///                                 },
///                                 variants: vec![
///                                     ast::Variant {
///                                         key: ast::VariantKey::Identifier {
///                                             name: "key1"
///                                         },
///                                         value: ast::Pattern {
///                                             elements: vec![
///                                                 ast::PatternElement::TextElement {
///                                                     value: "Value 1",
///                                                 }
///                                             ]
///                                         },
///                                         default: false,
///                                     },
///                                     ast::Variant {
///                                         key: ast::VariantKey::Identifier {
///                                             name: "other"
///                                         },
///                                         value: ast::Pattern {
///                                             elements: vec![
///                                                 ast::PatternElement::TextElement {
///                                                     value: "Value 2",
///                                                 }
///                                             ]
///                                         },
///                                         default: true,
///                                     },
///                                 ]
///                             }
///                         }
///                     ]
///                 }),
///                 attributes: vec![],
///                 comment: None,
///             }),
///         ]
///     }
/// );
/// ```
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[cfg_attr(feature = "serde", serde(untagged))]
pub enum Expression<S> {
    Select {
        selector: InlineExpression<S>,
        variants: Vec<Variant<S>>,
    },
    Inline(InlineExpression<S>),
}
