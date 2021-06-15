use digest::{Input, BlockInput, FixedOutput, Reset};
use digest::generic_array::GenericArray;
use digest::generic_array::typenum::{U28, U32, U48, U64, U128};
use block_buffer::BlockBuffer;
use block_buffer::byteorder::{BE, ByteOrder};

use consts::{STATE_LEN, H384, H512, H512_TRUNC_224, H512_TRUNC_256};

#[cfg(any(not(feature = "asm"), target_arch = "aarch64"))]
use sha512_utils::compress512;
#[cfg(all(feature = "asm", not(target_arch = "aarch64")))]
use sha2_asm::compress512;

type BlockSize = U128;
type Block = GenericArray<u8, BlockSize>;

/// A structure that represents that state of a digest computation for the
/// SHA-2 512 family of digest functions
#[derive(Clone)]
struct Engine512State {
    h: [u64; 8],
}

impl Engine512State {
    fn new(h: &[u64; 8]) -> Engine512State { Engine512State { h: *h } }

    pub fn process_block(&mut self, block: &Block) {
        let block = unsafe { &*(block.as_ptr() as *const [u8; 128])};
        compress512(&mut self.h, block);
    }
}

/// A structure that keeps track of the state of the Sha-512 operation and
/// contains the logic necessary to perform the final calculations.
#[derive(Clone)]
struct Engine512 {
    len: (u64, u64), // TODO: replace with u128 on MSRV bump
    buffer: BlockBuffer<BlockSize>,
    state: Engine512State,
}

impl Engine512 {
    fn new(h: &[u64; STATE_LEN]) -> Engine512 {
        Engine512 {
            len: (0, 0),
            buffer: Default::default(),
            state: Engine512State::new(h),
        }
    }

    fn input(&mut self, input: &[u8]) {
        let (res, over) = self.len.1.overflowing_add((input.len() as u64) << 3);
        self.len.1 = res;
        if over { self.len.0 += 1; }
        let self_state = &mut self.state;
        self.buffer.input(input, |d| self_state.process_block(d));
    }

    fn finish(&mut self) {
        let self_state = &mut self.state;
        let (hi, lo) = self.len;
        self.buffer.len128_padding_be(hi, lo, |d| self_state.process_block(d));
    }

    fn reset(&mut self, h: &[u64; STATE_LEN]) {
        self.len = (0, 0);
        self.buffer.reset();
        self.state = Engine512State::new(h);
    }
}


/// The SHA-512 hash algorithm with the SHA-512 initial hash value.
#[derive(Clone)]
pub struct Sha512 {
    engine: Engine512,
}

impl Default for Sha512 {
    fn default() -> Self { Sha512 { engine: Engine512::new(&H512) } }
}

impl BlockInput for Sha512 {
    type BlockSize = BlockSize;
}

impl Input for Sha512 {
    fn input<B: AsRef<[u8]>>(&mut self, input: B) {
        self.engine.input(input.as_ref());
    }
}

impl FixedOutput for Sha512 {
    type OutputSize = U64;

    fn fixed_result(mut self) -> GenericArray<u8, Self::OutputSize> {
        self.engine.finish();

        let mut out = GenericArray::default();
        BE::write_u64_into(&self.engine.state.h[..], out.as_mut_slice());
        out
    }
}

impl Reset for Sha512 {
    fn reset(&mut self) {
        self.engine.reset(&H512);
    }
}

/// The SHA-512 hash algorithm with the SHA-384 initial hash value. The result
/// is truncated to 384 bits.
#[derive(Clone)]
pub struct Sha384 {
    engine: Engine512,
}

impl Default for Sha384 {
    fn default() -> Self { Sha384 { engine: Engine512::new(&H384) } }
}

impl BlockInput for Sha384 {
    type BlockSize = BlockSize;
}

impl Input for Sha384 {
    fn input<B: AsRef<[u8]>>(&mut self, input: B) {
        self.engine.input(input.as_ref());
    }
}

impl FixedOutput for Sha384 {
    type OutputSize = U48;

    fn fixed_result(mut self) -> GenericArray<u8, Self::OutputSize> {
        self.engine.finish();

        let mut out = GenericArray::default();
        BE::write_u64_into(&self.engine.state.h[..6], out.as_mut_slice());
        out
    }
}

impl Reset for Sha384 {
    fn reset(&mut self) {
        self.engine.reset(&H384);
    }
}

/// The SHA-512 hash algorithm with the SHA-512/256 initial hash value. The
/// result is truncated to 256 bits.
#[derive(Clone)]
pub struct Sha512Trunc256 {
    engine: Engine512,
}

impl Default for Sha512Trunc256 {
    fn default() -> Self {
        Sha512Trunc256 { engine: Engine512::new(&H512_TRUNC_256) }
    }
}

impl BlockInput for Sha512Trunc256 {
    type BlockSize = BlockSize;
}

impl Input for Sha512Trunc256 {
    fn input<B: AsRef<[u8]>>(&mut self, input: B) {
        self.engine.input(input.as_ref());
    }
}

impl FixedOutput for Sha512Trunc256 {
    type OutputSize = U32;

    fn fixed_result(mut self) -> GenericArray<u8, Self::OutputSize> {
        self.engine.finish();

        let mut out = GenericArray::default();
        BE::write_u64_into(&self.engine.state.h[..4], out.as_mut_slice());
        out
    }
}

impl Reset for Sha512Trunc256 {
    fn reset(&mut self) {
        self.engine.reset(&H512_TRUNC_256);
    }
}

/// The SHA-512 hash algorithm with the SHA-512/224 initial hash value.
/// The result is truncated to 224 bits.
#[derive(Clone)]
pub struct Sha512Trunc224 {
    engine: Engine512,
}

impl Default for Sha512Trunc224 {
    fn default() -> Self {
        Sha512Trunc224 { engine: Engine512::new(&H512_TRUNC_224) }
    }
}

impl BlockInput for Sha512Trunc224 {
    type BlockSize = BlockSize;
}

impl Input for Sha512Trunc224 {
    fn input<B: AsRef<[u8]>>(&mut self, input: B) {
        self.engine.input(input.as_ref());
    }
}

impl FixedOutput for Sha512Trunc224 {
    type OutputSize = U28;

    fn fixed_result(mut self) -> GenericArray<u8, Self::OutputSize> {
        self.engine.finish();

        let mut out = GenericArray::default();
        BE::write_u64_into(&self.engine.state.h[..3], &mut out[..24]);
        BE::write_u32(&mut out[24..28], (self.engine.state.h[3] >> 32) as u32);
        out
    }
}

impl Reset for Sha512Trunc224 {
    fn reset(&mut self) {
        self.engine.reset(&H512_TRUNC_224);
    }
}

impl_opaque_debug!(Sha384);
impl_opaque_debug!(Sha512);
impl_opaque_debug!(Sha512Trunc224);
impl_opaque_debug!(Sha512Trunc256);

impl_write!(Sha384);
impl_write!(Sha512);
impl_write!(Sha512Trunc224);
impl_write!(Sha512Trunc256);
