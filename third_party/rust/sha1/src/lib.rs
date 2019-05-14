//! A minimal implementation of SHA1 for rust.
//!
//! Example:
//!
//! ```rust
//! extern crate sha1;
//! # fn main() {
//!
//! let mut m = sha1::Sha1::new();
//! m.update(b"Hello World!");
//! assert_eq!(m.digest().to_string(),
//!            "2ef7bde608ce5404e97d5f042f95f89f1c232871");
//! # }
//! ```

#![no_std]
#![deny(missing_docs)]

use core::cmp;
use core::fmt;

/// Represents a Sha1 hash object in memory.
#[derive(Clone)]
pub struct Sha1 {
    state: Sha1State,
    blocks: Blocks,
    len: u64,
}

struct Blocks {
    len: u32,
    block: [u8; 64],
}

#[derive(Copy, Clone)]
struct Sha1State {
    state: [u32; 5],
}

/// Digest generated from a `Sha1` instance.
///
/// A digest can be formatted to view the digest as a hex string, or the bytes
/// can be extracted for later processing.
pub struct Digest {
    data: Sha1State,
}

const DEFAULT_STATE: Sha1State = Sha1State {
    state: [0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0],
};

impl Sha1 {
    /// Creates an fresh sha1 hash object.
    pub fn new() -> Sha1 {
        Sha1 {
            state: DEFAULT_STATE,
            len: 0,
            blocks: Blocks {
                len: 0,
                block: [0; 64],
            },
        }
    }

    /// Resets the hash object to it's initial state.
    pub fn reset(&mut self) {
        self.state = DEFAULT_STATE;
        self.len = 0;
        self.blocks.len = 0;
    }

    /// Update hash with input data.
    pub fn update(&mut self, data: &[u8]) {
        let len = &mut self.len;
        let state = &mut self.state;
        self.blocks.input(data, |chunk| {
            *len += 64;
            state.process(chunk);
        })
    }

    /// Retrieve digest result.
    pub fn digest(&self) -> Digest {
        let mut state = self.state;
        let bits = (self.len + (self.blocks.len as u64)) * 8;
        let extra = [
            (bits >> 56) as u8,
            (bits >> 48) as u8,
            (bits >> 40) as u8,
            (bits >> 32) as u8,
            (bits >> 24) as u8,
            (bits >> 16) as u8,
            (bits >>  8) as u8,
            (bits >>  0) as u8,
        ];
        let mut last = [0; 128];
        let blocklen = self.blocks.len as usize;
        last[..blocklen].clone_from_slice(&self.blocks.block[..blocklen]);
        last[blocklen] = 0x80;

        if blocklen < 56 {
            last[56..64].clone_from_slice(&extra);
            state.process(&last[0..64]);
        } else {
            last[120..128].clone_from_slice(&extra);
            state.process(&last[0..64]);
            state.process(&last[64..128]);
        }

        Digest { data: state }
    }
}

impl Digest {
    /// Returns the 160 bit (20 byte) digest as a byte array.
    pub fn bytes(&self) -> [u8; 20] {
        [
            (self.data.state[0] >> 24) as u8,
            (self.data.state[0] >> 16) as u8,
            (self.data.state[0] >>  8) as u8,
            (self.data.state[0] >>  0) as u8,
            (self.data.state[1] >> 24) as u8,
            (self.data.state[1] >> 16) as u8,
            (self.data.state[1] >>  8) as u8,
            (self.data.state[1] >>  0) as u8,
            (self.data.state[2] >> 24) as u8,
            (self.data.state[2] >> 16) as u8,
            (self.data.state[2] >>  8) as u8,
            (self.data.state[2] >>  0) as u8,
            (self.data.state[3] >> 24) as u8,
            (self.data.state[3] >> 16) as u8,
            (self.data.state[3] >>  8) as u8,
            (self.data.state[3] >>  0) as u8,
            (self.data.state[4] >> 24) as u8,
            (self.data.state[4] >> 16) as u8,
            (self.data.state[4] >>  8) as u8,
            (self.data.state[4] >>  0) as u8,
        ]
    }
}

impl Blocks {
    fn input<F>(&mut self, mut input: &[u8], mut f: F) where F: FnMut(&[u8]) {
        if self.len > 0 {
            let len = self.len as usize;
            let amt = cmp::min(input.len(), self.block.len() - len);
            self.block[len..len + amt].clone_from_slice(&input[..amt]);
            if len + amt == self.block.len() {
                f(&self.block);
                self.len = 0;
                input = &input[amt..];
            } else {
                self.len += amt as u32;
                return
            }
        }
        assert_eq!(self.len, 0);
        for chunk in input.chunks(64) {
            if chunk.len() == 64 {
                f(chunk)
            } else {
                self.block[..chunk.len()].clone_from_slice(chunk);
                self.len = chunk.len() as u32;
            }
        }
    }
}

impl Sha1State {
    fn process(&mut self, block: &[u8]) {
        let mut words = [0u32; 80];
        for (i, chunk) in block.chunks(4).enumerate() {
            words[i] = (chunk[3] as u32) |
                       ((chunk[2] as u32) << 8) |
                       ((chunk[1] as u32) << 16) |
                       ((chunk[0] as u32) << 24);
        }

        fn ff(b: u32, c: u32, d: u32) -> u32 { d ^ (b & (c ^ d)) }
        fn gg(b: u32, c: u32, d: u32) -> u32 { b ^ c ^ d }
        fn hh(b: u32, c: u32, d: u32) -> u32 { (b & c) | (d & (b | c)) }
        fn ii(b: u32, c: u32, d: u32) -> u32 { b ^ c ^ d }

        for i in 16..80 {
            let n = words[i - 3] ^ words[i - 8] ^ words[i - 14] ^ words[i - 16];
            words[i] = n.rotate_left(1);
        }

        let mut a = self.state[0];
        let mut b = self.state[1];
        let mut c = self.state[2];
        let mut d = self.state[3];
        let mut e = self.state[4];

        for i in 0..80 {
            let (f, k) = match i {
                0 ... 19 => (ff(b, c, d), 0x5a827999),
                20 ... 39 => (gg(b, c, d), 0x6ed9eba1),
                40 ... 59 => (hh(b, c, d), 0x8f1bbcdc),
                60 ... 79 => (ii(b, c, d), 0xca62c1d6),
                _ => (0, 0),
            };

            let tmp = a.rotate_left(5)
                       .wrapping_add(f)
                       .wrapping_add(e)
                       .wrapping_add(k)
                       .wrapping_add(words[i]);
            e = d;
            d = c;
            c = b.rotate_left(30);
            b = a;
            a = tmp;
        }

        self.state[0] = self.state[0].wrapping_add(a);
        self.state[1] = self.state[1].wrapping_add(b);
        self.state[2] = self.state[2].wrapping_add(c);
        self.state[3] = self.state[3].wrapping_add(d);
        self.state[4] = self.state[4].wrapping_add(e);
    }
}

impl Clone for Blocks {
    fn clone(&self) -> Blocks {
        Blocks { ..*self }
    }
}

impl fmt::Display for Digest {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        for byte in self.data.state.iter() {
            try!(write!(f, "{:08x}", byte));
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    extern crate std;
    extern crate rand;
    extern crate openssl;

    use self::std::prelude::v1::*;
    use self::std::io::Write;

    use Sha1;

    #[test]
    fn test_simple() {
        let mut m = Sha1::new();

        let tests = [
            ("The quick brown fox jumps over the lazy dog",
             "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12"),
            ("The quick brown fox jumps over the lazy cog",
             "de9f2c7fd25e1b3afad3e85a0bd17d9b100db4b3"),
            ("", "da39a3ee5e6b4b0d3255bfef95601890afd80709"),
            ("testing\n", "9801739daae44ec5293d4e1f53d3f4d2d426d91c"),
            ("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
             "025ecbd5d70f8fb3c5457cd96bab13fda305dc59"),
            ("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
             "4300320394f7ee239bcdce7d3b8bcee173a0cd5c"),
            ("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
             "cef734ba81a024479e09eb5a75b6ddae62e6abf1"),
        ];

        for &(s, ref h) in tests.iter() {
            let data = s.as_bytes();

            m.reset();
            m.update(data);
            let hh = m.digest().to_string();

            assert_eq!(hh.len(), h.len());
            assert_eq!(hh, *h);
        }
    }

    #[test]
    fn test_multiple_updates() {
        let mut m = Sha1::new();

        m.reset();
        m.update("The quick brown ".as_bytes());
        m.update("fox jumps over ".as_bytes());
        m.update("the lazy dog".as_bytes());
        let hh = m.digest().to_string();


        let h = "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12";
        assert_eq!(hh.len(), h.len());
        assert_eq!(hh, &*h);
    }

    #[test]
    fn test_sha1_loop() {
        let mut m = Sha1::new();
        let s = "The quick brown fox jumps over the lazy dog.";
        let n = 1000u64;

        for _ in 0..3 {
            m.reset();
            for _ in 0..n {
                m.update(s.as_bytes());
            }
            assert_eq!(m.digest().to_string(),
                       "7ca27655f67fceaa78ed2e645a81c7f1d6e249d2");
        }
    }

    #[test]
    fn spray_and_pray() {
        use self::rand::Rng;

        let mut rng = rand::thread_rng();
        let mut m = Sha1::new();
        let mut bytes = [0; 512];

        for _ in 0..20 {
            let ty = openssl::crypto::hash::Type::SHA1;
            let mut r = openssl::crypto::hash::Hasher::new(ty);
            m.reset();
            for _ in 0..50 {
                let len = rng.gen::<usize>() % bytes.len();
                rng.fill_bytes(&mut bytes[..len]);
                m.update(&bytes[..len]);
                r.write(&bytes[..len]).unwrap();
            }
            assert_eq!(r.finish(), m.digest().bytes());
        }
    }
}
