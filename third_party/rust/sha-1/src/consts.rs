#![cfg_attr(feature = "cargo-clippy", allow(unreadable_literal))]

pub const STATE_LEN: usize = 5;

#[cfg(not(feature = "asm"))]
pub const BLOCK_LEN: usize = 16;

#[cfg(not(feature = "asm"))]
pub const K0: u32 = 0x5A827999u32;
#[cfg(not(feature = "asm"))]
pub const K1: u32 = 0x6ED9EBA1u32;
#[cfg(not(feature = "asm"))]
pub const K2: u32 = 0x8F1BBCDCu32;
#[cfg(not(feature = "asm"))]
pub const K3: u32 = 0xCA62C1D6u32;

pub const H: [u32; STATE_LEN] = [
    0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0
];
