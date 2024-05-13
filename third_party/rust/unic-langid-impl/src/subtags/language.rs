use crate::parser::errors::ParserError;
use std::borrow::Borrow;
use std::convert::TryFrom;
use std::str::FromStr;
use tinystr::TinyStr8;

#[derive(Default, Debug, PartialEq, Eq, Clone, Hash, PartialOrd, Ord, Copy)]
pub struct Language(Option<TinyStr8>);

impl Language {
    pub fn from_bytes(v: &[u8]) -> Result<Self, ParserError> {
        let slen = v.len();

        let s = TinyStr8::from_bytes(v).map_err(|_| ParserError::InvalidLanguage)?;
        if !(2..=8).contains(&slen) || slen == 4 || !s.is_ascii_alphabetic() {
            return Err(ParserError::InvalidLanguage);
        }

        let value = s.to_ascii_lowercase();

        if value == "und" {
            Ok(Self(None))
        } else {
            Ok(Self(Some(value)))
        }
    }

    pub fn as_str(&self) -> &str {
        self.0.as_deref().unwrap_or("und")
    }

    /// # Safety
    ///
    /// This function accepts any u64 that is exected to be a valid
    /// `TinyStr8` and a valid `Language` subtag.
    pub const unsafe fn from_raw_unchecked(v: u64) -> Self {
        Self(Some(TinyStr8::from_bytes_unchecked(v.to_le_bytes())))
    }

    pub fn matches<O: Borrow<Self>>(
        self,
        other: O,
        self_as_range: bool,
        other_as_range: bool,
    ) -> bool {
        (self_as_range && self.0.is_none())
            || (other_as_range && other.borrow().0.is_none())
            || self == *other.borrow()
    }

    pub fn clear(&mut self) {
        self.0 = None;
    }

    pub fn is_empty(self) -> bool {
        self.0.is_none()
    }
}

impl From<Language> for Option<u64> {
    fn from(input: Language) -> Self {
        input.0.map(|i| u64::from_le_bytes(*i.all_bytes()))
    }
}

impl From<&Language> for Option<u64> {
    fn from(input: &Language) -> Self {
        input.0.map(|i| u64::from_le_bytes(*i.all_bytes()))
    }
}

impl<T> TryFrom<Option<T>> for Language
where
    T: AsRef<[u8]>,
{
    type Error = ParserError;

    fn try_from(v: Option<T>) -> Result<Self, Self::Error> {
        match v {
            Some(l) => Ok(Self::from_bytes(l.as_ref())?),
            None => Ok(Self(None)),
        }
    }
}

impl FromStr for Language {
    type Err = ParserError;

    fn from_str(source: &str) -> Result<Self, Self::Err> {
        Self::from_bytes(source.as_bytes())
    }
}

impl std::fmt::Display for Language {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        if let Some(ref lang) = self.0 {
            f.write_str(lang)
        } else {
            f.write_str("und")
        }
    }
}

impl PartialEq<&str> for Language {
    fn eq(&self, other: &&str) -> bool {
        self.as_str() == *other
    }
}
