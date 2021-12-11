use super::{Input, FixedOutput, Reset};
use generic_array::{GenericArray, ArrayLength};
use generic_array::typenum::Unsigned;

/// The `Digest` trait specifies an interface common for digest functions.
///
/// It's a convenience wrapper around `Input`, `FixedOutput`, `Reset`, `Clone`,
/// and `Default` traits. It also provides additional convenience methods.
pub trait Digest {
    type OutputSize: ArrayLength<u8>;
    /// Create new hasher instance
    fn new() -> Self;

    /// Digest input data.
    ///
    /// This method can be called repeatedly for use with streaming messages.
    fn input<B: AsRef<[u8]>>(&mut self, data: B);

    /// Digest input data in a chained manner.
    fn chain<B: AsRef<[u8]>>(self, data: B) -> Self where Self: Sized;

    /// Retrieve result and consume hasher instance.
    fn result(self) -> GenericArray<u8, Self::OutputSize>;

    /// Retrieve result and reset hasher instance.
    ///
    /// This method sometimes can be more efficient compared to hasher
    /// re-creation.
    fn result_reset(&mut self) -> GenericArray<u8, Self::OutputSize>;

    /// Reset hasher instance to its initial state.
    fn reset(&mut self);

    /// Get output size of the hasher
    fn output_size() -> usize;

    /// Convenience function to compute hash of the `data`. It will handle
    /// hasher creation, data feeding and finalization.
    ///
    /// Example:
    ///
    /// ```rust,ignore
    /// println!("{:x}", sha2::Sha256::digest(b"Hello world"));
    /// ```
    fn digest(data: &[u8]) -> GenericArray<u8, Self::OutputSize>;
}

impl<D: Input + FixedOutput + Reset + Clone + Default> Digest for D {
    type OutputSize = <Self as FixedOutput>::OutputSize;

    fn new() -> Self {
        Self::default()
    }

    fn input<B: AsRef<[u8]>>(&mut self, data: B) {
        Input::input(self, data);
    }

    fn chain<B: AsRef<[u8]>>(self, data: B) -> Self where Self: Sized {
        Input::chain(self, data)
    }

    fn result(self) -> GenericArray<u8, Self::OutputSize> {
        self.fixed_result()
    }

    fn result_reset(&mut self) -> GenericArray<u8, Self::OutputSize> {
        let res = self.clone().fixed_result();
        self.reset();
        res
    }

    fn reset(&mut self) {
        <Self as Reset>::reset(self)
    }

    fn output_size() -> usize {
        Self::OutputSize::to_usize()
    }

    fn digest(data: &[u8]) -> GenericArray<u8, Self::OutputSize> {
        let mut hasher = Self::default();
        Input::input(&mut hasher, data);
        hasher.fixed_result()
    }
}
