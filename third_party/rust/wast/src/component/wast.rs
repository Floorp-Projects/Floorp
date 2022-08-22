use crate::kw;
use crate::parser::{Cursor, Parse, Parser, Peek, Result};
use crate::token::{Float32, Float64};

/// Expression that can be used inside of `invoke` expressions for core wasm
/// functions.
#[derive(Debug)]
#[allow(missing_docs)]
pub enum WastVal<'a> {
    Unit,
    Bool(bool),
    U8(u8),
    S8(i8),
    U16(u16),
    S16(i16),
    U32(u32),
    S32(i32),
    U64(u64),
    S64(i64),
    Float32(Float32),
    Float64(Float64),
    Char(char),
    String(&'a str),
    List(Vec<WastVal<'a>>),
    Record(Vec<(&'a str, WastVal<'a>)>),
    Tuple(Vec<WastVal<'a>>),
    Variant(&'a str, Box<WastVal<'a>>),
    Enum(&'a str),
    Union(u32, Box<WastVal<'a>>),
    Option(Option<Box<WastVal<'a>>>),
    Expected(Result<Box<WastVal<'a>>, Box<WastVal<'a>>>),
    Flags(Vec<&'a str>),
}

static CASES: &[(&str, fn(Parser<'_>) -> Result<WastVal<'_>>)] = {
    use WastVal::*;
    &[
        ("unit.const", |_| Ok(Unit)),
        ("bool.const", |p| {
            let mut l = p.lookahead1();
            if l.peek::<kw::true_>() {
                p.parse::<kw::true_>()?;
                Ok(Bool(true))
            } else if l.peek::<kw::false_>() {
                p.parse::<kw::false_>()?;
                Ok(Bool(false))
            } else {
                Err(l.error())
            }
        }),
        ("u8.const", |p| Ok(U8(p.parse()?))),
        ("s8.const", |p| Ok(S8(p.parse()?))),
        ("u16.const", |p| Ok(U16(p.parse()?))),
        ("s16.const", |p| Ok(S16(p.parse()?))),
        ("u32.const", |p| Ok(U32(p.parse()?))),
        ("s32.const", |p| Ok(S32(p.parse()?))),
        ("u64.const", |p| Ok(U64(p.parse()?))),
        ("s64.const", |p| Ok(S64(p.parse()?))),
        ("f32.const", |p| Ok(Float32(p.parse()?))),
        ("f64.const", |p| Ok(Float64(p.parse()?))),
        ("char.const", |p| {
            let s = p.parse::<&str>()?;
            let mut ch = s.chars();
            let ret = match ch.next() {
                Some(c) => c,
                None => return Err(p.error("empty string")),
            };
            if ch.next().is_some() {
                return Err(p.error("more than one character"));
            }
            Ok(Char(ret))
        }),
        ("str.const", |p| Ok(String(p.parse()?))),
        ("list.const", |p| {
            let mut ret = Vec::new();
            while !p.is_empty() {
                ret.push(p.parens(|p| p.parse())?);
            }
            Ok(List(ret))
        }),
        ("record.const", |p| {
            let mut ret = Vec::new();
            while !p.is_empty() {
                ret.push(p.parens(|p| {
                    p.parse::<kw::field>()?;
                    Ok((p.parse()?, p.parse()?))
                })?);
            }
            Ok(Record(ret))
        }),
        ("tuple.const", |p| {
            let mut ret = Vec::new();
            while !p.is_empty() {
                ret.push(p.parens(|p| p.parse())?);
            }
            Ok(Tuple(ret))
        }),
        ("variant.const", |p| {
            let name = p.parse()?;
            let payload = Box::new(p.parse()?);
            Ok(Variant(name, payload))
        }),
        ("enum.const", |p| Ok(Enum(p.parse()?))),
        ("union.const", |p| {
            let num = p.parse()?;
            let payload = Box::new(p.parse()?);
            Ok(Union(num, payload))
        }),
        ("option.none", |_| Ok(Option(None))),
        ("option.some", |p| Ok(Option(Some(Box::new(p.parse()?))))),
        ("expected.ok", |p| Ok(Expected(Ok(Box::new(p.parse()?))))),
        ("expected.err", |p| Ok(Expected(Err(Box::new(p.parse()?))))),
        ("flags.const", |p| {
            let mut ret = Vec::new();
            while !p.is_empty() {
                ret.push(p.parse()?);
            }
            Ok(Flags(ret))
        }),
    ]
};

impl<'a> Parse<'a> for WastVal<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let parse = parser.step(|c| {
            if let Some((kw, rest)) = c.keyword() {
                if let Some(i) = CASES.iter().position(|(name, _)| *name == kw) {
                    return Ok((CASES[i].1, rest));
                }
            }
            Err(c.error("expected a [type].const expression"))
        })?;
        parse(parser)
    }
}

impl Peek for WastVal<'_> {
    fn peek(cursor: Cursor<'_>) -> bool {
        let kw = match cursor.keyword() {
            Some((kw, _)) => kw,
            None => return false,
        };
        CASES.iter().find(|(name, _)| *name == kw).is_some()
    }

    fn display() -> &'static str {
        "core wasm argument"
    }
}
