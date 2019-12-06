use std::fmt;

use super::{ToValue, Value, Primitive};

impl ToValue for usize {
    fn to_value(&self) -> Value {
        Value::from(*self)
    }
}

impl<'v> From<usize> for Value<'v> {
    fn from(value: usize) -> Self {
        Value::from_primitive(Primitive::Unsigned(value as u64))
    }
}

impl ToValue for isize {
    fn to_value(&self) -> Value {
        Value::from(*self)
    }
}

impl<'v> From<isize> for Value<'v> {
    fn from(value: isize) -> Self {
        Value::from_primitive(Primitive::Signed(value as i64))
    }
}

impl ToValue for u8 {
    fn to_value(&self) -> Value {
        Value::from(*self)
    }
}

impl<'v> From<u8> for Value<'v> {
    fn from(value: u8) -> Self {
        Value::from_primitive(Primitive::Unsigned(value as u64))
    }
}

impl ToValue for u16 {
    fn to_value(&self) -> Value {
        Value::from(*self)
    }
}

impl<'v> From<u16> for Value<'v> {
    fn from(value: u16) -> Self {
        Value::from_primitive(Primitive::Unsigned(value as u64))
    }
}

impl ToValue for u32 {
    fn to_value(&self) -> Value {
        Value::from(*self)
    }
}

impl<'v> From<u32> for Value<'v> {
    fn from(value: u32) -> Self {
        Value::from_primitive(Primitive::Unsigned(value as u64))
    }
}

impl ToValue for u64 {
    fn to_value(&self) -> Value {
        Value::from(*self)
    }
}

impl<'v> From<u64> for Value<'v> {
    fn from(value: u64) -> Self {
        Value::from_primitive(Primitive::Unsigned(value))
    }
}

impl ToValue for i8 {
    fn to_value(&self) -> Value {
        Value::from(*self)
    }
}

impl<'v> From<i8> for Value<'v> {
    fn from(value: i8) -> Self {
        Value::from_primitive(Primitive::Signed(value as i64))
    }
}

impl ToValue for i16 {
    fn to_value(&self) -> Value {
        Value::from(*self)
    }
}

impl<'v> From<i16> for Value<'v> {
    fn from(value: i16) -> Self {
        Value::from_primitive(Primitive::Signed(value as i64))
    }
}

impl ToValue for i32 {
    fn to_value(&self) -> Value {
        Value::from(*self)
    }
}

impl<'v> From<i32> for Value<'v> {
    fn from(value: i32) -> Self {
        Value::from_primitive(Primitive::Signed(value as i64))
    }
}

impl ToValue for i64 {
    fn to_value(&self) -> Value {
        Value::from(*self)
    }
}

impl<'v> From<i64> for Value<'v> {
    fn from(value: i64) -> Self {
        Value::from_primitive(Primitive::Signed(value))
    }
}

impl ToValue for f32 {
    fn to_value(&self) -> Value {
        Value::from(*self)
    }
}

impl<'v> From<f32> for Value<'v> {
    fn from(value: f32) -> Self {
        Value::from_primitive(Primitive::Float(value as f64))
    }
}

impl ToValue for f64 {
    fn to_value(&self) -> Value {
        Value::from(*self)
    }
}

impl<'v> From<f64> for Value<'v> {
    fn from(value: f64) -> Self {
        Value::from_primitive(Primitive::Float(value))
    }
}

impl ToValue for bool {
    fn to_value(&self) -> Value {
        Value::from(*self)
    }
}

impl<'v> From<bool> for Value<'v> {
    fn from(value: bool) -> Self {
        Value::from_primitive(Primitive::Bool(value))
    }
}

impl ToValue for char {
    fn to_value(&self) -> Value {
        Value::from(*self)
    }
}

impl<'v> From<char> for Value<'v> {
    fn from(value: char) -> Self {
        Value::from_primitive(Primitive::Char(value))
    }
}

impl<'v> ToValue for &'v str {
    fn to_value(&self) -> Value {
        Value::from(*self)
    }
}

impl<'v> From<&'v str> for Value<'v> {
    fn from(value: &'v str) -> Self {
        Value::from_primitive(Primitive::Str(value))
    }
}

impl ToValue for () {
    fn to_value(&self) -> Value {
        Value::from_primitive(Primitive::None)
    }
}

impl<T> ToValue for Option<T>
where
    T: ToValue,
{
    fn to_value(&self) -> Value {
        match *self {
            Some(ref value) => value.to_value(),
            None => Value::from_primitive(Primitive::None),
        }
    }
}

impl<'v> ToValue for fmt::Arguments<'v> {
    fn to_value(&self) -> Value {
        Value::from_debug(self)
    }
}

#[cfg(feature = "std")]
mod std_support {
    use super::*;

    use std::borrow::Cow;

    impl<T> ToValue for Box<T>
    where
        T: ToValue + ?Sized,
    {
        fn to_value(&self) -> Value {
            (**self).to_value()
        }
    }

    impl ToValue for String {
        fn to_value(&self) -> Value {
            Value::from_primitive(Primitive::Str(&*self))
        }
    }

    impl<'v> ToValue for Cow<'v, str> {
        fn to_value(&self) -> Value {
            Value::from_primitive(Primitive::Str(&*self))
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use kv::value::test::Token;

    #[test]
    fn test_to_value_display() {
        assert_eq!(42u64.to_value().to_string(), "42");
        assert_eq!(42i64.to_value().to_string(), "42");
        assert_eq!(42.01f64.to_value().to_string(), "42.01");
        assert_eq!(true.to_value().to_string(), "true");
        assert_eq!('a'.to_value().to_string(), "'a'");
        assert_eq!(format_args!("a {}", "value").to_value().to_string(), "a value");
        assert_eq!("a loong string".to_value().to_string(), "\"a loong string\"");
        assert_eq!(Some(true).to_value().to_string(), "true");
        assert_eq!(().to_value().to_string(), "None");
        assert_eq!(Option::None::<bool>.to_value().to_string(), "None");
    }

    #[test]
    fn test_to_value_structured() {
        assert_eq!(42u64.to_value().to_token(), Token::U64(42));
        assert_eq!(42i64.to_value().to_token(), Token::I64(42));
        assert_eq!(42.01f64.to_value().to_token(), Token::F64(42.01));
        assert_eq!(true.to_value().to_token(), Token::Bool(true));
        assert_eq!('a'.to_value().to_token(), Token::Char('a'));
        assert_eq!(format_args!("a {}", "value").to_value().to_token(), Token::Str("a value".into()));
        assert_eq!("a loong string".to_value().to_token(), Token::Str("a loong string".into()));
        assert_eq!(Some(true).to_value().to_token(), Token::Bool(true));
        assert_eq!(().to_value().to_token(), Token::None);
        assert_eq!(Option::None::<bool>.to_value().to_token(), Token::None);
    }
}
