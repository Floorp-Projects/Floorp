extern crate audio_mixer;
use audio_mixer::{Channel, Mixer};

fn main() {
    // f32
    let input_channels = [
        Channel::FrontLeft,
        Channel::Silence,
        Channel::FrontRight,
        Channel::FrontCenter,
    ];
    let output_channels = [Channel::FrontLeft, Channel::FrontRight];

    let mut input_buffer = vec![0.0; input_channels.len()];
    for (i, data) in input_buffer.iter_mut().enumerate() {
        *data = (i + 1) as f32;
    }
    let mut output_buffer = vec![0.0; output_channels.len()];

    let mixer = Mixer::new(&input_channels, &output_channels);

    mixer.mix(input_buffer.as_slice(), output_buffer.as_mut_slice());
    println!("{:?} is mixed to {:?}", input_buffer, output_buffer);

    // i16
    let input_channels = [
        Channel::FrontLeft,
        Channel::Silence,
        Channel::FrontRight,
        Channel::FrontCenter,
        Channel::BackLeft,
        Channel::SideRight,
        Channel::LowFrequency,
        Channel::SideLeft,
        Channel::BackCenter,
        Channel::BackRight,
    ];
    let output_channels = [Channel::Silence, Channel::FrontRight, Channel::FrontLeft];

    let mut input_buffer = vec![0; input_channels.len()];
    for (i, data) in input_buffer.iter_mut().enumerate() {
        *data = (i + 0x7FFE) as i16;
    }
    let mut output_buffer = vec![0; output_channels.len()];

    let mixer = Mixer::new(&input_channels, &output_channels);

    mixer.mix(input_buffer.as_slice(), output_buffer.as_mut_slice());
    println!("{:?} is mixed to {:?}", input_buffer, output_buffer);
}
