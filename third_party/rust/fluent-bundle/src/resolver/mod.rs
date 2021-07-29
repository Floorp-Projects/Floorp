pub mod errors;
mod expression;
mod inline_expression;
mod pattern;
mod scope;

pub use errors::ResolverError;
pub use scope::Scope;

use std::borrow::Borrow;
use std::fmt;

use crate::memoizer::MemoizerKind;
use crate::resource::FluentResource;
use crate::types::FluentValue;

// Converts an AST node to a `FluentValue`.
pub(crate) trait ResolveValue {
    fn resolve<'source, 'errors, R, M>(
        &'source self,
        scope: &mut Scope<'source, 'errors, R, M>,
    ) -> FluentValue<'source>
    where
        R: Borrow<FluentResource>,
        M: MemoizerKind;
}

pub(crate) trait WriteValue {
    fn write<'source, 'errors, W, R, M>(
        &'source self,
        w: &mut W,
        scope: &mut Scope<'source, 'errors, R, M>,
    ) -> fmt::Result
    where
        W: fmt::Write,
        R: Borrow<FluentResource>,
        M: MemoizerKind;

    fn write_error<W>(&self, _w: &mut W) -> fmt::Result
    where
        W: fmt::Write;
}
