// The code is based from libcubeb's cubeb_mixer.cpp,
// which adapts the code from libswresample's rematrix.c

use crate::channel::{Channel, ChannelMap};

use std::fmt::Debug;

const CHANNELS: usize = Channel::count();

#[derive(Debug)]
enum Error {
    DuplicateNonSilenceChannel,
    AsymmetricChannels,
}

#[derive(Debug)]
struct ChannelLayout {
    channels: Vec<Channel>,
    channel_map: ChannelMap,
}

impl ChannelLayout {
    fn new(channels: &[Channel]) -> Result<Self, Error> {
        let channel_map = Self::get_channel_map(channels)?;
        Ok(Self {
            channels: channels.to_vec(),
            channel_map,
        })
    }

    // Except Silence channel, the duplicate channels are not allowed.
    fn get_channel_map(channels: &[Channel]) -> Result<ChannelMap, Error> {
        let mut map = ChannelMap::empty();
        for channel in channels {
            let bitmask = ChannelMap::from(*channel);
            if channel != &Channel::Silence && map.contains(bitmask) {
                return Err(Error::DuplicateNonSilenceChannel);
            }
            map.insert(bitmask);
        }
        Ok(map)
    }
}

#[derive(Debug)]
pub struct Coefficient<T>
where
    T: MixingCoefficient,
    T::Coef: Copy,
{
    input_layout: ChannelLayout,
    output_layout: ChannelLayout,
    matrix: Vec<Vec<T::Coef>>,
    would_overflow_from_coefficient_value: Option<bool>, // Only used when T is i16
}

impl<T> Coefficient<T>
where
    T: MixingCoefficient,
    T::Coef: Copy,
{
    // Given a M-channel input layout and a N-channel output layout, generate a NxM coefficients
    // matrix m such that out_audio = m * in_audio, where in_audio, out_audio are Mx1, Nx1 matrix
    // storing input and output audio data in their rows respectively.
    //
    // data in channel #1 ▸ │ Silence    │   │ 0, 0, 0, 0 │   │ FrontRight   │ ◂ data in channel #1
    // data in channel #2 ▸ │ FrontRight │ = │ 1, C, 0, L │ x │ FrontCenter  │ ◂ data in channel #2
    // data in channel #3 ▸ │ FrontLeft  │   │ 0, C, 1, L │   │ FrontLeft    │ ◂ data in channel #3
    //                            ▴                 ▴         │ LowFrequency │ ◂ data in channel #4
    //                            ┊                 ┊                ▴
    //                            ┊                 ┊                ┊
    //                        out_audio      mixing matrix m       in_audio
    //
    // The FrontLeft, FrontRight, ... etc label the data for front-left, front-right ... etc channel
    // in both input and output audio data buffer.
    //
    // C and L are coefficients mixing input data from front-center channel and low-frequency channel
    // to front-left and front-right.
    //
    // In math, the in_audio and out_audio should be a 2D-matrix with several rows containing only
    // one column. However, the in_audio and out_audio are passed by 1-D matrix here for convenience.
    pub fn create(input_channels: &[Channel], output_channels: &[Channel]) -> Self {
        let input_layout = ChannelLayout::new(input_channels).expect("Invalid input layout");
        let output_layout = ChannelLayout::new(output_channels).expect("Invalid output layout");

        let mixing_matrix =
            Self::build_mixing_matrix(input_layout.channel_map, output_layout.channel_map)
                .unwrap_or_else(|_| Self::get_basic_matrix());

        let coefficient_matrix = Self::pick_coefficients(
            &input_layout.channels,
            &output_layout.channels,
            &mixing_matrix,
        );

        let normalized_matrix = Self::normalize(T::max_coefficients_sum(), coefficient_matrix);

        let would_overflow = T::would_overflow_from_coefficient_value(&normalized_matrix);

        // Convert the type of the coefficients from f64 to T::Coef.
        let matrix = normalized_matrix
            .into_iter()
            .map(|row| row.into_iter().map(T::coefficient_from_f64).collect())
            .collect();

        Self {
            input_layout,
            output_layout,
            matrix,
            would_overflow_from_coefficient_value: would_overflow,
        }
    }

    // Return the coefficient for mixing input channel data into output channel.
    pub fn get(&self, input: usize, output: usize) -> T::Coef {
        assert!(output < self.matrix.len());
        assert!(input < self.matrix[output].len());
        self.matrix[output][input] // Perform copy so T::Coef must implement Copy.
    }

    pub fn would_overflow_from_coefficient_value(&self) -> Option<bool> {
        self.would_overflow_from_coefficient_value
    }

    pub fn input_channels(&self) -> &[Channel] {
        &self.input_layout.channels
    }

    pub fn output_channels(&self) -> &[Channel] {
        &self.output_layout.channels
    }

    // Given audio input and output channel-maps, generate a CxC mixing coefficients matrix M,
    // whose indice are ordered by the values defined in enum Channel, such that
    // output_data(i) = Σ M[i][j] * input_data(j), for all j in [0, C),
    // where i is in [0, C) and C is the number of channels defined in enum Channel,
    // output_data and input_data are buffers containing data for channels that are also ordered
    // by the values defined in enum Channel.
    //
    // │ FrontLeft    │   │ 1, 0, ..., 0 │   │ FrontLeft    │ ◂ data in front-left channel
    // │ FrontRight   │   │ 0, 1, ..., 0 │   │ FrontRight   │ ◂ data in front-right channel
    // │ FrontCenter  │ = │ ........., 0 │ x │ FrontCenter  │ ◂ data in front-center channel
    // │ ...........  │   │ ........., 0 |   │ ...........  │ ◂ ...
    // │ Silence      │   │ 0, 0, ..., 0 |   │ Silence      │ ◂ data in silence channel
    //        ▴                   ▴                  ▴
    //    out_audio         coef matrix M          in_audio
    //
    // ChannelMap would be used as a hash table to check the existence of channels.
    #[allow(clippy::cognitive_complexity)]
    fn build_mixing_matrix(
        input_map: ChannelMap,
        output_map: ChannelMap,
    ) -> Result<[[f64; CHANNELS]; CHANNELS], Error> {
        // Mixing coefficients constants.
        use std::f64::consts::FRAC_1_SQRT_2;
        use std::f64::consts::SQRT_2;
        const CENTER_MIX_LEVEL: f64 = FRAC_1_SQRT_2;
        const SURROUND_MIX_LEVEL: f64 = FRAC_1_SQRT_2;
        const LFE_MIX_LEVEL: f64 = 1.0;

        // The indices of channels in the mixing coefficients matrix.
        const FRONT_LEFT: usize = Channel::FrontLeft.number();
        const FRONT_RIGHT: usize = Channel::FrontRight.number();
        const FRONT_CENTER: usize = Channel::FrontCenter.number();
        const LOW_FREQUENCY: usize = Channel::LowFrequency.number();
        const BACK_LEFT: usize = Channel::BackLeft.number();
        const BACK_RIGHT: usize = Channel::BackRight.number();
        const FRONT_LEFT_OF_CENTER: usize = Channel::FrontLeftOfCenter.number();
        const FRONT_RIGHT_OF_CENTER: usize = Channel::FrontRightOfCenter.number();
        const BACK_CENTER: usize = Channel::BackCenter.number();
        const SIDE_LEFT: usize = Channel::SideLeft.number();
        const SIDE_RIGHT: usize = Channel::SideRight.number();

        // Return true if mixable channels are symmetric.
        fn is_symmetric(map: ChannelMap) -> bool {
            fn even(map: ChannelMap) -> bool {
                map.bits().count_ones() % 2 == 0
            }
            even(map & ChannelMap::FRONT_2)
                && even(map & ChannelMap::BACK_2)
                && even(map & ChannelMap::FRONT_2_OF_CENTER)
                && even(map & ChannelMap::SIDE_2)
        }

        if !is_symmetric(input_map) || !is_symmetric(output_map) {
            return Err(Error::AsymmetricChannels);
        }

        let mut matrix = Self::get_basic_matrix();

        // Get input channels that are not in the output channels.
        let unaccounted_input_map = input_map & !output_map;

        // When input has front-center but output has not, and output has front-stereo,
        // mix input's front-center to output's front-stereo.
        if unaccounted_input_map.contains(ChannelMap::FRONT_CENTER)
            && output_map.contains(ChannelMap::FRONT_2)
        {
            let coefficient = if input_map.contains(ChannelMap::FRONT_2) {
                CENTER_MIX_LEVEL
            } else {
                FRAC_1_SQRT_2
            };
            matrix[FRONT_LEFT][FRONT_CENTER] += coefficient;
            matrix[FRONT_RIGHT][FRONT_CENTER] += coefficient;
        }

        // When input has front-stereo but output has not, and output has front-center,
        // mix input's front-stereo to output's front-center.
        if unaccounted_input_map.contains(ChannelMap::FRONT_2)
            && output_map.contains(ChannelMap::FRONT_CENTER)
        {
            matrix[FRONT_CENTER][FRONT_LEFT] += FRAC_1_SQRT_2;
            matrix[FRONT_CENTER][FRONT_RIGHT] += FRAC_1_SQRT_2;
            if input_map.contains(ChannelMap::FRONT_CENTER) {
                matrix[FRONT_CENTER][FRONT_CENTER] = CENTER_MIX_LEVEL * SQRT_2;
            }
        }

        // When input has back-center but output has not,
        if unaccounted_input_map.contains(ChannelMap::BACK_CENTER) {
            // if output has back-stereo, mix input's back-center to output's back-stereo.
            if output_map.contains(ChannelMap::BACK_2) {
                matrix[BACK_LEFT][BACK_CENTER] += FRAC_1_SQRT_2;
                matrix[BACK_RIGHT][BACK_CENTER] += FRAC_1_SQRT_2;
            // or if output has side-stereo, mix input's back-center to output's side-stereo.
            } else if output_map.contains(ChannelMap::SIDE_2) {
                matrix[SIDE_LEFT][BACK_CENTER] += FRAC_1_SQRT_2;
                matrix[SIDE_RIGHT][BACK_CENTER] += FRAC_1_SQRT_2;
            // or if output has front-stereo, mix input's back-center to output's front-stereo.
            } else if output_map.contains(ChannelMap::FRONT_2) {
                matrix[FRONT_LEFT][BACK_CENTER] += SURROUND_MIX_LEVEL * FRAC_1_SQRT_2;
                matrix[FRONT_RIGHT][BACK_CENTER] += SURROUND_MIX_LEVEL * FRAC_1_SQRT_2;
            // or if output has front-center, mix input's back-center to output's front-center.
            } else if output_map.contains(ChannelMap::FRONT_CENTER) {
                matrix[FRONT_CENTER][BACK_CENTER] += SURROUND_MIX_LEVEL * FRAC_1_SQRT_2;
            }
        }

        // When input has back-stereo but output has not,
        if unaccounted_input_map.contains(ChannelMap::BACK_2) {
            // if output has back-center, mix input's back-stereo to output's back-center.
            if output_map.contains(ChannelMap::BACK_CENTER) {
                matrix[BACK_CENTER][BACK_LEFT] += FRAC_1_SQRT_2;
                matrix[BACK_CENTER][BACK_RIGHT] += FRAC_1_SQRT_2;
            // or if output has side-stereo, mix input's back-stereo to output's side-stereo.
            } else if output_map.contains(ChannelMap::SIDE_2) {
                let coefficient = if input_map.contains(ChannelMap::SIDE_2) {
                    FRAC_1_SQRT_2
                } else {
                    1.0
                };
                matrix[SIDE_LEFT][BACK_LEFT] += coefficient;
                matrix[SIDE_RIGHT][BACK_RIGHT] += coefficient;
            // or if output has front-stereo, mix input's back-stereo to output's side-stereo.
            } else if output_map.contains(ChannelMap::FRONT_2) {
                matrix[FRONT_LEFT][BACK_LEFT] += SURROUND_MIX_LEVEL;
                matrix[FRONT_RIGHT][BACK_RIGHT] += SURROUND_MIX_LEVEL;
            // or if output has front-center, mix input's back-stereo to output's front-center.
            } else if output_map.contains(ChannelMap::FRONT_CENTER) {
                matrix[FRONT_CENTER][BACK_LEFT] += SURROUND_MIX_LEVEL * FRAC_1_SQRT_2;
                matrix[FRONT_CENTER][BACK_RIGHT] += SURROUND_MIX_LEVEL * FRAC_1_SQRT_2;
            }
        }

        // When input has side-stereo but output has not,
        if unaccounted_input_map.contains(ChannelMap::SIDE_2) {
            // if output has back-stereo, mix input's side-stereo to output's back-stereo.
            if output_map.contains(ChannelMap::BACK_2) {
                let coefficient = if input_map.contains(ChannelMap::BACK_2) {
                    FRAC_1_SQRT_2
                } else {
                    1.0
                };
                matrix[BACK_LEFT][SIDE_LEFT] += coefficient;
                matrix[BACK_RIGHT][SIDE_RIGHT] += coefficient;
            // or if output has back-center, mix input's side-stereo to output's back-center.
            } else if output_map.contains(ChannelMap::BACK_CENTER) {
                matrix[BACK_CENTER][SIDE_LEFT] += FRAC_1_SQRT_2;
                matrix[BACK_CENTER][SIDE_RIGHT] += FRAC_1_SQRT_2;
            // or if output has front-stereo, mix input's side-stereo to output's front-stereo.
            } else if output_map.contains(ChannelMap::FRONT_2) {
                matrix[FRONT_LEFT][SIDE_LEFT] += SURROUND_MIX_LEVEL;
                matrix[FRONT_RIGHT][SIDE_RIGHT] += SURROUND_MIX_LEVEL;
            // or if output has front-center, mix input's side-stereo to output's front-center.
            } else if output_map.contains(ChannelMap::FRONT_CENTER) {
                matrix[FRONT_CENTER][SIDE_LEFT] += SURROUND_MIX_LEVEL * FRAC_1_SQRT_2;
                matrix[FRONT_CENTER][SIDE_RIGHT] += SURROUND_MIX_LEVEL * FRAC_1_SQRT_2;
            }
        }

        // When input has front-stereo-of-center but output has not,
        if unaccounted_input_map.contains(ChannelMap::FRONT_2_OF_CENTER) {
            // if output has front-stereo, mix input's front-stereo-of-center to output's front-stereo.
            if output_map.contains(ChannelMap::FRONT_2) {
                matrix[FRONT_LEFT][FRONT_LEFT_OF_CENTER] += 1.0;
                matrix[FRONT_RIGHT][FRONT_RIGHT_OF_CENTER] += 1.0;
            // or if output has front-center, mix input's front-stereo-of-center to output's front-center.
            } else if output_map.contains(ChannelMap::FRONT_CENTER) {
                matrix[FRONT_CENTER][FRONT_LEFT_OF_CENTER] += FRAC_1_SQRT_2;
                matrix[FRONT_CENTER][FRONT_RIGHT_OF_CENTER] += FRAC_1_SQRT_2;
            }
        }

        // When input has low-frequency but output has not,
        if unaccounted_input_map.contains(ChannelMap::LOW_FREQUENCY) {
            // if output has front-center, mix input's low-frequency to output's front-center.
            if output_map.contains(ChannelMap::FRONT_CENTER) {
                matrix[FRONT_CENTER][LOW_FREQUENCY] += LFE_MIX_LEVEL;
            // or if output has front-stereo, mix input's low-frequency to output's front-stereo.
            } else if output_map.contains(ChannelMap::FRONT_2) {
                matrix[FRONT_LEFT][LOW_FREQUENCY] += LFE_MIX_LEVEL * FRAC_1_SQRT_2;
                matrix[FRONT_RIGHT][LOW_FREQUENCY] += LFE_MIX_LEVEL * FRAC_1_SQRT_2;
            }
        }

        Ok(matrix)
    }

    // Return a CHANNELSxCHANNELS matrix M that is (CHANNELS-1)x(CHANNELS-1) identity matrix
    // padding with one extra row and one column containing only zero values. The result would be:
    //
    //      identity      padding
    //       matrix       column
    //          ▾           ▾
    // ┌┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┐                       i  ┐
    // │ 1, 0, 0, ..., 0 ┊, 0 │                ◂ 0  ┊ channel i
    // │ 0, 1, 0, ..., 0 ┊, 0 │                ◂ 1  ┊ for
    // │ 0, 0, 1, ..., 0 ┊, 0 │                ◂ 2  ┊ audio
    // │ 0, 0, 0, ..., 0 ┊, 0 │                .    ┊ output
    // │ ............... ┊    │                .    ┊
    // │ 0, 0, 0, ..., 1 ┊, 0 │                ◂ 16 ┊
    // ├┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┼┈┈┈┈┤                ◂ 17 ┊
    // │ 0, 0, 0, ..., 0 ┊, 0 │ ◂ padding row  ◂ 18 ┊
    //   ▴  ▴  ▴ ....  ▴    ▴                       ┘
    // j 0  1  2 ....  17   18
    // └┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┘
    // channel j for audio input
    //
    // Given an audio input buffer, in_audio, and an output buffer, out_audio,
    // and their channel data are both ordered by the values defined in enum Channel.
    // The generated matrix M makes sure that:
    //
    // out_audio(i) = in_audio(j), if i == j and both i, j are non-silence channel
    //              = 0,           if i != j or i, j are silence channel
    //
    // │ FrontLeft    │       │ FrontLeft    │ ◂ data in front-left channel
    // │ FrontRight   │       │ FrontRight   │ ◂ data in front-right channel
    // │ FrontCenter  │ = M x │ FrontCenter  │ ◂ data in front-center channel
    // │ ...........  │       │ ...........  │ ◂ ...
    // │ Silence      │       │ Silence      │ ◂ data in silence channel
    //        ▴                       ▴
    //    out_audio               in_audio
    //
    // That is,
    // 1. If the input-channel is silence, it won't be mixed into any channel.
    // 2. If the output-channel is silence, its output-channel data will be zero (silence).
    // 3. If input-channel j is different from output-channel i, audio data in input channel j
    //    won't be mixed into the audio output data in channel i
    // 4. If input-channel j is same as output-channel i,  audio data in input channel j will be
    //    copied to audio output data in channel i
    //
    fn get_basic_matrix() -> [[f64; CHANNELS]; CHANNELS] {
        const SILENCE: usize = Channel::Silence.number();
        let mut matrix = [[0.0; CHANNELS]; CHANNELS];
        for (i, row) in matrix.iter_mut().enumerate() {
            if i != SILENCE {
                row[i] = 1.0;
            }
        }
        matrix
    }

    // Given is an CHANNELSxCHANNELS mixing matrix whose indice are ordered by the values defined
    // in enum Channel, and the channel orders of M-channel input and N-channel output, generate a
    // mixing matrix m such that output_data(i) = Σ m[i][j] * input_data(j), for all j in [0, M),
    // where i is in [0, N) and {input/output}_data(k) means the data of the number k channel in
    // the input/output buffer.
    fn pick_coefficients(
        input_channels: &[Channel],
        output_channels: &[Channel],
        source: &[[f64; CHANNELS]; CHANNELS],
    ) -> Vec<Vec<f64>> {
        let mut matrix = Vec::with_capacity(output_channels.len());
        for output_channel in output_channels {
            let output_channel_index = (*output_channel).number();
            let mut coefficients = Vec::with_capacity(input_channels.len());
            for input_channel in input_channels {
                let input_channel_index = (*input_channel).number();
                coefficients.push(source[output_channel_index][input_channel_index]);
            }
            matrix.push(coefficients);
        }
        matrix
    }

    fn normalize(max_coefficients_sum: f64, mut coefficients: Vec<Vec<f64>>) -> Vec<Vec<f64>> {
        let mut max_sum: f64 = 0.0;
        for coefs in &coefficients {
            max_sum = max_sum.max(coefs.iter().sum());
        }
        if max_sum != 0.0 && max_sum > max_coefficients_sum {
            max_sum /= max_coefficients_sum;
            for coefs in &mut coefficients {
                for coef in coefs {
                    *coef /= max_sum;
                }
            }
        }
        coefficients
    }
}

pub trait MixingCoefficient {
    type Coef;

    // TODO: These should be private.
    fn max_coefficients_sum() -> f64; // Used for normalizing.
    fn coefficient_from_f64(value: f64) -> Self::Coef;
    // Precheck if overflow occurs when converting value from Self::Coef type to Self type.
    fn would_overflow_from_coefficient_value(coefficient: &[Vec<f64>]) -> Option<bool>;

    // Convert f32 (Self) -> f32 (Self::Coef) or i16 (Self) -> i32 (Self::Coef)
    #[allow(clippy::wrong_self_convention)]
    fn to_coefficient_value(value: Self) -> Self::Coef;
    fn from_coefficient_value(value: Self::Coef, would_overflow: Option<bool>) -> Self;
}

impl MixingCoefficient for f32 {
    type Coef = f32;

    fn max_coefficients_sum() -> f64 {
        f64::from(std::i32::MAX)
    }

    fn coefficient_from_f64(value: f64) -> Self::Coef {
        value as Self::Coef
    }

    fn would_overflow_from_coefficient_value(_coefficient: &[Vec<f64>]) -> Option<bool> {
        None
    }

    fn to_coefficient_value(value: Self) -> Self::Coef {
        value
    }

    fn from_coefficient_value(value: Self::Coef, would_overflow: Option<bool>) -> Self {
        assert!(would_overflow.is_none());
        value
    }
}

impl MixingCoefficient for i16 {
    type Coef = i32;

    fn max_coefficients_sum() -> f64 {
        1.0
    }

    fn coefficient_from_f64(value: f64) -> Self::Coef {
        (value * f64::from(1 << 15)).round() as Self::Coef
    }

    fn would_overflow_from_coefficient_value(coefficient: &[Vec<f64>]) -> Option<bool> {
        let mut max_sum: Self::Coef = 0;
        for row in coefficient {
            let mut sum: Self::Coef = 0;
            let mut rem: f64 = 0.0;
            for coef in row {
                let target = coef * f64::from(1 << 15) + rem;
                let value = target.round() as Self::Coef;
                rem += target - target.round();
                sum += value.abs();
            }
            max_sum = max_sum.max(sum);
        }
        Some(max_sum > (1 << 15))
    }

    fn to_coefficient_value(value: Self) -> Self::Coef {
        Self::Coef::from(value)
    }

    fn from_coefficient_value(value: Self::Coef, would_overflow: Option<bool>) -> Self {
        use std::convert::TryFrom;
        let would_overflow = would_overflow.expect("would_overflow must have value for i16 type");
        let mut converted = (value + (1 << 14)) >> 15;
        // clip the signed integer value into the -32768,32767 range.
        if would_overflow && ((converted + 0x8000) & !0xFFFF != 0) {
            converted = (converted >> 31) ^ 0x7FFF;
        }
        Self::try_from(converted).expect("Cannot convert coefficient from i32 to i16")
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_create_f32() {
        test_create::<f32>(MixDirection::Downmix);
        test_create::<f32>(MixDirection::Upmix);
    }

    #[test]
    fn test_create_i16() {
        test_create::<i16>(MixDirection::Downmix);
        test_create::<i16>(MixDirection::Upmix);
    }

    fn test_create<T>(direction: MixDirection)
    where
        T: MixingCoefficient,
        T::Coef: Copy + Debug,
    {
        let (input_channels, output_channels) = get_test_channels(direction);
        let coefficient = Coefficient::<T>::create(&input_channels, &output_channels);
        println!(
            "{:?} = {:?} * {:?}",
            output_channels, coefficient.matrix, input_channels
        );
    }

    enum MixDirection {
        Downmix,
        Upmix,
    }
    fn get_test_channels(direction: MixDirection) -> (Vec<Channel>, Vec<Channel>) {
        let more = vec![
            Channel::Silence,
            Channel::FrontRight,
            Channel::FrontLeft,
            Channel::LowFrequency,
            Channel::Silence,
            Channel::BackCenter,
        ];
        let less = vec![
            Channel::FrontLeft,
            Channel::Silence,
            Channel::FrontRight,
            Channel::FrontCenter,
        ];
        match direction {
            MixDirection::Downmix => (more, less),
            MixDirection::Upmix => (less, more),
        }
    }

    #[test]
    fn test_create_with_duplicate_silience_channels_f32() {
        test_create_with_duplicate_silience_channels::<f32>()
    }

    #[test]
    fn test_create_with_duplicate_silience_channels_i16() {
        test_create_with_duplicate_silience_channels::<i16>()
    }

    #[test]
    #[should_panic]
    fn test_create_with_duplicate_input_channels_f32() {
        test_create_with_duplicate_input_channels::<f32>()
    }

    #[test]
    #[should_panic]
    fn test_create_with_duplicate_input_channels_i16() {
        test_create_with_duplicate_input_channels::<i16>()
    }

    #[test]
    #[should_panic]
    fn test_create_with_duplicate_output_channels_f32() {
        test_create_with_duplicate_output_channels::<f32>()
    }

    #[test]
    #[should_panic]
    fn test_create_with_duplicate_output_channels_i16() {
        test_create_with_duplicate_output_channels::<i16>()
    }

    fn test_create_with_duplicate_silience_channels<T>()
    where
        T: MixingCoefficient,
        T::Coef: Copy,
    {
        // Duplicate of Silence channels is allowed on both input side and output side.
        let input_channels = [
            Channel::FrontLeft,
            Channel::Silence,
            Channel::FrontRight,
            Channel::FrontCenter,
            Channel::Silence,
        ];
        let output_channels = [
            Channel::Silence,
            Channel::FrontRight,
            Channel::FrontLeft,
            Channel::BackCenter,
            Channel::Silence,
        ];
        let _ = Coefficient::<T>::create(&input_channels, &output_channels);
    }

    fn test_create_with_duplicate_input_channels<T>()
    where
        T: MixingCoefficient,
        T::Coef: Copy,
    {
        let input_channels = [
            Channel::FrontLeft,
            Channel::Silence,
            Channel::FrontLeft,
            Channel::FrontCenter,
        ];
        let output_channels = [
            Channel::Silence,
            Channel::FrontRight,
            Channel::FrontLeft,
            Channel::FrontCenter,
            Channel::BackCenter,
        ];
        let _ = Coefficient::<T>::create(&input_channels, &output_channels);
    }

    fn test_create_with_duplicate_output_channels<T>()
    where
        T: MixingCoefficient,
        T::Coef: Copy,
    {
        let input_channels = [
            Channel::FrontLeft,
            Channel::Silence,
            Channel::FrontRight,
            Channel::FrontCenter,
        ];
        let output_channels = [
            Channel::Silence,
            Channel::FrontRight,
            Channel::FrontLeft,
            Channel::FrontCenter,
            Channel::FrontCenter,
            Channel::BackCenter,
        ];
        let _ = Coefficient::<T>::create(&input_channels, &output_channels);
    }

    #[test]
    fn test_get_redirect_matrix_f32() {
        test_get_redirect_matrix::<f32>();
    }

    #[test]
    fn test_get_redirect_matrix_i16() {
        test_get_redirect_matrix::<i16>();
    }

    fn test_get_redirect_matrix<T>()
    where
        T: MixingCoefficient,
        T::Coef: Copy + Debug + PartialEq,
    {
        // Create a matrix that only redirect the channels from input side to output side,
        // without mixing input audio data to output audio data.
        fn compute_redirect_matrix<T>(
            input_channels: &[Channel],
            output_channels: &[Channel],
        ) -> Vec<Vec<T::Coef>>
        where
            T: MixingCoefficient,
        {
            let mut matrix = Vec::with_capacity(output_channels.len());
            for output_channel in output_channels {
                let mut row = Vec::with_capacity(input_channels.len());
                for input_channel in input_channels {
                    row.push(
                        if input_channel != output_channel
                            || input_channel == &Channel::Silence
                            || output_channel == &Channel::Silence
                        {
                            0.0
                        } else {
                            1.0
                        },
                    );
                }
                matrix.push(row);
            }

            // Convert the type of the coefficients from f64 to T::Coef.
            matrix
                .into_iter()
                .map(|row| row.into_iter().map(T::coefficient_from_f64).collect())
                .collect()
        }

        let input_channels = [
            Channel::FrontLeft,
            Channel::Silence,
            Channel::FrontRight,
            Channel::FrontCenter,
        ];
        let output_channels = [
            Channel::Silence,
            Channel::FrontLeft,
            Channel::Silence,
            Channel::FrontCenter,
            Channel::BackCenter,
        ];

        // Get a redirect matrix since the output layout is asymmetric.
        let coefficient = Coefficient::<T>::create(&input_channels, &output_channels);

        let expected = compute_redirect_matrix::<T>(&input_channels, &output_channels);
        assert_eq!(coefficient.matrix, expected);

        println!(
            "{:?} = {:?} * {:?}",
            output_channels, coefficient.matrix, input_channels
        );
    }

    #[test]
    fn test_normalize() {
        use float_cmp::approx_eq;

        let m = vec![
            vec![1.0_f64, 2.0_f64, 3.0_f64],
            vec![4.0_f64, 6.0_f64, 10.0_f64],
        ];

        let mut max_row_sum: f64 = std::f64::MIN;
        for row in &m {
            max_row_sum = max_row_sum.max(row.iter().sum());
        }

        // Type of Coefficient doesn't matter here.
        // If the first argument of normalize >= max_row_sum, do nothing.
        let n = Coefficient::<f32>::normalize(max_row_sum, m.clone());
        assert_eq!(n, m);

        // If the first argument of normalize < max_row_sum, do normalizing.
        let smaller_max = max_row_sum - 0.5_f64;
        assert!(smaller_max > 0.0_f64);
        let n = Coefficient::<f32>::normalize(smaller_max, m);
        let mut max_row_sum: f64 = std::f64::MIN;
        for row in &n {
            max_row_sum = max_row_sum.max(row.iter().sum());
            assert!(row.iter().sum::<f64>() <= smaller_max);
        }
        assert!(approx_eq!(f64, smaller_max, max_row_sum));
    }
}
