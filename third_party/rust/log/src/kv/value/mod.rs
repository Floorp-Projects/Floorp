//! Structured values.

use std::fmt;

mod internal;
mod impls;

#[cfg(test)]
pub(in kv) mod test;

pub use kv::Error;

use self::internal::{Inner, Visitor, Primitive};

/// A type that can be converted into a [`Value`](struct.Value.html).
pub trait ToValue {
    /// Perform the conversion.
    fn to_value(&self) -> Value;
}

impl<'a, T> ToValue for &'a T
where
    T: ToValue + ?Sized,
{
    fn to_value(&self) -> Value {
        (**self).to_value()
    }
}

impl<'v> ToValue for Value<'v> {
    fn to_value(&self) -> Value {
        Value {
            inner: self.inner,
        }
    }
}

/// A type that requires extra work to convert into a [`Value`](struct.Value.html).
/// 
/// This trait is a more advanced initialization API than [`ToValue`](trait.ToValue.html).
/// It's intended for erased values coming from other logging frameworks that may need
/// to perform extra work to determine the concrete type to use.
pub trait Fill {
    /// Fill a value.
    fn fill(&self, slot: &mut Slot) -> Result<(), Error>;
}

impl<'a, T> Fill for &'a T
where
    T: Fill + ?Sized,
{
    fn fill(&self, slot: &mut Slot) -> Result<(), Error> {
        (**self).fill(slot)
    }
}

/// A value slot to fill using the [`Fill`](trait.Fill.html) trait.
pub struct Slot<'a> {
    filled: bool,
    visitor: &'a mut Visitor,
}

impl<'a> fmt::Debug for Slot<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("Slot").finish()
    }
}

impl<'a> Slot<'a> {
    fn new(visitor: &'a mut Visitor) -> Self {
        Slot {
            visitor,
            filled: false,
        }
    }

    /// Fill the slot with a value.
    /// 
    /// The given value doesn't need to satisfy any particular lifetime constraints.
    /// 
    /// # Panics
    /// 
    /// Calling `fill` more than once will panic.
    pub fn fill(&mut self, value: Value) -> Result<(), Error> {
        assert!(!self.filled, "the slot has already been filled");
        self.filled = true;

        value.visit(self.visitor)
    }
}

/// A value in a structured key-value pair.
pub struct Value<'v> {
    inner: Inner<'v>,
}

impl<'v> Value<'v> {
    /// Get a value from an internal `Visit`.
    fn from_primitive(value: Primitive<'v>) -> Self {
        Value {
            inner: Inner::Primitive(value),
        }
    }

    /// Get a value from a fillable slot.
    pub fn from_fill<T>(value: &'v T) -> Self
    where
        T: Fill,
    {
        Value {
            inner: Inner::Fill(value),
        }
    }

    fn visit(&self, visitor: &mut Visitor) -> Result<(), Error> {
        self.inner.visit(visitor)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn fill_value() {
        struct TestFill;

        impl Fill for TestFill {
            fn fill(&self, slot: &mut Slot) -> Result<(), Error> {
                let dbg: &fmt::Debug = &1;

                slot.fill(Value::from_debug(&dbg))
            }
        }

        assert_eq!("1", Value::from_fill(&TestFill).to_string());
    }

    #[test]
    #[should_panic]
    fn fill_multiple_times_panics() {
        struct BadFill;

        impl Fill for BadFill {
            fn fill(&self, slot: &mut Slot) -> Result<(), Error> {
                slot.fill(42.into())?;
                slot.fill(6789.into())?;

                Ok(())
            }
        }

        let _ = Value::from_fill(&BadFill).to_string();
    }
}
