use std::fmt;
use std::os::raw::c_void;
use std::slice;

use cubeb_backend::{SampleFormat, StreamParams};

use super::ringbuf::RingBuffer;

use self::LinearInputBuffer::*;
use self::RingBufferConsumer::*;
use self::RingBufferProducer::*;

pub enum RingBufferConsumer {
    IntegerRingBufferConsumer(ringbuf::Consumer<i16>),
    FloatRingBufferConsumer(ringbuf::Consumer<f32>),
}

pub enum RingBufferProducer {
    IntegerRingBufferProducer(ringbuf::Producer<i16>),
    FloatRingBufferProducer(ringbuf::Producer<f32>),
}

pub enum LinearInputBuffer {
    IntegerLinearInputBuffer(Vec<i16>),
    FloatLinearInputBuffer(Vec<f32>),
}

pub struct BufferManager {
    consumer: RingBufferConsumer,
    producer: RingBufferProducer,
    linear_input_buffer: LinearInputBuffer,
}

impl BufferManager {
    // When opening a duplex stream, the sample-spec are guaranteed to match. It's ok to have
    // either the input or output sample-spec here.
    pub fn new(params: &StreamParams, input_buffer_size_frames: u32) -> BufferManager {
        let format = params.format();
        let channel_count = params.channels();
        // Multiply by two for good measure.
        let buffer_element_count = (channel_count * input_buffer_size_frames * 2) as usize;
        if format == SampleFormat::S16LE || format == SampleFormat::S16BE {
            let ring = RingBuffer::<i16>::new(buffer_element_count);
            let (prod, cons) = ring.split();
            BufferManager {
                producer: IntegerRingBufferProducer(prod),
                consumer: IntegerRingBufferConsumer(cons),
                linear_input_buffer: IntegerLinearInputBuffer(Vec::<i16>::with_capacity(
                    buffer_element_count,
                )),
            }
        } else {
            let ring = RingBuffer::<f32>::new(buffer_element_count);
            let (prod, cons) = ring.split();
            BufferManager {
                producer: FloatRingBufferProducer(prod),
                consumer: FloatRingBufferConsumer(cons),
                linear_input_buffer: FloatLinearInputBuffer(Vec::<f32>::with_capacity(
                    buffer_element_count,
                )),
            }
        }
    }
    pub fn push_data(&mut self, input_data: *const c_void, read_samples: usize) {
        let pushed = match &mut self.producer {
            RingBufferProducer::FloatRingBufferProducer(p) => {
                let input_data =
                    unsafe { slice::from_raw_parts::<f32>(input_data as *const f32, read_samples) };
                p.push_slice(input_data)
            }
            RingBufferProducer::IntegerRingBufferProducer(p) => {
                let input_data =
                    unsafe { slice::from_raw_parts::<i16>(input_data as *const i16, read_samples) };
                p.push_slice(input_data)
            }
        };
        if pushed != read_samples {
            cubeb_log!(
                "Input ringbuffer full, could only push {} instead of {}",
                pushed,
                read_samples
            );
        }
    }
    fn pull_data(&mut self, input_data: *mut c_void, needed_samples: usize) {
        match &mut self.consumer {
            IntegerRingBufferConsumer(p) => {
                let input: &mut [i16] = unsafe {
                    slice::from_raw_parts_mut::<i16>(input_data as *mut i16, needed_samples)
                };
                let read = p.pop_slice(input);
                if read < needed_samples {
                    for i in 0..(needed_samples - read) {
                        input[read + i] = 0;
                    }
                }
            }
            FloatRingBufferConsumer(p) => {
                let input: &mut [f32] = unsafe {
                    slice::from_raw_parts_mut::<f32>(input_data as *mut f32, needed_samples)
                };
                let read = p.pop_slice(input);
                if read < needed_samples {
                    for i in 0..(needed_samples - read) {
                        input[read + i] = 0.0;
                    }
                }
            }
        }
    }
    pub fn get_linear_data(&mut self, nsamples: usize) -> *mut c_void {
        let p: *mut c_void;
        match &mut self.linear_input_buffer {
            LinearInputBuffer::IntegerLinearInputBuffer(b) => {
                b.resize(nsamples, 0);
                p = b.as_mut_ptr() as *mut c_void;
            }
            LinearInputBuffer::FloatLinearInputBuffer(b) => {
                b.resize(nsamples, 0.);
                p = b.as_mut_ptr() as *mut c_void;
            }
        }
        self.pull_data(p, nsamples);

        p
    }
    pub fn available_samples(&self) -> usize {
        match &self.consumer {
            IntegerRingBufferConsumer(p) => p.len(),
            FloatRingBufferConsumer(p) => p.len(),
        }
    }
    pub fn trim(&mut self, final_size: usize) {
        match &mut self.consumer {
            IntegerRingBufferConsumer(c) => {
                let available = c.len();
                assert!(available >= final_size);
                let to_pop = available - final_size;
                c.discard(to_pop);
            }
            FloatRingBufferConsumer(c) => {
                let available = c.len();
                assert!(available >= final_size);
                let to_pop = available - final_size;
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
