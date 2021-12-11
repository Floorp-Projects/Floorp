#![no_std]
use core::ops::{Add, BitAnd, BitOr, BitXor, Shl, Shr, Sub};

#[derive(Clone, Copy, PartialEq, Eq)]
#[allow(non_camel_case_types)]
pub struct u32x4(pub u32, pub u32, pub u32, pub u32);

impl Add for u32x4 {
    type Output = u32x4;

    #[inline(always)]
    fn add(self, rhs: u32x4) -> u32x4 {
        u32x4(
            self.0.wrapping_add(rhs.0),
            self.1.wrapping_add(rhs.1),
            self.2.wrapping_add(rhs.2),
            self.3.wrapping_add(rhs.3))
    }
}

impl Sub for u32x4 {
    type Output = u32x4;

    #[inline(always)]
    fn sub(self, rhs: u32x4) -> u32x4 {
        u32x4(
            self.0.wrapping_sub(rhs.0),
            self.1.wrapping_sub(rhs.1),
            self.2.wrapping_sub(rhs.2),
            self.3.wrapping_sub(rhs.3))
    }
}

impl BitAnd for u32x4 {
    type Output = u32x4;

    #[inline(always)]
    fn bitand(self, rhs: u32x4) -> u32x4 {
        u32x4(self.0 & rhs.0, self.1 & rhs.1, self.2 & rhs.2, self.3 & rhs.3)
    }
}

impl BitOr for u32x4 {
    type Output = u32x4;

    #[inline(always)]
    fn bitor(self, rhs: u32x4) -> u32x4 {
        u32x4(self.0 | rhs.0, self.1 | rhs.1, self.2 | rhs.2, self.3 | rhs.3)
    }
}

impl BitXor for u32x4 {
    type Output = u32x4;

    #[inline(always)]
    fn bitxor(self, rhs: u32x4) -> u32x4 {
        u32x4(self.0 ^ rhs.0, self.1 ^ rhs.1, self.2 ^ rhs.2, self.3 ^ rhs.3)
    }
}

impl Shl<usize> for u32x4 {
    type Output = u32x4;

    #[inline(always)]
    fn shl(self, amt: usize) -> u32x4 {
        u32x4(self.0 << amt, self.1 << amt, self.2 << amt, self.3 << amt)
    }
}

impl Shl<u32x4> for u32x4 {
    type Output = u32x4;

    #[inline(always)]
    fn shl(self, rhs: u32x4) -> u32x4 {
        u32x4(self.0 << rhs.0, self.1 << rhs.1, self.2 << rhs.2, self.3 << rhs.3)
    }
}

impl Shr<usize> for u32x4 {
    type Output = u32x4;

    #[inline(always)]
    fn shr(self, amt: usize) -> u32x4 {
        u32x4(self.0 >> amt, self.1 >> amt, self.2 >> amt, self.3 >> amt)
    }
}

impl Shr<u32x4> for u32x4 {
    type Output = u32x4;

    #[inline(always)]
    fn shr(self, rhs: u32x4) -> u32x4 {
        u32x4(self.0 >> rhs.0, self.1 >> rhs.1, self.2 >> rhs.2, self.3 >> rhs.3)
    }
}

#[derive(Clone, Copy)]
#[allow(non_camel_case_types)]
pub struct u64x2(pub u64, pub u64);

impl Add for u64x2 {
    type Output = u64x2;

    #[inline(always)]
    fn add(self, rhs: u64x2) -> u64x2 {
        u64x2(self.0.wrapping_add(rhs.0), self.1.wrapping_add(rhs.1))
    }
}
