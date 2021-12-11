#![no_std]
pub extern crate byteorder;
pub extern crate block_padding;
pub extern crate generic_array;
extern crate byte_tools;

use byteorder::{ByteOrder, BE};
use byte_tools::zero;
use block_padding::{Padding, PadError};
use generic_array::{GenericArray, ArrayLength};
use core::slice;

/// Buffer for block processing of data
#[derive(Clone, Default)]
pub struct BlockBuffer<BlockSize: ArrayLength<u8>>  {
    buffer: GenericArray<u8, BlockSize>,
    pos: usize,
}

#[inline(always)]
unsafe fn cast<N: ArrayLength<u8>>(block: &[u8]) -> &GenericArray<u8, N> {
    debug_assert_eq!(block.len(), N::to_usize());
    &*(block.as_ptr() as *const GenericArray<u8, N>)
}



impl<BlockSize: ArrayLength<u8>> BlockBuffer<BlockSize> {
    /// Process data in `input` in blocks of size `BlockSize` using function `f`.
    #[inline]
    pub fn input<F>(&mut self, mut input: &[u8], mut f: F)
        where F: FnMut(&GenericArray<u8, BlockSize>)
    {
        // If there is already data in the buffer, process it if we have
        // enough to complete the chunk.
        let rem = self.remaining();
        if self.pos != 0 && input.len() >= rem {
            let (l, r) = input.split_at(rem);
            input = r;
            self.buffer[self.pos..].copy_from_slice(l);
            self.pos = 0;
            f(&self.buffer);
        }

        // While we have at least a full buffer size chunks's worth of data,
        // process that data without copying it into the buffer
        while input.len() >= self.size() {
            let (block, r) = input.split_at(self.size());
            input = r;
            f(unsafe { cast(block) });
        }

        // Copy any remaining data into the buffer.
        self.buffer[self.pos..self.pos+input.len()].copy_from_slice(input);
        self.pos += input.len();
    }

    /*
    /// Process data in `input` in blocks of size `BlockSize` using function `f`, which accepts
    /// slice of blocks.
    #[inline]
    pub fn input2<F>(&mut self, mut input: &[u8], mut f: F)
        where F: FnMut(&[GenericArray<u8, BlockSize>])
    {
        // If there is already data in the buffer, process it if we have
        // enough to complete the chunk.
        let rem = self.remaining();
        if self.pos != 0 && input.len() >= rem {
            let (l, r) = input.split_at(rem);
            input = r;
            self.buffer[self.pos..].copy_from_slice(l);
            self.pos = 0;
            f(slice::from_ref(&self.buffer));
        }

        // While we have at least a full buffer size chunks's worth of data,
        // process it data without copying into the buffer
        let n_blocks = input.len()/self.size();
        let (left, right) = input.split_at(n_blocks*self.size());
        // safe because we guarantee that `blocks` does not point outside of `input` 
        let blocks = unsafe {
            slice::from_raw_parts(
                left.as_ptr() as *const GenericArray<u8, BlockSize>,
                n_blocks,
            )
        };
        f(blocks);

        // Copy remaining data into the buffer.
        self.buffer[self.pos..self.pos+right.len()].copy_from_slice(right);
        self.pos += right.len();
    }
    */

    /// Variant that doesn't flush the buffer until there's additional
    /// data to be processed. Suitable for tweakable block ciphers
    /// like Threefish that need to know whether a block is the *last*
    /// data block before processing it.
    #[inline]
    pub fn input_lazy<F>(&mut self, mut input: &[u8], mut f: F)
        where F: FnMut(&GenericArray<u8, BlockSize>)
    {
        let rem = self.remaining();
        if self.pos != 0 && input.len() > rem {
            let (l, r) = input.split_at(rem);
            input = r;
            self.buffer[self.pos..].copy_from_slice(l);
            self.pos = 0;
            f(&self.buffer);
        }

        while input.len() > self.size() {
            let (block, r) = input.split_at(self.size());
            input = r;
            f(unsafe { cast(block) });
        }

        self.buffer[self.pos..self.pos+input.len()].copy_from_slice(input);
        self.pos += input.len();
    }

    /// Pad buffer with `prefix` and make sure that internall buffer
    /// has at least `up_to` free bytes. All remaining bytes get
    /// zeroed-out.
    #[inline]
    fn digest_pad<F>(&mut self, up_to: usize, f: &mut F)
        where F: FnMut(&GenericArray<u8, BlockSize>)
    {
        if self.pos == self.size() {
            f(&self.buffer);
            self.pos = 0;
        }
        self.buffer[self.pos] = 0x80;
        self.pos += 1;

        zero(&mut self.buffer[self.pos..]);

        if self.remaining() < up_to {
            f(&self.buffer);
            zero(&mut self.buffer[..self.pos]);
        }
    }

    /// Pad message with 0x80, zeros and 64-bit message length
    /// in a byte order specified by `B`
    #[inline]
    pub fn len64_padding<B, F>(&mut self, data_len: u64, mut f: F)
        where B: ByteOrder, F: FnMut(&GenericArray<u8, BlockSize>)
    {
        // TODO: replace `F` with `impl Trait` on MSRV bump
        self.digest_pad(8, &mut f);
        let s = self.size();
        B::write_u64(&mut self.buffer[s-8..], data_len);
        f(&self.buffer);
        self.pos = 0;
    }


    /// Pad message with 0x80, zeros and 128-bit message length
    /// in the big-endian byte order
    #[inline]
    pub fn len128_padding_be<F>(&mut self, hi: u64, lo: u64, mut f: F)
        where F: FnMut(&GenericArray<u8, BlockSize>)
    {
        // TODO: on MSRV bump replace `F` with `impl Trait`, use `u128`, add `B`
        self.digest_pad(16, &mut f);
        let s = self.size();
        BE::write_u64(&mut self.buffer[s-16..s-8], hi);
        BE::write_u64(&mut self.buffer[s-8..], lo);
        f(&self.buffer);
        self.pos = 0;
    }

    /// Pad message with a given padding `P`
    ///
    /// Returns `PadError` if internall buffer is full, which can only happen if
    /// `input_lazy` was used.
    #[inline]
    pub fn pad_with<P: Padding>(&mut self)
        -> Result<&mut GenericArray<u8, BlockSize>, PadError>
    {
        P::pad_block(&mut self.buffer[..], self.pos)?;
        self.pos = 0;
        Ok(&mut self.buffer)
    }

    /// Return size of the internall buffer in bytes
    #[inline]
    pub fn size(&self) -> usize {
        BlockSize::to_usize()
    }

    /// Return current cursor position
    #[inline]
    pub fn position(&self) -> usize {
        self.pos
    }

    /// Return number of remaining bytes in the internall buffer
    #[inline]
    pub fn remaining(&self) -> usize {
        self.size() - self.pos
    }

    /// Reset buffer by setting cursor position to zero
    #[inline]
    pub fn reset(&mut self)  {
        self.pos = 0
    }
}
