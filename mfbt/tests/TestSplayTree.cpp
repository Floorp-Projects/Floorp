/* -*- Mode: C++; tab-width: 9; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/SplayTree.h"
#include "mozilla/Unused.h"

using mozilla::SplayTree;
using mozilla::SplayTreeNode;

// The following array contains the values 0..999 in random order, as computed
// with the following Python program:
//
//   from random import shuffle
//   x = range(1000)
//   shuffle(x)
//   print(x);
//
static int gValues[] = {
    778, 999, 248, 795, 607, 177, 725, 33,  215, 565, 436, 821, 941, 802, 322,
    54,  151, 416, 531, 65,  818, 99,  340, 401, 274, 767, 278, 617, 425, 629,
    833, 878, 440, 984, 724, 519, 100, 369, 490, 131, 422, 169, 932, 476, 823,
    521, 390, 781, 747, 218, 376, 461, 717, 532, 471, 298, 720, 608, 334, 788,
    161, 500, 280, 963, 430, 484, 779, 572, 96,  333, 650, 158, 199, 137, 991,
    399, 882, 689, 358, 548, 196, 718, 211, 388, 133, 188, 321, 892, 25,  694,
    735, 886, 872, 785, 195, 275, 696, 975, 393, 619, 894, 18,  281, 191, 792,
    846, 861, 351, 542, 806, 570, 702, 931, 585, 444, 284, 217, 132, 251, 253,
    302, 808, 224, 37,  63,  863, 409, 49,  780, 790, 31,  638, 890, 186, 114,
    152, 949, 491, 207, 392, 170, 460, 794, 482, 877, 407, 263, 909, 249, 710,
    614, 51,  431, 915, 62,  332, 74,  495, 901, 23,  365, 752, 89,  660, 745,
    741, 547, 669, 449, 465, 605, 107, 774, 205, 852, 266, 247, 690, 835, 765,
    410, 140, 122, 400, 510, 664, 105, 935, 230, 134, 106, 959, 375, 884, 361,
    527, 715, 840, 272, 232, 102, 415, 903, 117, 313, 153, 463, 464, 876, 406,
    967, 713, 381, 836, 555, 190, 859, 172, 483, 61,  633, 294, 993, 72,  337,
    11,  896, 523, 101, 916, 244, 566, 706, 533, 439, 201, 222, 695, 739, 553,
    571, 289, 918, 209, 189, 357, 814, 670, 866, 910, 579, 246, 636, 750, 891,
    494, 758, 341, 626, 426, 772, 254, 682, 588, 104, 347, 184, 977, 126, 498,
    165, 955, 241, 516, 235, 497, 121, 123, 791, 844, 259, 995, 283, 602, 417,
    221, 308, 855, 429, 86,  345, 928, 44,  679, 796, 363, 402, 445, 492, 450,
    964, 749, 925, 847, 637, 982, 648, 635, 481, 564, 867, 940, 291, 159, 290,
    929, 59,  712, 986, 611, 954, 820, 103, 622, 316, 142, 204, 225, 678, 314,
    84,  578, 315, 141, 990, 880, 504, 969, 412, 746, 47,  517, 124, 848, 466,
    438, 674, 979, 782, 651, 181, 26,  435, 832, 386, 951, 229, 642, 655, 91,
    162, 921, 647, 113, 686, 56,  805, 763, 245, 581, 287, 998, 525, 641, 135,
    634, 237, 728, 112, 828, 228, 899, 1,   723, 16,  613, 144, 659, 97,  185,
    312, 292, 733, 624, 276, 387, 926, 339, 768, 960, 610, 807, 656, 851, 219,
    582, 709, 927, 514, 680, 870, 597, 536, 77,  164, 512, 149, 900, 85,  335,
    997, 8,   705, 777, 653, 815, 311, 701, 507, 202, 530, 827, 541, 958, 82,
    874, 55,  487, 383, 885, 684, 180, 829, 760, 109, 194, 540, 816, 906, 657,
    469, 446, 857, 907, 38,  600, 618, 797, 950, 822, 277, 842, 116, 513, 255,
    424, 643, 163, 372, 129, 67,  118, 754, 529, 917, 687, 473, 174, 538, 939,
    663, 775, 474, 242, 883, 20,  837, 293, 584, 943, 32,  176, 904, 14,  448,
    893, 888, 744, 171, 714, 454, 691, 261, 934, 606, 789, 825, 671, 397, 338,
    317, 612, 737, 130, 41,  923, 574, 136, 980, 850, 12,  729, 197, 403, 57,
    783, 360, 146, 75,  432, 447, 192, 799, 740, 267, 214, 250, 367, 853, 968,
    120, 736, 391, 881, 784, 665, 68,  398, 350, 839, 268, 697, 567, 428, 738,
    48,  182, 70,  220, 865, 418, 374, 148, 945, 353, 539, 589, 307, 427, 506,
    265, 558, 128, 46,  336, 299, 349, 309, 377, 304, 420, 30,  34,  875, 948,
    212, 394, 442, 719, 273, 269, 157, 502, 675, 751, 838, 897, 862, 831, 676,
    590, 811, 966, 854, 477, 15,  598, 573, 108, 98,  81,  408, 421, 296, 73,
    644, 456, 362, 666, 550, 331, 368, 193, 470, 203, 769, 342, 36,  604, 60,
    970, 748, 813, 522, 515, 90,  672, 243, 793, 947, 595, 632, 912, 475, 258,
    80,  873, 623, 524, 546, 262, 727, 216, 505, 330, 373, 58,  297, 609, 908,
    150, 206, 703, 755, 260, 511, 213, 198, 766, 898, 992, 488, 405, 974, 770,
    936, 743, 554, 0,   499, 976, 94,  160, 919, 434, 324, 156, 757, 830, 677,
    183, 630, 871, 640, 938, 518, 344, 366, 742, 552, 306, 535, 200, 652, 496,
    233, 419, 787, 318, 981, 371, 166, 143, 384, 88,  508, 698, 812, 559, 658,
    549, 208, 599, 621, 961, 668, 563, 93,  154, 587, 560, 389, 3,   210, 326,
    4,   924, 300, 2,   804, 914, 801, 753, 654, 27,  236, 19,  708, 451, 985,
    596, 478, 922, 240, 127, 994, 983, 385, 472, 40,  528, 288, 111, 543, 568,
    155, 625, 759, 937, 956, 545, 953, 962, 382, 479, 809, 557, 501, 354, 414,
    343, 378, 843, 379, 178, 556, 800, 803, 592, 627, 942, 576, 920, 704, 707,
    726, 223, 119, 404, 24,  879, 722, 868, 5,   238, 817, 520, 631, 946, 462,
    457, 295, 480, 957, 441, 145, 286, 303, 688, 17,  628, 493, 364, 226, 110,
    615, 69,  320, 534, 593, 721, 411, 285, 869, 952, 849, 139, 356, 346, 28,
    887, 810, 92,  798, 544, 458, 996, 692, 396, 667, 328, 173, 22,  773, 50,
    645, 987, 42,  685, 734, 700, 683, 601, 580, 639, 913, 323, 858, 179, 761,
    6,   841, 905, 234, 730, 29,  21,  575, 586, 902, 443, 826, 646, 257, 125,
    649, 53,  453, 252, 13,  87,  971, 227, 485, 168, 380, 711, 79,  732, 325,
    52,  468, 76,  551, 39,  395, 327, 973, 459, 45,  583, 989, 147, 455, 776,
    944, 569, 889, 256, 35,  175, 834, 756, 933, 860, 526, 845, 864, 764, 771,
    282, 9,   693, 352, 731, 7,   577, 264, 319, 138, 467, 819, 930, 231, 115,
    988, 978, 762, 486, 301, 616, 10,  78,  603, 452, 965, 279, 972, 413, 895,
    591, 662, 594, 348, 423, 489, 43,  699, 433, 509, 355, 270, 66,  83,  95,
    561, 661, 562, 329, 620, 370, 64,  187, 503, 716, 856, 310, 786, 167, 71,
    239, 359, 537, 437, 305, 673, 824, 911, 681, 271};

struct SplayInt : SplayTreeNode<SplayInt> {
  explicit SplayInt(int aValue) : mValue(aValue) {}

  static int compare(const SplayInt& aOne, const SplayInt& aTwo) {
    if (aOne.mValue < aTwo.mValue) {
      return -1;
    }
    if (aOne.mValue > aTwo.mValue) {
      return 1;
    }
    return 0;
  }

  int mValue;
};

struct SplayNoCopy : SplayTreeNode<SplayNoCopy> {
  SplayNoCopy(const SplayNoCopy&) = delete;
  SplayNoCopy(SplayNoCopy&&) = delete;

  static int compare(const SplayNoCopy&, const SplayNoCopy&) { return 0; }
};

static SplayTree<SplayNoCopy, SplayNoCopy> testNoCopy;

int main() {
  mozilla::Unused << testNoCopy;

  SplayTree<SplayInt, SplayInt> tree;

  MOZ_RELEASE_ASSERT(tree.empty());

  MOZ_RELEASE_ASSERT(!tree.find(SplayInt(0)));

  static const int N = mozilla::ArrayLength(gValues);

  // Insert the values, and check each one is findable just after insertion.
  for (int i = 0; i < N; i++) {
    tree.insert(new SplayInt(gValues[i]));
    SplayInt* inserted = tree.find(SplayInt(gValues[i]));
    MOZ_RELEASE_ASSERT(inserted);
    MOZ_RELEASE_ASSERT(tree.findOrInsert(SplayInt(gValues[i])) == inserted);
    tree.checkCoherency();
  }

  // Check they're all findable after all insertions.
  for (int i = 0; i < N; i++) {
    MOZ_RELEASE_ASSERT(tree.find(SplayInt(gValues[i])));
    MOZ_RELEASE_ASSERT(tree.findOrInsert(SplayInt(gValues[i])));
    tree.checkCoherency();
  }

  // Check that non-inserted values cannot be found.
  MOZ_RELEASE_ASSERT(!tree.find(SplayInt(-1)));
  MOZ_RELEASE_ASSERT(!tree.find(SplayInt(N)));
  MOZ_RELEASE_ASSERT(!tree.find(SplayInt(0x7fffffff)));

  // Remove the values, and check each one is not findable just after removal.
  for (int i = 0; i < N; i++) {
    SplayInt* removed = tree.remove(SplayInt(gValues[i]));
    MOZ_RELEASE_ASSERT(removed->mValue == gValues[i]);
    MOZ_RELEASE_ASSERT(!tree.find(*removed));
    delete removed;
    tree.checkCoherency();
  }

  MOZ_RELEASE_ASSERT(tree.empty());

  // Insert the values, and check each one is findable just after insertion.
  for (int i = 0; i < N; i++) {
    SplayInt* inserted = tree.findOrInsert(SplayInt(gValues[i]));
    MOZ_RELEASE_ASSERT(tree.find(SplayInt(gValues[i])) == inserted);
    MOZ_RELEASE_ASSERT(tree.findOrInsert(SplayInt(gValues[i])) == inserted);
    tree.checkCoherency();
  }

  // Check they're all findable after all insertions.
  for (int i = 0; i < N; i++) {
    MOZ_RELEASE_ASSERT(tree.find(SplayInt(gValues[i])));
    MOZ_RELEASE_ASSERT(tree.findOrInsert(SplayInt(gValues[i])));
    tree.checkCoherency();
  }

  // Check that non-inserted values cannot be found.
  MOZ_RELEASE_ASSERT(!tree.find(SplayInt(-1)));
  MOZ_RELEASE_ASSERT(!tree.find(SplayInt(N)));
  MOZ_RELEASE_ASSERT(!tree.find(SplayInt(0x7fffffff)));

  // Remove the values, and check each one is not findable just after removal.
  for (int i = 0; i < N; i++) {
    SplayInt* removed = tree.remove(SplayInt(gValues[i]));
    MOZ_RELEASE_ASSERT(removed->mValue == gValues[i]);
    MOZ_RELEASE_ASSERT(!tree.find(*removed));
    delete removed;
    tree.checkCoherency();
  }

  MOZ_RELEASE_ASSERT(tree.empty());

  // Reinsert the values, in reverse order to last time.
  for (int i = 0; i < N; i++) {
    tree.insert(new SplayInt(gValues[N - i - 1]));
    tree.checkCoherency();
  }

  // Remove the minimum value repeatedly.
  for (int i = 0; i < N; i++) {
    SplayInt* removed = tree.removeMin();
    MOZ_RELEASE_ASSERT(removed->mValue == i);
    delete removed;
    tree.checkCoherency();
  }

  MOZ_RELEASE_ASSERT(tree.empty());

  return 0;
}
