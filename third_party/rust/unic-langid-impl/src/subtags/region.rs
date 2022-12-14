use crate::parser::errors::ParserError;
use std::str::FromStr;
use tinystr::TinyStr4;

#[derive(Debug, PartialEq, Eq, Clone, Hash, PartialOrd, Ord, Copy)]
pub struct Region(TinyStr4);

impl Region {
    pub fn from_bytes(v: &[u8]) -> Result<Self, ParserError> {
        let slen = v.len();

        match slen {
            2 => {
                let s = TinyStr4::from_bytes(v).map_err(|_| ParserError::InvalidSubtag)?;
                if !s.is_ascii_alphabetic() {
                    return Err(ParserError::InvalidSubtag);
                }
                Ok(Self(s.to_ascii_uppercase()))
            }
            3 => {
                let s = TinyStr4::from_bytes(v).map_err(|_| ParserError::InvalidSubtag)?;
                if !s.is_ascii_numeric() {
                    return Err(ParserError::InvalidSubtag);
                }
                Ok(Self(s))
            }
            _ => Err(ParserError::InvalidSubtag),
        }
    }

    pub fn as_str(&self) -> &str {
        self.0.as_str()
    }

    /// # Safety
    ///
    /// This function accepts any u64 that is exected to be a valid
    /// `TinyStr4` and a valid `Region` subtag.
    pub const unsafe fn from_raw_unchecked(v: u32) -> Self {
        Self(TinyStr4::from_bytes_unchecked(v.to_le_bytes()))
    }
}

impl From<Region> for u32 {
    fn from(input: Region) -> Self {
        u32::from_le_bytes(*input.0.all_bytes())
    }
}

impl<'l> From<&'l Region> for &'l str {
    fn from(input: &'l Region) -> Self {
        input.0.as_str()
    }
}

impl FromStr for Region {
    type Err = ParserError;

    fn from_str(source: &str) -> Result<Self, Self::Err> {
        Self::from_bytes(source.as_bytes())
    }
}

impl std::fmt::Display for Region {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        f.write_str(&self.0)
    }
}

impl PartialEq<&str> for Region {
    fn eq(&self, other: &&str) -> bool {
        self.as_str() == *other
    }
}
