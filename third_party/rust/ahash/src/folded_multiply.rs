use crate::convert::*;
use core::ops::Add;
use core::ops::Mul;

pub(crate) trait FoldedMultiply: Mul + Add + Sized {
    fn folded_multiply(&self, by: &Self) -> Self;
}

impl FoldedMultiply for u64 {
    #[inline(always)]
    fn folded_multiply(&self, by: &u64) -> u64 {
        let result: [u64; 2] = (*self as u128).wrapping_mul(*by as u128).convert();
        result[0].wrapping_add(result[1])
    }
}
