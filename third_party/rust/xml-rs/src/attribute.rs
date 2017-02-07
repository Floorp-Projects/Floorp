//! Contains XML attributes manipulation types and functions.
//!

use std::fmt;

use name::{Name, OwnedName};
use escape::escape_str_attribute;

/// A borrowed version of an XML attribute.
///
/// Consists of a borrowed qualified name and a borrowed string value.
#[derive(Copy, Clone, Eq, PartialEq, Hash, Debug)]
pub struct Attribute<'a> {
    /// Attribute name.
    pub name: Name<'a>,

    /// Attribute value.
    pub value: &'a str
}

impl<'a> fmt::Display for Attribute<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}=\"{}\"", self.name, escape_str_attribute(self.value))
    }
}

impl<'a> Attribute<'a> {
    /// Creates an owned attribute out of this borrowed one.
    #[inline]
    pub fn to_owned(&self) -> OwnedAttribute {
        OwnedAttribute {
            name: self.name.into(),
            value: self.value.into()
        }
    }

    /// Creates a borrowed attribute using the provided borrowed name and a borrowed string value.
    #[inline]
    pub fn new(name: Name<'a>, value: &'a str) -> Attribute<'a> {
        Attribute {
            name: name,
            value: value
        }
    }
}

/// An owned version of an XML attribute.
///
/// Consists of an owned qualified name and an owned string value.
#[derive(Clone, Eq, PartialEq, Hash, Debug)]
pub struct OwnedAttribute {
    /// Attribute name.
    pub name: OwnedName,

    /// Attribute value.
    pub value: String
}

impl OwnedAttribute {
    /// Returns a borrowed `Attribute` out of this owned one.
    pub fn borrow(&self) -> Attribute {
        Attribute {
            name: self.name.borrow(),
            value: &*self.value
        }
    }

    /// Creates a new owned attribute using the provided owned name and an owned string value.
    #[inline]
    pub fn new<S: Into<String>>(name: OwnedName, value: S) -> OwnedAttribute {
        OwnedAttribute {
            name: name,
            value: value.into()
        }
    }
}

impl fmt::Display for OwnedAttribute {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}=\"{}\"", self.name, escape_str_attribute(&*self.value))
    }
}

#[cfg(test)]
mod tests {
    use super::{Attribute};

    use name::Name;

    #[test]
    fn attribute_display() {
        let attr = Attribute::new(
            Name::qualified("attribute", "urn:namespace", Some("n")),
            "its value with > & \" ' < weird symbols"
        );

        assert_eq!(
            &*attr.to_string(),
            "{urn:namespace}n:attribute=\"its value with &gt; &amp; &quot; &apos; &lt; weird symbols\""
        )
    }
}
