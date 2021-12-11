use digest::{Input, BlockInput, FixedOutput, Reset};
use digest::generic_array::GenericArray;
use digest::generic_array::typenum::{U28, U32, U64};
use block_buffer::BlockBuffer;
use block_buffer::byteorder::{BE, ByteOrder};

use consts::{STATE_LEN, H224, H256};

#[cfg(not(feature = "asm"))]
use sha256_utils::compress256;
#[cfg(feature = "asm")]
use sha2_asm::compress256;

type BlockSize = U64;
type Block = GenericArray<u8, BlockSize>;

/// A structure that represents that state of a digest computation for the
/// SHA-2 512 family of digest functions
#[derive(Clone)]
struct Engine256State {
    h: [u32; 8],
}

impl Engine256State {
    fn new(h: &[u32; STATE_LEN]) -> Engine256State { Engine256State { h: *h } }

    #[cfg(not(feature = "asm-aarch64"))]
    pub fn process_block(&mut self, block: &Block) {
        let block = unsafe { &*(block.as_ptr() as *const [u8; 64]) };
        compress256(&mut self.h, block);
    }

    #[cfg(feature = "asm-aarch64")]
    pub fn process_block(&mut self, block: &Block) {
        let block = unsafe { &*(block.as_ptr() as *const [u8; 64]) };
        // TODO: Replace this platform-specific call with is_aarch64_feature_detected!("sha2") once
        // that macro is stabilised and https://github.com/rust-lang/rfcs/pull/2725 is implemented
        // to let us use it on no_std.
        if ::aarch64::sha2_supported() {
            compress256(&mut self.h, block);
        } else {
            ::sha256_utils::compress256(&mut self.h, block);
        }
    }
}

/// A structure that keeps track of the state of the Sha-256 operation and
/// contains the logic necessary to perform the final calculations.
#[derive(Clone)]
struct Engine256 {
    len: u64,
    buffer: BlockBuffer<BlockSize>,
    state: Engine256State,
}

impl Engine256 {
    fn new(h: &[u32; STATE_LEN]) -> Engine256 {
        Engine256 {
            len: 0,
            buffer: Default::default(),
            state: Engine256State::new(h),
        }
    }

    fn input(&mut self, input: &[u8]) {
        // Assumes that input.len() can be converted to u64 without overflow
        self.len += (input.len() as u64) << 3;
        let self_state = &mut self.state;
        self.buffer.input(input, |input| self_state.process_block(input));
    }

    fn finish(&mut self) {
        let self_state = &mut self.state;
        let l = self.len;
        self.buffer.len64_padding::<BE, _>(l, |b| self_state.process_block(b));
    }

    fn reset(&mut self, h: &[u32; STATE_LEN]) {
        self.len = 0;
        self.buffer.reset();
        self.state = Engine256State::new(h);
    }
}


/// The SHA-256 hash algorithm with the SHA-256 initial hash value.
#[derive(Clone)]
pub struct Sha256 {
    engine: Engine256,
}

impl Default for Sha256 {
    fn default() -> Self { Sha256 { engine: Engine256::new(&H256) } }
}

impl BlockInput for Sha256 {
    type BlockSize = BlockSize;
}

impl Input for Sha256 {
    fn input<B: AsRef<[u8]>>(&mut self, input: B) {
        self.engine.input(input.as_ref());
    }
}

impl FixedOutput for Sha256 {
    type OutputSize = U32;

    fn fixed_result(mut self) -> GenericArray<u8, Self::OutputSize> {
        self.engine.finish();
        let mut out = GenericArray::default();
        BE::write_u32_into(&self.engine.state.h, out.as_mut_slice());
        out
    }
}

impl Reset for Sha256 {
    fn reset(&mut self) {
        self.engine.reset(&H256);
    }
}

/// The SHA-256 hash algorithm with the SHA-224 initial hash value. The result
/// is truncated to 224 bits.
#[derive(Clone)]
pub struct Sha224 {
    engine: Engine256,
}

impl Default for Sha224 {
    fn default() -> Self { Sha224 { engine: Engine256::new(&H224) } }
}

impl BlockInput for Sha224 {
    type BlockSize = BlockSize;
}

impl Input for Sha224 {
    fn input<B: AsRef<[u8]>>(&mut self, input: B) {
        self.engine.input(input.as_ref());
    }
}

impl FixedOutput for Sha224 {
    type OutputSize = U28;

    fn fixed_result(mut self) -> GenericArray<u8, Self::OutputSize> {
        self.engine.finish();
        let mut out = GenericArray::default();
        BE::write_u32_into(&self.engine.state.h[..7], out.as_mut_slice());
        out
    }
}

impl Reset for Sha224 {
    fn reset(&mut self) {
        self.engine.reset(&H224);
    }
}

impl_opaque_debug!(Sha224);
impl_opaque_debug!(Sha256);

impl_write!(Sha224);
impl_write!(Sha256);
