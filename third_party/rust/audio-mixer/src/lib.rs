#[macro_use]
extern crate bitflags;

mod channel;
mod coefficient;

// Export Channel outside.
pub use channel::Channel;
use coefficient::{Coefficient, MixingCoefficient};

use std::default::Default;
use std::fmt::Debug;
use std::ops::{AddAssign, Mul};

// A mixer mixing M-channel input data to N-channel output data.
// T::Coef is an associated type defined in MixingCoefficient, which indicates the type of the
// mixing coefficient that would be used for type T. When T is f32, the T::Coef is f32. When T
// is i16, the T::Coef is i32. When mixing data, a temporary variable with type T::Coef would be
// created to hold the mixing result. Since the type of input and output audio data is T,
// the methods provided from MixingCoefficient trait would be used to convert the value between
// type T and T::Coef.
#[derive(Debug)]
pub struct Mixer<T>
where
    T: Copy + Debug + MixingCoefficient,
    T::Coef: AddAssign + Copy + Debug + Default + Mul<T::Coef, Output = T::Coef>,
{
    coefficient: Coefficient<T>,
}

impl<T> Mixer<T>
where
    T: Copy + Debug + MixingCoefficient,
    T::Coef: AddAssign + Copy + Debug + Default + Mul<T::Coef, Output = T::Coef>,
{
    pub fn new(input_channels: &[Channel], output_channels: &[Channel]) -> Self {
        Self {
            coefficient: Coefficient::create(input_channels, output_channels),
        }
    }

    // To mix M-channel audio input data to N-channel output data, the data in output-channel i
    // is the sum of product of data in input-channel j and the coefficient for mixing from
    // input-channel j to output-channel i, for all j in M channels. That is,
    // output_data(i) = Σ coefficient(j, i) * input_data(j), for all j in [0, M),
    // where i is in [0, N) and coefficient is a function returning mixing coefficient from
    // input channel j to output channel i.
    pub fn mix(&self, input_buffer: &[T], output_buffer: &mut [T]) {
        assert_eq!(
            input_buffer.len(),
            self.input_channels().len(),
            "input slice must have the same size as the input channel's one."
        );
        assert_eq!(
            output_buffer.len(),
            self.output_channels().len(),
            "output slice must have the same size as the output channel's one."
        );
        for (i, output) in output_buffer.iter_mut().enumerate() {
            // T must implement Default that returns a zero value from default().
            let mut value = T::Coef::default(); // Create a zero value.
            for (j, input) in input_buffer.iter().enumerate() {
                // T::Coef needs to implement `AddAssign` and `Mul` to make `+=` and `*` work.
                // T needs to implement `Copy` so `*input` can be copied.
                value += self.coefficient.get(j, i) * T::to_coefficient_value(*input);
            }
            *output = T::from_coefficient_value(
                value,
                self.coefficient.would_overflow_from_coefficient_value(),
            );
        }
    }

    pub fn input_channels(&self) -> &[Channel] {
        self.coefficient.input_channels()
    }

    pub fn output_channels(&self) -> &[Channel] {
        self.coefficient.output_channels()
    }
}
