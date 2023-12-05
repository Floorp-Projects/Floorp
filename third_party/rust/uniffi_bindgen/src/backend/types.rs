/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! # Backend traits
//!
//! A trait to help format items.
//!
//! Each backend will have its own `filter` module, which is used by the askama templates.
use super::Literal;
use std::fmt::Debug;

// XXX - Note that this trait is not used internally. It exists just to avoid an unnecessary
// breaking change for external bindings which use this trait.
// It is likely to be removed some time after 0.26.x.

/// A Trait to help render types in a language specific format.
pub trait CodeType: Debug {
    /// The language specific label used to reference this type. This will be used in
    /// method signatures and property declarations.
    fn type_label(&self) -> String;

    /// A representation of this type label that can be used as part of another
    /// identifier. e.g. `read_foo()`, or `FooInternals`.
    ///
    /// This is especially useful when creating specialized objects or methods to deal
    /// with this type only.
    fn canonical_name(&self) -> String {
        self.type_label()
    }

    fn literal(&self, _literal: &Literal) -> String {
        unimplemented!("Unimplemented for {}", self.type_label())
    }

    /// Name of the FfiConverter
    ///
    /// This is the object that contains the lower, write, lift, and read methods for this type.
    /// Depending on the binding this will either be a singleton or a class with static methods.
    ///
    /// This is the newer way of handling these methods and replaces the lower, write, lift, and
    /// read CodeType methods.  Currently only used by Kotlin, but the plan is to move other
    /// backends to using this.
    fn ffi_converter_name(&self) -> String {
        format!("FfiConverter{}", self.canonical_name())
    }

    /// An expression for lowering a value into something we can pass over the FFI.
    fn lower(&self) -> String {
        format!("{}.lower", self.ffi_converter_name())
    }

    /// An expression for writing a value into a byte buffer.
    fn write(&self) -> String {
        format!("{}.write", self.ffi_converter_name())
    }

    /// An expression for lifting a value from something we received over the FFI.
    fn lift(&self) -> String {
        format!("{}.lift", self.ffi_converter_name())
    }

    /// An expression for reading a value from a byte buffer.
    fn read(&self) -> String {
        format!("{}.read", self.ffi_converter_name())
    }

    /// A list of imports that are needed if this type is in use.
    /// Classes are imported exactly once.
    fn imports(&self) -> Option<Vec<String>> {
        None
    }

    /// Function to run at startup
    fn initialization_fn(&self) -> Option<String> {
        None
    }
}
