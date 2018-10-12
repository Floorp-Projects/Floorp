// Copyright 2015 blake2-rfc Developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use arrayvec::ArrayVec;

pub const SIGMA: [[usize; 16]; 10] = [
    [ 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15],
    [14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3],
    [11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4],
    [ 7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8],
    [ 9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13],
    [ 2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9],
    [12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11],
    [13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10],
    [ 6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5],
    [10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0],
];

macro_rules! blake2_impl {
    ($state:ident, $result:ident, $func:ident, $word:ident, $vec:ident,
     $bytes:expr, $R1:expr, $R2:expr, $R3:expr, $R4:expr, $IV:expr) => {
        use core::cmp;

        #[cfg(feature = "std")]
        use std::io;

        use $crate::as_bytes::AsBytes;
        use $crate::bytes::BytesExt;
        use $crate::constant_time_eq::constant_time_eq;
        use $crate::simd::{Vector4, $vec};

        /// Container for a hash result.
        ///
        /// This container uses a constant-time comparison for equality.
        /// If a constant-time comparison is not necessary, the hash
        /// result can be extracted with the `as_bytes` method.
        #[derive(Clone, Copy, Debug)]
        pub struct $result {
            h: [$vec; 2],
            nn: usize,
        }

        #[cfg_attr(feature = "clippy", allow(len_without_is_empty))]
        impl $result {
            /// Returns the contained hash result as a byte string.
            #[inline]
            pub fn as_bytes(&self) -> &[u8] { &self.h.as_bytes()[..self.nn] }

            /// Returns the length of the hash result.
            ///
            /// This is the same value that was used to create the hash
            /// context.
            #[inline]
            pub fn len(&self) -> usize { self.nn }
        }

        impl AsRef<[u8]> for $result {
            #[inline]
            fn as_ref(&self) -> &[u8] { self.as_bytes() }
        }

        impl Eq for $result { }

        impl PartialEq for $result {
            #[inline]
            fn eq(&self, other: &Self) -> bool {
                constant_time_eq(self.as_bytes(), other.as_bytes())
            }
        }

        impl PartialEq<[u8]> for $result {
            #[inline]
            fn eq(&self, other: &[u8]) -> bool {
                constant_time_eq(self.as_bytes(), other)
            }
        }

        /// State context.
        #[derive(Clone, Debug)]
        pub struct $state {
            m: [$word; 16],
            h: [$vec; 2],
            t: u64,
            nn: usize,
        }

        const IV: [$word; 8] = $IV;

        #[inline(always)]
        fn iv0() -> $vec { $vec::new(IV[0], IV[1], IV[2], IV[3]) }
        #[inline(always)]
        fn iv1() -> $vec { $vec::new(IV[4], IV[5], IV[6], IV[7]) }

        /// Convenience function for all-in-one computation.
        pub fn $func(nn: usize, k: &[u8], data: &[u8]) -> $result {
            let mut state = $state::with_key(nn, k);
            state.update(data);
            state.finalize()
        }

        impl $state {
            /// Creates a new hashing context without a key.
            pub fn new(nn: usize) -> Self { Self::with_key(nn, &[]) }

            /// Creates a new hashing context with a key.
            #[cfg_attr(feature = "clippy", allow(cast_possible_truncation))]
            pub fn with_key(nn: usize, k: &[u8]) -> Self {
                let kk = k.len();
                assert!(nn >= 1 && nn <= $bytes && kk <= $bytes);

                let p0 = 0x01010000 ^ ((kk as $word) << 8) ^ (nn as $word);
                let mut state = $state {
                    m: [0; 16],
                    h: [iv0() ^ $vec::new(p0, 0, 0, 0), iv1()],
                    t: 0,
                    nn: nn,
                };

                if kk > 0 {
                    state.m.as_mut_bytes().copy_bytes_from(k);
                    state.t = $bytes * 2;
                }
                state
            }

            #[doc(hidden)]
            #[cfg_attr(feature = "clippy", allow(cast_possible_truncation))]
            pub fn with_parameter_block(p: &[$word; 8]) -> Self {
                let nn = p[0] as u8 as usize;
                let kk = (p[0] >> 8) as u8 as usize;
                assert!(nn >= 1 && nn <= $bytes && kk <= $bytes);

                $state {
                    m: [0; 16],
                    h: [iv0() ^ $vec::new(p[0], p[1], p[2], p[3]),
                        iv1() ^ $vec::new(p[4], p[5], p[6], p[7])],
                    t: 0,
                    nn: nn,
                }
            }

            /// Updates the hashing context with more data.
            #[cfg_attr(feature = "clippy", allow(cast_possible_truncation))]
            pub fn update(&mut self, data: &[u8]) {
                let mut rest = data;

                let off = (self.t % ($bytes * 2)) as usize;
                if off != 0 || self.t == 0 {
                    let len = cmp::min(($bytes * 2) - off, rest.len());

                    let part = &rest[..len];
                    rest = &rest[part.len()..];

                    self.m.as_mut_bytes()[off..].copy_bytes_from(part);
                    self.t = self.t.checked_add(part.len() as u64)
                        .expect("hash data length overflow");
                }

                while rest.len() >= $bytes * 2 {
                    self.compress(0, 0);

                    let part = &rest[..($bytes * 2)];
                    rest = &rest[part.len()..];

                    self.m.as_mut_bytes().copy_bytes_from(part);
                    self.t = self.t.checked_add(part.len() as u64)
                        .expect("hash data length overflow");
                }

                if rest.len() > 0 {
                    self.compress(0, 0);

                    self.m.as_mut_bytes().copy_bytes_from(rest);
                    self.t = self.t.checked_add(rest.len() as u64)
                        .expect("hash data length overflow");
                }
            }

            /// Consumes the hashing context and returns the resulting hash.
            pub fn finalize(self) -> $result {
                self.finalize_with_flag(0)
            }

            #[doc(hidden)]
            pub fn finalize_last_node(self) -> $result {
                self.finalize_with_flag(!0)
            }

            #[cfg_attr(feature = "clippy", allow(cast_possible_truncation))]
            fn finalize_with_flag(mut self, f1: $word) -> $result {
                let off = (self.t % ($bytes * 2)) as usize;
                if off != 0 {
                    self.m.as_mut_bytes()[off..].set_bytes(0);
                }

                self.compress(!0, f1);

                $result {
                    h: [self.h[0].to_le(), self.h[1].to_le()],
                    nn: self.nn,
                }
            }

            #[inline(always)]
            fn quarter_round(v: &mut [$vec; 4], rd: u32, rb: u32, m: $vec) {
                v[0] = v[0].wrapping_add(v[1]).wrapping_add(m.from_le());
                v[3] = (v[3] ^ v[0]).rotate_right_const(rd);
                v[2] = v[2].wrapping_add(v[3]);
                v[1] = (v[1] ^ v[2]).rotate_right_const(rb);
            }

            #[inline(always)]
            fn shuffle(v: &mut [$vec; 4]) {
                v[1] = v[1].shuffle_left_1();
                v[2] = v[2].shuffle_left_2();
                v[3] = v[3].shuffle_left_3();
            }

            #[inline(always)]
            fn unshuffle(v: &mut [$vec; 4]) {
                v[1] = v[1].shuffle_right_1();
                v[2] = v[2].shuffle_right_2();
                v[3] = v[3].shuffle_right_3();
            }

            #[inline(always)]
            fn round(v: &mut [$vec; 4], m: &[$word; 16], s: &[usize; 16]) {
                $state::quarter_round(v, $R1, $R2, $vec::gather(m,
                                      s[ 0], s[ 2], s[ 4], s[ 6]));
                $state::quarter_round(v, $R3, $R4, $vec::gather(m,
                                      s[ 1], s[ 3], s[ 5], s[ 7]));

                $state::shuffle(v);
                $state::quarter_round(v, $R1, $R2, $vec::gather(m,
                                      s[ 8], s[10], s[12], s[14]));
                $state::quarter_round(v, $R3, $R4, $vec::gather(m,
                                      s[ 9], s[11], s[13], s[15]));
                $state::unshuffle(v);
            }

            #[cfg_attr(feature = "clippy", allow(cast_possible_truncation, eq_op))]
            fn compress(&mut self, f0: $word, f1: $word) {
                use $crate::blake2::SIGMA;

                let m = &self.m;
                let h = &mut self.h;

                let t0 = self.t as $word;
                let t1 = match $bytes {
                    64 => 0,
                    32 => (self.t >> 32) as $word,
                    _  => unreachable!(),
                };

                let mut v = [
                    h[0],
                    h[1],
                    iv0(),
                    iv1() ^ $vec::new(t0, t1, f0, f1),
                ];

                $state::round(&mut v, m, &SIGMA[0]);
                $state::round(&mut v, m, &SIGMA[1]);
                $state::round(&mut v, m, &SIGMA[2]);
                $state::round(&mut v, m, &SIGMA[3]);
                $state::round(&mut v, m, &SIGMA[4]);
                $state::round(&mut v, m, &SIGMA[5]);
                $state::round(&mut v, m, &SIGMA[6]);
                $state::round(&mut v, m, &SIGMA[7]);
                $state::round(&mut v, m, &SIGMA[8]);
                $state::round(&mut v, m, &SIGMA[9]);
                if $bytes > 32 {
                    $state::round(&mut v, m, &SIGMA[0]);
                    $state::round(&mut v, m, &SIGMA[1]);
                }

                h[0] = h[0] ^ (v[0] ^ v[2]);
                h[1] = h[1] ^ (v[1] ^ v[3]);
            }
        }

        #[cfg(feature = "std")]
        impl io::Write for $state {
            fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
                if self.t.checked_add(buf.len() as u64).is_none() {
                    return Err(io::Error::new(io::ErrorKind::WriteZero,
                                              "counter overflow"));
                }

                self.update(buf);
                Ok(buf.len())
            }

            #[inline]
            fn flush(&mut self) -> io::Result<()> {
                Ok(())
            }
        }
    }
}

#[cfg_attr(feature = "clippy", allow(cast_possible_truncation))]
#[cold]
pub fn selftest_seq(len: usize) -> ArrayVec<[u8; 1024]> {
    use core::num::Wrapping;

    let seed = Wrapping(len as u32);
    let mut out = ArrayVec::new();

    let mut a = Wrapping(0xDEAD4BAD) * seed;
    let mut b = Wrapping(1);

    for _ in 0..len {
        let t = a + b;
        a = b;
        b = t;
        out.push((t >> 24).0 as u8);
    }
    out
}

macro_rules! blake2_selftest_impl {
    ($state:ident, $func:ident, $res:expr, $md_len:expr, $in_len:expr) => {
        /// Runs the self-test for this hash function.
        #[cold]
        pub fn selftest() {
            use $crate::blake2::selftest_seq;

            const BLAKE2_RES: [u8; 32] = $res;
            const B2_MD_LEN: [usize; 4] = $md_len;
            const B2_IN_LEN: [usize; 6] = $in_len;

            let mut state = $state::new(32);

            for &outlen in &B2_MD_LEN {
                for &inlen in &B2_IN_LEN {
                    let data = selftest_seq(inlen);
                    let md = $func(outlen, &[], &data);
                    state.update(md.as_bytes());

                    let key = selftest_seq(outlen);
                    let md = $func(outlen, &key, &data);
                    state.update(md.as_bytes());
                }
            }

            assert_eq!(&state.finalize(), &BLAKE2_RES[..]);
        }
    }
}

macro_rules! blake2_bench_impl {
    ($state:ident, $bytes:expr) => {
        #[cfg(all(feature = "bench", test))]
        mod bench {
            use std::iter::repeat;
            use std::vec::Vec;
            use test::Bencher;

            use blake2::selftest_seq;
            use super::$state;

            fn bench_blake2(bytes: usize, b: &mut Bencher) {
                let data: Vec<u8> = repeat(selftest_seq(1024))
                    .flat_map(|v| v)
                    .take(bytes)
                    .collect();

                b.bytes = bytes as u64;
                b.iter(|| {
                    let mut state = $state::new($bytes);
                    state.update(&data[..]);
                    state.finalize()
                })
            }

            #[bench] fn bench_16(b: &mut Bencher) { bench_blake2(16, b) }
            #[bench] fn bench_4k(b: &mut Bencher) { bench_blake2(4096, b) }
            #[bench] fn bench_64k(b: &mut Bencher) { bench_blake2(65536, b) }
        }
    }
}
