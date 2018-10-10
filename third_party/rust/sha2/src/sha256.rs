use digest;
use digest::generic_array::GenericArray;
use digest::generic_array::typenum::{U28, U32, U64};
use block_buffer::BlockBuffer512;
use byte_tools::write_u32v_be;

use consts::{STATE_LEN, H224, H256};

#[cfg(not(feature = "asm"))]
use sha256_utils::compress256;
#[cfg(feature = "asm")]
use sha2_asm::compress256;

type BlockSize = U64;
pub type Block = [u8; 64];

/// A structure that represents that state of a digest computation for the
/// SHA-2 512 family of digest functions
#[derive(Clone)]
struct Engine256State {
    h: [u32; 8],
}

impl Engine256State {
    fn new(h: &[u32; STATE_LEN]) -> Engine256State { Engine256State { h: *h } }

    pub fn process_block(&mut self, data: &Block) {
        compress256(&mut self.h, data);
    }
}

/// A structure that keeps track of the state of the Sha-256 operation and
/// contains the logic necessary to perform the final calculations.
#[derive(Clone)]
struct Engine256 {
    len: u64,
    buffer: BlockBuffer512,
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
        // TODO: replace with `len_padding_be` method
        let l = if cfg!(target_endian = "little") { l.to_be() } else { l.to_le() };
        self.buffer.len_padding(l, |input| self_state.process_block(input));
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

impl digest::BlockInput for Sha256 {
    type BlockSize = BlockSize;
}

impl digest::Input for Sha256 {
    fn process(&mut self, msg: &[u8]) { self.engine.input(msg); }
}

impl digest::FixedOutput for Sha256 {
    type OutputSize = U32;

    fn fixed_result(mut self) -> GenericArray<u8, Self::OutputSize> {
        self.engine.finish();
        let mut out = GenericArray::default();
        write_u32v_be(out.as_mut_slice(), &self.engine.state.h);
        out
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

impl digest::BlockInput for Sha224 {
    type BlockSize = BlockSize;
}

impl digest::Input for Sha224 {
    fn process(&mut self, msg: &[u8]) { self.engine.input(msg); }
}

impl digest::FixedOutput for Sha224 {
    type OutputSize = U28;

    fn fixed_result(mut self) -> GenericArray<u8, Self::OutputSize> {
        self.engine.finish();
        let mut out = GenericArray::default();
        write_u32v_be(out.as_mut_slice(), &self.engine.state.h[..7]);
        out
    }
}

impl_opaque_debug!(Sha224);
impl_opaque_debug!(Sha256);
