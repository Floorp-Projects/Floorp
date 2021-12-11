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

    /// Clone hasher state into a boxed trait object
    fn box_clone(&self) -> Box<DynDigest>;
}

impl<D: Input + FixedOutput + Reset + Clone + 'static> DynDigest for D {
    fn input(&mut self, data: &[u8]) {
        Input::input(self, data);
    }

    fn result_reset(&mut self) -> Box<[u8]> {
        let res = self.clone().fixed_result().to_vec().into_boxed_slice();
        Reset::reset(self);
        res
    }

    fn result(self: Box<Self>) -> Box<[u8]> {
        self.fixed_result().to_vec().into_boxed_slice()
    }

    fn reset(&mut self) {
        Reset::reset(self);
    }

    fn output_size(&self) -> usize {
        <Self as FixedOutput>::OutputSize::to_usize()
    }

    fn box_clone(&self) -> Box<DynDigest> {
        Box::new(self.clone())
    }
}

impl Clone for Box<DynDigest> {
    fn clone(&self) -> Self {
        self.box_clone()
    }
}
