//! Formatting for log records.
//!
//! This module contains a [`Formatter`] that can be used to format log records
//! into without needing temporary allocations. Usually you won't need to worry
//! about the contents of this module and can use the `Formatter` like an ordinary
//! [`Write`].
//!
//! # Formatting log records
//!
//! The format used to print log records can be customised using the [`Builder::format`]
//! method.
//! Custom formats can apply different color and weight to printed values using
//! [`Style`] builders.
//!
//! ```
//! use std::io::Write;
//!
//! let mut builder = env_logger::Builder::new();
//!
//! builder.format(|buf, record| {
//!     writeln!(buf, "{}: {}",
//!         record.level(),
//!         record.args())
//! });
//! ```
//!
//! [`Formatter`]: struct.Formatter.html
//! [`Style`]: struct.Style.html
//! [`Builder::format`]: ../struct.Builder.html#method.format
//! [`Write`]: https://doc.rust-lang.org/stable/std/io/trait.Write.html

use std::io::prelude::*;
use std::{io, fmt, mem};
use std::rc::Rc;
use std::cell::RefCell;
use std::fmt::Display;

use log::Record;

pub(crate) mod writer;
mod humantime;

pub use self::humantime::glob::*;
pub use self::writer::glob::*;

use self::writer::{Writer, Buffer};

pub(crate) mod glob {
    pub use super::{Target, WriteStyle};
}

/// A formatter to write logs into.
///
/// `Formatter` implements the standard [`Write`] trait for writing log records.
/// It also supports terminal colors, through the [`style`] method.
///
/// # Examples
///
/// Use the [`writeln`] macro to format a log record.
/// An instance of a `Formatter` is passed to an `env_logger` format as `buf`:
///
/// ```
/// use std::io::Write;
///
/// let mut builder = env_logger::Builder::new();
///
/// builder.format(|buf, record| writeln!(buf, "{}: {}", record.level(), record.args()));
/// ```
///
/// [`Write`]: https://doc.rust-lang.org/stable/std/io/trait.Write.html
/// [`writeln`]: https://doc.rust-lang.org/stable/std/macro.writeln.html
/// [`style`]: #method.style
pub struct Formatter {
    buf: Rc<RefCell<Buffer>>,
    write_style: WriteStyle,
}

impl Formatter {
    pub(crate) fn new(writer: &Writer) -> Self {
        Formatter {
            buf: Rc::new(RefCell::new(writer.buffer())),
            write_style: writer.write_style(),
        }
    }

    pub(crate) fn write_style(&self) -> WriteStyle {
        self.write_style
    }

    pub(crate) fn print(&self, writer: &Writer) -> io::Result<()> {
        writer.print(&self.buf.borrow())
    }

    pub(crate) fn clear(&mut self) {
        self.buf.borrow_mut().clear()
    }
}

impl Write for Formatter {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.buf.borrow_mut().write(buf)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.buf.borrow_mut().flush()
    }
}

impl fmt::Debug for Formatter {
    fn fmt(&self, f: &mut fmt::Formatter)->fmt::Result {
        f.debug_struct("Formatter").finish()
    }
}

pub(crate) struct Builder {
    pub default_format_timestamp: bool,
    pub default_format_timestamp_nanos: bool,
    pub default_format_module_path: bool,
    pub default_format_level: bool,
    #[allow(unknown_lints, bare_trait_objects)]
    pub custom_format: Option<Box<Fn(&mut Formatter, &Record) -> io::Result<()> + Sync + Send>>,
    built: bool,
}

impl Default for Builder {
    fn default() -> Self {
        Builder {
            default_format_timestamp: true,
            default_format_timestamp_nanos: false,
            default_format_module_path: true,
            default_format_level: true,
            custom_format: None,
            built: false,
        }
    }
}

impl Builder {
    /// Convert the format into a callable function.
    /// 
    /// If the `custom_format` is `Some`, then any `default_format` switches are ignored.
    /// If the `custom_format` is `None`, then a default format is returned.
    /// Any `default_format` switches set to `false` won't be written by the format.
    #[allow(unknown_lints, bare_trait_objects)]
    pub fn build(&mut self) -> Box<Fn(&mut Formatter, &Record) -> io::Result<()> + Sync + Send> {
        assert!(!self.built, "attempt to re-use consumed builder");

        let built = mem::replace(self, Builder {
            built: true,
            ..Default::default()
        });

        if let Some(fmt) = built.custom_format {
            fmt
        }
        else {
            Box::new(move |buf, record| {
                let fmt = DefaultFormat {
                    timestamp: built.default_format_timestamp,
                    timestamp_nanos: built.default_format_timestamp_nanos,
                    module_path: built.default_format_module_path,
                    level: built.default_format_level,
                    written_header_value: false,
                    buf,
                };

                fmt.write(record)
            })
        }
    }
}

#[cfg(feature = "termcolor")]
type SubtleStyle = StyledValue<'static, &'static str>;
#[cfg(not(feature = "termcolor"))]
type SubtleStyle = &'static str;

/// The default format.
/// 
/// This format needs to work with any combination of crate features.
struct DefaultFormat<'a> {
    timestamp: bool,
    module_path: bool,
    level: bool,
    timestamp_nanos: bool,
    written_header_value: bool,
    buf: &'a mut Formatter,
}

impl<'a> DefaultFormat<'a> {
    fn write(mut self, record: &Record) -> io::Result<()> {
        self.write_timestamp()?;
        self.write_level(record)?;
        self.write_module_path(record)?;
        self.finish_header()?;

        self.write_args(record)
    }

    fn subtle_style(&self, text: &'static str) -> SubtleStyle {
        #[cfg(feature = "termcolor")]
        {
            self.buf.style()
                .set_color(Color::Black)
                .set_intense(true)
                .into_value(text)
        }
        #[cfg(not(feature = "termcolor"))]
        {
            text
        }
    }

    fn write_header_value<T>(&mut self, value: T) -> io::Result<()>
    where
        T: Display,
    {
        if !self.written_header_value {
            self.written_header_value = true;

            let open_brace = self.subtle_style("[");
            write!(self.buf, "{}{}", open_brace, value)
        } else {
            write!(self.buf, " {}", value)
        }
    }

    fn write_level(&mut self, record: &Record) -> io::Result<()> {
        if !self.level {
            return Ok(())
        }

        let level = {
            #[cfg(feature = "termcolor")]
            {
                self.buf.default_styled_level(record.level())
            }
            #[cfg(not(feature = "termcolor"))]
            {
                record.level()
            }
        };

        self.write_header_value(format_args!("{:<5}", level))
    }

    fn write_timestamp(&mut self) -> io::Result<()> {
        #[cfg(feature = "humantime")]
        {
            if !self.timestamp {
                return Ok(())
            }

            if self.timestamp_nanos {
                let ts_nanos = self.buf.precise_timestamp();
                self.write_header_value(ts_nanos)
            } else {
                let ts = self.buf.timestamp();
                self.write_header_value(ts)
            }
        }
        #[cfg(not(feature = "humantime"))]
        {
            let _ = self.timestamp;
            let _ = self.timestamp_nanos;
            Ok(())
        }
    }

    fn write_module_path(&mut self, record: &Record) -> io::Result<()> {
        if !self.module_path {
            return Ok(())
        }

        if let Some(module_path) = record.module_path() {
            self.write_header_value(module_path)
        } else {
            Ok(())
        }
    }

    fn finish_header(&mut self) -> io::Result<()> {
        if self.written_header_value {
            let close_brace = self.subtle_style("]");
            write!(self.buf, "{} ", close_brace)
        } else {
            Ok(())
        }
    }

    fn write_args(&mut self, record: &Record) -> io::Result<()> {
        writeln!(self.buf, "{}", record.args())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    use log::{Level, Record};

    fn write(fmt: DefaultFormat) -> String {
        let buf = fmt.buf.buf.clone();

        let record = Record::builder()
            .args(format_args!("log message"))
            .level(Level::Info)
            .file(Some("test.rs"))
            .line(Some(144))
            .module_path(Some("test::path"))
            .build();

        fmt.write(&record).expect("failed to write record");

        let buf = buf.borrow();
        String::from_utf8(buf.bytes().to_vec()).expect("failed to read record")
    }

    #[test]
    fn default_format_with_header() {
        let writer = writer::Builder::new()
            .write_style(WriteStyle::Never)
            .build();

        let mut f = Formatter::new(&writer);

        let written = write(DefaultFormat {
            timestamp: false,
            timestamp_nanos: false,
            module_path: true,
            level: true,
            written_header_value: false,
            buf: &mut f,
        });

        assert_eq!("[INFO  test::path] log message\n", written);
    }

    #[test]
    fn default_format_no_header() {
        let writer = writer::Builder::new()
            .write_style(WriteStyle::Never)
            .build();

        let mut f = Formatter::new(&writer);

        let written = write(DefaultFormat {
            timestamp: false,
            timestamp_nanos: false,
            module_path: false,
            level: false,
            written_header_value: false,
            buf: &mut f,
        });

        assert_eq!("log message\n", written);
    }
}
