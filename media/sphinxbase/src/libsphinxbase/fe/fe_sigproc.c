/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 1996-2004 Carnegie Mellon University.  All rights 
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * This work was supported in part by funding from the Defense Advanced 
 * Research Projects Agency and the National Science Foundation of the 
 * United States of America, and the CMU Sphinx Speech Consortium.
 *
 * THIS SOFTWARE IS PROVIDED BY CARNEGIE MELLON UNIVERSITY ``AS IS'' AND 
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
 * NOR ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ====================================================================
 *
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef _MSC_VER
#pragma warning (disable: 4244)
#endif

/**
 * Windows math.h does not contain M_PI
 */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "sphinxbase/prim_type.h"
#include "sphinxbase/ckd_alloc.h"
#include "sphinxbase/byteorder.h"
#include "sphinxbase/fixpoint.h"
#include "sphinxbase/fe.h"
#include "sphinxbase/genrand.h"
#include "sphinxbase/err.h"

#include "fe_internal.h"
#include "fe_warp.h"

/* Use extra precision for cosines, Hamming window, pre-emphasis
 * coefficient, twiddle factors. */
#ifdef FIXED_POINT
#define FLOAT2COS(x) FLOAT2FIX_ANY(x,30)
#define COSMUL(x,y) FIXMUL_ANY(x,y,30)
#else
#define FLOAT2COS(x) (x)
#define COSMUL(x,y) ((x)*(y))
#endif

#ifdef FIXED_POINT

/* Internal log-addition table for natural log with radix point at 8
 * bits.  Each entry is 256 * log(1 + e^{-n/256}).  This is used in the
 * log-add computation:
 *
 * e^z = e^x + e^y
 * e^z = e^x(1 + e^{y-x})     = e^y(1 + e^{x-y})
 * z   = x + log(1 + e^{y-x}) = y + log(1 + e^{x-y})
 *
 * So when y > x, z = y + logadd_table[-(x-y)]
 *    when x > y, z = x + logadd_table[-(y-x)]
 */
static const unsigned char fe_logadd_table[] = {
    177, 177, 176, 176, 175, 175, 174, 174, 173, 173,
    172, 172, 172, 171, 171, 170, 170, 169, 169, 168,
    168, 167, 167, 166, 166, 165, 165, 164, 164, 163,
    163, 162, 162, 161, 161, 161, 160, 160, 159, 159,
    158, 158, 157, 157, 156, 156, 155, 155, 155, 154,
    154, 153, 153, 152, 152, 151, 151, 151, 150, 150,
    149, 149, 148, 148, 147, 147, 147, 146, 146, 145,
    145, 144, 144, 144, 143, 143, 142, 142, 141, 141,
    141, 140, 140, 139, 139, 138, 138, 138, 137, 137,
    136, 136, 136, 135, 135, 134, 134, 134, 133, 133,
    132, 132, 131, 131, 131, 130, 130, 129, 129, 129,
    128, 128, 128, 127, 127, 126, 126, 126, 125, 125,
    124, 124, 124, 123, 123, 123, 122, 122, 121, 121,
    121, 120, 120, 119, 119, 119, 118, 118, 118, 117,
    117, 117, 116, 116, 115, 115, 115, 114, 114, 114,
    113, 113, 113, 112, 112, 112, 111, 111, 110, 110,
    110, 109, 109, 109, 108, 108, 108, 107, 107, 107,
    106, 106, 106, 105, 105, 105, 104, 104, 104, 103,
    103, 103, 102, 102, 102, 101, 101, 101, 100, 100,
    100, 99, 99, 99, 98, 98, 98, 97, 97, 97,
    96, 96, 96, 96, 95, 95, 95, 94, 94, 94,
    93, 93, 93, 92, 92, 92, 92, 91, 91, 91,
    90, 90, 90, 89, 89, 89, 89, 88, 88, 88,
    87, 87, 87, 87, 86, 86, 86, 85, 85, 85,
    85, 84, 84, 84, 83, 83, 83, 83, 82, 82,
    82, 82, 81, 81, 81, 80, 80, 80, 80, 79,
    79, 79, 79, 78, 78, 78, 78, 77, 77, 77,
    77, 76, 76, 76, 75, 75, 75, 75, 74, 74,
    74, 74, 73, 73, 73, 73, 72, 72, 72, 72,
    71, 71, 71, 71, 71, 70, 70, 70, 70, 69,
    69, 69, 69, 68, 68, 68, 68, 67, 67, 67,
    67, 67, 66, 66, 66, 66, 65, 65, 65, 65,
    64, 64, 64, 64, 64, 63, 63, 63, 63, 63,
    62, 62, 62, 62, 61, 61, 61, 61, 61, 60,
    60, 60, 60, 60, 59, 59, 59, 59, 59, 58,
    58, 58, 58, 58, 57, 57, 57, 57, 57, 56,
    56, 56, 56, 56, 55, 55, 55, 55, 55, 54,
    54, 54, 54, 54, 53, 53, 53, 53, 53, 52,
    52, 52, 52, 52, 52, 51, 51, 51, 51, 51,
    50, 50, 50, 50, 50, 50, 49, 49, 49, 49,
    49, 49, 48, 48, 48, 48, 48, 48, 47, 47,
    47, 47, 47, 47, 46, 46, 46, 46, 46, 46,
    45, 45, 45, 45, 45, 45, 44, 44, 44, 44,
    44, 44, 43, 43, 43, 43, 43, 43, 43, 42,
    42, 42, 42, 42, 42, 41, 41, 41, 41, 41,
    41, 41, 40, 40, 40, 40, 40, 40, 40, 39,
    39, 39, 39, 39, 39, 39, 38, 38, 38, 38,
    38, 38, 38, 37, 37, 37, 37, 37, 37, 37,
    37, 36, 36, 36, 36, 36, 36, 36, 35, 35,
    35, 35, 35, 35, 35, 35, 34, 34, 34, 34,
    34, 34, 34, 34, 33, 33, 33, 33, 33, 33,
    33, 33, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 31, 31, 31, 31, 31, 31, 31, 31, 31,
    30, 30, 30, 30, 30, 30, 30, 30, 30, 29,
    29, 29, 29, 29, 29, 29, 29, 29, 28, 28,
    28, 28, 28, 28, 28, 28, 28, 28, 27, 27,
    27, 27, 27, 27, 27, 27, 27, 27, 26, 26,
    26, 26, 26, 26, 26, 26, 26, 26, 25, 25,
    25, 25, 25, 25, 25, 25, 25, 25, 25, 24,
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 22, 22, 22, 22, 22, 22, 22, 22,
    22, 22, 22, 22, 21, 21, 21, 21, 21, 21,
    21, 21, 21, 21, 21, 21, 21, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
    19, 19, 19, 19, 18, 18, 18, 18, 18, 18,
    18, 18, 18, 18, 18, 18, 18, 18, 18, 17,
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
    17, 17, 17, 17, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 11,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 0
};

static const int fe_logadd_table_size =
    sizeof(fe_logadd_table) / sizeof(fe_logadd_table[0]);

fixed32
fe_log_add(fixed32 x, fixed32 y)
{
    fixed32 d, r;

    if (x > y) {
        d = (x - y) >> (DEFAULT_RADIX - 8);
        r = x;
    }
    else {
        d = (y - x) >> (DEFAULT_RADIX - 8);
        r = y;
    }

    if (r <= MIN_FIXLOG)
	return MIN_FIXLOG;
    else if (d > fe_logadd_table_size - 1)
        return r;
    else {
        r += ((fixed32) fe_logadd_table[d] << (DEFAULT_RADIX - 8));
/*        printf("%d - %d = %d | %f - %f = %f | %f - %f = %f\n",
               x, y, r, FIX2FLOAT(x), FIX2FLOAT(y), FIX2FLOAT(r),
               exp(FIX2FLOAT(x)), exp(FIX2FLOAT(y)), exp(FIX2FLOAT(r)));
*/
        return r;
    }
}

/*
 * log_sub for spectral subtraction, similar to logadd but we had
 * to smooth function around zero with fixlog in order to improve
 * table interpolation properties
 *
 * The table is created with the file included into distribution
 *
 * e^z = e^x - e^y
 * e^z = e^x (1 - e^(-(x - y)))
 * z = x + log(1 - e^(-(x - y)))
 * z = x + fixlog(a) + (log(1 - e^(- a)) - log(a))
 *
 * Input radix is 8 output radix is 10
 */
static const uint16 fe_logsub_table[] = {
1, 3, 5, 7, 9, 11, 13, 15, 17, 19,
21, 23, 25, 27, 29, 31, 33, 35, 37, 39,
41, 43, 45, 47, 49, 51, 53, 55, 56, 58,
60, 62, 64, 66, 68, 70, 72, 74, 76, 78,
80, 82, 84, 86, 88, 90, 92, 94, 95, 97,
99, 101, 103, 105, 107, 109, 111, 113, 115, 117,
119, 121, 122, 124, 126, 128, 130, 132, 134, 136,
138, 140, 142, 143, 145, 147, 149, 151, 153, 155,
157, 159, 161, 162, 164, 166, 168, 170, 172, 174,
176, 178, 179, 181, 183, 185, 187, 189, 191, 193,
194, 196, 198, 200, 202, 204, 206, 207, 209, 211,
213, 215, 217, 219, 220, 222, 224, 226, 228, 230,
232, 233, 235, 237, 239, 241, 243, 244, 246, 248,
250, 252, 254, 255, 257, 259, 261, 263, 265, 266,
268, 270, 272, 274, 275, 277, 279, 281, 283, 284,
286, 288, 290, 292, 294, 295, 297, 299, 301, 302,
304, 306, 308, 310, 311, 313, 315, 317, 319, 320,
322, 324, 326, 327, 329, 331, 333, 335, 336, 338,
340, 342, 343, 345, 347, 349, 350, 352, 354, 356,
357, 359, 361, 363, 364, 366, 368, 370, 371, 373,
375, 377, 378, 380, 382, 384, 385, 387, 389, 391,
392, 394, 396, 397, 399, 401, 403, 404, 406, 408,
410, 411, 413, 415, 416, 418, 420, 422, 423, 425,
427, 428, 430, 432, 433, 435, 437, 439, 440, 442,
444, 445, 447, 449, 450, 452, 454, 455, 457, 459,
460, 462, 464, 465, 467, 469, 471, 472, 474, 476,
477, 479, 481, 482, 484, 486, 487, 489, 490, 492,
494, 495, 497, 499, 500, 502, 504, 505, 507, 509,
510, 512, 514, 515, 517, 518, 520, 522, 523, 525,
527, 528, 530, 532, 533, 535, 536, 538, 540, 541,
543, 544, 546, 548, 549, 551, 553, 554, 556, 557,
559, 561, 562, 564, 565, 567, 569, 570, 572, 573,
575, 577, 578, 580, 581, 583, 585, 586, 588, 589,
591, 592, 594, 596, 597, 599, 600, 602, 603, 605,
607, 608, 610, 611, 613, 614, 616, 618, 619, 621,
622, 624, 625, 627, 628, 630, 632, 633, 635, 636,
638, 639, 641, 642, 644, 645, 647, 649, 650, 652,
653, 655, 656, 658, 659, 661, 662, 664, 665, 667,
668, 670, 671, 673, 674, 676, 678, 679, 681, 682,
684, 685, 687, 688, 690, 691, 693, 694, 696, 697,
699, 700, 702, 703, 705, 706, 708, 709, 711, 712,
714, 715, 717, 718, 719, 721, 722, 724, 725, 727,
728, 730, 731, 733, 734, 736, 737, 739, 740, 742,
743, 745, 746, 747, 749, 750, 752, 753, 755, 756,
758, 759, 761, 762, 763, 765, 766, 768, 769, 771,
772, 774, 775, 776, 778, 779, 781, 782, 784, 785,
786, 788, 789, 791, 792, 794, 795, 796, 798, 799,
801, 802, 804, 805, 806, 808, 809, 811, 812, 813,
815, 816, 818, 819, 820, 822, 823, 825, 826, 827,
829, 830, 832, 833, 834, 836, 837, 839, 840, 841,
843, 844, 846, 847, 848, 850, 851, 852, 854, 855,
857, 858, 859, 861, 862, 863, 865, 866, 868, 869,
870, 872, 873, 874, 876, 877, 878, 880, 881, 883,
884, 885, 887, 888, 889, 891, 892, 893, 895, 896,
897, 899, 900, 901, 903, 904, 905, 907, 908, 909,
911, 912, 913, 915, 916, 917, 919, 920, 921, 923,
924, 925, 927, 928, 929, 931, 932, 933, 935, 936,
937, 939, 940, 941, 942, 944, 945, 946, 948, 949,
950, 952, 953, 954, 956, 957, 958, 959, 961, 962,
963, 965, 966, 967, 968, 970, 971, 972, 974, 975,
976, 977, 979, 980, 981, 983, 984, 985, 986, 988,
989, 990, 991, 993, 994, 995, 997, 998, 999, 1000,
1002, 1003, 1004, 1005, 1007, 1008, 1009, 1010, 1012, 1013,
1014, 1015, 1017, 1018, 1019, 1020, 1022, 1023, 1024, 1025,
1027, 1028, 1029, 1030, 1032, 1033, 1034, 1035, 1037, 1038,
1039, 1040, 1041, 1043, 1044, 1045, 1046, 1048, 1049, 1050,
1051, 1052, 1054, 1055, 1056, 1057, 1059, 1060, 1061, 1062,
1063, 1065, 1066, 1067, 1068, 1069, 1071, 1072, 1073, 1074,
1076, 1077, 1078, 1079, 1080, 1082, 1083, 1084, 1085, 1086,
1087, 1089, 1090, 1091, 1092, 1093, 1095, 1096, 1097, 1098,
1099, 1101, 1102, 1103, 1104, 1105, 1106, 1108, 1109, 1110,
1111, 1112, 1114, 1115, 1116, 1117, 1118, 1119, 1121, 1122,
1123, 1124, 1125, 1126, 1128, 1129, 1130, 1131, 1132, 1133,
1135, 1136, 1137, 1138, 1139, 1140, 1141, 1143, 1144, 1145,
1146, 1147, 1148, 1149, 1151, 1152, 1153, 1154, 1155, 1156,
1157, 1159, 1160, 1161, 1162, 1163, 1164, 1165, 1167, 1168,
1169, 1170, 1171, 1172, 1173, 1174, 1176, 1177, 1178, 1179,
1180, 1181, 1182, 1183, 1185, 1186, 1187, 1188, 1189, 1190,
1191, 1192, 1193, 1195, 1196, 1197, 1198, 1199, 1200, 1201,
1202, 1203, 1205, 1206, 1207, 1208, 1209, 1210, 1211, 1212,
1213, 1214, 1216, 1217, 1218, 1219, 1220, 1221, 1222, 1223,
1224, 1225, 1226, 1228, 1229, 1230, 1231, 1232, 1233, 1234,
1235, 1236, 1237, 1238, 1239, 1240, 1242, 1243, 1244, 1245,
1246, 1247, 1248, 1249, 1250, 1251, 1252, 1253, 1254, 1255,
1256, 1258, 1259, 1260, 1261, 1262, 1263, 1264, 1265, 1266,
1267, 1268, 1269, 1270, 1271, 1272, 1273, 1274, 1275, 1277,
1278, 1279, 1280, 1281, 1282, 1283, 1284, 1285, 1286, 1287,
1288, 1289, 1290, 1291, 1292, 1293, 1294, 1295, 1296, 1297,
1298, 1299, 1300, 1301, 1302, 1303, 1305, 1306, 1307, 1308,
1309, 1310, 1311, 1312, 1313, 1314, 1315, 1316, 1317, 1318,
1319, 1320, 1321, 1322, 1323, 1324, 1325, 1326, 1327, 1328,
1329, 1330, 1331, 1332, 1333, 1334, 1335, 1336, 1337, 1338,
1339, 1340, 1341, 1342, 1343, 1344, 1345, 1346, 1347, 1348,
1349, 1350, 1351, 1352, 1353, 1354, 1355, 1356, 1357, 1358,
1359, 1360, 1361, 1362, 1363, 1364, 1365, 1366, 1367, 1368,
1369, 1370, 1371, 1372, 1372, 1373, 1374, 1375, 1376, 1377,
1378, 1379, 1380, 1381, 1382, 1383, 1384, 1385, 1386, 1387,
1388, 1389, 1390, 1391, 1392, 1393, 1394, 1395, 1396, 1397,
1398, 1399, 1399, 1400, 1401, 1402, 1403, 1404, 1405, 1406,
1407, 1408, 1409, 1410, 1411, 1412, 1413, 1414, 1415, 1416,
1417, 1418, 1418, 1419, 1420, 1421, 1422, 1423, 1424, 1425,
1426, 1427, 1428, 1429, 1430, 1431, 1432, 1432, 1433, 1434,
1435, 1436, 1437, 1438, 1439, 1440, 1441, 1442, 1443, 1444,
1444, 1445, 1446, 1447, 1448, 1449, 1450, 1451, 1452, 1453,
1454, 1455, 1455, 1456, 1457, 1458, 1459, 1460, 1461, 1462,
1463, 1464, 1465, 1466, 1466, 1467, 1468, 1469, 1470, 1471,
1472, 1473, 1474, 1475, 1475, 1476, 1477, 1478, 1479, 1480,
1481, 1482, 1483, 1483, 1484, 1485, 1486, 1487, 1488, 1489,
1490, 1491, 1491, 1492, 1493, 1494, 1495, 1496, 1497, 1498,
1499, 1499, 1500, 1501, 1502, 1503, 1504, 1505, 1506, 1506,
1507, 1508, 1509, 1510, 1511, 1512, 1513, 1513, 1514, 1515,
1516, 1517, 1518, 1519, 1520, 1520, 1521, 1522, 1523, 1524,
1525, 1526, 1526, 1527, 1528, 1529, 1530, 1531, 1532, 1532,
1533, 1534, 1535, 1536, 1537, 1538, 1538, 1539, 1540, 1541,
1542, 1543, 1544, 1544, 1545, 1546, 1547, 1548, 1549, 1550,
1550, 1551, 1552, 1553, 1554, 1555, 1555, 1556, 1557, 1558,
1559, 1560, 1560, 1561, 1562, 1563, 1564, 1565, 1565, 1566,
1567, 1568, 1569, 1570, 1570, 1571, 1572, 1573, 1574, 1575,
1575, 1576, 1577, 1578, 1579, 1580, 1580, 1581, 1582, 1583,
1584, 1584, 1585, 1586, 1587, 1588, 1589, 1589, 1590, 1591,
1592, 1593, 1593, 1594, 1595, 1596, 1597, 1598, 1598, 1599,
1600, 1601, 1602, 1602, 1603, 1604, 1605, 1606, 1606, 1607,
1608, 1609, 1610, 1610, 1611, 1612, 1613, 1614, 1614, 1615,
1616, 1617, 1618, 1618, 1619, 1620, 1621, 1622, 1622, 1623,
1624, 1625, 1626, 1626, 1627, 1628, 1629, 1630, 1630, 1631,
1632, 1633, 1634, 1634, 1635, 1636, 1637, 1637, 1638, 1639,
1640, 1641, 1641, 1642, 1643, 1644, 1645, 1645, 1646, 1647,
1648, 1648, 1649, 1650, 1651, 1652, 1652, 1653, 1654, 1655,
1655, 1656, 1657, 1658, 1658, 1659, 1660, 1661, 1662, 1662,
1663, 1664, 1665, 1665, 1666, 1667, 1668, 1668, 1669, 1670,
1671, 1671, 1672, 1673, 1674, 1675, 1675, 1676, 1677, 1678,
1678, 1679, 1680, 1681, 1681, 1682, 1683, 1684, 1684, 1685,
1686, 1687, 1687, 1688, 1689, 1690, 1690, 1691, 1692, 1693,
1693, 1694, 1695, 1696, 1696, 1697, 1698, 1699, 1699, 1700,
1701, 1702, 1702, 1703, 1704, 1705, 1705, 1706, 1707, 1707,
1708, 1709, 1710, 1710, 1711, 1712, 1713, 1713, 1714, 1715,
1716, 1716, 1717, 1718, 1718, 1719, 1720, 1721, 1721, 1722,
1723, 1724, 1724, 1725, 1726, 1727, 1727, 1728, 1729, 1729,
1730, 1731, 1732, 1732, 1733, 1734, 1734, 1735, 1736, 1737,
1737, 1738, 1739, 1740, 1740, 1741, 1742, 1742, 1743, 1744,
1745, 1745, 1746, 1747, 1747, 1748, 1749, 1749, 1750, 1751,
1752, 1752, 1753, 1754, 1754, 1755, 1756, 1757, 1757, 1758,
1759, 1759, 1760, 1761, 1762, 1762, 1763, 1764, 1764, 1765,
1766, 1766, 1767, 1768, 1769, 1769, 1770, 1771, 1771, 1772,
1773, 1773, 1774, 1775, 1776, 1776, 1777, 1778, 1778, 1779,
1780, 1780, 1781, 1782, 1782, 1783, 1784, 1784, 1785, 1786,
1787, 1787, 1788, 1789, 1789, 1790, 1791, 1791, 1792, 1793,
1793, 1794, 1795, 1795, 1796, 1797, 1798, 1798, 1799, 1800,
1800, 1801, 1802, 1802, 1803, 1804, 1804, 1805, 1806, 1806,
1807, 1808, 1808, 1809, 1810, 1810, 1811, 1812, 1812, 1813,
1814, 1814, 1815, 1816, 1816, 1817, 1818, 1818, 1819, 1820,
1820, 1821, 1822, 1822, 1823, 1824, 1824, 1825, 1826, 1826,
1827, 1828, 1828, 1829, 1830, 1830, 1831, 1832, 1832, 1833,
1834, 1834, 1835, 1836, 1836, 1837, 1838, 1838, 1839, 1840,
1840, 1841, 1842, 1842, 1843, 1844, 1844, 1845, 1845, 1846,
1847, 1847, 1848, 1849, 1849, 1850, 1851, 1851, 1852, 1853,
1853, 1854, 1855, 1855, 1856, 1857, 1857, 1858, 1858, 1859,
1860, 1860, 1861, 1862, 1862, 1863, 1864, 1864, 1865, 1866,
1866, 1867, 1867, 1868, 1869, 1869, 1870, 1871, 1871, 1872,
1873, 1873, 1874, 1874, 1875, 1876, 1876, 1877, 1878, 1878,
1879, 1879, 1880, 1881, 1881, 1882, 1883, 1883, 1884, 1885,
1885, 1886, 1886, 1887, 1888, 1888, 1889, 1890, 1890, 1891,
1891, 1892, 1893, 1893, 1894, 1895, 1895, 1896, 1896, 1897,
1898, 1898, 1899, 1900, 1900, 1901, 1901, 1902, 1903, 1903,
1904, 1904, 1905, 1906, 1906, 1907, 1908, 1908, 1909, 1909,
1910, 1911, 1911, 1912, 1912, 1913, 1914, 1914, 1915, 1916,
1916, 1917, 1917, 1918, 1919, 1919, 1920, 1920, 1921, 1922,
1922, 1923, 1923, 1924, 1925, 1925, 1926, 1926, 1927, 1928,
1928, 1929, 1929, 1930, 1931, 1931, 1932, 1932, 1933, 1934,
1934, 1935, 1935, 1936, 1937, 1937, 1938, 1938, 1939, 1940,
1940, 1941, 1941, 1942, 1943, 1943, 1944, 1944, 1945, 1946,
1946, 1947, 1947, 1948, 1949, 1949, 1950, 1950, 1951, 1952,
1952, 1953, 1953, 1954, 1955, 1955, 1956, 1956, 1957, 1957,
1958, 1959, 1959, 1960, 1960, 1961, 1962, 1962, 1963, 1963,
1964, 1964, 1965, 1966, 1966, 1967, 1967, 1968, 1969, 1969,
1970, 1970, 1971, 1971, 1972, 1973, 1973, 1974, 1974, 1975,
1976, 1976, 1977, 1977, 1978, 1978, 1979, 1980, 1980, 1981,
1981, 1982, 1982, 1983, 1984, 1984, 1985, 1985, 1986, 1986,
1987, 1988, 1988, 1989, 1989, 1990, 1990, 1991, 1992, 1992,
1993, 1993, 1994, 1994, 1995, 1996, 1996, 1997, 1997, 1998,
1998, 1999, 1999, 2000, 2001, 2001, 2002, 2002, 2003, 2003,
2004, 2005, 2005, 2006, 2006, 2007, 2007, 2008, 2008, 2009,
2010, 2010, 2011, 2011, 2012, 2012, 2013, 2014, 2014, 2015,
2015, 2016, 2016, 2017, 2017, 2018, 2019, 2019, 2020, 2020,
2021, 2021, 2022, 2022, 2023, 2023, 2024, 2025, 2025, 2026,
2026, 2027, 2027, 2028, 2028, 2029, 2030, 2030, 2031, 2031,
2032, 2032, 2033, 2033, 2034, 2034, 2035, 2036, 2036, 2037,
2037, 2038, 2038, 2039, 2039, 2040, 2040, 2041, 2042, 2042,
2043, 2043, 2044, 2044, 2045, 2045, 2046, 2046, 2047, 2048,
2048, 2049, 2049, 2050, 2050, 2051, 2051, 2052, 2052, 2053,
2053, 2054, 2054, 2055, 2056, 2056, 2057, 2057, 2058, 2058,
2059, 2059, 2060, 2060, 2061, 2061, 2062, 2062, 2063, 2064,
2064, 2065, 2065, 2066, 2066, 2067, 2067, 2068, 2068, 2069,
2069, 2070, 2070, 2071, 2072, 2072, 2073, 2073, 2074, 2074,
2075, 2075, 2076, 2076, 2077, 2077, 2078, 2078, 2079, 2079,
2080, 2080, 2081
};

static const int fe_logsub_table_size =
    sizeof(fe_logsub_table) / sizeof(fe_logsub_table[0]);

fixed32
fe_log_sub(fixed32 x, fixed32 y)
{
    fixed32 d, r;

    if (x < MIN_FIXLOG || y >= x)
	return MIN_FIXLOG;

    d = (x - y) >> (DEFAULT_RADIX - 8);
    
    if (d > fe_logsub_table_size - 1)
        return x;

    r = fe_logsub_table[d] << (DEFAULT_RADIX - 10);
/*
    printf("diff=%d\n", 
    	    x + FIXLN(x-y) - r -
	    (x + FLOAT2FIX(logf(-expm1f(FIX2FLOAT(y - x))))));
*/
    return x + FIXLN(x-y) - r;
}

static fixed32
fe_log(float32 x)
{
    if (x <= 0) {
        return MIN_FIXLOG;
    }
    else {
        return FLOAT2FIX(log(x));
    }
}
#endif

static float32
fe_mel(melfb_t * mel, float32 x)
{
    float32 warped = fe_warp_unwarped_to_warped(mel, x);

    return (float32) (2595.0 * log10(1.0 + warped / 700.0));
}

static float32
fe_melinv(melfb_t * mel, float32 x)
{
    float32 warped = (float32) (700.0 * (pow(10.0, x / 2595.0) - 1.0));
    return fe_warp_warped_to_unwarped(mel, warped);
}

int32
fe_build_melfilters(melfb_t * mel_fb)
{
    float32 melmin, melmax, melbw, fftfreq;
    int n_coeffs, i, j;


    /* Filter coefficient matrix, in flattened form. */
    mel_fb->spec_start =
        ckd_calloc(mel_fb->num_filters, sizeof(*mel_fb->spec_start));
    mel_fb->filt_start =
        ckd_calloc(mel_fb->num_filters, sizeof(*mel_fb->filt_start));
    mel_fb->filt_width =
        ckd_calloc(mel_fb->num_filters, sizeof(*mel_fb->filt_width));

    /* First calculate the widths of each filter. */
    /* Minimum and maximum frequencies in mel scale. */
    melmin = fe_mel(mel_fb, mel_fb->lower_filt_freq);
    melmax = fe_mel(mel_fb, mel_fb->upper_filt_freq);

    /* Width of filters in mel scale */
    melbw = (melmax - melmin) / (mel_fb->num_filters + 1);
    if (mel_fb->doublewide) {
        melmin -= melbw;
        melmax += melbw;
        if ((fe_melinv(mel_fb, melmin) < 0) ||
            (fe_melinv(mel_fb, melmax) > mel_fb->sampling_rate / 2)) {
            E_WARN
                ("Out of Range: low  filter edge = %f (%f)\n",
                 fe_melinv(mel_fb, melmin), 0.0);
            E_WARN
                ("              high filter edge = %f (%f)\n",
                 fe_melinv(mel_fb, melmax), mel_fb->sampling_rate / 2);
            return FE_INVALID_PARAM_ERROR;
        }
    }

    /* DFT point spacing */
    fftfreq = mel_fb->sampling_rate / (float32) mel_fb->fft_size;

    /* Count and place filter coefficients. */
    n_coeffs = 0;
    for (i = 0; i < mel_fb->num_filters; ++i) {
        float32 freqs[3];

        /* Left, center, right frequencies in Hertz */
        for (j = 0; j < 3; ++j) {
            if (mel_fb->doublewide)
                freqs[j] = fe_melinv(mel_fb, (i + j * 2) * melbw + melmin);
            else
                freqs[j] = fe_melinv(mel_fb, (i + j) * melbw + melmin);
            /* Round them to DFT points if requested */
            if (mel_fb->round_filters)
                freqs[j] = ((int) (freqs[j] / fftfreq + 0.5)) * fftfreq;
        }

        /* spec_start is the start of this filter in the power spectrum. */
        mel_fb->spec_start[i] = -1;
        /* There must be a better way... */
        for (j = 0; j < mel_fb->fft_size / 2 + 1; ++j) {
            float32 hz = j * fftfreq;
            if (hz < freqs[0])
                continue;
            else if (hz > freqs[2] || j == mel_fb->fft_size / 2) {
                /* filt_width is the width in DFT points of this filter. */
                mel_fb->filt_width[i] = j - mel_fb->spec_start[i];
                /* filt_start is the start of this filter in the filt_coeffs array. */
                mel_fb->filt_start[i] = n_coeffs;
                n_coeffs += mel_fb->filt_width[i];
                break;
            }
            if (mel_fb->spec_start[i] == -1)
                mel_fb->spec_start[i] = j;
        }
    }

    /* Now go back and allocate the coefficient array. */
    mel_fb->filt_coeffs =
        ckd_malloc(n_coeffs * sizeof(*mel_fb->filt_coeffs));

    /* And now generate the coefficients. */
    n_coeffs = 0;
    for (i = 0; i < mel_fb->num_filters; ++i) {
        float32 freqs[3];

        /* Left, center, right frequencies in Hertz */
        for (j = 0; j < 3; ++j) {
            if (mel_fb->doublewide)
                freqs[j] = fe_melinv(mel_fb, (i + j * 2) * melbw + melmin);
            else
                freqs[j] = fe_melinv(mel_fb, (i + j) * melbw + melmin);
            /* Round them to DFT points if requested */
            if (mel_fb->round_filters)
                freqs[j] = ((int) (freqs[j] / fftfreq + 0.5)) * fftfreq;
        }

        for (j = 0; j < mel_fb->filt_width[i]; ++j) {
            float32 hz, loslope, hislope;

            hz = (mel_fb->spec_start[i] + j) * fftfreq;
            if (hz < freqs[0] || hz > freqs[2]) {
                E_FATAL
                    ("Failed to create filterbank, frequency range does not match. "
                     "Sample rate %f, FFT size %d, lowerf %f < freq %f > upperf %f.\n",
                     mel_fb->sampling_rate, mel_fb->fft_size, freqs[0], hz,
                     freqs[2]);
            }
            loslope = (hz - freqs[0]) / (freqs[1] - freqs[0]);
            hislope = (freqs[2] - hz) / (freqs[2] - freqs[1]);
            if (mel_fb->unit_area) {
                loslope *= 2 / (freqs[2] - freqs[0]);
                hislope *= 2 / (freqs[2] - freqs[0]);
            }
            if (loslope < hislope) {
#ifdef FIXED_POINT
                mel_fb->filt_coeffs[n_coeffs] = fe_log(loslope);
#else
                mel_fb->filt_coeffs[n_coeffs] = loslope;
#endif
            }
            else {
#ifdef FIXED_POINT
                mel_fb->filt_coeffs[n_coeffs] = fe_log(hislope);
#else
                mel_fb->filt_coeffs[n_coeffs] = hislope;
#endif
            }
            ++n_coeffs;
        }
    }

    return FE_SUCCESS;
}

int32
fe_compute_melcosine(melfb_t * mel_fb)
{

    float64 freqstep;
    int32 i, j;

    mel_fb->mel_cosine =
        (mfcc_t **) ckd_calloc_2d(mel_fb->num_cepstra,
                                  mel_fb->num_filters, sizeof(mfcc_t));

    freqstep = M_PI / mel_fb->num_filters;
    /* NOTE: The first row vector is actually unnecessary but we leave
     * it in to avoid confusion. */
    for (i = 0; i < mel_fb->num_cepstra; i++) {
        for (j = 0; j < mel_fb->num_filters; j++) {
            float64 cosine;

            cosine = cos(freqstep * i * (j + 0.5));
            mel_fb->mel_cosine[i][j] = FLOAT2COS(cosine);
        }
    }

    /* Also precompute normalization constants for unitary DCT. */
    mel_fb->sqrt_inv_n = FLOAT2COS(sqrt(1.0 / mel_fb->num_filters));
    mel_fb->sqrt_inv_2n = FLOAT2COS(sqrt(2.0 / mel_fb->num_filters));

    /* And liftering weights */
    if (mel_fb->lifter_val) {
        mel_fb->lifter =
            calloc(mel_fb->num_cepstra, sizeof(*mel_fb->lifter));
        for (i = 0; i < mel_fb->num_cepstra; ++i) {
            mel_fb->lifter[i] = FLOAT2MFCC(1 + mel_fb->lifter_val / 2
                                           * sin(i * M_PI /
                                                 mel_fb->lifter_val));
        }
    }

    return (0);
}

static void
fe_pre_emphasis(int16 const *in, frame_t * out, int32 len,
                float32 factor, int16 prior)
{
    int i;

#if defined(FIXED16)
    int16 fxd_alpha = (int16) (factor * 0x8000);
    int32 tmp1, tmp2;

    tmp1 = (int32) in[0] << 15;
    tmp2 = (int32) prior *fxd_alpha;
    out[0] = (int16) ((tmp1 - tmp2) >> 15);
    for (i = 1; i < len; ++i) {
        tmp1 = (int32) in[i] << 15;
        tmp2 = (int32) in[i - 1] * fxd_alpha;
        out[i] = (int16) ((tmp1 - tmp2) >> 15);
    }
#elif defined(FIXED_POINT)
    fixed32 fxd_alpha = FLOAT2FIX(factor);
    out[0] = ((fixed32) in[0] << DEFAULT_RADIX) - (prior * fxd_alpha);
    for (i = 1; i < len; ++i)
        out[i] = ((fixed32) in[i] << DEFAULT_RADIX)
            - (fixed32) in[i - 1] * fxd_alpha;
#else
    out[0] = (frame_t) in[0] - (frame_t) prior *factor;
    for (i = 1; i < len; i++)
        out[i] = (frame_t) in[i] - (frame_t) in[i - 1] * factor;
#endif
}

static void
fe_short_to_frame(int16 const *in, frame_t * out, int32 len)
{
    int i;

#if defined(FIXED16)
    memcpy(out, in, len * sizeof(*out));
#elif defined(FIXED_POINT)
    for (i = 0; i < len; i++)
        out[i] = (int32) in[i] << DEFAULT_RADIX;
#else                           /* FIXED_POINT */
    for (i = 0; i < len; i++)
        out[i] = (frame_t) in[i];
#endif                          /* FIXED_POINT */
}

void
fe_create_hamming(window_t * in, int32 in_len)
{
    int i;

    /* Symmetric, so we only create the first half of it. */
    for (i = 0; i < in_len / 2; i++) {
        float64 hamm;
        hamm = (0.54 - 0.46 * cos(2 * M_PI * i /
                                  ((float64) in_len - 1.0)));
#ifdef FIXED16
        in[i] = (int16) (hamm * 0x8000);
#else
        in[i] = FLOAT2COS(hamm);
#endif
    }
}

static void
fe_hamming_window(frame_t * in, window_t * window, int32 in_len,
                  int32 remove_dc)
{
    int i;

    if (remove_dc) {
#ifdef FIXED16
        int32 mean = 0;         /* Use int32 to avoid possibility of overflow */
#else
        frame_t mean = 0;
#endif

        for (i = 0; i < in_len; i++)
            mean += in[i];
        mean /= in_len;
        for (i = 0; i < in_len; i++)
            in[i] -= (frame_t) mean;
    }

#ifdef FIXED16
    for (i = 0; i < in_len / 2; i++) {
        int32 tmp1, tmp2;

        tmp1 = (int32) in[i] * window[i];
        tmp2 = (int32) in[in_len - 1 - i] * window[i];
        in[i] = (int16) (tmp1 >> 15);
        in[in_len - 1 - i] = (int16) (tmp2 >> 15);
    }
#else
    for (i = 0; i < in_len / 2; i++) {
        in[i] = COSMUL(in[i], window[i]);
        in[in_len - 1 - i] = COSMUL(in[in_len - 1 - i], window[i]);
    }
#endif
}

static int
fe_spch_to_frame(fe_t * fe, int len)
{
    /* Copy to the frame buffer. */
    if (fe->pre_emphasis_alpha != 0.0) {
        fe_pre_emphasis(fe->spch, fe->frame, len,
                        fe->pre_emphasis_alpha, fe->prior);
        if (len >= fe->frame_shift)
            fe->prior = fe->spch[fe->frame_shift - 1];
        else
            fe->prior = fe->spch[len - 1];
    }
    else
        fe_short_to_frame(fe->spch, fe->frame, len);

    /* Zero pad up to FFT size. */
    memset(fe->frame + len, 0, (fe->fft_size - len) * sizeof(*fe->frame));

    /* Window. */
    fe_hamming_window(fe->frame, fe->hamming_window, fe->frame_size,
                      fe->remove_dc);

    return len;
}

int
fe_read_frame(fe_t * fe, int16 const *in, int32 len)
{
    int i;

    if (len > fe->frame_size)
        len = fe->frame_size;

    /* Read it into the raw speech buffer. */
    memcpy(fe->spch, in, len * sizeof(*in));
    /* Swap and dither if necessary. */
    if (fe->swap)
        for (i = 0; i < len; ++i)
            SWAP_INT16(&fe->spch[i]);
    if (fe->dither)
        for (i = 0; i < len; ++i)
            fe->spch[i] += (int16) ((!(s3_rand_int31() % 4)) ? 1 : 0);

    return fe_spch_to_frame(fe, len);
}

int
fe_shift_frame(fe_t * fe, int16 const *in, int32 len)
{
    int offset, i;

    if (len > fe->frame_shift)
        len = fe->frame_shift;
    offset = fe->frame_size - fe->frame_shift;

    /* Shift data into the raw speech buffer. */
    memmove(fe->spch, fe->spch + fe->frame_shift,
            offset * sizeof(*fe->spch));
    memcpy(fe->spch + offset, in, len * sizeof(*fe->spch));
    /* Swap and dither if necessary. */
    if (fe->swap)
        for (i = 0; i < len; ++i)
            SWAP_INT16(&fe->spch[offset + i]);
    if (fe->dither)
        for (i = 0; i < len; ++i)
            fe->spch[offset + i]
                += (int16) ((!(s3_rand_int31() % 4)) ? 1 : 0);

    return fe_spch_to_frame(fe, offset + len);
}

/**
 * Create arrays of twiddle factors.
 */
void
fe_create_twiddle(fe_t * fe)
{
    int i;

    for (i = 0; i < fe->fft_size / 4; ++i) {
        float64 a = 2 * M_PI * i / fe->fft_size;
#ifdef FIXED16
        fe->ccc[i] = (int16) (cos(a) * 0x8000);
        fe->sss[i] = (int16) (sin(a) * 0x8000);
#elif defined(FIXED_POINT)
        fe->ccc[i] = FLOAT2COS(cos(a));
        fe->sss[i] = FLOAT2COS(sin(a));
#else
        fe->ccc[i] = cos(a);
        fe->sss[i] = sin(a);
#endif
    }
}


/* Translated from the FORTRAN (obviously) from "Real-Valued Fast
 * Fourier Transform Algorithms" by Henrik V. Sorensen et al., IEEE
 * Transactions on Acoustics, Speech, and Signal Processing, vol. 35,
 * no.6.  The 16-bit version does a version of "block floating
 * point" in order to avoid rounding errors.
 */
#if defined(FIXED16)
static int
fe_fft_real(fe_t * fe)
{
    int i, j, k, m, n, lz;
    frame_t *x, xt, max;

    x = fe->frame;
    m = fe->fft_order;
    n = fe->fft_size;

    /* Bit-reverse the input. */
    j = 0;
    for (i = 0; i < n - 1; ++i) {
        if (i < j) {
            xt = x[j];
            x[j] = x[i];
            x[i] = xt;
        }
        k = n / 2;
        while (k <= j) {
            j -= k;
            k /= 2;
        }
        j += k;
    }
    /* Determine how many bits of dynamic range are in the input. */
    max = 0;
    for (i = 0; i < n; ++i)
        if (abs(x[i]) > max)
            max = abs(x[i]);
    /* The FFT has a gain of M bits, so we need to attenuate the input
     * by M bits minus the number of leading zeroes in the input's
     * range in order to avoid overflows.  */
    for (lz = 0; lz < m; ++lz)
        if (max & (1 << (15 - lz)))
            break;

    /* Basic butterflies (2-point FFT, real twiddle factors):
     * x[i]   = x[i] +  1 * x[i+1]
     * x[i+1] = x[i] + -1 * x[i+1]
     */
    /* The quantization error introduced by attenuating the input at
     * any given stage of the FFT has a cascading effect, so we hold
     * off on it until it's absolutely necessary. */
    for (i = 0; i < n; i += 2) {
        int atten = (lz == 0);
        xt = x[i] >> atten;
        x[i] = xt + (x[i + 1] >> atten);
        x[i + 1] = xt - (x[i + 1] >> atten);
    }

    /* The rest of the butterflies, in stages from 1..m */
    for (k = 1; k < m; ++k) {
        int n1, n2, n4;
        /* Start attenuating once we hit the number of leading zeros. */
        int atten = (k >= lz);

        n4 = k - 1;
        n2 = k;
        n1 = k + 1;
        /* Stride over each (1 << (k+1)) points */
        for (i = 0; i < n; i += (1 << n1)) {
            /* Basic butterfly with real twiddle factors:
             * x[i]          = x[i] +  1 * x[i + (1<<k)]
             * x[i + (1<<k)] = x[i] + -1 * x[i + (1<<k)]
             */
            xt = x[i] >> atten;
            x[i] = xt + (x[i + (1 << n2)] >> atten);
            x[i + (1 << n2)] = xt - (x[i + (1 << n2)] >> atten);

            /* The other ones with real twiddle factors:
             * x[i + (1<<k) + (1<<(k-1))]
             *   = 0 * x[i + (1<<k-1)] + -1 * x[i + (1<<k) + (1<<k-1)]
             * x[i + (1<<(k-1))]
             *   = 1 * x[i + (1<<k-1)] +  0 * x[i + (1<<k) + (1<<k-1)]
             */
            x[i + (1 << n2) + (1 << n4)] =
                -x[i + (1 << n2) + (1 << n4)] >> atten;
            x[i + (1 << n4)] = x[i + (1 << n4)] >> atten;

            /* Butterflies with complex twiddle factors.
             * There are (1<<k-1) of them.
             */
            for (j = 1; j < (1 << n4); ++j) {
                frame_t cc, ss, t1, t2;
                int i1, i2, i3, i4;

                i1 = i + j;
                i2 = i + (1 << n2) - j;
                i3 = i + (1 << n2) + j;
                i4 = i + (1 << n2) + (1 << n2) - j;

                /*
                 * cc = real(W[j * n / (1<<(k+1))])
                 * ss = imag(W[j * n / (1<<(k+1))])
                 */
                cc = fe->ccc[j << (m - n1)];
                ss = fe->sss[j << (m - n1)];

                /* There are some symmetry properties which allow us
                 * to get away with only four multiplications here. */
                {
                    int32 tmp1, tmp2;
                    tmp1 = (int32) x[i3] * cc + (int32) x[i4] * ss;
                    tmp2 = (int32) x[i3] * ss - (int32) x[i4] * cc;
                    t1 = (int16) (tmp1 >> 15) >> atten;
                    t2 = (int16) (tmp2 >> 15) >> atten;
                }

                x[i4] = (x[i2] >> atten) - t2;
                x[i3] = (-x[i2] >> atten) - t2;
                x[i2] = (x[i1] >> atten) - t1;
                x[i1] = (x[i1] >> atten) + t1;
            }
        }
    }

    /* Return the residual scaling factor. */
    return lz;
}
#else                           /* !FIXED16 */
static int
fe_fft_real(fe_t * fe)
{
    int i, j, k, m, n;
    frame_t *x, xt;

    x = fe->frame;
    m = fe->fft_order;
    n = fe->fft_size;

    /* Bit-reverse the input. */
    j = 0;
    for (i = 0; i < n - 1; ++i) {
        if (i < j) {
            xt = x[j];
            x[j] = x[i];
            x[i] = xt;
        }
        k = n / 2;
        while (k <= j) {
            j -= k;
            k /= 2;
        }
        j += k;
    }

    /* Basic butterflies (2-point FFT, real twiddle factors):
     * x[i]   = x[i] +  1 * x[i+1]
     * x[i+1] = x[i] + -1 * x[i+1]
     */
    for (i = 0; i < n; i += 2) {
        xt = x[i];
        x[i] = (xt + x[i + 1]);
        x[i + 1] = (xt - x[i + 1]);
    }

    /* The rest of the butterflies, in stages from 1..m */
    for (k = 1; k < m; ++k) {
        int n1, n2, n4;

        n4 = k - 1;
        n2 = k;
        n1 = k + 1;
        /* Stride over each (1 << (k+1)) points */
        for (i = 0; i < n; i += (1 << n1)) {
            /* Basic butterfly with real twiddle factors:
             * x[i]          = x[i] +  1 * x[i + (1<<k)]
             * x[i + (1<<k)] = x[i] + -1 * x[i + (1<<k)]
             */
            xt = x[i];
            x[i] = (xt + x[i + (1 << n2)]);
            x[i + (1 << n2)] = (xt - x[i + (1 << n2)]);

            /* The other ones with real twiddle factors:
             * x[i + (1<<k) + (1<<(k-1))]
             *   = 0 * x[i + (1<<k-1)] + -1 * x[i + (1<<k) + (1<<k-1)]
             * x[i + (1<<(k-1))]
             *   = 1 * x[i + (1<<k-1)] +  0 * x[i + (1<<k) + (1<<k-1)]
             */
            x[i + (1 << n2) + (1 << n4)] = -x[i + (1 << n2) + (1 << n4)];
            x[i + (1 << n4)] = x[i + (1 << n4)];

            /* Butterflies with complex twiddle factors.
             * There are (1<<k-1) of them.
             */
            for (j = 1; j < (1 << n4); ++j) {
                frame_t cc, ss, t1, t2;
                int i1, i2, i3, i4;

                i1 = i + j;
                i2 = i + (1 << n2) - j;
                i3 = i + (1 << n2) + j;
                i4 = i + (1 << n2) + (1 << n2) - j;

                /*
                 * cc = real(W[j * n / (1<<(k+1))])
                 * ss = imag(W[j * n / (1<<(k+1))])
                 */
                cc = fe->ccc[j << (m - n1)];
                ss = fe->sss[j << (m - n1)];

                /* There are some symmetry properties which allow us
                 * to get away with only four multiplications here. */
                t1 = COSMUL(x[i3], cc) + COSMUL(x[i4], ss);
                t2 = COSMUL(x[i3], ss) - COSMUL(x[i4], cc);

                x[i4] = (x[i2] - t2);
                x[i3] = (-x[i2] - t2);
                x[i2] = (x[i1] - t1);
                x[i1] = (x[i1] + t1);
            }
        }
    }

    /* This isn't used, but return it for completeness. */
    return m;
}
#endif                          /* !FIXED16 */

static void
fe_spec_magnitude(fe_t * fe)
{
    frame_t *fft;
    powspec_t *spec;
    int32 j, scale, fftsize;

    /* Do FFT and get the scaling factor back (only actually used in
     * fixed-point).  Note the scaling factor is expressed in bits. */
    scale = fe_fft_real(fe);

    /* Convenience pointers to make things less awkward below. */
    fft = fe->frame;
    spec = fe->spec;
    fftsize = fe->fft_size;

    /* We need to scale things up the rest of the way to N. */
    scale = fe->fft_order - scale;

    /* The first point (DC coefficient) has no imaginary part */
    {
#ifdef FIXED16
        spec[0] = fixlog(abs(fft[0]) << scale) * 2;
#elif defined(FIXED_POINT)
        spec[0] = FIXLN(abs(fft[0]) << scale) * 2;
#else
        spec[0] = fft[0] * fft[0];
#endif
    }

    for (j = 1; j <= fftsize / 2; j++) {
#ifdef FIXED16
        int32 rr = fixlog(abs(fft[j]) << scale) * 2;
        int32 ii = fixlog(abs(fft[fftsize - j]) << scale) * 2;
        spec[j] = fe_log_add(rr, ii);
#elif defined(FIXED_POINT)
        int32 rr = FIXLN(abs(fft[j]) << scale) * 2;
        int32 ii = FIXLN(abs(fft[fftsize - j]) << scale) * 2;
        spec[j] = fe_log_add(rr, ii);
#else
        spec[j] = fft[j] * fft[j] + fft[fftsize - j] * fft[fftsize - j];
#endif
    }
}

static void
fe_mel_spec(fe_t * fe)
{
    int whichfilt;
    powspec_t *spec, *mfspec;

    /* Convenience poitners. */
    spec = fe->spec;
    mfspec = fe->mfspec;
    for (whichfilt = 0; whichfilt < fe->mel_fb->num_filters; whichfilt++) {
        int spec_start, filt_start, i;

        spec_start = fe->mel_fb->spec_start[whichfilt];
        filt_start = fe->mel_fb->filt_start[whichfilt];

#ifdef FIXED_POINT
        mfspec[whichfilt] =
            spec[spec_start] + fe->mel_fb->filt_coeffs[filt_start];
        for (i = 1; i < fe->mel_fb->filt_width[whichfilt]; i++) {
            mfspec[whichfilt] = fe_log_add(mfspec[whichfilt],
                                           spec[spec_start + i] +
                                           fe->mel_fb->
                                           filt_coeffs[filt_start + i]);
        }
#else                           /* !FIXED_POINT */
        mfspec[whichfilt] = 0;
        for (i = 0; i < fe->mel_fb->filt_width[whichfilt]; i++)
            mfspec[whichfilt] +=
                spec[spec_start + i] * fe->mel_fb->filt_coeffs[filt_start +
                                                               i];
#endif                          /* !FIXED_POINT */
    }

}

#define LOG_FLOOR 1e-4

static void
fe_mel_cep(fe_t * fe, mfcc_t * mfcep)
{
    int32 i;
    powspec_t *mfspec;

    /* Convenience pointer. */
    mfspec = fe->mfspec;

    for (i = 0; i < fe->mel_fb->num_filters; ++i) {
#ifndef FIXED_POINT             /* It's already in log domain for fixed point */
        mfspec[i] = log(mfspec[i] + LOG_FLOOR);
#endif                          /* !FIXED_POINT */
    }

    /* If we are doing LOG_SPEC, then do nothing. */
    if (fe->log_spec == RAW_LOG_SPEC) {
        for (i = 0; i < fe->feature_dimension; i++) {
            mfcep[i] = (mfcc_t) mfspec[i];
        }
    }
    /* For smoothed spectrum, do DCT-II followed by (its inverse) DCT-III */
    else if (fe->log_spec == SMOOTH_LOG_SPEC) {
        /* FIXME: This is probably broken for fixed-point. */
        fe_dct2(fe, mfspec, mfcep, 0);
        fe_dct3(fe, mfcep, mfspec);
        for (i = 0; i < fe->feature_dimension; i++) {
            mfcep[i] = (mfcc_t) mfspec[i];
        }
    }
    else if (fe->transform == DCT_II)
        fe_dct2(fe, mfspec, mfcep, FALSE);
    else if (fe->transform == DCT_HTK)
        fe_dct2(fe, mfspec, mfcep, TRUE);
    else
        fe_spec2cep(fe, mfspec, mfcep);

    return;
}

void
fe_spec2cep(fe_t * fe, const powspec_t * mflogspec, mfcc_t * mfcep)
{
    int32 i, j, beta;

    /* Compute C0 separately (its basis vector is 1) to avoid
     * costly multiplications. */
    mfcep[0] = mflogspec[0] / 2;        /* beta = 0.5 */
    for (j = 1; j < fe->mel_fb->num_filters; j++)
        mfcep[0] += mflogspec[j];       /* beta = 1.0 */
    mfcep[0] /= (frame_t) fe->mel_fb->num_filters;

    for (i = 1; i < fe->num_cepstra; ++i) {
        mfcep[i] = 0;
        for (j = 0; j < fe->mel_fb->num_filters; j++) {
            if (j == 0)
                beta = 1;       /* 0.5 */
            else
                beta = 2;       /* 1.0 */
            mfcep[i] += COSMUL(mflogspec[j],
                               fe->mel_fb->mel_cosine[i][j]) * beta;
        }
        /* Note that this actually normalizes by num_filters, like the
         * original Sphinx front-end, due to the doubled 'beta' factor
         * above.  */
        mfcep[i] /= (frame_t) fe->mel_fb->num_filters * 2;
    }
}

void
fe_dct2(fe_t * fe, const powspec_t * mflogspec, mfcc_t * mfcep, int htk)
{
    int32 i, j;

    /* Compute C0 separately (its basis vector is 1) to avoid
     * costly multiplications. */
    mfcep[0] = mflogspec[0];
    for (j = 1; j < fe->mel_fb->num_filters; j++)
        mfcep[0] += mflogspec[j];
    if (htk)
        mfcep[0] = COSMUL(mfcep[0], fe->mel_fb->sqrt_inv_2n);
    else                        /* sqrt(1/N) = sqrt(2/N) * 1/sqrt(2) */
        mfcep[0] = COSMUL(mfcep[0], fe->mel_fb->sqrt_inv_n);

    for (i = 1; i < fe->num_cepstra; ++i) {
        mfcep[i] = 0;
        for (j = 0; j < fe->mel_fb->num_filters; j++) {
            mfcep[i] += COSMUL(mflogspec[j], fe->mel_fb->mel_cosine[i][j]);
        }
        mfcep[i] = COSMUL(mfcep[i], fe->mel_fb->sqrt_inv_2n);
    }
}

void
fe_lifter(fe_t * fe, mfcc_t * mfcep)
{
    int32 i;

    if (fe->mel_fb->lifter_val == 0)
        return;

    for (i = 0; i < fe->num_cepstra; ++i) {
        mfcep[i] = MFCCMUL(mfcep[i], fe->mel_fb->lifter[i]);
    }
}

void
fe_dct3(fe_t * fe, const mfcc_t * mfcep, powspec_t * mflogspec)
{
    int32 i, j;

    for (i = 0; i < fe->mel_fb->num_filters; ++i) {
        mflogspec[i] = COSMUL(mfcep[0], SQRT_HALF);
        for (j = 1; j < fe->num_cepstra; j++) {
            mflogspec[i] += COSMUL(mfcep[j], fe->mel_fb->mel_cosine[j][i]);
        }
        mflogspec[i] = COSMUL(mflogspec[i], fe->mel_fb->sqrt_inv_2n);
    }
}

void
fe_write_frame(fe_t * fe, mfcc_t * fea)
{
    int32 is_speech;

    fe_spec_magnitude(fe);
    fe_mel_spec(fe);
    fe_track_snr(fe, &is_speech);
    fe_mel_cep(fe, fea);
    fe_lifter(fe, fea);
    fe_vad_hangover(fe, fea, is_speech);
}


void *
fe_create_2d(int32 d1, int32 d2, int32 elem_size)
{
    return (void *) ckd_calloc_2d(d1, d2, elem_size);
}

void
fe_free_2d(void *arr)
{
    ckd_free_2d((void **) arr);
}
