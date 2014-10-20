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

#include "vp9/common/vp9_scan.h"

DECLARE_ALIGNED(16, static const int16_t, default_scan_4x4[16]) = {
  0,  4,  1,  5,
  8,  2, 12,  9,
  3,  6, 13, 10,
  7, 14, 11, 15,
};

DECLARE_ALIGNED(16, static const int16_t, col_scan_4x4[16]) = {
  0,  4,  8,  1,
  12,  5,  9,  2,
  13,  6, 10,  3,
  7, 14, 11, 15,
};

DECLARE_ALIGNED(16, static const int16_t, row_scan_4x4[16]) = {
  0,  1,  4,  2,
  5,  3,  6,  8,
  9,  7, 12, 10,
  13, 11, 14, 15,
};

DECLARE_ALIGNED(16, static const int16_t, default_scan_8x8[64]) = {
  0,  8,  1, 16,  9,  2, 17, 24,
  10,  3, 18, 25, 32, 11,  4, 26,
  33, 19, 40, 12, 34, 27,  5, 41,
  20, 48, 13, 35, 42, 28, 21,  6,
  49, 56, 36, 43, 29,  7, 14, 50,
  57, 44, 22, 37, 15, 51, 58, 30,
  45, 23, 52, 59, 38, 31, 60, 53,
  46, 39, 61, 54, 47, 62, 55, 63,
};

DECLARE_ALIGNED(16, static const int16_t, col_scan_8x8[64]) = {
  0,  8, 16,  1, 24,  9, 32, 17,
  2, 40, 25, 10, 33, 18, 48,  3,
  26, 41, 11, 56, 19, 34,  4, 49,
  27, 42, 12, 35, 20, 57, 50, 28,
  5, 43, 13, 36, 58, 51, 21, 44,
  6, 29, 59, 37, 14, 52, 22,  7,
  45, 60, 30, 15, 38, 53, 23, 46,
  31, 61, 39, 54, 47, 62, 55, 63,
};

DECLARE_ALIGNED(16, static const int16_t, row_scan_8x8[64]) = {
  0,  1,  2,  8,  9,  3, 16, 10,
  4, 17, 11, 24,  5, 18, 25, 12,
  19, 26, 32,  6, 13, 20, 33, 27,
  7, 34, 40, 21, 28, 41, 14, 35,
  48, 42, 29, 36, 49, 22, 43, 15,
  56, 37, 50, 44, 30, 57, 23, 51,
  58, 45, 38, 52, 31, 59, 53, 46,
  60, 39, 61, 47, 54, 55, 62, 63,
};

DECLARE_ALIGNED(16, static const int16_t, default_scan_16x16[256]) = {
  0, 16, 1, 32, 17, 2, 48, 33, 18, 3, 64, 34, 49, 19, 65, 80,
  50, 4, 35, 66, 20, 81, 96, 51, 5, 36, 82, 97, 67, 112, 21, 52,
  98, 37, 83, 113, 6, 68, 128, 53, 22, 99, 114, 84, 7, 129, 38, 69,
  100, 115, 144, 130, 85, 54, 23, 8, 145, 39, 70, 116, 101, 131, 160, 146,
  55, 86, 24, 71, 132, 117, 161, 40, 9, 102, 147, 176, 162, 87, 56, 25,
  133, 118, 177, 148, 72, 103, 41, 163, 10, 192, 178, 88, 57, 134, 149, 119,
  26, 164, 73, 104, 193, 42, 179, 208, 11, 135, 89, 165, 120, 150, 58, 194,
  180, 27, 74, 209, 105, 151, 136, 43, 90, 224, 166, 195, 181, 121, 210, 59,
  12, 152, 106, 167, 196, 75, 137, 225, 211, 240, 182, 122, 91, 28, 197, 13,
  226, 168, 183, 153, 44, 212, 138, 107, 241, 60, 29, 123, 198, 184, 227, 169,
  242, 76, 213, 154, 45, 92, 14, 199, 139, 61, 228, 214, 170, 185, 243, 108,
  77, 155, 30, 15, 200, 229, 124, 215, 244, 93, 46, 186, 171, 201, 109, 140,
  230, 62, 216, 245, 31, 125, 78, 156, 231, 47, 187, 202, 217, 94, 246, 141,
  63, 232, 172, 110, 247, 157, 79, 218, 203, 126, 233, 188, 248, 95, 173, 142,
  219, 111, 249, 234, 158, 127, 189, 204, 250, 235, 143, 174, 220, 205, 159,
  251,
  190, 221, 175, 236, 237, 191, 206, 252, 222, 253, 207, 238, 223, 254, 239,
  255,
};

DECLARE_ALIGNED(16, static const int16_t, col_scan_16x16[256]) = {
  0, 16, 32, 48, 1, 64, 17, 80, 33, 96, 49, 2, 65, 112, 18, 81,
  34, 128, 50, 97, 3, 66, 144, 19, 113, 35, 82, 160, 98, 51, 129, 4,
  67, 176, 20, 114, 145, 83, 36, 99, 130, 52, 192, 5, 161, 68, 115, 21,
  146, 84, 208, 177, 37, 131, 100, 53, 162, 224, 69, 6, 116, 193, 147, 85,
  22, 240, 132, 38, 178, 101, 163, 54, 209, 117, 70, 7, 148, 194, 86, 179,
  225, 23, 133, 39, 164, 8, 102, 210, 241, 55, 195, 118, 149, 71, 180, 24,
  87, 226, 134, 165, 211, 40, 103, 56, 72, 150, 196, 242, 119, 9, 181, 227,
  88, 166, 25, 135, 41, 104, 212, 57, 151, 197, 120, 73, 243, 182, 136, 167,
  213, 89, 10, 228, 105, 152, 198, 26, 42, 121, 183, 244, 168, 58, 137, 229,
  74, 214, 90, 153, 199, 184, 11, 106, 245, 27, 122, 230, 169, 43, 215, 59,
  200, 138, 185, 246, 75, 12, 91, 154, 216, 231, 107, 28, 44, 201, 123, 170,
  60, 247, 232, 76, 139, 13, 92, 217, 186, 248, 155, 108, 29, 124, 45, 202,
  233, 171, 61, 14, 77, 140, 15, 249, 93, 30, 187, 156, 218, 46, 109, 125,
  62, 172, 78, 203, 31, 141, 234, 94, 47, 188, 63, 157, 110, 250, 219, 79,
  126, 204, 173, 142, 95, 189, 111, 235, 158, 220, 251, 127, 174, 143, 205,
  236,
  159, 190, 221, 252, 175, 206, 237, 191, 253, 222, 238, 207, 254, 223, 239,
  255,
};

DECLARE_ALIGNED(16, static const int16_t, row_scan_16x16[256]) = {
  0, 1, 2, 16, 3, 17, 4, 18, 32, 5, 33, 19, 6, 34, 48, 20,
  49, 7, 35, 21, 50, 64, 8, 36, 65, 22, 51, 37, 80, 9, 66, 52,
  23, 38, 81, 67, 10, 53, 24, 82, 68, 96, 39, 11, 54, 83, 97, 69,
  25, 98, 84, 40, 112, 55, 12, 70, 99, 113, 85, 26, 41, 56, 114, 100,
  13, 71, 128, 86, 27, 115, 101, 129, 42, 57, 72, 116, 14, 87, 130, 102,
  144, 73, 131, 117, 28, 58, 15, 88, 43, 145, 103, 132, 146, 118, 74, 160,
  89, 133, 104, 29, 59, 147, 119, 44, 161, 148, 90, 105, 134, 162, 120, 176,
  75, 135, 149, 30, 60, 163, 177, 45, 121, 91, 106, 164, 178, 150, 192, 136,
  165, 179, 31, 151, 193, 76, 122, 61, 137, 194, 107, 152, 180, 208, 46, 166,
  167, 195, 92, 181, 138, 209, 123, 153, 224, 196, 77, 168, 210, 182, 240, 108,
  197, 62, 154, 225, 183, 169, 211, 47, 139, 93, 184, 226, 212, 241, 198, 170,
  124, 155, 199, 78, 213, 185, 109, 227, 200, 63, 228, 242, 140, 214, 171, 186,
  156, 229, 243, 125, 94, 201, 244, 215, 216, 230, 141, 187, 202, 79, 172, 110,
  157, 245, 217, 231, 95, 246, 232, 126, 203, 247, 233, 173, 218, 142, 111,
  158,
  188, 248, 127, 234, 219, 249, 189, 204, 143, 174, 159, 250, 235, 205, 220,
  175,
  190, 251, 221, 191, 206, 236, 207, 237, 252, 222, 253, 223, 238, 239, 254,
  255,
};

DECLARE_ALIGNED(16, static const int16_t, default_scan_32x32[1024]) = {
  0, 32, 1, 64, 33, 2, 96, 65, 34, 128, 3, 97, 66, 160,
  129, 35, 98, 4, 67, 130, 161, 192, 36, 99, 224, 5, 162, 193,
  68, 131, 37, 100,
  225, 194, 256, 163, 69, 132, 6, 226, 257, 288, 195, 101, 164, 38,
  258, 7, 227, 289, 133, 320, 70, 196, 165, 290, 259, 228, 39, 321,
  102, 352, 8, 197,
  71, 134, 322, 291, 260, 353, 384, 229, 166, 103, 40, 354, 323, 292,
  135, 385, 198, 261, 72, 9, 416, 167, 386, 355, 230, 324, 104, 293,
  41, 417, 199, 136,
  262, 387, 448, 325, 356, 10, 73, 418, 231, 168, 449, 294, 388, 105,
  419, 263, 42, 200, 357, 450, 137, 480, 74, 326, 232, 11, 389, 169,
  295, 420, 106, 451,
  481, 358, 264, 327, 201, 43, 138, 512, 482, 390, 296, 233, 170, 421,
  75, 452, 359, 12, 513, 265, 483, 328, 107, 202, 514, 544, 422, 391,
  453, 139, 44, 234,
  484, 297, 360, 171, 76, 515, 545, 266, 329, 454, 13, 423, 203, 108,
  546, 485, 576, 298, 235, 140, 361, 330, 172, 547, 45, 455, 267, 577,
  486, 77, 204, 362,
  608, 14, 299, 578, 109, 236, 487, 609, 331, 141, 579, 46, 15, 173,
  610, 363, 78, 205, 16, 110, 237, 611, 142, 47, 174, 79, 206, 17,
  111, 238, 48, 143,
  80, 175, 112, 207, 49, 18, 239, 81, 113, 19, 50, 82, 114, 51,
  83, 115, 640, 516, 392, 268, 144, 20, 672, 641, 548, 517, 424,
  393, 300, 269, 176, 145,
  52, 21, 704, 673, 642, 580, 549, 518, 456, 425, 394, 332, 301,
  270, 208, 177, 146, 84, 53, 22, 736, 705, 674, 643, 612, 581,
  550, 519, 488, 457, 426, 395,
  364, 333, 302, 271, 240, 209, 178, 147, 116, 85, 54, 23, 737,
  706, 675, 613, 582, 551, 489, 458, 427, 365, 334, 303, 241,
  210, 179, 117, 86, 55, 738, 707,
  614, 583, 490, 459, 366, 335, 242, 211, 118, 87, 739, 615, 491,
  367, 243, 119, 768, 644, 520, 396, 272, 148, 24, 800, 769, 676,
  645, 552, 521, 428, 397, 304,
  273, 180, 149, 56, 25, 832, 801, 770, 708, 677, 646, 584, 553,
  522, 460, 429, 398, 336, 305, 274, 212, 181, 150, 88, 57, 26,
  864, 833, 802, 771, 740, 709,
  678, 647, 616, 585, 554, 523, 492, 461, 430, 399, 368, 337, 306,
  275, 244, 213, 182, 151, 120, 89, 58, 27, 865, 834, 803, 741,
  710, 679, 617, 586, 555, 493,
  462, 431, 369, 338, 307, 245, 214, 183, 121, 90, 59, 866, 835,
  742, 711, 618, 587, 494, 463, 370, 339, 246, 215, 122, 91, 867,
  743, 619, 495, 371, 247, 123,
  896, 772, 648, 524, 400, 276, 152, 28, 928, 897, 804, 773, 680,
  649, 556, 525, 432, 401, 308, 277, 184, 153, 60, 29, 960, 929,
  898, 836, 805, 774, 712, 681,
  650, 588, 557, 526, 464, 433, 402, 340, 309, 278, 216, 185, 154,
  92, 61, 30, 992, 961, 930, 899, 868, 837, 806, 775, 744, 713, 682,
  651, 620, 589, 558, 527,
  496, 465, 434, 403, 372, 341, 310, 279, 248, 217, 186, 155, 124,
  93, 62, 31, 993, 962, 931, 869, 838, 807, 745, 714, 683, 621, 590,
  559, 497, 466, 435, 373,
  342, 311, 249, 218, 187, 125, 94, 63, 994, 963, 870, 839, 746, 715,
  622, 591, 498, 467, 374, 343, 250, 219, 126, 95, 995, 871, 747, 623,
  499, 375, 251, 127,
  900, 776, 652, 528, 404, 280, 156, 932, 901, 808, 777, 684, 653, 560,
  529, 436, 405, 312, 281, 188, 157, 964, 933, 902, 840, 809, 778, 716,
  685, 654, 592, 561,
  530, 468, 437, 406, 344, 313, 282, 220, 189, 158, 996, 965, 934, 903,
  872, 841, 810, 779, 748, 717, 686, 655, 624, 593, 562, 531, 500, 469,
  438, 407, 376, 345,
  314, 283, 252, 221, 190, 159, 997, 966, 935, 873, 842, 811, 749, 718,
  687, 625, 594, 563, 501, 470, 439, 377, 346, 315, 253, 222, 191, 998,
  967, 874, 843, 750,
  719, 626, 595, 502, 471, 378, 347, 254, 223, 999, 875, 751, 627, 503,
  379, 255, 904, 780, 656, 532, 408, 284, 936, 905, 812, 781, 688, 657,
  564, 533, 440, 409,
  316, 285, 968, 937, 906, 844, 813, 782, 720, 689, 658, 596, 565, 534,
  472, 441, 410, 348, 317, 286, 1000, 969, 938, 907, 876, 845, 814, 783,
  752, 721, 690, 659,
  628, 597, 566, 535, 504, 473, 442, 411, 380, 349, 318, 287, 1001, 970,
  939, 877, 846, 815, 753, 722, 691, 629, 598, 567, 505, 474, 443, 381,
  350, 319, 1002, 971,
  878, 847, 754, 723, 630, 599, 506, 475, 382, 351, 1003, 879, 755, 631,
  507, 383, 908, 784, 660, 536, 412, 940, 909, 816, 785, 692, 661, 568,
  537, 444, 413, 972,
  941, 910, 848, 817, 786, 724, 693, 662, 600, 569, 538, 476, 445, 414,
  1004, 973, 942, 911, 880, 849, 818, 787, 756, 725, 694, 663, 632, 601,
  570, 539, 508, 477,
  446, 415, 1005, 974, 943, 881, 850, 819, 757, 726, 695, 633, 602, 571,
  509, 478, 447, 1006, 975, 882, 851, 758, 727, 634, 603, 510, 479,
  1007, 883, 759, 635, 511,
  912, 788, 664, 540, 944, 913, 820, 789, 696, 665, 572, 541, 976, 945,
  914, 852, 821, 790, 728, 697, 666, 604, 573, 542, 1008, 977, 946, 915,
  884, 853, 822, 791,
  760, 729, 698, 667, 636, 605, 574, 543, 1009, 978, 947, 885, 854, 823,
  761, 730, 699, 637, 606, 575, 1010, 979, 886, 855, 762, 731, 638, 607,
  1011, 887, 763, 639,
  916, 792, 668, 948, 917, 824, 793, 700, 669, 980, 949, 918, 856, 825,
  794, 732, 701, 670, 1012, 981, 950, 919, 888, 857, 826, 795, 764, 733,
  702, 671, 1013, 982,
  951, 889, 858, 827, 765, 734, 703, 1014, 983, 890, 859, 766, 735, 1015,
  891, 767, 920, 796, 952, 921, 828, 797, 984, 953, 922, 860, 829, 798,
  1016, 985, 954, 923,
  892, 861, 830, 799, 1017, 986, 955, 893, 862, 831, 1018, 987, 894, 863,
  1019, 895, 924, 956, 925, 988, 957, 926, 1020, 989, 958, 927, 1021,
  990, 959, 1022, 991, 1023,
};

// Neighborhood 5-tuples for various scans and blocksizes,
// in {top, left, topleft, topright, bottomleft} order
// for each position in raster scan order.
// -1 indicates the neighbor does not exist.
DECLARE_ALIGNED(16, static int16_t,
                default_scan_4x4_neighbors[17 * MAX_NEIGHBORS]);
DECLARE_ALIGNED(16, static int16_t,
                col_scan_4x4_neighbors[17 * MAX_NEIGHBORS]);
DECLARE_ALIGNED(16, static int16_t,
                row_scan_4x4_neighbors[17 * MAX_NEIGHBORS]);
DECLARE_ALIGNED(16, static int16_t,
                col_scan_8x8_neighbors[65 * MAX_NEIGHBORS]);
DECLARE_ALIGNED(16, static int16_t,
                row_scan_8x8_neighbors[65 * MAX_NEIGHBORS]);
DECLARE_ALIGNED(16, static int16_t,
                default_scan_8x8_neighbors[65 * MAX_NEIGHBORS]);
DECLARE_ALIGNED(16, static int16_t,
                col_scan_16x16_neighbors[257 * MAX_NEIGHBORS]);
DECLARE_ALIGNED(16, static int16_t,
                row_scan_16x16_neighbors[257 * MAX_NEIGHBORS]);
DECLARE_ALIGNED(16, static int16_t,
                default_scan_16x16_neighbors[257 * MAX_NEIGHBORS]);
DECLARE_ALIGNED(16, static int16_t,
                default_scan_32x32_neighbors[1025 * MAX_NEIGHBORS]);

DECLARE_ALIGNED(16, static int16_t, vp9_default_iscan_4x4[16]);
DECLARE_ALIGNED(16, static int16_t, vp9_col_iscan_4x4[16]);
DECLARE_ALIGNED(16, static int16_t, vp9_row_iscan_4x4[16]);
DECLARE_ALIGNED(16, static int16_t, vp9_col_iscan_8x8[64]);
DECLARE_ALIGNED(16, static int16_t, vp9_row_iscan_8x8[64]);
DECLARE_ALIGNED(16, static int16_t, vp9_default_iscan_8x8[64]);
DECLARE_ALIGNED(16, static int16_t, vp9_col_iscan_16x16[256]);
DECLARE_ALIGNED(16, static int16_t, vp9_row_iscan_16x16[256]);
DECLARE_ALIGNED(16, static  int16_t, vp9_default_iscan_16x16[256]);
DECLARE_ALIGNED(16, static int16_t, vp9_default_iscan_32x32[1024]);

const scan_order vp9_default_scan_orders[TX_SIZES] = {
  {default_scan_4x4,   vp9_default_iscan_4x4,   default_scan_4x4_neighbors},
  {default_scan_8x8,   vp9_default_iscan_8x8,   default_scan_8x8_neighbors},
  {default_scan_16x16, vp9_default_iscan_16x16, default_scan_16x16_neighbors},
  {default_scan_32x32, vp9_default_iscan_32x32, default_scan_32x32_neighbors},
};

const scan_order vp9_scan_orders[TX_SIZES][TX_TYPES] = {
  {  // TX_4X4
    {default_scan_4x4, vp9_default_iscan_4x4, default_scan_4x4_neighbors},
    {row_scan_4x4,     vp9_row_iscan_4x4,     row_scan_4x4_neighbors},
    {col_scan_4x4,     vp9_col_iscan_4x4,     col_scan_4x4_neighbors},
    {default_scan_4x4, vp9_default_iscan_4x4, default_scan_4x4_neighbors}
  }, {  // TX_8X8
    {default_scan_8x8, vp9_default_iscan_8x8, default_scan_8x8_neighbors},
    {row_scan_8x8,     vp9_row_iscan_8x8,     row_scan_8x8_neighbors},
    {col_scan_8x8,     vp9_col_iscan_8x8,     col_scan_8x8_neighbors},
    {default_scan_8x8, vp9_default_iscan_8x8, default_scan_8x8_neighbors}
  }, {  // TX_16X16
    {default_scan_16x16, vp9_default_iscan_16x16, default_scan_16x16_neighbors},
    {row_scan_16x16,     vp9_row_iscan_16x16,     row_scan_16x16_neighbors},
    {col_scan_16x16,     vp9_col_iscan_16x16,     col_scan_16x16_neighbors},
    {default_scan_16x16, vp9_default_iscan_16x16, default_scan_16x16_neighbors}
  }, {  // TX_32X32
    {default_scan_32x32, vp9_default_iscan_32x32, default_scan_32x32_neighbors},
    {default_scan_32x32, vp9_default_iscan_32x32, default_scan_32x32_neighbors},
    {default_scan_32x32, vp9_default_iscan_32x32, default_scan_32x32_neighbors},
    {default_scan_32x32, vp9_default_iscan_32x32, default_scan_32x32_neighbors},
  }
};

static int find_in_scan(const int16_t *scan, int l, int idx) {
  int n, l2 = l * l;
  for (n = 0; n < l2; n++) {
    int rc = scan[n];
    if (rc == idx)
      return  n;
  }
  assert(0);
  return -1;
}

static void init_scan_neighbors(const int16_t *scan, int16_t *iscan, int l,
                                int16_t *neighbors) {
  int l2 = l * l;
  int n, i, j;

  // dc doesn't use this type of prediction
  neighbors[MAX_NEIGHBORS * 0 + 0] = 0;
  neighbors[MAX_NEIGHBORS * 0 + 1] = 0;
  iscan[0] = find_in_scan(scan, l, 0);
  for (n = 1; n < l2; n++) {
    int rc = scan[n];
    iscan[n] = find_in_scan(scan, l, n);
    i = rc / l;
    j = rc % l;
    if (i > 0 && j > 0) {
      // col/row scan is used for adst/dct, and generally means that
      // energy decreases to zero much faster in the dimension in
      // which ADST is used compared to the direction in which DCT
      // is used. Likewise, we find much higher correlation between
      // coefficients within the direction in which DCT is used.
      // Therefore, if we use ADST/DCT, prefer the DCT neighbor coeff
      // as a context. If ADST or DCT is used in both directions, we
      // use the combination of the two as a context.
      int a = (i - 1) * l + j;
      int b =  i      * l + j - 1;
      if (scan == col_scan_4x4 || scan == col_scan_8x8 ||
          scan == col_scan_16x16) {
        // in the col/row scan cases (as well as left/top edge cases), we set
        // both contexts to the same value, so we can branchlessly do a+b+1>>1
        // which automatically becomes a if a == b
        neighbors[MAX_NEIGHBORS * n + 0] =
        neighbors[MAX_NEIGHBORS * n + 1] = a;
      } else if (scan == row_scan_4x4 || scan == row_scan_8x8 ||
                 scan == row_scan_16x16) {
        neighbors[MAX_NEIGHBORS * n + 0] =
        neighbors[MAX_NEIGHBORS * n + 1] = b;
      } else {
        neighbors[MAX_NEIGHBORS * n + 0] = a;
        neighbors[MAX_NEIGHBORS * n + 1] = b;
      }
    } else if (i > 0) {
      neighbors[MAX_NEIGHBORS * n + 0] =
      neighbors[MAX_NEIGHBORS * n + 1] = (i - 1) * l + j;
    } else {
      assert(j > 0);
      neighbors[MAX_NEIGHBORS * n + 0] =
      neighbors[MAX_NEIGHBORS * n + 1] =  i      * l + j - 1;
    }
    assert(iscan[neighbors[MAX_NEIGHBORS * n + 0]] < n);
  }
  // one padding item so we don't have to add branches in code to handle
  // calls to get_coef_context() for the token after the final dc token
  neighbors[MAX_NEIGHBORS * l2 + 0] = 0;
  neighbors[MAX_NEIGHBORS * l2 + 1] = 0;
}

void vp9_init_neighbors() {
  init_scan_neighbors(default_scan_4x4, vp9_default_iscan_4x4, 4,
                      default_scan_4x4_neighbors);
  init_scan_neighbors(row_scan_4x4, vp9_row_iscan_4x4, 4,
                      row_scan_4x4_neighbors);
  init_scan_neighbors(col_scan_4x4, vp9_col_iscan_4x4, 4,
                      col_scan_4x4_neighbors);
  init_scan_neighbors(default_scan_8x8, vp9_default_iscan_8x8, 8,
                      default_scan_8x8_neighbors);
  init_scan_neighbors(row_scan_8x8, vp9_row_iscan_8x8, 8,
                      row_scan_8x8_neighbors);
  init_scan_neighbors(col_scan_8x8, vp9_col_iscan_8x8, 8,
                      col_scan_8x8_neighbors);
  init_scan_neighbors(default_scan_16x16, vp9_default_iscan_16x16, 16,
                      default_scan_16x16_neighbors);
  init_scan_neighbors(row_scan_16x16, vp9_row_iscan_16x16, 16,
                      row_scan_16x16_neighbors);
  init_scan_neighbors(col_scan_16x16, vp9_col_iscan_16x16, 16,
                      col_scan_16x16_neighbors);
  init_scan_neighbors(default_scan_32x32, vp9_default_iscan_32x32, 32,
                      default_scan_32x32_neighbors);
}
