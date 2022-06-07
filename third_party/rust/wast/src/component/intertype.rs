use crate::kw;
use crate::parser::{Parse, Parser, Result};
use crate::token::*;

/// An interface-types type.
#[allow(missing_docs)]
#[derive(Debug, Clone)]
pub enum Primitive {
    Unit,
    Bool,
    S8,
    U8,
    S16,
    U16,
    S32,
    U32,
    S64,
    U64,
    Float32,
    Float64,
    Char,
    String,
}

impl<'a> Parse<'a> for Primitive {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut l = parser.lookahead1();
        if l.peek::<kw::unit>() {
            parser.parse::<kw::unit>()?;
            Ok(Primitive::Unit)
        } else if l.peek::<kw::bool_>() {
            parser.parse::<kw::bool_>()?;
            Ok(Primitive::Bool)
        } else if l.peek::<kw::s8>() {
            parser.parse::<kw::s8>()?;
            Ok(Primitive::S8)
        } else if l.peek::<kw::u8>() {
            parser.parse::<kw::u8>()?;
            Ok(Primitive::U8)
        } else if l.peek::<kw::s16>() {
            parser.parse::<kw::s16>()?;
            Ok(Primitive::S16)
        } else if l.peek::<kw::u16>() {
            parser.parse::<kw::u16>()?;
            Ok(Primitive::U16)
        } else if l.peek::<kw::s32>() {
            parser.parse::<kw::s32>()?;
            Ok(Primitive::S32)
        } else if l.peek::<kw::u32>() {
            parser.parse::<kw::u32>()?;
            Ok(Primitive::U32)
        } else if l.peek::<kw::s64>() {
            parser.parse::<kw::s64>()?;
            Ok(Primitive::S64)
        } else if l.peek::<kw::u64>() {
            parser.parse::<kw::u64>()?;
            Ok(Primitive::U64)
        } else if l.peek::<kw::float32>() {
            parser.parse::<kw::float32>()?;
            Ok(Primitive::Float32)
        } else if l.peek::<kw::float64>() {
            parser.parse::<kw::float64>()?;
            Ok(Primitive::Float64)
        } else if l.peek::<kw::char>() {
            parser.parse::<kw::char>()?;
            Ok(Primitive::Char)
        } else if l.peek::<kw::string>() {
            parser.parse::<kw::string>()?;
            Ok(Primitive::String)
        } else {
            Err(l.error())
        }
    }
}

/// An interface-types type.
#[allow(missing_docs)]
#[derive(Debug, Clone)]
pub enum InterType<'a> {
    Primitive(Primitive),
    Record(Record<'a>),
    Variant(Variant<'a>),
    List(List<'a>),
    Tuple(Tuple<'a>),
    Flags(Flags<'a>),
    Enum(Enum<'a>),
    Union(Union<'a>),
    Option(OptionType<'a>),
    Expected(Expected<'a>),
}

impl<'a> Parse<'a> for InterType<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        if parser.peek::<LParen>() {
            parser.parens(|parser| {
                if parser.peek::<kw::record>() {
                    let record = parser.parse()?;
                    Ok(InterType::Record(record))
                } else if parser.peek::<kw::variant>() {
                    let variant = parser.parse()?;
                    Ok(InterType::Variant(variant))
                } else if parser.peek::<kw::list>() {
                    let list = parser.parse()?;
                    Ok(InterType::List(list))
                } else if parser.peek::<kw::tuple>() {
                    let tuple = parser.parse()?;
                    Ok(InterType::Tuple(tuple))
                } else if parser.peek::<kw::flags>() {
                    let flags = parser.parse()?;
                    Ok(InterType::Flags(flags))
                } else if parser.peek::<kw::enum_>() {
                    let enum_ = parser.parse()?;
                    Ok(InterType::Enum(enum_))
                } else if parser.peek::<kw::union>() {
                    let union = parser.parse()?;
                    Ok(InterType::Union(union))
                } else if parser.peek::<kw::option>() {
                    let optional = parser.parse()?;
                    Ok(InterType::Option(optional))
                } else if parser.peek::<kw::expected>() {
                    let expected = parser.parse()?;
                    Ok(InterType::Expected(expected))
                } else {
                    Err(parser.error("expected derived intertype"))
                }
            })
        } else {
            Ok(InterType::Primitive(parser.parse()?))
        }
    }
}

/// An interface-types type.
#[allow(missing_docs)]
#[derive(Debug, Clone)]
pub enum InterTypeRef<'a> {
    Primitive(Primitive),
    Inline(InterType<'a>),
    Ref(Index<'a>),
}

impl<'a> Parse<'a> for InterTypeRef<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        if parser.peek::<Index<'_>>() {
            Ok(InterTypeRef::Ref(parser.parse()?))
        } else if parser.peek::<LParen>() {
            Ok(InterTypeRef::Inline(parser.parse()?))
        } else {
            Ok(InterTypeRef::Primitive(parser.parse()?))
        }
    }
}

/// An interface-types record, aka a struct.
#[derive(Debug, Clone)]
pub struct Record<'a> {
    /// The fields of the struct.
    pub fields: Vec<Field<'a>>,
}

impl<'a> Parse<'a> for Record<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::record>()?;
        let mut fields = Vec::new();
        while !parser.is_empty() {
            fields.push(parser.parens(|p| p.parse())?);
        }
        Ok(Record { fields })
    }
}

/// An interface-types record field.
#[derive(Debug, Clone)]
pub struct Field<'a> {
    /// The name of the field.
    pub name: &'a str,
    /// The type of the field.
    pub type_: InterTypeRef<'a>,
}

impl<'a> Parse<'a> for Field<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::field>()?;
        Ok(Field {
            name: parser.parse()?,
            type_: parser.parse()?,
        })
    }
}

/// An interface-types variant, aka a discriminated union with named arms.
#[derive(Debug, Clone)]
pub struct Variant<'a> {
    /// The cases of the variant type.
    pub cases: Vec<Case<'a>>,
}

impl<'a> Parse<'a> for Variant<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::variant>()?;
        let mut cases = Vec::new();
        while !parser.is_empty() {
            cases.push(parser.parens(|p| p.parse())?);
        }
        Ok(Variant { cases })
    }
}

/// An interface-types variant case.
#[derive(Debug, Clone)]
pub struct Case<'a> {
    /// The name of the case.
    pub name: &'a str,
    /// Where this `component` was defined
    pub span: Span,
    /// The type of the case.
    pub type_: InterTypeRef<'a>,
    /// The optional defaults-to name.
    pub defaults_to: Option<&'a str>,
}

impl<'a> Parse<'a> for Case<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::case>()?.0;
        let name = parser.parse()?;
        let type_ = parser.parse()?;
        let defaults_to = if !parser.is_empty() {
            Some(parser.parens(|parser| {
                parser.parse::<kw::defaults_to>()?;
                Ok(parser.parse()?)
            })?)
        } else {
            None
        };
        Ok(Case {
            name,
            span,
            type_,
            defaults_to,
        })
    }
}

/// An interface-types list, aka a fixed-size array.
#[derive(Debug, Clone)]
pub struct List<'a> {
    /// The element type of the array.
    pub element: Box<InterTypeRef<'a>>,
}

impl<'a> Parse<'a> for List<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::list>()?;
        let ty = parser.parse()?;
        Ok(List {
            element: Box::new(ty),
        })
    }
}

/// An interface-types tuple, aka a record with anonymous fields.
#[derive(Debug, Clone)]
pub struct Tuple<'a> {
    /// The types of the fields of the tuple.
    pub fields: Vec<InterTypeRef<'a>>,
}

impl<'a> Parse<'a> for Tuple<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::tuple>()?;
        let mut fields = Vec::new();
        while !parser.is_empty() {
            fields.push(parser.parse()?);
        }
        Ok(Tuple { fields })
    }
}

/// An interface-types flags, aka a fixed-sized bitfield with named fields.
#[derive(Debug, Clone)]
pub struct Flags<'a> {
    /// The names of the individual flags.
    pub flag_names: Vec<&'a str>,
}

impl<'a> Parse<'a> for Flags<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::flags>()?;
        let mut flag_names = Vec::new();
        while !parser.is_empty() {
            flag_names.push(parser.parse()?);
        }
        Ok(Flags { flag_names })
    }
}

/// An interface-types enum, aka a discriminated union with unit arms.
#[derive(Debug, Clone)]
pub struct Enum<'a> {
    /// The arms of the enum.
    pub arms: Vec<&'a str>,
}

impl<'a> Parse<'a> for Enum<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::enum_>()?;
        let mut arms = Vec::new();
        while !parser.is_empty() {
            arms.push(parser.parse()?);
        }
        Ok(Enum { arms })
    }
}

/// An interface-types union, aka a discriminated union with anonymous arms.
#[derive(Debug, Clone)]
pub struct Union<'a> {
    /// The arms of the union.
    pub arms: Vec<InterTypeRef<'a>>,
}

impl<'a> Parse<'a> for Union<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::union>()?;
        let mut arms = Vec::new();
        while !parser.is_empty() {
            arms.push(parser.parse()?);
        }
        Ok(Union { arms })
    }
}

/// An interface-types optional, aka an option.
#[derive(Debug, Clone)]
pub struct OptionType<'a> {
    /// The type of the value, when a value is present.
    pub element: Box<InterTypeRef<'a>>,
}

impl<'a> Parse<'a> for OptionType<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::option>()?;
        let ty = parser.parse()?;
        Ok(OptionType {
            element: Box::new(ty),
        })
    }
}

/// An interface-types expected, aka an result.
#[derive(Debug, Clone)]
pub struct Expected<'a> {
    /// The type on success.
    pub ok: Box<InterTypeRef<'a>>,
    /// The type on failure.
    pub err: Box<InterTypeRef<'a>>,
}

impl<'a> Parse<'a> for Expected<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::expected>()?;
        let ok = parser.parse()?;
        let err = parser.parse()?;
        Ok(Expected {
            ok: Box::new(ok),
            err: Box::new(err),
        })
    }
}
