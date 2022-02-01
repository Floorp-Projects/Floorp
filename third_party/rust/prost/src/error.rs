//! Protobuf encoding and decoding errors.

use alloc::borrow::Cow;
use alloc::boxed::Box;
use alloc::vec::Vec;

use core::fmt;

/// A Protobuf message decoding error.
///
/// `DecodeError` indicates that the input buffer does not caontain a valid
/// Protobuf message. The error details should be considered 'best effort': in
/// general it is not possible to exactly pinpoint why data is malformed.
#[derive(Clone, PartialEq, Eq)]
pub struct DecodeError {
    inner: Box<Inner>,
}

#[derive(Clone, PartialEq, Eq)]
struct Inner {
    /// A 'best effort' root cause description.
    description: Cow<'static, str>,
    /// A stack of (message, field) name pairs, which identify the specific
    /// message type and field where decoding failed. The stack contains an
    /// entry per level of nesting.
    stack: Vec<(&'static str, &'static str)>,
}

impl DecodeError {
    /// Creates a new `DecodeError` with a 'best effort' root cause description.
    ///
    /// Meant to be used only by `Message` implementations.
    #[doc(hidden)]
    #[cold]
    pub fn new(description: impl Into<Cow<'static, str>>) -> DecodeError {
        DecodeError {
            inner: Box::new(Inner {
                description: description.into(),
                stack: Vec::new(),
            }),
        }
    }

    /// Pushes a (message, field) name location pair on to the location stack.
    ///
    /// Meant to be used only by `Message` implementations.
    #[doc(hidden)]
    pub fn push(&mut self, message: &'static str, field: &'static str) {
        self.inner.stack.push((message, field));
    }
}

impl fmt::Debug for DecodeError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("DecodeError")
            .field("description", &self.inner.description)
            .field("stack", &self.inner.stack)
            .finish()
    }
}

impl fmt::Display for DecodeError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str("failed to decode Protobuf message: ")?;
        for &(message, field) in &self.inner.stack {
            write!(f, "{}.{}: ", message, field)?;
        }
        f.write_str(&self.inner.description)
    }
}

#[cfg(feature = "std")]
impl std::error::Error for DecodeError {}

#[cfg(feature = "std")]
impl From<DecodeError> for std::io::Error {
    fn from(error: DecodeError) -> std::io::Error {
        std::io::Error::new(std::io::ErrorKind::InvalidData, error)
    }
}

/// A Protobuf message encoding error.
///
/// `EncodeError` always indicates that a message failed to encode because the
/// provided buffer had insufficient capacity. Message encoding is otherwise
/// infallible.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct EncodeError {
    required: usize,
    remaining: usize,
}

impl EncodeError {
    /// Creates a new `EncodeError`.
    pub(crate) fn new(required: usize, remaining: usize) -> EncodeError {
        EncodeError {
            required,
            remaining,
        }
    }

    /// Returns the required buffer capacity to encode the message.
    pub fn required_capacity(&self) -> usize {
        self.required
    }

    /// Returns the remaining length in the provided buffer at the time of encoding.
    pub fn remaining(&self) -> usize {
        self.remaining
    }
}

impl fmt::Display for EncodeError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "failed to encode Protobuf messsage; insufficient buffer capacity (required: {}, remaining: {})",
            self.required, self.remaining
        )
    }
}

#[cfg(feature = "std")]
impl std::error::Error for EncodeError {}

#[cfg(feature = "std")]
impl From<EncodeError> for std::io::Error {
    fn from(error: EncodeError) -> std::io::Error {
        std::io::Error::new(std::io::ErrorKind::InvalidInput, error)
    }
}
