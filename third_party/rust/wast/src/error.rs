use crate::lexer::LexError;
use crate::token::Span;
use std::fmt;
use std::path::{Path, PathBuf};
use unicode_width::UnicodeWidthStr;

/// A convenience error type to tie together all the detailed errors produced by
/// this crate.
///
/// This type can be created from a [`LexError`]. This also contains
/// storage for file/text information so a nice error can be rendered along the
/// same lines of rustc's own error messages (minus the color).
///
/// This type is typically suitable for use in public APIs for consumers of this
/// crate.
#[derive(Debug)]
pub struct Error {
    inner: Box<ErrorInner>,
}

#[derive(Debug)]
struct ErrorInner {
    text: Option<Text>,
    file: Option<PathBuf>,
    span: Span,
    kind: ErrorKind,
}

#[derive(Debug)]
struct Text {
    line: usize,
    col: usize,
    snippet: String,
}

#[derive(Debug)]
enum ErrorKind {
    Lex(LexError),
    Custom(String),
}

impl Error {
    pub(crate) fn lex(span: Span, content: &str, kind: LexError) -> Error {
        let mut ret = Error {
            inner: Box::new(ErrorInner {
                text: None,
                file: None,
                span,
                kind: ErrorKind::Lex(kind),
            }),
        };
        ret.set_text(content);
        ret
    }

    pub(crate) fn parse(span: Span, content: &str, message: String) -> Error {
        let mut ret = Error {
            inner: Box::new(ErrorInner {
                text: None,
                file: None,
                span,
                kind: ErrorKind::Custom(message),
            }),
        };
        ret.set_text(content);
        ret
    }

    /// Creates a new error with the given `message` which is targeted at the
    /// given `span`
    ///
    /// Note that you'll want to ensure that `set_text` or `set_path` is called
    /// on the resulting error to improve the rendering of the error message.
    pub fn new(span: Span, message: String) -> Error {
        Error {
            inner: Box::new(ErrorInner {
                text: None,
                file: None,
                span,
                kind: ErrorKind::Custom(message),
            }),
        }
    }

    /// Return the `Span` for this error.
    pub fn span(&self) -> Span {
        self.inner.span
    }

    /// To provide a more useful error this function can be used to extract
    /// relevant textual information about this error into the error itself.
    ///
    /// The `contents` here should be the full text of the original file being
    /// parsed, and this will extract a sub-slice as necessary to render in the
    /// `Display` implementation later on.
    pub fn set_text(&mut self, contents: &str) {
        if self.inner.text.is_some() {
            return;
        }
        self.inner.text = Some(Text::new(contents, self.inner.span));
    }

    /// To provide a more useful error this function can be used to set
    /// the file name that this error is associated with.
    ///
    /// The `path` here will be stored in this error and later rendered in the
    /// `Display` implementation.
    pub fn set_path(&mut self, path: &Path) {
        if self.inner.file.is_some() {
            return;
        }
        self.inner.file = Some(path.to_path_buf());
    }

    /// Returns the underlying `LexError`, if any, that describes this error.
    pub fn lex_error(&self) -> Option<&LexError> {
        match &self.inner.kind {
            ErrorKind::Lex(e) => Some(e),
            _ => None,
        }
    }

    /// Returns the underlying message, if any, that describes this error.
    pub fn message(&self) -> String {
        match &self.inner.kind {
            ErrorKind::Lex(e) => e.to_string(),
            ErrorKind::Custom(e) => e.clone(),
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let err = match &self.inner.kind {
            ErrorKind::Lex(e) => e as &dyn fmt::Display,
            ErrorKind::Custom(e) => e as &dyn fmt::Display,
        };
        let text = match &self.inner.text {
            Some(text) => text,
            None => {
                return write!(f, "{} at byte offset {}", err, self.inner.span.offset);
            }
        };
        let file = self
            .inner
            .file
            .as_ref()
            .and_then(|p| p.to_str())
            .unwrap_or("<anon>");
        write!(
            f,
            "\
{err}
     --> {file}:{line}:{col}
      |
 {line:4} | {text}
      | {marker:>0$}",
            text.col + 1,
            file = file,
            line = text.line + 1,
            col = text.col + 1,
            err = err,
            text = text.snippet,
            marker = "^",
        )
    }
}

impl std::error::Error for Error {}

impl Text {
    fn new(content: &str, span: Span) -> Text {
        let (line, col) = span.linecol_in(content);
        let contents = content.lines().nth(line).unwrap_or("");
        let mut snippet = String::new();
        for ch in contents.chars() {
            match ch {
                // Replace tabs with spaces to render consistently
                '\t' => {
                    snippet.push_str("    ");
                }
                // these codepoints change how text is rendered so for clarity
                // in error messages they're dropped.
                '\u{202a}' | '\u{202b}' | '\u{202d}' | '\u{202e}' | '\u{2066}' | '\u{2067}'
                | '\u{2068}' | '\u{206c}' | '\u{2069}' => {}

                c => snippet.push(c),
            }
        }
        // Use the `unicode-width` crate to figure out how wide the snippet, up
        // to our "column", actually is. That'll tell us how many spaces to
        // place before the `^` character that points at the problem
        let col = snippet.get(..col).map(|s| s.width()).unwrap_or(col);
        Text { line, col, snippet }
    }
}
