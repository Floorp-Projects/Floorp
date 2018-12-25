#![cfg(feature = "std")]
use std::boxed::Box;

use super::{Input, FixedOutput, Reset};
use generic_array::typenum::Unsigned;

/// The `DynDigest` trait is a modification of `Digest` trait suitable
/// for trait objects.
pub trait DynDigest {
    /// Digest input data.
    ///
    /// This method can be called repeatedly for use with streaming messages.
    fn input(&mut self, data: &[u8]);

    /// Retrieve result and reset hasher instance
    fn result_reset(&mut self) -> Box<[u8]>;

    /// Retrieve result and consume boxed hasher instance
    fn result(self: Box<Self>) -> Box<[u8]>;

    /// Reset hasher instance to its initial state.
    fn reset(&mut self);

    /// Get output size of the hasher
    fn output_size(&self) -> usize;
}

impl<D: Input + FixedOutput + Reset + Clone> DynDigest for D {
    /// Digest input data.
    ///
    /// This method can be called repeatedly for use with streaming messages.
    fn input(&mut self, data: &[u8]) {
        Input::input(self, data);
    }

    /// Retrieve result and reset hasher instance
    fn result_reset(&mut self) -> Box<[u8]> {
        let res = self.clone().fixed_result().to_vec().into_boxed_slice();
        Reset::reset(self);
        res
    }

    /// Retrieve result and consume boxed hasher instance
    fn result(self: Box<Self>) -> Box<[u8]> {
        self.fixed_result().to_vec().into_boxed_slice()
    }

    fn reset(&mut self) {
        Reset::reset(self);
    }

    /// Get output size of the hasher
    fn output_size(&self) -> usize {
        <Self as FixedOutput>::OutputSize::to_usize()
    }
}
