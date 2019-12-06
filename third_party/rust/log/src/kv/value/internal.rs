use std::fmt;

use super::{Fill, Slot, Error};
use kv;

// `Visitor` is an internal API for visiting the structure of a value.
// It's not intended to be public (at this stage).

/// A container for a structured value for a specific kind of visitor.
#[derive(Clone, Copy)]
pub(super) enum Inner<'v> {
    /// A simple primitive value that can be copied without allocating.
    Primitive(Primitive<'v>),
    /// A value that can be filled.
    Fill(&'v Fill),
    /// A debuggable value.
    Debug(&'v fmt::Debug),
    /// A displayable value.
    Display(&'v fmt::Display),

    #[cfg(feature = "kv_unstable_sval")]
    /// A structured value from `sval`.
    Sval(&'v sval_support::Value),
}

impl<'v> Inner<'v> {
    pub(super) fn visit(&self, visitor: &mut Visitor) -> Result<(), Error> {
        match *self {
            Inner::Primitive(value) => match value {
                Primitive::Signed(value) => visitor.i64(value),
                Primitive::Unsigned(value) => visitor.u64(value),
                Primitive::Float(value) => visitor.f64(value),
                Primitive::Bool(value) => visitor.bool(value),
                Primitive::Char(value) => visitor.char(value),
                Primitive::Str(value) => visitor.str(value),
                Primitive::None => visitor.none(),
            },
            Inner::Fill(value) => value.fill(&mut Slot::new(visitor)),
            Inner::Debug(value) => visitor.debug(value),
            Inner::Display(value) => visitor.display(value),

            #[cfg(feature = "kv_unstable_sval")]
            Inner::Sval(value) => visitor.sval(value),
        }
    }
}

/// The internal serialization contract.
pub(super) trait Visitor {
    fn debug(&mut self, v: &fmt::Debug) -> Result<(), Error>;
    fn display(&mut self, v: &fmt::Display) -> Result<(), Error> {
        self.debug(&format_args!("{}",  v))
    }

    fn u64(&mut self, v: u64) -> Result<(), Error>;
    fn i64(&mut self, v: i64) -> Result<(), Error>;
    fn f64(&mut self, v: f64) -> Result<(), Error>;
    fn bool(&mut self, v: bool) -> Result<(), Error>;
    fn char(&mut self, v: char) -> Result<(), Error>;
    fn str(&mut self, v: &str) -> Result<(), Error>;
    fn none(&mut self) -> Result<(), Error>;

    #[cfg(feature = "kv_unstable_sval")]
    fn sval(&mut self, v: &sval_support::Value) -> Result<(), Error>;
}

#[derive(Clone, Copy)]
pub(super) enum Primitive<'v> {
    Signed(i64),
    Unsigned(u64),
    Float(f64),
    Bool(bool),
    Char(char),
    Str(&'v str),
    None,
}

mod fmt_support {
    use super::*;

    impl<'v> kv::Value<'v> {
        /// Get a value from a debuggable type.
        pub fn from_debug<T>(value: &'v T) -> Self
        where
            T: fmt::Debug,
        {
            kv::Value {
                inner: Inner::Debug(value),
            }
        }

        /// Get a value from a displayable type.
        pub fn from_display<T>(value: &'v T) -> Self
        where
            T: fmt::Display,
        {
            kv::Value {
                inner: Inner::Display(value),
            }
        }
    }

    impl<'v> fmt::Debug for kv::Value<'v> {
        fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
            self.visit(&mut FmtVisitor(f))?;

            Ok(())
        }
    }

    impl<'v> fmt::Display for kv::Value<'v> {
        fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
            self.visit(&mut FmtVisitor(f))?;

            Ok(())
        }
    }
    
    struct FmtVisitor<'a, 'b: 'a>(&'a mut fmt::Formatter<'b>);

    impl<'a, 'b: 'a> Visitor for FmtVisitor<'a, 'b> {
        fn debug(&mut self, v: &fmt::Debug) -> Result<(), Error> {
            v.fmt(self.0)?;

            Ok(())
        }

        fn u64(&mut self, v: u64) -> Result<(), Error> {
            self.debug(&format_args!("{:?}", v))
        }

        fn i64(&mut self, v: i64) -> Result<(), Error> {
            self.debug(&format_args!("{:?}", v))
        }

        fn f64(&mut self, v: f64) -> Result<(), Error> {
            self.debug(&format_args!("{:?}", v))
        }

        fn bool(&mut self, v: bool) -> Result<(), Error> {
            self.debug(&format_args!("{:?}", v))
        }

        fn char(&mut self, v: char) -> Result<(), Error> {
            self.debug(&format_args!("{:?}", v))
        }

        fn str(&mut self, v: &str) -> Result<(), Error> {
            self.debug(&format_args!("{:?}", v))
        }

        fn none(&mut self) -> Result<(), Error> {
            self.debug(&format_args!("None"))
        }

        #[cfg(feature = "kv_unstable_sval")]
        fn sval(&mut self, v: &sval_support::Value) -> Result<(), Error> {
            sval_support::fmt(self.0, v)
        }
    }
}

#[cfg(feature = "kv_unstable_sval")]
pub(super) mod sval_support {
    use super::*;

    extern crate sval;

    impl<'v> kv::Value<'v> {
        /// Get a value from a structured type.
        pub fn from_sval<T>(value: &'v T) -> Self
        where
            T: sval::Value,
        {
            kv::Value {
                inner: Inner::Sval(value),
            }
        }
    }

    impl<'v> sval::Value for kv::Value<'v> {
        fn stream(&self, s: &mut sval::value::Stream) -> sval::value::Result {
            self.visit(&mut SvalVisitor(s)).map_err(Error::into_sval)?;

            Ok(())
        }
    }

    pub(in kv::value) use self::sval::Value;

    pub(super) fn fmt(f: &mut fmt::Formatter, v: &sval::Value) -> Result<(), Error> {
        sval::fmt::debug(f, v)?;
        Ok(())
    }

    impl Error {
        fn from_sval(_: sval::value::Error) -> Self {
            Error::msg("`sval` serialization failed")
        }
        
        fn into_sval(self) -> sval::value::Error {
            sval::value::Error::msg("`sval` serialization failed")
        }
    }

    struct SvalVisitor<'a, 'b: 'a>(&'a mut sval::value::Stream<'b>);

    impl<'a, 'b: 'a> Visitor for SvalVisitor<'a, 'b> {
        fn debug(&mut self, v: &fmt::Debug) -> Result<(), Error> {
            self.0.fmt(format_args!("{:?}", v)).map_err(Error::from_sval)
        }

        fn u64(&mut self, v: u64) -> Result<(), Error> {
            self.0.u64(v).map_err(Error::from_sval)
        }

        fn i64(&mut self, v: i64) -> Result<(), Error> {
            self.0.i64(v).map_err(Error::from_sval)
        }

        fn f64(&mut self, v: f64) -> Result<(), Error> {
            self.0.f64(v).map_err(Error::from_sval)
        }

        fn bool(&mut self, v: bool) -> Result<(), Error> {
            self.0.bool(v).map_err(Error::from_sval)
        }

        fn char(&mut self, v: char) -> Result<(), Error> {
            self.0.char(v).map_err(Error::from_sval)
        }

        fn str(&mut self, v: &str) -> Result<(), Error> {
            self.0.str(v).map_err(Error::from_sval)
        }

        fn none(&mut self) -> Result<(), Error> {
            self.0.none().map_err(Error::from_sval)
        }

        fn sval(&mut self, v: &sval::Value) -> Result<(), Error> {
            self.0.any(v).map_err(Error::from_sval)
        }
    }

    #[cfg(test)]
    mod tests {
        use super::*;
        use kv::value::test::Token;

        #[test]
        fn test_from_sval() {
            assert_eq!(kv::Value::from_sval(&42u64).to_token(), Token::Sval);
        }

        #[test]
        fn test_sval_structured() {
            let value = kv::Value::from(42u64);
            let expected = vec![sval::test::Token::Unsigned(42)];

            assert_eq!(sval::test::tokens(value), expected);
        }
    }
}
