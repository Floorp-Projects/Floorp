use crate::ast::{self, kw};
use crate::parser::{Parse, Parser, Result};

/// A WebAssembly GC opt-in directive, part of the GC prototype.
#[derive(Debug)]
pub struct GcOptIn {
    /// Where this event was defined
    pub span: ast::Span,
    /// The version of the GC proposal
    pub version: u8,
}

impl<'a> Parse<'a> for GcOptIn {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::gc_feature_opt_in>()?.0;
        let version = parser.parse()?;
        Ok(GcOptIn {
            span,
            version,
        })
    }
}
