/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include "vp9/encoder/vp9_boolhuff.h"
#include "vp9/common/vp9_entropy.h"

#if defined(SECTIONBITS_OUTPUT)
unsigned __int64 Sectionbits[500];

#endif

#ifdef ENTROPY_STATS
unsigned int active_section = 0;
#endif

const unsigned int vp9_prob_cost[256] = {
  2047, 2047, 1791, 1641, 1535, 1452, 1385, 1328, 1279, 1235, 1196, 1161,
  1129, 1099, 1072, 1046, 1023, 1000, 979,  959,  940,  922,  905,  889,
  873,  858,  843,  829,  816,  803,  790,  778,  767,  755,  744,  733,
  723,  713,  703,  693,  684,  675,  666,  657,  649,  641,  633,  625,
  617,  609,  602,  594,  587,  580,  573,  567,  560,  553,  547,  541,
  534,  528,  522,  516,  511,  505,  499,  494,  488,  483,  477,  472,
  467,  462,  457,  452,  447,  442,  437,  433,  428,  424,  419,  415,
  410,  406,  401,  397,  393,  389,  385,  381,  377,  373,  369,  365,
  361,  357,  353,  349,  346,  342,  338,  335,  331,  328,  324,  321,
  317,  314,  311,  307,  304,  301,  297,  294,  291,  288,  285,  281,
  278,  275,  272,  269,  266,  263,  260,  257,  255,  252,  249,  246,
  243,  240,  238,  235,  232,  229,  227,  224,  221,  219,  216,  214,
  211,  208,  206,  203,  201,  198,  196,  194,  191,  189,  186,  184,
  181,  179,  177,  174,  172,  170,  168,  165,  163,  161,  159,  156,
  154,  152,  150,  148,  145,  143,  141,  139,  137,  135,  133,  131,
  129,  127,  125,  123,  121,  119,  117,  115,  113,  111,  109,  107,
  105,  103,  101,  99,   97,   95,   93,   92,   90,   88,   86,   84,
  82,   81,   79,   77,   75,   73,   72,   70,   68,   66,   65,   63,
  61,   60,   58,   56,   55,   53,   51,   50,   48,   46,   45,   43,
  41,   40,   38,   37,   35,   33,   32,   30,   29,   27,   25,   24,
  22,   21,   19,   18,   16,   15,   13,   12,   10,   9,    7,    6,
  4,    3,    1,    1};

void vp9_start_encode(vp9_writer *br, uint8_t *source) {
  br->lowvalue = 0;
  br->range    = 255;
  br->value    = 0;
  br->count    = -24;
  br->buffer   = source;
  br->pos      = 0;
  vp9_write_bit(br, 0);
}

void vp9_stop_encode(vp9_writer *br) {
  int i;

  for (i = 0; i < 32; i++)
    vp9_write_bit(br, 0);

  // Ensure there's no ambigous collision with any index marker bytes
  if ((br->buffer[br->pos - 1] & 0xe0) == 0xc0)
    br->buffer[br->pos++] = 0;
}

