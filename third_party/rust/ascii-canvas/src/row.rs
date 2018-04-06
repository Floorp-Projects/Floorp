use std::fmt::{Debug, Display, Formatter, Error};
use style::{Style, StyleCursor};
use term::{self, Terminal};

pub struct Row {
    text: String,
    styles: Vec<Style>
}

impl Row {
    pub fn new(chars: &[char], styles: &[Style]) -> Row {
        assert_eq!(chars.len(), styles.len());
        Row {
            text: chars.iter().cloned().collect(),
            styles: styles.to_vec()
        }
    }

    pub fn write_to<T:Terminal+?Sized>(&self, term: &mut T) -> term::Result<()> {
        let mut cursor = try!(StyleCursor::new(term));
        for (character, &style) in self.text.trim_right().chars()
                                                         .zip(&self.styles)
        {
            try!(cursor.set_style(style));
            try!(write!(cursor.term(), "{}", character));
        }
        Ok(())
    }
}

// Using display/debug just skips the styling.

impl Display for Row {
    fn fmt(&self, fmt: &mut Formatter) -> Result<(), Error> {
        Display::fmt(self.text.trim_right(), fmt)
    }
}

impl Debug for Row {
    fn fmt(&self, fmt: &mut Formatter) -> Result<(), Error> {
        // NB: use Display, not Debug, just throw some quotes around it
        try!(write!(fmt, "\""));
        try!(Display::fmt(self.text.trim_right(), fmt));
        write!(fmt, "\"")
    }
}
