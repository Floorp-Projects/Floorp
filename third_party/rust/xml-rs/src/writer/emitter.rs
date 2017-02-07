use std::io;
use std::io::prelude::*;
use std::fmt;
use std::result;
use std::borrow::Cow;

use common;
use name::{Name, OwnedName};
use attribute::Attribute;
use escape::{escape_str_attribute, escape_str_pcdata};
use common::XmlVersion;
use namespace::{NamespaceStack, NS_NO_PREFIX, NS_EMPTY_URI, NS_XMLNS_PREFIX, NS_XML_PREFIX};

use writer::config::EmitterConfig;

/// An error which may be returned by `XmlWriter` when writing XML events.
#[derive(Debug)]
pub enum EmitterError {
    /// An I/O error occured in the underlying `Write` instance.
    Io(io::Error),

    /// Document declaration has already been written to the output stream.
    DocumentStartAlreadyEmitted,

    /// The name of the last opening element is not available.
    LastElementNameNotAvailable,

    /// The name of the last opening element is not equal to the name of the provided
    /// closing element.
    EndElementNameIsNotEqualToLastStartElementName,

    /// End element name is not specified when it is needed, for example, when automatic
    /// closing is not enabled in configuration.
    EndElementNameIsNotSpecified
}

impl From<io::Error> for EmitterError {
    fn from(err: io::Error) -> EmitterError {
        EmitterError::Io(err)
    }
}

impl fmt::Display for EmitterError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        try!(write!(f, "emitter error: "));
        match *self {
            EmitterError::Io(ref e) =>
                write!(f, "I/O error: {}", e),
            EmitterError::DocumentStartAlreadyEmitted =>
                write!(f, "document start event has already been emitted"),
            EmitterError::LastElementNameNotAvailable =>
                write!(f, "last element name is not available"),
            EmitterError::EndElementNameIsNotEqualToLastStartElementName =>
                write!(f, "end element name is not equal to last start element name"),
            EmitterError::EndElementNameIsNotSpecified =>
                write!(f, "end element name is not specified and can't be inferred")
        }
    }
}

/// A result type yielded by `XmlWriter`.
pub type Result<T> = result::Result<T, EmitterError>;

// TODO: split into a low-level fast writer without any checks and formatting logic and a
// high-level indenting validating writer
pub struct Emitter {
    config: EmitterConfig,

    nst: NamespaceStack,

    indent_level: usize,
    indent_stack: Vec<IndentFlags>,

    element_names: Vec<OwnedName>,

    start_document_emitted: bool,
    just_wrote_start_element: bool
}

impl Emitter {
    pub fn new(config: EmitterConfig) -> Emitter {
        Emitter {
            config: config,

            nst: NamespaceStack::empty(),

            indent_level: 0,
            indent_stack: vec!(IndentFlags::empty()),

            element_names: Vec::new(),

            start_document_emitted: false,
            just_wrote_start_element: false
        }
    }
}

macro_rules! try_chain(
    (ok_unit $e:expr) => ({try!($e); Ok(())});
    (ignore $e:expr) => (try!($e));
    ($e:expr) => (Ok(try!($e)));
    ($e:expr, $($rest:tt)*) => ({
        try!($e);
        try_chain!($($rest)*)
    })
);

macro_rules! wrapped_with(
    ($_self:ident; $before_name:ident ($arg:expr) and $after_name:ident, $body:expr) => ({
        try!($_self.$before_name($arg));
        let result = $body;
        $_self.$after_name();
        result
    })
);

macro_rules! if_present(
    ($opt:ident, $body:expr) => ($opt.map(|$opt| $body).unwrap_or(Ok(())))
);

bitflags!(
    flags IndentFlags: u8 {
        const WROTE_NOTHING = 0,
        const WROTE_MARKUP  = 1,
        const WROTE_TEXT    = 2
    }
);

impl Emitter {
    /// Returns the current state of namespaces.
    #[inline]
    pub fn namespace_stack_mut(&mut self) -> &mut NamespaceStack {
        &mut self.nst
    }

    #[inline]
    fn wrote_text(&self) -> bool {
        self.indent_stack.last().unwrap().contains(WROTE_TEXT)
    }

    #[inline]
    fn wrote_markup(&self) -> bool {
        self.indent_stack.last().unwrap().contains(WROTE_MARKUP)
    }

    #[inline]
    fn set_wrote_text(&mut self) {
        *self.indent_stack.last_mut().unwrap() = WROTE_TEXT;
    }

    #[inline]
    fn set_wrote_markup(&mut self) {
        *self.indent_stack.last_mut().unwrap() = WROTE_MARKUP;
    }

    #[inline]
    fn reset_state(&mut self) {
        *self.indent_stack.last_mut().unwrap() = WROTE_NOTHING;
    }

    fn write_newline<W: Write>(&mut self, target: &mut W, level: usize) -> Result<()> {
        try!(target.write(self.config.line_separator.as_bytes()));
        for _ in 0..level {
            try!(target.write(self.config.indent_string.as_bytes()));
        }
        Ok(())
    }

    fn before_markup<W: Write>(&mut self, target: &mut W) -> Result<()> {
        if self.config.perform_indent && !self.wrote_text() &&
           (self.indent_level > 0 || self.wrote_markup()) {
            let indent_level = self.indent_level;
            try!(self.write_newline(target, indent_level));
            if self.indent_level > 0 && self.config.indent_string.len() > 0 {
                self.after_markup();
            }
        }
        Ok(())
    }

    fn after_markup(&mut self) {
        self.set_wrote_markup();
    }

    fn before_start_element<W: Write>(&mut self, target: &mut W) -> Result<()> {
        try!(self.before_markup(target));
        self.indent_stack.push(WROTE_NOTHING);
        Ok(())
    }

    fn after_start_element(&mut self) {
        self.after_markup();
        self.indent_level += 1;
    }

    fn before_end_element<W: Write>(&mut self, target: &mut W) -> Result<()> {
        if self.config.perform_indent && self.indent_level > 0 && self.wrote_markup() &&
           !self.wrote_text() {
            let indent_level = self.indent_level;
            self.write_newline(target, indent_level - 1)
        } else {
            Ok(())
        }
    }

    fn after_end_element(&mut self) {
        if self.indent_level > 0 {
            self.indent_level -= 1;
            self.indent_stack.pop();
        }
        self.set_wrote_markup();
    }

    fn after_text(&mut self) {
        self.set_wrote_text();
    }

    pub fn emit_start_document<W: Write>(&mut self, target: &mut W,
                                         version: XmlVersion,
                                         encoding: &str,
                                         standalone: Option<bool>) -> Result<()> {
        if self.start_document_emitted {
            return Err(EmitterError::DocumentStartAlreadyEmitted);
        }
        self.start_document_emitted = true;

        wrapped_with!(self; before_markup(target) and after_markup,
            try_chain! {
                write!(target, "<?xml version=\"{}\" encoding=\"{}\"", version, encoding),

                if_present!(standalone,
                            write!(target, " standalone=\"{}\"",
                                   if standalone { "yes" } else { "no" })),

                write!(target, "?>")
            }
        )
    }

    fn check_document_started<W: Write>(&mut self, target: &mut W) -> Result<()> {
        if !self.start_document_emitted && self.config.write_document_declaration {
            self.emit_start_document(target, common::XmlVersion::Version10, "utf-8", None)
        } else {
            Ok(())
        }
    }

    fn fix_non_empty_element<W: Write>(&mut self, target: &mut W) -> Result<()> {
        if self.config.normalize_empty_elements && self.just_wrote_start_element {
            self.just_wrote_start_element = false;
            target.write(b">").map(|_| ()).map_err(From::from)
        } else {
            Ok(())
        }
    }

    pub fn emit_processing_instruction<W: Write>(&mut self,
                                                 target: &mut W,
                                                 name: &str,
                                                 data: Option<&str>) -> Result<()> {
        try!(self.check_document_started(target));
        try!(self.fix_non_empty_element(target));

        wrapped_with!(self; before_markup(target) and after_markup,
            try_chain! {
                write!(target, "<?{}", name),
                if_present!(data, write!(target, " {}", data)),
                write!(target, "?>")
            }
        )
    }

    fn emit_start_element_initial<W>(&mut self, target: &mut W,
                                     name: Name,
                                     attributes: &[Attribute]) -> Result<()>
        where W: Write
    {
        try_chain! {
            self.check_document_started(target),
            self.fix_non_empty_element(target),
            self.before_start_element(target),
            write!(target, "<{}", name.repr_display()),
            self.emit_current_namespace_attributes(target),
            ignore self.emit_attributes(target, attributes)
        }
        self.after_start_element();
        Ok(())
    }

    pub fn emit_start_element<W>(&mut self, target: &mut W,
                                 name: Name,
                                 attributes: &[Attribute]) -> Result<()>
        where W: Write
    {
        if self.config.keep_element_names_stack {
            self.element_names.push(name.to_owned());
        }
        try_chain! {
            self.emit_start_element_initial(target, name, attributes),
            {
                self.just_wrote_start_element = true;
                if self.config.normalize_empty_elements { Ok(()) }
                else { write!(target, ">") }
            }
        }
    }

    pub fn emit_current_namespace_attributes<W>(&mut self, target: &mut W) -> Result<()>
        where W: Write
    {
        for (prefix, uri) in self.nst.peek() {
            try!(match prefix {
                // internal namespaces are not emitted
                NS_XMLNS_PREFIX | NS_XML_PREFIX => Ok(()),
                //// there is already a namespace binding with this prefix in scope
                //prefix if self.nst.get(prefix) == Some(uri) => Ok(()),
                // emit xmlns only if it is overridden
                NS_NO_PREFIX => if uri != NS_EMPTY_URI {
                    write!(target, " xmlns=\"{}\"", uri)
                } else { Ok(()) },
                // everything else
                prefix => write!(target, " xmlns:{}=\"{}\"", prefix, uri)
            });
        }
        Ok(())
    }

    pub fn emit_attributes<W: Write>(&mut self, target: &mut W,
                                      attributes: &[Attribute]) -> Result<()> {
        for attr in attributes.iter() {
            try!(write!(
                target, " {}=\"{}\"",
                attr.name.repr_display(),
                if self.config.perform_escaping { escape_str_attribute(attr.value) }
                else { Cow::Borrowed(attr.value) }
            ))
        }
        Ok(())
    }

    pub fn emit_end_element<W: Write>(&mut self, target: &mut W,
                                      name: Option<Name>) -> Result<()> {
        let owned_name = if self.config.keep_element_names_stack {
            Some(try!(self.element_names.pop().ok_or(EmitterError::LastElementNameNotAvailable)))
        } else {
            None
        };

        // Check that last started element name equals to the provided name, if there are both
        if let Some(ref last_name) = owned_name {
            if let Some(ref name) = name {
                if last_name.borrow() != *name {
                    return Err(EmitterError::EndElementNameIsNotEqualToLastStartElementName);
                }
            }
        }

        if let Some(name) = owned_name.as_ref().map(|n| n.borrow()).or(name) {
            if self.config.normalize_empty_elements && self.just_wrote_start_element {
                self.just_wrote_start_element = false;
                // TODO: make this space configurable
                let result = target.write(b" />").map_err(From::from);
                self.after_end_element();
                result.map(|_| ())
            } else {
                self.just_wrote_start_element = false;
                wrapped_with!(self; before_end_element(target) and after_end_element,
                    write!(target, "</{}>", name.repr_display()).map_err(From::from)
                )
            }
        } else {
            Err(EmitterError::EndElementNameIsNotSpecified)
        }
    }

    pub fn emit_cdata<W: Write>(&mut self, target: &mut W, content: &str) -> Result<()> {
        try!(self.fix_non_empty_element(target));
        if self.config.cdata_to_characters {
            self.emit_characters(target, content)
        } else {
            // TODO: escape ']]>' characters in CDATA as two adjacent CDATA blocks
            try_chain! {
                target.write(b"<![CDATA["),
                target.write(content.as_bytes()),
                ignore target.write(b"]]>")
            };
            self.after_text();
            Ok(())
        }
    }

    pub fn emit_characters<W: Write>(&mut self, target: &mut W,
                                      content: &str) -> Result<()> {
        try!(self.fix_non_empty_element(target));
        try!(target.write(
            (if self.config.perform_escaping { escape_str_pcdata(content) }
            else { Cow::Borrowed(content) }).as_bytes()
        ));
        self.after_text();
        Ok(())
    }

    pub fn emit_comment<W: Write>(&mut self, target: &mut W, content: &str) -> Result<()> {
        try!(self.fix_non_empty_element(target));

        // TODO: add escaping dashes at the end of the comment

        let autopad_comments = self.config.autopad_comments;
        let write = |target: &mut W| -> Result<()> {
            try_chain! {
                target.write(b"<!--"),
                if autopad_comments && !content.starts_with(char::is_whitespace) {
                    target.write(b" ")
                } else { Ok(0) },
                target.write(content.as_bytes()),
                if autopad_comments && !content.ends_with(char::is_whitespace) {
                    target.write(b" ")
                } else { Ok(0) },
                ok_unit target.write(b"-->")
            }
        };

        wrapped_with!(self; before_markup(target) and after_markup,
            write(target)
        )
    }
}
