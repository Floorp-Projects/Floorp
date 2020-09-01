mod number;
mod plural;

pub use number::*;
use plural::*;

use std::any::Any;
use std::borrow::{Borrow, Cow};
use std::default::Default;
use std::fmt;
use std::str::FromStr;

use fluent_syntax::ast;
use intl_pluralrules::{PluralCategory, PluralRuleType};

use crate::memoizer::MemoizerKind;
use crate::resolve::Scope;
use crate::resource::FluentResource;

#[derive(Debug, PartialEq, Clone)]
pub enum DisplayableNodeType<'source> {
    Message(&'source str),
    Term(&'source str),
    Variable(&'source str),
    Function(&'source str),
    Expression,
}

#[derive(Debug, PartialEq, Clone)]
pub struct DisplayableNode<'source> {
    node_type: DisplayableNodeType<'source>,
    attribute: Option<&'source str>,
}

impl<'source> Default for DisplayableNode<'source> {
    fn default() -> Self {
        DisplayableNode {
            node_type: DisplayableNodeType::Expression,
            attribute: None,
        }
    }
}

impl<'source> DisplayableNode<'source> {
    pub fn get_error(&self) -> String {
        if self.attribute.is_some() {
            format!("Unknown attribute: {}", self)
        } else {
            match self.node_type {
                DisplayableNodeType::Message(..) => format!("Unknown message: {}", self),
                DisplayableNodeType::Term(..) => format!("Unknown term: {}", self),
                DisplayableNodeType::Variable(..) => format!("Unknown variable: {}", self),
                DisplayableNodeType::Function(..) => format!("Unknown function: {}", self),
                DisplayableNodeType::Expression => "Failed to resolve an expression.".to_string(),
            }
        }
    }
}

impl<'source> fmt::Display for DisplayableNode<'source> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self.node_type {
            DisplayableNodeType::Message(id) => write!(f, "{}", id)?,
            DisplayableNodeType::Term(id) => write!(f, "-{}", id)?,
            DisplayableNodeType::Variable(id) => write!(f, "${}", id)?,
            DisplayableNodeType::Function(id) => write!(f, "{}()", id)?,
            DisplayableNodeType::Expression => f.write_str("???")?,
        };
        if let Some(attr) = self.attribute {
            write!(f, ".{}", attr)?;
        }
        Ok(())
    }
}

impl<'source> From<&ast::Expression<'source>> for DisplayableNode<'source> {
    fn from(expr: &ast::Expression<'source>) -> Self {
        match expr {
            ast::Expression::InlineExpression(e) => e.into(),
            ast::Expression::SelectExpression { .. } => DisplayableNode::default(),
        }
    }
}

impl<'source> From<&ast::InlineExpression<'source>> for DisplayableNode<'source> {
    fn from(expr: &ast::InlineExpression<'source>) -> Self {
        match expr {
            ast::InlineExpression::MessageReference { id, attribute } => DisplayableNode {
                node_type: DisplayableNodeType::Message(id.name),
                attribute: attribute.as_ref().map(|attr| attr.name),
            },
            ast::InlineExpression::TermReference { id, attribute, .. } => DisplayableNode {
                node_type: DisplayableNodeType::Term(id.name),
                attribute: attribute.as_ref().map(|attr| attr.name),
            },
            ast::InlineExpression::VariableReference { id } => DisplayableNode {
                node_type: DisplayableNodeType::Variable(id.name),
                attribute: None,
            },
            ast::InlineExpression::FunctionReference { id, .. } => DisplayableNode {
                node_type: DisplayableNodeType::Function(id.name),
                attribute: None,
            },
            _ => DisplayableNode::default(),
        }
    }
}

pub trait FluentType: fmt::Debug + AnyEq + 'static {
    fn duplicate(&self) -> Box<dyn FluentType>;
    fn as_string(&self, intls: &intl_memoizer::IntlLangMemoizer) -> Cow<'static, str>;
    fn as_string_threadsafe(
        &self,
        intls: &intl_memoizer::concurrent::IntlLangMemoizer,
    ) -> Cow<'static, str>;
}

impl PartialEq for dyn FluentType {
    fn eq(&self, other: &Self) -> bool {
        self.equals(other.as_any())
    }
}

pub trait AnyEq: Any + 'static {
    fn equals(&self, other: &dyn Any) -> bool;
    fn as_any(&self) -> &dyn Any;
}

impl<T: Any + PartialEq> AnyEq for T {
    fn equals(&self, other: &dyn Any) -> bool {
        if let Some(that) = other.downcast_ref::<Self>() {
            self == that
        } else {
            false
        }
    }
    fn as_any(&self) -> &dyn Any {
        self
    }
}

/// The `FluentValue` enum represents values which can be formatted to a String.
///
/// The [`ResolveValue`][] trait from the [`resolve`][] module evaluates AST nodes into
/// `FluentValues` which can then be formatted to Strings using the i18n formatters stored by the
/// `FluentBundle` instance if required.
///
/// The arguments `HashMap` passed to [`FluentBundle::format`][] should also use `FluentValues`
/// as values of arguments.
///
/// [`ResolveValue`]: ../resolve/trait.ResolveValue.html
/// [`resolve`]: ../resolve
/// [`FluentBundle::format`]: ../bundle/struct.FluentBundle.html#method.format
#[derive(Debug)]
pub enum FluentValue<'source> {
    String(Cow<'source, str>),
    Number(FluentNumber),
    Custom(Box<dyn FluentType>),
    Error(DisplayableNode<'source>),
    None,
}
impl<'s> PartialEq for FluentValue<'s> {
    fn eq(&self, other: &Self) -> bool {
        match (self, other) {
            (FluentValue::String(s), FluentValue::String(s2)) => s == s2,
            (FluentValue::Number(s), FluentValue::Number(s2)) => s == s2,
            (FluentValue::Custom(s), FluentValue::Custom(s2)) => s == s2,
            _ => false,
        }
    }
}

impl<'s> Clone for FluentValue<'s> {
    fn clone(&self) -> Self {
        match self {
            FluentValue::String(s) => FluentValue::String(s.clone()),
            FluentValue::Number(s) => FluentValue::Number(s.clone()),
            FluentValue::Custom(s) => {
                let new_value: Box<dyn FluentType> = s.duplicate();
                FluentValue::Custom(new_value)
            }
            FluentValue::Error(e) => FluentValue::Error(e.clone()),
            FluentValue::None => FluentValue::None,
        }
    }
}

impl<'source> FluentValue<'source> {
    pub fn try_number<S: ToString>(v: S) -> Self {
        let s = v.to_string();
        if let Ok(num) = FluentNumber::from_str(&s.to_string()) {
            num.into()
        } else {
            s.into()
        }
    }

    pub fn matches<R: Borrow<FluentResource>, M: MemoizerKind>(
        &self,
        other: &FluentValue,
        scope: &Scope<R, M>,
    ) -> bool {
        match (self, other) {
            (&FluentValue::String(ref a), &FluentValue::String(ref b)) => a == b,
            (&FluentValue::Number(ref a), &FluentValue::Number(ref b)) => a == b,
            (&FluentValue::String(ref a), &FluentValue::Number(ref b)) => {
                let cat = match a.as_ref() {
                    "zero" => PluralCategory::ZERO,
                    "one" => PluralCategory::ONE,
                    "two" => PluralCategory::TWO,
                    "few" => PluralCategory::FEW,
                    "many" => PluralCategory::MANY,
                    "other" => PluralCategory::OTHER,
                    _ => return false,
                };
                scope
                    .bundle
                    .intls
                    .with_try_get_threadsafe::<PluralRules, _, _>(
                        (PluralRuleType::CARDINAL,),
                        |pr| pr.0.select(b) == Ok(cat),
                    )
                    .unwrap()
            }
            _ => false,
        }
    }

    pub fn as_string<R: Borrow<FluentResource>, M: MemoizerKind>(
        &self,
        scope: &Scope<R, M>,
    ) -> Cow<'source, str> {
        if let Some(formatter) = &scope.bundle.formatter {
            if let Some(val) = formatter(self, &scope.bundle.intls) {
                return val.into();
            }
        }
        match self {
            FluentValue::String(s) => s.clone(),
            FluentValue::Number(n) => n.as_string(),
            FluentValue::Error(d) => format!("{{{}}}", d.to_string()).into(),
            FluentValue::Custom(s) => scope.bundle.intls.stringify_value(&**s),
            FluentValue::None => "???".into(),
        }
    }
}

impl<'source> From<String> for FluentValue<'source> {
    fn from(s: String) -> Self {
        FluentValue::String(s.into())
    }
}

impl<'source> From<&'source str> for FluentValue<'source> {
    fn from(s: &'source str) -> Self {
        FluentValue::String(s.into())
    }
}

impl<'source> From<Cow<'source, str>> for FluentValue<'source> {
    fn from(s: Cow<'source, str>) -> Self {
        FluentValue::String(s)
    }
}
