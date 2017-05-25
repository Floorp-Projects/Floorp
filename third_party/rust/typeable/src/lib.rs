#![deny(missing_docs)]
#![deny(warnings)]

//! Exposes `Typeable`, which exposes the `get_type` method, which gives
//! the `TypeId` of any 'static type.

use std::any::{Any, TypeId};

/// Universal mixin trait for adding a `get_type` method.
///
pub trait Typeable: Any {
    /// Get the `TypeId` of this object.
    #[inline(always)]
    fn get_type(&self) -> TypeId { TypeId::of::<Self>() }
}

impl<T: Any> Typeable for T {}

