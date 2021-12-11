use audio_mixer::{Channel, Mixer};
use criterion::{black_box, criterion_group, criterion_main, Criterion};

use std::any::{Any, TypeId};

fn criterion_benchmark(c: &mut Criterion) {
    let frames = 512;
    c.bench_function("downmix_f32", |b| {
        b.iter(|| downmix::<f32>(black_box(frames)))
    });
    c.bench_function("downmix_i16", |b| {
        b.iter(|| downmix::<i16>(black_box(frames)))
    });
    c.bench_function("upmix_f32", |b| b.iter(|| upmix::<f32>(black_box(frames))));
    c.bench_function("upmix_i16", |b| b.iter(|| upmix::<i16>(black_box(frames))));
}

fn downmix<T>(frames: usize)
where
    T: Clone + Default + From<u8> + ?Sized + Any,
{
    // Downmix from 5.1 to stereo.
    let input_channels = [
        Channel::FrontLeft,
        Channel::FrontRight,
        Channel::FrontCenter,
        Channel::LowFrequency,
        Channel::BackLeft,
        Channel::BackRight,
    ];
    let output_channels = [Channel::FrontLeft, Channel::FrontRight];
    mix::<T>(&input_channels, &output_channels, frames);
}

fn upmix<T>(frames: usize)
where
    T: Clone + Default + From<u8> + ?Sized + Any,
{
    // upmix from mono to stereo.
    let input_channels = [Channel::FrontCenter];
    let output_channels = [Channel::FrontLeft, Channel::FrontRight];
    mix::<T>(&input_channels, &output_channels, frames);
}

fn mix<T>(input_channels: &[Channel], output_channels: &[Channel], frames: usize)
where
    T: Clone + Default + From<u8> + ?Sized + Any,
{
    if TypeId::of::<T>() == TypeId::of::<f32>() {
        let (input_buffer, mut output_buffer) = create_buffers::<f32>(
            input_channels.len() * frames,
            output_channels.len() * frames,
        );
        let mut in_buf = input_buffer.chunks(input_channels.len());
        let mut out_buf = output_buffer.chunks_mut(output_channels.len());
        let mixer = Mixer::<f32>::new(input_channels, output_channels);
        for _ in 0..frames {
            mixer.mix(in_buf.next().unwrap(), out_buf.next().unwrap());
        }
    } else if TypeId::of::<T>() == TypeId::of::<i16>() {
        let (input_buffer, mut output_buffer) = create_buffers::<i16>(
            input_channels.len() * frames,
            output_channels.len() * frames,
        );
        let mut in_buf = input_buffer.chunks(input_channels.len());
        let mut out_buf = output_buffer.chunks_mut(output_channels.len());
        let mixer = Mixer::<i16>::new(input_channels, output_channels);
        for _ in 0..frames {
            mixer.mix(in_buf.next().unwrap(), out_buf.next().unwrap());
        }
    } else {
        panic!("Unsupport type");
    }
}

fn create_buffers<T: Clone + Default + From<u8>>(
    input_size: usize,
    output_size: usize,
) -> (Vec<T>, Vec<T>) {
    let mut input_buffer = default_buffer::<T>(input_size);
    for (i, data) in input_buffer.iter_mut().enumerate() {
        *data = T::from((i + 1) as u8);
    }
    let output_buffer = default_buffer::<T>(output_size);
    (input_buffer, output_buffer)
}

fn default_buffer<T: Clone + Default>(size: usize) -> Vec<T> {
    let mut v = Vec::with_capacity(size);
    v.resize(size, T::default());
    v
}

criterion_group!(benches, criterion_benchmark);
criterion_main!(benches);
