//! Chunk types and functions
#![allow(dead_code)]
#![allow(non_upper_case_globals)]

pub type ChunkType = [u8; 4];

// -- Critical chunks --

/// Image header
pub const IHDR: ChunkType = [b'I', b'H', b'D', b'R'];
/// Palette
pub const PLTE: ChunkType = [b'P', b'L', b'T', b'E'];
/// Image data
pub const IDAT: ChunkType = [b'I', b'D', b'A', b'T'];
/// Image trailer
pub const IEND: ChunkType = [b'I', b'E', b'N', b'D'];

// -- Ancillary chunks --

/// Transparency
pub const tRNS: ChunkType = [b't', b'R', b'N', b'S'];
/// Background colour
pub const bKGD: ChunkType = [b'b', b'K', b'G', b'D'];
/// Image last-modification time
pub const tIME: ChunkType = [b't', b'I', b'M', b'E'];
/// Physical pixel dimensions
pub const pHYs: ChunkType = [b'p', b'H', b'Y', b's'];

// -- Extension chunks --

/// Animation control
pub const acTL: ChunkType = [b'a', b'c', b'T', b'L'];
/// Frame control
pub const fcTL: ChunkType = [b'f', b'c', b'T', b'L'];
/// Frame data
pub const fdAT: ChunkType = [b'f', b'd', b'A', b'T'];

// -- Chunk type determination --

/// Returns true if the chunk is critical.
pub fn is_critical(type_: ChunkType) -> bool {
    type_[0] & 32 == 0
}

/// Returns true if the chunk is private.
pub fn is_private(type_: ChunkType) -> bool {
    type_[1] & 32 != 0
}

/// Checks whether the reserved bit of the chunk name is set.
/// If it is set the chunk name is invalid.
pub fn reserved_set(type_: ChunkType) -> bool {
    type_[2] & 32 != 0
}

/// Returns true if the chunk is safe to copy if unknown.
pub fn safe_to_copy(type_: ChunkType) -> bool {
    type_[3] & 32 != 0
}