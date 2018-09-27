use std::collections::BTreeMap;

pub type Preferences = BTreeMap<String, Pref>;

#[derive(Debug, PartialEq, Clone)]
pub enum PrefValue {
    Bool(bool),
    String(String),
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

impl From<i8> for PrefValue {
    fn from(value: i8) -> Self {
        PrefValue::Int(value.into())
    }
}

impl From<u8> for PrefValue {
    fn from(value: u8) -> Self {
        PrefValue::Int(value.into())
    }
}

impl From<i16> for PrefValue {
    fn from(value: i16) -> Self {
        PrefValue::Int(value.into())
    }
}

impl From<u16> for PrefValue {
    fn from(value: u16) -> Self {
        PrefValue::Int(value.into())
    }
}

impl From<i32> for PrefValue {
    fn from(value: i32) -> Self {
        PrefValue::Int(value.into())
    }
}

impl From<u32> for PrefValue {
    fn from(value: u32) -> Self {
        PrefValue::Int(value.into())
    }
}

impl From<i64> for PrefValue {
    fn from(value: i64) -> Self {
        PrefValue::Int(value)
    }
}

// Implementing From<u64> for PrefValue wouldn't be safe
// because it might overflow.

#[derive(Debug, PartialEq, Clone)]
pub struct Pref {
    pub value: PrefValue,
    pub sticky: bool,
}

impl Pref {
    /// Create a new preference with `value`.
    pub fn new<T>(value: T) -> Pref
    where
        T: Into<PrefValue>,
    {
        Pref {
            value: value.into(),
            sticky: false,
        }
    }

    /// Create a new sticky, or locked, preference with `value`.
    /// These cannot be changed by the user in `about:config`.
    pub fn new_sticky<T>(value: T) -> Pref
    where
        T: Into<PrefValue>,
    {
        Pref {
            value: value.into(),
            sticky: true,
        }
    }
}

#[cfg(test)]
mod test {
    use super::PrefValue;

    #[test]
    fn test_bool() {
        assert_eq!(PrefValue::from(true), PrefValue::Bool(true));
    }

    #[test]
    fn test_string() {
        assert_eq!(PrefValue::from("foo"), PrefValue::String("foo".to_string()));
        assert_eq!(
            PrefValue::from("foo".to_string()),
            PrefValue::String("foo".to_string())
        );
    }

    #[test]
    fn test_int() {
        assert_eq!(PrefValue::from(42i8), PrefValue::Int(42i64));
        assert_eq!(PrefValue::from(42u8), PrefValue::Int(42i64));
        assert_eq!(PrefValue::from(42i16), PrefValue::Int(42i64));
        assert_eq!(PrefValue::from(42u16), PrefValue::Int(42i64));
        assert_eq!(PrefValue::from(42i32), PrefValue::Int(42i64));
        assert_eq!(PrefValue::from(42u32), PrefValue::Int(42i64));
        assert_eq!(PrefValue::from(42i64), PrefValue::Int(42i64));
    }
}
