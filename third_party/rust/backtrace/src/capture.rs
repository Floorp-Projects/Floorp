use std::fmt;
use std::mem;
use std::os::raw::c_void;
use std::path::{Path, PathBuf};

use {trace, resolve, SymbolName};

/// Representation of an owned and self-contained backtrace.
///
/// This structure can be used to capture a backtrace at various points in a
/// program and later used to inspect what the backtrace was at that time.
#[derive(Clone)]
#[cfg_attr(feature = "serialize-rustc", derive(RustcDecodable, RustcEncodable))]
#[cfg_attr(feature = "serialize-serde", derive(Deserialize, Serialize))]
pub struct Backtrace {
    frames: Vec<BacktraceFrame>,
}

/// Captured version of a frame in a backtrace.
///
/// This type is returned as a list from `Backtrace::frames` and represents one
/// stack frame in a captured backtrace.
#[derive(Clone)]
#[cfg_attr(feature = "serialize-rustc", derive(RustcDecodable, RustcEncodable))]
#[cfg_attr(feature = "serialize-serde", derive(Deserialize, Serialize))]
pub struct BacktraceFrame {
    ip: usize,
    symbol_address: usize,
    symbols: Vec<BacktraceSymbol>,
}

/// Captured version of a symbol in a backtrace.
///
/// This type is returned as a list from `BacktraceFrame::symbols` and
/// represents the metadata for a symbol in a backtrace.
#[derive(Clone)]
#[cfg_attr(feature = "serialize-rustc", derive(RustcDecodable, RustcEncodable))]
#[cfg_attr(feature = "serialize-serde", derive(Deserialize, Serialize))]
pub struct BacktraceSymbol {
    name: Option<Vec<u8>>,
    addr: Option<usize>,
    filename: Option<PathBuf>,
    lineno: Option<u32>,
}

impl Backtrace {
    /// Captures a backtrace at the callsite of this function, returning an
    /// owned representation.
    ///
    /// This function is useful for representing a backtrace as an object in
    /// Rust. This returned value can be sent across threads and printed
    /// elsewhere, and thie purpose of this value is to be entirely self
    /// contained.
    ///
    /// # Examples
    ///
    /// ```
    /// use backtrace::Backtrace;
    ///
    /// let current_backtrace = Backtrace::new();
    /// ```
    pub fn new() -> Backtrace {
        let mut frames = Vec::new();
        trace(|frame| {
            let mut symbols = Vec::new();
            resolve(frame.ip(), |symbol| {
                symbols.push(BacktraceSymbol {
                    name: symbol.name().map(|m| m.as_bytes().to_vec()),
                    addr: symbol.addr().map(|a| a as usize),
                    filename: symbol.filename().map(|m| m.to_path_buf()),
                    lineno: symbol.lineno(),
                });
            });
            frames.push(BacktraceFrame {
                ip: frame.ip() as usize,
                symbol_address: frame.symbol_address() as usize,
                symbols: symbols,
            });
            true
        });

        Backtrace { frames: frames }
    }

    /// Returns the frames from when this backtrace was captured.
    ///
    /// The first entry of this slice is likely the function `Backtrace::new`,
    /// and the last frame is likely something about how this thread or the main
    /// function started.
    pub fn frames(&self) -> &[BacktraceFrame] {
        &self.frames
    }
}

impl From<Vec<BacktraceFrame>> for Backtrace {
    fn from(frames: Vec<BacktraceFrame>) -> Self {
        Backtrace {
            frames: frames
        }
    }
}

impl Into<Vec<BacktraceFrame>> for Backtrace {
    fn into(self) -> Vec<BacktraceFrame> {
        self.frames
    }
}

impl BacktraceFrame {
    /// Same as `Frame::ip`
    pub fn ip(&self) -> *mut c_void {
        self.ip as *mut c_void
    }

    /// Same as `Frame::symbol_address`
    pub fn symbol_address(&self) -> *mut c_void {
        self.symbol_address as *mut c_void
    }
}

impl BacktraceFrame {
    /// Returns the list of symbols that this frame corresponds to.
    ///
    /// Normally there is only one symbol per frame, but sometimes if a number
    /// of functions are inlined into one frame then multiple symbols will be
    /// returned. The first symbol listed is the "innermost function", whereas
    /// the last symbol is the outermost (last caller).
    pub fn symbols(&self) -> &[BacktraceSymbol] {
        &self.symbols
    }
}

impl BacktraceSymbol {
    /// Same as `Symbol::name`
    pub fn name(&self) -> Option<SymbolName> {
        self.name.as_ref().map(|s| SymbolName::new(s))
    }

    /// Same as `Symbol::addr`
    pub fn addr(&self) -> Option<*mut c_void> {
        self.addr.map(|s| s as *mut c_void)
    }

    /// Same as `Symbol::filename`
    pub fn filename(&self) -> Option<&Path> {
        self.filename.as_ref().map(|p| &**p)
    }

    /// Same as `Symbol::lineno`
    pub fn lineno(&self) -> Option<u32> {
        self.lineno
    }
}

impl fmt::Debug for Backtrace {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        let hex_width = mem::size_of::<usize>() * 2 + 2;

        try!(write!(fmt, "stack backtrace:"));

        for (idx, frame) in self.frames().iter().enumerate() {
            let ip = frame.ip();
            try!(write!(fmt, "\n{:4}: {:2$?}", idx, ip, hex_width));

            if frame.symbols.len() == 0 {
                try!(write!(fmt, " - <no info>"));
            }

            for (idx, symbol) in frame.symbols().iter().enumerate() {
                if idx != 0 {
                    try!(write!(fmt, "\n      {:1$}", "", hex_width));
                }

                if let Some(name) = symbol.name() {
                    try!(write!(fmt, " - {}", name));
                } else {
                    try!(write!(fmt, " - <unknown>"));
                }

                if let (Some(file), Some(line)) = (symbol.filename(), symbol.lineno()) {
                    try!(write!(fmt, "\n      {:3$}at {}:{}", "", file.display(), line, hex_width));
                }
            }
        }

        Ok(())
    }
}

impl Default for Backtrace {
    fn default() -> Backtrace {
        Backtrace::new()
    }
}
