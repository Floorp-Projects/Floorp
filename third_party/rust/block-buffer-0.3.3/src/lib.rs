#![no_std]
#[macro_use]
extern crate arrayref;
extern crate byte_tools;

use byte_tools::{zero, write_u64_le};

mod paddings;

pub use paddings::*;

macro_rules! impl_buffer {
    ($name:ident, $len:expr) => {

        pub struct $name {
            buffer: [u8; $len],
            pos: usize,
        }

        impl Copy for $name {}

        impl Clone for $name {
            fn clone(&self) -> Self {
                *self
            }
        }

        impl Default for $name {
            fn default() -> Self {
                $name {buffer: [0; $len], pos: 0}
            }
        }

        impl $name {
            #[inline]
            pub fn input<F: FnMut(&[u8; $len])>(&mut self, mut input: &[u8], mut func: F) {
                // If there is already data in the buffer, copy as much as we can
                // into it and process the data if the buffer becomes full.
                if self.pos != 0 {
                    let rem = self.remaining();

                    if input.len() >= rem {
                        let (l, r) = input.split_at(rem);
                        input = r;
                        self.buffer[self.pos..].copy_from_slice(l);
                        self.pos = 0;
                        func(&self.buffer);
                    } else {
                        let end = self.pos + input.len();
                        self.buffer[self.pos..end].copy_from_slice(input);
                        self.pos = end;
                        return;
                    }
                }

                // While we have at least a full buffer size chunks's worth of data,
                // process that data without copying it into the buffer
                while input.len() >= self.size() {
                    let (l, r) = input.split_at(self.size());
                    input = r;
                    func(array_ref!(l, 0, $len));
                }

                // Copy any input data into the buffer. At this point in the method,
                // the ammount of data left in the input vector will be less than
                // the buffer size and the buffer will be empty.
                self.buffer[..input.len()].copy_from_slice(input);
                self.pos = input.len();
            }

            #[inline]
            fn digest_pad<F>(&mut self, up_to: usize, func: &mut F)
                where F: FnMut(&[u8; $len])
            {
                self.buffer[self.pos] = 0x80;
                self.pos += 1;

                zero(&mut self.buffer[self.pos..]);

                if self.remaining() < up_to {
                    func(&self.buffer);
                    zero(&mut self.buffer[..self.pos]);
                }
            }

            #[inline]
            /// Will pad message with message length in big-endian format
            pub fn len_padding<F>(&mut self, data_len: u64, mut func: F)
                where F: FnMut(&[u8; $len])
            {
                self.digest_pad(8, &mut func);
                let s = self.size();
                write_u64_le(&mut self.buffer[s-8..], data_len);
                func(&self.buffer);
                self.pos = 0;
            }

            #[inline]
            pub fn len_padding_u128<F>(&mut self, hi: u64, lo: u64, mut func: F)
                where F: FnMut(&[u8; $len])
            {
                self.digest_pad(16, &mut func);
                let s = self.size();
                write_u64_le(&mut self.buffer[s-16..s-8], hi);
                write_u64_le(&mut self.buffer[s-8..], lo);
                func(&self.buffer);
                self.pos = 0;
            }

            #[inline]
            pub fn pad_with<P: Padding>(&mut self) -> &mut [u8; $len] {
                P::pad(&mut self.buffer[..], self.pos);
                self.pos = 0;
                &mut self.buffer
            }

            #[inline]
            pub fn size(&self) -> usize {
                $len
            }

            #[inline]
            pub fn position(&self) -> usize {
                self.pos
            }

            #[inline]
            pub fn remaining(&self) -> usize {
                self.size() - self.pos
            }
        }
    }
}

impl_buffer!(BlockBuffer128, 16);
impl_buffer!(BlockBuffer256, 32);
impl_buffer!(BlockBuffer512, 64);
impl_buffer!(BlockBuffer1024, 128);

impl_buffer!(BlockBuffer576, 72);
impl_buffer!(BlockBuffer832, 104);
impl_buffer!(BlockBuffer1088, 136);
impl_buffer!(BlockBuffer1152, 144);
impl_buffer!(BlockBuffer1344, 168);
