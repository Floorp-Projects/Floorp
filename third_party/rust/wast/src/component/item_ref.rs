use crate::parser::{Cursor, Parse, Parser, Peek, Result};
use crate::token::Index;

/// Parses `(func $foo)`
#[derive(Clone, Debug)]
#[allow(missing_docs)]
pub struct ItemRef<'a, K> {
    pub kind: K,
    pub idx: Index<'a>,
    pub export_names: Vec<&'a str>,
}

impl<'a, K: Parse<'a>> Parse<'a> for ItemRef<'a, K> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parens(|parser| {
            let kind = parser.parse::<K>()?;
            let idx = parser.parse()?;
            let mut export_names = Vec::new();
            while !parser.is_empty() {
                export_names.push(parser.parse()?);
            }
            Ok(ItemRef {
                kind,
                idx,
                export_names,
            })
        })
    }
}

impl<'a, K: Peek> Peek for ItemRef<'a, K> {
    fn peek(cursor: Cursor<'_>) -> bool {
        match cursor.lparen() {
            // This is a little fancy because when parsing something like:
            //
            //      (type (component (type $foo)))
            //
            // we need to disambiguate that from
            //
            //      (type (component (type $foo (func))))
            //
            // where the first is a type reference and the second is an inline
            // component type defining a type internally. The peek here not only
            // peeks for `K` but also for the index and possibly trailing
            // strings.
            Some(remaining) if K::peek(remaining) => {
                let remaining = match remaining.keyword() {
                    Some((_, remaining)) => remaining,
                    None => return false,
                };
                match remaining
                    .id()
                    .map(|p| p.1)
                    .or_else(|| remaining.integer().map(|p| p.1))
                {
                    Some(remaining) => remaining.rparen().is_some() || remaining.string().is_some(),
                    None => false,
                }
            }
            _ => false,
        }
    }

    fn display() -> &'static str {
        "an item reference"
    }
}

/// Convenience structure to parse `$f` or `(item $f)`.
#[derive(Clone, Debug)]
pub struct IndexOrRef<'a, K>(pub ItemRef<'a, K>);

impl<'a, K> Parse<'a> for IndexOrRef<'a, K>
where
    K: Parse<'a> + Default,
{
    fn parse(parser: Parser<'a>) -> Result<Self> {
        if parser.peek::<Index<'_>>() {
            Ok(IndexOrRef(ItemRef {
                kind: K::default(),
                idx: parser.parse()?,
                export_names: Vec::new(),
            }))
        } else {
            Ok(IndexOrRef(parser.parse()?))
        }
    }
}

impl<'a, K: Peek> Peek for IndexOrRef<'a, K> {
    fn peek(cursor: Cursor<'_>) -> bool {
        Index::peek(cursor) || ItemRef::<K>::peek(cursor)
    }

    fn display() -> &'static str {
        "an item reference"
    }
}
