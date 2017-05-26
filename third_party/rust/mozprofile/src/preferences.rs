use std::collections::BTreeMap;
use std::string;
use std::convert::From;

pub type Preferences = BTreeMap<String, Pref>;

#[derive(Debug, PartialEq, Clone)]
pub enum PrefValue {
    Bool(bool),
    String(string::String),
    Int(i64),
}

impl From<bool> for PrefValue {
    fn from(value: bool) -> Self {
        PrefValue::Bool(value)
    }
}

impl From<String> for PrefValue {
    fn from(value: String) -> Self {
        PrefValue::String(value)
    }
}

impl From<&'static str> for PrefValue {
    fn from(value: &'static str) -> Self {
        PrefValue::String(value.into())
    }
}

impl From<i64> for PrefValue {
    fn from(value: i64) -> Self {
        PrefValue::Int(value)
    }
}

#[derive(Debug, PartialEq, Clone)]
pub struct Pref {
    pub value: PrefValue,
    pub sticky: bool
}

impl Pref {
    pub fn new<T>(value: T) -> Pref
        where T: Into<PrefValue> {
        Pref {
            value: value.into(),
            sticky: false
        }
    }

    pub fn new_sticky<T>(value: T) -> Pref
        where T: Into<PrefValue> {
        Pref {
            value: value.into(),
            sticky: true
        }
    }
}
