#[cfg(feature = "simd")]
use std::mem::transmute;
use std::ops::{Add, BitXor, Mul};

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[cfg_attr(feature = "simd", repr(simd))]
#[allow(non_camel_case_types)]
pub struct u64x2(pub u64, pub u64);

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[cfg_attr(feature = "simd", repr(simd))]
#[allow(non_camel_case_types)]
struct u32x4(u32, u32, u32, u32);

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[cfg_attr(feature = "simd", repr(simd))]
#[allow(non_camel_case_types)]
struct u8x16(u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8);

#[cfg(feature = "simd")]
extern "platform-intrinsic" {
    fn x86_mm_mul_epu32(x: u32x4, y: u32x4) -> u64x2;
    fn simd_add<T>(x: T, y: T) -> T;
    fn simd_mul<T>(x: T, y: T) -> T;
    fn simd_xor<T>(x: T, y: T) -> T;
    fn simd_shr<T>(x: T, y: T) -> T;
    fn simd_shl<T>(x: T, y: T) -> T;
    fn simd_shuffle16<T, U>(x: T, y: T, idx: [u32; 16]) -> U;
}

impl Add for u64x2 {
    type Output = Self;
    #[cfg(feature = "simd")]
    #[inline(always)]
    fn add(self, r: Self) -> Self { unsafe { simd_add(self, r) } }
    #[cfg(not(feature = "simd"))]
    #[inline(always)]
    fn add(self, r: Self) -> Self {
        u64x2(self.0.wrapping_add(r.0), self.1.wrapping_add(r.1))
    }
}

impl Mul for u64x2 {
    type Output = Self;
    #[cfg(feature = "simd")]
    #[inline(always)]
    fn mul(self, r: Self) -> Self { unsafe { simd_mul(self, r) } }
    #[cfg(not(feature = "simd"))]
    fn mul(self, r: Self) -> Self {
        u64x2(self.0.wrapping_mul(r.0), self.1.wrapping_mul(r.1))
    }
}

impl BitXor for u64x2 {
    type Output = Self;
    #[cfg(feature = "simd")]
    #[inline(always)]
    fn bitxor(self, r: Self) -> u64x2 { unsafe { simd_xor(self, r) } }
    #[cfg(not(feature = "simd"))]
    #[inline(always)]
    fn bitxor(self, r: Self) -> u64x2 { u64x2(self.0 ^ r.0, self.1 ^ r.1) }
}

#[cfg_attr(rustfmt, rustfmt_skip)]
#[cfg(feature = "simd")]
impl u8x16 {
    #[inline(always)]
    fn rotr_32_u64x2(self) -> u64x2 {
        unsafe {
            let rv: Self = simd_shuffle16(self, self, [4, 5, 6, 7, 0, 1, 2, 3,
                                                12, 13, 14, 15, 8, 9, 10, 11]);
            transmute(rv)
        }
    }

    #[inline(always)]
    fn rotr_24_u64x2(self) -> u64x2 {
        // recall that rotating right = rotating towards lsb
        unsafe {
            let rv: Self = simd_shuffle16(self, self, [3, 4, 5, 6, 7, 0, 1, 2,
                                                11, 12, 13, 14, 15, 8, 9, 10]);
            transmute(rv)
        }
    }

    #[inline(always)]
    fn rotr_16_u64x2(self) -> u64x2 {
        unsafe {
            let rv: Self = simd_shuffle16(self, self, [2, 3, 4, 5, 6, 7, 0, 1,
                                                10, 11, 12, 13, 14, 15, 8, 9]);
            transmute(rv)
        }
    }

    #[inline(always)]
    fn rotr_8_u64x2(self) -> u64x2 {
        unsafe {
            let rv: Self = simd_shuffle16(self, self, [1, 2, 3, 4, 5, 6, 7, 0,
                                                9, 10, 11, 12, 13, 14, 15, 8]);
            transmute(rv)
        }
    }
}

fn lo(n: u64) -> u64 { n & 0xffffffff }

impl u64x2 {
    #[cfg(feature = "simd")]
    #[inline(always)]
    fn as_u8x16(self) -> u8x16 { unsafe { transmute(self) } }

    #[cfg(feature = "simd")]
    #[inline(always)]
    pub fn lower_mult(self, r: Self) -> Self {
        unsafe {
            let (lhs, rhs): (u32x4, u32x4) = (transmute(self), transmute(r));
            x86_mm_mul_epu32(lhs, rhs)
        }
    }

    #[cfg(not(feature = "simd"))]
    #[inline(always)]
    pub fn lower_mult(self, r: Self) -> Self {
        u64x2(lo(self.0) * lo(r.0), lo(self.1) * lo(r.1))
    }

    #[cfg(feature = "simd")]
    #[inline(always)]
    pub fn rotate_right(self, n: u32) -> Self {
        match n {
            32 => self.as_u8x16().rotr_32_u64x2(),
            24 => self.as_u8x16().rotr_24_u64x2(),
            16 => self.as_u8x16().rotr_16_u64x2(),
            8 => self.as_u8x16().rotr_8_u64x2(),
            _ => unsafe {
                let k = (64 - n) as u64;
                simd_shl(self, u64x2(k, k)) ^
                simd_shr(self, u64x2(n as u64, n as u64))
            },
        }
    }

    #[cfg(not(feature = "simd"))]
    #[inline(always)]
    pub fn rotate_right(self, n: u32) -> Self {
        u64x2(self.0.rotate_right(n), self.1.rotate_right(n))
    }

    #[inline(always)]
    pub fn cross_swap(self, r: Self) -> (Self, Self) {
        let u64x2(v4, v5) = self;       // so the rule is:
        let u64x2(v6, v7) = r;          // +--+
        (u64x2(v7, v4), u64x2(v5, v6))  //  \/
                                        //  /\
                                        // 1  0
    }
}

#[cfg(test)]
mod test {
    use super::u64x2;

    // XXX Use function call to workaround rust-lang/rust#33764.
    fn t0() -> u64x2 {
        u64x2(0xdeadbeef_01234567, 0xcafe3210_babe9932)
    }
    fn t1() -> u64x2 {
        u64x2(0x09990578_01234567, 0x1128f9a9_88e89448)
    }

    #[test]
    fn test_rotate_right() {
        for &s in [8 as u32, 16, 24, 32].iter() {
            assert_eq!(t0().rotate_right(s).0, t0().0.rotate_right(s));
            assert_eq!(t0().rotate_right(s).1, t0().1.rotate_right(s));
        }
    }

    #[test]
    fn test_lower_mult() {
        use super::lo;
        assert_eq!(t0().lower_mult(t1()), t1().lower_mult(t0()));
        assert_eq!(t0().lower_mult(t1()).0, lo(t0().0) * lo(t1().0));
        assert_eq!(t0().lower_mult(t1()).1, lo(t0().1) * lo(t1().1));
    }
}
