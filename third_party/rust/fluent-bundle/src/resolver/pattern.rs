use super::scope::Scope;
use super::{ResolverError, WriteValue};

use std::borrow::Borrow;
use std::fmt;

use fluent_syntax::ast;

use crate::memoizer::MemoizerKind;
use crate::resolver::ResolveValue;
use crate::resource::FluentResource;
use crate::types::FluentValue;

const MAX_PLACEABLES: u8 = 100;

impl<'p> WriteValue for ast::Pattern<&'p str> {
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
        let len = self.elements.len();

        for elem in &self.elements {
            if scope.dirty {
                return Ok(());
            }

            match elem {
                ast::PatternElement::TextElement { value } => {
                    if let Some(ref transform) = scope.bundle.transform {
                        w.write_str(&transform(value))?;
                    } else {
                        w.write_str(value)?;
                    }
                }
                ast::PatternElement::Placeable { ref expression } => {
                    scope.placeables += 1;
                    if scope.placeables > MAX_PLACEABLES {
                        scope.dirty = true;
                        scope.add_error(ResolverError::TooManyPlaceables);
                        return Ok(());
                    }

                    let needs_isolation = scope.bundle.use_isolating
                        && len > 1
                        && !matches!(
                            expression,
                            ast::Expression::Inline(ast::InlineExpression::MessageReference { .. },)
                                | ast::Expression::Inline(
                                    ast::InlineExpression::TermReference { .. },
                                )
                                | ast::Expression::Inline(
                                    ast::InlineExpression::StringLiteral { .. },
                                )
                        );
                    if needs_isolation {
                        w.write_char('\u{2068}')?;
                    }
                    scope.maybe_track(w, self, expression)?;
                    if needs_isolation {
                        w.write_char('\u{2069}')?;
                    }
                }
            }
        }
        Ok(())
    }

    fn write_error<W>(&self, _w: &mut W) -> fmt::Result
    where
        W: fmt::Write,
    {
        unreachable!()
    }
}

impl<'p> ResolveValue for ast::Pattern<&'p str> {
    fn resolve<'source, 'errors, R, M>(
        &'source self,
        scope: &mut Scope<'source, 'errors, R, M>,
    ) -> FluentValue<'source>
    where
        R: Borrow<FluentResource>,
        M: MemoizerKind,
    {
        let len = self.elements.len();

        if len == 1 {
            if let ast::PatternElement::TextElement { value } = self.elements[0] {
                return scope
                    .bundle
                    .transform
                    .map_or_else(|| value.into(), |transform| transform(value).into());
            }
        }

        let mut result = String::new();
        self.write(&mut result, scope)
            .expect("Failed to write to a string.");
        result.into()
    }
}
