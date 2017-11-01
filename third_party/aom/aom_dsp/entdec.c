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

#ifdef HAVE_CONFIG_H
#include "./config.h"
#endif

#include "aom_dsp/entdec.h"

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
    OD_ASSERT(s <= OD_EC_WINDOW_SIZE - 8);
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
  OD_ASSERT(rng <= 65535U);
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
void od_ec_dec_init(od_ec_dec *dec, const unsigned char *buf,
                    uint32_t storage) {
  dec->buf = buf;
  dec->eptr = buf + storage;
  dec->end_window = 0;
  dec->nend_bits = 0;
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
int od_ec_decode_bool_q15(od_ec_dec *dec, unsigned f) {
  od_ec_window dif;
  od_ec_window vw;
  unsigned r;
  unsigned r_new;
  unsigned v;
  int ret;
  OD_ASSERT(0 < f);
  OD_ASSERT(f < 32768U);
  dif = dec->dif;
  r = dec->rng;
  OD_ASSERT(dif >> (OD_EC_WINDOW_SIZE - 16) < r);
  OD_ASSERT(32768U <= r);
  v = (r >> 8) * (uint32_t)f >> 7;
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
  icdf: 32768 minus the CDF, such that symbol s falls in the range
         [s > 0 ? (32768 - icdf[s - 1]) : 0, 32768 - icdf[s]).
        The values must be monotonically non-increasing, and icdf[nsyms - 1]
         must be 0.
  nsyms: The number of symbols in the alphabet.
         This should be at most 16.
  Return: The decoded symbol s.*/
int od_ec_decode_cdf_q15(od_ec_dec *dec, const uint16_t *icdf, int nsyms) {
  od_ec_window dif;
  unsigned r;
  unsigned c;
  unsigned u;
  unsigned v;
  int ret;
  (void)nsyms;
  dif = dec->dif;
  r = dec->rng;
  OD_ASSERT(dif >> (OD_EC_WINDOW_SIZE - 16) < r);
  OD_ASSERT(icdf[nsyms - 1] == OD_ICDF(32768U));
  OD_ASSERT(32768U <= r);
  c = (unsigned)(dif >> (OD_EC_WINDOW_SIZE - 16));
  v = r;
  ret = -1;
  do {
    u = v;
    v = (r >> 8) * (uint32_t)icdf[++ret] >> 7;
  } while (c < v);
  OD_ASSERT(v < u);
  OD_ASSERT(u <= r);
  r = u - v;
  dif -= (od_ec_window)v << (OD_EC_WINDOW_SIZE - 16);
  return od_ec_dec_normalize(dec, dif, r, ret);
}

#if CONFIG_RAWBITS
/*Extracts a sequence of raw bits from the stream.
  The bits must have been encoded with od_ec_enc_bits().
  ftb: The number of bits to extract.
       This must be between 0 and 25, inclusive.
  Return: The decoded bits.*/
uint32_t od_ec_dec_bits_(od_ec_dec *dec, unsigned ftb) {
  od_ec_window window;
  int available;
  uint32_t ret;
  OD_ASSERT(ftb <= 25);
  window = dec->end_window;
  available = dec->nend_bits;
  if ((unsigned)available < ftb) {
    const unsigned char *buf;
    const unsigned char *eptr;
    buf = dec->buf;
    eptr = dec->eptr;
    OD_ASSERT(available <= OD_EC_WINDOW_SIZE - 8);
    do {
      if (eptr <= buf) {
        dec->tell_offs += OD_EC_LOTS_OF_BITS - available;
        available = OD_EC_LOTS_OF_BITS;
        break;
      }
      window |= (od_ec_window) * --eptr << available;
      available += 8;
    } while (available <= OD_EC_WINDOW_SIZE - 8);
    dec->eptr = eptr;
  }
  ret = (uint32_t)window & (((uint32_t)1 << ftb) - 1);
  window >>= ftb;
  available -= ftb;
  dec->end_window = window;
  dec->nend_bits = available;
  return ret;
}
#endif

/*Returns the number of bits "used" by the decoded symbols so far.
  This same number can be computed in either the encoder or the decoder, and is
   suitable for making coding decisions.
  Return: The number of bits.
          This will always be slightly larger than the exact value (e.g., all
           rounding error is in the positive direction).*/
int od_ec_dec_tell(const od_ec_dec *dec) {
  return (int)(((dec->end - dec->eptr) + (dec->bptr - dec->buf)) * 8 -
               dec->cnt - dec->nend_bits + dec->tell_offs);
}

/*Returns the number of bits "used" by the decoded symbols so far.
  This same number can be computed in either the encoder or the decoder, and is
   suitable for making coding decisions.
  Return: The number of bits scaled by 2**OD_BITRES.
          This will always be slightly larger than the exact value (e.g., all
           rounding error is in the positive direction).*/
uint32_t od_ec_dec_tell_frac(const od_ec_dec *dec) {
  return od_ec_tell_frac(od_ec_dec_tell(dec), dec->rng);
}
