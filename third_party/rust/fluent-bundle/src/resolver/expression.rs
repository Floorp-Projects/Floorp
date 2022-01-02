use super::scope::Scope;
use super::WriteValue;

use std::borrow::Borrow;
use std::fmt;

use fluent_syntax::ast;

use crate::memoizer::MemoizerKind;
use crate::resolver::{ResolveValue, ResolverError};
use crate::resource::FluentResource;
use crate::types::FluentValue;

impl<'p> WriteValue for ast::Expression<&'p str> {
    fn write<'scope, 'errors, W, R, M>(
        &'scope self,
        w: &mut W,
        scope: &mut Scope<'scope, 'errors, R, M>,
    ) -> fmt::Result
    where
        W: fmt::Write,
        R: Borrow<FluentResource>,
        M: MemoizerKind,
    {
        match self {
            Self::Inline(exp) => exp.write(w, scope),
            Self::Select { selector, variants } => {
                let selector = selector.resolve(scope);
                match selector {
                    FluentValue::String(_) | FluentValue::Number(_) => {
                        for variant in variants {
                            let key = match variant.key {
                                ast::VariantKey::Identifier { name } => name.into(),
                                ast::VariantKey::NumberLiteral { value } => {
                                    FluentValue::try_number(value)
                                }
                            };
                            if key.matches(&selector, scope) {
                                return variant.value.write(w, scope);
                            }
                        }
                    }
                    _ => {}
                }

                for variant in variants {
                    if variant.default {
                        return variant.value.write(w, scope);
                    }
                }
                scope.add_error(ResolverError::MissingDefault);
                Ok(())
            }
        }
    }

    fn write_error<W>(&self, w: &mut W) -> fmt::Result
    where
        W: fmt::Write,
    {
        match self {
            Self::Inline(exp) => exp.write_error(w),
            Self::Select { .. } => unreachable!(),
        }
    }
}
