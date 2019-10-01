use super::Value;

impl PartialEq for Value {
    fn eq(&self, other: &Value) -> bool {
        match (self, other) {
            (&Value::Null, &Value::Null) => true,
            (&Value::Bool(a), &Value::Bool(b)) => a == b,
            (&Value::Number(ref a), &Value::Number(ref b)) => a == b,
            (&Value::String(ref a), &Value::String(ref b)) => a == b,
            (&Value::Sequence(ref a), &Value::Sequence(ref b)) => a == b,
            (&Value::Mapping(ref a), &Value::Mapping(ref b)) => a == b,
            _ => false,
        }
    }
}

impl PartialEq<str> for Value {
    /// Compare `str` with YAML value
    ///
    /// # Examples
    ///
    /// ```edition2018
    /// # use serde_yaml::Value;
    /// assert!(Value::String("lorem".into()) == *"lorem");
    /// ```
    fn eq(&self, other: &str) -> bool {
        self.as_str().map_or(false, |s| s == other)
    }
}

impl<'a> PartialEq<&'a str> for Value {
    /// Compare `&str` with YAML value
    ///
    /// # Examples
    ///
    /// ```edition2018
    /// # use serde_yaml::Value;
    /// assert!(Value::String("lorem".into()) == "lorem");
    /// ```
    fn eq(&self, other: &&str) -> bool {
        self.as_str().map_or(false, |s| s == *other)
    }
}

impl PartialEq<Value> for str {
    /// Compare YAML value with `str`
    ///
    /// # Examples
    ///
    /// ```edition2018
    /// # use serde_yaml::Value;
    /// assert!(*"lorem" == Value::String("lorem".into()));
    /// ```
    fn eq(&self, other: &Value) -> bool {
        other.as_str().map_or(false, |s| s == self)
    }
}

impl<'a> PartialEq<Value> for &'a str {
    /// Compare `&str` with YAML value
    ///
    /// # Examples
    ///
    /// ```edition2018
    /// # use serde_yaml::Value;
    /// assert!("lorem" == Value::String("lorem".into()));
    /// ```
    fn eq(&self, other: &Value) -> bool {
        other.as_str().map_or(false, |s| s == *self)
    }
}

impl PartialEq<String> for Value {
    /// Compare YAML value with String
    ///
    /// # Examples
    ///
    /// ```edition2018
    /// # use serde_yaml::Value;
    /// assert!(Value::String("lorem".into()) == "lorem".to_string());
    /// ```
    fn eq(&self, other: &String) -> bool {
        self.as_str().map_or(false, |s| s == other)
    }
}

impl PartialEq<Value> for String {
    /// Compare `String` with YAML value
    ///
    /// # Examples
    ///
    /// ```edition2018
    /// # use serde_yaml::Value;
    /// assert!("lorem".to_string() == Value::String("lorem".into()));
    /// ```
    fn eq(&self, other: &Value) -> bool {
        other.as_str().map_or(false, |s| s == self)
    }
}

macro_rules! partialeq_numeric {
    ($([$($ty:ty)*], $conversion:ident, $base:ty)*) => {
        $($(
            impl PartialEq<$ty> for Value {
                fn eq(&self, other: &$ty) -> bool {
                    self.$conversion().map_or(false, |i| i == (*other as $base))
                }
            }

            impl PartialEq<Value> for $ty {
                fn eq(&self, other: &Value) -> bool {
                    other.$conversion().map_or(false, |i| i == (*self as $base))
                }
            }

            impl<'a> PartialEq<$ty> for &'a Value {
                fn eq(&self, other: &$ty) -> bool {
                    self.$conversion().map_or(false, |i| i == (*other as $base))
                }
            }

            impl<'a> PartialEq<$ty> for &'a mut Value {
                fn eq(&self, other: &$ty) -> bool {
                    self.$conversion().map_or(false, |i| i == (*other as $base))
                }
            }
        )*)*
    }
}

partialeq_numeric! {
    [i8 i16 i32 i64 isize], as_i64, i64
    [u8 u16 u32 usize], as_i64, i64
    [f32 f64], as_f64, f64
}
