//! This is a library for controlling colours and formatting, such as
//! red bold text or blue underlined text, on ANSI terminals.
//!
//!
//! ## Basic usage
//!
//! There are two main data structures in this crate that you need to be
//! concerned with: `ANSIString` and `Style`. A `Style` holds stylistic
//! information: colours, whether the text should be bold, or blinking, or
//! whatever. There are also `Colour` variants that represent simple foreground
//! colour styles. An `ANSIString` is a string paired with a `Style`.
//!
//! (Yes, it’s British English, but you won’t have to write “colour” very often.
//! `Style` is used the majority of the time.)
//!
//! To format a string, call the `paint` method on a `Style` or a `Colour`,
//! passing in the string you want to format as the argument. For example,
//! here’s how to get some red text:
//!
//!     use ansi_term::Colour::Red;
//!     println!("This is in red: {}", Red.paint("a red string"));
//!
//! It’s important to note that the `paint` method does *not* actually return a
//! string with the ANSI control characters surrounding it. Instead, it returns
//! an `ANSIString` value that has a `Display` implementation that, when
//! formatted, returns the characters. This allows strings to be printed with a
//! minimum of `String` allocations being performed behind the scenes.
//!
//! If you *do* want to get at the escape codes, then you can convert the
//! `ANSIString` to a string as you would any other `Display` value:
//!
//!     use ansi_term::Colour::Red;
//!     use std::string::ToString;
//!     let red_string = Red.paint("a red string").to_string();
//!
//!
//! ## Bold, underline, background, and other styles
//!
//! For anything more complex than plain foreground colour changes, you need to
//! construct `Style` objects themselves, rather than beginning with a `Colour`.
//! You can do this by chaining methods based on a new `Style`, created with
//! `Style::new()`. Each method creates a new style that has that specific
//! property set. For example:
//!
//!     use ansi_term::Style;
//!     println!("How about some {} and {}?",
//!              Style::new().bold().paint("bold"),
//!              Style::new().underline().paint("underline"));
//!
//! For brevity, these methods have also been implemented for `Colour` values,
//! so you can give your styles a foreground colour without having to begin with
//! an empty `Style` value:
//!
//!     use ansi_term::Colour::{Blue, Yellow};
//!     println!("Demonstrating {} and {}!",
//!              Blue.bold().paint("blue bold"),
//!              Yellow.underline().paint("yellow underline"));
//!     println!("Yellow on blue: {}", Yellow.on(Blue).paint("wow!"));
//!
//! The complete list of styles you can use are: `bold`, `dimmed`, `italic`,
//! `underline`, `blink`, `reverse`, `hidden`, `strikethrough`, and `on` for
//! background colours.
//!
//! In some cases, you may find it easier to change the foreground on an
//! existing `Style` rather than starting from the appropriate `Colour`.
//! You can do this using the `fg` method:
//!
//!     use ansi_term::Style;
//!     use ansi_term::Colour::{Blue, Cyan, Yellow};
//!     println!("Yellow on blue: {}", Style::new().on(Blue).fg(Yellow).paint("yow!"));
//!     println!("Also yellow on blue: {}", Cyan.on(Blue).fg(Yellow).paint("zow!"));
//!
//! Finally, you can turn a `Colour` into a `Style` with the `normal` method.
//! This will produce the exact same `ANSIString` as if you just used the
//! `paint` method on the `Colour` directly, but it’s useful in certain cases:
//! for example, you may have a method that returns `Styles`, and need to
//! represent both the “red bold” and “red, but not bold” styles with values of
//! the same type. The `Style` struct also has a `Default` implementation if you
//! want to have a style with *nothing* set.
//!
//!     use ansi_term::Style;
//!     use ansi_term::Colour::Red;
//!     Red.normal().paint("yet another red string");
//!     Style::default().paint("a completely regular string");
//!
//!
//! ## Extended colours
//!
//! You can access the extended range of 256 colours by using the `Fixed` colour
//! variant, which takes an argument of the colour number to use. This can be
//! included wherever you would use a `Colour`:
//!
//!     use ansi_term::Colour::Fixed;
//!     Fixed(134).paint("A sort of light purple");
//!     Fixed(221).on(Fixed(124)).paint("Mustard in the ketchup");
//!
//! The first sixteen of these values are the same as the normal and bold
//! standard colour variants. There’s nothing stopping you from using these as
//! `Fixed` colours instead, but there’s nothing to be gained by doing so
//! either.
//!
//! You can also access full 24-bit color by using the `RGB` colour variant,
//! which takes separate `u8` arguments for red, green, and blue:
//!
//!     use ansi_term::Colour::RGB;
//!     RGB(70, 130, 180).paint("Steel blue");
//!
//! ## Combining successive coloured strings
//!
//! The benefit of writing ANSI escape codes to the terminal is that they
//! *stack*: you do not need to end every coloured string with a reset code if
//! the text that follows it is of a similar style. For example, if you want to
//! have some blue text followed by some blue bold text, it’s possible to send
//! the ANSI code for blue, followed by the ANSI code for bold, and finishing
//! with a reset code without having to have an extra one between the two
//! strings.
//!
//! This crate can optimise the ANSI codes that get printed in situations like
//! this, making life easier for your terminal renderer. The `ANSIStrings`
//! struct takes a slice of several `ANSIString` values, and will iterate over
//! each of them, printing only the codes for the styles that need to be updated
//! as part of its formatting routine.
//!
//! The following code snippet uses this to enclose a binary number displayed in
//! red bold text inside some red, but not bold, brackets:
//!
//!     use ansi_term::Colour::Red;
//!     use ansi_term::{ANSIString, ANSIStrings};
//!     let some_value = format!("{:b}", 42);
//!     let strings: &[ANSIString<'static>] = &[
//!         Red.paint("["),
//!         Red.bold().paint(some_value),
//!         Red.paint("]"),
//!     ];
//!     println!("Value: {}", ANSIStrings(strings));
//!
//! There are several things to note here. Firstly, the `paint` method can take
//! *either* an owned `String` or a borrowed `&str`. Internally, an `ANSIString`
//! holds a copy-on-write (`Cow`) string value to deal with both owned and
//! borrowed strings at the same time. This is used here to display a `String`,
//! the result of the `format!` call, using the same mechanism as some
//! statically-available `&str` slices. Secondly, that the `ANSIStrings` value
//! works in the same way as its singular counterpart, with a `Display`
//! implementation that only performs the formatting when required.
//!
//! ## Byte strings
//!
//! This library also supports formatting `[u8]` byte strings; this supports
//! applications working with text in an unknown encoding.  `Style` and
//! `Color` support painting `[u8]` values, resulting in an `ANSIByteString`.
//! This type does not implement `Display`, as it may not contain UTF-8, but
//! it does provide a method `write_to` to write the result to any
//! `io::Write`:
//!
//!     use ansi_term::Colour::Green;
//!     Green.paint("user data".as_bytes()).write_to(&mut std::io::stdout()).unwrap();
//!
//! Similarly, the type `ANSIByteStrings` supports writing a list of
//! `ANSIByteString` values with minimal escape sequences:
//!
//!     use ansi_term::Colour::Green;
//!     use ansi_term::ANSIByteStrings;
//!     ANSIByteStrings(&[
//!         Green.paint("user data 1\n".as_bytes()),
//!         Green.bold().paint("user data 2\n".as_bytes()),
//!     ]).write_to(&mut std::io::stdout()).unwrap();


#![crate_name = "ansi_term"]
#![crate_type = "rlib"]
#![crate_type = "dylib"]

#![warn(missing_copy_implementations)]
#![warn(missing_docs)]
#![warn(trivial_casts, trivial_numeric_casts)]
#![warn(unused_extern_crates, unused_qualifications)]


use std::borrow::Cow;
use std::default::Default;
use std::fmt;
use std::io;
use std::ops::Deref;

use Colour::*;
use Difference::*;

trait AnyWrite {
    type str : ?Sized;
    type Error;
    fn write_fmt(&mut self, fmt: fmt::Arguments) -> Result<(), Self::Error>;
    fn write_str(&mut self, s: &Self::str) -> Result<(), Self::Error>;
}

impl<'a> AnyWrite for fmt::Write + 'a {
    type str = str;
    type Error = fmt::Error;
    fn write_fmt(&mut self, fmt: fmt::Arguments) -> Result<(), Self::Error> {
        fmt::Write::write_fmt(self, fmt)
    }
    fn write_str(&mut self, s: &Self::str) -> Result<(), Self::Error> {
        fmt::Write::write_str(self, s)
    }
}

impl<'a> AnyWrite for io::Write + 'a {
    type str = [u8];
    type Error = io::Error;
    fn write_fmt(&mut self, fmt: fmt::Arguments) -> Result<(), Self::Error> {
        io::Write::write_fmt(self, fmt)
    }
    fn write_str(&mut self, s: &Self::str) -> Result<(), Self::Error> {
        io::Write::write_all(self, s)
    }
}

/// An ANSIGenericString includes a generic string type and a Style to
/// display that string.  ANSIString and ANSIByteString are aliases for
/// this type on str and [u8], respectively.
#[derive(PartialEq, Debug, Clone)]
pub struct ANSIGenericString<'a, S: 'a + ToOwned + ?Sized>
where <S as ToOwned>::Owned: std::fmt::Debug {
    style: Style,
    string: Cow<'a, S>,
}

impl<'a, S: 'a + ToOwned + ?Sized> ANSIGenericString<'a, S>
where <S as ToOwned>::Owned: std::fmt::Debug {
    fn write_to_any<W: AnyWrite<str=S> + ?Sized>(&self, w: &mut W) -> Result<(), W::Error> {
        try!(self.style.write_prefix(w));
        try!(w.write_str(&self.string));
        self.style.write_suffix(w)
    }
}

/// An ANSI String is a string coupled with the Style to display it
/// in a terminal.
///
/// Although not technically a string itself, it can be turned into
/// one with the `to_string` method.
///
/// ### Examples
///
/// ```no_run
/// use ansi_term::ANSIString;
/// use ansi_term::Colour::Red;
///
/// let red_string = Red.paint("a red string");
/// println!("{}", red_string);
/// ```
///
/// ```
/// use ansi_term::ANSIString;
///
/// let plain_string = ANSIString::from("a plain string");
/// assert_eq!(&*plain_string, "a plain string");
/// ```
pub type ANSIString<'a> = ANSIGenericString<'a, str>;

/// An ANSIByteString represents a formatted series of bytes.  Use
/// ANSIByteString when styling text with an unknown encoding.
pub type ANSIByteString<'a> = ANSIGenericString<'a, [u8]>;

/// Like `ANSIString`, but only displays the style prefix.
#[derive(Clone, Copy, Debug)]
pub struct Prefix(Style);

/// Like `ANSIString`, but only displays the style suffix.
#[derive(Clone, Copy, Debug)]
pub struct Suffix(Style);

/// Like `ANSIString`, but only displays the difference between two
/// styles.
#[derive(Clone, Copy, Debug)]
pub struct Infix(Style, Style);

impl<'a> fmt::Display for ANSIString<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let w: &mut fmt::Write = f;
        self.write_to_any(w)
    }
}

impl<'a> ANSIByteString<'a> {
    /// Write an ANSIByteString to an io::Write.  This writes the escape
    /// sequences for the associated Style around the bytes.
    pub fn write_to<W: io::Write>(&self, w: &mut W) -> io::Result<()> {
        let w: &mut io::Write = w;
        self.write_to_any(w)
    }
}


impl<'a, I, S: 'a + ToOwned + ?Sized> From<I> for ANSIGenericString<'a, S>
where I: Into<Cow<'a, S>>,
      <S as ToOwned>::Owned: std::fmt::Debug {
    fn from(input: I) -> ANSIGenericString<'a, S> {
        ANSIGenericString {
            string: input.into(),
            style:  Style::default(),
        }
    }
}

impl<'a, S: 'a + ToOwned + ?Sized> Deref for ANSIGenericString<'a, S>
where <S as ToOwned>::Owned: std::fmt::Debug {
    type Target = S;

    fn deref(&self) -> &S {
        self.string.deref()
    }
}


impl fmt::Display for Prefix {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let f: &mut fmt::Write = f;
        try!(self.0.write_prefix(f));
        Ok(())
    }
}

impl fmt::Display for Suffix {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let f: &mut fmt::Write = f;
        try!(self.0.write_suffix(f));
        Ok(())
    }
}

impl fmt::Display for Infix {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self.0.difference(&self.1) {
            ExtraStyles(style) => {
                let f: &mut fmt::Write = f;
                try!(style.write_prefix(f))
            },
            Reset => {
                let f: &mut fmt::Write = f;
                try!(f.write_str("\x1B[0m"));
                try!(self.0.write_prefix(f));
            },
            NoDifference => { /* Do nothing! */ },
        }
        Ok(())
    }
}


/// A colour is one specific type of ANSI escape code, and can refer
/// to either the foreground or background colour.
///
/// These use the standard numeric sequences.
/// See http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
#[derive(PartialEq, Clone, Copy, Debug)]
pub enum Colour {

    /// Colour #0 (foreground code `30`, background code `40`).
    ///
    /// This is not necessarily the background colour, and using it as one may
    /// render the text hard to read on terminals with dark backgrounds.
    Black,

    /// Colour #1 (foreground code `31`, background code `41`).
    Red,

    /// Colour #2 (foreground code `32`, background code `42`).
    Green,

    /// Colour #3 (foreground code `33`, background code `43`).
    Yellow,

    /// Colour #4 (foreground code `34`, background code `44`).
    Blue,

    /// Colour #5 (foreground code `35`, background code `45`).
    Purple,

    /// Colour #6 (foreground code `36`, background code `46`).
    Cyan,

    /// Colour #7 (foreground code `37`, background code `47`).
    ///
    /// As above, this is not necessarily the foreground colour, and may be
    /// hard to read on terminals with light backgrounds.
    White,

    /// A colour number from 0 to 255, for use in 256-colour terminal
    /// environments.
    ///
    /// - Colours 0 to 7 are the `Black` to `White` variants respectively.
    ///   These colours can usually be changed in the terminal emulator.
    /// - Colours 8 to 15 are brighter versions of the eight colours above.
    ///   These can also usually be changed in the terminal emulator, or it
    ///   could be configured to use the original colours and show the text in
    ///   bold instead. It varies depending on the program.
    /// - Colours 16 to 231 contain several palettes of bright colours,
    ///   arranged in six squares measuring six by six each.
    /// - Colours 232 to 255 are shades of grey from black to white.
    ///
    /// It might make more sense to look at a [colour chart][cc].
    /// [cc]: https://upload.wikimedia.org/wikipedia/en/1/15/Xterm_256color_chart.svg
    Fixed(u8),

    /// A 24-bit RGB color, as specified by ISO-8613-3.
    RGB(u8, u8, u8),
}

/// Color is a type alias for Colour for those who can't be bothered.
pub use Colour as Color;

// I'm not beyond calling Colour Colour, rather than Color, but I did
// purposefully name this crate 'ansi-term' so people wouldn't get
// confused when they tried to install it.
//
// Only *after* they'd installed it.

impl Colour {
    fn write_foreground_code<W: AnyWrite + ?Sized>(&self, f: &mut W) -> Result<(), W::Error> {
        match *self {
            Black      => write!(f, "30"),
            Red        => write!(f, "31"),
            Green      => write!(f, "32"),
            Yellow     => write!(f, "33"),
            Blue       => write!(f, "34"),
            Purple     => write!(f, "35"),
            Cyan       => write!(f, "36"),
            White      => write!(f, "37"),
            Fixed(num) => write!(f, "38;5;{}", &num),
            RGB(r,g,b) => write!(f, "38;2;{};{};{}", &r, &g, &b),
        }
    }

    fn write_background_code<W: AnyWrite + ?Sized>(&self, f: &mut W) -> Result<(), W::Error> {
        match *self {
            Black      => write!(f, "40"),
            Red        => write!(f, "41"),
            Green      => write!(f, "42"),
            Yellow     => write!(f, "43"),
            Blue       => write!(f, "44"),
            Purple     => write!(f, "45"),
            Cyan       => write!(f, "46"),
            White      => write!(f, "47"),
            Fixed(num) => write!(f, "48;5;{}", &num),
            RGB(r,g,b) => write!(f, "48;2;{};{};{}", &r, &g, &b),
        }
    }

    /// Return a Style with the foreground colour set to this colour.
    pub fn normal(self) -> Style {
        Style { foreground: Some(self), .. Style::default() }
    }

    /// Paints the given text with this colour, returning an ANSI string.
    /// This is a short-cut so you don't have to use Blue.normal() just
    /// to get blue text.
    pub fn paint<'a, I, S: 'a + ToOwned + ?Sized>(self, input: I) -> ANSIGenericString<'a, S>
    where I: Into<Cow<'a, S>>,
          <S as ToOwned>::Owned: std::fmt::Debug {
        ANSIGenericString {
            string: input.into(),
            style:  self.normal(),
        }
    }

    /// The prefix for this colour.
    pub fn prefix(self) -> Prefix {
        Prefix(self.normal())
    }

    /// The suffix for this colour.
    pub fn suffix(self) -> Suffix {
        Suffix(self.normal())
    }

    /// The infix between this colour and another.
    pub fn infix(self, other: Colour) -> Infix {
        Infix(self.normal(), other.normal())
    }

    /// Returns a Style with the bold property set.
    pub fn bold(self) -> Style {
        Style { foreground: Some(self), is_bold: true, .. Style::default() }
    }

    /// Returns a Style with the dimmed property set.
    pub fn dimmed(self) -> Style {
        Style { foreground: Some(self), is_dimmed: true, .. Style::default() }
    }

    /// Returns a Style with the italic property set.
    pub fn italic(self) -> Style {
        Style { foreground: Some(self), is_italic: true, .. Style::default() }
    }

    /// Returns a Style with the underline property set.
    pub fn underline(self) -> Style {
        Style { foreground: Some(self), is_underline: true, .. Style::default() }
    }

    /// Returns a Style with the blink property set.
    pub fn blink(self) -> Style {
        Style { foreground: Some(self), is_blink: true, .. Style::default() }
    }

    /// Returns a Style with the reverse property set.
    pub fn reverse(self) -> Style {
        Style { foreground: Some(self), is_reverse: true, .. Style::default() }
    }

    /// Returns a Style with the hidden property set.
    pub fn hidden(self) -> Style {
        Style { foreground: Some(self), is_hidden: true, .. Style::default() }
    }

    /// Returns a Style with the strikethrough property set.
    pub fn strikethrough(self) -> Style {
        Style { foreground: Some(self), is_strikethrough: true, .. Style::default() }
    }

    /// Returns a Style with the background colour property set.
    pub fn on(self, background: Colour) -> Style {
        Style { foreground: Some(self), background: Some(background), .. Style::default() }
    }
}

/// A style is a collection of properties that can format a string
/// using ANSI escape codes.
#[derive(PartialEq, Clone, Copy, Debug)]
pub struct Style {
    foreground: Option<Colour>,
    background: Option<Colour>,
    is_bold: bool,
    is_dimmed: bool,
    is_italic: bool,
    is_underline: bool,
    is_blink: bool,
    is_reverse: bool,
    is_hidden: bool,
    is_strikethrough: bool
}

impl Style {
    /// Creates a new Style with no differences.
    pub fn new() -> Style {
        Style::default()
    }

    /// Paints the given text with this colour, returning an ANSI string.
    pub fn paint<'a, I, S: 'a + ToOwned + ?Sized>(self, input: I) -> ANSIGenericString<'a, S>
    where I: Into<Cow<'a, S>>,
          <S as ToOwned>::Owned: std::fmt::Debug {
        ANSIGenericString {
            string: input.into(),
            style:  self,
        }
    }

    /// The prefix for this style.
    pub fn prefix(self) -> Prefix {
        Prefix(self)
    }

    /// The suffix for this style.
    pub fn suffix(self) -> Suffix {
        Suffix(self)
    }

    /// The infix between this style and another.
    pub fn infix(self, other: Style) -> Infix {
        Infix(self, other)
    }

    /// Returns a Style with the bold property set.
    pub fn bold(&self) -> Style {
        Style { is_bold: true, .. *self }
    }

    /// Returns a Style with the dimmed property set.
    pub fn dimmed(&self) -> Style {
        Style { is_dimmed: true, .. *self }
    }

    /// Returns a Style with the italic property set.
    pub fn italic(&self) -> Style {
        Style { is_italic: true, .. *self }
    }

    /// Returns a Style with the underline property set.
    pub fn underline(&self) -> Style {
        Style { is_underline: true, .. *self }
    }

    /// Returns a Style with the blink property set.
    pub fn blink(&self) -> Style {
        Style { is_blink: true, .. *self }
    }

    /// Returns a Style with the reverse property set.
    pub fn reverse(&self) -> Style {
        Style { is_reverse: true, .. *self }
    }

    /// Returns a Style with the hidden property set.
    pub fn hidden(&self) -> Style {
        Style { is_hidden: true, .. *self }
    }

    /// Returns a Style with the hidden property set.
    pub fn strikethrough(&self) -> Style {
        Style { is_strikethrough: true, .. *self }
    }

    /// Returns a Style with the foreground colour property set.
    pub fn fg(&self, foreground: Colour) -> Style {
        Style { foreground: Some(foreground), .. *self }
    }

    /// Returns a Style with the background colour property set.
    pub fn on(&self, background: Colour) -> Style {
        Style { background: Some(background), .. *self }
    }

    /// Write any ANSI codes that go *before* a piece of text. These should be
    /// the codes to set the terminal to a different colour or font style.
    fn write_prefix<W: AnyWrite + ?Sized>(&self, f: &mut W) -> Result<(), W::Error> {
        // If there are actually no styles here, then don’t write *any* codes
        // as the prefix. An empty ANSI code may not affect the terminal
        // output at all, but a user may just want a code-free string.
        if self.is_plain() {
            return Ok(());
        }

        // Write the codes’ prefix, then write numbers, separated by
        // semicolons, for each text style we want to apply.
        try!(write!(f, "\x1B["));
        let mut written_anything = false;

        {
            let mut write_char = |c| {
                if written_anything { try!(write!(f, ";")); }
                written_anything = true;
                try!(write!(f, "{}", c));
                Ok(())
            };

            if self.is_bold           { try!(write_char('1')); }
            if self.is_dimmed         { try!(write_char('2')); }
            if self.is_italic         { try!(write_char('3')); }
            if self.is_underline      { try!(write_char('4')); }
            if self.is_blink          { try!(write_char('5')); }
            if self.is_reverse        { try!(write_char('7')); }
            if self.is_hidden         { try!(write_char('8')); }
            if self.is_strikethrough  { try!(write_char('9')); }
        }

        // The foreground and background colours, if specified, need to be
        // handled specially because the number codes are more complicated.
        // (see `write_background_code` and `write_foreground_code`)
        if let Some(bg) = self.background {
            if written_anything { try!(write!(f, ";")); }
            written_anything = true;

            try!(bg.write_background_code(f));
        }

        if let Some(fg) = self.foreground {
            if written_anything { try!(write!(f, ";")); }

            try!(fg.write_foreground_code(f));
        }

        // All the codes end with an `m`, because reasons.
        try!(write!(f, "m"));
        Ok(())
    }

    /// Write any ANSI codes that go *after* a piece of text. These should be
    /// the codes to *reset* the terminal back to its normal colour and style.
    fn write_suffix<W: AnyWrite + ?Sized>(&self, f: &mut W) -> Result<(), W::Error> {
        if self.is_plain() {
            Ok(())
        }
        else {
            write!(f, "\x1B[0m")
        }
    }

    /// Compute the 'style difference' required to turn an existing style into
    /// the given, second style.
    ///
    /// For example, to turn green text into green bold text, it's redundant
    /// to write a reset command then a second green+bold command, instead of
    /// just writing one bold command. This method should see that both styles
    /// use the foreground colour green, and reduce it to a single command.
    ///
    /// This method returns an enum value because it's not actually always
    /// possible to turn one style into another: for example, text could be
    /// made bold and underlined, but you can't remove the bold property
    /// without also removing the underline property. So when this has to
    /// happen, this function returns None, meaning that the entire set of
    /// styles should be reset and begun again.
    fn difference(&self, next: &Style) -> Difference {
        // XXX(Havvy): This algorithm is kind of hard to replicate without
        // having the Plain/Foreground enum variants, so I'm just leaving
        // it commented out for now, and defaulting to Reset.

        if self == next {
            return NoDifference;
        }

        // Cannot un-bold, so must Reset.
        if self.is_bold && !next.is_bold {
            return Reset;
        }

        if self.is_dimmed && !next.is_dimmed {
            return Reset;
        }

        if self.is_italic && !next.is_italic {
            return Reset;
        }

        // Cannot un-underline, so must Reset.
        if self.is_underline && !next.is_underline {
            return Reset;
        }

        if self.is_blink && !next.is_blink {
            return Reset;
        }

        if self.is_reverse && !next.is_reverse {
            return Reset;
        }

        if self.is_hidden && !next.is_hidden {
            return Reset;
        }

        if self.is_strikethrough && !next.is_strikethrough {
            return Reset;
        }

        // Cannot go from foreground to no foreground, so must Reset.
        if self.foreground.is_some() && next.foreground.is_none() {
            return Reset;
        }

        // Cannot go from background to no background, so must Reset.
        if self.background.is_some() && next.background.is_none() {
            return Reset;
        }

        let mut extra_styles = Style::default();

        if self.is_bold != next.is_bold {
            extra_styles.is_bold = true;
        }

        if self.is_dimmed != next.is_dimmed {
            extra_styles.is_dimmed = true;
        }

        if self.is_italic != next.is_italic {
            extra_styles.is_italic = true;
        }

        if self.is_underline != next.is_underline {
            extra_styles.is_underline = true;
        }

        if self.is_blink != next.is_blink {
            extra_styles.is_blink = true;
        }

        if self.is_reverse != next.is_reverse {
            extra_styles.is_reverse = true;
        }

        if self.is_hidden != next.is_hidden {
            extra_styles.is_hidden = true;
        }

        if self.is_strikethrough != next.is_strikethrough {
            extra_styles.is_strikethrough = true;
        }

        if self.foreground != next.foreground {
            extra_styles.foreground = next.foreground;
        }

        if self.background != next.background {
            extra_styles.background = next.background;
        }

        ExtraStyles(extra_styles)
    }

    /// Return true if this `Style` has no actual styles, and can be written
    /// without any control characters.
    fn is_plain(self) -> bool {
        self == Style::default()
    }
}

impl Default for Style {
    fn default() -> Style {
        Style {
            foreground: None,
            background: None,
            is_bold: false,
            is_dimmed: false,
            is_italic: false,
            is_underline: false,
            is_blink: false,
            is_reverse: false,
            is_hidden: false,
            is_strikethrough: false,
        }
    }
}

/// When printing out one coloured string followed by another, use one of
/// these rules to figure out which *extra* control codes need to be sent.
#[derive(PartialEq, Clone, Copy, Debug)]
enum Difference {

    /// Print out the control codes specified by this style to end up looking
    /// like the second string's styles.
    ExtraStyles(Style),

    /// Converting between these two is impossible, so just send a reset
    /// command and then the second string's styles.
    Reset,

    /// The before style is exactly the same as the after style, so no further
    /// control codes need to be printed.
    NoDifference,
}

/// A set of `ANSIGenericString`s collected together, in order to be
/// written with a minimum of control characters.
pub struct ANSIGenericStrings<'a, S: 'a + ToOwned + ?Sized>
    (pub &'a [ANSIGenericString<'a, S>])
    where <S as ToOwned>::Owned: std::fmt::Debug;

/// A set of `ANSIString`s collected together, in order to be written with a
/// minimum of control characters.
pub type ANSIStrings<'a> = ANSIGenericStrings<'a, str>;

/// A function to construct an ANSIStrings instance.
#[allow(non_snake_case)]
pub fn ANSIStrings<'a>(arg: &'a [ANSIString<'a>]) -> ANSIStrings<'a> {
    ANSIGenericStrings(arg)
}

/// A set of `ANSIByteString`s collected together, in order to be
/// written with a minimum of control characters.
pub type ANSIByteStrings<'a> = ANSIGenericStrings<'a, [u8]>;

/// A function to construct an ANSIByteStrings instance.
#[allow(non_snake_case)]
pub fn ANSIByteStrings<'a>(arg: &'a [ANSIByteString<'a>]) -> ANSIByteStrings<'a> {
    ANSIGenericStrings(arg)
}

impl<'a, S: 'a + ToOwned + ?Sized> ANSIGenericStrings<'a, S>
where <S as ToOwned>::Owned: std::fmt::Debug {
    fn write_to_any<W: AnyWrite<str=S> + ?Sized>(&self, w: &mut W) -> Result<(), W::Error> {
        let first = match self.0.first() {
            None => return Ok(()),
            Some(f) => f,
        };

        try!(first.style.write_prefix(w));
        try!(w.write_str(&first.string));

        for window in self.0.windows(2) {
            match window[0].style.difference(&window[1].style) {
                ExtraStyles(style) => try!(style.write_prefix(w)),
                Reset => {
                    try!(write!(w, "\x1B[0m"));
                    try!(window[1].style.write_prefix(w));
                },
                NoDifference => { /* Do nothing! */ },
            }

            try!(w.write_str(&window[1].string));
        }

        // Write the final reset string after all of the ANSIStrings have been
        // written, *except* if the last one has no styles, because it would
        // have already been written by this point.
        if let Some(last) = self.0.last() {
            if !last.style.is_plain() {
                try!(write!(w, "\x1B[0m"));
            }
        }

        Ok(())
    }
}

impl<'a> fmt::Display for ANSIStrings<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let f: &mut fmt::Write = f;
        self.write_to_any(f)
    }
}

impl<'a> ANSIByteStrings<'a> {
    /// Write ANSIByteStrings to an io::Write.  This writes the minimal
    /// escape sequences for the associated Styles around each set of
    /// bytes.
    pub fn write_to<W: io::Write>(&self, w: &mut W) -> io::Result<()> {
        let w: &mut io::Write = w;
        self.write_to_any(w)
    }
}

#[cfg(test)]
mod tests {
    pub use super::{Style, ANSIStrings};
    pub use super::Colour::*;

    macro_rules! test {
        ($name: ident: $style: expr; $input: expr => $result: expr) => {
            #[test]
            fn $name() {
                assert_eq!($style.paint($input).to_string(), $result.to_string());

                let mut v = Vec::new();
                $style.paint($input.as_bytes()).write_to(&mut v).unwrap();
                assert_eq!(v.as_slice(), $result.as_bytes());
            }
        };
    }

    test!(plain:                 Style::default();                  "text/plain" => "text/plain");
    test!(red:                   Red;                               "hi" => "\x1B[31mhi\x1B[0m");
    test!(black:                 Black.normal();                    "hi" => "\x1B[30mhi\x1B[0m");
    test!(yellow_bold:           Yellow.bold();                     "hi" => "\x1B[1;33mhi\x1B[0m");
    test!(yellow_bold_2:         Yellow.normal().bold();            "hi" => "\x1B[1;33mhi\x1B[0m");
    test!(blue_underline:        Blue.underline();                  "hi" => "\x1B[4;34mhi\x1B[0m");
    test!(green_bold_ul:         Green.bold().underline();          "hi" => "\x1B[1;4;32mhi\x1B[0m");
    test!(green_bold_ul_2:       Green.underline().bold();          "hi" => "\x1B[1;4;32mhi\x1B[0m");
    test!(purple_on_white:       Purple.on(White);                  "hi" => "\x1B[47;35mhi\x1B[0m");
    test!(purple_on_white_2:     Purple.normal().on(White);         "hi" => "\x1B[47;35mhi\x1B[0m");
    test!(yellow_on_blue:        Style::new().on(Blue).fg(Yellow);  "hi" => "\x1B[44;33mhi\x1B[0m");
    test!(yellow_on_blue_2:      Cyan.on(Blue).fg(Yellow);          "hi" => "\x1B[44;33mhi\x1B[0m");
    test!(cyan_bold_on_white:    Cyan.bold().on(White);             "hi" => "\x1B[1;47;36mhi\x1B[0m");
    test!(cyan_ul_on_white:      Cyan.underline().on(White);        "hi" => "\x1B[4;47;36mhi\x1B[0m");
    test!(cyan_bold_ul_on_white: Cyan.bold().underline().on(White); "hi" => "\x1B[1;4;47;36mhi\x1B[0m");
    test!(cyan_ul_bold_on_white: Cyan.underline().bold().on(White); "hi" => "\x1B[1;4;47;36mhi\x1B[0m");
    test!(fixed:                 Fixed(100);                        "hi" => "\x1B[38;5;100mhi\x1B[0m");
    test!(fixed_on_purple:       Fixed(100).on(Purple);             "hi" => "\x1B[45;38;5;100mhi\x1B[0m");
    test!(fixed_on_fixed:        Fixed(100).on(Fixed(200));         "hi" => "\x1B[48;5;200;38;5;100mhi\x1B[0m");
    test!(rgb:                   RGB(70,130,180);                   "hi" => "\x1B[38;2;70;130;180mhi\x1B[0m");
    test!(rgb_on_blue:           RGB(70,130,180).on(Blue);          "hi" => "\x1B[44;38;2;70;130;180mhi\x1B[0m");
    test!(blue_on_rgb:           Blue.on(RGB(70,130,180));          "hi" => "\x1B[48;2;70;130;180;34mhi\x1B[0m");
    test!(rgb_on_rgb:            RGB(70,130,180).on(RGB(5,10,15));  "hi" => "\x1B[48;2;5;10;15;38;2;70;130;180mhi\x1B[0m");
    test!(bold:                  Style::new().bold();               "hi" => "\x1B[1mhi\x1B[0m");
    test!(underline:             Style::new().underline();          "hi" => "\x1B[4mhi\x1B[0m");
    test!(bunderline:            Style::new().bold().underline();   "hi" => "\x1B[1;4mhi\x1B[0m");
    test!(dimmed:                Style::new().dimmed();             "hi" => "\x1B[2mhi\x1B[0m");
    test!(italic:                Style::new().italic();             "hi" => "\x1B[3mhi\x1B[0m");
    test!(blink:                 Style::new().blink();              "hi" => "\x1B[5mhi\x1B[0m");
    test!(reverse:               Style::new().reverse();            "hi" => "\x1B[7mhi\x1B[0m");
    test!(hidden:                Style::new().hidden();             "hi" => "\x1B[8mhi\x1B[0m");
    test!(stricken:              Style::new().strikethrough();      "hi" => "\x1B[9mhi\x1B[0m");

    mod difference {
        use super::*;
        use super::super::Difference::*;

        #[test]
        fn diff() {
            let expected = ExtraStyles(Style::new().bold());
            let got = Green.normal().difference(&Green.bold());
            assert_eq!(expected, got)
        }

        #[test]
        fn dlb() {
            let got = Green.bold().difference(&Green.normal());
            assert_eq!(Reset, got)
        }

        #[test]
        fn nothing() {
            assert_eq!(NoDifference, Green.bold().difference(&Green.bold()));
        }

        #[test]
        fn nothing_2() {
            assert_eq!(NoDifference, Green.normal().difference(&Green.normal()));
        }

        #[test]
        fn colour_change() {
            assert_eq!(ExtraStyles(Blue.normal()), Red.normal().difference(&Blue.normal()))
        }

        #[test]
        fn removal_of_dimmed() {
            let dimmed = Style::new().dimmed();
            let normal = Style::default();

            assert_eq!(Reset, dimmed.difference(&normal));
        }

        #[test]
        fn addition_of_dimmed() {
            let dimmed = Style::new().dimmed();
            let normal = Style::default();
            let extra_styles = ExtraStyles(dimmed);

            assert_eq!(extra_styles, normal.difference(&dimmed));
        }

        #[test]
        fn removal_of_blink() {
            let blink = Style::new().blink();
            let normal = Style::default();

            assert_eq!(Reset, blink.difference(&normal));
        }

        #[test]
        fn addition_of_blink() {
            let blink = Style::new().blink();
            let normal = Style::default();
            let extra_styles = ExtraStyles(blink);

            assert_eq!(extra_styles, normal.difference(&blink));
        }

        #[test]
        fn removal_of_reverse() {
            let reverse = Style::new().reverse();
            let normal = Style::default();

            assert_eq!(Reset, reverse.difference(&normal));
        }

        #[test]
        fn addition_of_reverse() {
            let reverse = Style::new().reverse();
            let normal = Style::default();
            let extra_styles = ExtraStyles(reverse);

            assert_eq!(extra_styles, normal.difference(&reverse));
        }

        #[test]
        fn removal_of_hidden() {
            let hidden = Style::new().hidden();
            let normal = Style::default();

            assert_eq!(Reset, hidden.difference(&normal));
        }

        #[test]
        fn addition_of_hidden() {
            let hidden = Style::new().hidden();
            let normal = Style::default();
            let extra_styles = ExtraStyles(hidden);

            assert_eq!(extra_styles, normal.difference(&hidden));
        }

        #[test]
        fn removal_of_strikethrough() {
            let strikethrough = Style::new().strikethrough();
            let normal = Style::default();

            assert_eq!(Reset, strikethrough.difference(&normal));
        }

        #[test]
        fn addition_of_strikethrough() {
            let strikethrough = Style::new().strikethrough();
            let normal = Style::default();
            let extra_styles = ExtraStyles(strikethrough);

            assert_eq!(extra_styles, normal.difference(&strikethrough));
        }

        #[test]
        fn no_control_codes_for_plain() {
            let one = Style::default().paint("one");
            let two = Style::default().paint("two");
            let output = format!("{}", ANSIStrings( &[ one, two ] ));
            assert_eq!(&*output, "onetwo");
        }
    }
}
