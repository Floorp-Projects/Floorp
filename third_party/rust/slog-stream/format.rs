
/// Formats `Record`-s into IO
pub trait Format: Send + Sync + Sized {
    /// Format one logging record and write into `io`
    fn format(&self,
              io: &mut io::Write,
              info: &Record,
              logger_values: &OwnedKeyValueList)
              -> io::Result<()>;
}

/// Formatted stream decorator
///
/// Some `Format`s for which it make sense can use this to parametrize
/// themselves on the exact behavior of formatting parts of the output. This
/// can be used eg. to use color when displaying logger records on the terminal.
pub trait Decorator: Send + Sync + Sized {
    /// Per-record decorator
    type RecordDecorator: RecordDecorator;

    /// Get a `RecordDecorator` for a given `record`
    fn decorate(&self, record: &Record) -> Self::RecordDecorator;
}

/// Per-record decorator
// TODO 2.0: Make everything take `&mut self`.
// TODO 2.0: Make everything take `f: &FnOnce`
pub trait RecordDecorator {
    /// Format a field
    fn fmt_msg(&self, io: &mut io::Write, f: &Fn(&mut io::Write) -> io::Result<()>) -> io::Result<()> {
        f(io)
    }
    /// Format a key
    fn fmt_key(&self, io: &mut io::Write, f: &Fn(&mut io::Write) -> io::Result<()>) -> io::Result<()> {
        f(io)
    }
    /// Format a separator
    fn fmt_separator(&self, io: &mut io::Write, f: &Fn(&mut io::Write) -> io::Result<()>) -> io::Result<()> {
        f(io)
    }
    /// Format a value
    fn fmt_value(&self, io: &mut io::Write, f: &Fn(&mut io::Write) -> io::Result<()>) -> io::Result<()> {
        f(io)
    }
    /// Format a timestamp
    fn fmt_timestamp(&self, io: &mut io::Write, f : &Fn(&mut io::Write) -> io::Result<()>) -> io::Result<()> {
        f(io)
    }
    /// Format a level
    fn fmt_level(&self, io: &mut io::Write, f: &Fn(&mut io::Write) -> io::Result<()>) -> io::Result<()> {
        f(io)
    }
}
