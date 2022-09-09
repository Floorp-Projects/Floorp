use std::fmt;
use std::os::raw::c_void;
use std::slice;

use cubeb_backend::SampleFormat;

use super::ringbuf::RingBuffer;

use self::LinearBuffer::*;
use self::RingBufferConsumer::*;
use self::RingBufferProducer::*;

// Shuffles the data so that the first n channels of the interleaved buffer are overwritten by
// the remaining channels.
fn drop_first_n_channels_in_place<T: Copy>(
    n: usize,
    data: &mut [T],
    frame_count: usize,
    channel_count: usize,
) {
    // This function works if the numbers are equal but it's not particularly useful, so we hope to
    // catch issues by checking using > and not >= here.
    assert!(channel_count > n);
    let mut read_idx: usize = 0;
    let mut write_idx: usize = 0;

    let channel_to_keep = channel_count - n;
    for _ in 0..frame_count {
        read_idx += n;
        for _ in 0..channel_to_keep {
            data[write_idx] = data[read_idx];
            read_idx += 1;
            write_idx += 1;
        }
    }
}

// It can be that the a stereo microphone is in use, but the user asked for mono input. In this
// particular case, downmix the stereo pair into a mono channel. In all other cases, simply drop
// the remaining channels before appending to the ringbuffer, becauses there is no right or wrong
// way to do this, unlike with the output side, where proper channel matrixing can be done.
// Return the number of valid samples in the buffer.
fn remix_or_drop_channels<T: Copy + std::ops::Add<Output = T>>(
    input_channels: usize,
    output_channels: usize,
    data: &mut [T],
    frame_count: usize,
) -> usize {
    assert!(input_channels >= output_channels);
    // Nothing to do, just return
    if input_channels == output_channels {
        return output_channels * frame_count;
    }
    // Simple stereo downmix
    if input_channels == 2 && output_channels == 1 {
        let mut read_idx = 0;
        for (write_idx, _) in (0..frame_count).enumerate() {
            data[write_idx] = data[read_idx] + data[read_idx + 1];
            read_idx += 2;
        }
        return output_channels * frame_count;
    }
    // Drop excess channels
    let mut read_idx = 0;
    let mut write_idx = 0;
    let channel_dropped_count = input_channels - output_channels;
    for _ in 0..frame_count {
        for _ in 0..output_channels {
            data[write_idx] = data[read_idx];
            write_idx += 1;
            read_idx += 1;
        }
        read_idx += channel_dropped_count;
    }
    output_channels * frame_count
}

fn process_data<T: Copy + std::ops::Add<Output = T>>(
    data: *mut c_void,
    frame_count: usize,
    input_channel_count: usize,
    input_channels_to_ignore: usize,
    output_channel_count: usize,
) -> &'static [T] {
    assert!(input_channel_count >= input_channels_to_ignore + output_channel_count); // Only support downmix.
    let input_slice = unsafe {
        slice::from_raw_parts_mut::<T>(data as *mut T, frame_count * input_channel_count)
    };
    if input_channel_count == output_channel_count {
        assert_eq!(input_channels_to_ignore, 0);
        input_slice
    } else {
        if input_channels_to_ignore > 0 {
            drop_first_n_channels_in_place(
                input_channels_to_ignore,
                input_slice,
                frame_count,
                input_channel_count,
            );
        }
        let new_count_remixed = remix_or_drop_channels(
            input_channel_count - input_channels_to_ignore,
            output_channel_count,
            input_slice,
            frame_count,
        );
        unsafe { slice::from_raw_parts_mut::<T>(data as *mut T, new_count_remixed) }
    }
}

pub enum RingBufferConsumer {
    IntegerRingBufferConsumer(ringbuf::Consumer<i16>),
    FloatRingBufferConsumer(ringbuf::Consumer<f32>),
}

pub enum RingBufferProducer {
    IntegerRingBufferProducer(ringbuf::Producer<i16>),
    FloatRingBufferProducer(ringbuf::Producer<f32>),
}

pub enum LinearBuffer {
    IntegerLinearBuffer(Vec<i16>),
    FloatLinearBuffer(Vec<f32>),
}

pub struct BufferManager {
    consumer: RingBufferConsumer,
    producer: RingBufferProducer,
    linear_buffer: LinearBuffer,
    // The number of channels in the interleaved data given to push_data
    input_channel_count: usize,
    // The number of channels that needs to be skipped in the beginning of input_channel_count
    input_channels_to_ignore: usize,
    // The number of channels we actually needs, which is also the channel count of the
    // processed data stored in the internal ring buffer.
    output_channel_count: usize,
}

impl BufferManager {
    pub fn new(
        format: SampleFormat,
        buffer_size_frames: usize,
        input_channel_count: usize,
        input_channels_to_ignore: usize,
        output_channel_count: usize,
    ) -> Self {
        assert!(input_channel_count >= input_channels_to_ignore + output_channel_count);
        // 8 times the expected callback size, to handle the input callback being caled multiple
        //   times in a row correctly.
        let buffer_element_count = output_channel_count * buffer_size_frames as usize * 8;
        match format {
            SampleFormat::S16LE | SampleFormat::S16BE | SampleFormat::S16NE => {
                let ring = RingBuffer::<i16>::new(buffer_element_count);
                let (prod, cons) = ring.split();
                Self {
                    producer: IntegerRingBufferProducer(prod),
                    consumer: IntegerRingBufferConsumer(cons),
                    linear_buffer: IntegerLinearBuffer(Vec::<i16>::with_capacity(
                        buffer_element_count,
                    )),
                    input_channel_count,
                    input_channels_to_ignore,
                    output_channel_count,
                }
            }
            SampleFormat::Float32LE | SampleFormat::Float32BE | SampleFormat::Float32NE => {
                let ring = RingBuffer::<f32>::new(buffer_element_count);
                let (prod, cons) = ring.split();
                Self {
                    producer: FloatRingBufferProducer(prod),
                    consumer: FloatRingBufferConsumer(cons),
                    linear_buffer: FloatLinearBuffer(Vec::<f32>::with_capacity(
                        buffer_element_count,
                    )),
                    input_channel_count,
                    input_channels_to_ignore,
                    output_channel_count,
                }
            }
        }
    }
    fn channel_count(&self) -> usize {
        self.output_channel_count
    }
    pub fn push_data(&mut self, data: *mut c_void, frame_count: usize) {
        let to_push = frame_count * self.output_channel_count;
        let pushed = match &mut self.producer {
            RingBufferProducer::FloatRingBufferProducer(p) => {
                let processed_input = process_data(
                    data,
                    frame_count,
                    self.input_channel_count,
                    self.input_channels_to_ignore,
                    self.output_channel_count,
                );
                p.push_slice(processed_input)
            }
            RingBufferProducer::IntegerRingBufferProducer(p) => {
                let processed_input = process_data(
                    data,
                    frame_count,
                    self.input_channel_count,
                    self.input_channels_to_ignore,
                    self.output_channel_count,
                );
                p.push_slice(processed_input)
            }
        };
        assert!(pushed <= to_push, "We don't support upmix");
        if pushed != to_push {
            cubeb_alog!(
                "Input ringbuffer full, could only push {} instead of {}",
                pushed,
                to_push
            );
        }
    }
    fn pull_data(&mut self, data: *mut c_void, needed_samples: usize) {
        match &mut self.consumer {
            IntegerRingBufferConsumer(p) => {
                let input: &mut [i16] =
                    unsafe { slice::from_raw_parts_mut::<i16>(data as *mut i16, needed_samples) };
                let read = p.pop_slice(input);
                if read < needed_samples {
                    cubeb_alog!(
                        "Underrun during input data pull: (needed: {}, available: {})",
                        needed_samples,
                        read
                    );
                    for i in 0..(needed_samples - read) {
                        input[read + i] = 0;
                    }
                }
            }
            FloatRingBufferConsumer(p) => {
                let input: &mut [f32] =
                    unsafe { slice::from_raw_parts_mut::<f32>(data as *mut f32, needed_samples) };
                let read = p.pop_slice(input);
                if read < needed_samples {
                    cubeb_alog!(
                        "Underrun during input data pull: (needed: {}, available: {})",
                        needed_samples,
                        read
                    );
                    for i in 0..(needed_samples - read) {
                        input[read + i] = 0.0;
                    }
                }
            }
        }
    }
    pub fn get_linear_data(&mut self, frame_count: usize) -> *mut c_void {
        let sample_count = frame_count * self.channel_count();
        let p = match &mut self.linear_buffer {
            LinearBuffer::IntegerLinearBuffer(b) => {
                b.resize(sample_count, 0);
                b.as_mut_ptr() as *mut c_void
            }
            LinearBuffer::FloatLinearBuffer(b) => {
                b.resize(sample_count, 0.);
                b.as_mut_ptr() as *mut c_void
            }
        };
        self.pull_data(p, sample_count);

        p
    }
    fn available_samples(&self) -> usize {
        match &self.consumer {
            IntegerRingBufferConsumer(p) => p.len(),
            FloatRingBufferConsumer(p) => p.len(),
        }
    }
    pub fn available_frames(&self) -> usize {
        assert_ne!(self.channel_count(), 0);
        self.available_samples() / self.channel_count()
    }
    pub fn trim(&mut self, final_frame_count: usize) {
        let final_sample_count = final_frame_count * self.channel_count();
        match &mut self.consumer {
            IntegerRingBufferConsumer(c) => {
                let available = c.len();
                assert!(available >= final_sample_count);
                let to_pop = available - final_sample_count;
                c.discard(to_pop);
            }
            FloatRingBufferConsumer(c) => {
                let available = c.len();
                assert!(available >= final_sample_count);
                let to_pop = available - final_sample_count;
                c.discard(to_pop);
            }
        }
    }
}

impl fmt::Debug for BufferManager {
    fn fmt(&self, _: &mut fmt::Formatter<'_>) -> fmt::Result {
        Ok(())
    }
}
