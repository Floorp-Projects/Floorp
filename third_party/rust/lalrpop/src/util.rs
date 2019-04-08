use std::fmt::{Display, Error, Formatter};

pub use std::collections::btree_map as map;
pub struct Sep<S>(pub &'static str, pub S);

impl<'a, S: Display> Display for Sep<&'a Vec<S>> {
    fn fmt(&self, fmt: &mut Formatter) -> Result<(), Error> {
        let &Sep(sep, vec) = self;
        let mut elems = vec.iter();
        if let Some(elem) = elems.next() {
            try!(write!(fmt, "{}", elem));
            while let Some(elem) = elems.next() {
                try!(write!(fmt, "{}{}", sep, elem));
            }
        }
        Ok(())
    }
}

pub struct Escape<S>(pub S);

impl<S: Display> Display for Escape<S> {
    fn fmt(&self, fmt: &mut Formatter) -> Result<(), Error> {
        let tmp = format!("{}", self.0);
        for c in tmp.chars() {
            match c {
                'a'...'z' | '0'...'9' | 'A'...'Z' => try!(write!(fmt, "{}", c)),
                '_' => try!(write!(fmt, "__")),
                _ => try!(write!(fmt, "_{:x}", c as usize)),
            }
        }
        Ok(())
    }
}

pub struct Prefix<S>(pub &'static str, pub S);

impl<'a, S: Display> Display for Prefix<&'a [S]> {
    fn fmt(&self, fmt: &mut Formatter) -> Result<(), Error> {
        let &Prefix(prefix, vec) = self;
        let mut elems = vec.iter();
        while let Some(elem) = elems.next() {
            try!(write!(fmt, "{}{}", prefix, elem));
        }
        Ok(())
    }
}

/// Strip leading and trailing whitespace.
pub fn strip(s: &str) -> &str {
    s.trim_matches(char::is_whitespace)
}
