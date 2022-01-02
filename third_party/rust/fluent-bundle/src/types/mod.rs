//! `types` module contains types necessary for Fluent runtime
//! value handling.
//! The core struct is [`FluentValue`] which is a type that can be passed
//! to the [`FluentBundle::format_pattern`](crate::bundle::FluentBundle) as an argument, it can be passed
//! to any Fluent Function, and any function may return it.
//!
//! This part of functionality is not fully hashed out yet, since we're waiting
//! for the internationalization APIs to mature, at which point all number
//! formatting operations will be moved out of Fluent.
//!
//! For now, [`FluentValue`] can be a string, a number, or a custom [`FluentType`]
//! which allows users of the library to implement their own types of values,
//! such as dates, or more complex structures needed for their bindings.
mod number;
mod plural;

pub use number::*;
use plural::PluralRules;

use std::any::Any;
use std::borrow::{Borrow, Cow};
use std::fmt;
use std::str::FromStr;

use intl_pluralrules::{PluralCategory, PluralRuleType};

use crate::memoizer::MemoizerKind;
use crate::resolver::Scope;
use crate::resource::FluentResource;

pub trait FluentType: fmt::Debug + AnyEq + 'static {
    fn duplicate(&self) -> Box<dyn FluentType + Send>;
    fn as_string(&self, intls: &intl_memoizer::IntlLangMemoizer) -> Cow<'static, str>;
    fn as_string_threadsafe(
        &self,
        intls: &intl_memoizer::concurrent::IntlLangMemoizer,
    ) -> Cow<'static, str>;
}

impl PartialEq for dyn FluentType + Send {
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
        other
            .downcast_ref::<Self>()
            .map_or(false, |that| self == that)
    }
    fn as_any(&self) -> &dyn Any {
        self
    }
}

/// The `FluentValue` enum represents values which can be formatted to a String.
///
/// Those values are either passed as arguments to [`FluentBundle::format_pattern`][] or
/// produced by functions, or generated in the process of pattern resolution.
///
/// [`FluentBundle::format_pattern`]: ../bundle/struct.FluentBundle.html#method.format_pattern
#[derive(Debug)]
pub enum FluentValue<'source> {
    String(Cow<'source, str>),
    Number(FluentNumber),
    Custom(Box<dyn FluentType + Send>),
    None,
    Error,
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
                let new_value: Box<dyn FluentType + Send> = s.duplicate();
                FluentValue::Custom(new_value)
            }
            FluentValue::Error => FluentValue::Error,
            FluentValue::None => FluentValue::None,
        }
    }
}

impl<'source> FluentValue<'source> {
    pub fn try_number<S: ToString>(v: S) -> Self {
        let s = v.to_string();
        if let Ok(num) = FluentNumber::from_str(&s) {
            num.into()
        } else {
            s.into()
        }
    }

    pub fn matches<R: Borrow<FluentResource>, M>(
        &self,
        other: &FluentValue,
        scope: &Scope<R, M>,
    ) -> bool
    where
        M: MemoizerKind,
    {
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

    pub fn write<W, R, M>(&self, w: &mut W, scope: &Scope<R, M>) -> fmt::Result
    where
        W: fmt::Write,
        R: Borrow<FluentResource>,
        M: MemoizerKind,
    {
        if let Some(formatter) = &scope.bundle.formatter {
            if let Some(val) = formatter(self, &scope.bundle.intls) {
                return w.write_str(&val);
            }
        }
        match self {
            FluentValue::String(s) => w.write_str(s),
            FluentValue::Number(n) => w.write_str(&n.as_string()),
            FluentValue::Custom(s) => w.write_str(&scope.bundle.intls.stringify_value(&**s)),
            FluentValue::Error => Ok(()),
            FluentValue::None => Ok(()),
        }
    }

    pub fn as_string<R: Borrow<FluentResource>, M>(&self, scope: &Scope<R, M>) -> Cow<'source, str>
    where
        M: MemoizerKind,
    {
        if let Some(formatter) = &scope.bundle.formatter {
            if let Some(val) = formatter(self, &scope.bundle.intls) {
                return val.into();
            }
        }
        match self {
            FluentValue::String(s) => s.clone(),
            FluentValue::Number(n) => n.as_string(),
            FluentValue::Custom(s) => scope.bundle.intls.stringify_value(&**s),
            FluentValue::Error => "".into(),
            FluentValue::None => "".into(),
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
