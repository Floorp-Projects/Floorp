use crate::parser::errors::ParserError;
use std::str::FromStr;
use tinystr::TinyStr8;

#[derive(Debug, PartialEq, Eq, Clone, Hash, PartialOrd, Ord, Copy)]
pub struct Variant(TinyStr8);

impl Variant {
    pub fn from_bytes(v: &[u8]) -> Result<Self, ParserError> {
        let slen = v.len();

        if !(4..=8).contains(&slen) {
            return Err(ParserError::InvalidSubtag);
        }

        let s = TinyStr8::from_bytes(v).map_err(|_| ParserError::InvalidSubtag)?;

        if (slen >= 5 && !s.is_ascii_alphanumeric())
            || (slen == 4
                && !v[0].is_ascii_digit()
                && v[1..].iter().any(|c: &u8| !c.is_ascii_alphanumeric()))
        {
            return Err(ParserError::InvalidSubtag);
        }

        Ok(Self(s.to_ascii_lowercase()))
    }

    pub fn as_str(&self) -> &str {
        self.0.as_str()
    }

    /// # Safety
    ///
    /// This function accepts any u64 that is exected to be a valid
    /// `TinyStr8` and a valid `Variant` subtag.
    pub const unsafe fn from_raw_unchecked(v: u64) -> Self {
        Self(TinyStr8::from_bytes_unchecked(v.to_le_bytes()))
    }
}

impl From<Variant> for u64 {
    fn from(input: Variant) -> Self {
        u64::from_le_bytes(*input.0.all_bytes())
    }
}

impl From<&Variant> for u64 {
    fn from(input: &Variant) -> Self {
        u64::from_le_bytes(*input.0.all_bytes())
    }
}

impl FromStr for Variant {
    type Err = ParserError;

    fn from_str(source: &str) -> Result<Self, Self::Err> {
        Self::from_bytes(source.as_bytes())
    }
}

impl std::fmt::Display for Variant {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        f.write_str(&self.0)
    }
}

impl PartialEq<&str> for Variant {
    fn eq(&self, other: &&str) -> bool {
        self.as_str() == *other
    }
}

impl PartialEq<str> for Variant {
    fn eq(&self, other: &str) -> bool {
        self.as_str() == other
    }
}
