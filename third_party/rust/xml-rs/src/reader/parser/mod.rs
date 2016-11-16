//! Contains an implementation of pull-based XML parser.

use std::mem;
use std::borrow::Cow;
use std::io::prelude::*;

use common::{
    self,
    XmlVersion, Position, TextPosition,
    is_name_start_char, is_name_char,
};
use name::OwnedName;
use attribute::OwnedAttribute;
use namespace::NamespaceStack;

use reader::events::XmlEvent;
use reader::config::ParserConfig;
use reader::lexer::{Lexer, Token};

macro_rules! gen_takes(
    ($($field:ident -> $method:ident, $t:ty, $def:expr);+) => (
        $(
        impl MarkupData {
            #[inline]
            fn $method(&mut self) -> $t {
                mem::replace(&mut self.$field, $def)
            }
        }
        )+
    )
);

gen_takes!(
    name         -> take_name, String, String::new();
    ref_data     -> take_ref_data, String, String::new();

    version      -> take_version, Option<common::XmlVersion>, None;
    encoding     -> take_encoding, Option<String>, None;
    standalone   -> take_standalone, Option<bool>, None;

    element_name -> take_element_name, Option<OwnedName>, None;

    attr_name    -> take_attr_name, Option<OwnedName>, None;
    attributes   -> take_attributes, Vec<OwnedAttribute>, vec!()
);

macro_rules! self_error(
    ($this:ident; $msg:expr) => ($this.error($msg));
    ($this:ident; $fmt:expr, $($arg:expr),+) => ($this.error(format!($fmt, $($arg),+)))
);

mod outside_tag;
mod inside_processing_instruction;
mod inside_declaration;
mod inside_doctype;
mod inside_opening_tag;
mod inside_closing_tag_name;
mod inside_comment;
mod inside_cdata;
mod inside_reference;

static DEFAULT_VERSION: XmlVersion      = XmlVersion::Version10;
static DEFAULT_ENCODING: &'static str   = "UTF-8";
static DEFAULT_STANDALONE: Option<bool> = None;

type ElementStack = Vec<OwnedName>;
pub type Result = super::Result<XmlEvent>;

/// Pull-based XML parser.
pub struct PullParser {
    config: ParserConfig,
    lexer: Lexer,
    st: State,
    buf: String,
    nst: NamespaceStack,

    data: MarkupData,
    final_result: Option<Result>,
    next_event: Option<Result>,
    est: ElementStack,
    pos: Vec<TextPosition>,

    encountered_element: bool,
    parsed_declaration: bool,
    inside_whitespace: bool,
    read_prefix_separator: bool,
    pop_namespace: bool
}

impl PullParser {
    /// Returns a new parser using the given config.
    pub fn new(config: ParserConfig) -> PullParser {
        PullParser {
            config: config,
            lexer: Lexer::new(),
            st: State::OutsideTag,
            buf: String::new(),
            nst: NamespaceStack::default(),

            data: MarkupData {
                name: String::new(),
                version: None,
                encoding: None,
                standalone: None,
                ref_data: String::new(),
                element_name: None,
                quote: None,
                attr_name: None,
                attributes: Vec::new()
            },
            final_result: None,
            next_event: None,
            est: Vec::new(),
            pos: vec![TextPosition::new()],

            encountered_element: false,
            parsed_declaration: false,
            inside_whitespace: true,
            read_prefix_separator: false,
            pop_namespace: false
        }
    }
}

impl Position for PullParser {
    /// Returns the position of the last event produced by the parser
    #[inline]
    fn position(&self) -> TextPosition {
        self.pos[0]
    }
}

#[derive(Clone, PartialEq)]
pub enum State {
    OutsideTag,
    InsideOpeningTag(OpeningTagSubstate),
    InsideClosingTag(ClosingTagSubstate),
    InsideProcessingInstruction(ProcessingInstructionSubstate),
    InsideComment,
    InsideCData,
    InsideDeclaration(DeclarationSubstate),
    InsideDoctype,
    InsideReference(Box<State>)
}

#[derive(Clone, PartialEq)]
pub enum OpeningTagSubstate {
    InsideName,

    InsideTag,

    InsideAttributeName,
    AfterAttributeName,

    InsideAttributeValue,
}

#[derive(Clone, PartialEq)]
pub enum ClosingTagSubstate {
    CTInsideName,
    CTAfterName
}

#[derive(Clone, PartialEq)]
pub enum ProcessingInstructionSubstate {
    PIInsideName,
    PIInsideData
}

#[derive(Clone, PartialEq)]
pub enum DeclarationSubstate {
    BeforeVersion,
    InsideVersion,
    AfterVersion,

    InsideVersionValue,
    AfterVersionValue,

    InsideEncoding,
    AfterEncoding,

    InsideEncodingValue,

    BeforeStandaloneDecl,
    InsideStandaloneDecl,
    AfterStandaloneDecl,

    InsideStandaloneDeclValue,
    AfterStandaloneDeclValue
}

#[derive(PartialEq)]
enum QualifiedNameTarget {
    AttributeNameTarget,
    OpeningTagNameTarget,
    ClosingTagNameTarget
}

#[derive(Copy, Clone, PartialEq, Eq)]
enum QuoteToken {
    SingleQuoteToken,
    DoubleQuoteToken
}

impl QuoteToken {
    fn from_token(t: &Token) -> QuoteToken {
        match *t {
            Token::SingleQuote => QuoteToken::SingleQuoteToken,
            Token::DoubleQuote => QuoteToken::DoubleQuoteToken,
            _ => panic!("Unexpected token: {}", t)
        }
    }

    fn as_token(self) -> Token {
        match self {
            QuoteToken::SingleQuoteToken => Token::SingleQuote,
            QuoteToken::DoubleQuoteToken => Token::DoubleQuote
        }
    }
}

struct MarkupData {
    name: String,     // used for processing instruction name
    ref_data: String,  // used for reference content

    version: Option<common::XmlVersion>,  // used for XML declaration version
    encoding: Option<String>,  // used for XML declaration encoding
    standalone: Option<bool>,  // used for XML declaration standalone parameter

    element_name: Option<OwnedName>,  // used for element name

    quote: Option<QuoteToken>,  // used to hold opening quote for attribute value
    attr_name: Option<OwnedName>,  // used to hold attribute name
    attributes: Vec<OwnedAttribute>   // used to hold all accumulated attributes
}

impl PullParser {
    /// Returns next event read from the given buffer.
    ///
    /// This method should be always called with the same buffer. If you call it
    /// providing different buffers each time, the result will be undefined.
    pub fn next<R: Read>(&mut self, r: &mut R) -> Result {
        if let Some(ref ev) = self.final_result {
            return ev.clone();
        }

        if let Some(ev) = self.next_event.take() {
            return ev;
        }

        if self.pop_namespace {
            self.pop_namespace = false;
            self.nst.pop();
        }

        loop {
            // While lexer gives us Ok(maybe_token) -- we loop.
            // Upon having a complete XML-event -- we return from the whole function.
            match self.lexer.next_token(r) {
                Ok(maybe_token) =>
                    match maybe_token {
                        None => break,
                        Some(token) =>
                            match self.dispatch_token(token) {
                                None => {} // continue
                                Some(Ok(XmlEvent::EndDocument)) =>
                                    return {
                                        self.next_pos();
                                        self.set_final_result(Ok(XmlEvent::EndDocument))
                                    },
                                Some(Ok(xml_event)) =>
                                    return {
                                        self.next_pos();
                                        Ok(xml_event)
                                    },
                                Some(Err(xml_error)) =>
                                    return {
                                        self.next_pos();
                                        self.set_final_result(Err(xml_error))
                                    },
                            }
                    },
                Err(lexer_error) =>
                    return self.set_final_result(Err(lexer_error)),
            }
        }

        // Handle end of stream
        // Forward pos to the lexer head
        self.next_pos();
        let ev = if self.depth() == 0 {
            if self.encountered_element && self.st == State::OutsideTag {  // all is ok
                Ok(XmlEvent::EndDocument)
            } else if !self.encountered_element {
                self_error!(self; "Unexpected end of stream: no root element found")
            } else {  // self.st != State::OutsideTag
                self_error!(self; "Unexpected end of stream")  // TODO: add expected hint?
            }
        } else {
            self_error!(self; "Unexpected end of stream: still inside the root element")
        };
        self.set_final_result(ev)
    }

    // This function is to be called when a terminal event is reached.
    // The function sets up the `self.final_result` into `Some(result)` and return `result`.
    fn set_final_result(&mut self, result: Result) -> Result {
        self.final_result = Some(result.clone());
        result
    }

    #[inline]
    fn error<M: Into<Cow<'static, str>>>(&self, msg: M) -> Result {
        Err((&self.lexer, msg).into())
    }

    #[inline]
    fn next_pos(&mut self) {
        if self.pos.len() > 1 {
            self.pos.remove(0);
        } else {
            self.pos[0] = self.lexer.position();
        }
    }

    #[inline]
    fn push_pos(&mut self) {
        self.pos.push(self.lexer.position());
    }

    fn dispatch_token(&mut self, t: Token) -> Option<Result> {
        match self.st.clone() {
            State::OutsideTag                     => self.outside_tag(t),
            State::InsideProcessingInstruction(s) => self.inside_processing_instruction(t, s),
            State::InsideDeclaration(s)           => self.inside_declaration(t, s),
            State::InsideDoctype                  => self.inside_doctype(t),
            State::InsideOpeningTag(s)            => self.inside_opening_tag(t, s),
            State::InsideClosingTag(s)            => self.inside_closing_tag_name(t, s),
            State::InsideComment                  => self.inside_comment(t),
            State::InsideCData                    => self.inside_cdata(t),
            State::InsideReference(s)             => self.inside_reference(t, *s)
        }
    }

    #[inline]
    fn depth(&self) -> usize {
        self.est.len()
    }

    #[inline]
    fn buf_has_data(&self) -> bool {
        self.buf.len() > 0
    }

    #[inline]
    fn take_buf(&mut self) -> String {
        mem::replace(&mut self.buf, String::new())
    }

    #[inline]
    fn append_char_continue(&mut self, c: char) -> Option<Result> {
        self.buf.push(c);
        None
    }

    #[inline]
    fn into_state(&mut self, st: State, ev: Option<Result>) -> Option<Result> {
        self.st = st;
        ev
    }

    #[inline]
    fn into_state_continue(&mut self, st: State) -> Option<Result> {
        self.into_state(st, None)
    }

    #[inline]
    fn into_state_emit(&mut self, st: State, ev: Result) -> Option<Result> {
        self.into_state(st, Some(ev))
    }

    /// Dispatches tokens in order to process qualified name. If qualified name cannot be parsed,
    /// an error is returned.
    ///
    /// # Parameters
    /// * `t`       --- next token;
    /// * `on_name` --- a callback which is executed when whitespace is encountered.
    fn read_qualified_name<F>(&mut self, t: Token, target: QualifiedNameTarget, on_name: F) -> Option<Result>
      where F: Fn(&mut PullParser, Token, OwnedName) -> Option<Result> {
        // We can get here for the first time only when self.data.name contains zero or one character,
        // but first character cannot be a colon anyway
        if self.buf.len() <= 1 {
            self.read_prefix_separator = false;
        }

        let invoke_callback = |this: &mut PullParser, t| {
            let name = this.take_buf();
            match name.parse() {
                Ok(name) => on_name(this, t, name),
                Err(_) => Some(self_error!(this; "Qualified name is invalid: {}", name))
            }
        };

        match t {
            // There can be only one colon, and not as the first character
            Token::Character(':') if self.buf_has_data() && !self.read_prefix_separator => {
                self.buf.push(':');
                self.read_prefix_separator = true;
                None
            }

            Token::Character(c) if c != ':' && (!self.buf_has_data() && is_name_start_char(c) ||
                                          self.buf_has_data() && is_name_char(c)) =>
                self.append_char_continue(c),

            Token::EqualsSign if target == QualifiedNameTarget::AttributeNameTarget => invoke_callback(self, t),

            Token::EmptyTagEnd if target == QualifiedNameTarget::OpeningTagNameTarget => invoke_callback(self, t),

            Token::TagEnd if target == QualifiedNameTarget::OpeningTagNameTarget ||
                      target == QualifiedNameTarget::ClosingTagNameTarget => invoke_callback(self, t),

            Token::Whitespace(_) => invoke_callback(self, t),

            _ => Some(self_error!(self; "Unexpected token inside qualified name: {}", t))
        }
    }

    /// Dispatches tokens in order to process attribute value.
    ///
    /// # Parameters
    /// * `t`        --- next token;
    /// * `on_value` --- a callback which is called when terminating quote is encountered.
    fn read_attribute_value<F>(&mut self, t: Token, on_value: F) -> Option<Result>
      where F: Fn(&mut PullParser, String) -> Option<Result> {
        match t {
            Token::Whitespace(_) if self.data.quote.is_none() => None,  // skip leading whitespace

            Token::DoubleQuote | Token::SingleQuote => match self.data.quote {
                None => {  // Entered attribute value
                    self.data.quote = Some(QuoteToken::from_token(&t));
                    None
                }
                Some(q) if q.as_token() == t => {
                    self.data.quote = None;
                    let value = self.take_buf();
                    on_value(self, value)
                }
                _ => {
                    t.push_to_string(&mut self.buf);
                    None
                }
            },

            Token::ReferenceStart => {
                let st = Box::new(self.st.clone());
                self.into_state_continue(State::InsideReference(st))
            }

            Token::OpeningTagStart =>
                Some(self_error!(self; "Unexpected token inside attribute value: <")),

            // Every character except " and ' and < is okay
            _  => {
                t.push_to_string(&mut self.buf);
                None
            }
        }
    }

    fn emit_start_element(&mut self, emit_end_element: bool) -> Option<Result> {
        let mut name = self.data.take_element_name().unwrap();
        let mut attributes = self.data.take_attributes();

        // check whether the name prefix is bound and fix its namespace
        match self.nst.get(name.borrow().prefix_repr()) {
            Some("") => name.namespace = None,  // default namespace
            Some(ns) => name.namespace = Some(ns.into()),
            None => return Some(self_error!(self; "Element {} prefix is unbound", name))
        }

        // check and fix accumulated attributes prefixes
        for attr in attributes.iter_mut() {
            if let Some(ref pfx) = attr.name.prefix {
                let new_ns = match self.nst.get(pfx) {
                    Some("") => None,  // default namespace
                    Some(ns) => Some(ns.into()),
                    None => return Some(self_error!(self; "Attribute {} prefix is unbound", attr.name))
                };
                attr.name.namespace = new_ns;
            }
        }

        if emit_end_element {
            self.pop_namespace = true;
            self.next_event = Some(Ok(XmlEvent::EndElement {
                name: name.clone()
            }));
        } else {
            self.est.push(name.clone());
        }
        let namespace = self.nst.squash();
        self.into_state_emit(State::OutsideTag, Ok(XmlEvent::StartElement {
            name: name,
            attributes: attributes,
            namespace: namespace
        }))
    }

    fn emit_end_element(&mut self) -> Option<Result> {
        let mut name = self.data.take_element_name().unwrap();

        // check whether the name prefix is bound and fix its namespace
        match self.nst.get(name.borrow().prefix_repr()) {
            Some("") => name.namespace = None,  // default namespace
            Some(ns) => name.namespace = Some(ns.into()),
            None => return Some(self_error!(self; "Element {} prefix is unbound", name))
        }

        let op_name = self.est.pop().unwrap();

        if name == op_name {
            self.pop_namespace = true;
            self.into_state_emit(State::OutsideTag, Ok(XmlEvent::EndElement { name: name }))
        } else {
            Some(self_error!(self; "Unexpected closing tag: {}, expected {}", name, op_name))
        }
    }

}

#[cfg(test)]
mod tests {
    use std::io::BufReader;

    use common::{Position, TextPosition};
    use name::OwnedName;
    use attribute::OwnedAttribute;
    use reader::parser::PullParser;
    use reader::ParserConfig;
    use reader::events::XmlEvent;

    fn new_parser() -> PullParser {
        PullParser::new(ParserConfig::new())
    }

    macro_rules! expect_event(
        ($r:expr, $p:expr, $t:pat) => (
            match $p.next(&mut $r) {
                $t => {}
                e => panic!("Unexpected event: {:?}", e)
            }
        );
        ($r:expr, $p:expr, $t:pat [ $c:expr ]) => (
            match $p.next(&mut $r) {
                $t if $c => {}
                e => panic!("Unexpected event: {:?}", e)
            }
        )
    );

    macro_rules! test_data(
        ($d:expr) => ({
            static DATA: &'static str = $d;
            let r = BufReader::new(DATA.as_bytes());
            let p = new_parser();
            (r, p)
        })
    );

    #[test]
    fn issue_3_semicolon_in_attribute_value() {
        let (mut r, mut p) = test_data!(r#"
            <a attr="zzz;zzz" />
        "#);

        expect_event!(r, p, Ok(XmlEvent::StartDocument { .. }));
        expect_event!(r, p, Ok(XmlEvent::StartElement { ref name, ref attributes, ref namespace })
            [ *name == OwnedName::local("a") &&
               attributes.len() == 1 &&
               attributes[0] == OwnedAttribute::new(OwnedName::local("attr"), "zzz;zzz") &&
               namespace.is_essentially_empty()
            ]
        );
        expect_event!(r, p, Ok(XmlEvent::EndElement { ref name }) [ *name == OwnedName::local("a") ]);
        expect_event!(r, p, Ok(XmlEvent::EndDocument));
    }

    #[test]
    fn opening_tag_in_attribute_value() {
        let (mut r, mut p) = test_data!(r#"
            <a attr="zzz<zzz" />
        "#);

        expect_event!(r, p, Ok(XmlEvent::StartDocument { .. }));
        expect_event!(r, p, Err(ref e)
            [ e.msg() == "Unexpected token inside attribute value: <"
                && e.position() == TextPosition { row: 1, column: 24 } ]
        );
    }
}
