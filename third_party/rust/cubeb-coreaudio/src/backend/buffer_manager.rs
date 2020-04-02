use std::fmt;
use std::os::raw::c_void;
use std::slice;

use cubeb_backend::SampleFormat;

use super::ringbuf::RingBuffer;

use self::LinearInputBuffer::*;
use self::RingBufferConsumer::*;
use self::RingBufferProducer::*;

// When running duplex callbacks, the input data is fed to a ring buffer, and then later copied to
// a linear piece of memory is used to hold the input samples, so that they are passed to the audio
// callback that delivers it to the callees. Their size depends on the buffer size requested
// initially.  This is to be tuned when changing tlength and fragsize, but this value works for
// now.
const INPUT_BUFFER_CAPACITY: usize = 4096;

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
    pub fn new(format: SampleFormat) -> BufferManager {
        if format == SampleFormat::S16LE || format == SampleFormat::S16BE {
            let ring = RingBuffer::<i16>::new(INPUT_BUFFER_CAPACITY);
            let (prod, cons) = ring.split();
            BufferManager {
                producer: IntegerRingBufferProducer(prod),
                consumer: IntegerRingBufferConsumer(cons),
                linear_input_buffer: IntegerLinearInputBuffer(Vec::<i16>::with_capacity(
                    INPUT_BUFFER_CAPACITY,
                )),
            }
        } else {
            let ring = RingBuffer::<f32>::new(INPUT_BUFFER_CAPACITY);
            let (prod, cons) = ring.split();
            BufferManager {
                producer: FloatRingBufferProducer(prod),
                consumer: FloatRingBufferConsumer(cons),
                linear_input_buffer: FloatLinearInputBuffer(Vec::<f32>::with_capacity(
                    INPUT_BUFFER_CAPACITY,
                )),
            }
        }
    }
    pub fn push_silent_data(&mut self, silent_samples: usize) {
        let pushed = match &mut self.producer {
            RingBufferProducer::FloatRingBufferProducer(p) => {
                let mut silent_buffer = [0. as f32; INPUT_BUFFER_CAPACITY];
                let silent_slice = silent_buffer.split_at_mut(silent_samples).0;
                p.push_slice(silent_slice)
            }
            RingBufferProducer::IntegerRingBufferProducer(p) => {
                let mut silent_buffer = [0 as i16; INPUT_BUFFER_CAPACITY];
                let silent_slice = silent_buffer.split_at_mut(silent_samples).0;
                p.push_slice(silent_slice)
            }
        };
        if pushed != silent_samples {
            cubeb_log!(
                "Input ringbuffer full, could only push {} instead of {}",
                pushed,
                silent_samples
            );
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
                let mut input: &mut [i16] = unsafe {
                    slice::from_raw_parts_mut::<i16>(input_data as *mut i16, needed_samples)
                };
                let read = p.pop_slice(&mut input);
                if read < needed_samples {
                    for i in 0..(needed_samples - read) {
                        input[read + i] = 0;
                    }
                }
            }
            FloatRingBufferConsumer(p) => {
                let mut input: &mut [f32] = unsafe {
                    slice::from_raw_parts_mut::<f32>(input_data as *mut f32, needed_samples)
                };
                let read = p.pop_slice(&mut input);
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
                let mut buffer = [0 as i16; INPUT_BUFFER_CAPACITY];
                let mut slice_to_pop = buffer.split_at_mut(to_pop);
                c.pop_slice(&mut slice_to_pop.0);
            }
            FloatRingBufferConsumer(c) => {
                let available = c.len();
                assert!(available >= final_size);
                let to_pop = available - final_size;
                let mut buffer = [0 as f32; INPUT_BUFFER_CAPACITY];
                let mut slice_to_pop = buffer.split_at_mut(to_pop);
                c.pop_slice(&mut slice_to_pop.0);
            }
        }
    }
}

impl fmt::Debug for BufferManager {
    fn fmt(&self, _: &mut fmt::Formatter<'_>) -> fmt::Result {
        Ok(())
    }
}
