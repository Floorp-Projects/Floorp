use core::convert::TryInto;

const PLEN: usize = 25;
const DEFAULT_ROUND_COUNT: usize = 24;

#[derive(Clone)]
pub(crate) struct Sha3State {
    pub state: [u64; PLEN],
    round_count: usize,
}

impl Default for Sha3State {
    fn default() -> Self {
        Self {
            state: [0u64; PLEN],
            round_count: DEFAULT_ROUND_COUNT,
        }
    }
}

impl Sha3State {
    pub(crate) fn new(round_count: usize) -> Self {
        Self {
            state: [0u64; PLEN],
            round_count,
        }
    }

    #[inline(always)]
    pub(crate) fn absorb_block(&mut self, block: &[u8]) {
        debug_assert_eq!(block.len() % 8, 0);

        for (b, s) in block.chunks_exact(8).zip(self.state.iter_mut()) {
            *s ^= u64::from_le_bytes(b.try_into().unwrap());
        }

        keccak::p1600(&mut self.state, self.round_count);
    }

    #[inline(always)]
    pub(crate) fn as_bytes(&self, out: &mut [u8]) {
        for (o, s) in out.chunks_mut(8).zip(self.state.iter()) {
            o.copy_from_slice(&s.to_le_bytes()[..o.len()]);
        }
    }

    #[inline(always)]
    pub(crate) fn permute(&mut self) {
        keccak::p1600(&mut self.state, self.round_count);
    }
}
