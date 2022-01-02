// Copyright © 2011 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

//! libcubeb api/function test. Plays a simple tone.
extern crate cubeb;

mod common;

use cubeb::{MonoFrame, Sample};
use std::f32::consts::PI;
use std::thread;
use std::time::Duration;

const SAMPLE_FREQUENCY: u32 = 48_000;
const STREAM_FORMAT: cubeb::SampleFormat = cubeb::SampleFormat::S16LE;

type Frame = MonoFrame<i16>;

fn main() {
    let ctx = common::init("Cubeb tone example").expect("Failed to create cubeb context");

    let params = cubeb::StreamParamsBuilder::new()
        .format(STREAM_FORMAT)
        .rate(SAMPLE_FREQUENCY)
        .channels(1)
        .layout(cubeb::ChannelLayout::MONO)
        .take();

    let mut position = 0u32;

    let mut builder = cubeb::StreamBuilder::<Frame>::new();
    builder
        .name("Cubeb tone (mono)")
        .default_output(&params)
        .latency(0x1000)
        .data_callback(move |_, output| {
            // generate our test tone on the fly
            for f in output.iter_mut() {
                // North American dial tone
                let t1 = (2.0 * PI * 350.0 * position as f32 / SAMPLE_FREQUENCY as f32).sin();
                let t2 = (2.0 * PI * 440.0 * position as f32 / SAMPLE_FREQUENCY as f32).sin();

                f.m = i16::from_float(0.5 * (t1 + t2));

                position += 1;
            }
            output.len() as isize
        })
        .state_callback(|state| {
            println!("stream {:?}", state);
        });

    let stream = builder.init(&ctx).expect("Failed to create cubeb stream");

    stream.start().unwrap();
    thread::sleep(Duration::from_millis(500));
    stream.stop().unwrap();
}
