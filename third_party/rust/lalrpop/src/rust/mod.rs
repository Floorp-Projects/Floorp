//! Simple Rust AST. This is what the various code generators create,
//! which then gets serialized.

use grammar::parse_tree::Visibility;
use grammar::repr::Grammar;
use std::fmt;
use std::io::{self, Write};
use tls::Tls;

macro_rules! rust {
    ($w:expr, $($args:tt)*) => {
        try!(($w).writeln(&::std::fmt::format(format_args!($($args)*))))
    }
}

/// A wrapper around a Write instance that handles indentation for
/// Rust code. It expects Rust code to be written in a stylized way,
/// with lots of braces and newlines (example shown here with no
/// indentation). Over time maybe we can extend this to make things
/// look prettier, but seems like...meh, just run it through some
/// rustfmt tool.
///
/// ```ignore
/// fn foo(
/// arg1: Type1,
/// arg2: Type2,
/// arg3: Type3)
/// -> ReturnType
/// {
/// match foo {
/// Variant => {
/// }
/// }
/// }
/// ```
pub struct RustWrite<W: Write> {
    write: W,
    indent: usize,
}

const TAB: usize = 4;

impl<W: Write> RustWrite<W> {
    pub fn new(w: W) -> RustWrite<W> {
        RustWrite {
            write: w,
            indent: 0,
        }
    }

    pub fn into_inner(self) -> W {
        self.write
    }

    fn write_indentation(&mut self) -> io::Result<()> {
        if Tls::session().emit_whitespace {
            write!(self.write, "{0:1$}", "", self.indent)?;
        }
        Ok(())
    }

    fn write_indented(&mut self, out: &str) -> io::Result<()> {
        self.write_indentation()?;
        writeln!(self.write, "{}", out)
    }

    pub fn write_table_row<I, C>(&mut self, iterable: I) -> io::Result<()>
    where
        I: IntoIterator<Item = (i32, C)>,
        C: fmt::Display,
    {
        let session = Tls::session();
        if session.emit_comments {
            for (i, comment) in iterable {
                try!(self.write_indentation());
                try!(writeln!(self.write, "{}, {}", i, comment));
            }
        } else {
            try!(self.write_indentation());
            let mut first = true;
            for (i, _comment) in iterable {
                if !first && session.emit_whitespace {
                    try!(write!(self.write, " "));
                }
                try!(write!(self.write, "{},", i));
                first = false;
            }
        }
        writeln!(self.write, "")
    }

    pub fn writeln(&mut self, out: &str) -> io::Result<()> {
        let buf = out.as_bytes();

        // pass empty lines through with no indentation
        if buf.is_empty() {
            return self.write.write_all("\n".as_bytes());
        }

        let n = buf.len() - 1;

        // If the line begins with a `}`, `]`, or `)`, first decrement the indentation.
        if buf[0] == ('}' as u8) || buf[0] == (']' as u8) || buf[0] == (')' as u8) {
            self.indent -= TAB;
        }

        try!(self.write_indented(out));

        // Detect a line that ends in a `{` or `(` and increase indentation for future lines.
        if buf[n] == ('{' as u8) || buf[n] == ('[' as u8) || buf[n] == ('(' as u8) {
            self.indent += TAB;
        }

        Ok(())
    }

    pub fn write_fn_header(
        &mut self,
        grammar: &Grammar,
        visibility: &Visibility,
        name: String,
        type_parameters: Vec<String>,
        first_parameter: Option<String>,
        parameters: Vec<String>,
        return_type: String,
        where_clauses: Vec<String>,
    ) -> io::Result<()> {
        rust!(self, "{}fn {}<", visibility, name);

        for type_parameter in &grammar.type_parameters {
            rust!(self, "{0:1$}{2},", "", TAB, type_parameter);
        }

        for type_parameter in type_parameters {
            rust!(self, "{0:1$}{2},", "", TAB, type_parameter);
        }

        rust!(self, ">(");

        if let Some(param) = first_parameter {
            rust!(self, "{},", param);
        }
        for parameter in &grammar.parameters {
            rust!(self, "{}: {},", parameter.name, parameter.ty);
        }

        for parameter in &parameters {
            rust!(self, "{},", parameter);
        }

        if !grammar.where_clauses.is_empty() || !where_clauses.is_empty() {
            rust!(self, ") -> {} where", return_type);

            for where_clause in &grammar.where_clauses {
                rust!(self, "  {},", where_clause);
            }

            for where_clause in &where_clauses {
                rust!(self, "  {},", where_clause);
            }
        } else {
            rust!(self, ") -> {}", return_type);
        }

        Ok(())
    }

    pub fn write_module_attributes(&mut self, grammar: &Grammar) -> io::Result<()> {
        for attribute in grammar.module_attributes.iter() {
            rust!(self, "{}", attribute);
        }
        Ok(())
    }

    pub fn write_uses(&mut self, super_prefix: &str, grammar: &Grammar) -> io::Result<()> {
        // things the user wrote
        for u in &grammar.uses {
            if u.starts_with("super::") {
                rust!(self, "use {}{};", super_prefix, u);
            } else {
                rust!(self, "use {};", u);
            }
        }

        self.write_standard_uses(&grammar.prefix)
    }

    pub fn write_standard_uses(&mut self, prefix: &str) -> io::Result<()> {
        // Stuff that we plan to use.
        // Occasionally we happen to not use it after all, hence the allow.
        rust!(self, "#[allow(unused_extern_crates)]");
        rust!(self, "extern crate lalrpop_util as {}lalrpop_util;", prefix);

        Ok(())
    }
}
