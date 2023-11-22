use crate::component::WastVal;
use crate::core::{WastArgCore, WastRetCore};
use crate::kw;
use crate::parser::{self, Cursor, Parse, ParseBuffer, Parser, Peek, Result};
use crate::token::{Id, Span};
use crate::{Error, Wat};

/// A parsed representation of a `*.wast` file.
///
/// WAST files are not officially specified but are used in the official test
/// suite to write official spec tests for wasm. This type represents a parsed
/// `*.wast` file which parses a list of directives in a file.
#[derive(Debug)]
pub struct Wast<'a> {
    #[allow(missing_docs)]
    pub directives: Vec<WastDirective<'a>>,
}

impl<'a> Parse<'a> for Wast<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut directives = Vec::new();

        // If it looks like a directive token is in the stream then we parse a
        // bunch of directives, otherwise assume this is an inline module.
        if parser.peek2::<WastDirectiveToken>()? {
            while !parser.is_empty() {
                directives.push(parser.parens(|p| p.parse())?);
            }
        } else {
            let module = parser.parse::<Wat>()?;
            directives.push(WastDirective::Wat(QuoteWat::Wat(module)));
        }
        Ok(Wast { directives })
    }
}

struct WastDirectiveToken;

impl Peek for WastDirectiveToken {
    fn peek(cursor: Cursor<'_>) -> Result<bool> {
        let kw = match cursor.keyword()? {
            Some((kw, _)) => kw,
            None => return Ok(false),
        };
        Ok(kw.starts_with("assert_")
            || kw == "module"
            || kw == "component"
            || kw == "register"
            || kw == "invoke")
    }

    fn display() -> &'static str {
        unimplemented!()
    }
}

/// The different kinds of directives found in a `*.wast` file.
///
/// It's not entirely clear to me what all of these are per se, but they're only
/// really interesting to test harnesses mostly.
#[allow(missing_docs)]
#[derive(Debug)]
pub enum WastDirective<'a> {
    Wat(QuoteWat<'a>),
    AssertMalformed {
        span: Span,
        module: QuoteWat<'a>,
        message: &'a str,
    },
    AssertInvalid {
        span: Span,
        module: QuoteWat<'a>,
        message: &'a str,
    },
    Register {
        span: Span,
        name: &'a str,
        module: Option<Id<'a>>,
    },
    Invoke(WastInvoke<'a>),
    AssertTrap {
        span: Span,
        exec: WastExecute<'a>,
        message: &'a str,
    },
    AssertReturn {
        span: Span,
        exec: WastExecute<'a>,
        results: Vec<WastRet<'a>>,
    },
    AssertExhaustion {
        span: Span,
        call: WastInvoke<'a>,
        message: &'a str,
    },
    AssertUnlinkable {
        span: Span,
        module: Wat<'a>,
        message: &'a str,
    },
    AssertException {
        span: Span,
        exec: WastExecute<'a>,
    },
    Thread(WastThread<'a>),
    Wait {
        span: Span,
        thread: Id<'a>,
    },
}

impl WastDirective<'_> {
    /// Returns the location in the source that this directive was defined at
    pub fn span(&self) -> Span {
        match self {
            WastDirective::Wat(QuoteWat::Wat(Wat::Module(m))) => m.span,
            WastDirective::Wat(QuoteWat::Wat(Wat::Component(c))) => c.span,
            WastDirective::Wat(QuoteWat::QuoteModule(span, _)) => *span,
            WastDirective::Wat(QuoteWat::QuoteComponent(span, _)) => *span,
            WastDirective::AssertMalformed { span, .. }
            | WastDirective::Register { span, .. }
            | WastDirective::AssertTrap { span, .. }
            | WastDirective::AssertReturn { span, .. }
            | WastDirective::AssertExhaustion { span, .. }
            | WastDirective::AssertUnlinkable { span, .. }
            | WastDirective::AssertInvalid { span, .. }
            | WastDirective::AssertException { span, .. }
            | WastDirective::Wait { span, .. } => *span,
            WastDirective::Invoke(i) => i.span,
            WastDirective::Thread(t) => t.span,
        }
    }
}

impl<'a> Parse<'a> for WastDirective<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut l = parser.lookahead1();
        if l.peek::<kw::module>()? || l.peek::<kw::component>()? {
            Ok(WastDirective::Wat(parser.parse()?))
        } else if l.peek::<kw::assert_malformed>()? {
            let span = parser.parse::<kw::assert_malformed>()?.0;
            Ok(WastDirective::AssertMalformed {
                span,
                module: parser.parens(|p| p.parse())?,
                message: parser.parse()?,
            })
        } else if l.peek::<kw::assert_invalid>()? {
            let span = parser.parse::<kw::assert_invalid>()?.0;
            Ok(WastDirective::AssertInvalid {
                span,
                module: parser.parens(|p| p.parse())?,
                message: parser.parse()?,
            })
        } else if l.peek::<kw::register>()? {
            let span = parser.parse::<kw::register>()?.0;
            Ok(WastDirective::Register {
                span,
                name: parser.parse()?,
                module: parser.parse()?,
            })
        } else if l.peek::<kw::invoke>()? {
            Ok(WastDirective::Invoke(parser.parse()?))
        } else if l.peek::<kw::assert_trap>()? {
            let span = parser.parse::<kw::assert_trap>()?.0;
            Ok(WastDirective::AssertTrap {
                span,
                exec: parser.parens(|p| p.parse())?,
                message: parser.parse()?,
            })
        } else if l.peek::<kw::assert_return>()? {
            let span = parser.parse::<kw::assert_return>()?.0;
            let exec = parser.parens(|p| p.parse())?;
            let mut results = Vec::new();
            while !parser.is_empty() {
                results.push(parser.parens(|p| p.parse())?);
            }
            Ok(WastDirective::AssertReturn {
                span,
                exec,
                results,
            })
        } else if l.peek::<kw::assert_exhaustion>()? {
            let span = parser.parse::<kw::assert_exhaustion>()?.0;
            Ok(WastDirective::AssertExhaustion {
                span,
                call: parser.parens(|p| p.parse())?,
                message: parser.parse()?,
            })
        } else if l.peek::<kw::assert_unlinkable>()? {
            let span = parser.parse::<kw::assert_unlinkable>()?.0;
            Ok(WastDirective::AssertUnlinkable {
                span,
                module: parser.parens(parse_wat)?,
                message: parser.parse()?,
            })
        } else if l.peek::<kw::assert_exception>()? {
            let span = parser.parse::<kw::assert_exception>()?.0;
            Ok(WastDirective::AssertException {
                span,
                exec: parser.parens(|p| p.parse())?,
            })
        } else if l.peek::<kw::thread>()? {
            Ok(WastDirective::Thread(parser.parse()?))
        } else if l.peek::<kw::wait>()? {
            let span = parser.parse::<kw::wait>()?.0;
            Ok(WastDirective::Wait {
                span,
                thread: parser.parse()?,
            })
        } else {
            Err(l.error())
        }
    }
}

#[allow(missing_docs)]
#[derive(Debug)]
pub enum WastExecute<'a> {
    Invoke(WastInvoke<'a>),
    Wat(Wat<'a>),
    Get {
        module: Option<Id<'a>>,
        global: &'a str,
    },
}

impl<'a> Parse<'a> for WastExecute<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut l = parser.lookahead1();
        if l.peek::<kw::invoke>()? {
            Ok(WastExecute::Invoke(parser.parse()?))
        } else if l.peek::<kw::module>()? || l.peek::<kw::component>()? {
            Ok(WastExecute::Wat(parse_wat(parser)?))
        } else if l.peek::<kw::get>()? {
            parser.parse::<kw::get>()?;
            Ok(WastExecute::Get {
                module: parser.parse()?,
                global: parser.parse()?,
            })
        } else {
            Err(l.error())
        }
    }
}

fn parse_wat(parser: Parser) -> Result<Wat> {
    // Note that this doesn't use `Parse for Wat` since the `parser` provided
    // has already peeled back the first layer of parentheses while `Parse for
    // Wat` expects to be the top layer which means it also tries to peel off
    // the parens. Instead we can skip the sugar that `Wat` has for simply a
    // list of fields (no `(module ...)` container) and just parse the `Module`
    // itself.
    if parser.peek::<kw::component>()? {
        Ok(Wat::Component(parser.parse()?))
    } else {
        Ok(Wat::Module(parser.parse()?))
    }
}

#[allow(missing_docs)]
#[derive(Debug)]
pub struct WastInvoke<'a> {
    pub span: Span,
    pub module: Option<Id<'a>>,
    pub name: &'a str,
    pub args: Vec<WastArg<'a>>,
}

impl<'a> Parse<'a> for WastInvoke<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::invoke>()?.0;
        let module = parser.parse()?;
        let name = parser.parse()?;
        let mut args = Vec::new();
        while !parser.is_empty() {
            args.push(parser.parens(|p| p.parse())?);
        }
        Ok(WastInvoke {
            span,
            module,
            name,
            args,
        })
    }
}

#[allow(missing_docs)]
#[derive(Debug)]
pub enum QuoteWat<'a> {
    Wat(Wat<'a>),
    QuoteModule(Span, Vec<(Span, &'a [u8])>),
    QuoteComponent(Span, Vec<(Span, &'a [u8])>),
}

impl QuoteWat<'_> {
    /// Encodes this module to bytes, either by encoding the module directly or
    /// parsing the contents and then encoding it.
    pub fn encode(&mut self) -> Result<Vec<u8>, Error> {
        let (source, prefix) = match self {
            QuoteWat::Wat(m) => return m.encode(),
            QuoteWat::QuoteModule(_, source) => (source, None),
            QuoteWat::QuoteComponent(_, source) => (source, Some("(component")),
        };
        let mut ret = String::new();
        for (span, src) in source {
            match std::str::from_utf8(src) {
                Ok(s) => ret.push_str(s),
                Err(_) => {
                    return Err(Error::new(*span, "malformed UTF-8 encoding".to_string()));
                }
            }
            ret.push(' ');
        }
        if let Some(prefix) = prefix {
            ret.insert_str(0, prefix);
            ret.push(')');
        }
        let buf = ParseBuffer::new(&ret)?;
        let mut wat = parser::parse::<Wat<'_>>(&buf)?;
        wat.encode()
    }
}

impl<'a> Parse<'a> for QuoteWat<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        if parser.peek2::<kw::quote>()? {
            let ctor = if parser.peek::<kw::component>()? {
                parser.parse::<kw::component>()?;
                QuoteWat::QuoteComponent
            } else {
                parser.parse::<kw::module>()?;
                QuoteWat::QuoteModule
            };
            let span = parser.parse::<kw::quote>()?.0;
            let mut src = Vec::new();
            while !parser.is_empty() {
                let span = parser.cur_span();
                let string = parser.parse()?;
                src.push((span, string));
            }
            Ok(ctor(span, src))
        } else {
            Ok(QuoteWat::Wat(parse_wat(parser)?))
        }
    }
}

#[derive(Debug)]
#[allow(missing_docs)]
pub enum WastArg<'a> {
    Core(WastArgCore<'a>),
    Component(WastVal<'a>),
}

impl<'a> Parse<'a> for WastArg<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        if parser.peek::<WastArgCore<'_>>()? {
            Ok(WastArg::Core(parser.parse()?))
        } else {
            Ok(WastArg::Component(parser.parse()?))
        }
    }
}

#[derive(Debug)]
#[allow(missing_docs)]
pub enum WastRet<'a> {
    Core(WastRetCore<'a>),
    Component(WastVal<'a>),
}

impl<'a> Parse<'a> for WastRet<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        if parser.peek::<WastRetCore<'_>>()? {
            Ok(WastRet::Core(parser.parse()?))
        } else {
            Ok(WastRet::Component(parser.parse()?))
        }
    }
}

#[derive(Debug)]
#[allow(missing_docs)]
pub struct WastThread<'a> {
    pub span: Span,
    pub name: Id<'a>,
    pub shared_module: Option<Id<'a>>,
    pub directives: Vec<WastDirective<'a>>,
}

impl<'a> Parse<'a> for WastThread<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.depth_check()?;
        let span = parser.parse::<kw::thread>()?.0;
        let name = parser.parse()?;

        let shared_module = if parser.peek2::<kw::shared>()? {
            let name = parser.parens(|p| {
                p.parse::<kw::shared>()?;
                p.parens(|p| {
                    p.parse::<kw::module>()?;
                    p.parse()
                })
            })?;
            Some(name)
        } else {
            None
        };
        let mut directives = Vec::new();
        while !parser.is_empty() {
            directives.push(parser.parens(|p| p.parse())?);
        }
        Ok(WastThread {
            span,
            name,
            shared_module,
            directives,
        })
    }
}
