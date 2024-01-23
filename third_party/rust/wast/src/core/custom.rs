use crate::parser::{Parse, Parser, Result};
use crate::token::{self, Span};
use crate::{annotation, kw};

/// A custom section within a wasm module.
#[derive(Debug)]
pub enum Custom<'a> {
    /// A raw custom section with the manual placement and bytes specified.
    Raw(RawCustomSection<'a>),
    /// A producers custom section.
    Producers(Producers<'a>),
    /// The `dylink.0` custom section
    Dylink0(Dylink0<'a>),
}

impl Custom<'_> {
    /// Where this custom section is placed.
    pub fn place(&self) -> CustomPlace {
        match self {
            Custom::Raw(s) => s.place,
            Custom::Producers(_) => CustomPlace::AfterLast,
            Custom::Dylink0(_) => CustomPlace::BeforeFirst,
        }
    }

    /// The name of this custom section
    pub fn name(&self) -> &str {
        match self {
            Custom::Raw(s) => s.name,
            Custom::Producers(_) => "producers",
            Custom::Dylink0(_) => "dylink.0",
        }
    }
}

impl<'a> Parse<'a> for Custom<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        if parser.peek::<annotation::producers>()? {
            Ok(Custom::Producers(parser.parse()?))
        } else if parser.peek::<annotation::dylink_0>()? {
            Ok(Custom::Dylink0(parser.parse()?))
        } else {
            Ok(Custom::Raw(parser.parse()?))
        }
    }
}

/// A wasm custom section within a module.
#[derive(Debug)]
pub struct RawCustomSection<'a> {
    /// Where this `@custom` was defined.
    pub span: Span,

    /// Name of the custom section.
    pub name: &'a str,

    /// Where the custom section is being placed,
    pub place: CustomPlace,

    /// Payload of this custom section.
    pub data: Vec<&'a [u8]>,
}

/// Possible locations to place a custom section within a module.
#[derive(Debug, PartialEq, Copy, Clone)]
pub enum CustomPlace {
    /// This custom section will appear before the first section in the module.
    BeforeFirst,
    /// This custom section will be placed just before a known section.
    Before(CustomPlaceAnchor),
    /// This custom section will be placed just after a known section.
    After(CustomPlaceAnchor),
    /// This custom section will appear after the last section in the module.
    AfterLast,
}

/// Known sections that custom sections can be placed relative to.
#[derive(Debug, PartialEq, Eq, Copy, Clone)]
#[allow(missing_docs)]
pub enum CustomPlaceAnchor {
    Type,
    Import,
    Func,
    Table,
    Memory,
    Global,
    Export,
    Start,
    Elem,
    Code,
    Data,
    Tag,
}

impl<'a> Parse<'a> for RawCustomSection<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<annotation::custom>()?.0;
        let name = parser.parse()?;
        let place = if parser.peek::<token::LParen>()? {
            parser.parens(|p| p.parse())?
        } else {
            CustomPlace::AfterLast
        };
        let mut data = Vec::new();
        while !parser.is_empty() {
            data.push(parser.parse()?);
        }
        Ok(RawCustomSection {
            span,
            name,
            place,
            data,
        })
    }
}

impl<'a> Parse<'a> for CustomPlace {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut l = parser.lookahead1();
        let ctor = if l.peek::<kw::before>()? {
            parser.parse::<kw::before>()?;
            if l.peek::<kw::first>()? {
                parser.parse::<kw::first>()?;
                return Ok(CustomPlace::BeforeFirst);
            }
            CustomPlace::Before as fn(CustomPlaceAnchor) -> _
        } else if l.peek::<kw::after>()? {
            parser.parse::<kw::after>()?;
            if l.peek::<kw::last>()? {
                parser.parse::<kw::last>()?;
                return Ok(CustomPlace::AfterLast);
            }
            CustomPlace::After
        } else {
            return Err(l.error());
        };
        Ok(ctor(parser.parse()?))
    }
}

impl<'a> Parse<'a> for CustomPlaceAnchor {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        if parser.peek::<kw::r#type>()? {
            parser.parse::<kw::r#type>()?;
            return Ok(CustomPlaceAnchor::Type);
        }
        if parser.peek::<kw::import>()? {
            parser.parse::<kw::import>()?;
            return Ok(CustomPlaceAnchor::Import);
        }
        if parser.peek::<kw::func>()? {
            parser.parse::<kw::func>()?;
            return Ok(CustomPlaceAnchor::Func);
        }
        if parser.peek::<kw::table>()? {
            parser.parse::<kw::table>()?;
            return Ok(CustomPlaceAnchor::Table);
        }
        if parser.peek::<kw::memory>()? {
            parser.parse::<kw::memory>()?;
            return Ok(CustomPlaceAnchor::Memory);
        }
        if parser.peek::<kw::global>()? {
            parser.parse::<kw::global>()?;
            return Ok(CustomPlaceAnchor::Global);
        }
        if parser.peek::<kw::export>()? {
            parser.parse::<kw::export>()?;
            return Ok(CustomPlaceAnchor::Export);
        }
        if parser.peek::<kw::start>()? {
            parser.parse::<kw::start>()?;
            return Ok(CustomPlaceAnchor::Start);
        }
        if parser.peek::<kw::elem>()? {
            parser.parse::<kw::elem>()?;
            return Ok(CustomPlaceAnchor::Elem);
        }
        if parser.peek::<kw::code>()? {
            parser.parse::<kw::code>()?;
            return Ok(CustomPlaceAnchor::Code);
        }
        if parser.peek::<kw::data>()? {
            parser.parse::<kw::data>()?;
            return Ok(CustomPlaceAnchor::Data);
        }
        if parser.peek::<kw::tag>()? {
            parser.parse::<kw::tag>()?;
            return Ok(CustomPlaceAnchor::Tag);
        }

        Err(parser.error("expected a valid section name"))
    }
}

/// A producers custom section
#[allow(missing_docs)]
#[derive(Debug)]
pub struct Producers<'a> {
    pub fields: Vec<(&'a str, Vec<(&'a str, &'a str)>)>,
}

impl<'a> Parse<'a> for Producers<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<annotation::producers>()?.0;
        let mut languages = Vec::new();
        let mut sdks = Vec::new();
        let mut processed_by = Vec::new();
        while !parser.is_empty() {
            parser.parens(|parser| {
                let mut l = parser.lookahead1();
                let dst = if l.peek::<kw::language>()? {
                    parser.parse::<kw::language>()?;
                    &mut languages
                } else if l.peek::<kw::sdk>()? {
                    parser.parse::<kw::sdk>()?;
                    &mut sdks
                } else if l.peek::<kw::processed_by>()? {
                    parser.parse::<kw::processed_by>()?;
                    &mut processed_by
                } else {
                    return Err(l.error());
                };

                dst.push((parser.parse()?, parser.parse()?));
                Ok(())
            })?;
        }

        let mut fields = Vec::new();
        if !languages.is_empty() {
            fields.push(("language", languages));
        }
        if !sdks.is_empty() {
            fields.push(("sdk", sdks));
        }
        if !processed_by.is_empty() {
            fields.push(("processed-by", processed_by));
        }
        Ok(Producers { fields })
    }
}

/// A `dylink.0` custom section
#[allow(missing_docs)]
#[derive(Debug)]
pub struct Dylink0<'a> {
    pub subsections: Vec<Dylink0Subsection<'a>>,
}

/// Possible subsections of the `dylink.0` custom section
#[derive(Debug)]
#[allow(missing_docs)]
pub enum Dylink0Subsection<'a> {
    MemInfo {
        memory_size: u32,
        memory_align: u32,
        table_size: u32,
        table_align: u32,
    },
    Needed(Vec<&'a str>),
    ExportInfo(Vec<(&'a str, u32)>),
    ImportInfo(Vec<(&'a str, &'a str, u32)>),
}

impl<'a> Parse<'a> for Dylink0<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<annotation::dylink_0>()?.0;
        let mut ret = Dylink0 {
            subsections: Vec::new(),
        };
        while !parser.is_empty() {
            parser.parens(|p| ret.parse_next(p))?;
        }
        Ok(ret)
    }
}

impl<'a> Dylink0<'a> {
    fn parse_next(&mut self, parser: Parser<'a>) -> Result<()> {
        let mut l = parser.lookahead1();
        if l.peek::<kw::mem_info>()? {
            parser.parse::<kw::mem_info>()?;
            let mut memory_size = 0;
            let mut memory_align = 0;
            let mut table_size = 0;
            let mut table_align = 0;
            if parser.peek2::<kw::memory>()? {
                parser.parens(|p| {
                    p.parse::<kw::memory>()?;
                    memory_size = p.parse()?;
                    memory_align = p.parse()?;
                    Ok(())
                })?;
            }
            if parser.peek2::<kw::table>()? {
                parser.parens(|p| {
                    p.parse::<kw::table>()?;
                    table_size = p.parse()?;
                    table_align = p.parse()?;
                    Ok(())
                })?;
            }
            self.subsections.push(Dylink0Subsection::MemInfo {
                memory_size,
                memory_align,
                table_size,
                table_align,
            });
        } else if l.peek::<kw::needed>()? {
            parser.parse::<kw::needed>()?;
            let mut names = Vec::new();
            while !parser.is_empty() {
                names.push(parser.parse()?);
            }
            self.subsections.push(Dylink0Subsection::Needed(names));
        } else if l.peek::<kw::export_info>()? {
            parser.parse::<kw::export_info>()?;
            let name = parser.parse()?;
            let flags = parse_sym_flags(parser)?;
            match self.subsections.last_mut() {
                Some(Dylink0Subsection::ExportInfo(list)) => list.push((name, flags)),
                _ => self
                    .subsections
                    .push(Dylink0Subsection::ExportInfo(vec![(name, flags)])),
            }
        } else if l.peek::<kw::import_info>()? {
            parser.parse::<kw::import_info>()?;
            let module = parser.parse()?;
            let name = parser.parse()?;
            let flags = parse_sym_flags(parser)?;
            match self.subsections.last_mut() {
                Some(Dylink0Subsection::ImportInfo(list)) => list.push((module, name, flags)),
                _ => self
                    .subsections
                    .push(Dylink0Subsection::ImportInfo(vec![(module, name, flags)])),
            }
        } else {
            return Err(l.error());
        }
        Ok(())
    }
}

fn parse_sym_flags(parser: Parser<'_>) -> Result<u32> {
    let mut flags = 0;
    while !parser.is_empty() {
        let mut l = parser.lookahead1();
        if l.peek::<u32>()? {
            flags |= parser.parse::<u32>()?;
            continue;
        }
        macro_rules! parse_flags {
            ($($kw:tt = $val:expr,)*) => {$({
                custom_keyword!(flag = $kw);
                if l.peek::<flag>()? {
                    parser.parse::<flag>()?;
                    flags |= $val;
                    continue;
                }
            })*};
        }
        parse_flags! {
            "binding-weak" = 1 << 0,
            "binding-local" = 1 << 1,
            "visibility-hidden" = 1 << 2,
            "undefined" = 1 << 4,
            "exported" = 1 << 5,
            "explicit-name" = 1 << 6,
            "no-strip" = 1 << 7,
        }
        return Err(l.error());
    }
    Ok(flags)
}

mod flag {}

impl Dylink0Subsection<'_> {
    /// Returns the byte id of this subsection used to identify it.
    pub fn id(&self) -> u8 {
        use Dylink0Subsection::*;
        match self {
            MemInfo { .. } => 1,
            Needed(..) => 2,
            ExportInfo(..) => 3,
            ImportInfo(..) => 4,
        }
    }
}
