#![cfg_attr(feature = "cargo-clippy", allow(many_single_char_names))]

use simd::u64x2;
use consts::{BLOCK_LEN, K64X2};
use byte_tools::{read_u64v_be};
use sha512::Block;

/// Not an intrinsic, but works like an unaligned load.
#[inline]
fn sha512load(v0: u64x2, v1: u64x2) -> u64x2 {
    u64x2(v1.1, v0.0)
}

/// Performs 2 rounds of the SHA-512 message schedule update.
pub fn sha512_schedule_x2(v0: u64x2, v1: u64x2, v4to5: u64x2, v7: u64x2)
                          -> u64x2 {

    // sigma 0
    fn sigma0(x: u64) -> u64 {
        ((x << 63) | (x >> 1)) ^ ((x << 56) | (x >> 8)) ^ (x >> 7)
    }

    // sigma 1
    fn sigma1(x: u64) -> u64 {
        ((x << 45) | (x >> 19)) ^ ((x << 3) | (x >> 61)) ^ (x >> 6)
    }

    let u64x2(w1, w0) = v0;
    let u64x2(_, w2) = v1;
    let u64x2(w10, w9) = v4to5;
    let u64x2(w15, w14) = v7;

    let w16 =
        sigma1(w14).wrapping_add(w9).wrapping_add(sigma0(w1)).wrapping_add(w0);
    let w17 =
        sigma1(w15).wrapping_add(w10).wrapping_add(sigma0(w2)).wrapping_add(w1);

    u64x2(w17, w16)
}

/// Performs one round of the SHA-512 message block digest.
pub fn sha512_digest_round(ae: u64x2, bf: u64x2, cg: u64x2, dh: u64x2,
                           wk0: u64)
                           -> u64x2 {

    macro_rules! big_sigma0 {
        ($a:expr) => (($a.rotate_right(28) ^ $a.rotate_right(34) ^ $a.rotate_right(39)))
    }
    macro_rules! big_sigma1 {
        ($a:expr) => (($a.rotate_right(14) ^ $a.rotate_right(18) ^ $a.rotate_right(41)))
    }
    macro_rules! bool3ary_202 {
        ($a:expr, $b:expr, $c:expr) => ($c ^ ($a & ($b ^ $c)))
    } // Choose, MD5F, SHA1C
    macro_rules! bool3ary_232 {
        ($a:expr, $b:expr, $c:expr) => (($a & $b) ^ ($a & $c) ^ ($b & $c))
    } // Majority, SHA1M

    let u64x2(a0, e0) = ae;
    let u64x2(b0, f0) = bf;
    let u64x2(c0, g0) = cg;
    let u64x2(d0, h0) = dh;

    // a round
    let x0 = big_sigma1!(e0)
        .wrapping_add(bool3ary_202!(e0, f0, g0))
        .wrapping_add(wk0)
        .wrapping_add(h0);
    let y0 = big_sigma0!(a0).wrapping_add(bool3ary_232!(a0, b0, c0));
    let (a1, _, _, _, e1, _, _, _) =
        (x0.wrapping_add(y0), a0, b0, c0, x0.wrapping_add(d0), e0, f0, g0);

    u64x2(a1, e1)
}

/// Process a block with the SHA-512 algorithm.
pub fn sha512_digest_block_u64(state: &mut [u64; 8], block: &[u64; 16]) {
    let k = &K64X2;

    macro_rules! schedule {
        ($v0:expr, $v1:expr, $v4:expr, $v5:expr, $v7:expr) => (
             sha512_schedule_x2($v0, $v1, sha512load($v4, $v5), $v7)
        )
    }

    macro_rules! rounds4 {
        ($ae:ident, $bf:ident, $cg:ident, $dh:ident, $wk0:expr, $wk1:expr) => {
            {
                let u64x2(u, t) = $wk0;
                let u64x2(w, v) = $wk1;

                $dh = sha512_digest_round($ae, $bf, $cg, $dh, t);
                $cg = sha512_digest_round($dh, $ae, $bf, $cg, u);
                $bf = sha512_digest_round($cg, $dh, $ae, $bf, v);
                $ae = sha512_digest_round($bf, $cg, $dh, $ae, w);
            }
        }
    }

    let mut ae = u64x2(state[0], state[4]);
    let mut bf = u64x2(state[1], state[5]);
    let mut cg = u64x2(state[2], state[6]);
    let mut dh = u64x2(state[3], state[7]);

    // Rounds 0..20
    let (mut w1, mut w0) = (u64x2(block[3], block[2]),
                            u64x2(block[1], block[0]));
    rounds4!(ae, bf, cg, dh, k[0] + w0, k[1] + w1);
    let (mut w3, mut w2) = (u64x2(block[7], block[6]),
                            u64x2(block[5], block[4]));
    rounds4!(ae, bf, cg, dh, k[2] + w2, k[3] + w3);
    let (mut w5, mut w4) = (u64x2(block[11], block[10]),
                            u64x2(block[9], block[8]));
    rounds4!(ae, bf, cg, dh, k[4] + w4, k[5] + w5);
    let (mut w7, mut w6) = (u64x2(block[15], block[14]),
                            u64x2(block[13], block[12]));
    rounds4!(ae, bf, cg, dh, k[6] + w6, k[7] + w7);
    let mut w8 = schedule!(w0, w1, w4, w5, w7);
    let mut w9 = schedule!(w1, w2, w5, w6, w8);
    rounds4!(ae, bf, cg, dh, k[8] + w8, k[9] + w9);

    // Rounds 20..40
    w0 = schedule!(w2, w3, w6, w7, w9);
    w1 = schedule!(w3, w4, w7, w8, w0);
    rounds4!(ae, bf, cg, dh, k[10] + w0, k[11] + w1);
    w2 = schedule!(w4, w5, w8, w9, w1);
    w3 = schedule!(w5, w6, w9, w0, w2);
    rounds4!(ae, bf, cg, dh, k[12] + w2, k[13] + w3);
    w4 = schedule!(w6, w7, w0, w1, w3);
    w5 = schedule!(w7, w8, w1, w2, w4);
    rounds4!(ae, bf, cg, dh, k[14] + w4, k[15] + w5);
    w6 = schedule!(w8, w9, w2, w3, w5);
    w7 = schedule!(w9, w0, w3, w4, w6);
    rounds4!(ae, bf, cg, dh, k[16] + w6, k[17] + w7);
    w8 = schedule!(w0, w1, w4, w5, w7);
    w9 = schedule!(w1, w2, w5, w6, w8);
    rounds4!(ae, bf, cg, dh, k[18] + w8, k[19] + w9);

    // Rounds 40..60
    w0 = schedule!(w2, w3, w6, w7, w9);
    w1 = schedule!(w3, w4, w7, w8, w0);
    rounds4!(ae, bf, cg, dh, k[20] + w0, k[21] + w1);
    w2 = schedule!(w4, w5, w8, w9, w1);
    w3 = schedule!(w5, w6, w9, w0, w2);
    rounds4!(ae, bf, cg, dh, k[22] + w2, k[23] + w3);
    w4 = schedule!(w6, w7, w0, w1, w3);
    w5 = schedule!(w7, w8, w1, w2, w4);
    rounds4!(ae, bf, cg, dh, k[24] + w4, k[25] + w5);
    w6 = schedule!(w8, w9, w2, w3, w5);
    w7 = schedule!(w9, w0, w3, w4, w6);
    rounds4!(ae, bf, cg, dh, k[26] + w6, k[27] + w7);
    w8 = schedule!(w0, w1, w4, w5, w7);
    w9 = schedule!(w1, w2, w5, w6, w8);
    rounds4!(ae, bf, cg, dh, k[28] + w8, k[29] + w9);

    // Rounds 60..80
    w0 = schedule!(w2, w3, w6, w7, w9);
    w1 = schedule!(w3, w4, w7, w8, w0);
    rounds4!(ae, bf, cg, dh, k[30] + w0, k[31] + w1);
    w2 = schedule!(w4, w5, w8, w9, w1);
    w3 = schedule!(w5, w6, w9, w0, w2);
    rounds4!(ae, bf, cg, dh, k[32] + w2, k[33] + w3);
    w4 = schedule!(w6, w7, w0, w1, w3);
    w5 = schedule!(w7, w8, w1, w2, w4);
    rounds4!(ae, bf, cg, dh, k[34] + w4, k[35] + w5);
    w6 = schedule!(w8, w9, w2, w3, w5);
    w7 = schedule!(w9, w0, w3, w4, w6);
    rounds4!(ae, bf, cg, dh, k[36] + w6, k[37] + w7);
    w8 = schedule!(w0, w1, w4, w5, w7);
    w9 = schedule!(w1, w2, w5, w6, w8);
    rounds4!(ae, bf, cg, dh, k[38] + w8, k[39] + w9);

    let u64x2(a, e) = ae;
    let u64x2(b, f) = bf;
    let u64x2(c, g) = cg;
    let u64x2(d, h) = dh;

    state[0] = state[0].wrapping_add(a);
    state[1] = state[1].wrapping_add(b);
    state[2] = state[2].wrapping_add(c);
    state[3] = state[3].wrapping_add(d);
    state[4] = state[4].wrapping_add(e);
    state[5] = state[5].wrapping_add(f);
    state[6] = state[6].wrapping_add(g);
    state[7] = state[7].wrapping_add(h);
}

/// Process a block with the SHA-512 algorithm. (See more...)
///
/// Internally, this uses functions that resemble the new Intel SHA
/// instruction set extensions, but since no architecture seems to
/// have any designs, these may not be the final designs if and/or when
/// there are instruction set extensions with SHA-512. So to summarize:
/// SHA-1 and SHA-256 are being implemented in hardware soon (at the time
/// of this writing), but it doesn't look like SHA-512 will be hardware
/// accelerated any time soon.
///
/// # Implementation
///
/// These functions fall into two categories: message schedule calculation, and
/// the message block 64-round digest calculation. The schedule-related
/// functions allow 4 rounds to be calculated as:
///
/// ```ignore
/// use std::simd::u64x2;
/// use self::crypto::sha2::{
///     sha512msg,
///     sha512load
/// };
///
/// fn schedule4_data(work: &mut [u64x2], w: &[u64]) {
///
///     // this is to illustrate the data order
///     work[0] = u64x2(w[1], w[0]);
///     work[1] = u64x2(w[3], w[2]);
///     work[2] = u64x2(w[5], w[4]);
///     work[3] = u64x2(w[7], w[6]);
///     work[4] = u64x2(w[9], w[8]);
///     work[5] = u64x2(w[11], w[10]);
///     work[6] = u64x2(w[13], w[12]);
///     work[7] = u64x2(w[15], w[14]);
/// }
///
/// fn schedule4_work(work: &mut [u64x2], t: usize) {
///
///     // this is the core expression
///     work[t] = sha512msg(work[t - 8],
///                         work[t - 7],
///                         sha512load(work[t - 4], work[t - 3]),
///                         work[t - 1]);
/// }
/// ```
///
/// instead of 4 rounds of:
///
/// ```ignore
/// fn schedule_work(w: &mut [u64], t: usize) {
///     w[t] = sigma1!(w[t - 2]) + w[t - 7] + sigma0!(w[t - 15]) + w[t - 16];
/// }
/// ```
///
/// and the digest-related functions allow 4 rounds to be calculated as:
///
/// ```ignore
/// use std::simd::u64x2;
/// use self::crypto::sha2::{K64X2, sha512rnd};
///
/// fn rounds4(state: &mut [u64; 8], work: &mut [u64x2], t: usize) {
///     let [a, b, c, d, e, f, g, h]: [u64; 8] = *state;
///
///     // this is to illustrate the data order
///     let mut ae = u64x2(a, e);
///     let mut bf = u64x2(b, f);
///     let mut cg = u64x2(c, g);
///     let mut dh = u64x2(d, h);
///     let u64x2(w1, w0) = K64X2[2*t]     + work[2*t];
///     let u64x2(w3, w2) = K64X2[2*t + 1] + work[2*t + 1];
///
///     // this is the core expression
///     dh = sha512rnd(ae, bf, cg, dh, w0);
///     cg = sha512rnd(dh, ae, bf, cg, w1);
///     bf = sha512rnd(cg, dh, ae, bf, w2);
///     ae = sha512rnd(bf, cg, dh, ae, w3);
///
///     *state = [ae.0, bf.0, cg.0, dh.0,
///               ae.1, bf.1, cg.1, dh.1];
/// }
/// ```
///
/// instead of 4 rounds of:
///
/// ```ignore
/// fn round(state: &mut [u64; 8], w: &mut [u64], t: usize) {
///     let [a, b, c, mut d, e, f, g, mut h]: [u64; 8] = *state;
///
///     h += big_sigma1!(e) +   choose!(e, f, g) + K64[t] + w[t]; d += h;
///     h += big_sigma0!(a) + majority!(a, b, c);
///
///     *state = [h, a, b, c, d, e, f, g];
/// }
/// ```
///
pub fn compress512(state: &mut [u64; 8], block: &Block) {
    let mut block_u64 = [0u64; BLOCK_LEN];
    read_u64v_be(&mut block_u64[..], block);
    sha512_digest_block_u64(state, &block_u64);
}
