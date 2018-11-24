/*
 * Copyright (c) 2001-2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "config.h"

#include <assert.h>
#include <limits.h>

#include "common/intops.h"

#include "src/msac.h"

typedef MsacContext od_ec_dec;

//#define CDF_SIZE(x) ((x) + 1)
#define CDF_PROB_BITS 15
#define CDF_PROB_TOP (1 << CDF_PROB_BITS)
//#define CDF_INIT_TOP 32768
#define CDF_SHIFT (15 - CDF_PROB_BITS)

#define OD_CLZ0 (1)
#define OD_CLZ(x) (-get_msb(x))
#define OD_ILOG_NZ(x) (OD_CLZ0 - OD_CLZ(x))

static inline int get_msb(unsigned int n) {
    assert(n != 0);
    return 31 ^ clz(n);
}

#define EC_PROB_SHIFT 6
#define EC_MIN_PROB 4  // must be <= (1<<EC_PROB_SHIFT)/16

/*OPT: od_ec_window must be at least 32 bits, but if you have fast arithmetic
 on a larger type, you can speed up the decoder by using it here.*/
typedef uint32_t od_ec_window;

#define OD_EC_WINDOW_SIZE ((int)sizeof(od_ec_window) * CHAR_BIT)

/*The resolution of fractional-precision bit usage measurements, i.e.,
 3 => 1/8th bits.*/
#define OD_BITRES (3)

#define OD_ICDF AOM_ICDF

#define AOM_ICDF(a) (32768-(a))

/*A range decoder.
  This is an entropy decoder based upon \cite{Mar79}, which is itself a
   rediscovery of the FIFO arithmetic code introduced by \cite{Pas76}.
  It is very similar to arithmetic encoding, except that encoding is done with
   digits in any base, instead of with bits, and so it is faster when using
   larger bases (i.e.: a byte).
  The author claims an average waste of $\frac{1}{2}\log_b(2b)$ bits, where $b$
   is the base, longer than the theoretical optimum, but to my knowledge there
   is no published justification for this claim.
  This only seems true when using near-infinite precision arithmetic so that
   the process is carried out with no rounding errors.

  An excellent description of implementation details is available at
   http://www.arturocampos.com/ac_range.html
  A recent work \cite{MNW98} which proposes several changes to arithmetic
   encoding for efficiency actually re-discovers many of the principles
   behind range encoding, and presents a good theoretical analysis of them.

  End of stream is handled by writing out the smallest number of bits that
   ensures that the stream will be correctly decoded regardless of the value of
   any subsequent bits.
  od_ec_dec_tell() can be used to determine how many bits were needed to decode
   all the symbols thus far; other data can be packed in the remaining bits of
   the input buffer.
  @PHDTHESIS{Pas76,
    author="Richard Clark Pasco",
    title="Source coding algorithms for fast data compression",
    school="Dept. of Electrical Engineering, Stanford University",
    address="Stanford, CA",
    month=May,
    year=1976,
    URL="http://www.richpasco.org/scaffdc.pdf"
  }
  @INPROCEEDINGS{Mar79,
   author="Martin, G.N.N.",
   title="Range encoding: an algorithm for removing redundancy from a digitised
    message",
   booktitle="Video & Data Recording Conference",
   year=1979,
   address="Southampton",
   month=Jul,
   URL="http://www.compressconsult.com/rangecoder/rngcod.pdf.gz"
  }
  @ARTICLE{MNW98,
   author="Alistair Moffat and Radford Neal and Ian H. Witten",
   title="Arithmetic Coding Revisited",
   journal="{ACM} Transactions on Information Systems",
   year=1998,
   volume=16,
   number=3,
   pages="256--294",
   month=Jul,
   URL="http://researchcommons.waikato.ac.nz/bitstream/handle/10289/78/content.pdf"
  }*/

/*This is meant to be a large, positive constant that can still be efficiently
   loaded as an immediate (on platforms like ARM, for example).
  Even relatively modest values like 100 would work fine.*/
#define OD_EC_LOTS_OF_BITS (0x4000)

static void od_ec_dec_refill(od_ec_dec *dec) {
  int s;
  od_ec_window dif;
  int16_t cnt;
  const unsigned char *bptr;
  const unsigned char *end;
  dif = dec->dif;
  cnt = dec->cnt;
  bptr = dec->bptr;
  end = dec->end;
  s = OD_EC_WINDOW_SIZE - 9 - (cnt + 15);
  for (; s >= 0 && bptr < end; s -= 8, bptr++) {
    assert(s <= OD_EC_WINDOW_SIZE - 8);
    dif ^= (od_ec_window)bptr[0] << s;
    cnt += 8;
  }
  if (bptr >= end) {
    dec->tell_offs += OD_EC_LOTS_OF_BITS - cnt;
    cnt = OD_EC_LOTS_OF_BITS;
  }
  dec->dif = dif;
  dec->cnt = cnt;
  dec->bptr = bptr;
}

/*Takes updated dif and range values, renormalizes them so that
   32768 <= rng < 65536 (reading more bytes from the stream into dif if
   necessary), and stores them back in the decoder context.
  dif: The new value of dif.
  rng: The new value of the range.
  ret: The value to return.
  Return: ret.
          This allows the compiler to jump to this function via a tail-call.*/
static int od_ec_dec_normalize(od_ec_dec *dec, od_ec_window dif, unsigned rng,
                               int ret) {
  int d;
  assert(rng <= 65535U);
  d = 16 - OD_ILOG_NZ(rng);
  dec->cnt -= d;
  /*This is equivalent to shifting in 1's instead of 0's.*/
  dec->dif = ((dif + 1) << d) - 1;
  dec->rng = rng << d;
  if (dec->cnt < 0) od_ec_dec_refill(dec);
  return ret;
}

/*Initializes the decoder.
  buf: The input buffer to use.
  Return: 0 on success, or a negative value on error.*/
static void od_ec_dec_init(od_ec_dec *dec, const unsigned char *buf,
                    uint32_t storage) {
  dec->buf = buf;
  dec->tell_offs = 10 - (OD_EC_WINDOW_SIZE - 8);
  dec->end = buf + storage;
  dec->bptr = buf;
  dec->dif = ((od_ec_window)1 << (OD_EC_WINDOW_SIZE - 1)) - 1;
  dec->rng = 0x8000;
  dec->cnt = -15;
  dec->error = 0;
  od_ec_dec_refill(dec);
}

/*Decode a single binary value.
  f: The probability that the bit is one, scaled by 32768.
  Return: The value decoded (0 or 1).*/
static int od_ec_decode_bool_q15(od_ec_dec *dec, unsigned f) {
  od_ec_window dif;
  od_ec_window vw;
  unsigned r;
  unsigned r_new;
  unsigned v;
  int ret;
  assert(0 < f);
  assert(f < 32768U);
  dif = dec->dif;
  r = dec->rng;
  assert(dif >> (OD_EC_WINDOW_SIZE - 16) < r);
  assert(32768U <= r);
  v = ((r >> 8) * (uint32_t)(f >> EC_PROB_SHIFT) >> (7 - EC_PROB_SHIFT));
  v += EC_MIN_PROB;
  vw = (od_ec_window)v << (OD_EC_WINDOW_SIZE - 16);
  ret = 1;
  r_new = v;
  if (dif >= vw) {
    r_new = r - v;
    dif -= vw;
    ret = 0;
  }
  return od_ec_dec_normalize(dec, dif, r_new, ret);
}

/*Decodes a symbol given an inverse cumulative distribution function (CDF)
   table in Q15.
  icdf: CDF_PROB_TOP minus the CDF, such that symbol s falls in the range
         [s > 0 ? (CDF_PROB_TOP - icdf[s - 1]) : 0, CDF_PROB_TOP - icdf[s]).
        The values must be monotonically non-increasing, and icdf[nsyms - 1]
         must be 0.
  nsyms: The number of symbols in the alphabet.
         This should be at most 16.
  Return: The decoded symbol s.*/
static int od_ec_decode_cdf_q15(od_ec_dec *dec, const uint16_t *icdf, int nsyms) {
  od_ec_window dif;
  unsigned r;
  unsigned c;
  unsigned u;
  unsigned v;
  int ret;
  (void)nsyms;
  dif = dec->dif;
  r = dec->rng;
  const int N = nsyms - 1;

  assert(dif >> (OD_EC_WINDOW_SIZE - 16) < r);
  assert(icdf[nsyms - 1] == OD_ICDF(CDF_PROB_TOP));
  assert(32768U <= r);
  assert(7 - EC_PROB_SHIFT - CDF_SHIFT >= 0);
  c = (unsigned)(dif >> (OD_EC_WINDOW_SIZE - 16));
  v = r;
  ret = -1;
  do {
    u = v;
    v = ((r >> 8) * (uint32_t)(icdf[++ret] >> EC_PROB_SHIFT) >>
         (7 - EC_PROB_SHIFT - CDF_SHIFT));
    v += EC_MIN_PROB * (N - ret);
  } while (c < v);
  assert(v < u);
  assert(u <= r);
  r = u - v;
  dif -= (od_ec_window)v << (OD_EC_WINDOW_SIZE - 16);
  return od_ec_dec_normalize(dec, dif, r, ret);
}

void msac_init(MsacContext *const c,
               const uint8_t *const data, const size_t sz)
{
    od_ec_dec_init(c, data, sz);
}

unsigned msac_decode_symbol(MsacContext *const c, const uint16_t *const cdf,
                            const unsigned n_symbols)
{
    return od_ec_decode_cdf_q15(c, cdf, n_symbols);
}

unsigned msac_decode_bool(MsacContext *const c, const unsigned cdf) {
    return od_ec_decode_bool_q15(c, cdf);
}

unsigned msac_decode_bools(MsacContext *const c, const unsigned l) {
    int v = 0;
    for (int n = (int) l - 1; n >= 0; n--)
        v = (v << 1) | msac_decode_bool(c, 128 << 7);
    return v;
}

int msac_decode_subexp(MsacContext *const c, const int ref,
                       const unsigned n, const unsigned k)
{
    int i = 0;
    int a = 0;
    int b = k;
    while ((2U << b) < n) {
        if (!msac_decode_bool(c, 128 << 7)) break;
        b = k + i++;
        a = (1 << b);
    }
    const unsigned v = msac_decode_bools(c, b) + a;
    return ref * 2U <= n ? inv_recenter(ref, v) :
                           n - 1 - inv_recenter(n - 1 - ref, v);
}

int msac_decode_uniform(MsacContext *const c, const unsigned n) {
    assert(n > 0);
    const int l = ulog2(n) + 1;
    assert(l > 1);
    const unsigned m = (1U << l) - n;
    const unsigned v = msac_decode_bools(c, l - 1);
    return v < m ? v : (v << 1) - m + msac_decode_bool(c, 128 << 7);
}

void update_cdf(uint16_t *cdf, unsigned val, unsigned nsymbs) {
    int rate;
    unsigned i, tmp;

    static const int nsymbs2speed[17] = {
        0, 0, 1, 1, 2, 2, 2, 2, 2,
        2, 2, 2, 2, 2, 2, 2, 2
    };
    assert(nsymbs < 17);
    rate = 3 + (cdf[nsymbs] > 15) + (cdf[nsymbs] > 31) + nsymbs2speed[nsymbs];
    tmp = 32768U;

    // Single loop (faster)
    for (i = 0; i < nsymbs - 1; ++i) {
        tmp = (i == val) ? 0 : tmp;
        if (tmp < cdf[i]) {
            cdf[i] -= ((cdf[i] - tmp) >> rate);
        } else {
            cdf[i] += ((tmp - cdf[i]) >> rate);
        }
    }

    cdf[nsymbs] += (cdf[nsymbs] < 32);
}
