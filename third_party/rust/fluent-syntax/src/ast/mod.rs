mod helper;

#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize};

#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Resource<S> {
    pub body: Vec<Entry<S>>,
}

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

#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Message<S> {
    pub id: Identifier<S>,
    pub value: Option<Pattern<S>>,
    pub attributes: Vec<Attribute<S>>,
    pub comment: Option<Comment<S>>,
}

#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Term<S> {
    pub id: Identifier<S>,
    pub value: Pattern<S>,
    pub attributes: Vec<Attribute<S>>,
    pub comment: Option<Comment<S>>,
}

#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Pattern<S> {
    pub elements: Vec<PatternElement<S>>,
}

#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[cfg_attr(feature = "serde", serde(tag = "type"))]
pub enum PatternElement<S> {
    TextElement { value: S },
    Placeable { expression: Expression<S> },
}

#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Attribute<S> {
    pub id: Identifier<S>,
    pub value: Pattern<S>,
}

#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Identifier<S> {
    pub name: S,
}

#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[cfg_attr(feature = "serde", serde(tag = "type"))]
pub struct Variant<S> {
    pub key: VariantKey<S>,
    pub value: Pattern<S>,
    pub default: bool,
}

#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[cfg_attr(feature = "serde", serde(tag = "type"))]
pub enum VariantKey<S> {
    Identifier { name: S },
    NumberLiteral { value: S },
}

#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[cfg_attr(feature = "serde", serde(from = "helper::CommentDef<S>"))]
pub struct Comment<S> {
    pub content: Vec<S>,
}

#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[cfg_attr(feature = "serde", serde(tag = "type"))]
pub struct CallArguments<S> {
    pub positional: Vec<InlineExpression<S>>,
    pub named: Vec<NamedArgument<S>>,
}

#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[cfg_attr(feature = "serde", serde(tag = "type"))]
pub struct NamedArgument<S> {
    pub name: Identifier<S>,
    pub value: InlineExpression<S>,
}

#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[cfg_attr(feature = "serde", serde(tag = "type"))]
pub enum InlineExpression<S> {
    StringLiteral {
        value: S,
    },
    NumberLiteral {
        value: S,
    },
    FunctionReference {
        id: Identifier<S>,
        arguments: Option<CallArguments<S>>,
    },
    MessageReference {
        id: Identifier<S>,
        attribute: Option<Identifier<S>>,
    },
    TermReference {
        id: Identifier<S>,
        attribute: Option<Identifier<S>>,
        arguments: Option<CallArguments<S>>,
    },
    VariableReference {
        id: Identifier<S>,
    },
    Placeable {
        expression: Box<Expression<S>>,
    },
}

#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[cfg_attr(feature = "serde", serde(untagged))]
pub enum Expression<S> {
    SelectExpression {
        selector: InlineExpression<S>,
        variants: Vec<Variant<S>>,
    },
    InlineExpression(InlineExpression<S>),
}
