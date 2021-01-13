use fluent_syntax::ast::InlineExpression;
use std::error::Error;

#[derive(Debug, PartialEq, Clone)]
pub enum ReferenceKind {
    Function {
        id: String,
    },
    Message {
        id: String,
        attribute: Option<String>,
    },
    Term {
        id: String,
        attribute: Option<String>,
    },
    Variable {
        id: String,
    },
}

impl<T> From<&InlineExpression<T>> for ReferenceKind
where
    T: ToString,
{
    fn from(exp: &InlineExpression<T>) -> Self {
        match exp {
            InlineExpression::FunctionReference { id, .. } => Self::Function {
                id: id.name.to_string(),
            },
            InlineExpression::MessageReference { id, attribute } => Self::Message {
                id: id.name.to_string(),
                attribute: attribute.as_ref().map(|i| i.name.to_string()),
            },
            InlineExpression::TermReference { id, attribute, .. } => Self::Term {
                id: id.name.to_string(),
                attribute: attribute.as_ref().map(|i| i.name.to_string()),
            },
            InlineExpression::VariableReference { id, .. } => Self::Variable {
                id: id.name.to_string(),
            },
            _ => unreachable!(),
        }
    }
}

#[derive(Debug, PartialEq, Clone)]
pub enum ResolverError {
    Reference(ReferenceKind),
    NoValue(String),
    MissingDefault,
    Cyclic,
    TooManyPlaceables,
}

impl std::fmt::Display for ResolverError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Reference(exp) => match exp {
                ReferenceKind::Function { id } => write!(f, "Unknown function: {}()", id),
                ReferenceKind::Message {
                    id,
                    attribute: None,
                } => write!(f, "Unknown message: {}", id),
                ReferenceKind::Message {
                    id,
                    attribute: Some(attribute),
                } => write!(f, "Unknown attribute: {}.{}", id, attribute),
                ReferenceKind::Term {
                    id,
                    attribute: None,
                } => write!(f, "Unknown term: -{}", id),
                ReferenceKind::Term {
                    id,
                    attribute: Some(attribute),
                } => write!(f, "Unknown attribute: -{}.{}", id, attribute),
                ReferenceKind::Variable { id } => write!(f, "Unknown variable: ${}", id),
            },
            Self::NoValue(id) => write!(f, "No value: {}", id),
            Self::MissingDefault => f.write_str("No default"),
            Self::Cyclic => f.write_str("Cyclical dependency detected"),
            Self::TooManyPlaceables => f.write_str("Too many placeables"),
        }
    }
}

impl<T> From<&InlineExpression<T>> for ResolverError
where
    T: ToString,
{
    fn from(exp: &InlineExpression<T>) -> Self {
        Self::Reference(exp.into())
    }
}

impl Error for ResolverError {}
