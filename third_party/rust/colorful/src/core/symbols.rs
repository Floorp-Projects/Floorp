use std::fmt::Display;
use std::fmt::Formatter;
use std::fmt::Result as FmtResult;

pub enum Symbol {
    Mode,
    Semicolon,
    LeftBrackets,
    Esc,
    Reset,
    Simple256Foreground,
    Simple256Background,
    RgbForeground,
    RgbBackground,
    ResetStyle,
    ResetForeground,
    ResetBackground,
    ClearScreenFromCursorToEnd,
    ClearScreenUpToCursor,
    ClearEntireScreen,
    ClearLineFromCursorToEnd,
    ClearLineUpToCursor,
    ClearEntireLine
}

impl Symbol {
    pub fn to_str<'a>(&self) -> &'a str {
        match self {
            Symbol::Mode => "m",
            Symbol::Semicolon => ";",
            Symbol::LeftBrackets => "[",
            Symbol::Esc => "\x1B",
            Symbol::Reset => "\x1B[0m",
            Symbol::Simple256Foreground => "\x1B[38;5;",
            Symbol::Simple256Background => "\x1B[48;5;",
            Symbol::RgbForeground => "\x1B[38;2;",
            Symbol::RgbBackground => "\x1B[48;2;",
            Symbol::ResetStyle => "\x1B[20m",
            Symbol::ResetForeground => "\x1B[39m",
            Symbol::ResetBackground => "\x1B[49m",
            Symbol::ClearScreenFromCursorToEnd => "\x1B[0J",
            Symbol::ClearScreenUpToCursor => "\x1B[1J",
            Symbol::ClearEntireScreen => "\x1B[2J",
            Symbol::ClearLineFromCursorToEnd => "\x1B[0K",
            Symbol::ClearLineUpToCursor => "\x1B[1K",
            Symbol::ClearEntireLine => "\x1B[2K",
        }
    }
}

impl Display for Symbol {
    fn fmt(&self, f: &mut Formatter) -> FmtResult {
        write!(f, "{}", self.to_str())
    }
}