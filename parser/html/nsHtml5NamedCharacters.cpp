#define nsHtml5NamedCharacters_cpp__
#include "prtypes.h"
#include "jArray.h"
#include "nscore.h"

#include "nsHtml5NamedCharacters.h"

jArray<jArray<PRUnichar,PRInt32>,PRInt32> nsHtml5NamedCharacters::NAMES = jArray<jArray<PRUnichar,PRInt32>,PRInt32>();
jArray<PRUnichar,PRInt32>* nsHtml5NamedCharacters::VALUES = nsnull;
PRUnichar** nsHtml5NamedCharacters::WINDOWS_1252 = nsnull;
static PRUnichar const WINDOWS_1252_DATA[] = {
  0x20AC,
  0xFFFD,
  0x201A,
  0x0192,
  0x201E,
  0x2026,
  0x2020,
  0x2021,
  0x02C6,
  0x2030,
  0x0160,
  0x2039,
  0x0152,
  0xFFFD,
  0x017D,
  0xFFFD,
  0xFFFD,
  0x2018,
  0x2019,
  0x201C,
  0x201D,
  0x2022,
  0x2013,
  0x2014,
  0x02DC,
  0x2122,
  0x0161,
  0x203A,
  0x0153,
  0xFFFD,
  0x017E,
  0x0178
};
static PRUnichar const NAME_0[] = {
  'A', 'E', 'l', 'i', 'g'
};
static PRUnichar const VALUE_0[] = {
  0x00c6
};
static PRUnichar const NAME_1[] = {
  'A', 'E', 'l', 'i', 'g', ';'
};
static PRUnichar const VALUE_1[] = {
  0x00c6
};
static PRUnichar const NAME_2[] = {
  'A', 'M', 'P'
};
static PRUnichar const VALUE_2[] = {
  0x0026
};
static PRUnichar const NAME_3[] = {
  'A', 'M', 'P', ';'
};
static PRUnichar const VALUE_3[] = {
  0x0026
};
static PRUnichar const NAME_4[] = {
  'A', 'a', 'c', 'u', 't', 'e'
};
static PRUnichar const VALUE_4[] = {
  0x00c1
};
static PRUnichar const NAME_5[] = {
  'A', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_5[] = {
  0x00c1
};
static PRUnichar const NAME_6[] = {
  'A', 'b', 'r', 'e', 'v', 'e', ';'
};
static PRUnichar const VALUE_6[] = {
  0x0102
};
static PRUnichar const NAME_7[] = {
  'A', 'c', 'i', 'r', 'c'
};
static PRUnichar const VALUE_7[] = {
  0x00c2
};
static PRUnichar const NAME_8[] = {
  'A', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_8[] = {
  0x00c2
};
static PRUnichar const NAME_9[] = {
  'A', 'c', 'y', ';'
};
static PRUnichar const VALUE_9[] = {
  0x0410
};
static PRUnichar const NAME_10[] = {
  'A', 'f', 'r', ';'
};
static PRUnichar const VALUE_10[] = {
  0xd835, 0xdd04
};
static PRUnichar const NAME_11[] = {
  'A', 'g', 'r', 'a', 'v', 'e'
};
static PRUnichar const VALUE_11[] = {
  0x00c0
};
static PRUnichar const NAME_12[] = {
  'A', 'g', 'r', 'a', 'v', 'e', ';'
};
static PRUnichar const VALUE_12[] = {
  0x00c0
};
static PRUnichar const NAME_13[] = {
  'A', 'l', 'p', 'h', 'a', ';'
};
static PRUnichar const VALUE_13[] = {
  0x0391
};
static PRUnichar const NAME_14[] = {
  'A', 'm', 'a', 'c', 'r', ';'
};
static PRUnichar const VALUE_14[] = {
  0x0100
};
static PRUnichar const NAME_15[] = {
  'A', 'n', 'd', ';'
};
static PRUnichar const VALUE_15[] = {
  0x2a53
};
static PRUnichar const NAME_16[] = {
  'A', 'o', 'g', 'o', 'n', ';'
};
static PRUnichar const VALUE_16[] = {
  0x0104
};
static PRUnichar const NAME_17[] = {
  'A', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_17[] = {
  0xd835, 0xdd38
};
static PRUnichar const NAME_18[] = {
  'A', 'p', 'p', 'l', 'y', 'F', 'u', 'n', 'c', 't', 'i', 'o', 'n', ';'
};
static PRUnichar const VALUE_18[] = {
  0x2061
};
static PRUnichar const NAME_19[] = {
  'A', 'r', 'i', 'n', 'g'
};
static PRUnichar const VALUE_19[] = {
  0x00c5
};
static PRUnichar const NAME_20[] = {
  'A', 'r', 'i', 'n', 'g', ';'
};
static PRUnichar const VALUE_20[] = {
  0x00c5
};
static PRUnichar const NAME_21[] = {
  'A', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_21[] = {
  0xd835, 0xdc9c
};
static PRUnichar const NAME_22[] = {
  'A', 's', 's', 'i', 'g', 'n', ';'
};
static PRUnichar const VALUE_22[] = {
  0x2254
};
static PRUnichar const NAME_23[] = {
  'A', 't', 'i', 'l', 'd', 'e'
};
static PRUnichar const VALUE_23[] = {
  0x00c3
};
static PRUnichar const NAME_24[] = {
  'A', 't', 'i', 'l', 'd', 'e', ';'
};
static PRUnichar const VALUE_24[] = {
  0x00c3
};
static PRUnichar const NAME_25[] = {
  'A', 'u', 'm', 'l'
};
static PRUnichar const VALUE_25[] = {
  0x00c4
};
static PRUnichar const NAME_26[] = {
  'A', 'u', 'm', 'l', ';'
};
static PRUnichar const VALUE_26[] = {
  0x00c4
};
static PRUnichar const NAME_27[] = {
  'B', 'a', 'c', 'k', 's', 'l', 'a', 's', 'h', ';'
};
static PRUnichar const VALUE_27[] = {
  0x2216
};
static PRUnichar const NAME_28[] = {
  'B', 'a', 'r', 'v', ';'
};
static PRUnichar const VALUE_28[] = {
  0x2ae7
};
static PRUnichar const NAME_29[] = {
  'B', 'a', 'r', 'w', 'e', 'd', ';'
};
static PRUnichar const VALUE_29[] = {
  0x2306
};
static PRUnichar const NAME_30[] = {
  'B', 'c', 'y', ';'
};
static PRUnichar const VALUE_30[] = {
  0x0411
};
static PRUnichar const NAME_31[] = {
  'B', 'e', 'c', 'a', 'u', 's', 'e', ';'
};
static PRUnichar const VALUE_31[] = {
  0x2235
};
static PRUnichar const NAME_32[] = {
  'B', 'e', 'r', 'n', 'o', 'u', 'l', 'l', 'i', 's', ';'
};
static PRUnichar const VALUE_32[] = {
  0x212c
};
static PRUnichar const NAME_33[] = {
  'B', 'e', 't', 'a', ';'
};
static PRUnichar const VALUE_33[] = {
  0x0392
};
static PRUnichar const NAME_34[] = {
  'B', 'f', 'r', ';'
};
static PRUnichar const VALUE_34[] = {
  0xd835, 0xdd05
};
static PRUnichar const NAME_35[] = {
  'B', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_35[] = {
  0xd835, 0xdd39
};
static PRUnichar const NAME_36[] = {
  'B', 'r', 'e', 'v', 'e', ';'
};
static PRUnichar const VALUE_36[] = {
  0x02d8
};
static PRUnichar const NAME_37[] = {
  'B', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_37[] = {
  0x212c
};
static PRUnichar const NAME_38[] = {
  'B', 'u', 'm', 'p', 'e', 'q', ';'
};
static PRUnichar const VALUE_38[] = {
  0x224e
};
static PRUnichar const NAME_39[] = {
  'C', 'H', 'c', 'y', ';'
};
static PRUnichar const VALUE_39[] = {
  0x0427
};
static PRUnichar const NAME_40[] = {
  'C', 'O', 'P', 'Y'
};
static PRUnichar const VALUE_40[] = {
  0x00a9
};
static PRUnichar const NAME_41[] = {
  'C', 'O', 'P', 'Y', ';'
};
static PRUnichar const VALUE_41[] = {
  0x00a9
};
static PRUnichar const NAME_42[] = {
  'C', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_42[] = {
  0x0106
};
static PRUnichar const NAME_43[] = {
  'C', 'a', 'p', ';'
};
static PRUnichar const VALUE_43[] = {
  0x22d2
};
static PRUnichar const NAME_44[] = {
  'C', 'a', 'p', 'i', 't', 'a', 'l', 'D', 'i', 'f', 'f', 'e', 'r', 'e', 'n', 't', 'i', 'a', 'l', 'D', ';'
};
static PRUnichar const VALUE_44[] = {
  0x2145
};
static PRUnichar const NAME_45[] = {
  'C', 'a', 'y', 'l', 'e', 'y', 's', ';'
};
static PRUnichar const VALUE_45[] = {
  0x212d
};
static PRUnichar const NAME_46[] = {
  'C', 'c', 'a', 'r', 'o', 'n', ';'
};
static PRUnichar const VALUE_46[] = {
  0x010c
};
static PRUnichar const NAME_47[] = {
  'C', 'c', 'e', 'd', 'i', 'l'
};
static PRUnichar const VALUE_47[] = {
  0x00c7
};
static PRUnichar const NAME_48[] = {
  'C', 'c', 'e', 'd', 'i', 'l', ';'
};
static PRUnichar const VALUE_48[] = {
  0x00c7
};
static PRUnichar const NAME_49[] = {
  'C', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_49[] = {
  0x0108
};
static PRUnichar const NAME_50[] = {
  'C', 'c', 'o', 'n', 'i', 'n', 't', ';'
};
static PRUnichar const VALUE_50[] = {
  0x2230
};
static PRUnichar const NAME_51[] = {
  'C', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_51[] = {
  0x010a
};
static PRUnichar const NAME_52[] = {
  'C', 'e', 'd', 'i', 'l', 'l', 'a', ';'
};
static PRUnichar const VALUE_52[] = {
  0x00b8
};
static PRUnichar const NAME_53[] = {
  'C', 'e', 'n', 't', 'e', 'r', 'D', 'o', 't', ';'
};
static PRUnichar const VALUE_53[] = {
  0x00b7
};
static PRUnichar const NAME_54[] = {
  'C', 'f', 'r', ';'
};
static PRUnichar const VALUE_54[] = {
  0x212d
};
static PRUnichar const NAME_55[] = {
  'C', 'h', 'i', ';'
};
static PRUnichar const VALUE_55[] = {
  0x03a7
};
static PRUnichar const NAME_56[] = {
  'C', 'i', 'r', 'c', 'l', 'e', 'D', 'o', 't', ';'
};
static PRUnichar const VALUE_56[] = {
  0x2299
};
static PRUnichar const NAME_57[] = {
  'C', 'i', 'r', 'c', 'l', 'e', 'M', 'i', 'n', 'u', 's', ';'
};
static PRUnichar const VALUE_57[] = {
  0x2296
};
static PRUnichar const NAME_58[] = {
  'C', 'i', 'r', 'c', 'l', 'e', 'P', 'l', 'u', 's', ';'
};
static PRUnichar const VALUE_58[] = {
  0x2295
};
static PRUnichar const NAME_59[] = {
  'C', 'i', 'r', 'c', 'l', 'e', 'T', 'i', 'm', 'e', 's', ';'
};
static PRUnichar const VALUE_59[] = {
  0x2297
};
static PRUnichar const NAME_60[] = {
  'C', 'l', 'o', 'c', 'k', 'w', 'i', 's', 'e', 'C', 'o', 'n', 't', 'o', 'u', 'r', 'I', 'n', 't', 'e', 'g', 'r', 'a', 'l', ';'
};
static PRUnichar const VALUE_60[] = {
  0x2232
};
static PRUnichar const NAME_61[] = {
  'C', 'l', 'o', 's', 'e', 'C', 'u', 'r', 'l', 'y', 'D', 'o', 'u', 'b', 'l', 'e', 'Q', 'u', 'o', 't', 'e', ';'
};
static PRUnichar const VALUE_61[] = {
  0x201d
};
static PRUnichar const NAME_62[] = {
  'C', 'l', 'o', 's', 'e', 'C', 'u', 'r', 'l', 'y', 'Q', 'u', 'o', 't', 'e', ';'
};
static PRUnichar const VALUE_62[] = {
  0x2019
};
static PRUnichar const NAME_63[] = {
  'C', 'o', 'l', 'o', 'n', ';'
};
static PRUnichar const VALUE_63[] = {
  0x2237
};
static PRUnichar const NAME_64[] = {
  'C', 'o', 'l', 'o', 'n', 'e', ';'
};
static PRUnichar const VALUE_64[] = {
  0x2a74
};
static PRUnichar const NAME_65[] = {
  'C', 'o', 'n', 'g', 'r', 'u', 'e', 'n', 't', ';'
};
static PRUnichar const VALUE_65[] = {
  0x2261
};
static PRUnichar const NAME_66[] = {
  'C', 'o', 'n', 'i', 'n', 't', ';'
};
static PRUnichar const VALUE_66[] = {
  0x222f
};
static PRUnichar const NAME_67[] = {
  'C', 'o', 'n', 't', 'o', 'u', 'r', 'I', 'n', 't', 'e', 'g', 'r', 'a', 'l', ';'
};
static PRUnichar const VALUE_67[] = {
  0x222e
};
static PRUnichar const NAME_68[] = {
  'C', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_68[] = {
  0x2102
};
static PRUnichar const NAME_69[] = {
  'C', 'o', 'p', 'r', 'o', 'd', 'u', 'c', 't', ';'
};
static PRUnichar const VALUE_69[] = {
  0x2210
};
static PRUnichar const NAME_70[] = {
  'C', 'o', 'u', 'n', 't', 'e', 'r', 'C', 'l', 'o', 'c', 'k', 'w', 'i', 's', 'e', 'C', 'o', 'n', 't', 'o', 'u', 'r', 'I', 'n', 't', 'e', 'g', 'r', 'a', 'l', ';'
};
static PRUnichar const VALUE_70[] = {
  0x2233
};
static PRUnichar const NAME_71[] = {
  'C', 'r', 'o', 's', 's', ';'
};
static PRUnichar const VALUE_71[] = {
  0x2a2f
};
static PRUnichar const NAME_72[] = {
  'C', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_72[] = {
  0xd835, 0xdc9e
};
static PRUnichar const NAME_73[] = {
  'C', 'u', 'p', ';'
};
static PRUnichar const VALUE_73[] = {
  0x22d3
};
static PRUnichar const NAME_74[] = {
  'C', 'u', 'p', 'C', 'a', 'p', ';'
};
static PRUnichar const VALUE_74[] = {
  0x224d
};
static PRUnichar const NAME_75[] = {
  'D', 'D', ';'
};
static PRUnichar const VALUE_75[] = {
  0x2145
};
static PRUnichar const NAME_76[] = {
  'D', 'D', 'o', 't', 'r', 'a', 'h', 'd', ';'
};
static PRUnichar const VALUE_76[] = {
  0x2911
};
static PRUnichar const NAME_77[] = {
  'D', 'J', 'c', 'y', ';'
};
static PRUnichar const VALUE_77[] = {
  0x0402
};
static PRUnichar const NAME_78[] = {
  'D', 'S', 'c', 'y', ';'
};
static PRUnichar const VALUE_78[] = {
  0x0405
};
static PRUnichar const NAME_79[] = {
  'D', 'Z', 'c', 'y', ';'
};
static PRUnichar const VALUE_79[] = {
  0x040f
};
static PRUnichar const NAME_80[] = {
  'D', 'a', 'g', 'g', 'e', 'r', ';'
};
static PRUnichar const VALUE_80[] = {
  0x2021
};
static PRUnichar const NAME_81[] = {
  'D', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_81[] = {
  0x21a1
};
static PRUnichar const NAME_82[] = {
  'D', 'a', 's', 'h', 'v', ';'
};
static PRUnichar const VALUE_82[] = {
  0x2ae4
};
static PRUnichar const NAME_83[] = {
  'D', 'c', 'a', 'r', 'o', 'n', ';'
};
static PRUnichar const VALUE_83[] = {
  0x010e
};
static PRUnichar const NAME_84[] = {
  'D', 'c', 'y', ';'
};
static PRUnichar const VALUE_84[] = {
  0x0414
};
static PRUnichar const NAME_85[] = {
  'D', 'e', 'l', ';'
};
static PRUnichar const VALUE_85[] = {
  0x2207
};
static PRUnichar const NAME_86[] = {
  'D', 'e', 'l', 't', 'a', ';'
};
static PRUnichar const VALUE_86[] = {
  0x0394
};
static PRUnichar const NAME_87[] = {
  'D', 'f', 'r', ';'
};
static PRUnichar const VALUE_87[] = {
  0xd835, 0xdd07
};
static PRUnichar const NAME_88[] = {
  'D', 'i', 'a', 'c', 'r', 'i', 't', 'i', 'c', 'a', 'l', 'A', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_88[] = {
  0x00b4
};
static PRUnichar const NAME_89[] = {
  'D', 'i', 'a', 'c', 'r', 'i', 't', 'i', 'c', 'a', 'l', 'D', 'o', 't', ';'
};
static PRUnichar const VALUE_89[] = {
  0x02d9
};
static PRUnichar const NAME_90[] = {
  'D', 'i', 'a', 'c', 'r', 'i', 't', 'i', 'c', 'a', 'l', 'D', 'o', 'u', 'b', 'l', 'e', 'A', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_90[] = {
  0x02dd
};
static PRUnichar const NAME_91[] = {
  'D', 'i', 'a', 'c', 'r', 'i', 't', 'i', 'c', 'a', 'l', 'G', 'r', 'a', 'v', 'e', ';'
};
static PRUnichar const VALUE_91[] = {
  0x0060
};
static PRUnichar const NAME_92[] = {
  'D', 'i', 'a', 'c', 'r', 'i', 't', 'i', 'c', 'a', 'l', 'T', 'i', 'l', 'd', 'e', ';'
};
static PRUnichar const VALUE_92[] = {
  0x02dc
};
static PRUnichar const NAME_93[] = {
  'D', 'i', 'a', 'm', 'o', 'n', 'd', ';'
};
static PRUnichar const VALUE_93[] = {
  0x22c4
};
static PRUnichar const NAME_94[] = {
  'D', 'i', 'f', 'f', 'e', 'r', 'e', 'n', 't', 'i', 'a', 'l', 'D', ';'
};
static PRUnichar const VALUE_94[] = {
  0x2146
};
static PRUnichar const NAME_95[] = {
  'D', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_95[] = {
  0xd835, 0xdd3b
};
static PRUnichar const NAME_96[] = {
  'D', 'o', 't', ';'
};
static PRUnichar const VALUE_96[] = {
  0x00a8
};
static PRUnichar const NAME_97[] = {
  'D', 'o', 't', 'D', 'o', 't', ';'
};
static PRUnichar const VALUE_97[] = {
  0x20dc
};
static PRUnichar const NAME_98[] = {
  'D', 'o', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_98[] = {
  0x2250
};
static PRUnichar const NAME_99[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'C', 'o', 'n', 't', 'o', 'u', 'r', 'I', 'n', 't', 'e', 'g', 'r', 'a', 'l', ';'
};
static PRUnichar const VALUE_99[] = {
  0x222f
};
static PRUnichar const NAME_100[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'D', 'o', 't', ';'
};
static PRUnichar const VALUE_100[] = {
  0x00a8
};
static PRUnichar const NAME_101[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'D', 'o', 'w', 'n', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_101[] = {
  0x21d3
};
static PRUnichar const NAME_102[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'L', 'e', 'f', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_102[] = {
  0x21d0
};
static PRUnichar const NAME_103[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'L', 'e', 'f', 't', 'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_103[] = {
  0x21d4
};
static PRUnichar const NAME_104[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'L', 'e', 'f', 't', 'T', 'e', 'e', ';'
};
static PRUnichar const VALUE_104[] = {
  0x2ae4
};
static PRUnichar const NAME_105[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'L', 'o', 'n', 'g', 'L', 'e', 'f', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_105[] = {
  0x27f8
};
static PRUnichar const NAME_106[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'L', 'o', 'n', 'g', 'L', 'e', 'f', 't', 'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_106[] = {
  0x27fa
};
static PRUnichar const NAME_107[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'L', 'o', 'n', 'g', 'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_107[] = {
  0x27f9
};
static PRUnichar const NAME_108[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_108[] = {
  0x21d2
};
static PRUnichar const NAME_109[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'R', 'i', 'g', 'h', 't', 'T', 'e', 'e', ';'
};
static PRUnichar const VALUE_109[] = {
  0x22a8
};
static PRUnichar const NAME_110[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'U', 'p', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_110[] = {
  0x21d1
};
static PRUnichar const NAME_111[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'U', 'p', 'D', 'o', 'w', 'n', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_111[] = {
  0x21d5
};
static PRUnichar const NAME_112[] = {
  'D', 'o', 'u', 'b', 'l', 'e', 'V', 'e', 'r', 't', 'i', 'c', 'a', 'l', 'B', 'a', 'r', ';'
};
static PRUnichar const VALUE_112[] = {
  0x2225
};
static PRUnichar const NAME_113[] = {
  'D', 'o', 'w', 'n', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_113[] = {
  0x2193
};
static PRUnichar const NAME_114[] = {
  'D', 'o', 'w', 'n', 'A', 'r', 'r', 'o', 'w', 'B', 'a', 'r', ';'
};
static PRUnichar const VALUE_114[] = {
  0x2913
};
static PRUnichar const NAME_115[] = {
  'D', 'o', 'w', 'n', 'A', 'r', 'r', 'o', 'w', 'U', 'p', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_115[] = {
  0x21f5
};
static PRUnichar const NAME_116[] = {
  'D', 'o', 'w', 'n', 'B', 'r', 'e', 'v', 'e', ';'
};
static PRUnichar const VALUE_116[] = {
  0x0311
};
static PRUnichar const NAME_117[] = {
  'D', 'o', 'w', 'n', 'L', 'e', 'f', 't', 'R', 'i', 'g', 'h', 't', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
static PRUnichar const VALUE_117[] = {
  0x2950
};
static PRUnichar const NAME_118[] = {
  'D', 'o', 'w', 'n', 'L', 'e', 'f', 't', 'T', 'e', 'e', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
static PRUnichar const VALUE_118[] = {
  0x295e
};
static PRUnichar const NAME_119[] = {
  'D', 'o', 'w', 'n', 'L', 'e', 'f', 't', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
static PRUnichar const VALUE_119[] = {
  0x21bd
};
static PRUnichar const NAME_120[] = {
  'D', 'o', 'w', 'n', 'L', 'e', 'f', 't', 'V', 'e', 'c', 't', 'o', 'r', 'B', 'a', 'r', ';'
};
static PRUnichar const VALUE_120[] = {
  0x2956
};
static PRUnichar const NAME_121[] = {
  'D', 'o', 'w', 'n', 'R', 'i', 'g', 'h', 't', 'T', 'e', 'e', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
static PRUnichar const VALUE_121[] = {
  0x295f
};
static PRUnichar const NAME_122[] = {
  'D', 'o', 'w', 'n', 'R', 'i', 'g', 'h', 't', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
static PRUnichar const VALUE_122[] = {
  0x21c1
};
static PRUnichar const NAME_123[] = {
  'D', 'o', 'w', 'n', 'R', 'i', 'g', 'h', 't', 'V', 'e', 'c', 't', 'o', 'r', 'B', 'a', 'r', ';'
};
static PRUnichar const VALUE_123[] = {
  0x2957
};
static PRUnichar const NAME_124[] = {
  'D', 'o', 'w', 'n', 'T', 'e', 'e', ';'
};
static PRUnichar const VALUE_124[] = {
  0x22a4
};
static PRUnichar const NAME_125[] = {
  'D', 'o', 'w', 'n', 'T', 'e', 'e', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_125[] = {
  0x21a7
};
static PRUnichar const NAME_126[] = {
  'D', 'o', 'w', 'n', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_126[] = {
  0x21d3
};
static PRUnichar const NAME_127[] = {
  'D', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_127[] = {
  0xd835, 0xdc9f
};
static PRUnichar const NAME_128[] = {
  'D', 's', 't', 'r', 'o', 'k', ';'
};
static PRUnichar const VALUE_128[] = {
  0x0110
};
static PRUnichar const NAME_129[] = {
  'E', 'N', 'G', ';'
};
static PRUnichar const VALUE_129[] = {
  0x014a
};
static PRUnichar const NAME_130[] = {
  'E', 'T', 'H'
};
static PRUnichar const VALUE_130[] = {
  0x00d0
};
static PRUnichar const NAME_131[] = {
  'E', 'T', 'H', ';'
};
static PRUnichar const VALUE_131[] = {
  0x00d0
};
static PRUnichar const NAME_132[] = {
  'E', 'a', 'c', 'u', 't', 'e'
};
static PRUnichar const VALUE_132[] = {
  0x00c9
};
static PRUnichar const NAME_133[] = {
  'E', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_133[] = {
  0x00c9
};
static PRUnichar const NAME_134[] = {
  'E', 'c', 'a', 'r', 'o', 'n', ';'
};
static PRUnichar const VALUE_134[] = {
  0x011a
};
static PRUnichar const NAME_135[] = {
  'E', 'c', 'i', 'r', 'c'
};
static PRUnichar const VALUE_135[] = {
  0x00ca
};
static PRUnichar const NAME_136[] = {
  'E', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_136[] = {
  0x00ca
};
static PRUnichar const NAME_137[] = {
  'E', 'c', 'y', ';'
};
static PRUnichar const VALUE_137[] = {
  0x042d
};
static PRUnichar const NAME_138[] = {
  'E', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_138[] = {
  0x0116
};
static PRUnichar const NAME_139[] = {
  'E', 'f', 'r', ';'
};
static PRUnichar const VALUE_139[] = {
  0xd835, 0xdd08
};
static PRUnichar const NAME_140[] = {
  'E', 'g', 'r', 'a', 'v', 'e'
};
static PRUnichar const VALUE_140[] = {
  0x00c8
};
static PRUnichar const NAME_141[] = {
  'E', 'g', 'r', 'a', 'v', 'e', ';'
};
static PRUnichar const VALUE_141[] = {
  0x00c8
};
static PRUnichar const NAME_142[] = {
  'E', 'l', 'e', 'm', 'e', 'n', 't', ';'
};
static PRUnichar const VALUE_142[] = {
  0x2208
};
static PRUnichar const NAME_143[] = {
  'E', 'm', 'a', 'c', 'r', ';'
};
static PRUnichar const VALUE_143[] = {
  0x0112
};
static PRUnichar const NAME_144[] = {
  'E', 'm', 'p', 't', 'y', 'S', 'm', 'a', 'l', 'l', 'S', 'q', 'u', 'a', 'r', 'e', ';'
};
static PRUnichar const VALUE_144[] = {
  0x25fb
};
static PRUnichar const NAME_145[] = {
  'E', 'm', 'p', 't', 'y', 'V', 'e', 'r', 'y', 'S', 'm', 'a', 'l', 'l', 'S', 'q', 'u', 'a', 'r', 'e', ';'
};
static PRUnichar const VALUE_145[] = {
  0x25ab
};
static PRUnichar const NAME_146[] = {
  'E', 'o', 'g', 'o', 'n', ';'
};
static PRUnichar const VALUE_146[] = {
  0x0118
};
static PRUnichar const NAME_147[] = {
  'E', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_147[] = {
  0xd835, 0xdd3c
};
static PRUnichar const NAME_148[] = {
  'E', 'p', 's', 'i', 'l', 'o', 'n', ';'
};
static PRUnichar const VALUE_148[] = {
  0x0395
};
static PRUnichar const NAME_149[] = {
  'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_149[] = {
  0x2a75
};
static PRUnichar const NAME_150[] = {
  'E', 'q', 'u', 'a', 'l', 'T', 'i', 'l', 'd', 'e', ';'
};
static PRUnichar const VALUE_150[] = {
  0x2242
};
static PRUnichar const NAME_151[] = {
  'E', 'q', 'u', 'i', 'l', 'i', 'b', 'r', 'i', 'u', 'm', ';'
};
static PRUnichar const VALUE_151[] = {
  0x21cc
};
static PRUnichar const NAME_152[] = {
  'E', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_152[] = {
  0x2130
};
static PRUnichar const NAME_153[] = {
  'E', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_153[] = {
  0x2a73
};
static PRUnichar const NAME_154[] = {
  'E', 't', 'a', ';'
};
static PRUnichar const VALUE_154[] = {
  0x0397
};
static PRUnichar const NAME_155[] = {
  'E', 'u', 'm', 'l'
};
static PRUnichar const VALUE_155[] = {
  0x00cb
};
static PRUnichar const NAME_156[] = {
  'E', 'u', 'm', 'l', ';'
};
static PRUnichar const VALUE_156[] = {
  0x00cb
};
static PRUnichar const NAME_157[] = {
  'E', 'x', 'i', 's', 't', 's', ';'
};
static PRUnichar const VALUE_157[] = {
  0x2203
};
static PRUnichar const NAME_158[] = {
  'E', 'x', 'p', 'o', 'n', 'e', 'n', 't', 'i', 'a', 'l', 'E', ';'
};
static PRUnichar const VALUE_158[] = {
  0x2147
};
static PRUnichar const NAME_159[] = {
  'F', 'c', 'y', ';'
};
static PRUnichar const VALUE_159[] = {
  0x0424
};
static PRUnichar const NAME_160[] = {
  'F', 'f', 'r', ';'
};
static PRUnichar const VALUE_160[] = {
  0xd835, 0xdd09
};
static PRUnichar const NAME_161[] = {
  'F', 'i', 'l', 'l', 'e', 'd', 'S', 'm', 'a', 'l', 'l', 'S', 'q', 'u', 'a', 'r', 'e', ';'
};
static PRUnichar const VALUE_161[] = {
  0x25fc
};
static PRUnichar const NAME_162[] = {
  'F', 'i', 'l', 'l', 'e', 'd', 'V', 'e', 'r', 'y', 'S', 'm', 'a', 'l', 'l', 'S', 'q', 'u', 'a', 'r', 'e', ';'
};
static PRUnichar const VALUE_162[] = {
  0x25aa
};
static PRUnichar const NAME_163[] = {
  'F', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_163[] = {
  0xd835, 0xdd3d
};
static PRUnichar const NAME_164[] = {
  'F', 'o', 'r', 'A', 'l', 'l', ';'
};
static PRUnichar const VALUE_164[] = {
  0x2200
};
static PRUnichar const NAME_165[] = {
  'F', 'o', 'u', 'r', 'i', 'e', 'r', 't', 'r', 'f', ';'
};
static PRUnichar const VALUE_165[] = {
  0x2131
};
static PRUnichar const NAME_166[] = {
  'F', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_166[] = {
  0x2131
};
static PRUnichar const NAME_167[] = {
  'G', 'J', 'c', 'y', ';'
};
static PRUnichar const VALUE_167[] = {
  0x0403
};
static PRUnichar const NAME_168[] = {
  'G', 'T'
};
static PRUnichar const VALUE_168[] = {
  0x003e
};
static PRUnichar const NAME_169[] = {
  'G', 'T', ';'
};
static PRUnichar const VALUE_169[] = {
  0x003e
};
static PRUnichar const NAME_170[] = {
  'G', 'a', 'm', 'm', 'a', ';'
};
static PRUnichar const VALUE_170[] = {
  0x0393
};
static PRUnichar const NAME_171[] = {
  'G', 'a', 'm', 'm', 'a', 'd', ';'
};
static PRUnichar const VALUE_171[] = {
  0x03dc
};
static PRUnichar const NAME_172[] = {
  'G', 'b', 'r', 'e', 'v', 'e', ';'
};
static PRUnichar const VALUE_172[] = {
  0x011e
};
static PRUnichar const NAME_173[] = {
  'G', 'c', 'e', 'd', 'i', 'l', ';'
};
static PRUnichar const VALUE_173[] = {
  0x0122
};
static PRUnichar const NAME_174[] = {
  'G', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_174[] = {
  0x011c
};
static PRUnichar const NAME_175[] = {
  'G', 'c', 'y', ';'
};
static PRUnichar const VALUE_175[] = {
  0x0413
};
static PRUnichar const NAME_176[] = {
  'G', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_176[] = {
  0x0120
};
static PRUnichar const NAME_177[] = {
  'G', 'f', 'r', ';'
};
static PRUnichar const VALUE_177[] = {
  0xd835, 0xdd0a
};
static PRUnichar const NAME_178[] = {
  'G', 'g', ';'
};
static PRUnichar const VALUE_178[] = {
  0x22d9
};
static PRUnichar const NAME_179[] = {
  'G', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_179[] = {
  0xd835, 0xdd3e
};
static PRUnichar const NAME_180[] = {
  'G', 'r', 'e', 'a', 't', 'e', 'r', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_180[] = {
  0x2265
};
static PRUnichar const NAME_181[] = {
  'G', 'r', 'e', 'a', 't', 'e', 'r', 'E', 'q', 'u', 'a', 'l', 'L', 'e', 's', 's', ';'
};
static PRUnichar const VALUE_181[] = {
  0x22db
};
static PRUnichar const NAME_182[] = {
  'G', 'r', 'e', 'a', 't', 'e', 'r', 'F', 'u', 'l', 'l', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_182[] = {
  0x2267
};
static PRUnichar const NAME_183[] = {
  'G', 'r', 'e', 'a', 't', 'e', 'r', 'G', 'r', 'e', 'a', 't', 'e', 'r', ';'
};
static PRUnichar const VALUE_183[] = {
  0x2aa2
};
static PRUnichar const NAME_184[] = {
  'G', 'r', 'e', 'a', 't', 'e', 'r', 'L', 'e', 's', 's', ';'
};
static PRUnichar const VALUE_184[] = {
  0x2277
};
static PRUnichar const NAME_185[] = {
  'G', 'r', 'e', 'a', 't', 'e', 'r', 'S', 'l', 'a', 'n', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_185[] = {
  0x2a7e
};
static PRUnichar const NAME_186[] = {
  'G', 'r', 'e', 'a', 't', 'e', 'r', 'T', 'i', 'l', 'd', 'e', ';'
};
static PRUnichar const VALUE_186[] = {
  0x2273
};
static PRUnichar const NAME_187[] = {
  'G', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_187[] = {
  0xd835, 0xdca2
};
static PRUnichar const NAME_188[] = {
  'G', 't', ';'
};
static PRUnichar const VALUE_188[] = {
  0x226b
};
static PRUnichar const NAME_189[] = {
  'H', 'A', 'R', 'D', 'c', 'y', ';'
};
static PRUnichar const VALUE_189[] = {
  0x042a
};
static PRUnichar const NAME_190[] = {
  'H', 'a', 'c', 'e', 'k', ';'
};
static PRUnichar const VALUE_190[] = {
  0x02c7
};
static PRUnichar const NAME_191[] = {
  'H', 'a', 't', ';'
};
static PRUnichar const VALUE_191[] = {
  0x005e
};
static PRUnichar const NAME_192[] = {
  'H', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_192[] = {
  0x0124
};
static PRUnichar const NAME_193[] = {
  'H', 'f', 'r', ';'
};
static PRUnichar const VALUE_193[] = {
  0x210c
};
static PRUnichar const NAME_194[] = {
  'H', 'i', 'l', 'b', 'e', 'r', 't', 'S', 'p', 'a', 'c', 'e', ';'
};
static PRUnichar const VALUE_194[] = {
  0x210b
};
static PRUnichar const NAME_195[] = {
  'H', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_195[] = {
  0x210d
};
static PRUnichar const NAME_196[] = {
  'H', 'o', 'r', 'i', 'z', 'o', 'n', 't', 'a', 'l', 'L', 'i', 'n', 'e', ';'
};
static PRUnichar const VALUE_196[] = {
  0x2500
};
static PRUnichar const NAME_197[] = {
  'H', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_197[] = {
  0x210b
};
static PRUnichar const NAME_198[] = {
  'H', 's', 't', 'r', 'o', 'k', ';'
};
static PRUnichar const VALUE_198[] = {
  0x0126
};
static PRUnichar const NAME_199[] = {
  'H', 'u', 'm', 'p', 'D', 'o', 'w', 'n', 'H', 'u', 'm', 'p', ';'
};
static PRUnichar const VALUE_199[] = {
  0x224e
};
static PRUnichar const NAME_200[] = {
  'H', 'u', 'm', 'p', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_200[] = {
  0x224f
};
static PRUnichar const NAME_201[] = {
  'I', 'E', 'c', 'y', ';'
};
static PRUnichar const VALUE_201[] = {
  0x0415
};
static PRUnichar const NAME_202[] = {
  'I', 'J', 'l', 'i', 'g', ';'
};
static PRUnichar const VALUE_202[] = {
  0x0132
};
static PRUnichar const NAME_203[] = {
  'I', 'O', 'c', 'y', ';'
};
static PRUnichar const VALUE_203[] = {
  0x0401
};
static PRUnichar const NAME_204[] = {
  'I', 'a', 'c', 'u', 't', 'e'
};
static PRUnichar const VALUE_204[] = {
  0x00cd
};
static PRUnichar const NAME_205[] = {
  'I', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_205[] = {
  0x00cd
};
static PRUnichar const NAME_206[] = {
  'I', 'c', 'i', 'r', 'c'
};
static PRUnichar const VALUE_206[] = {
  0x00ce
};
static PRUnichar const NAME_207[] = {
  'I', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_207[] = {
  0x00ce
};
static PRUnichar const NAME_208[] = {
  'I', 'c', 'y', ';'
};
static PRUnichar const VALUE_208[] = {
  0x0418
};
static PRUnichar const NAME_209[] = {
  'I', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_209[] = {
  0x0130
};
static PRUnichar const NAME_210[] = {
  'I', 'f', 'r', ';'
};
static PRUnichar const VALUE_210[] = {
  0x2111
};
static PRUnichar const NAME_211[] = {
  'I', 'g', 'r', 'a', 'v', 'e'
};
static PRUnichar const VALUE_211[] = {
  0x00cc
};
static PRUnichar const NAME_212[] = {
  'I', 'g', 'r', 'a', 'v', 'e', ';'
};
static PRUnichar const VALUE_212[] = {
  0x00cc
};
static PRUnichar const NAME_213[] = {
  'I', 'm', ';'
};
static PRUnichar const VALUE_213[] = {
  0x2111
};
static PRUnichar const NAME_214[] = {
  'I', 'm', 'a', 'c', 'r', ';'
};
static PRUnichar const VALUE_214[] = {
  0x012a
};
static PRUnichar const NAME_215[] = {
  'I', 'm', 'a', 'g', 'i', 'n', 'a', 'r', 'y', 'I', ';'
};
static PRUnichar const VALUE_215[] = {
  0x2148
};
static PRUnichar const NAME_216[] = {
  'I', 'm', 'p', 'l', 'i', 'e', 's', ';'
};
static PRUnichar const VALUE_216[] = {
  0x21d2
};
static PRUnichar const NAME_217[] = {
  'I', 'n', 't', ';'
};
static PRUnichar const VALUE_217[] = {
  0x222c
};
static PRUnichar const NAME_218[] = {
  'I', 'n', 't', 'e', 'g', 'r', 'a', 'l', ';'
};
static PRUnichar const VALUE_218[] = {
  0x222b
};
static PRUnichar const NAME_219[] = {
  'I', 'n', 't', 'e', 'r', 's', 'e', 'c', 't', 'i', 'o', 'n', ';'
};
static PRUnichar const VALUE_219[] = {
  0x22c2
};
static PRUnichar const NAME_220[] = {
  'I', 'n', 'v', 'i', 's', 'i', 'b', 'l', 'e', 'C', 'o', 'm', 'm', 'a', ';'
};
static PRUnichar const VALUE_220[] = {
  0x2063
};
static PRUnichar const NAME_221[] = {
  'I', 'n', 'v', 'i', 's', 'i', 'b', 'l', 'e', 'T', 'i', 'm', 'e', 's', ';'
};
static PRUnichar const VALUE_221[] = {
  0x2062
};
static PRUnichar const NAME_222[] = {
  'I', 'o', 'g', 'o', 'n', ';'
};
static PRUnichar const VALUE_222[] = {
  0x012e
};
static PRUnichar const NAME_223[] = {
  'I', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_223[] = {
  0xd835, 0xdd40
};
static PRUnichar const NAME_224[] = {
  'I', 'o', 't', 'a', ';'
};
static PRUnichar const VALUE_224[] = {
  0x0399
};
static PRUnichar const NAME_225[] = {
  'I', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_225[] = {
  0x2110
};
static PRUnichar const NAME_226[] = {
  'I', 't', 'i', 'l', 'd', 'e', ';'
};
static PRUnichar const VALUE_226[] = {
  0x0128
};
static PRUnichar const NAME_227[] = {
  'I', 'u', 'k', 'c', 'y', ';'
};
static PRUnichar const VALUE_227[] = {
  0x0406
};
static PRUnichar const NAME_228[] = {
  'I', 'u', 'm', 'l'
};
static PRUnichar const VALUE_228[] = {
  0x00cf
};
static PRUnichar const NAME_229[] = {
  'I', 'u', 'm', 'l', ';'
};
static PRUnichar const VALUE_229[] = {
  0x00cf
};
static PRUnichar const NAME_230[] = {
  'J', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_230[] = {
  0x0134
};
static PRUnichar const NAME_231[] = {
  'J', 'c', 'y', ';'
};
static PRUnichar const VALUE_231[] = {
  0x0419
};
static PRUnichar const NAME_232[] = {
  'J', 'f', 'r', ';'
};
static PRUnichar const VALUE_232[] = {
  0xd835, 0xdd0d
};
static PRUnichar const NAME_233[] = {
  'J', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_233[] = {
  0xd835, 0xdd41
};
static PRUnichar const NAME_234[] = {
  'J', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_234[] = {
  0xd835, 0xdca5
};
static PRUnichar const NAME_235[] = {
  'J', 's', 'e', 'r', 'c', 'y', ';'
};
static PRUnichar const VALUE_235[] = {
  0x0408
};
static PRUnichar const NAME_236[] = {
  'J', 'u', 'k', 'c', 'y', ';'
};
static PRUnichar const VALUE_236[] = {
  0x0404
};
static PRUnichar const NAME_237[] = {
  'K', 'H', 'c', 'y', ';'
};
static PRUnichar const VALUE_237[] = {
  0x0425
};
static PRUnichar const NAME_238[] = {
  'K', 'J', 'c', 'y', ';'
};
static PRUnichar const VALUE_238[] = {
  0x040c
};
static PRUnichar const NAME_239[] = {
  'K', 'a', 'p', 'p', 'a', ';'
};
static PRUnichar const VALUE_239[] = {
  0x039a
};
static PRUnichar const NAME_240[] = {
  'K', 'c', 'e', 'd', 'i', 'l', ';'
};
static PRUnichar const VALUE_240[] = {
  0x0136
};
static PRUnichar const NAME_241[] = {
  'K', 'c', 'y', ';'
};
static PRUnichar const VALUE_241[] = {
  0x041a
};
static PRUnichar const NAME_242[] = {
  'K', 'f', 'r', ';'
};
static PRUnichar const VALUE_242[] = {
  0xd835, 0xdd0e
};
static PRUnichar const NAME_243[] = {
  'K', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_243[] = {
  0xd835, 0xdd42
};
static PRUnichar const NAME_244[] = {
  'K', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_244[] = {
  0xd835, 0xdca6
};
static PRUnichar const NAME_245[] = {
  'L', 'J', 'c', 'y', ';'
};
static PRUnichar const VALUE_245[] = {
  0x0409
};
static PRUnichar const NAME_246[] = {
  'L', 'T'
};
static PRUnichar const VALUE_246[] = {
  0x003c
};
static PRUnichar const NAME_247[] = {
  'L', 'T', ';'
};
static PRUnichar const VALUE_247[] = {
  0x003c
};
static PRUnichar const NAME_248[] = {
  'L', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_248[] = {
  0x0139
};
static PRUnichar const NAME_249[] = {
  'L', 'a', 'm', 'b', 'd', 'a', ';'
};
static PRUnichar const VALUE_249[] = {
  0x039b
};
static PRUnichar const NAME_250[] = {
  'L', 'a', 'n', 'g', ';'
};
static PRUnichar const VALUE_250[] = {
  0x27ea
};
static PRUnichar const NAME_251[] = {
  'L', 'a', 'p', 'l', 'a', 'c', 'e', 't', 'r', 'f', ';'
};
static PRUnichar const VALUE_251[] = {
  0x2112
};
static PRUnichar const NAME_252[] = {
  'L', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_252[] = {
  0x219e
};
static PRUnichar const NAME_253[] = {
  'L', 'c', 'a', 'r', 'o', 'n', ';'
};
static PRUnichar const VALUE_253[] = {
  0x013d
};
static PRUnichar const NAME_254[] = {
  'L', 'c', 'e', 'd', 'i', 'l', ';'
};
static PRUnichar const VALUE_254[] = {
  0x013b
};
static PRUnichar const NAME_255[] = {
  'L', 'c', 'y', ';'
};
static PRUnichar const VALUE_255[] = {
  0x041b
};
static PRUnichar const NAME_256[] = {
  'L', 'e', 'f', 't', 'A', 'n', 'g', 'l', 'e', 'B', 'r', 'a', 'c', 'k', 'e', 't', ';'
};
static PRUnichar const VALUE_256[] = {
  0x27e8
};
static PRUnichar const NAME_257[] = {
  'L', 'e', 'f', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_257[] = {
  0x2190
};
static PRUnichar const NAME_258[] = {
  'L', 'e', 'f', 't', 'A', 'r', 'r', 'o', 'w', 'B', 'a', 'r', ';'
};
static PRUnichar const VALUE_258[] = {
  0x21e4
};
static PRUnichar const NAME_259[] = {
  'L', 'e', 'f', 't', 'A', 'r', 'r', 'o', 'w', 'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_259[] = {
  0x21c6
};
static PRUnichar const NAME_260[] = {
  'L', 'e', 'f', 't', 'C', 'e', 'i', 'l', 'i', 'n', 'g', ';'
};
static PRUnichar const VALUE_260[] = {
  0x2308
};
static PRUnichar const NAME_261[] = {
  'L', 'e', 'f', 't', 'D', 'o', 'u', 'b', 'l', 'e', 'B', 'r', 'a', 'c', 'k', 'e', 't', ';'
};
static PRUnichar const VALUE_261[] = {
  0x27e6
};
static PRUnichar const NAME_262[] = {
  'L', 'e', 'f', 't', 'D', 'o', 'w', 'n', 'T', 'e', 'e', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
static PRUnichar const VALUE_262[] = {
  0x2961
};
static PRUnichar const NAME_263[] = {
  'L', 'e', 'f', 't', 'D', 'o', 'w', 'n', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
static PRUnichar const VALUE_263[] = {
  0x21c3
};
static PRUnichar const NAME_264[] = {
  'L', 'e', 'f', 't', 'D', 'o', 'w', 'n', 'V', 'e', 'c', 't', 'o', 'r', 'B', 'a', 'r', ';'
};
static PRUnichar const VALUE_264[] = {
  0x2959
};
static PRUnichar const NAME_265[] = {
  'L', 'e', 'f', 't', 'F', 'l', 'o', 'o', 'r', ';'
};
static PRUnichar const VALUE_265[] = {
  0x230a
};
static PRUnichar const NAME_266[] = {
  'L', 'e', 'f', 't', 'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_266[] = {
  0x2194
};
static PRUnichar const NAME_267[] = {
  'L', 'e', 'f', 't', 'R', 'i', 'g', 'h', 't', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
static PRUnichar const VALUE_267[] = {
  0x294e
};
static PRUnichar const NAME_268[] = {
  'L', 'e', 'f', 't', 'T', 'e', 'e', ';'
};
static PRUnichar const VALUE_268[] = {
  0x22a3
};
static PRUnichar const NAME_269[] = {
  'L', 'e', 'f', 't', 'T', 'e', 'e', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_269[] = {
  0x21a4
};
static PRUnichar const NAME_270[] = {
  'L', 'e', 'f', 't', 'T', 'e', 'e', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
static PRUnichar const VALUE_270[] = {
  0x295a
};
static PRUnichar const NAME_271[] = {
  'L', 'e', 'f', 't', 'T', 'r', 'i', 'a', 'n', 'g', 'l', 'e', ';'
};
static PRUnichar const VALUE_271[] = {
  0x22b2
};
static PRUnichar const NAME_272[] = {
  'L', 'e', 'f', 't', 'T', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'B', 'a', 'r', ';'
};
static PRUnichar const VALUE_272[] = {
  0x29cf
};
static PRUnichar const NAME_273[] = {
  'L', 'e', 'f', 't', 'T', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_273[] = {
  0x22b4
};
static PRUnichar const NAME_274[] = {
  'L', 'e', 'f', 't', 'U', 'p', 'D', 'o', 'w', 'n', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
static PRUnichar const VALUE_274[] = {
  0x2951
};
static PRUnichar const NAME_275[] = {
  'L', 'e', 'f', 't', 'U', 'p', 'T', 'e', 'e', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
static PRUnichar const VALUE_275[] = {
  0x2960
};
static PRUnichar const NAME_276[] = {
  'L', 'e', 'f', 't', 'U', 'p', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
static PRUnichar const VALUE_276[] = {
  0x21bf
};
static PRUnichar const NAME_277[] = {
  'L', 'e', 'f', 't', 'U', 'p', 'V', 'e', 'c', 't', 'o', 'r', 'B', 'a', 'r', ';'
};
static PRUnichar const VALUE_277[] = {
  0x2958
};
static PRUnichar const NAME_278[] = {
  'L', 'e', 'f', 't', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
static PRUnichar const VALUE_278[] = {
  0x21bc
};
static PRUnichar const NAME_279[] = {
  'L', 'e', 'f', 't', 'V', 'e', 'c', 't', 'o', 'r', 'B', 'a', 'r', ';'
};
static PRUnichar const VALUE_279[] = {
  0x2952
};
static PRUnichar const NAME_280[] = {
  'L', 'e', 'f', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_280[] = {
  0x21d0
};
static PRUnichar const NAME_281[] = {
  'L', 'e', 'f', 't', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_281[] = {
  0x21d4
};
static PRUnichar const NAME_282[] = {
  'L', 'e', 's', 's', 'E', 'q', 'u', 'a', 'l', 'G', 'r', 'e', 'a', 't', 'e', 'r', ';'
};
static PRUnichar const VALUE_282[] = {
  0x22da
};
static PRUnichar const NAME_283[] = {
  'L', 'e', 's', 's', 'F', 'u', 'l', 'l', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_283[] = {
  0x2266
};
static PRUnichar const NAME_284[] = {
  'L', 'e', 's', 's', 'G', 'r', 'e', 'a', 't', 'e', 'r', ';'
};
static PRUnichar const VALUE_284[] = {
  0x2276
};
static PRUnichar const NAME_285[] = {
  'L', 'e', 's', 's', 'L', 'e', 's', 's', ';'
};
static PRUnichar const VALUE_285[] = {
  0x2aa1
};
static PRUnichar const NAME_286[] = {
  'L', 'e', 's', 's', 'S', 'l', 'a', 'n', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_286[] = {
  0x2a7d
};
static PRUnichar const NAME_287[] = {
  'L', 'e', 's', 's', 'T', 'i', 'l', 'd', 'e', ';'
};
static PRUnichar const VALUE_287[] = {
  0x2272
};
static PRUnichar const NAME_288[] = {
  'L', 'f', 'r', ';'
};
static PRUnichar const VALUE_288[] = {
  0xd835, 0xdd0f
};
static PRUnichar const NAME_289[] = {
  'L', 'l', ';'
};
static PRUnichar const VALUE_289[] = {
  0x22d8
};
static PRUnichar const NAME_290[] = {
  'L', 'l', 'e', 'f', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_290[] = {
  0x21da
};
static PRUnichar const NAME_291[] = {
  'L', 'm', 'i', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_291[] = {
  0x013f
};
static PRUnichar const NAME_292[] = {
  'L', 'o', 'n', 'g', 'L', 'e', 'f', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_292[] = {
  0x27f5
};
static PRUnichar const NAME_293[] = {
  'L', 'o', 'n', 'g', 'L', 'e', 'f', 't', 'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_293[] = {
  0x27f7
};
static PRUnichar const NAME_294[] = {
  'L', 'o', 'n', 'g', 'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_294[] = {
  0x27f6
};
static PRUnichar const NAME_295[] = {
  'L', 'o', 'n', 'g', 'l', 'e', 'f', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_295[] = {
  0x27f8
};
static PRUnichar const NAME_296[] = {
  'L', 'o', 'n', 'g', 'l', 'e', 'f', 't', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_296[] = {
  0x27fa
};
static PRUnichar const NAME_297[] = {
  'L', 'o', 'n', 'g', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_297[] = {
  0x27f9
};
static PRUnichar const NAME_298[] = {
  'L', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_298[] = {
  0xd835, 0xdd43
};
static PRUnichar const NAME_299[] = {
  'L', 'o', 'w', 'e', 'r', 'L', 'e', 'f', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_299[] = {
  0x2199
};
static PRUnichar const NAME_300[] = {
  'L', 'o', 'w', 'e', 'r', 'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_300[] = {
  0x2198
};
static PRUnichar const NAME_301[] = {
  'L', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_301[] = {
  0x2112
};
static PRUnichar const NAME_302[] = {
  'L', 's', 'h', ';'
};
static PRUnichar const VALUE_302[] = {
  0x21b0
};
static PRUnichar const NAME_303[] = {
  'L', 's', 't', 'r', 'o', 'k', ';'
};
static PRUnichar const VALUE_303[] = {
  0x0141
};
static PRUnichar const NAME_304[] = {
  'L', 't', ';'
};
static PRUnichar const VALUE_304[] = {
  0x226a
};
static PRUnichar const NAME_305[] = {
  'M', 'a', 'p', ';'
};
static PRUnichar const VALUE_305[] = {
  0x2905
};
static PRUnichar const NAME_306[] = {
  'M', 'c', 'y', ';'
};
static PRUnichar const VALUE_306[] = {
  0x041c
};
static PRUnichar const NAME_307[] = {
  'M', 'e', 'd', 'i', 'u', 'm', 'S', 'p', 'a', 'c', 'e', ';'
};
static PRUnichar const VALUE_307[] = {
  0x205f
};
static PRUnichar const NAME_308[] = {
  'M', 'e', 'l', 'l', 'i', 'n', 't', 'r', 'f', ';'
};
static PRUnichar const VALUE_308[] = {
  0x2133
};
static PRUnichar const NAME_309[] = {
  'M', 'f', 'r', ';'
};
static PRUnichar const VALUE_309[] = {
  0xd835, 0xdd10
};
static PRUnichar const NAME_310[] = {
  'M', 'i', 'n', 'u', 's', 'P', 'l', 'u', 's', ';'
};
static PRUnichar const VALUE_310[] = {
  0x2213
};
static PRUnichar const NAME_311[] = {
  'M', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_311[] = {
  0xd835, 0xdd44
};
static PRUnichar const NAME_312[] = {
  'M', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_312[] = {
  0x2133
};
static PRUnichar const NAME_313[] = {
  'M', 'u', ';'
};
static PRUnichar const VALUE_313[] = {
  0x039c
};
static PRUnichar const NAME_314[] = {
  'N', 'J', 'c', 'y', ';'
};
static PRUnichar const VALUE_314[] = {
  0x040a
};
static PRUnichar const NAME_315[] = {
  'N', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_315[] = {
  0x0143
};
static PRUnichar const NAME_316[] = {
  'N', 'c', 'a', 'r', 'o', 'n', ';'
};
static PRUnichar const VALUE_316[] = {
  0x0147
};
static PRUnichar const NAME_317[] = {
  'N', 'c', 'e', 'd', 'i', 'l', ';'
};
static PRUnichar const VALUE_317[] = {
  0x0145
};
static PRUnichar const NAME_318[] = {
  'N', 'c', 'y', ';'
};
static PRUnichar const VALUE_318[] = {
  0x041d
};
static PRUnichar const NAME_319[] = {
  'N', 'e', 'g', 'a', 't', 'i', 'v', 'e', 'M', 'e', 'd', 'i', 'u', 'm', 'S', 'p', 'a', 'c', 'e', ';'
};
static PRUnichar const VALUE_319[] = {
  0x200b
};
static PRUnichar const NAME_320[] = {
  'N', 'e', 'g', 'a', 't', 'i', 'v', 'e', 'T', 'h', 'i', 'c', 'k', 'S', 'p', 'a', 'c', 'e', ';'
};
static PRUnichar const VALUE_320[] = {
  0x200b
};
static PRUnichar const NAME_321[] = {
  'N', 'e', 'g', 'a', 't', 'i', 'v', 'e', 'T', 'h', 'i', 'n', 'S', 'p', 'a', 'c', 'e', ';'
};
static PRUnichar const VALUE_321[] = {
  0x200b
};
static PRUnichar const NAME_322[] = {
  'N', 'e', 'g', 'a', 't', 'i', 'v', 'e', 'V', 'e', 'r', 'y', 'T', 'h', 'i', 'n', 'S', 'p', 'a', 'c', 'e', ';'
};
static PRUnichar const VALUE_322[] = {
  0x200b
};
static PRUnichar const NAME_323[] = {
  'N', 'e', 's', 't', 'e', 'd', 'G', 'r', 'e', 'a', 't', 'e', 'r', 'G', 'r', 'e', 'a', 't', 'e', 'r', ';'
};
static PRUnichar const VALUE_323[] = {
  0x226b
};
static PRUnichar const NAME_324[] = {
  'N', 'e', 's', 't', 'e', 'd', 'L', 'e', 's', 's', 'L', 'e', 's', 's', ';'
};
static PRUnichar const VALUE_324[] = {
  0x226a
};
static PRUnichar const NAME_325[] = {
  'N', 'e', 'w', 'L', 'i', 'n', 'e', ';'
};
static PRUnichar const VALUE_325[] = {
  0x000a
};
static PRUnichar const NAME_326[] = {
  'N', 'f', 'r', ';'
};
static PRUnichar const VALUE_326[] = {
  0xd835, 0xdd11
};
static PRUnichar const NAME_327[] = {
  'N', 'o', 'B', 'r', 'e', 'a', 'k', ';'
};
static PRUnichar const VALUE_327[] = {
  0x2060
};
static PRUnichar const NAME_328[] = {
  'N', 'o', 'n', 'B', 'r', 'e', 'a', 'k', 'i', 'n', 'g', 'S', 'p', 'a', 'c', 'e', ';'
};
static PRUnichar const VALUE_328[] = {
  0x00a0
};
static PRUnichar const NAME_329[] = {
  'N', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_329[] = {
  0x2115
};
static PRUnichar const NAME_330[] = {
  'N', 'o', 't', ';'
};
static PRUnichar const VALUE_330[] = {
  0x2aec
};
static PRUnichar const NAME_331[] = {
  'N', 'o', 't', 'C', 'o', 'n', 'g', 'r', 'u', 'e', 'n', 't', ';'
};
static PRUnichar const VALUE_331[] = {
  0x2262
};
static PRUnichar const NAME_332[] = {
  'N', 'o', 't', 'C', 'u', 'p', 'C', 'a', 'p', ';'
};
static PRUnichar const VALUE_332[] = {
  0x226d
};
static PRUnichar const NAME_333[] = {
  'N', 'o', 't', 'D', 'o', 'u', 'b', 'l', 'e', 'V', 'e', 'r', 't', 'i', 'c', 'a', 'l', 'B', 'a', 'r', ';'
};
static PRUnichar const VALUE_333[] = {
  0x2226
};
static PRUnichar const NAME_334[] = {
  'N', 'o', 't', 'E', 'l', 'e', 'm', 'e', 'n', 't', ';'
};
static PRUnichar const VALUE_334[] = {
  0x2209
};
static PRUnichar const NAME_335[] = {
  'N', 'o', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_335[] = {
  0x2260
};
static PRUnichar const NAME_336[] = {
  'N', 'o', 't', 'E', 'x', 'i', 's', 't', 's', ';'
};
static PRUnichar const VALUE_336[] = {
  0x2204
};
static PRUnichar const NAME_337[] = {
  'N', 'o', 't', 'G', 'r', 'e', 'a', 't', 'e', 'r', ';'
};
static PRUnichar const VALUE_337[] = {
  0x226f
};
static PRUnichar const NAME_338[] = {
  'N', 'o', 't', 'G', 'r', 'e', 'a', 't', 'e', 'r', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_338[] = {
  0x2271
};
static PRUnichar const NAME_339[] = {
  'N', 'o', 't', 'G', 'r', 'e', 'a', 't', 'e', 'r', 'L', 'e', 's', 's', ';'
};
static PRUnichar const VALUE_339[] = {
  0x2279
};
static PRUnichar const NAME_340[] = {
  'N', 'o', 't', 'G', 'r', 'e', 'a', 't', 'e', 'r', 'T', 'i', 'l', 'd', 'e', ';'
};
static PRUnichar const VALUE_340[] = {
  0x2275
};
static PRUnichar const NAME_341[] = {
  'N', 'o', 't', 'L', 'e', 'f', 't', 'T', 'r', 'i', 'a', 'n', 'g', 'l', 'e', ';'
};
static PRUnichar const VALUE_341[] = {
  0x22ea
};
static PRUnichar const NAME_342[] = {
  'N', 'o', 't', 'L', 'e', 'f', 't', 'T', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_342[] = {
  0x22ec
};
static PRUnichar const NAME_343[] = {
  'N', 'o', 't', 'L', 'e', 's', 's', ';'
};
static PRUnichar const VALUE_343[] = {
  0x226e
};
static PRUnichar const NAME_344[] = {
  'N', 'o', 't', 'L', 'e', 's', 's', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_344[] = {
  0x2270
};
static PRUnichar const NAME_345[] = {
  'N', 'o', 't', 'L', 'e', 's', 's', 'G', 'r', 'e', 'a', 't', 'e', 'r', ';'
};
static PRUnichar const VALUE_345[] = {
  0x2278
};
static PRUnichar const NAME_346[] = {
  'N', 'o', 't', 'L', 'e', 's', 's', 'T', 'i', 'l', 'd', 'e', ';'
};
static PRUnichar const VALUE_346[] = {
  0x2274
};
static PRUnichar const NAME_347[] = {
  'N', 'o', 't', 'P', 'r', 'e', 'c', 'e', 'd', 'e', 's', ';'
};
static PRUnichar const VALUE_347[] = {
  0x2280
};
static PRUnichar const NAME_348[] = {
  'N', 'o', 't', 'P', 'r', 'e', 'c', 'e', 'd', 'e', 's', 'S', 'l', 'a', 'n', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_348[] = {
  0x22e0
};
static PRUnichar const NAME_349[] = {
  'N', 'o', 't', 'R', 'e', 'v', 'e', 'r', 's', 'e', 'E', 'l', 'e', 'm', 'e', 'n', 't', ';'
};
static PRUnichar const VALUE_349[] = {
  0x220c
};
static PRUnichar const NAME_350[] = {
  'N', 'o', 't', 'R', 'i', 'g', 'h', 't', 'T', 'r', 'i', 'a', 'n', 'g', 'l', 'e', ';'
};
static PRUnichar const VALUE_350[] = {
  0x22eb
};
static PRUnichar const NAME_351[] = {
  'N', 'o', 't', 'R', 'i', 'g', 'h', 't', 'T', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_351[] = {
  0x22ed
};
static PRUnichar const NAME_352[] = {
  'N', 'o', 't', 'S', 'q', 'u', 'a', 'r', 'e', 'S', 'u', 'b', 's', 'e', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_352[] = {
  0x22e2
};
static PRUnichar const NAME_353[] = {
  'N', 'o', 't', 'S', 'q', 'u', 'a', 'r', 'e', 'S', 'u', 'p', 'e', 'r', 's', 'e', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_353[] = {
  0x22e3
};
static PRUnichar const NAME_354[] = {
  'N', 'o', 't', 'S', 'u', 'b', 's', 'e', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_354[] = {
  0x2288
};
static PRUnichar const NAME_355[] = {
  'N', 'o', 't', 'S', 'u', 'c', 'c', 'e', 'e', 'd', 's', ';'
};
static PRUnichar const VALUE_355[] = {
  0x2281
};
static PRUnichar const NAME_356[] = {
  'N', 'o', 't', 'S', 'u', 'c', 'c', 'e', 'e', 'd', 's', 'S', 'l', 'a', 'n', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_356[] = {
  0x22e1
};
static PRUnichar const NAME_357[] = {
  'N', 'o', 't', 'S', 'u', 'p', 'e', 'r', 's', 'e', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_357[] = {
  0x2289
};
static PRUnichar const NAME_358[] = {
  'N', 'o', 't', 'T', 'i', 'l', 'd', 'e', ';'
};
static PRUnichar const VALUE_358[] = {
  0x2241
};
static PRUnichar const NAME_359[] = {
  'N', 'o', 't', 'T', 'i', 'l', 'd', 'e', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_359[] = {
  0x2244
};
static PRUnichar const NAME_360[] = {
  'N', 'o', 't', 'T', 'i', 'l', 'd', 'e', 'F', 'u', 'l', 'l', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_360[] = {
  0x2247
};
static PRUnichar const NAME_361[] = {
  'N', 'o', 't', 'T', 'i', 'l', 'd', 'e', 'T', 'i', 'l', 'd', 'e', ';'
};
static PRUnichar const VALUE_361[] = {
  0x2249
};
static PRUnichar const NAME_362[] = {
  'N', 'o', 't', 'V', 'e', 'r', 't', 'i', 'c', 'a', 'l', 'B', 'a', 'r', ';'
};
static PRUnichar const VALUE_362[] = {
  0x2224
};
static PRUnichar const NAME_363[] = {
  'N', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_363[] = {
  0xd835, 0xdca9
};
static PRUnichar const NAME_364[] = {
  'N', 't', 'i', 'l', 'd', 'e'
};
static PRUnichar const VALUE_364[] = {
  0x00d1
};
static PRUnichar const NAME_365[] = {
  'N', 't', 'i', 'l', 'd', 'e', ';'
};
static PRUnichar const VALUE_365[] = {
  0x00d1
};
static PRUnichar const NAME_366[] = {
  'N', 'u', ';'
};
static PRUnichar const VALUE_366[] = {
  0x039d
};
static PRUnichar const NAME_367[] = {
  'O', 'E', 'l', 'i', 'g', ';'
};
static PRUnichar const VALUE_367[] = {
  0x0152
};
static PRUnichar const NAME_368[] = {
  'O', 'a', 'c', 'u', 't', 'e'
};
static PRUnichar const VALUE_368[] = {
  0x00d3
};
static PRUnichar const NAME_369[] = {
  'O', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_369[] = {
  0x00d3
};
static PRUnichar const NAME_370[] = {
  'O', 'c', 'i', 'r', 'c'
};
static PRUnichar const VALUE_370[] = {
  0x00d4
};
static PRUnichar const NAME_371[] = {
  'O', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_371[] = {
  0x00d4
};
static PRUnichar const NAME_372[] = {
  'O', 'c', 'y', ';'
};
static PRUnichar const VALUE_372[] = {
  0x041e
};
static PRUnichar const NAME_373[] = {
  'O', 'd', 'b', 'l', 'a', 'c', ';'
};
static PRUnichar const VALUE_373[] = {
  0x0150
};
static PRUnichar const NAME_374[] = {
  'O', 'f', 'r', ';'
};
static PRUnichar const VALUE_374[] = {
  0xd835, 0xdd12
};
static PRUnichar const NAME_375[] = {
  'O', 'g', 'r', 'a', 'v', 'e'
};
static PRUnichar const VALUE_375[] = {
  0x00d2
};
static PRUnichar const NAME_376[] = {
  'O', 'g', 'r', 'a', 'v', 'e', ';'
};
static PRUnichar const VALUE_376[] = {
  0x00d2
};
static PRUnichar const NAME_377[] = {
  'O', 'm', 'a', 'c', 'r', ';'
};
static PRUnichar const VALUE_377[] = {
  0x014c
};
static PRUnichar const NAME_378[] = {
  'O', 'm', 'e', 'g', 'a', ';'
};
static PRUnichar const VALUE_378[] = {
  0x03a9
};
static PRUnichar const NAME_379[] = {
  'O', 'm', 'i', 'c', 'r', 'o', 'n', ';'
};
static PRUnichar const VALUE_379[] = {
  0x039f
};
static PRUnichar const NAME_380[] = {
  'O', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_380[] = {
  0xd835, 0xdd46
};
static PRUnichar const NAME_381[] = {
  'O', 'p', 'e', 'n', 'C', 'u', 'r', 'l', 'y', 'D', 'o', 'u', 'b', 'l', 'e', 'Q', 'u', 'o', 't', 'e', ';'
};
static PRUnichar const VALUE_381[] = {
  0x201c
};
static PRUnichar const NAME_382[] = {
  'O', 'p', 'e', 'n', 'C', 'u', 'r', 'l', 'y', 'Q', 'u', 'o', 't', 'e', ';'
};
static PRUnichar const VALUE_382[] = {
  0x2018
};
static PRUnichar const NAME_383[] = {
  'O', 'r', ';'
};
static PRUnichar const VALUE_383[] = {
  0x2a54
};
static PRUnichar const NAME_384[] = {
  'O', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_384[] = {
  0xd835, 0xdcaa
};
static PRUnichar const NAME_385[] = {
  'O', 's', 'l', 'a', 's', 'h'
};
static PRUnichar const VALUE_385[] = {
  0x00d8
};
static PRUnichar const NAME_386[] = {
  'O', 's', 'l', 'a', 's', 'h', ';'
};
static PRUnichar const VALUE_386[] = {
  0x00d8
};
static PRUnichar const NAME_387[] = {
  'O', 't', 'i', 'l', 'd', 'e'
};
static PRUnichar const VALUE_387[] = {
  0x00d5
};
static PRUnichar const NAME_388[] = {
  'O', 't', 'i', 'l', 'd', 'e', ';'
};
static PRUnichar const VALUE_388[] = {
  0x00d5
};
static PRUnichar const NAME_389[] = {
  'O', 't', 'i', 'm', 'e', 's', ';'
};
static PRUnichar const VALUE_389[] = {
  0x2a37
};
static PRUnichar const NAME_390[] = {
  'O', 'u', 'm', 'l'
};
static PRUnichar const VALUE_390[] = {
  0x00d6
};
static PRUnichar const NAME_391[] = {
  'O', 'u', 'm', 'l', ';'
};
static PRUnichar const VALUE_391[] = {
  0x00d6
};
static PRUnichar const NAME_392[] = {
  'O', 'v', 'e', 'r', 'B', 'a', 'r', ';'
};
static PRUnichar const VALUE_392[] = {
  0x00af
};
static PRUnichar const NAME_393[] = {
  'O', 'v', 'e', 'r', 'B', 'r', 'a', 'c', 'e', ';'
};
static PRUnichar const VALUE_393[] = {
  0x23de
};
static PRUnichar const NAME_394[] = {
  'O', 'v', 'e', 'r', 'B', 'r', 'a', 'c', 'k', 'e', 't', ';'
};
static PRUnichar const VALUE_394[] = {
  0x23b4
};
static PRUnichar const NAME_395[] = {
  'O', 'v', 'e', 'r', 'P', 'a', 'r', 'e', 'n', 't', 'h', 'e', 's', 'i', 's', ';'
};
static PRUnichar const VALUE_395[] = {
  0x23dc
};
static PRUnichar const NAME_396[] = {
  'P', 'a', 'r', 't', 'i', 'a', 'l', 'D', ';'
};
static PRUnichar const VALUE_396[] = {
  0x2202
};
static PRUnichar const NAME_397[] = {
  'P', 'c', 'y', ';'
};
static PRUnichar const VALUE_397[] = {
  0x041f
};
static PRUnichar const NAME_398[] = {
  'P', 'f', 'r', ';'
};
static PRUnichar const VALUE_398[] = {
  0xd835, 0xdd13
};
static PRUnichar const NAME_399[] = {
  'P', 'h', 'i', ';'
};
static PRUnichar const VALUE_399[] = {
  0x03a6
};
static PRUnichar const NAME_400[] = {
  'P', 'i', ';'
};
static PRUnichar const VALUE_400[] = {
  0x03a0
};
static PRUnichar const NAME_401[] = {
  'P', 'l', 'u', 's', 'M', 'i', 'n', 'u', 's', ';'
};
static PRUnichar const VALUE_401[] = {
  0x00b1
};
static PRUnichar const NAME_402[] = {
  'P', 'o', 'i', 'n', 'c', 'a', 'r', 'e', 'p', 'l', 'a', 'n', 'e', ';'
};
static PRUnichar const VALUE_402[] = {
  0x210c
};
static PRUnichar const NAME_403[] = {
  'P', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_403[] = {
  0x2119
};
static PRUnichar const NAME_404[] = {
  'P', 'r', ';'
};
static PRUnichar const VALUE_404[] = {
  0x2abb
};
static PRUnichar const NAME_405[] = {
  'P', 'r', 'e', 'c', 'e', 'd', 'e', 's', ';'
};
static PRUnichar const VALUE_405[] = {
  0x227a
};
static PRUnichar const NAME_406[] = {
  'P', 'r', 'e', 'c', 'e', 'd', 'e', 's', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_406[] = {
  0x2aaf
};
static PRUnichar const NAME_407[] = {
  'P', 'r', 'e', 'c', 'e', 'd', 'e', 's', 'S', 'l', 'a', 'n', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_407[] = {
  0x227c
};
static PRUnichar const NAME_408[] = {
  'P', 'r', 'e', 'c', 'e', 'd', 'e', 's', 'T', 'i', 'l', 'd', 'e', ';'
};
static PRUnichar const VALUE_408[] = {
  0x227e
};
static PRUnichar const NAME_409[] = {
  'P', 'r', 'i', 'm', 'e', ';'
};
static PRUnichar const VALUE_409[] = {
  0x2033
};
static PRUnichar const NAME_410[] = {
  'P', 'r', 'o', 'd', 'u', 'c', 't', ';'
};
static PRUnichar const VALUE_410[] = {
  0x220f
};
static PRUnichar const NAME_411[] = {
  'P', 'r', 'o', 'p', 'o', 'r', 't', 'i', 'o', 'n', ';'
};
static PRUnichar const VALUE_411[] = {
  0x2237
};
static PRUnichar const NAME_412[] = {
  'P', 'r', 'o', 'p', 'o', 'r', 't', 'i', 'o', 'n', 'a', 'l', ';'
};
static PRUnichar const VALUE_412[] = {
  0x221d
};
static PRUnichar const NAME_413[] = {
  'P', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_413[] = {
  0xd835, 0xdcab
};
static PRUnichar const NAME_414[] = {
  'P', 's', 'i', ';'
};
static PRUnichar const VALUE_414[] = {
  0x03a8
};
static PRUnichar const NAME_415[] = {
  'Q', 'U', 'O', 'T'
};
static PRUnichar const VALUE_415[] = {
  0x0022
};
static PRUnichar const NAME_416[] = {
  'Q', 'U', 'O', 'T', ';'
};
static PRUnichar const VALUE_416[] = {
  0x0022
};
static PRUnichar const NAME_417[] = {
  'Q', 'f', 'r', ';'
};
static PRUnichar const VALUE_417[] = {
  0xd835, 0xdd14
};
static PRUnichar const NAME_418[] = {
  'Q', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_418[] = {
  0x211a
};
static PRUnichar const NAME_419[] = {
  'Q', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_419[] = {
  0xd835, 0xdcac
};
static PRUnichar const NAME_420[] = {
  'R', 'B', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_420[] = {
  0x2910
};
static PRUnichar const NAME_421[] = {
  'R', 'E', 'G'
};
static PRUnichar const VALUE_421[] = {
  0x00ae
};
static PRUnichar const NAME_422[] = {
  'R', 'E', 'G', ';'
};
static PRUnichar const VALUE_422[] = {
  0x00ae
};
static PRUnichar const NAME_423[] = {
  'R', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_423[] = {
  0x0154
};
static PRUnichar const NAME_424[] = {
  'R', 'a', 'n', 'g', ';'
};
static PRUnichar const VALUE_424[] = {
  0x27eb
};
static PRUnichar const NAME_425[] = {
  'R', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_425[] = {
  0x21a0
};
static PRUnichar const NAME_426[] = {
  'R', 'a', 'r', 'r', 't', 'l', ';'
};
static PRUnichar const VALUE_426[] = {
  0x2916
};
static PRUnichar const NAME_427[] = {
  'R', 'c', 'a', 'r', 'o', 'n', ';'
};
static PRUnichar const VALUE_427[] = {
  0x0158
};
static PRUnichar const NAME_428[] = {
  'R', 'c', 'e', 'd', 'i', 'l', ';'
};
static PRUnichar const VALUE_428[] = {
  0x0156
};
static PRUnichar const NAME_429[] = {
  'R', 'c', 'y', ';'
};
static PRUnichar const VALUE_429[] = {
  0x0420
};
static PRUnichar const NAME_430[] = {
  'R', 'e', ';'
};
static PRUnichar const VALUE_430[] = {
  0x211c
};
static PRUnichar const NAME_431[] = {
  'R', 'e', 'v', 'e', 'r', 's', 'e', 'E', 'l', 'e', 'm', 'e', 'n', 't', ';'
};
static PRUnichar const VALUE_431[] = {
  0x220b
};
static PRUnichar const NAME_432[] = {
  'R', 'e', 'v', 'e', 'r', 's', 'e', 'E', 'q', 'u', 'i', 'l', 'i', 'b', 'r', 'i', 'u', 'm', ';'
};
static PRUnichar const VALUE_432[] = {
  0x21cb
};
static PRUnichar const NAME_433[] = {
  'R', 'e', 'v', 'e', 'r', 's', 'e', 'U', 'p', 'E', 'q', 'u', 'i', 'l', 'i', 'b', 'r', 'i', 'u', 'm', ';'
};
static PRUnichar const VALUE_433[] = {
  0x296f
};
static PRUnichar const NAME_434[] = {
  'R', 'f', 'r', ';'
};
static PRUnichar const VALUE_434[] = {
  0x211c
};
static PRUnichar const NAME_435[] = {
  'R', 'h', 'o', ';'
};
static PRUnichar const VALUE_435[] = {
  0x03a1
};
static PRUnichar const NAME_436[] = {
  'R', 'i', 'g', 'h', 't', 'A', 'n', 'g', 'l', 'e', 'B', 'r', 'a', 'c', 'k', 'e', 't', ';'
};
static PRUnichar const VALUE_436[] = {
  0x27e9
};
static PRUnichar const NAME_437[] = {
  'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_437[] = {
  0x2192
};
static PRUnichar const NAME_438[] = {
  'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', 'B', 'a', 'r', ';'
};
static PRUnichar const VALUE_438[] = {
  0x21e5
};
static PRUnichar const NAME_439[] = {
  'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', 'L', 'e', 'f', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_439[] = {
  0x21c4
};
static PRUnichar const NAME_440[] = {
  'R', 'i', 'g', 'h', 't', 'C', 'e', 'i', 'l', 'i', 'n', 'g', ';'
};
static PRUnichar const VALUE_440[] = {
  0x2309
};
static PRUnichar const NAME_441[] = {
  'R', 'i', 'g', 'h', 't', 'D', 'o', 'u', 'b', 'l', 'e', 'B', 'r', 'a', 'c', 'k', 'e', 't', ';'
};
static PRUnichar const VALUE_441[] = {
  0x27e7
};
static PRUnichar const NAME_442[] = {
  'R', 'i', 'g', 'h', 't', 'D', 'o', 'w', 'n', 'T', 'e', 'e', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
static PRUnichar const VALUE_442[] = {
  0x295d
};
static PRUnichar const NAME_443[] = {
  'R', 'i', 'g', 'h', 't', 'D', 'o', 'w', 'n', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
static PRUnichar const VALUE_443[] = {
  0x21c2
};
static PRUnichar const NAME_444[] = {
  'R', 'i', 'g', 'h', 't', 'D', 'o', 'w', 'n', 'V', 'e', 'c', 't', 'o', 'r', 'B', 'a', 'r', ';'
};
static PRUnichar const VALUE_444[] = {
  0x2955
};
static PRUnichar const NAME_445[] = {
  'R', 'i', 'g', 'h', 't', 'F', 'l', 'o', 'o', 'r', ';'
};
static PRUnichar const VALUE_445[] = {
  0x230b
};
static PRUnichar const NAME_446[] = {
  'R', 'i', 'g', 'h', 't', 'T', 'e', 'e', ';'
};
static PRUnichar const VALUE_446[] = {
  0x22a2
};
static PRUnichar const NAME_447[] = {
  'R', 'i', 'g', 'h', 't', 'T', 'e', 'e', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_447[] = {
  0x21a6
};
static PRUnichar const NAME_448[] = {
  'R', 'i', 'g', 'h', 't', 'T', 'e', 'e', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
static PRUnichar const VALUE_448[] = {
  0x295b
};
static PRUnichar const NAME_449[] = {
  'R', 'i', 'g', 'h', 't', 'T', 'r', 'i', 'a', 'n', 'g', 'l', 'e', ';'
};
static PRUnichar const VALUE_449[] = {
  0x22b3
};
static PRUnichar const NAME_450[] = {
  'R', 'i', 'g', 'h', 't', 'T', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'B', 'a', 'r', ';'
};
static PRUnichar const VALUE_450[] = {
  0x29d0
};
static PRUnichar const NAME_451[] = {
  'R', 'i', 'g', 'h', 't', 'T', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_451[] = {
  0x22b5
};
static PRUnichar const NAME_452[] = {
  'R', 'i', 'g', 'h', 't', 'U', 'p', 'D', 'o', 'w', 'n', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
static PRUnichar const VALUE_452[] = {
  0x294f
};
static PRUnichar const NAME_453[] = {
  'R', 'i', 'g', 'h', 't', 'U', 'p', 'T', 'e', 'e', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
static PRUnichar const VALUE_453[] = {
  0x295c
};
static PRUnichar const NAME_454[] = {
  'R', 'i', 'g', 'h', 't', 'U', 'p', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
static PRUnichar const VALUE_454[] = {
  0x21be
};
static PRUnichar const NAME_455[] = {
  'R', 'i', 'g', 'h', 't', 'U', 'p', 'V', 'e', 'c', 't', 'o', 'r', 'B', 'a', 'r', ';'
};
static PRUnichar const VALUE_455[] = {
  0x2954
};
static PRUnichar const NAME_456[] = {
  'R', 'i', 'g', 'h', 't', 'V', 'e', 'c', 't', 'o', 'r', ';'
};
static PRUnichar const VALUE_456[] = {
  0x21c0
};
static PRUnichar const NAME_457[] = {
  'R', 'i', 'g', 'h', 't', 'V', 'e', 'c', 't', 'o', 'r', 'B', 'a', 'r', ';'
};
static PRUnichar const VALUE_457[] = {
  0x2953
};
static PRUnichar const NAME_458[] = {
  'R', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_458[] = {
  0x21d2
};
static PRUnichar const NAME_459[] = {
  'R', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_459[] = {
  0x211d
};
static PRUnichar const NAME_460[] = {
  'R', 'o', 'u', 'n', 'd', 'I', 'm', 'p', 'l', 'i', 'e', 's', ';'
};
static PRUnichar const VALUE_460[] = {
  0x2970
};
static PRUnichar const NAME_461[] = {
  'R', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_461[] = {
  0x21db
};
static PRUnichar const NAME_462[] = {
  'R', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_462[] = {
  0x211b
};
static PRUnichar const NAME_463[] = {
  'R', 's', 'h', ';'
};
static PRUnichar const VALUE_463[] = {
  0x21b1
};
static PRUnichar const NAME_464[] = {
  'R', 'u', 'l', 'e', 'D', 'e', 'l', 'a', 'y', 'e', 'd', ';'
};
static PRUnichar const VALUE_464[] = {
  0x29f4
};
static PRUnichar const NAME_465[] = {
  'S', 'H', 'C', 'H', 'c', 'y', ';'
};
static PRUnichar const VALUE_465[] = {
  0x0429
};
static PRUnichar const NAME_466[] = {
  'S', 'H', 'c', 'y', ';'
};
static PRUnichar const VALUE_466[] = {
  0x0428
};
static PRUnichar const NAME_467[] = {
  'S', 'O', 'F', 'T', 'c', 'y', ';'
};
static PRUnichar const VALUE_467[] = {
  0x042c
};
static PRUnichar const NAME_468[] = {
  'S', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_468[] = {
  0x015a
};
static PRUnichar const NAME_469[] = {
  'S', 'c', ';'
};
static PRUnichar const VALUE_469[] = {
  0x2abc
};
static PRUnichar const NAME_470[] = {
  'S', 'c', 'a', 'r', 'o', 'n', ';'
};
static PRUnichar const VALUE_470[] = {
  0x0160
};
static PRUnichar const NAME_471[] = {
  'S', 'c', 'e', 'd', 'i', 'l', ';'
};
static PRUnichar const VALUE_471[] = {
  0x015e
};
static PRUnichar const NAME_472[] = {
  'S', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_472[] = {
  0x015c
};
static PRUnichar const NAME_473[] = {
  'S', 'c', 'y', ';'
};
static PRUnichar const VALUE_473[] = {
  0x0421
};
static PRUnichar const NAME_474[] = {
  'S', 'f', 'r', ';'
};
static PRUnichar const VALUE_474[] = {
  0xd835, 0xdd16
};
static PRUnichar const NAME_475[] = {
  'S', 'h', 'o', 'r', 't', 'D', 'o', 'w', 'n', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_475[] = {
  0x2193
};
static PRUnichar const NAME_476[] = {
  'S', 'h', 'o', 'r', 't', 'L', 'e', 'f', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_476[] = {
  0x2190
};
static PRUnichar const NAME_477[] = {
  'S', 'h', 'o', 'r', 't', 'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_477[] = {
  0x2192
};
static PRUnichar const NAME_478[] = {
  'S', 'h', 'o', 'r', 't', 'U', 'p', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_478[] = {
  0x2191
};
static PRUnichar const NAME_479[] = {
  'S', 'i', 'g', 'm', 'a', ';'
};
static PRUnichar const VALUE_479[] = {
  0x03a3
};
static PRUnichar const NAME_480[] = {
  'S', 'm', 'a', 'l', 'l', 'C', 'i', 'r', 'c', 'l', 'e', ';'
};
static PRUnichar const VALUE_480[] = {
  0x2218
};
static PRUnichar const NAME_481[] = {
  'S', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_481[] = {
  0xd835, 0xdd4a
};
static PRUnichar const NAME_482[] = {
  'S', 'q', 'r', 't', ';'
};
static PRUnichar const VALUE_482[] = {
  0x221a
};
static PRUnichar const NAME_483[] = {
  'S', 'q', 'u', 'a', 'r', 'e', ';'
};
static PRUnichar const VALUE_483[] = {
  0x25a1
};
static PRUnichar const NAME_484[] = {
  'S', 'q', 'u', 'a', 'r', 'e', 'I', 'n', 't', 'e', 'r', 's', 'e', 'c', 't', 'i', 'o', 'n', ';'
};
static PRUnichar const VALUE_484[] = {
  0x2293
};
static PRUnichar const NAME_485[] = {
  'S', 'q', 'u', 'a', 'r', 'e', 'S', 'u', 'b', 's', 'e', 't', ';'
};
static PRUnichar const VALUE_485[] = {
  0x228f
};
static PRUnichar const NAME_486[] = {
  'S', 'q', 'u', 'a', 'r', 'e', 'S', 'u', 'b', 's', 'e', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_486[] = {
  0x2291
};
static PRUnichar const NAME_487[] = {
  'S', 'q', 'u', 'a', 'r', 'e', 'S', 'u', 'p', 'e', 'r', 's', 'e', 't', ';'
};
static PRUnichar const VALUE_487[] = {
  0x2290
};
static PRUnichar const NAME_488[] = {
  'S', 'q', 'u', 'a', 'r', 'e', 'S', 'u', 'p', 'e', 'r', 's', 'e', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_488[] = {
  0x2292
};
static PRUnichar const NAME_489[] = {
  'S', 'q', 'u', 'a', 'r', 'e', 'U', 'n', 'i', 'o', 'n', ';'
};
static PRUnichar const VALUE_489[] = {
  0x2294
};
static PRUnichar const NAME_490[] = {
  'S', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_490[] = {
  0xd835, 0xdcae
};
static PRUnichar const NAME_491[] = {
  'S', 't', 'a', 'r', ';'
};
static PRUnichar const VALUE_491[] = {
  0x22c6
};
static PRUnichar const NAME_492[] = {
  'S', 'u', 'b', ';'
};
static PRUnichar const VALUE_492[] = {
  0x22d0
};
static PRUnichar const NAME_493[] = {
  'S', 'u', 'b', 's', 'e', 't', ';'
};
static PRUnichar const VALUE_493[] = {
  0x22d0
};
static PRUnichar const NAME_494[] = {
  'S', 'u', 'b', 's', 'e', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_494[] = {
  0x2286
};
static PRUnichar const NAME_495[] = {
  'S', 'u', 'c', 'c', 'e', 'e', 'd', 's', ';'
};
static PRUnichar const VALUE_495[] = {
  0x227b
};
static PRUnichar const NAME_496[] = {
  'S', 'u', 'c', 'c', 'e', 'e', 'd', 's', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_496[] = {
  0x2ab0
};
static PRUnichar const NAME_497[] = {
  'S', 'u', 'c', 'c', 'e', 'e', 'd', 's', 'S', 'l', 'a', 'n', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_497[] = {
  0x227d
};
static PRUnichar const NAME_498[] = {
  'S', 'u', 'c', 'c', 'e', 'e', 'd', 's', 'T', 'i', 'l', 'd', 'e', ';'
};
static PRUnichar const VALUE_498[] = {
  0x227f
};
static PRUnichar const NAME_499[] = {
  'S', 'u', 'c', 'h', 'T', 'h', 'a', 't', ';'
};
static PRUnichar const VALUE_499[] = {
  0x220b
};
static PRUnichar const NAME_500[] = {
  'S', 'u', 'm', ';'
};
static PRUnichar const VALUE_500[] = {
  0x2211
};
static PRUnichar const NAME_501[] = {
  'S', 'u', 'p', ';'
};
static PRUnichar const VALUE_501[] = {
  0x22d1
};
static PRUnichar const NAME_502[] = {
  'S', 'u', 'p', 'e', 'r', 's', 'e', 't', ';'
};
static PRUnichar const VALUE_502[] = {
  0x2283
};
static PRUnichar const NAME_503[] = {
  'S', 'u', 'p', 'e', 'r', 's', 'e', 't', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_503[] = {
  0x2287
};
static PRUnichar const NAME_504[] = {
  'S', 'u', 'p', 's', 'e', 't', ';'
};
static PRUnichar const VALUE_504[] = {
  0x22d1
};
static PRUnichar const NAME_505[] = {
  'T', 'H', 'O', 'R', 'N'
};
static PRUnichar const VALUE_505[] = {
  0x00de
};
static PRUnichar const NAME_506[] = {
  'T', 'H', 'O', 'R', 'N', ';'
};
static PRUnichar const VALUE_506[] = {
  0x00de
};
static PRUnichar const NAME_507[] = {
  'T', 'R', 'A', 'D', 'E', ';'
};
static PRUnichar const VALUE_507[] = {
  0x2122
};
static PRUnichar const NAME_508[] = {
  'T', 'S', 'H', 'c', 'y', ';'
};
static PRUnichar const VALUE_508[] = {
  0x040b
};
static PRUnichar const NAME_509[] = {
  'T', 'S', 'c', 'y', ';'
};
static PRUnichar const VALUE_509[] = {
  0x0426
};
static PRUnichar const NAME_510[] = {
  'T', 'a', 'b', ';'
};
static PRUnichar const VALUE_510[] = {
  0x0009
};
static PRUnichar const NAME_511[] = {
  'T', 'a', 'u', ';'
};
static PRUnichar const VALUE_511[] = {
  0x03a4
};
static PRUnichar const NAME_512[] = {
  'T', 'c', 'a', 'r', 'o', 'n', ';'
};
static PRUnichar const VALUE_512[] = {
  0x0164
};
static PRUnichar const NAME_513[] = {
  'T', 'c', 'e', 'd', 'i', 'l', ';'
};
static PRUnichar const VALUE_513[] = {
  0x0162
};
static PRUnichar const NAME_514[] = {
  'T', 'c', 'y', ';'
};
static PRUnichar const VALUE_514[] = {
  0x0422
};
static PRUnichar const NAME_515[] = {
  'T', 'f', 'r', ';'
};
static PRUnichar const VALUE_515[] = {
  0xd835, 0xdd17
};
static PRUnichar const NAME_516[] = {
  'T', 'h', 'e', 'r', 'e', 'f', 'o', 'r', 'e', ';'
};
static PRUnichar const VALUE_516[] = {
  0x2234
};
static PRUnichar const NAME_517[] = {
  'T', 'h', 'e', 't', 'a', ';'
};
static PRUnichar const VALUE_517[] = {
  0x0398
};
static PRUnichar const NAME_518[] = {
  'T', 'h', 'i', 'n', 'S', 'p', 'a', 'c', 'e', ';'
};
static PRUnichar const VALUE_518[] = {
  0x2009
};
static PRUnichar const NAME_519[] = {
  'T', 'i', 'l', 'd', 'e', ';'
};
static PRUnichar const VALUE_519[] = {
  0x223c
};
static PRUnichar const NAME_520[] = {
  'T', 'i', 'l', 'd', 'e', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_520[] = {
  0x2243
};
static PRUnichar const NAME_521[] = {
  'T', 'i', 'l', 'd', 'e', 'F', 'u', 'l', 'l', 'E', 'q', 'u', 'a', 'l', ';'
};
static PRUnichar const VALUE_521[] = {
  0x2245
};
static PRUnichar const NAME_522[] = {
  'T', 'i', 'l', 'd', 'e', 'T', 'i', 'l', 'd', 'e', ';'
};
static PRUnichar const VALUE_522[] = {
  0x2248
};
static PRUnichar const NAME_523[] = {
  'T', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_523[] = {
  0xd835, 0xdd4b
};
static PRUnichar const NAME_524[] = {
  'T', 'r', 'i', 'p', 'l', 'e', 'D', 'o', 't', ';'
};
static PRUnichar const VALUE_524[] = {
  0x20db
};
static PRUnichar const NAME_525[] = {
  'T', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_525[] = {
  0xd835, 0xdcaf
};
static PRUnichar const NAME_526[] = {
  'T', 's', 't', 'r', 'o', 'k', ';'
};
static PRUnichar const VALUE_526[] = {
  0x0166
};
static PRUnichar const NAME_527[] = {
  'U', 'a', 'c', 'u', 't', 'e'
};
static PRUnichar const VALUE_527[] = {
  0x00da
};
static PRUnichar const NAME_528[] = {
  'U', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_528[] = {
  0x00da
};
static PRUnichar const NAME_529[] = {
  'U', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_529[] = {
  0x219f
};
static PRUnichar const NAME_530[] = {
  'U', 'a', 'r', 'r', 'o', 'c', 'i', 'r', ';'
};
static PRUnichar const VALUE_530[] = {
  0x2949
};
static PRUnichar const NAME_531[] = {
  'U', 'b', 'r', 'c', 'y', ';'
};
static PRUnichar const VALUE_531[] = {
  0x040e
};
static PRUnichar const NAME_532[] = {
  'U', 'b', 'r', 'e', 'v', 'e', ';'
};
static PRUnichar const VALUE_532[] = {
  0x016c
};
static PRUnichar const NAME_533[] = {
  'U', 'c', 'i', 'r', 'c'
};
static PRUnichar const VALUE_533[] = {
  0x00db
};
static PRUnichar const NAME_534[] = {
  'U', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_534[] = {
  0x00db
};
static PRUnichar const NAME_535[] = {
  'U', 'c', 'y', ';'
};
static PRUnichar const VALUE_535[] = {
  0x0423
};
static PRUnichar const NAME_536[] = {
  'U', 'd', 'b', 'l', 'a', 'c', ';'
};
static PRUnichar const VALUE_536[] = {
  0x0170
};
static PRUnichar const NAME_537[] = {
  'U', 'f', 'r', ';'
};
static PRUnichar const VALUE_537[] = {
  0xd835, 0xdd18
};
static PRUnichar const NAME_538[] = {
  'U', 'g', 'r', 'a', 'v', 'e'
};
static PRUnichar const VALUE_538[] = {
  0x00d9
};
static PRUnichar const NAME_539[] = {
  'U', 'g', 'r', 'a', 'v', 'e', ';'
};
static PRUnichar const VALUE_539[] = {
  0x00d9
};
static PRUnichar const NAME_540[] = {
  'U', 'm', 'a', 'c', 'r', ';'
};
static PRUnichar const VALUE_540[] = {
  0x016a
};
static PRUnichar const NAME_541[] = {
  'U', 'n', 'd', 'e', 'r', 'B', 'a', 'r', ';'
};
static PRUnichar const VALUE_541[] = {
  0x0332
};
static PRUnichar const NAME_542[] = {
  'U', 'n', 'd', 'e', 'r', 'B', 'r', 'a', 'c', 'e', ';'
};
static PRUnichar const VALUE_542[] = {
  0x23df
};
static PRUnichar const NAME_543[] = {
  'U', 'n', 'd', 'e', 'r', 'B', 'r', 'a', 'c', 'k', 'e', 't', ';'
};
static PRUnichar const VALUE_543[] = {
  0x23b5
};
static PRUnichar const NAME_544[] = {
  'U', 'n', 'd', 'e', 'r', 'P', 'a', 'r', 'e', 'n', 't', 'h', 'e', 's', 'i', 's', ';'
};
static PRUnichar const VALUE_544[] = {
  0x23dd
};
static PRUnichar const NAME_545[] = {
  'U', 'n', 'i', 'o', 'n', ';'
};
static PRUnichar const VALUE_545[] = {
  0x22c3
};
static PRUnichar const NAME_546[] = {
  'U', 'n', 'i', 'o', 'n', 'P', 'l', 'u', 's', ';'
};
static PRUnichar const VALUE_546[] = {
  0x228e
};
static PRUnichar const NAME_547[] = {
  'U', 'o', 'g', 'o', 'n', ';'
};
static PRUnichar const VALUE_547[] = {
  0x0172
};
static PRUnichar const NAME_548[] = {
  'U', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_548[] = {
  0xd835, 0xdd4c
};
static PRUnichar const NAME_549[] = {
  'U', 'p', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_549[] = {
  0x2191
};
static PRUnichar const NAME_550[] = {
  'U', 'p', 'A', 'r', 'r', 'o', 'w', 'B', 'a', 'r', ';'
};
static PRUnichar const VALUE_550[] = {
  0x2912
};
static PRUnichar const NAME_551[] = {
  'U', 'p', 'A', 'r', 'r', 'o', 'w', 'D', 'o', 'w', 'n', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_551[] = {
  0x21c5
};
static PRUnichar const NAME_552[] = {
  'U', 'p', 'D', 'o', 'w', 'n', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_552[] = {
  0x2195
};
static PRUnichar const NAME_553[] = {
  'U', 'p', 'E', 'q', 'u', 'i', 'l', 'i', 'b', 'r', 'i', 'u', 'm', ';'
};
static PRUnichar const VALUE_553[] = {
  0x296e
};
static PRUnichar const NAME_554[] = {
  'U', 'p', 'T', 'e', 'e', ';'
};
static PRUnichar const VALUE_554[] = {
  0x22a5
};
static PRUnichar const NAME_555[] = {
  'U', 'p', 'T', 'e', 'e', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_555[] = {
  0x21a5
};
static PRUnichar const NAME_556[] = {
  'U', 'p', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_556[] = {
  0x21d1
};
static PRUnichar const NAME_557[] = {
  'U', 'p', 'd', 'o', 'w', 'n', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_557[] = {
  0x21d5
};
static PRUnichar const NAME_558[] = {
  'U', 'p', 'p', 'e', 'r', 'L', 'e', 'f', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_558[] = {
  0x2196
};
static PRUnichar const NAME_559[] = {
  'U', 'p', 'p', 'e', 'r', 'R', 'i', 'g', 'h', 't', 'A', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_559[] = {
  0x2197
};
static PRUnichar const NAME_560[] = {
  'U', 'p', 's', 'i', ';'
};
static PRUnichar const VALUE_560[] = {
  0x03d2
};
static PRUnichar const NAME_561[] = {
  'U', 'p', 's', 'i', 'l', 'o', 'n', ';'
};
static PRUnichar const VALUE_561[] = {
  0x03a5
};
static PRUnichar const NAME_562[] = {
  'U', 'r', 'i', 'n', 'g', ';'
};
static PRUnichar const VALUE_562[] = {
  0x016e
};
static PRUnichar const NAME_563[] = {
  'U', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_563[] = {
  0xd835, 0xdcb0
};
static PRUnichar const NAME_564[] = {
  'U', 't', 'i', 'l', 'd', 'e', ';'
};
static PRUnichar const VALUE_564[] = {
  0x0168
};
static PRUnichar const NAME_565[] = {
  'U', 'u', 'm', 'l'
};
static PRUnichar const VALUE_565[] = {
  0x00dc
};
static PRUnichar const NAME_566[] = {
  'U', 'u', 'm', 'l', ';'
};
static PRUnichar const VALUE_566[] = {
  0x00dc
};
static PRUnichar const NAME_567[] = {
  'V', 'D', 'a', 's', 'h', ';'
};
static PRUnichar const VALUE_567[] = {
  0x22ab
};
static PRUnichar const NAME_568[] = {
  'V', 'b', 'a', 'r', ';'
};
static PRUnichar const VALUE_568[] = {
  0x2aeb
};
static PRUnichar const NAME_569[] = {
  'V', 'c', 'y', ';'
};
static PRUnichar const VALUE_569[] = {
  0x0412
};
static PRUnichar const NAME_570[] = {
  'V', 'd', 'a', 's', 'h', ';'
};
static PRUnichar const VALUE_570[] = {
  0x22a9
};
static PRUnichar const NAME_571[] = {
  'V', 'd', 'a', 's', 'h', 'l', ';'
};
static PRUnichar const VALUE_571[] = {
  0x2ae6
};
static PRUnichar const NAME_572[] = {
  'V', 'e', 'e', ';'
};
static PRUnichar const VALUE_572[] = {
  0x22c1
};
static PRUnichar const NAME_573[] = {
  'V', 'e', 'r', 'b', 'a', 'r', ';'
};
static PRUnichar const VALUE_573[] = {
  0x2016
};
static PRUnichar const NAME_574[] = {
  'V', 'e', 'r', 't', ';'
};
static PRUnichar const VALUE_574[] = {
  0x2016
};
static PRUnichar const NAME_575[] = {
  'V', 'e', 'r', 't', 'i', 'c', 'a', 'l', 'B', 'a', 'r', ';'
};
static PRUnichar const VALUE_575[] = {
  0x2223
};
static PRUnichar const NAME_576[] = {
  'V', 'e', 'r', 't', 'i', 'c', 'a', 'l', 'L', 'i', 'n', 'e', ';'
};
static PRUnichar const VALUE_576[] = {
  0x007c
};
static PRUnichar const NAME_577[] = {
  'V', 'e', 'r', 't', 'i', 'c', 'a', 'l', 'S', 'e', 'p', 'a', 'r', 'a', 't', 'o', 'r', ';'
};
static PRUnichar const VALUE_577[] = {
  0x2758
};
static PRUnichar const NAME_578[] = {
  'V', 'e', 'r', 't', 'i', 'c', 'a', 'l', 'T', 'i', 'l', 'd', 'e', ';'
};
static PRUnichar const VALUE_578[] = {
  0x2240
};
static PRUnichar const NAME_579[] = {
  'V', 'e', 'r', 'y', 'T', 'h', 'i', 'n', 'S', 'p', 'a', 'c', 'e', ';'
};
static PRUnichar const VALUE_579[] = {
  0x200a
};
static PRUnichar const NAME_580[] = {
  'V', 'f', 'r', ';'
};
static PRUnichar const VALUE_580[] = {
  0xd835, 0xdd19
};
static PRUnichar const NAME_581[] = {
  'V', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_581[] = {
  0xd835, 0xdd4d
};
static PRUnichar const NAME_582[] = {
  'V', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_582[] = {
  0xd835, 0xdcb1
};
static PRUnichar const NAME_583[] = {
  'V', 'v', 'd', 'a', 's', 'h', ';'
};
static PRUnichar const VALUE_583[] = {
  0x22aa
};
static PRUnichar const NAME_584[] = {
  'W', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_584[] = {
  0x0174
};
static PRUnichar const NAME_585[] = {
  'W', 'e', 'd', 'g', 'e', ';'
};
static PRUnichar const VALUE_585[] = {
  0x22c0
};
static PRUnichar const NAME_586[] = {
  'W', 'f', 'r', ';'
};
static PRUnichar const VALUE_586[] = {
  0xd835, 0xdd1a
};
static PRUnichar const NAME_587[] = {
  'W', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_587[] = {
  0xd835, 0xdd4e
};
static PRUnichar const NAME_588[] = {
  'W', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_588[] = {
  0xd835, 0xdcb2
};
static PRUnichar const NAME_589[] = {
  'X', 'f', 'r', ';'
};
static PRUnichar const VALUE_589[] = {
  0xd835, 0xdd1b
};
static PRUnichar const NAME_590[] = {
  'X', 'i', ';'
};
static PRUnichar const VALUE_590[] = {
  0x039e
};
static PRUnichar const NAME_591[] = {
  'X', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_591[] = {
  0xd835, 0xdd4f
};
static PRUnichar const NAME_592[] = {
  'X', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_592[] = {
  0xd835, 0xdcb3
};
static PRUnichar const NAME_593[] = {
  'Y', 'A', 'c', 'y', ';'
};
static PRUnichar const VALUE_593[] = {
  0x042f
};
static PRUnichar const NAME_594[] = {
  'Y', 'I', 'c', 'y', ';'
};
static PRUnichar const VALUE_594[] = {
  0x0407
};
static PRUnichar const NAME_595[] = {
  'Y', 'U', 'c', 'y', ';'
};
static PRUnichar const VALUE_595[] = {
  0x042e
};
static PRUnichar const NAME_596[] = {
  'Y', 'a', 'c', 'u', 't', 'e'
};
static PRUnichar const VALUE_596[] = {
  0x00dd
};
static PRUnichar const NAME_597[] = {
  'Y', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_597[] = {
  0x00dd
};
static PRUnichar const NAME_598[] = {
  'Y', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_598[] = {
  0x0176
};
static PRUnichar const NAME_599[] = {
  'Y', 'c', 'y', ';'
};
static PRUnichar const VALUE_599[] = {
  0x042b
};
static PRUnichar const NAME_600[] = {
  'Y', 'f', 'r', ';'
};
static PRUnichar const VALUE_600[] = {
  0xd835, 0xdd1c
};
static PRUnichar const NAME_601[] = {
  'Y', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_601[] = {
  0xd835, 0xdd50
};
static PRUnichar const NAME_602[] = {
  'Y', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_602[] = {
  0xd835, 0xdcb4
};
static PRUnichar const NAME_603[] = {
  'Y', 'u', 'm', 'l', ';'
};
static PRUnichar const VALUE_603[] = {
  0x0178
};
static PRUnichar const NAME_604[] = {
  'Z', 'H', 'c', 'y', ';'
};
static PRUnichar const VALUE_604[] = {
  0x0416
};
static PRUnichar const NAME_605[] = {
  'Z', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_605[] = {
  0x0179
};
static PRUnichar const NAME_606[] = {
  'Z', 'c', 'a', 'r', 'o', 'n', ';'
};
static PRUnichar const VALUE_606[] = {
  0x017d
};
static PRUnichar const NAME_607[] = {
  'Z', 'c', 'y', ';'
};
static PRUnichar const VALUE_607[] = {
  0x0417
};
static PRUnichar const NAME_608[] = {
  'Z', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_608[] = {
  0x017b
};
static PRUnichar const NAME_609[] = {
  'Z', 'e', 'r', 'o', 'W', 'i', 'd', 't', 'h', 'S', 'p', 'a', 'c', 'e', ';'
};
static PRUnichar const VALUE_609[] = {
  0x200b
};
static PRUnichar const NAME_610[] = {
  'Z', 'e', 't', 'a', ';'
};
static PRUnichar const VALUE_610[] = {
  0x0396
};
static PRUnichar const NAME_611[] = {
  'Z', 'f', 'r', ';'
};
static PRUnichar const VALUE_611[] = {
  0x2128
};
static PRUnichar const NAME_612[] = {
  'Z', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_612[] = {
  0x2124
};
static PRUnichar const NAME_613[] = {
  'Z', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_613[] = {
  0xd835, 0xdcb5
};
static PRUnichar const NAME_614[] = {
  'a', 'a', 'c', 'u', 't', 'e'
};
static PRUnichar const VALUE_614[] = {
  0x00e1
};
static PRUnichar const NAME_615[] = {
  'a', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_615[] = {
  0x00e1
};
static PRUnichar const NAME_616[] = {
  'a', 'b', 'r', 'e', 'v', 'e', ';'
};
static PRUnichar const VALUE_616[] = {
  0x0103
};
static PRUnichar const NAME_617[] = {
  'a', 'c', ';'
};
static PRUnichar const VALUE_617[] = {
  0x223e
};
static PRUnichar const NAME_618[] = {
  'a', 'c', 'd', ';'
};
static PRUnichar const VALUE_618[] = {
  0x223f
};
static PRUnichar const NAME_619[] = {
  'a', 'c', 'i', 'r', 'c'
};
static PRUnichar const VALUE_619[] = {
  0x00e2
};
static PRUnichar const NAME_620[] = {
  'a', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_620[] = {
  0x00e2
};
static PRUnichar const NAME_621[] = {
  'a', 'c', 'u', 't', 'e'
};
static PRUnichar const VALUE_621[] = {
  0x00b4
};
static PRUnichar const NAME_622[] = {
  'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_622[] = {
  0x00b4
};
static PRUnichar const NAME_623[] = {
  'a', 'c', 'y', ';'
};
static PRUnichar const VALUE_623[] = {
  0x0430
};
static PRUnichar const NAME_624[] = {
  'a', 'e', 'l', 'i', 'g'
};
static PRUnichar const VALUE_624[] = {
  0x00e6
};
static PRUnichar const NAME_625[] = {
  'a', 'e', 'l', 'i', 'g', ';'
};
static PRUnichar const VALUE_625[] = {
  0x00e6
};
static PRUnichar const NAME_626[] = {
  'a', 'f', ';'
};
static PRUnichar const VALUE_626[] = {
  0x2061
};
static PRUnichar const NAME_627[] = {
  'a', 'f', 'r', ';'
};
static PRUnichar const VALUE_627[] = {
  0xd835, 0xdd1e
};
static PRUnichar const NAME_628[] = {
  'a', 'g', 'r', 'a', 'v', 'e'
};
static PRUnichar const VALUE_628[] = {
  0x00e0
};
static PRUnichar const NAME_629[] = {
  'a', 'g', 'r', 'a', 'v', 'e', ';'
};
static PRUnichar const VALUE_629[] = {
  0x00e0
};
static PRUnichar const NAME_630[] = {
  'a', 'l', 'e', 'f', 's', 'y', 'm', ';'
};
static PRUnichar const VALUE_630[] = {
  0x2135
};
static PRUnichar const NAME_631[] = {
  'a', 'l', 'e', 'p', 'h', ';'
};
static PRUnichar const VALUE_631[] = {
  0x2135
};
static PRUnichar const NAME_632[] = {
  'a', 'l', 'p', 'h', 'a', ';'
};
static PRUnichar const VALUE_632[] = {
  0x03b1
};
static PRUnichar const NAME_633[] = {
  'a', 'm', 'a', 'c', 'r', ';'
};
static PRUnichar const VALUE_633[] = {
  0x0101
};
static PRUnichar const NAME_634[] = {
  'a', 'm', 'a', 'l', 'g', ';'
};
static PRUnichar const VALUE_634[] = {
  0x2a3f
};
static PRUnichar const NAME_635[] = {
  'a', 'm', 'p'
};
static PRUnichar const VALUE_635[] = {
  0x0026
};
static PRUnichar const NAME_636[] = {
  'a', 'm', 'p', ';'
};
static PRUnichar const VALUE_636[] = {
  0x0026
};
static PRUnichar const NAME_637[] = {
  'a', 'n', 'd', ';'
};
static PRUnichar const VALUE_637[] = {
  0x2227
};
static PRUnichar const NAME_638[] = {
  'a', 'n', 'd', 'a', 'n', 'd', ';'
};
static PRUnichar const VALUE_638[] = {
  0x2a55
};
static PRUnichar const NAME_639[] = {
  'a', 'n', 'd', 'd', ';'
};
static PRUnichar const VALUE_639[] = {
  0x2a5c
};
static PRUnichar const NAME_640[] = {
  'a', 'n', 'd', 's', 'l', 'o', 'p', 'e', ';'
};
static PRUnichar const VALUE_640[] = {
  0x2a58
};
static PRUnichar const NAME_641[] = {
  'a', 'n', 'd', 'v', ';'
};
static PRUnichar const VALUE_641[] = {
  0x2a5a
};
static PRUnichar const NAME_642[] = {
  'a', 'n', 'g', ';'
};
static PRUnichar const VALUE_642[] = {
  0x2220
};
static PRUnichar const NAME_643[] = {
  'a', 'n', 'g', 'e', ';'
};
static PRUnichar const VALUE_643[] = {
  0x29a4
};
static PRUnichar const NAME_644[] = {
  'a', 'n', 'g', 'l', 'e', ';'
};
static PRUnichar const VALUE_644[] = {
  0x2220
};
static PRUnichar const NAME_645[] = {
  'a', 'n', 'g', 'm', 's', 'd', ';'
};
static PRUnichar const VALUE_645[] = {
  0x2221
};
static PRUnichar const NAME_646[] = {
  'a', 'n', 'g', 'm', 's', 'd', 'a', 'a', ';'
};
static PRUnichar const VALUE_646[] = {
  0x29a8
};
static PRUnichar const NAME_647[] = {
  'a', 'n', 'g', 'm', 's', 'd', 'a', 'b', ';'
};
static PRUnichar const VALUE_647[] = {
  0x29a9
};
static PRUnichar const NAME_648[] = {
  'a', 'n', 'g', 'm', 's', 'd', 'a', 'c', ';'
};
static PRUnichar const VALUE_648[] = {
  0x29aa
};
static PRUnichar const NAME_649[] = {
  'a', 'n', 'g', 'm', 's', 'd', 'a', 'd', ';'
};
static PRUnichar const VALUE_649[] = {
  0x29ab
};
static PRUnichar const NAME_650[] = {
  'a', 'n', 'g', 'm', 's', 'd', 'a', 'e', ';'
};
static PRUnichar const VALUE_650[] = {
  0x29ac
};
static PRUnichar const NAME_651[] = {
  'a', 'n', 'g', 'm', 's', 'd', 'a', 'f', ';'
};
static PRUnichar const VALUE_651[] = {
  0x29ad
};
static PRUnichar const NAME_652[] = {
  'a', 'n', 'g', 'm', 's', 'd', 'a', 'g', ';'
};
static PRUnichar const VALUE_652[] = {
  0x29ae
};
static PRUnichar const NAME_653[] = {
  'a', 'n', 'g', 'm', 's', 'd', 'a', 'h', ';'
};
static PRUnichar const VALUE_653[] = {
  0x29af
};
static PRUnichar const NAME_654[] = {
  'a', 'n', 'g', 'r', 't', ';'
};
static PRUnichar const VALUE_654[] = {
  0x221f
};
static PRUnichar const NAME_655[] = {
  'a', 'n', 'g', 'r', 't', 'v', 'b', ';'
};
static PRUnichar const VALUE_655[] = {
  0x22be
};
static PRUnichar const NAME_656[] = {
  'a', 'n', 'g', 'r', 't', 'v', 'b', 'd', ';'
};
static PRUnichar const VALUE_656[] = {
  0x299d
};
static PRUnichar const NAME_657[] = {
  'a', 'n', 'g', 's', 'p', 'h', ';'
};
static PRUnichar const VALUE_657[] = {
  0x2222
};
static PRUnichar const NAME_658[] = {
  'a', 'n', 'g', 's', 't', ';'
};
static PRUnichar const VALUE_658[] = {
  0x212b
};
static PRUnichar const NAME_659[] = {
  'a', 'n', 'g', 'z', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_659[] = {
  0x237c
};
static PRUnichar const NAME_660[] = {
  'a', 'o', 'g', 'o', 'n', ';'
};
static PRUnichar const VALUE_660[] = {
  0x0105
};
static PRUnichar const NAME_661[] = {
  'a', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_661[] = {
  0xd835, 0xdd52
};
static PRUnichar const NAME_662[] = {
  'a', 'p', ';'
};
static PRUnichar const VALUE_662[] = {
  0x2248
};
static PRUnichar const NAME_663[] = {
  'a', 'p', 'E', ';'
};
static PRUnichar const VALUE_663[] = {
  0x2a70
};
static PRUnichar const NAME_664[] = {
  'a', 'p', 'a', 'c', 'i', 'r', ';'
};
static PRUnichar const VALUE_664[] = {
  0x2a6f
};
static PRUnichar const NAME_665[] = {
  'a', 'p', 'e', ';'
};
static PRUnichar const VALUE_665[] = {
  0x224a
};
static PRUnichar const NAME_666[] = {
  'a', 'p', 'i', 'd', ';'
};
static PRUnichar const VALUE_666[] = {
  0x224b
};
static PRUnichar const NAME_667[] = {
  'a', 'p', 'o', 's', ';'
};
static PRUnichar const VALUE_667[] = {
  0x0027
};
static PRUnichar const NAME_668[] = {
  'a', 'p', 'p', 'r', 'o', 'x', ';'
};
static PRUnichar const VALUE_668[] = {
  0x2248
};
static PRUnichar const NAME_669[] = {
  'a', 'p', 'p', 'r', 'o', 'x', 'e', 'q', ';'
};
static PRUnichar const VALUE_669[] = {
  0x224a
};
static PRUnichar const NAME_670[] = {
  'a', 'r', 'i', 'n', 'g'
};
static PRUnichar const VALUE_670[] = {
  0x00e5
};
static PRUnichar const NAME_671[] = {
  'a', 'r', 'i', 'n', 'g', ';'
};
static PRUnichar const VALUE_671[] = {
  0x00e5
};
static PRUnichar const NAME_672[] = {
  'a', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_672[] = {
  0xd835, 0xdcb6
};
static PRUnichar const NAME_673[] = {
  'a', 's', 't', ';'
};
static PRUnichar const VALUE_673[] = {
  0x002a
};
static PRUnichar const NAME_674[] = {
  'a', 's', 'y', 'm', 'p', ';'
};
static PRUnichar const VALUE_674[] = {
  0x2248
};
static PRUnichar const NAME_675[] = {
  'a', 's', 'y', 'm', 'p', 'e', 'q', ';'
};
static PRUnichar const VALUE_675[] = {
  0x224d
};
static PRUnichar const NAME_676[] = {
  'a', 't', 'i', 'l', 'd', 'e'
};
static PRUnichar const VALUE_676[] = {
  0x00e3
};
static PRUnichar const NAME_677[] = {
  'a', 't', 'i', 'l', 'd', 'e', ';'
};
static PRUnichar const VALUE_677[] = {
  0x00e3
};
static PRUnichar const NAME_678[] = {
  'a', 'u', 'm', 'l'
};
static PRUnichar const VALUE_678[] = {
  0x00e4
};
static PRUnichar const NAME_679[] = {
  'a', 'u', 'm', 'l', ';'
};
static PRUnichar const VALUE_679[] = {
  0x00e4
};
static PRUnichar const NAME_680[] = {
  'a', 'w', 'c', 'o', 'n', 'i', 'n', 't', ';'
};
static PRUnichar const VALUE_680[] = {
  0x2233
};
static PRUnichar const NAME_681[] = {
  'a', 'w', 'i', 'n', 't', ';'
};
static PRUnichar const VALUE_681[] = {
  0x2a11
};
static PRUnichar const NAME_682[] = {
  'b', 'N', 'o', 't', ';'
};
static PRUnichar const VALUE_682[] = {
  0x2aed
};
static PRUnichar const NAME_683[] = {
  'b', 'a', 'c', 'k', 'c', 'o', 'n', 'g', ';'
};
static PRUnichar const VALUE_683[] = {
  0x224c
};
static PRUnichar const NAME_684[] = {
  'b', 'a', 'c', 'k', 'e', 'p', 's', 'i', 'l', 'o', 'n', ';'
};
static PRUnichar const VALUE_684[] = {
  0x03f6
};
static PRUnichar const NAME_685[] = {
  'b', 'a', 'c', 'k', 'p', 'r', 'i', 'm', 'e', ';'
};
static PRUnichar const VALUE_685[] = {
  0x2035
};
static PRUnichar const NAME_686[] = {
  'b', 'a', 'c', 'k', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_686[] = {
  0x223d
};
static PRUnichar const NAME_687[] = {
  'b', 'a', 'c', 'k', 's', 'i', 'm', 'e', 'q', ';'
};
static PRUnichar const VALUE_687[] = {
  0x22cd
};
static PRUnichar const NAME_688[] = {
  'b', 'a', 'r', 'v', 'e', 'e', ';'
};
static PRUnichar const VALUE_688[] = {
  0x22bd
};
static PRUnichar const NAME_689[] = {
  'b', 'a', 'r', 'w', 'e', 'd', ';'
};
static PRUnichar const VALUE_689[] = {
  0x2305
};
static PRUnichar const NAME_690[] = {
  'b', 'a', 'r', 'w', 'e', 'd', 'g', 'e', ';'
};
static PRUnichar const VALUE_690[] = {
  0x2305
};
static PRUnichar const NAME_691[] = {
  'b', 'b', 'r', 'k', ';'
};
static PRUnichar const VALUE_691[] = {
  0x23b5
};
static PRUnichar const NAME_692[] = {
  'b', 'b', 'r', 'k', 't', 'b', 'r', 'k', ';'
};
static PRUnichar const VALUE_692[] = {
  0x23b6
};
static PRUnichar const NAME_693[] = {
  'b', 'c', 'o', 'n', 'g', ';'
};
static PRUnichar const VALUE_693[] = {
  0x224c
};
static PRUnichar const NAME_694[] = {
  'b', 'c', 'y', ';'
};
static PRUnichar const VALUE_694[] = {
  0x0431
};
static PRUnichar const NAME_695[] = {
  'b', 'd', 'q', 'u', 'o', ';'
};
static PRUnichar const VALUE_695[] = {
  0x201e
};
static PRUnichar const NAME_696[] = {
  'b', 'e', 'c', 'a', 'u', 's', ';'
};
static PRUnichar const VALUE_696[] = {
  0x2235
};
static PRUnichar const NAME_697[] = {
  'b', 'e', 'c', 'a', 'u', 's', 'e', ';'
};
static PRUnichar const VALUE_697[] = {
  0x2235
};
static PRUnichar const NAME_698[] = {
  'b', 'e', 'm', 'p', 't', 'y', 'v', ';'
};
static PRUnichar const VALUE_698[] = {
  0x29b0
};
static PRUnichar const NAME_699[] = {
  'b', 'e', 'p', 's', 'i', ';'
};
static PRUnichar const VALUE_699[] = {
  0x03f6
};
static PRUnichar const NAME_700[] = {
  'b', 'e', 'r', 'n', 'o', 'u', ';'
};
static PRUnichar const VALUE_700[] = {
  0x212c
};
static PRUnichar const NAME_701[] = {
  'b', 'e', 't', 'a', ';'
};
static PRUnichar const VALUE_701[] = {
  0x03b2
};
static PRUnichar const NAME_702[] = {
  'b', 'e', 't', 'h', ';'
};
static PRUnichar const VALUE_702[] = {
  0x2136
};
static PRUnichar const NAME_703[] = {
  'b', 'e', 't', 'w', 'e', 'e', 'n', ';'
};
static PRUnichar const VALUE_703[] = {
  0x226c
};
static PRUnichar const NAME_704[] = {
  'b', 'f', 'r', ';'
};
static PRUnichar const VALUE_704[] = {
  0xd835, 0xdd1f
};
static PRUnichar const NAME_705[] = {
  'b', 'i', 'g', 'c', 'a', 'p', ';'
};
static PRUnichar const VALUE_705[] = {
  0x22c2
};
static PRUnichar const NAME_706[] = {
  'b', 'i', 'g', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_706[] = {
  0x25ef
};
static PRUnichar const NAME_707[] = {
  'b', 'i', 'g', 'c', 'u', 'p', ';'
};
static PRUnichar const VALUE_707[] = {
  0x22c3
};
static PRUnichar const NAME_708[] = {
  'b', 'i', 'g', 'o', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_708[] = {
  0x2a00
};
static PRUnichar const NAME_709[] = {
  'b', 'i', 'g', 'o', 'p', 'l', 'u', 's', ';'
};
static PRUnichar const VALUE_709[] = {
  0x2a01
};
static PRUnichar const NAME_710[] = {
  'b', 'i', 'g', 'o', 't', 'i', 'm', 'e', 's', ';'
};
static PRUnichar const VALUE_710[] = {
  0x2a02
};
static PRUnichar const NAME_711[] = {
  'b', 'i', 'g', 's', 'q', 'c', 'u', 'p', ';'
};
static PRUnichar const VALUE_711[] = {
  0x2a06
};
static PRUnichar const NAME_712[] = {
  'b', 'i', 'g', 's', 't', 'a', 'r', ';'
};
static PRUnichar const VALUE_712[] = {
  0x2605
};
static PRUnichar const NAME_713[] = {
  'b', 'i', 'g', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'd', 'o', 'w', 'n', ';'
};
static PRUnichar const VALUE_713[] = {
  0x25bd
};
static PRUnichar const NAME_714[] = {
  'b', 'i', 'g', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'u', 'p', ';'
};
static PRUnichar const VALUE_714[] = {
  0x25b3
};
static PRUnichar const NAME_715[] = {
  'b', 'i', 'g', 'u', 'p', 'l', 'u', 's', ';'
};
static PRUnichar const VALUE_715[] = {
  0x2a04
};
static PRUnichar const NAME_716[] = {
  'b', 'i', 'g', 'v', 'e', 'e', ';'
};
static PRUnichar const VALUE_716[] = {
  0x22c1
};
static PRUnichar const NAME_717[] = {
  'b', 'i', 'g', 'w', 'e', 'd', 'g', 'e', ';'
};
static PRUnichar const VALUE_717[] = {
  0x22c0
};
static PRUnichar const NAME_718[] = {
  'b', 'k', 'a', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_718[] = {
  0x290d
};
static PRUnichar const NAME_719[] = {
  'b', 'l', 'a', 'c', 'k', 'l', 'o', 'z', 'e', 'n', 'g', 'e', ';'
};
static PRUnichar const VALUE_719[] = {
  0x29eb
};
static PRUnichar const NAME_720[] = {
  'b', 'l', 'a', 'c', 'k', 's', 'q', 'u', 'a', 'r', 'e', ';'
};
static PRUnichar const VALUE_720[] = {
  0x25aa
};
static PRUnichar const NAME_721[] = {
  'b', 'l', 'a', 'c', 'k', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', ';'
};
static PRUnichar const VALUE_721[] = {
  0x25b4
};
static PRUnichar const NAME_722[] = {
  'b', 'l', 'a', 'c', 'k', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'd', 'o', 'w', 'n', ';'
};
static PRUnichar const VALUE_722[] = {
  0x25be
};
static PRUnichar const NAME_723[] = {
  'b', 'l', 'a', 'c', 'k', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'l', 'e', 'f', 't', ';'
};
static PRUnichar const VALUE_723[] = {
  0x25c2
};
static PRUnichar const NAME_724[] = {
  'b', 'l', 'a', 'c', 'k', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'r', 'i', 'g', 'h', 't', ';'
};
static PRUnichar const VALUE_724[] = {
  0x25b8
};
static PRUnichar const NAME_725[] = {
  'b', 'l', 'a', 'n', 'k', ';'
};
static PRUnichar const VALUE_725[] = {
  0x2423
};
static PRUnichar const NAME_726[] = {
  'b', 'l', 'k', '1', '2', ';'
};
static PRUnichar const VALUE_726[] = {
  0x2592
};
static PRUnichar const NAME_727[] = {
  'b', 'l', 'k', '1', '4', ';'
};
static PRUnichar const VALUE_727[] = {
  0x2591
};
static PRUnichar const NAME_728[] = {
  'b', 'l', 'k', '3', '4', ';'
};
static PRUnichar const VALUE_728[] = {
  0x2593
};
static PRUnichar const NAME_729[] = {
  'b', 'l', 'o', 'c', 'k', ';'
};
static PRUnichar const VALUE_729[] = {
  0x2588
};
static PRUnichar const NAME_730[] = {
  'b', 'n', 'o', 't', ';'
};
static PRUnichar const VALUE_730[] = {
  0x2310
};
static PRUnichar const NAME_731[] = {
  'b', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_731[] = {
  0xd835, 0xdd53
};
static PRUnichar const NAME_732[] = {
  'b', 'o', 't', ';'
};
static PRUnichar const VALUE_732[] = {
  0x22a5
};
static PRUnichar const NAME_733[] = {
  'b', 'o', 't', 't', 'o', 'm', ';'
};
static PRUnichar const VALUE_733[] = {
  0x22a5
};
static PRUnichar const NAME_734[] = {
  'b', 'o', 'w', 't', 'i', 'e', ';'
};
static PRUnichar const VALUE_734[] = {
  0x22c8
};
static PRUnichar const NAME_735[] = {
  'b', 'o', 'x', 'D', 'L', ';'
};
static PRUnichar const VALUE_735[] = {
  0x2557
};
static PRUnichar const NAME_736[] = {
  'b', 'o', 'x', 'D', 'R', ';'
};
static PRUnichar const VALUE_736[] = {
  0x2554
};
static PRUnichar const NAME_737[] = {
  'b', 'o', 'x', 'D', 'l', ';'
};
static PRUnichar const VALUE_737[] = {
  0x2556
};
static PRUnichar const NAME_738[] = {
  'b', 'o', 'x', 'D', 'r', ';'
};
static PRUnichar const VALUE_738[] = {
  0x2553
};
static PRUnichar const NAME_739[] = {
  'b', 'o', 'x', 'H', ';'
};
static PRUnichar const VALUE_739[] = {
  0x2550
};
static PRUnichar const NAME_740[] = {
  'b', 'o', 'x', 'H', 'D', ';'
};
static PRUnichar const VALUE_740[] = {
  0x2566
};
static PRUnichar const NAME_741[] = {
  'b', 'o', 'x', 'H', 'U', ';'
};
static PRUnichar const VALUE_741[] = {
  0x2569
};
static PRUnichar const NAME_742[] = {
  'b', 'o', 'x', 'H', 'd', ';'
};
static PRUnichar const VALUE_742[] = {
  0x2564
};
static PRUnichar const NAME_743[] = {
  'b', 'o', 'x', 'H', 'u', ';'
};
static PRUnichar const VALUE_743[] = {
  0x2567
};
static PRUnichar const NAME_744[] = {
  'b', 'o', 'x', 'U', 'L', ';'
};
static PRUnichar const VALUE_744[] = {
  0x255d
};
static PRUnichar const NAME_745[] = {
  'b', 'o', 'x', 'U', 'R', ';'
};
static PRUnichar const VALUE_745[] = {
  0x255a
};
static PRUnichar const NAME_746[] = {
  'b', 'o', 'x', 'U', 'l', ';'
};
static PRUnichar const VALUE_746[] = {
  0x255c
};
static PRUnichar const NAME_747[] = {
  'b', 'o', 'x', 'U', 'r', ';'
};
static PRUnichar const VALUE_747[] = {
  0x2559
};
static PRUnichar const NAME_748[] = {
  'b', 'o', 'x', 'V', ';'
};
static PRUnichar const VALUE_748[] = {
  0x2551
};
static PRUnichar const NAME_749[] = {
  'b', 'o', 'x', 'V', 'H', ';'
};
static PRUnichar const VALUE_749[] = {
  0x256c
};
static PRUnichar const NAME_750[] = {
  'b', 'o', 'x', 'V', 'L', ';'
};
static PRUnichar const VALUE_750[] = {
  0x2563
};
static PRUnichar const NAME_751[] = {
  'b', 'o', 'x', 'V', 'R', ';'
};
static PRUnichar const VALUE_751[] = {
  0x2560
};
static PRUnichar const NAME_752[] = {
  'b', 'o', 'x', 'V', 'h', ';'
};
static PRUnichar const VALUE_752[] = {
  0x256b
};
static PRUnichar const NAME_753[] = {
  'b', 'o', 'x', 'V', 'l', ';'
};
static PRUnichar const VALUE_753[] = {
  0x2562
};
static PRUnichar const NAME_754[] = {
  'b', 'o', 'x', 'V', 'r', ';'
};
static PRUnichar const VALUE_754[] = {
  0x255f
};
static PRUnichar const NAME_755[] = {
  'b', 'o', 'x', 'b', 'o', 'x', ';'
};
static PRUnichar const VALUE_755[] = {
  0x29c9
};
static PRUnichar const NAME_756[] = {
  'b', 'o', 'x', 'd', 'L', ';'
};
static PRUnichar const VALUE_756[] = {
  0x2555
};
static PRUnichar const NAME_757[] = {
  'b', 'o', 'x', 'd', 'R', ';'
};
static PRUnichar const VALUE_757[] = {
  0x2552
};
static PRUnichar const NAME_758[] = {
  'b', 'o', 'x', 'd', 'l', ';'
};
static PRUnichar const VALUE_758[] = {
  0x2510
};
static PRUnichar const NAME_759[] = {
  'b', 'o', 'x', 'd', 'r', ';'
};
static PRUnichar const VALUE_759[] = {
  0x250c
};
static PRUnichar const NAME_760[] = {
  'b', 'o', 'x', 'h', ';'
};
static PRUnichar const VALUE_760[] = {
  0x2500
};
static PRUnichar const NAME_761[] = {
  'b', 'o', 'x', 'h', 'D', ';'
};
static PRUnichar const VALUE_761[] = {
  0x2565
};
static PRUnichar const NAME_762[] = {
  'b', 'o', 'x', 'h', 'U', ';'
};
static PRUnichar const VALUE_762[] = {
  0x2568
};
static PRUnichar const NAME_763[] = {
  'b', 'o', 'x', 'h', 'd', ';'
};
static PRUnichar const VALUE_763[] = {
  0x252c
};
static PRUnichar const NAME_764[] = {
  'b', 'o', 'x', 'h', 'u', ';'
};
static PRUnichar const VALUE_764[] = {
  0x2534
};
static PRUnichar const NAME_765[] = {
  'b', 'o', 'x', 'm', 'i', 'n', 'u', 's', ';'
};
static PRUnichar const VALUE_765[] = {
  0x229f
};
static PRUnichar const NAME_766[] = {
  'b', 'o', 'x', 'p', 'l', 'u', 's', ';'
};
static PRUnichar const VALUE_766[] = {
  0x229e
};
static PRUnichar const NAME_767[] = {
  'b', 'o', 'x', 't', 'i', 'm', 'e', 's', ';'
};
static PRUnichar const VALUE_767[] = {
  0x22a0
};
static PRUnichar const NAME_768[] = {
  'b', 'o', 'x', 'u', 'L', ';'
};
static PRUnichar const VALUE_768[] = {
  0x255b
};
static PRUnichar const NAME_769[] = {
  'b', 'o', 'x', 'u', 'R', ';'
};
static PRUnichar const VALUE_769[] = {
  0x2558
};
static PRUnichar const NAME_770[] = {
  'b', 'o', 'x', 'u', 'l', ';'
};
static PRUnichar const VALUE_770[] = {
  0x2518
};
static PRUnichar const NAME_771[] = {
  'b', 'o', 'x', 'u', 'r', ';'
};
static PRUnichar const VALUE_771[] = {
  0x2514
};
static PRUnichar const NAME_772[] = {
  'b', 'o', 'x', 'v', ';'
};
static PRUnichar const VALUE_772[] = {
  0x2502
};
static PRUnichar const NAME_773[] = {
  'b', 'o', 'x', 'v', 'H', ';'
};
static PRUnichar const VALUE_773[] = {
  0x256a
};
static PRUnichar const NAME_774[] = {
  'b', 'o', 'x', 'v', 'L', ';'
};
static PRUnichar const VALUE_774[] = {
  0x2561
};
static PRUnichar const NAME_775[] = {
  'b', 'o', 'x', 'v', 'R', ';'
};
static PRUnichar const VALUE_775[] = {
  0x255e
};
static PRUnichar const NAME_776[] = {
  'b', 'o', 'x', 'v', 'h', ';'
};
static PRUnichar const VALUE_776[] = {
  0x253c
};
static PRUnichar const NAME_777[] = {
  'b', 'o', 'x', 'v', 'l', ';'
};
static PRUnichar const VALUE_777[] = {
  0x2524
};
static PRUnichar const NAME_778[] = {
  'b', 'o', 'x', 'v', 'r', ';'
};
static PRUnichar const VALUE_778[] = {
  0x251c
};
static PRUnichar const NAME_779[] = {
  'b', 'p', 'r', 'i', 'm', 'e', ';'
};
static PRUnichar const VALUE_779[] = {
  0x2035
};
static PRUnichar const NAME_780[] = {
  'b', 'r', 'e', 'v', 'e', ';'
};
static PRUnichar const VALUE_780[] = {
  0x02d8
};
static PRUnichar const NAME_781[] = {
  'b', 'r', 'v', 'b', 'a', 'r'
};
static PRUnichar const VALUE_781[] = {
  0x00a6
};
static PRUnichar const NAME_782[] = {
  'b', 'r', 'v', 'b', 'a', 'r', ';'
};
static PRUnichar const VALUE_782[] = {
  0x00a6
};
static PRUnichar const NAME_783[] = {
  'b', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_783[] = {
  0xd835, 0xdcb7
};
static PRUnichar const NAME_784[] = {
  'b', 's', 'e', 'm', 'i', ';'
};
static PRUnichar const VALUE_784[] = {
  0x204f
};
static PRUnichar const NAME_785[] = {
  'b', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_785[] = {
  0x223d
};
static PRUnichar const NAME_786[] = {
  'b', 's', 'i', 'm', 'e', ';'
};
static PRUnichar const VALUE_786[] = {
  0x22cd
};
static PRUnichar const NAME_787[] = {
  'b', 's', 'o', 'l', ';'
};
static PRUnichar const VALUE_787[] = {
  0x005c
};
static PRUnichar const NAME_788[] = {
  'b', 's', 'o', 'l', 'b', ';'
};
static PRUnichar const VALUE_788[] = {
  0x29c5
};
static PRUnichar const NAME_789[] = {
  'b', 'u', 'l', 'l', ';'
};
static PRUnichar const VALUE_789[] = {
  0x2022
};
static PRUnichar const NAME_790[] = {
  'b', 'u', 'l', 'l', 'e', 't', ';'
};
static PRUnichar const VALUE_790[] = {
  0x2022
};
static PRUnichar const NAME_791[] = {
  'b', 'u', 'm', 'p', ';'
};
static PRUnichar const VALUE_791[] = {
  0x224e
};
static PRUnichar const NAME_792[] = {
  'b', 'u', 'm', 'p', 'E', ';'
};
static PRUnichar const VALUE_792[] = {
  0x2aae
};
static PRUnichar const NAME_793[] = {
  'b', 'u', 'm', 'p', 'e', ';'
};
static PRUnichar const VALUE_793[] = {
  0x224f
};
static PRUnichar const NAME_794[] = {
  'b', 'u', 'm', 'p', 'e', 'q', ';'
};
static PRUnichar const VALUE_794[] = {
  0x224f
};
static PRUnichar const NAME_795[] = {
  'c', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_795[] = {
  0x0107
};
static PRUnichar const NAME_796[] = {
  'c', 'a', 'p', ';'
};
static PRUnichar const VALUE_796[] = {
  0x2229
};
static PRUnichar const NAME_797[] = {
  'c', 'a', 'p', 'a', 'n', 'd', ';'
};
static PRUnichar const VALUE_797[] = {
  0x2a44
};
static PRUnichar const NAME_798[] = {
  'c', 'a', 'p', 'b', 'r', 'c', 'u', 'p', ';'
};
static PRUnichar const VALUE_798[] = {
  0x2a49
};
static PRUnichar const NAME_799[] = {
  'c', 'a', 'p', 'c', 'a', 'p', ';'
};
static PRUnichar const VALUE_799[] = {
  0x2a4b
};
static PRUnichar const NAME_800[] = {
  'c', 'a', 'p', 'c', 'u', 'p', ';'
};
static PRUnichar const VALUE_800[] = {
  0x2a47
};
static PRUnichar const NAME_801[] = {
  'c', 'a', 'p', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_801[] = {
  0x2a40
};
static PRUnichar const NAME_802[] = {
  'c', 'a', 'r', 'e', 't', ';'
};
static PRUnichar const VALUE_802[] = {
  0x2041
};
static PRUnichar const NAME_803[] = {
  'c', 'a', 'r', 'o', 'n', ';'
};
static PRUnichar const VALUE_803[] = {
  0x02c7
};
static PRUnichar const NAME_804[] = {
  'c', 'c', 'a', 'p', 's', ';'
};
static PRUnichar const VALUE_804[] = {
  0x2a4d
};
static PRUnichar const NAME_805[] = {
  'c', 'c', 'a', 'r', 'o', 'n', ';'
};
static PRUnichar const VALUE_805[] = {
  0x010d
};
static PRUnichar const NAME_806[] = {
  'c', 'c', 'e', 'd', 'i', 'l'
};
static PRUnichar const VALUE_806[] = {
  0x00e7
};
static PRUnichar const NAME_807[] = {
  'c', 'c', 'e', 'd', 'i', 'l', ';'
};
static PRUnichar const VALUE_807[] = {
  0x00e7
};
static PRUnichar const NAME_808[] = {
  'c', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_808[] = {
  0x0109
};
static PRUnichar const NAME_809[] = {
  'c', 'c', 'u', 'p', 's', ';'
};
static PRUnichar const VALUE_809[] = {
  0x2a4c
};
static PRUnichar const NAME_810[] = {
  'c', 'c', 'u', 'p', 's', 's', 'm', ';'
};
static PRUnichar const VALUE_810[] = {
  0x2a50
};
static PRUnichar const NAME_811[] = {
  'c', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_811[] = {
  0x010b
};
static PRUnichar const NAME_812[] = {
  'c', 'e', 'd', 'i', 'l'
};
static PRUnichar const VALUE_812[] = {
  0x00b8
};
static PRUnichar const NAME_813[] = {
  'c', 'e', 'd', 'i', 'l', ';'
};
static PRUnichar const VALUE_813[] = {
  0x00b8
};
static PRUnichar const NAME_814[] = {
  'c', 'e', 'm', 'p', 't', 'y', 'v', ';'
};
static PRUnichar const VALUE_814[] = {
  0x29b2
};
static PRUnichar const NAME_815[] = {
  'c', 'e', 'n', 't'
};
static PRUnichar const VALUE_815[] = {
  0x00a2
};
static PRUnichar const NAME_816[] = {
  'c', 'e', 'n', 't', ';'
};
static PRUnichar const VALUE_816[] = {
  0x00a2
};
static PRUnichar const NAME_817[] = {
  'c', 'e', 'n', 't', 'e', 'r', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_817[] = {
  0x00b7
};
static PRUnichar const NAME_818[] = {
  'c', 'f', 'r', ';'
};
static PRUnichar const VALUE_818[] = {
  0xd835, 0xdd20
};
static PRUnichar const NAME_819[] = {
  'c', 'h', 'c', 'y', ';'
};
static PRUnichar const VALUE_819[] = {
  0x0447
};
static PRUnichar const NAME_820[] = {
  'c', 'h', 'e', 'c', 'k', ';'
};
static PRUnichar const VALUE_820[] = {
  0x2713
};
static PRUnichar const NAME_821[] = {
  'c', 'h', 'e', 'c', 'k', 'm', 'a', 'r', 'k', ';'
};
static PRUnichar const VALUE_821[] = {
  0x2713
};
static PRUnichar const NAME_822[] = {
  'c', 'h', 'i', ';'
};
static PRUnichar const VALUE_822[] = {
  0x03c7
};
static PRUnichar const NAME_823[] = {
  'c', 'i', 'r', ';'
};
static PRUnichar const VALUE_823[] = {
  0x25cb
};
static PRUnichar const NAME_824[] = {
  'c', 'i', 'r', 'E', ';'
};
static PRUnichar const VALUE_824[] = {
  0x29c3
};
static PRUnichar const NAME_825[] = {
  'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_825[] = {
  0x02c6
};
static PRUnichar const NAME_826[] = {
  'c', 'i', 'r', 'c', 'e', 'q', ';'
};
static PRUnichar const VALUE_826[] = {
  0x2257
};
static PRUnichar const NAME_827[] = {
  'c', 'i', 'r', 'c', 'l', 'e', 'a', 'r', 'r', 'o', 'w', 'l', 'e', 'f', 't', ';'
};
static PRUnichar const VALUE_827[] = {
  0x21ba
};
static PRUnichar const NAME_828[] = {
  'c', 'i', 'r', 'c', 'l', 'e', 'a', 'r', 'r', 'o', 'w', 'r', 'i', 'g', 'h', 't', ';'
};
static PRUnichar const VALUE_828[] = {
  0x21bb
};
static PRUnichar const NAME_829[] = {
  'c', 'i', 'r', 'c', 'l', 'e', 'd', 'R', ';'
};
static PRUnichar const VALUE_829[] = {
  0x00ae
};
static PRUnichar const NAME_830[] = {
  'c', 'i', 'r', 'c', 'l', 'e', 'd', 'S', ';'
};
static PRUnichar const VALUE_830[] = {
  0x24c8
};
static PRUnichar const NAME_831[] = {
  'c', 'i', 'r', 'c', 'l', 'e', 'd', 'a', 's', 't', ';'
};
static PRUnichar const VALUE_831[] = {
  0x229b
};
static PRUnichar const NAME_832[] = {
  'c', 'i', 'r', 'c', 'l', 'e', 'd', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_832[] = {
  0x229a
};
static PRUnichar const NAME_833[] = {
  'c', 'i', 'r', 'c', 'l', 'e', 'd', 'd', 'a', 's', 'h', ';'
};
static PRUnichar const VALUE_833[] = {
  0x229d
};
static PRUnichar const NAME_834[] = {
  'c', 'i', 'r', 'e', ';'
};
static PRUnichar const VALUE_834[] = {
  0x2257
};
static PRUnichar const NAME_835[] = {
  'c', 'i', 'r', 'f', 'n', 'i', 'n', 't', ';'
};
static PRUnichar const VALUE_835[] = {
  0x2a10
};
static PRUnichar const NAME_836[] = {
  'c', 'i', 'r', 'm', 'i', 'd', ';'
};
static PRUnichar const VALUE_836[] = {
  0x2aef
};
static PRUnichar const NAME_837[] = {
  'c', 'i', 'r', 's', 'c', 'i', 'r', ';'
};
static PRUnichar const VALUE_837[] = {
  0x29c2
};
static PRUnichar const NAME_838[] = {
  'c', 'l', 'u', 'b', 's', ';'
};
static PRUnichar const VALUE_838[] = {
  0x2663
};
static PRUnichar const NAME_839[] = {
  'c', 'l', 'u', 'b', 's', 'u', 'i', 't', ';'
};
static PRUnichar const VALUE_839[] = {
  0x2663
};
static PRUnichar const NAME_840[] = {
  'c', 'o', 'l', 'o', 'n', ';'
};
static PRUnichar const VALUE_840[] = {
  0x003a
};
static PRUnichar const NAME_841[] = {
  'c', 'o', 'l', 'o', 'n', 'e', ';'
};
static PRUnichar const VALUE_841[] = {
  0x2254
};
static PRUnichar const NAME_842[] = {
  'c', 'o', 'l', 'o', 'n', 'e', 'q', ';'
};
static PRUnichar const VALUE_842[] = {
  0x2254
};
static PRUnichar const NAME_843[] = {
  'c', 'o', 'm', 'm', 'a', ';'
};
static PRUnichar const VALUE_843[] = {
  0x002c
};
static PRUnichar const NAME_844[] = {
  'c', 'o', 'm', 'm', 'a', 't', ';'
};
static PRUnichar const VALUE_844[] = {
  0x0040
};
static PRUnichar const NAME_845[] = {
  'c', 'o', 'm', 'p', ';'
};
static PRUnichar const VALUE_845[] = {
  0x2201
};
static PRUnichar const NAME_846[] = {
  'c', 'o', 'm', 'p', 'f', 'n', ';'
};
static PRUnichar const VALUE_846[] = {
  0x2218
};
static PRUnichar const NAME_847[] = {
  'c', 'o', 'm', 'p', 'l', 'e', 'm', 'e', 'n', 't', ';'
};
static PRUnichar const VALUE_847[] = {
  0x2201
};
static PRUnichar const NAME_848[] = {
  'c', 'o', 'm', 'p', 'l', 'e', 'x', 'e', 's', ';'
};
static PRUnichar const VALUE_848[] = {
  0x2102
};
static PRUnichar const NAME_849[] = {
  'c', 'o', 'n', 'g', ';'
};
static PRUnichar const VALUE_849[] = {
  0x2245
};
static PRUnichar const NAME_850[] = {
  'c', 'o', 'n', 'g', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_850[] = {
  0x2a6d
};
static PRUnichar const NAME_851[] = {
  'c', 'o', 'n', 'i', 'n', 't', ';'
};
static PRUnichar const VALUE_851[] = {
  0x222e
};
static PRUnichar const NAME_852[] = {
  'c', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_852[] = {
  0xd835, 0xdd54
};
static PRUnichar const NAME_853[] = {
  'c', 'o', 'p', 'r', 'o', 'd', ';'
};
static PRUnichar const VALUE_853[] = {
  0x2210
};
static PRUnichar const NAME_854[] = {
  'c', 'o', 'p', 'y'
};
static PRUnichar const VALUE_854[] = {
  0x00a9
};
static PRUnichar const NAME_855[] = {
  'c', 'o', 'p', 'y', ';'
};
static PRUnichar const VALUE_855[] = {
  0x00a9
};
static PRUnichar const NAME_856[] = {
  'c', 'o', 'p', 'y', 's', 'r', ';'
};
static PRUnichar const VALUE_856[] = {
  0x2117
};
static PRUnichar const NAME_857[] = {
  'c', 'r', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_857[] = {
  0x21b5
};
static PRUnichar const NAME_858[] = {
  'c', 'r', 'o', 's', 's', ';'
};
static PRUnichar const VALUE_858[] = {
  0x2717
};
static PRUnichar const NAME_859[] = {
  'c', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_859[] = {
  0xd835, 0xdcb8
};
static PRUnichar const NAME_860[] = {
  'c', 's', 'u', 'b', ';'
};
static PRUnichar const VALUE_860[] = {
  0x2acf
};
static PRUnichar const NAME_861[] = {
  'c', 's', 'u', 'b', 'e', ';'
};
static PRUnichar const VALUE_861[] = {
  0x2ad1
};
static PRUnichar const NAME_862[] = {
  'c', 's', 'u', 'p', ';'
};
static PRUnichar const VALUE_862[] = {
  0x2ad0
};
static PRUnichar const NAME_863[] = {
  'c', 's', 'u', 'p', 'e', ';'
};
static PRUnichar const VALUE_863[] = {
  0x2ad2
};
static PRUnichar const NAME_864[] = {
  'c', 't', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_864[] = {
  0x22ef
};
static PRUnichar const NAME_865[] = {
  'c', 'u', 'd', 'a', 'r', 'r', 'l', ';'
};
static PRUnichar const VALUE_865[] = {
  0x2938
};
static PRUnichar const NAME_866[] = {
  'c', 'u', 'd', 'a', 'r', 'r', 'r', ';'
};
static PRUnichar const VALUE_866[] = {
  0x2935
};
static PRUnichar const NAME_867[] = {
  'c', 'u', 'e', 'p', 'r', ';'
};
static PRUnichar const VALUE_867[] = {
  0x22de
};
static PRUnichar const NAME_868[] = {
  'c', 'u', 'e', 's', 'c', ';'
};
static PRUnichar const VALUE_868[] = {
  0x22df
};
static PRUnichar const NAME_869[] = {
  'c', 'u', 'l', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_869[] = {
  0x21b6
};
static PRUnichar const NAME_870[] = {
  'c', 'u', 'l', 'a', 'r', 'r', 'p', ';'
};
static PRUnichar const VALUE_870[] = {
  0x293d
};
static PRUnichar const NAME_871[] = {
  'c', 'u', 'p', ';'
};
static PRUnichar const VALUE_871[] = {
  0x222a
};
static PRUnichar const NAME_872[] = {
  'c', 'u', 'p', 'b', 'r', 'c', 'a', 'p', ';'
};
static PRUnichar const VALUE_872[] = {
  0x2a48
};
static PRUnichar const NAME_873[] = {
  'c', 'u', 'p', 'c', 'a', 'p', ';'
};
static PRUnichar const VALUE_873[] = {
  0x2a46
};
static PRUnichar const NAME_874[] = {
  'c', 'u', 'p', 'c', 'u', 'p', ';'
};
static PRUnichar const VALUE_874[] = {
  0x2a4a
};
static PRUnichar const NAME_875[] = {
  'c', 'u', 'p', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_875[] = {
  0x228d
};
static PRUnichar const NAME_876[] = {
  'c', 'u', 'p', 'o', 'r', ';'
};
static PRUnichar const VALUE_876[] = {
  0x2a45
};
static PRUnichar const NAME_877[] = {
  'c', 'u', 'r', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_877[] = {
  0x21b7
};
static PRUnichar const NAME_878[] = {
  'c', 'u', 'r', 'a', 'r', 'r', 'm', ';'
};
static PRUnichar const VALUE_878[] = {
  0x293c
};
static PRUnichar const NAME_879[] = {
  'c', 'u', 'r', 'l', 'y', 'e', 'q', 'p', 'r', 'e', 'c', ';'
};
static PRUnichar const VALUE_879[] = {
  0x22de
};
static PRUnichar const NAME_880[] = {
  'c', 'u', 'r', 'l', 'y', 'e', 'q', 's', 'u', 'c', 'c', ';'
};
static PRUnichar const VALUE_880[] = {
  0x22df
};
static PRUnichar const NAME_881[] = {
  'c', 'u', 'r', 'l', 'y', 'v', 'e', 'e', ';'
};
static PRUnichar const VALUE_881[] = {
  0x22ce
};
static PRUnichar const NAME_882[] = {
  'c', 'u', 'r', 'l', 'y', 'w', 'e', 'd', 'g', 'e', ';'
};
static PRUnichar const VALUE_882[] = {
  0x22cf
};
static PRUnichar const NAME_883[] = {
  'c', 'u', 'r', 'r', 'e', 'n'
};
static PRUnichar const VALUE_883[] = {
  0x00a4
};
static PRUnichar const NAME_884[] = {
  'c', 'u', 'r', 'r', 'e', 'n', ';'
};
static PRUnichar const VALUE_884[] = {
  0x00a4
};
static PRUnichar const NAME_885[] = {
  'c', 'u', 'r', 'v', 'e', 'a', 'r', 'r', 'o', 'w', 'l', 'e', 'f', 't', ';'
};
static PRUnichar const VALUE_885[] = {
  0x21b6
};
static PRUnichar const NAME_886[] = {
  'c', 'u', 'r', 'v', 'e', 'a', 'r', 'r', 'o', 'w', 'r', 'i', 'g', 'h', 't', ';'
};
static PRUnichar const VALUE_886[] = {
  0x21b7
};
static PRUnichar const NAME_887[] = {
  'c', 'u', 'v', 'e', 'e', ';'
};
static PRUnichar const VALUE_887[] = {
  0x22ce
};
static PRUnichar const NAME_888[] = {
  'c', 'u', 'w', 'e', 'd', ';'
};
static PRUnichar const VALUE_888[] = {
  0x22cf
};
static PRUnichar const NAME_889[] = {
  'c', 'w', 'c', 'o', 'n', 'i', 'n', 't', ';'
};
static PRUnichar const VALUE_889[] = {
  0x2232
};
static PRUnichar const NAME_890[] = {
  'c', 'w', 'i', 'n', 't', ';'
};
static PRUnichar const VALUE_890[] = {
  0x2231
};
static PRUnichar const NAME_891[] = {
  'c', 'y', 'l', 'c', 't', 'y', ';'
};
static PRUnichar const VALUE_891[] = {
  0x232d
};
static PRUnichar const NAME_892[] = {
  'd', 'A', 'r', 'r', ';'
};
static PRUnichar const VALUE_892[] = {
  0x21d3
};
static PRUnichar const NAME_893[] = {
  'd', 'H', 'a', 'r', ';'
};
static PRUnichar const VALUE_893[] = {
  0x2965
};
static PRUnichar const NAME_894[] = {
  'd', 'a', 'g', 'g', 'e', 'r', ';'
};
static PRUnichar const VALUE_894[] = {
  0x2020
};
static PRUnichar const NAME_895[] = {
  'd', 'a', 'l', 'e', 't', 'h', ';'
};
static PRUnichar const VALUE_895[] = {
  0x2138
};
static PRUnichar const NAME_896[] = {
  'd', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_896[] = {
  0x2193
};
static PRUnichar const NAME_897[] = {
  'd', 'a', 's', 'h', ';'
};
static PRUnichar const VALUE_897[] = {
  0x2010
};
static PRUnichar const NAME_898[] = {
  'd', 'a', 's', 'h', 'v', ';'
};
static PRUnichar const VALUE_898[] = {
  0x22a3
};
static PRUnichar const NAME_899[] = {
  'd', 'b', 'k', 'a', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_899[] = {
  0x290f
};
static PRUnichar const NAME_900[] = {
  'd', 'b', 'l', 'a', 'c', ';'
};
static PRUnichar const VALUE_900[] = {
  0x02dd
};
static PRUnichar const NAME_901[] = {
  'd', 'c', 'a', 'r', 'o', 'n', ';'
};
static PRUnichar const VALUE_901[] = {
  0x010f
};
static PRUnichar const NAME_902[] = {
  'd', 'c', 'y', ';'
};
static PRUnichar const VALUE_902[] = {
  0x0434
};
static PRUnichar const NAME_903[] = {
  'd', 'd', ';'
};
static PRUnichar const VALUE_903[] = {
  0x2146
};
static PRUnichar const NAME_904[] = {
  'd', 'd', 'a', 'g', 'g', 'e', 'r', ';'
};
static PRUnichar const VALUE_904[] = {
  0x2021
};
static PRUnichar const NAME_905[] = {
  'd', 'd', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_905[] = {
  0x21ca
};
static PRUnichar const NAME_906[] = {
  'd', 'd', 'o', 't', 's', 'e', 'q', ';'
};
static PRUnichar const VALUE_906[] = {
  0x2a77
};
static PRUnichar const NAME_907[] = {
  'd', 'e', 'g'
};
static PRUnichar const VALUE_907[] = {
  0x00b0
};
static PRUnichar const NAME_908[] = {
  'd', 'e', 'g', ';'
};
static PRUnichar const VALUE_908[] = {
  0x00b0
};
static PRUnichar const NAME_909[] = {
  'd', 'e', 'l', 't', 'a', ';'
};
static PRUnichar const VALUE_909[] = {
  0x03b4
};
static PRUnichar const NAME_910[] = {
  'd', 'e', 'm', 'p', 't', 'y', 'v', ';'
};
static PRUnichar const VALUE_910[] = {
  0x29b1
};
static PRUnichar const NAME_911[] = {
  'd', 'f', 'i', 's', 'h', 't', ';'
};
static PRUnichar const VALUE_911[] = {
  0x297f
};
static PRUnichar const NAME_912[] = {
  'd', 'f', 'r', ';'
};
static PRUnichar const VALUE_912[] = {
  0xd835, 0xdd21
};
static PRUnichar const NAME_913[] = {
  'd', 'h', 'a', 'r', 'l', ';'
};
static PRUnichar const VALUE_913[] = {
  0x21c3
};
static PRUnichar const NAME_914[] = {
  'd', 'h', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_914[] = {
  0x21c2
};
static PRUnichar const NAME_915[] = {
  'd', 'i', 'a', 'm', ';'
};
static PRUnichar const VALUE_915[] = {
  0x22c4
};
static PRUnichar const NAME_916[] = {
  'd', 'i', 'a', 'm', 'o', 'n', 'd', ';'
};
static PRUnichar const VALUE_916[] = {
  0x22c4
};
static PRUnichar const NAME_917[] = {
  'd', 'i', 'a', 'm', 'o', 'n', 'd', 's', 'u', 'i', 't', ';'
};
static PRUnichar const VALUE_917[] = {
  0x2666
};
static PRUnichar const NAME_918[] = {
  'd', 'i', 'a', 'm', 's', ';'
};
static PRUnichar const VALUE_918[] = {
  0x2666
};
static PRUnichar const NAME_919[] = {
  'd', 'i', 'e', ';'
};
static PRUnichar const VALUE_919[] = {
  0x00a8
};
static PRUnichar const NAME_920[] = {
  'd', 'i', 'g', 'a', 'm', 'm', 'a', ';'
};
static PRUnichar const VALUE_920[] = {
  0x03dd
};
static PRUnichar const NAME_921[] = {
  'd', 'i', 's', 'i', 'n', ';'
};
static PRUnichar const VALUE_921[] = {
  0x22f2
};
static PRUnichar const NAME_922[] = {
  'd', 'i', 'v', ';'
};
static PRUnichar const VALUE_922[] = {
  0x00f7
};
static PRUnichar const NAME_923[] = {
  'd', 'i', 'v', 'i', 'd', 'e'
};
static PRUnichar const VALUE_923[] = {
  0x00f7
};
static PRUnichar const NAME_924[] = {
  'd', 'i', 'v', 'i', 'd', 'e', ';'
};
static PRUnichar const VALUE_924[] = {
  0x00f7
};
static PRUnichar const NAME_925[] = {
  'd', 'i', 'v', 'i', 'd', 'e', 'o', 'n', 't', 'i', 'm', 'e', 's', ';'
};
static PRUnichar const VALUE_925[] = {
  0x22c7
};
static PRUnichar const NAME_926[] = {
  'd', 'i', 'v', 'o', 'n', 'x', ';'
};
static PRUnichar const VALUE_926[] = {
  0x22c7
};
static PRUnichar const NAME_927[] = {
  'd', 'j', 'c', 'y', ';'
};
static PRUnichar const VALUE_927[] = {
  0x0452
};
static PRUnichar const NAME_928[] = {
  'd', 'l', 'c', 'o', 'r', 'n', ';'
};
static PRUnichar const VALUE_928[] = {
  0x231e
};
static PRUnichar const NAME_929[] = {
  'd', 'l', 'c', 'r', 'o', 'p', ';'
};
static PRUnichar const VALUE_929[] = {
  0x230d
};
static PRUnichar const NAME_930[] = {
  'd', 'o', 'l', 'l', 'a', 'r', ';'
};
static PRUnichar const VALUE_930[] = {
  0x0024
};
static PRUnichar const NAME_931[] = {
  'd', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_931[] = {
  0xd835, 0xdd55
};
static PRUnichar const NAME_932[] = {
  'd', 'o', 't', ';'
};
static PRUnichar const VALUE_932[] = {
  0x02d9
};
static PRUnichar const NAME_933[] = {
  'd', 'o', 't', 'e', 'q', ';'
};
static PRUnichar const VALUE_933[] = {
  0x2250
};
static PRUnichar const NAME_934[] = {
  'd', 'o', 't', 'e', 'q', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_934[] = {
  0x2251
};
static PRUnichar const NAME_935[] = {
  'd', 'o', 't', 'm', 'i', 'n', 'u', 's', ';'
};
static PRUnichar const VALUE_935[] = {
  0x2238
};
static PRUnichar const NAME_936[] = {
  'd', 'o', 't', 'p', 'l', 'u', 's', ';'
};
static PRUnichar const VALUE_936[] = {
  0x2214
};
static PRUnichar const NAME_937[] = {
  'd', 'o', 't', 's', 'q', 'u', 'a', 'r', 'e', ';'
};
static PRUnichar const VALUE_937[] = {
  0x22a1
};
static PRUnichar const NAME_938[] = {
  'd', 'o', 'u', 'b', 'l', 'e', 'b', 'a', 'r', 'w', 'e', 'd', 'g', 'e', ';'
};
static PRUnichar const VALUE_938[] = {
  0x2306
};
static PRUnichar const NAME_939[] = {
  'd', 'o', 'w', 'n', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_939[] = {
  0x2193
};
static PRUnichar const NAME_940[] = {
  'd', 'o', 'w', 'n', 'd', 'o', 'w', 'n', 'a', 'r', 'r', 'o', 'w', 's', ';'
};
static PRUnichar const VALUE_940[] = {
  0x21ca
};
static PRUnichar const NAME_941[] = {
  'd', 'o', 'w', 'n', 'h', 'a', 'r', 'p', 'o', 'o', 'n', 'l', 'e', 'f', 't', ';'
};
static PRUnichar const VALUE_941[] = {
  0x21c3
};
static PRUnichar const NAME_942[] = {
  'd', 'o', 'w', 'n', 'h', 'a', 'r', 'p', 'o', 'o', 'n', 'r', 'i', 'g', 'h', 't', ';'
};
static PRUnichar const VALUE_942[] = {
  0x21c2
};
static PRUnichar const NAME_943[] = {
  'd', 'r', 'b', 'k', 'a', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_943[] = {
  0x2910
};
static PRUnichar const NAME_944[] = {
  'd', 'r', 'c', 'o', 'r', 'n', ';'
};
static PRUnichar const VALUE_944[] = {
  0x231f
};
static PRUnichar const NAME_945[] = {
  'd', 'r', 'c', 'r', 'o', 'p', ';'
};
static PRUnichar const VALUE_945[] = {
  0x230c
};
static PRUnichar const NAME_946[] = {
  'd', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_946[] = {
  0xd835, 0xdcb9
};
static PRUnichar const NAME_947[] = {
  'd', 's', 'c', 'y', ';'
};
static PRUnichar const VALUE_947[] = {
  0x0455
};
static PRUnichar const NAME_948[] = {
  'd', 's', 'o', 'l', ';'
};
static PRUnichar const VALUE_948[] = {
  0x29f6
};
static PRUnichar const NAME_949[] = {
  'd', 's', 't', 'r', 'o', 'k', ';'
};
static PRUnichar const VALUE_949[] = {
  0x0111
};
static PRUnichar const NAME_950[] = {
  'd', 't', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_950[] = {
  0x22f1
};
static PRUnichar const NAME_951[] = {
  'd', 't', 'r', 'i', ';'
};
static PRUnichar const VALUE_951[] = {
  0x25bf
};
static PRUnichar const NAME_952[] = {
  'd', 't', 'r', 'i', 'f', ';'
};
static PRUnichar const VALUE_952[] = {
  0x25be
};
static PRUnichar const NAME_953[] = {
  'd', 'u', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_953[] = {
  0x21f5
};
static PRUnichar const NAME_954[] = {
  'd', 'u', 'h', 'a', 'r', ';'
};
static PRUnichar const VALUE_954[] = {
  0x296f
};
static PRUnichar const NAME_955[] = {
  'd', 'w', 'a', 'n', 'g', 'l', 'e', ';'
};
static PRUnichar const VALUE_955[] = {
  0x29a6
};
static PRUnichar const NAME_956[] = {
  'd', 'z', 'c', 'y', ';'
};
static PRUnichar const VALUE_956[] = {
  0x045f
};
static PRUnichar const NAME_957[] = {
  'd', 'z', 'i', 'g', 'r', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_957[] = {
  0x27ff
};
static PRUnichar const NAME_958[] = {
  'e', 'D', 'D', 'o', 't', ';'
};
static PRUnichar const VALUE_958[] = {
  0x2a77
};
static PRUnichar const NAME_959[] = {
  'e', 'D', 'o', 't', ';'
};
static PRUnichar const VALUE_959[] = {
  0x2251
};
static PRUnichar const NAME_960[] = {
  'e', 'a', 'c', 'u', 't', 'e'
};
static PRUnichar const VALUE_960[] = {
  0x00e9
};
static PRUnichar const NAME_961[] = {
  'e', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_961[] = {
  0x00e9
};
static PRUnichar const NAME_962[] = {
  'e', 'a', 's', 't', 'e', 'r', ';'
};
static PRUnichar const VALUE_962[] = {
  0x2a6e
};
static PRUnichar const NAME_963[] = {
  'e', 'c', 'a', 'r', 'o', 'n', ';'
};
static PRUnichar const VALUE_963[] = {
  0x011b
};
static PRUnichar const NAME_964[] = {
  'e', 'c', 'i', 'r', ';'
};
static PRUnichar const VALUE_964[] = {
  0x2256
};
static PRUnichar const NAME_965[] = {
  'e', 'c', 'i', 'r', 'c'
};
static PRUnichar const VALUE_965[] = {
  0x00ea
};
static PRUnichar const NAME_966[] = {
  'e', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_966[] = {
  0x00ea
};
static PRUnichar const NAME_967[] = {
  'e', 'c', 'o', 'l', 'o', 'n', ';'
};
static PRUnichar const VALUE_967[] = {
  0x2255
};
static PRUnichar const NAME_968[] = {
  'e', 'c', 'y', ';'
};
static PRUnichar const VALUE_968[] = {
  0x044d
};
static PRUnichar const NAME_969[] = {
  'e', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_969[] = {
  0x0117
};
static PRUnichar const NAME_970[] = {
  'e', 'e', ';'
};
static PRUnichar const VALUE_970[] = {
  0x2147
};
static PRUnichar const NAME_971[] = {
  'e', 'f', 'D', 'o', 't', ';'
};
static PRUnichar const VALUE_971[] = {
  0x2252
};
static PRUnichar const NAME_972[] = {
  'e', 'f', 'r', ';'
};
static PRUnichar const VALUE_972[] = {
  0xd835, 0xdd22
};
static PRUnichar const NAME_973[] = {
  'e', 'g', ';'
};
static PRUnichar const VALUE_973[] = {
  0x2a9a
};
static PRUnichar const NAME_974[] = {
  'e', 'g', 'r', 'a', 'v', 'e'
};
static PRUnichar const VALUE_974[] = {
  0x00e8
};
static PRUnichar const NAME_975[] = {
  'e', 'g', 'r', 'a', 'v', 'e', ';'
};
static PRUnichar const VALUE_975[] = {
  0x00e8
};
static PRUnichar const NAME_976[] = {
  'e', 'g', 's', ';'
};
static PRUnichar const VALUE_976[] = {
  0x2a96
};
static PRUnichar const NAME_977[] = {
  'e', 'g', 's', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_977[] = {
  0x2a98
};
static PRUnichar const NAME_978[] = {
  'e', 'l', ';'
};
static PRUnichar const VALUE_978[] = {
  0x2a99
};
static PRUnichar const NAME_979[] = {
  'e', 'l', 'i', 'n', 't', 'e', 'r', 's', ';'
};
static PRUnichar const VALUE_979[] = {
  0x23e7
};
static PRUnichar const NAME_980[] = {
  'e', 'l', 'l', ';'
};
static PRUnichar const VALUE_980[] = {
  0x2113
};
static PRUnichar const NAME_981[] = {
  'e', 'l', 's', ';'
};
static PRUnichar const VALUE_981[] = {
  0x2a95
};
static PRUnichar const NAME_982[] = {
  'e', 'l', 's', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_982[] = {
  0x2a97
};
static PRUnichar const NAME_983[] = {
  'e', 'm', 'a', 'c', 'r', ';'
};
static PRUnichar const VALUE_983[] = {
  0x0113
};
static PRUnichar const NAME_984[] = {
  'e', 'm', 'p', 't', 'y', ';'
};
static PRUnichar const VALUE_984[] = {
  0x2205
};
static PRUnichar const NAME_985[] = {
  'e', 'm', 'p', 't', 'y', 's', 'e', 't', ';'
};
static PRUnichar const VALUE_985[] = {
  0x2205
};
static PRUnichar const NAME_986[] = {
  'e', 'm', 'p', 't', 'y', 'v', ';'
};
static PRUnichar const VALUE_986[] = {
  0x2205
};
static PRUnichar const NAME_987[] = {
  'e', 'm', 's', 'p', '1', '3', ';'
};
static PRUnichar const VALUE_987[] = {
  0x2004
};
static PRUnichar const NAME_988[] = {
  'e', 'm', 's', 'p', '1', '4', ';'
};
static PRUnichar const VALUE_988[] = {
  0x2005
};
static PRUnichar const NAME_989[] = {
  'e', 'm', 's', 'p', ';'
};
static PRUnichar const VALUE_989[] = {
  0x2003
};
static PRUnichar const NAME_990[] = {
  'e', 'n', 'g', ';'
};
static PRUnichar const VALUE_990[] = {
  0x014b
};
static PRUnichar const NAME_991[] = {
  'e', 'n', 's', 'p', ';'
};
static PRUnichar const VALUE_991[] = {
  0x2002
};
static PRUnichar const NAME_992[] = {
  'e', 'o', 'g', 'o', 'n', ';'
};
static PRUnichar const VALUE_992[] = {
  0x0119
};
static PRUnichar const NAME_993[] = {
  'e', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_993[] = {
  0xd835, 0xdd56
};
static PRUnichar const NAME_994[] = {
  'e', 'p', 'a', 'r', ';'
};
static PRUnichar const VALUE_994[] = {
  0x22d5
};
static PRUnichar const NAME_995[] = {
  'e', 'p', 'a', 'r', 's', 'l', ';'
};
static PRUnichar const VALUE_995[] = {
  0x29e3
};
static PRUnichar const NAME_996[] = {
  'e', 'p', 'l', 'u', 's', ';'
};
static PRUnichar const VALUE_996[] = {
  0x2a71
};
static PRUnichar const NAME_997[] = {
  'e', 'p', 's', 'i', ';'
};
static PRUnichar const VALUE_997[] = {
  0x03f5
};
static PRUnichar const NAME_998[] = {
  'e', 'p', 's', 'i', 'l', 'o', 'n', ';'
};
static PRUnichar const VALUE_998[] = {
  0x03b5
};
static PRUnichar const NAME_999[] = {
  'e', 'p', 's', 'i', 'v', ';'
};
static PRUnichar const VALUE_999[] = {
  0x03b5
};
static PRUnichar const NAME_1000[] = {
  'e', 'q', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_1000[] = {
  0x2256
};
static PRUnichar const NAME_1001[] = {
  'e', 'q', 'c', 'o', 'l', 'o', 'n', ';'
};
static PRUnichar const VALUE_1001[] = {
  0x2255
};
static PRUnichar const NAME_1002[] = {
  'e', 'q', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1002[] = {
  0x2242
};
static PRUnichar const NAME_1003[] = {
  'e', 'q', 's', 'l', 'a', 'n', 't', 'g', 't', 'r', ';'
};
static PRUnichar const VALUE_1003[] = {
  0x2a96
};
static PRUnichar const NAME_1004[] = {
  'e', 'q', 's', 'l', 'a', 'n', 't', 'l', 'e', 's', 's', ';'
};
static PRUnichar const VALUE_1004[] = {
  0x2a95
};
static PRUnichar const NAME_1005[] = {
  'e', 'q', 'u', 'a', 'l', 's', ';'
};
static PRUnichar const VALUE_1005[] = {
  0x003d
};
static PRUnichar const NAME_1006[] = {
  'e', 'q', 'u', 'e', 's', 't', ';'
};
static PRUnichar const VALUE_1006[] = {
  0x225f
};
static PRUnichar const NAME_1007[] = {
  'e', 'q', 'u', 'i', 'v', ';'
};
static PRUnichar const VALUE_1007[] = {
  0x2261
};
static PRUnichar const NAME_1008[] = {
  'e', 'q', 'u', 'i', 'v', 'D', 'D', ';'
};
static PRUnichar const VALUE_1008[] = {
  0x2a78
};
static PRUnichar const NAME_1009[] = {
  'e', 'q', 'v', 'p', 'a', 'r', 's', 'l', ';'
};
static PRUnichar const VALUE_1009[] = {
  0x29e5
};
static PRUnichar const NAME_1010[] = {
  'e', 'r', 'D', 'o', 't', ';'
};
static PRUnichar const VALUE_1010[] = {
  0x2253
};
static PRUnichar const NAME_1011[] = {
  'e', 'r', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1011[] = {
  0x2971
};
static PRUnichar const NAME_1012[] = {
  'e', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_1012[] = {
  0x212f
};
static PRUnichar const NAME_1013[] = {
  'e', 's', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_1013[] = {
  0x2250
};
static PRUnichar const NAME_1014[] = {
  'e', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1014[] = {
  0x2242
};
static PRUnichar const NAME_1015[] = {
  'e', 't', 'a', ';'
};
static PRUnichar const VALUE_1015[] = {
  0x03b7
};
static PRUnichar const NAME_1016[] = {
  'e', 't', 'h'
};
static PRUnichar const VALUE_1016[] = {
  0x00f0
};
static PRUnichar const NAME_1017[] = {
  'e', 't', 'h', ';'
};
static PRUnichar const VALUE_1017[] = {
  0x00f0
};
static PRUnichar const NAME_1018[] = {
  'e', 'u', 'm', 'l'
};
static PRUnichar const VALUE_1018[] = {
  0x00eb
};
static PRUnichar const NAME_1019[] = {
  'e', 'u', 'm', 'l', ';'
};
static PRUnichar const VALUE_1019[] = {
  0x00eb
};
static PRUnichar const NAME_1020[] = {
  'e', 'u', 'r', 'o', ';'
};
static PRUnichar const VALUE_1020[] = {
  0x20ac
};
static PRUnichar const NAME_1021[] = {
  'e', 'x', 'c', 'l', ';'
};
static PRUnichar const VALUE_1021[] = {
  0x0021
};
static PRUnichar const NAME_1022[] = {
  'e', 'x', 'i', 's', 't', ';'
};
static PRUnichar const VALUE_1022[] = {
  0x2203
};
static PRUnichar const NAME_1023[] = {
  'e', 'x', 'p', 'e', 'c', 't', 'a', 't', 'i', 'o', 'n', ';'
};
static PRUnichar const VALUE_1023[] = {
  0x2130
};
static PRUnichar const NAME_1024[] = {
  'e', 'x', 'p', 'o', 'n', 'e', 'n', 't', 'i', 'a', 'l', 'e', ';'
};
static PRUnichar const VALUE_1024[] = {
  0x2147
};
static PRUnichar const NAME_1025[] = {
  'f', 'a', 'l', 'l', 'i', 'n', 'g', 'd', 'o', 't', 's', 'e', 'q', ';'
};
static PRUnichar const VALUE_1025[] = {
  0x2252
};
static PRUnichar const NAME_1026[] = {
  'f', 'c', 'y', ';'
};
static PRUnichar const VALUE_1026[] = {
  0x0444
};
static PRUnichar const NAME_1027[] = {
  'f', 'e', 'm', 'a', 'l', 'e', ';'
};
static PRUnichar const VALUE_1027[] = {
  0x2640
};
static PRUnichar const NAME_1028[] = {
  'f', 'f', 'i', 'l', 'i', 'g', ';'
};
static PRUnichar const VALUE_1028[] = {
  0xfb03
};
static PRUnichar const NAME_1029[] = {
  'f', 'f', 'l', 'i', 'g', ';'
};
static PRUnichar const VALUE_1029[] = {
  0xfb00
};
static PRUnichar const NAME_1030[] = {
  'f', 'f', 'l', 'l', 'i', 'g', ';'
};
static PRUnichar const VALUE_1030[] = {
  0xfb04
};
static PRUnichar const NAME_1031[] = {
  'f', 'f', 'r', ';'
};
static PRUnichar const VALUE_1031[] = {
  0xd835, 0xdd23
};
static PRUnichar const NAME_1032[] = {
  'f', 'i', 'l', 'i', 'g', ';'
};
static PRUnichar const VALUE_1032[] = {
  0xfb01
};
static PRUnichar const NAME_1033[] = {
  'f', 'l', 'a', 't', ';'
};
static PRUnichar const VALUE_1033[] = {
  0x266d
};
static PRUnichar const NAME_1034[] = {
  'f', 'l', 'l', 'i', 'g', ';'
};
static PRUnichar const VALUE_1034[] = {
  0xfb02
};
static PRUnichar const NAME_1035[] = {
  'f', 'l', 't', 'n', 's', ';'
};
static PRUnichar const VALUE_1035[] = {
  0x25b1
};
static PRUnichar const NAME_1036[] = {
  'f', 'n', 'o', 'f', ';'
};
static PRUnichar const VALUE_1036[] = {
  0x0192
};
static PRUnichar const NAME_1037[] = {
  'f', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_1037[] = {
  0xd835, 0xdd57
};
static PRUnichar const NAME_1038[] = {
  'f', 'o', 'r', 'a', 'l', 'l', ';'
};
static PRUnichar const VALUE_1038[] = {
  0x2200
};
static PRUnichar const NAME_1039[] = {
  'f', 'o', 'r', 'k', ';'
};
static PRUnichar const VALUE_1039[] = {
  0x22d4
};
static PRUnichar const NAME_1040[] = {
  'f', 'o', 'r', 'k', 'v', ';'
};
static PRUnichar const VALUE_1040[] = {
  0x2ad9
};
static PRUnichar const NAME_1041[] = {
  'f', 'p', 'a', 'r', 't', 'i', 'n', 't', ';'
};
static PRUnichar const VALUE_1041[] = {
  0x2a0d
};
static PRUnichar const NAME_1042[] = {
  'f', 'r', 'a', 'c', '1', '2'
};
static PRUnichar const VALUE_1042[] = {
  0x00bd
};
static PRUnichar const NAME_1043[] = {
  'f', 'r', 'a', 'c', '1', '2', ';'
};
static PRUnichar const VALUE_1043[] = {
  0x00bd
};
static PRUnichar const NAME_1044[] = {
  'f', 'r', 'a', 'c', '1', '3', ';'
};
static PRUnichar const VALUE_1044[] = {
  0x2153
};
static PRUnichar const NAME_1045[] = {
  'f', 'r', 'a', 'c', '1', '4'
};
static PRUnichar const VALUE_1045[] = {
  0x00bc
};
static PRUnichar const NAME_1046[] = {
  'f', 'r', 'a', 'c', '1', '4', ';'
};
static PRUnichar const VALUE_1046[] = {
  0x00bc
};
static PRUnichar const NAME_1047[] = {
  'f', 'r', 'a', 'c', '1', '5', ';'
};
static PRUnichar const VALUE_1047[] = {
  0x2155
};
static PRUnichar const NAME_1048[] = {
  'f', 'r', 'a', 'c', '1', '6', ';'
};
static PRUnichar const VALUE_1048[] = {
  0x2159
};
static PRUnichar const NAME_1049[] = {
  'f', 'r', 'a', 'c', '1', '8', ';'
};
static PRUnichar const VALUE_1049[] = {
  0x215b
};
static PRUnichar const NAME_1050[] = {
  'f', 'r', 'a', 'c', '2', '3', ';'
};
static PRUnichar const VALUE_1050[] = {
  0x2154
};
static PRUnichar const NAME_1051[] = {
  'f', 'r', 'a', 'c', '2', '5', ';'
};
static PRUnichar const VALUE_1051[] = {
  0x2156
};
static PRUnichar const NAME_1052[] = {
  'f', 'r', 'a', 'c', '3', '4'
};
static PRUnichar const VALUE_1052[] = {
  0x00be
};
static PRUnichar const NAME_1053[] = {
  'f', 'r', 'a', 'c', '3', '4', ';'
};
static PRUnichar const VALUE_1053[] = {
  0x00be
};
static PRUnichar const NAME_1054[] = {
  'f', 'r', 'a', 'c', '3', '5', ';'
};
static PRUnichar const VALUE_1054[] = {
  0x2157
};
static PRUnichar const NAME_1055[] = {
  'f', 'r', 'a', 'c', '3', '8', ';'
};
static PRUnichar const VALUE_1055[] = {
  0x215c
};
static PRUnichar const NAME_1056[] = {
  'f', 'r', 'a', 'c', '4', '5', ';'
};
static PRUnichar const VALUE_1056[] = {
  0x2158
};
static PRUnichar const NAME_1057[] = {
  'f', 'r', 'a', 'c', '5', '6', ';'
};
static PRUnichar const VALUE_1057[] = {
  0x215a
};
static PRUnichar const NAME_1058[] = {
  'f', 'r', 'a', 'c', '5', '8', ';'
};
static PRUnichar const VALUE_1058[] = {
  0x215d
};
static PRUnichar const NAME_1059[] = {
  'f', 'r', 'a', 'c', '7', '8', ';'
};
static PRUnichar const VALUE_1059[] = {
  0x215e
};
static PRUnichar const NAME_1060[] = {
  'f', 'r', 'a', 's', 'l', ';'
};
static PRUnichar const VALUE_1060[] = {
  0x2044
};
static PRUnichar const NAME_1061[] = {
  'f', 'r', 'o', 'w', 'n', ';'
};
static PRUnichar const VALUE_1061[] = {
  0x2322
};
static PRUnichar const NAME_1062[] = {
  'f', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_1062[] = {
  0xd835, 0xdcbb
};
static PRUnichar const NAME_1063[] = {
  'g', 'E', ';'
};
static PRUnichar const VALUE_1063[] = {
  0x2267
};
static PRUnichar const NAME_1064[] = {
  'g', 'E', 'l', ';'
};
static PRUnichar const VALUE_1064[] = {
  0x2a8c
};
static PRUnichar const NAME_1065[] = {
  'g', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_1065[] = {
  0x01f5
};
static PRUnichar const NAME_1066[] = {
  'g', 'a', 'm', 'm', 'a', ';'
};
static PRUnichar const VALUE_1066[] = {
  0x03b3
};
static PRUnichar const NAME_1067[] = {
  'g', 'a', 'm', 'm', 'a', 'd', ';'
};
static PRUnichar const VALUE_1067[] = {
  0x03dd
};
static PRUnichar const NAME_1068[] = {
  'g', 'a', 'p', ';'
};
static PRUnichar const VALUE_1068[] = {
  0x2a86
};
static PRUnichar const NAME_1069[] = {
  'g', 'b', 'r', 'e', 'v', 'e', ';'
};
static PRUnichar const VALUE_1069[] = {
  0x011f
};
static PRUnichar const NAME_1070[] = {
  'g', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_1070[] = {
  0x011d
};
static PRUnichar const NAME_1071[] = {
  'g', 'c', 'y', ';'
};
static PRUnichar const VALUE_1071[] = {
  0x0433
};
static PRUnichar const NAME_1072[] = {
  'g', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_1072[] = {
  0x0121
};
static PRUnichar const NAME_1073[] = {
  'g', 'e', ';'
};
static PRUnichar const VALUE_1073[] = {
  0x2265
};
static PRUnichar const NAME_1074[] = {
  'g', 'e', 'l', ';'
};
static PRUnichar const VALUE_1074[] = {
  0x22db
};
static PRUnichar const NAME_1075[] = {
  'g', 'e', 'q', ';'
};
static PRUnichar const VALUE_1075[] = {
  0x2265
};
static PRUnichar const NAME_1076[] = {
  'g', 'e', 'q', 'q', ';'
};
static PRUnichar const VALUE_1076[] = {
  0x2267
};
static PRUnichar const NAME_1077[] = {
  'g', 'e', 'q', 's', 'l', 'a', 'n', 't', ';'
};
static PRUnichar const VALUE_1077[] = {
  0x2a7e
};
static PRUnichar const NAME_1078[] = {
  'g', 'e', 's', ';'
};
static PRUnichar const VALUE_1078[] = {
  0x2a7e
};
static PRUnichar const NAME_1079[] = {
  'g', 'e', 's', 'c', 'c', ';'
};
static PRUnichar const VALUE_1079[] = {
  0x2aa9
};
static PRUnichar const NAME_1080[] = {
  'g', 'e', 's', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_1080[] = {
  0x2a80
};
static PRUnichar const NAME_1081[] = {
  'g', 'e', 's', 'd', 'o', 't', 'o', ';'
};
static PRUnichar const VALUE_1081[] = {
  0x2a82
};
static PRUnichar const NAME_1082[] = {
  'g', 'e', 's', 'd', 'o', 't', 'o', 'l', ';'
};
static PRUnichar const VALUE_1082[] = {
  0x2a84
};
static PRUnichar const NAME_1083[] = {
  'g', 'e', 's', 'l', 'e', 's', ';'
};
static PRUnichar const VALUE_1083[] = {
  0x2a94
};
static PRUnichar const NAME_1084[] = {
  'g', 'f', 'r', ';'
};
static PRUnichar const VALUE_1084[] = {
  0xd835, 0xdd24
};
static PRUnichar const NAME_1085[] = {
  'g', 'g', ';'
};
static PRUnichar const VALUE_1085[] = {
  0x226b
};
static PRUnichar const NAME_1086[] = {
  'g', 'g', 'g', ';'
};
static PRUnichar const VALUE_1086[] = {
  0x22d9
};
static PRUnichar const NAME_1087[] = {
  'g', 'i', 'm', 'e', 'l', ';'
};
static PRUnichar const VALUE_1087[] = {
  0x2137
};
static PRUnichar const NAME_1088[] = {
  'g', 'j', 'c', 'y', ';'
};
static PRUnichar const VALUE_1088[] = {
  0x0453
};
static PRUnichar const NAME_1089[] = {
  'g', 'l', ';'
};
static PRUnichar const VALUE_1089[] = {
  0x2277
};
static PRUnichar const NAME_1090[] = {
  'g', 'l', 'E', ';'
};
static PRUnichar const VALUE_1090[] = {
  0x2a92
};
static PRUnichar const NAME_1091[] = {
  'g', 'l', 'a', ';'
};
static PRUnichar const VALUE_1091[] = {
  0x2aa5
};
static PRUnichar const NAME_1092[] = {
  'g', 'l', 'j', ';'
};
static PRUnichar const VALUE_1092[] = {
  0x2aa4
};
static PRUnichar const NAME_1093[] = {
  'g', 'n', 'E', ';'
};
static PRUnichar const VALUE_1093[] = {
  0x2269
};
static PRUnichar const NAME_1094[] = {
  'g', 'n', 'a', 'p', ';'
};
static PRUnichar const VALUE_1094[] = {
  0x2a8a
};
static PRUnichar const NAME_1095[] = {
  'g', 'n', 'a', 'p', 'p', 'r', 'o', 'x', ';'
};
static PRUnichar const VALUE_1095[] = {
  0x2a8a
};
static PRUnichar const NAME_1096[] = {
  'g', 'n', 'e', ';'
};
static PRUnichar const VALUE_1096[] = {
  0x2a88
};
static PRUnichar const NAME_1097[] = {
  'g', 'n', 'e', 'q', ';'
};
static PRUnichar const VALUE_1097[] = {
  0x2a88
};
static PRUnichar const NAME_1098[] = {
  'g', 'n', 'e', 'q', 'q', ';'
};
static PRUnichar const VALUE_1098[] = {
  0x2269
};
static PRUnichar const NAME_1099[] = {
  'g', 'n', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1099[] = {
  0x22e7
};
static PRUnichar const NAME_1100[] = {
  'g', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_1100[] = {
  0xd835, 0xdd58
};
static PRUnichar const NAME_1101[] = {
  'g', 'r', 'a', 'v', 'e', ';'
};
static PRUnichar const VALUE_1101[] = {
  0x0060
};
static PRUnichar const NAME_1102[] = {
  'g', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_1102[] = {
  0x210a
};
static PRUnichar const NAME_1103[] = {
  'g', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1103[] = {
  0x2273
};
static PRUnichar const NAME_1104[] = {
  'g', 's', 'i', 'm', 'e', ';'
};
static PRUnichar const VALUE_1104[] = {
  0x2a8e
};
static PRUnichar const NAME_1105[] = {
  'g', 's', 'i', 'm', 'l', ';'
};
static PRUnichar const VALUE_1105[] = {
  0x2a90
};
static PRUnichar const NAME_1106[] = {
  'g', 't'
};
static PRUnichar const VALUE_1106[] = {
  0x003e
};
static PRUnichar const NAME_1107[] = {
  'g', 't', ';'
};
static PRUnichar const VALUE_1107[] = {
  0x003e
};
static PRUnichar const NAME_1108[] = {
  'g', 't', 'c', 'c', ';'
};
static PRUnichar const VALUE_1108[] = {
  0x2aa7
};
static PRUnichar const NAME_1109[] = {
  'g', 't', 'c', 'i', 'r', ';'
};
static PRUnichar const VALUE_1109[] = {
  0x2a7a
};
static PRUnichar const NAME_1110[] = {
  'g', 't', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_1110[] = {
  0x22d7
};
static PRUnichar const NAME_1111[] = {
  'g', 't', 'l', 'P', 'a', 'r', ';'
};
static PRUnichar const VALUE_1111[] = {
  0x2995
};
static PRUnichar const NAME_1112[] = {
  'g', 't', 'q', 'u', 'e', 's', 't', ';'
};
static PRUnichar const VALUE_1112[] = {
  0x2a7c
};
static PRUnichar const NAME_1113[] = {
  'g', 't', 'r', 'a', 'p', 'p', 'r', 'o', 'x', ';'
};
static PRUnichar const VALUE_1113[] = {
  0x2a86
};
static PRUnichar const NAME_1114[] = {
  'g', 't', 'r', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1114[] = {
  0x2978
};
static PRUnichar const NAME_1115[] = {
  'g', 't', 'r', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_1115[] = {
  0x22d7
};
static PRUnichar const NAME_1116[] = {
  'g', 't', 'r', 'e', 'q', 'l', 'e', 's', 's', ';'
};
static PRUnichar const VALUE_1116[] = {
  0x22db
};
static PRUnichar const NAME_1117[] = {
  'g', 't', 'r', 'e', 'q', 'q', 'l', 'e', 's', 's', ';'
};
static PRUnichar const VALUE_1117[] = {
  0x2a8c
};
static PRUnichar const NAME_1118[] = {
  'g', 't', 'r', 'l', 'e', 's', 's', ';'
};
static PRUnichar const VALUE_1118[] = {
  0x2277
};
static PRUnichar const NAME_1119[] = {
  'g', 't', 'r', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1119[] = {
  0x2273
};
static PRUnichar const NAME_1120[] = {
  'h', 'A', 'r', 'r', ';'
};
static PRUnichar const VALUE_1120[] = {
  0x21d4
};
static PRUnichar const NAME_1121[] = {
  'h', 'a', 'i', 'r', 's', 'p', ';'
};
static PRUnichar const VALUE_1121[] = {
  0x200a
};
static PRUnichar const NAME_1122[] = {
  'h', 'a', 'l', 'f', ';'
};
static PRUnichar const VALUE_1122[] = {
  0x00bd
};
static PRUnichar const NAME_1123[] = {
  'h', 'a', 'm', 'i', 'l', 't', ';'
};
static PRUnichar const VALUE_1123[] = {
  0x210b
};
static PRUnichar const NAME_1124[] = {
  'h', 'a', 'r', 'd', 'c', 'y', ';'
};
static PRUnichar const VALUE_1124[] = {
  0x044a
};
static PRUnichar const NAME_1125[] = {
  'h', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1125[] = {
  0x2194
};
static PRUnichar const NAME_1126[] = {
  'h', 'a', 'r', 'r', 'c', 'i', 'r', ';'
};
static PRUnichar const VALUE_1126[] = {
  0x2948
};
static PRUnichar const NAME_1127[] = {
  'h', 'a', 'r', 'r', 'w', ';'
};
static PRUnichar const VALUE_1127[] = {
  0x21ad
};
static PRUnichar const NAME_1128[] = {
  'h', 'b', 'a', 'r', ';'
};
static PRUnichar const VALUE_1128[] = {
  0x210f
};
static PRUnichar const NAME_1129[] = {
  'h', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_1129[] = {
  0x0125
};
static PRUnichar const NAME_1130[] = {
  'h', 'e', 'a', 'r', 't', 's', ';'
};
static PRUnichar const VALUE_1130[] = {
  0x2665
};
static PRUnichar const NAME_1131[] = {
  'h', 'e', 'a', 'r', 't', 's', 'u', 'i', 't', ';'
};
static PRUnichar const VALUE_1131[] = {
  0x2665
};
static PRUnichar const NAME_1132[] = {
  'h', 'e', 'l', 'l', 'i', 'p', ';'
};
static PRUnichar const VALUE_1132[] = {
  0x2026
};
static PRUnichar const NAME_1133[] = {
  'h', 'e', 'r', 'c', 'o', 'n', ';'
};
static PRUnichar const VALUE_1133[] = {
  0x22b9
};
static PRUnichar const NAME_1134[] = {
  'h', 'f', 'r', ';'
};
static PRUnichar const VALUE_1134[] = {
  0xd835, 0xdd25
};
static PRUnichar const NAME_1135[] = {
  'h', 'k', 's', 'e', 'a', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_1135[] = {
  0x2925
};
static PRUnichar const NAME_1136[] = {
  'h', 'k', 's', 'w', 'a', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_1136[] = {
  0x2926
};
static PRUnichar const NAME_1137[] = {
  'h', 'o', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1137[] = {
  0x21ff
};
static PRUnichar const NAME_1138[] = {
  'h', 'o', 'm', 't', 'h', 't', ';'
};
static PRUnichar const VALUE_1138[] = {
  0x223b
};
static PRUnichar const NAME_1139[] = {
  'h', 'o', 'o', 'k', 'l', 'e', 'f', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_1139[] = {
  0x21a9
};
static PRUnichar const NAME_1140[] = {
  'h', 'o', 'o', 'k', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_1140[] = {
  0x21aa
};
static PRUnichar const NAME_1141[] = {
  'h', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_1141[] = {
  0xd835, 0xdd59
};
static PRUnichar const NAME_1142[] = {
  'h', 'o', 'r', 'b', 'a', 'r', ';'
};
static PRUnichar const VALUE_1142[] = {
  0x2015
};
static PRUnichar const NAME_1143[] = {
  'h', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_1143[] = {
  0xd835, 0xdcbd
};
static PRUnichar const NAME_1144[] = {
  'h', 's', 'l', 'a', 's', 'h', ';'
};
static PRUnichar const VALUE_1144[] = {
  0x210f
};
static PRUnichar const NAME_1145[] = {
  'h', 's', 't', 'r', 'o', 'k', ';'
};
static PRUnichar const VALUE_1145[] = {
  0x0127
};
static PRUnichar const NAME_1146[] = {
  'h', 'y', 'b', 'u', 'l', 'l', ';'
};
static PRUnichar const VALUE_1146[] = {
  0x2043
};
static PRUnichar const NAME_1147[] = {
  'h', 'y', 'p', 'h', 'e', 'n', ';'
};
static PRUnichar const VALUE_1147[] = {
  0x2010
};
static PRUnichar const NAME_1148[] = {
  'i', 'a', 'c', 'u', 't', 'e'
};
static PRUnichar const VALUE_1148[] = {
  0x00ed
};
static PRUnichar const NAME_1149[] = {
  'i', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_1149[] = {
  0x00ed
};
static PRUnichar const NAME_1150[] = {
  'i', 'c', ';'
};
static PRUnichar const VALUE_1150[] = {
  0x2063
};
static PRUnichar const NAME_1151[] = {
  'i', 'c', 'i', 'r', 'c'
};
static PRUnichar const VALUE_1151[] = {
  0x00ee
};
static PRUnichar const NAME_1152[] = {
  'i', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_1152[] = {
  0x00ee
};
static PRUnichar const NAME_1153[] = {
  'i', 'c', 'y', ';'
};
static PRUnichar const VALUE_1153[] = {
  0x0438
};
static PRUnichar const NAME_1154[] = {
  'i', 'e', 'c', 'y', ';'
};
static PRUnichar const VALUE_1154[] = {
  0x0435
};
static PRUnichar const NAME_1155[] = {
  'i', 'e', 'x', 'c', 'l'
};
static PRUnichar const VALUE_1155[] = {
  0x00a1
};
static PRUnichar const NAME_1156[] = {
  'i', 'e', 'x', 'c', 'l', ';'
};
static PRUnichar const VALUE_1156[] = {
  0x00a1
};
static PRUnichar const NAME_1157[] = {
  'i', 'f', 'f', ';'
};
static PRUnichar const VALUE_1157[] = {
  0x21d4
};
static PRUnichar const NAME_1158[] = {
  'i', 'f', 'r', ';'
};
static PRUnichar const VALUE_1158[] = {
  0xd835, 0xdd26
};
static PRUnichar const NAME_1159[] = {
  'i', 'g', 'r', 'a', 'v', 'e'
};
static PRUnichar const VALUE_1159[] = {
  0x00ec
};
static PRUnichar const NAME_1160[] = {
  'i', 'g', 'r', 'a', 'v', 'e', ';'
};
static PRUnichar const VALUE_1160[] = {
  0x00ec
};
static PRUnichar const NAME_1161[] = {
  'i', 'i', ';'
};
static PRUnichar const VALUE_1161[] = {
  0x2148
};
static PRUnichar const NAME_1162[] = {
  'i', 'i', 'i', 'i', 'n', 't', ';'
};
static PRUnichar const VALUE_1162[] = {
  0x2a0c
};
static PRUnichar const NAME_1163[] = {
  'i', 'i', 'i', 'n', 't', ';'
};
static PRUnichar const VALUE_1163[] = {
  0x222d
};
static PRUnichar const NAME_1164[] = {
  'i', 'i', 'n', 'f', 'i', 'n', ';'
};
static PRUnichar const VALUE_1164[] = {
  0x29dc
};
static PRUnichar const NAME_1165[] = {
  'i', 'i', 'o', 't', 'a', ';'
};
static PRUnichar const VALUE_1165[] = {
  0x2129
};
static PRUnichar const NAME_1166[] = {
  'i', 'j', 'l', 'i', 'g', ';'
};
static PRUnichar const VALUE_1166[] = {
  0x0133
};
static PRUnichar const NAME_1167[] = {
  'i', 'm', 'a', 'c', 'r', ';'
};
static PRUnichar const VALUE_1167[] = {
  0x012b
};
static PRUnichar const NAME_1168[] = {
  'i', 'm', 'a', 'g', 'e', ';'
};
static PRUnichar const VALUE_1168[] = {
  0x2111
};
static PRUnichar const NAME_1169[] = {
  'i', 'm', 'a', 'g', 'l', 'i', 'n', 'e', ';'
};
static PRUnichar const VALUE_1169[] = {
  0x2110
};
static PRUnichar const NAME_1170[] = {
  'i', 'm', 'a', 'g', 'p', 'a', 'r', 't', ';'
};
static PRUnichar const VALUE_1170[] = {
  0x2111
};
static PRUnichar const NAME_1171[] = {
  'i', 'm', 'a', 't', 'h', ';'
};
static PRUnichar const VALUE_1171[] = {
  0x0131
};
static PRUnichar const NAME_1172[] = {
  'i', 'm', 'o', 'f', ';'
};
static PRUnichar const VALUE_1172[] = {
  0x22b7
};
static PRUnichar const NAME_1173[] = {
  'i', 'm', 'p', 'e', 'd', ';'
};
static PRUnichar const VALUE_1173[] = {
  0x01b5
};
static PRUnichar const NAME_1174[] = {
  'i', 'n', ';'
};
static PRUnichar const VALUE_1174[] = {
  0x2208
};
static PRUnichar const NAME_1175[] = {
  'i', 'n', 'c', 'a', 'r', 'e', ';'
};
static PRUnichar const VALUE_1175[] = {
  0x2105
};
static PRUnichar const NAME_1176[] = {
  'i', 'n', 'f', 'i', 'n', ';'
};
static PRUnichar const VALUE_1176[] = {
  0x221e
};
static PRUnichar const NAME_1177[] = {
  'i', 'n', 'f', 'i', 'n', 't', 'i', 'e', ';'
};
static PRUnichar const VALUE_1177[] = {
  0x29dd
};
static PRUnichar const NAME_1178[] = {
  'i', 'n', 'o', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_1178[] = {
  0x0131
};
static PRUnichar const NAME_1179[] = {
  'i', 'n', 't', ';'
};
static PRUnichar const VALUE_1179[] = {
  0x222b
};
static PRUnichar const NAME_1180[] = {
  'i', 'n', 't', 'c', 'a', 'l', ';'
};
static PRUnichar const VALUE_1180[] = {
  0x22ba
};
static PRUnichar const NAME_1181[] = {
  'i', 'n', 't', 'e', 'g', 'e', 'r', 's', ';'
};
static PRUnichar const VALUE_1181[] = {
  0x2124
};
static PRUnichar const NAME_1182[] = {
  'i', 'n', 't', 'e', 'r', 'c', 'a', 'l', ';'
};
static PRUnichar const VALUE_1182[] = {
  0x22ba
};
static PRUnichar const NAME_1183[] = {
  'i', 'n', 't', 'l', 'a', 'r', 'h', 'k', ';'
};
static PRUnichar const VALUE_1183[] = {
  0x2a17
};
static PRUnichar const NAME_1184[] = {
  'i', 'n', 't', 'p', 'r', 'o', 'd', ';'
};
static PRUnichar const VALUE_1184[] = {
  0x2a3c
};
static PRUnichar const NAME_1185[] = {
  'i', 'o', 'c', 'y', ';'
};
static PRUnichar const VALUE_1185[] = {
  0x0451
};
static PRUnichar const NAME_1186[] = {
  'i', 'o', 'g', 'o', 'n', ';'
};
static PRUnichar const VALUE_1186[] = {
  0x012f
};
static PRUnichar const NAME_1187[] = {
  'i', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_1187[] = {
  0xd835, 0xdd5a
};
static PRUnichar const NAME_1188[] = {
  'i', 'o', 't', 'a', ';'
};
static PRUnichar const VALUE_1188[] = {
  0x03b9
};
static PRUnichar const NAME_1189[] = {
  'i', 'p', 'r', 'o', 'd', ';'
};
static PRUnichar const VALUE_1189[] = {
  0x2a3c
};
static PRUnichar const NAME_1190[] = {
  'i', 'q', 'u', 'e', 's', 't'
};
static PRUnichar const VALUE_1190[] = {
  0x00bf
};
static PRUnichar const NAME_1191[] = {
  'i', 'q', 'u', 'e', 's', 't', ';'
};
static PRUnichar const VALUE_1191[] = {
  0x00bf
};
static PRUnichar const NAME_1192[] = {
  'i', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_1192[] = {
  0xd835, 0xdcbe
};
static PRUnichar const NAME_1193[] = {
  'i', 's', 'i', 'n', ';'
};
static PRUnichar const VALUE_1193[] = {
  0x2208
};
static PRUnichar const NAME_1194[] = {
  'i', 's', 'i', 'n', 'E', ';'
};
static PRUnichar const VALUE_1194[] = {
  0x22f9
};
static PRUnichar const NAME_1195[] = {
  'i', 's', 'i', 'n', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_1195[] = {
  0x22f5
};
static PRUnichar const NAME_1196[] = {
  'i', 's', 'i', 'n', 's', ';'
};
static PRUnichar const VALUE_1196[] = {
  0x22f4
};
static PRUnichar const NAME_1197[] = {
  'i', 's', 'i', 'n', 's', 'v', ';'
};
static PRUnichar const VALUE_1197[] = {
  0x22f3
};
static PRUnichar const NAME_1198[] = {
  'i', 's', 'i', 'n', 'v', ';'
};
static PRUnichar const VALUE_1198[] = {
  0x2208
};
static PRUnichar const NAME_1199[] = {
  'i', 't', ';'
};
static PRUnichar const VALUE_1199[] = {
  0x2062
};
static PRUnichar const NAME_1200[] = {
  'i', 't', 'i', 'l', 'd', 'e', ';'
};
static PRUnichar const VALUE_1200[] = {
  0x0129
};
static PRUnichar const NAME_1201[] = {
  'i', 'u', 'k', 'c', 'y', ';'
};
static PRUnichar const VALUE_1201[] = {
  0x0456
};
static PRUnichar const NAME_1202[] = {
  'i', 'u', 'm', 'l'
};
static PRUnichar const VALUE_1202[] = {
  0x00ef
};
static PRUnichar const NAME_1203[] = {
  'i', 'u', 'm', 'l', ';'
};
static PRUnichar const VALUE_1203[] = {
  0x00ef
};
static PRUnichar const NAME_1204[] = {
  'j', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_1204[] = {
  0x0135
};
static PRUnichar const NAME_1205[] = {
  'j', 'c', 'y', ';'
};
static PRUnichar const VALUE_1205[] = {
  0x0439
};
static PRUnichar const NAME_1206[] = {
  'j', 'f', 'r', ';'
};
static PRUnichar const VALUE_1206[] = {
  0xd835, 0xdd27
};
static PRUnichar const NAME_1207[] = {
  'j', 'm', 'a', 't', 'h', ';'
};
static PRUnichar const VALUE_1207[] = {
  0x0237
};
static PRUnichar const NAME_1208[] = {
  'j', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_1208[] = {
  0xd835, 0xdd5b
};
static PRUnichar const NAME_1209[] = {
  'j', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_1209[] = {
  0xd835, 0xdcbf
};
static PRUnichar const NAME_1210[] = {
  'j', 's', 'e', 'r', 'c', 'y', ';'
};
static PRUnichar const VALUE_1210[] = {
  0x0458
};
static PRUnichar const NAME_1211[] = {
  'j', 'u', 'k', 'c', 'y', ';'
};
static PRUnichar const VALUE_1211[] = {
  0x0454
};
static PRUnichar const NAME_1212[] = {
  'k', 'a', 'p', 'p', 'a', ';'
};
static PRUnichar const VALUE_1212[] = {
  0x03ba
};
static PRUnichar const NAME_1213[] = {
  'k', 'a', 'p', 'p', 'a', 'v', ';'
};
static PRUnichar const VALUE_1213[] = {
  0x03f0
};
static PRUnichar const NAME_1214[] = {
  'k', 'c', 'e', 'd', 'i', 'l', ';'
};
static PRUnichar const VALUE_1214[] = {
  0x0137
};
static PRUnichar const NAME_1215[] = {
  'k', 'c', 'y', ';'
};
static PRUnichar const VALUE_1215[] = {
  0x043a
};
static PRUnichar const NAME_1216[] = {
  'k', 'f', 'r', ';'
};
static PRUnichar const VALUE_1216[] = {
  0xd835, 0xdd28
};
static PRUnichar const NAME_1217[] = {
  'k', 'g', 'r', 'e', 'e', 'n', ';'
};
static PRUnichar const VALUE_1217[] = {
  0x0138
};
static PRUnichar const NAME_1218[] = {
  'k', 'h', 'c', 'y', ';'
};
static PRUnichar const VALUE_1218[] = {
  0x0445
};
static PRUnichar const NAME_1219[] = {
  'k', 'j', 'c', 'y', ';'
};
static PRUnichar const VALUE_1219[] = {
  0x045c
};
static PRUnichar const NAME_1220[] = {
  'k', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_1220[] = {
  0xd835, 0xdd5c
};
static PRUnichar const NAME_1221[] = {
  'k', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_1221[] = {
  0xd835, 0xdcc0
};
static PRUnichar const NAME_1222[] = {
  'l', 'A', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1222[] = {
  0x21da
};
static PRUnichar const NAME_1223[] = {
  'l', 'A', 'r', 'r', ';'
};
static PRUnichar const VALUE_1223[] = {
  0x21d0
};
static PRUnichar const NAME_1224[] = {
  'l', 'A', 't', 'a', 'i', 'l', ';'
};
static PRUnichar const VALUE_1224[] = {
  0x291b
};
static PRUnichar const NAME_1225[] = {
  'l', 'B', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1225[] = {
  0x290e
};
static PRUnichar const NAME_1226[] = {
  'l', 'E', ';'
};
static PRUnichar const VALUE_1226[] = {
  0x2266
};
static PRUnichar const NAME_1227[] = {
  'l', 'E', 'g', ';'
};
static PRUnichar const VALUE_1227[] = {
  0x2a8b
};
static PRUnichar const NAME_1228[] = {
  'l', 'H', 'a', 'r', ';'
};
static PRUnichar const VALUE_1228[] = {
  0x2962
};
static PRUnichar const NAME_1229[] = {
  'l', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_1229[] = {
  0x013a
};
static PRUnichar const NAME_1230[] = {
  'l', 'a', 'e', 'm', 'p', 't', 'y', 'v', ';'
};
static PRUnichar const VALUE_1230[] = {
  0x29b4
};
static PRUnichar const NAME_1231[] = {
  'l', 'a', 'g', 'r', 'a', 'n', ';'
};
static PRUnichar const VALUE_1231[] = {
  0x2112
};
static PRUnichar const NAME_1232[] = {
  'l', 'a', 'm', 'b', 'd', 'a', ';'
};
static PRUnichar const VALUE_1232[] = {
  0x03bb
};
static PRUnichar const NAME_1233[] = {
  'l', 'a', 'n', 'g', ';'
};
static PRUnichar const VALUE_1233[] = {
  0x27e8
};
static PRUnichar const NAME_1234[] = {
  'l', 'a', 'n', 'g', 'd', ';'
};
static PRUnichar const VALUE_1234[] = {
  0x2991
};
static PRUnichar const NAME_1235[] = {
  'l', 'a', 'n', 'g', 'l', 'e', ';'
};
static PRUnichar const VALUE_1235[] = {
  0x27e8
};
static PRUnichar const NAME_1236[] = {
  'l', 'a', 'p', ';'
};
static PRUnichar const VALUE_1236[] = {
  0x2a85
};
static PRUnichar const NAME_1237[] = {
  'l', 'a', 'q', 'u', 'o'
};
static PRUnichar const VALUE_1237[] = {
  0x00ab
};
static PRUnichar const NAME_1238[] = {
  'l', 'a', 'q', 'u', 'o', ';'
};
static PRUnichar const VALUE_1238[] = {
  0x00ab
};
static PRUnichar const NAME_1239[] = {
  'l', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1239[] = {
  0x2190
};
static PRUnichar const NAME_1240[] = {
  'l', 'a', 'r', 'r', 'b', ';'
};
static PRUnichar const VALUE_1240[] = {
  0x21e4
};
static PRUnichar const NAME_1241[] = {
  'l', 'a', 'r', 'r', 'b', 'f', 's', ';'
};
static PRUnichar const VALUE_1241[] = {
  0x291f
};
static PRUnichar const NAME_1242[] = {
  'l', 'a', 'r', 'r', 'f', 's', ';'
};
static PRUnichar const VALUE_1242[] = {
  0x291d
};
static PRUnichar const NAME_1243[] = {
  'l', 'a', 'r', 'r', 'h', 'k', ';'
};
static PRUnichar const VALUE_1243[] = {
  0x21a9
};
static PRUnichar const NAME_1244[] = {
  'l', 'a', 'r', 'r', 'l', 'p', ';'
};
static PRUnichar const VALUE_1244[] = {
  0x21ab
};
static PRUnichar const NAME_1245[] = {
  'l', 'a', 'r', 'r', 'p', 'l', ';'
};
static PRUnichar const VALUE_1245[] = {
  0x2939
};
static PRUnichar const NAME_1246[] = {
  'l', 'a', 'r', 'r', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1246[] = {
  0x2973
};
static PRUnichar const NAME_1247[] = {
  'l', 'a', 'r', 'r', 't', 'l', ';'
};
static PRUnichar const VALUE_1247[] = {
  0x21a2
};
static PRUnichar const NAME_1248[] = {
  'l', 'a', 't', ';'
};
static PRUnichar const VALUE_1248[] = {
  0x2aab
};
static PRUnichar const NAME_1249[] = {
  'l', 'a', 't', 'a', 'i', 'l', ';'
};
static PRUnichar const VALUE_1249[] = {
  0x2919
};
static PRUnichar const NAME_1250[] = {
  'l', 'a', 't', 'e', ';'
};
static PRUnichar const VALUE_1250[] = {
  0x2aad
};
static PRUnichar const NAME_1251[] = {
  'l', 'b', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1251[] = {
  0x290c
};
static PRUnichar const NAME_1252[] = {
  'l', 'b', 'b', 'r', 'k', ';'
};
static PRUnichar const VALUE_1252[] = {
  0x2772
};
static PRUnichar const NAME_1253[] = {
  'l', 'b', 'r', 'a', 'c', 'e', ';'
};
static PRUnichar const VALUE_1253[] = {
  0x007b
};
static PRUnichar const NAME_1254[] = {
  'l', 'b', 'r', 'a', 'c', 'k', ';'
};
static PRUnichar const VALUE_1254[] = {
  0x005b
};
static PRUnichar const NAME_1255[] = {
  'l', 'b', 'r', 'k', 'e', ';'
};
static PRUnichar const VALUE_1255[] = {
  0x298b
};
static PRUnichar const NAME_1256[] = {
  'l', 'b', 'r', 'k', 's', 'l', 'd', ';'
};
static PRUnichar const VALUE_1256[] = {
  0x298f
};
static PRUnichar const NAME_1257[] = {
  'l', 'b', 'r', 'k', 's', 'l', 'u', ';'
};
static PRUnichar const VALUE_1257[] = {
  0x298d
};
static PRUnichar const NAME_1258[] = {
  'l', 'c', 'a', 'r', 'o', 'n', ';'
};
static PRUnichar const VALUE_1258[] = {
  0x013e
};
static PRUnichar const NAME_1259[] = {
  'l', 'c', 'e', 'd', 'i', 'l', ';'
};
static PRUnichar const VALUE_1259[] = {
  0x013c
};
static PRUnichar const NAME_1260[] = {
  'l', 'c', 'e', 'i', 'l', ';'
};
static PRUnichar const VALUE_1260[] = {
  0x2308
};
static PRUnichar const NAME_1261[] = {
  'l', 'c', 'u', 'b', ';'
};
static PRUnichar const VALUE_1261[] = {
  0x007b
};
static PRUnichar const NAME_1262[] = {
  'l', 'c', 'y', ';'
};
static PRUnichar const VALUE_1262[] = {
  0x043b
};
static PRUnichar const NAME_1263[] = {
  'l', 'd', 'c', 'a', ';'
};
static PRUnichar const VALUE_1263[] = {
  0x2936
};
static PRUnichar const NAME_1264[] = {
  'l', 'd', 'q', 'u', 'o', ';'
};
static PRUnichar const VALUE_1264[] = {
  0x201c
};
static PRUnichar const NAME_1265[] = {
  'l', 'd', 'q', 'u', 'o', 'r', ';'
};
static PRUnichar const VALUE_1265[] = {
  0x201e
};
static PRUnichar const NAME_1266[] = {
  'l', 'd', 'r', 'd', 'h', 'a', 'r', ';'
};
static PRUnichar const VALUE_1266[] = {
  0x2967
};
static PRUnichar const NAME_1267[] = {
  'l', 'd', 'r', 'u', 's', 'h', 'a', 'r', ';'
};
static PRUnichar const VALUE_1267[] = {
  0x294b
};
static PRUnichar const NAME_1268[] = {
  'l', 'd', 's', 'h', ';'
};
static PRUnichar const VALUE_1268[] = {
  0x21b2
};
static PRUnichar const NAME_1269[] = {
  'l', 'e', ';'
};
static PRUnichar const VALUE_1269[] = {
  0x2264
};
static PRUnichar const NAME_1270[] = {
  'l', 'e', 'f', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_1270[] = {
  0x2190
};
static PRUnichar const NAME_1271[] = {
  'l', 'e', 'f', 't', 'a', 'r', 'r', 'o', 'w', 't', 'a', 'i', 'l', ';'
};
static PRUnichar const VALUE_1271[] = {
  0x21a2
};
static PRUnichar const NAME_1272[] = {
  'l', 'e', 'f', 't', 'h', 'a', 'r', 'p', 'o', 'o', 'n', 'd', 'o', 'w', 'n', ';'
};
static PRUnichar const VALUE_1272[] = {
  0x21bd
};
static PRUnichar const NAME_1273[] = {
  'l', 'e', 'f', 't', 'h', 'a', 'r', 'p', 'o', 'o', 'n', 'u', 'p', ';'
};
static PRUnichar const VALUE_1273[] = {
  0x21bc
};
static PRUnichar const NAME_1274[] = {
  'l', 'e', 'f', 't', 'l', 'e', 'f', 't', 'a', 'r', 'r', 'o', 'w', 's', ';'
};
static PRUnichar const VALUE_1274[] = {
  0x21c7
};
static PRUnichar const NAME_1275[] = {
  'l', 'e', 'f', 't', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_1275[] = {
  0x2194
};
static PRUnichar const NAME_1276[] = {
  'l', 'e', 'f', 't', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', 's', ';'
};
static PRUnichar const VALUE_1276[] = {
  0x21c6
};
static PRUnichar const NAME_1277[] = {
  'l', 'e', 'f', 't', 'r', 'i', 'g', 'h', 't', 'h', 'a', 'r', 'p', 'o', 'o', 'n', 's', ';'
};
static PRUnichar const VALUE_1277[] = {
  0x21cb
};
static PRUnichar const NAME_1278[] = {
  'l', 'e', 'f', 't', 'r', 'i', 'g', 'h', 't', 's', 'q', 'u', 'i', 'g', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_1278[] = {
  0x21ad
};
static PRUnichar const NAME_1279[] = {
  'l', 'e', 'f', 't', 't', 'h', 'r', 'e', 'e', 't', 'i', 'm', 'e', 's', ';'
};
static PRUnichar const VALUE_1279[] = {
  0x22cb
};
static PRUnichar const NAME_1280[] = {
  'l', 'e', 'g', ';'
};
static PRUnichar const VALUE_1280[] = {
  0x22da
};
static PRUnichar const NAME_1281[] = {
  'l', 'e', 'q', ';'
};
static PRUnichar const VALUE_1281[] = {
  0x2264
};
static PRUnichar const NAME_1282[] = {
  'l', 'e', 'q', 'q', ';'
};
static PRUnichar const VALUE_1282[] = {
  0x2266
};
static PRUnichar const NAME_1283[] = {
  'l', 'e', 'q', 's', 'l', 'a', 'n', 't', ';'
};
static PRUnichar const VALUE_1283[] = {
  0x2a7d
};
static PRUnichar const NAME_1284[] = {
  'l', 'e', 's', ';'
};
static PRUnichar const VALUE_1284[] = {
  0x2a7d
};
static PRUnichar const NAME_1285[] = {
  'l', 'e', 's', 'c', 'c', ';'
};
static PRUnichar const VALUE_1285[] = {
  0x2aa8
};
static PRUnichar const NAME_1286[] = {
  'l', 'e', 's', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_1286[] = {
  0x2a7f
};
static PRUnichar const NAME_1287[] = {
  'l', 'e', 's', 'd', 'o', 't', 'o', ';'
};
static PRUnichar const VALUE_1287[] = {
  0x2a81
};
static PRUnichar const NAME_1288[] = {
  'l', 'e', 's', 'd', 'o', 't', 'o', 'r', ';'
};
static PRUnichar const VALUE_1288[] = {
  0x2a83
};
static PRUnichar const NAME_1289[] = {
  'l', 'e', 's', 'g', 'e', 's', ';'
};
static PRUnichar const VALUE_1289[] = {
  0x2a93
};
static PRUnichar const NAME_1290[] = {
  'l', 'e', 's', 's', 'a', 'p', 'p', 'r', 'o', 'x', ';'
};
static PRUnichar const VALUE_1290[] = {
  0x2a85
};
static PRUnichar const NAME_1291[] = {
  'l', 'e', 's', 's', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_1291[] = {
  0x22d6
};
static PRUnichar const NAME_1292[] = {
  'l', 'e', 's', 's', 'e', 'q', 'g', 't', 'r', ';'
};
static PRUnichar const VALUE_1292[] = {
  0x22da
};
static PRUnichar const NAME_1293[] = {
  'l', 'e', 's', 's', 'e', 'q', 'q', 'g', 't', 'r', ';'
};
static PRUnichar const VALUE_1293[] = {
  0x2a8b
};
static PRUnichar const NAME_1294[] = {
  'l', 'e', 's', 's', 'g', 't', 'r', ';'
};
static PRUnichar const VALUE_1294[] = {
  0x2276
};
static PRUnichar const NAME_1295[] = {
  'l', 'e', 's', 's', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1295[] = {
  0x2272
};
static PRUnichar const NAME_1296[] = {
  'l', 'f', 'i', 's', 'h', 't', ';'
};
static PRUnichar const VALUE_1296[] = {
  0x297c
};
static PRUnichar const NAME_1297[] = {
  'l', 'f', 'l', 'o', 'o', 'r', ';'
};
static PRUnichar const VALUE_1297[] = {
  0x230a
};
static PRUnichar const NAME_1298[] = {
  'l', 'f', 'r', ';'
};
static PRUnichar const VALUE_1298[] = {
  0xd835, 0xdd29
};
static PRUnichar const NAME_1299[] = {
  'l', 'g', ';'
};
static PRUnichar const VALUE_1299[] = {
  0x2276
};
static PRUnichar const NAME_1300[] = {
  'l', 'g', 'E', ';'
};
static PRUnichar const VALUE_1300[] = {
  0x2a91
};
static PRUnichar const NAME_1301[] = {
  'l', 'h', 'a', 'r', 'd', ';'
};
static PRUnichar const VALUE_1301[] = {
  0x21bd
};
static PRUnichar const NAME_1302[] = {
  'l', 'h', 'a', 'r', 'u', ';'
};
static PRUnichar const VALUE_1302[] = {
  0x21bc
};
static PRUnichar const NAME_1303[] = {
  'l', 'h', 'a', 'r', 'u', 'l', ';'
};
static PRUnichar const VALUE_1303[] = {
  0x296a
};
static PRUnichar const NAME_1304[] = {
  'l', 'h', 'b', 'l', 'k', ';'
};
static PRUnichar const VALUE_1304[] = {
  0x2584
};
static PRUnichar const NAME_1305[] = {
  'l', 'j', 'c', 'y', ';'
};
static PRUnichar const VALUE_1305[] = {
  0x0459
};
static PRUnichar const NAME_1306[] = {
  'l', 'l', ';'
};
static PRUnichar const VALUE_1306[] = {
  0x226a
};
static PRUnichar const NAME_1307[] = {
  'l', 'l', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1307[] = {
  0x21c7
};
static PRUnichar const NAME_1308[] = {
  'l', 'l', 'c', 'o', 'r', 'n', 'e', 'r', ';'
};
static PRUnichar const VALUE_1308[] = {
  0x231e
};
static PRUnichar const NAME_1309[] = {
  'l', 'l', 'h', 'a', 'r', 'd', ';'
};
static PRUnichar const VALUE_1309[] = {
  0x296b
};
static PRUnichar const NAME_1310[] = {
  'l', 'l', 't', 'r', 'i', ';'
};
static PRUnichar const VALUE_1310[] = {
  0x25fa
};
static PRUnichar const NAME_1311[] = {
  'l', 'm', 'i', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_1311[] = {
  0x0140
};
static PRUnichar const NAME_1312[] = {
  'l', 'm', 'o', 'u', 's', 't', ';'
};
static PRUnichar const VALUE_1312[] = {
  0x23b0
};
static PRUnichar const NAME_1313[] = {
  'l', 'm', 'o', 'u', 's', 't', 'a', 'c', 'h', 'e', ';'
};
static PRUnichar const VALUE_1313[] = {
  0x23b0
};
static PRUnichar const NAME_1314[] = {
  'l', 'n', 'E', ';'
};
static PRUnichar const VALUE_1314[] = {
  0x2268
};
static PRUnichar const NAME_1315[] = {
  'l', 'n', 'a', 'p', ';'
};
static PRUnichar const VALUE_1315[] = {
  0x2a89
};
static PRUnichar const NAME_1316[] = {
  'l', 'n', 'a', 'p', 'p', 'r', 'o', 'x', ';'
};
static PRUnichar const VALUE_1316[] = {
  0x2a89
};
static PRUnichar const NAME_1317[] = {
  'l', 'n', 'e', ';'
};
static PRUnichar const VALUE_1317[] = {
  0x2a87
};
static PRUnichar const NAME_1318[] = {
  'l', 'n', 'e', 'q', ';'
};
static PRUnichar const VALUE_1318[] = {
  0x2a87
};
static PRUnichar const NAME_1319[] = {
  'l', 'n', 'e', 'q', 'q', ';'
};
static PRUnichar const VALUE_1319[] = {
  0x2268
};
static PRUnichar const NAME_1320[] = {
  'l', 'n', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1320[] = {
  0x22e6
};
static PRUnichar const NAME_1321[] = {
  'l', 'o', 'a', 'n', 'g', ';'
};
static PRUnichar const VALUE_1321[] = {
  0x27ec
};
static PRUnichar const NAME_1322[] = {
  'l', 'o', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1322[] = {
  0x21fd
};
static PRUnichar const NAME_1323[] = {
  'l', 'o', 'b', 'r', 'k', ';'
};
static PRUnichar const VALUE_1323[] = {
  0x27e6
};
static PRUnichar const NAME_1324[] = {
  'l', 'o', 'n', 'g', 'l', 'e', 'f', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_1324[] = {
  0x27f5
};
static PRUnichar const NAME_1325[] = {
  'l', 'o', 'n', 'g', 'l', 'e', 'f', 't', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_1325[] = {
  0x27f7
};
static PRUnichar const NAME_1326[] = {
  'l', 'o', 'n', 'g', 'm', 'a', 'p', 's', 't', 'o', ';'
};
static PRUnichar const VALUE_1326[] = {
  0x27fc
};
static PRUnichar const NAME_1327[] = {
  'l', 'o', 'n', 'g', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_1327[] = {
  0x27f6
};
static PRUnichar const NAME_1328[] = {
  'l', 'o', 'o', 'p', 'a', 'r', 'r', 'o', 'w', 'l', 'e', 'f', 't', ';'
};
static PRUnichar const VALUE_1328[] = {
  0x21ab
};
static PRUnichar const NAME_1329[] = {
  'l', 'o', 'o', 'p', 'a', 'r', 'r', 'o', 'w', 'r', 'i', 'g', 'h', 't', ';'
};
static PRUnichar const VALUE_1329[] = {
  0x21ac
};
static PRUnichar const NAME_1330[] = {
  'l', 'o', 'p', 'a', 'r', ';'
};
static PRUnichar const VALUE_1330[] = {
  0x2985
};
static PRUnichar const NAME_1331[] = {
  'l', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_1331[] = {
  0xd835, 0xdd5d
};
static PRUnichar const NAME_1332[] = {
  'l', 'o', 'p', 'l', 'u', 's', ';'
};
static PRUnichar const VALUE_1332[] = {
  0x2a2d
};
static PRUnichar const NAME_1333[] = {
  'l', 'o', 't', 'i', 'm', 'e', 's', ';'
};
static PRUnichar const VALUE_1333[] = {
  0x2a34
};
static PRUnichar const NAME_1334[] = {
  'l', 'o', 'w', 'a', 's', 't', ';'
};
static PRUnichar const VALUE_1334[] = {
  0x2217
};
static PRUnichar const NAME_1335[] = {
  'l', 'o', 'w', 'b', 'a', 'r', ';'
};
static PRUnichar const VALUE_1335[] = {
  0x005f
};
static PRUnichar const NAME_1336[] = {
  'l', 'o', 'z', ';'
};
static PRUnichar const VALUE_1336[] = {
  0x25ca
};
static PRUnichar const NAME_1337[] = {
  'l', 'o', 'z', 'e', 'n', 'g', 'e', ';'
};
static PRUnichar const VALUE_1337[] = {
  0x25ca
};
static PRUnichar const NAME_1338[] = {
  'l', 'o', 'z', 'f', ';'
};
static PRUnichar const VALUE_1338[] = {
  0x29eb
};
static PRUnichar const NAME_1339[] = {
  'l', 'p', 'a', 'r', ';'
};
static PRUnichar const VALUE_1339[] = {
  0x0028
};
static PRUnichar const NAME_1340[] = {
  'l', 'p', 'a', 'r', 'l', 't', ';'
};
static PRUnichar const VALUE_1340[] = {
  0x2993
};
static PRUnichar const NAME_1341[] = {
  'l', 'r', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1341[] = {
  0x21c6
};
static PRUnichar const NAME_1342[] = {
  'l', 'r', 'c', 'o', 'r', 'n', 'e', 'r', ';'
};
static PRUnichar const VALUE_1342[] = {
  0x231f
};
static PRUnichar const NAME_1343[] = {
  'l', 'r', 'h', 'a', 'r', ';'
};
static PRUnichar const VALUE_1343[] = {
  0x21cb
};
static PRUnichar const NAME_1344[] = {
  'l', 'r', 'h', 'a', 'r', 'd', ';'
};
static PRUnichar const VALUE_1344[] = {
  0x296d
};
static PRUnichar const NAME_1345[] = {
  'l', 'r', 'm', ';'
};
static PRUnichar const VALUE_1345[] = {
  0x200e
};
static PRUnichar const NAME_1346[] = {
  'l', 'r', 't', 'r', 'i', ';'
};
static PRUnichar const VALUE_1346[] = {
  0x22bf
};
static PRUnichar const NAME_1347[] = {
  'l', 's', 'a', 'q', 'u', 'o', ';'
};
static PRUnichar const VALUE_1347[] = {
  0x2039
};
static PRUnichar const NAME_1348[] = {
  'l', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_1348[] = {
  0xd835, 0xdcc1
};
static PRUnichar const NAME_1349[] = {
  'l', 's', 'h', ';'
};
static PRUnichar const VALUE_1349[] = {
  0x21b0
};
static PRUnichar const NAME_1350[] = {
  'l', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1350[] = {
  0x2272
};
static PRUnichar const NAME_1351[] = {
  'l', 's', 'i', 'm', 'e', ';'
};
static PRUnichar const VALUE_1351[] = {
  0x2a8d
};
static PRUnichar const NAME_1352[] = {
  'l', 's', 'i', 'm', 'g', ';'
};
static PRUnichar const VALUE_1352[] = {
  0x2a8f
};
static PRUnichar const NAME_1353[] = {
  'l', 's', 'q', 'b', ';'
};
static PRUnichar const VALUE_1353[] = {
  0x005b
};
static PRUnichar const NAME_1354[] = {
  'l', 's', 'q', 'u', 'o', ';'
};
static PRUnichar const VALUE_1354[] = {
  0x2018
};
static PRUnichar const NAME_1355[] = {
  'l', 's', 'q', 'u', 'o', 'r', ';'
};
static PRUnichar const VALUE_1355[] = {
  0x201a
};
static PRUnichar const NAME_1356[] = {
  'l', 's', 't', 'r', 'o', 'k', ';'
};
static PRUnichar const VALUE_1356[] = {
  0x0142
};
static PRUnichar const NAME_1357[] = {
  'l', 't'
};
static PRUnichar const VALUE_1357[] = {
  0x003c
};
static PRUnichar const NAME_1358[] = {
  'l', 't', ';'
};
static PRUnichar const VALUE_1358[] = {
  0x003c
};
static PRUnichar const NAME_1359[] = {
  'l', 't', 'c', 'c', ';'
};
static PRUnichar const VALUE_1359[] = {
  0x2aa6
};
static PRUnichar const NAME_1360[] = {
  'l', 't', 'c', 'i', 'r', ';'
};
static PRUnichar const VALUE_1360[] = {
  0x2a79
};
static PRUnichar const NAME_1361[] = {
  'l', 't', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_1361[] = {
  0x22d6
};
static PRUnichar const NAME_1362[] = {
  'l', 't', 'h', 'r', 'e', 'e', ';'
};
static PRUnichar const VALUE_1362[] = {
  0x22cb
};
static PRUnichar const NAME_1363[] = {
  'l', 't', 'i', 'm', 'e', 's', ';'
};
static PRUnichar const VALUE_1363[] = {
  0x22c9
};
static PRUnichar const NAME_1364[] = {
  'l', 't', 'l', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1364[] = {
  0x2976
};
static PRUnichar const NAME_1365[] = {
  'l', 't', 'q', 'u', 'e', 's', 't', ';'
};
static PRUnichar const VALUE_1365[] = {
  0x2a7b
};
static PRUnichar const NAME_1366[] = {
  'l', 't', 'r', 'P', 'a', 'r', ';'
};
static PRUnichar const VALUE_1366[] = {
  0x2996
};
static PRUnichar const NAME_1367[] = {
  'l', 't', 'r', 'i', ';'
};
static PRUnichar const VALUE_1367[] = {
  0x25c3
};
static PRUnichar const NAME_1368[] = {
  'l', 't', 'r', 'i', 'e', ';'
};
static PRUnichar const VALUE_1368[] = {
  0x22b4
};
static PRUnichar const NAME_1369[] = {
  'l', 't', 'r', 'i', 'f', ';'
};
static PRUnichar const VALUE_1369[] = {
  0x25c2
};
static PRUnichar const NAME_1370[] = {
  'l', 'u', 'r', 'd', 's', 'h', 'a', 'r', ';'
};
static PRUnichar const VALUE_1370[] = {
  0x294a
};
static PRUnichar const NAME_1371[] = {
  'l', 'u', 'r', 'u', 'h', 'a', 'r', ';'
};
static PRUnichar const VALUE_1371[] = {
  0x2966
};
static PRUnichar const NAME_1372[] = {
  'm', 'D', 'D', 'o', 't', ';'
};
static PRUnichar const VALUE_1372[] = {
  0x223a
};
static PRUnichar const NAME_1373[] = {
  'm', 'a', 'c', 'r'
};
static PRUnichar const VALUE_1373[] = {
  0x00af
};
static PRUnichar const NAME_1374[] = {
  'm', 'a', 'c', 'r', ';'
};
static PRUnichar const VALUE_1374[] = {
  0x00af
};
static PRUnichar const NAME_1375[] = {
  'm', 'a', 'l', 'e', ';'
};
static PRUnichar const VALUE_1375[] = {
  0x2642
};
static PRUnichar const NAME_1376[] = {
  'm', 'a', 'l', 't', ';'
};
static PRUnichar const VALUE_1376[] = {
  0x2720
};
static PRUnichar const NAME_1377[] = {
  'm', 'a', 'l', 't', 'e', 's', 'e', ';'
};
static PRUnichar const VALUE_1377[] = {
  0x2720
};
static PRUnichar const NAME_1378[] = {
  'm', 'a', 'p', ';'
};
static PRUnichar const VALUE_1378[] = {
  0x21a6
};
static PRUnichar const NAME_1379[] = {
  'm', 'a', 'p', 's', 't', 'o', ';'
};
static PRUnichar const VALUE_1379[] = {
  0x21a6
};
static PRUnichar const NAME_1380[] = {
  'm', 'a', 'p', 's', 't', 'o', 'd', 'o', 'w', 'n', ';'
};
static PRUnichar const VALUE_1380[] = {
  0x21a7
};
static PRUnichar const NAME_1381[] = {
  'm', 'a', 'p', 's', 't', 'o', 'l', 'e', 'f', 't', ';'
};
static PRUnichar const VALUE_1381[] = {
  0x21a4
};
static PRUnichar const NAME_1382[] = {
  'm', 'a', 'p', 's', 't', 'o', 'u', 'p', ';'
};
static PRUnichar const VALUE_1382[] = {
  0x21a5
};
static PRUnichar const NAME_1383[] = {
  'm', 'a', 'r', 'k', 'e', 'r', ';'
};
static PRUnichar const VALUE_1383[] = {
  0x25ae
};
static PRUnichar const NAME_1384[] = {
  'm', 'c', 'o', 'm', 'm', 'a', ';'
};
static PRUnichar const VALUE_1384[] = {
  0x2a29
};
static PRUnichar const NAME_1385[] = {
  'm', 'c', 'y', ';'
};
static PRUnichar const VALUE_1385[] = {
  0x043c
};
static PRUnichar const NAME_1386[] = {
  'm', 'd', 'a', 's', 'h', ';'
};
static PRUnichar const VALUE_1386[] = {
  0x2014
};
static PRUnichar const NAME_1387[] = {
  'm', 'e', 'a', 's', 'u', 'r', 'e', 'd', 'a', 'n', 'g', 'l', 'e', ';'
};
static PRUnichar const VALUE_1387[] = {
  0x2221
};
static PRUnichar const NAME_1388[] = {
  'm', 'f', 'r', ';'
};
static PRUnichar const VALUE_1388[] = {
  0xd835, 0xdd2a
};
static PRUnichar const NAME_1389[] = {
  'm', 'h', 'o', ';'
};
static PRUnichar const VALUE_1389[] = {
  0x2127
};
static PRUnichar const NAME_1390[] = {
  'm', 'i', 'c', 'r', 'o'
};
static PRUnichar const VALUE_1390[] = {
  0x00b5
};
static PRUnichar const NAME_1391[] = {
  'm', 'i', 'c', 'r', 'o', ';'
};
static PRUnichar const VALUE_1391[] = {
  0x00b5
};
static PRUnichar const NAME_1392[] = {
  'm', 'i', 'd', ';'
};
static PRUnichar const VALUE_1392[] = {
  0x2223
};
static PRUnichar const NAME_1393[] = {
  'm', 'i', 'd', 'a', 's', 't', ';'
};
static PRUnichar const VALUE_1393[] = {
  0x002a
};
static PRUnichar const NAME_1394[] = {
  'm', 'i', 'd', 'c', 'i', 'r', ';'
};
static PRUnichar const VALUE_1394[] = {
  0x2af0
};
static PRUnichar const NAME_1395[] = {
  'm', 'i', 'd', 'd', 'o', 't'
};
static PRUnichar const VALUE_1395[] = {
  0x00b7
};
static PRUnichar const NAME_1396[] = {
  'm', 'i', 'd', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_1396[] = {
  0x00b7
};
static PRUnichar const NAME_1397[] = {
  'm', 'i', 'n', 'u', 's', ';'
};
static PRUnichar const VALUE_1397[] = {
  0x2212
};
static PRUnichar const NAME_1398[] = {
  'm', 'i', 'n', 'u', 's', 'b', ';'
};
static PRUnichar const VALUE_1398[] = {
  0x229f
};
static PRUnichar const NAME_1399[] = {
  'm', 'i', 'n', 'u', 's', 'd', ';'
};
static PRUnichar const VALUE_1399[] = {
  0x2238
};
static PRUnichar const NAME_1400[] = {
  'm', 'i', 'n', 'u', 's', 'd', 'u', ';'
};
static PRUnichar const VALUE_1400[] = {
  0x2a2a
};
static PRUnichar const NAME_1401[] = {
  'm', 'l', 'c', 'p', ';'
};
static PRUnichar const VALUE_1401[] = {
  0x2adb
};
static PRUnichar const NAME_1402[] = {
  'm', 'l', 'd', 'r', ';'
};
static PRUnichar const VALUE_1402[] = {
  0x2026
};
static PRUnichar const NAME_1403[] = {
  'm', 'n', 'p', 'l', 'u', 's', ';'
};
static PRUnichar const VALUE_1403[] = {
  0x2213
};
static PRUnichar const NAME_1404[] = {
  'm', 'o', 'd', 'e', 'l', 's', ';'
};
static PRUnichar const VALUE_1404[] = {
  0x22a7
};
static PRUnichar const NAME_1405[] = {
  'm', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_1405[] = {
  0xd835, 0xdd5e
};
static PRUnichar const NAME_1406[] = {
  'm', 'p', ';'
};
static PRUnichar const VALUE_1406[] = {
  0x2213
};
static PRUnichar const NAME_1407[] = {
  'm', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_1407[] = {
  0xd835, 0xdcc2
};
static PRUnichar const NAME_1408[] = {
  'm', 's', 't', 'p', 'o', 's', ';'
};
static PRUnichar const VALUE_1408[] = {
  0x223e
};
static PRUnichar const NAME_1409[] = {
  'm', 'u', ';'
};
static PRUnichar const VALUE_1409[] = {
  0x03bc
};
static PRUnichar const NAME_1410[] = {
  'm', 'u', 'l', 't', 'i', 'm', 'a', 'p', ';'
};
static PRUnichar const VALUE_1410[] = {
  0x22b8
};
static PRUnichar const NAME_1411[] = {
  'm', 'u', 'm', 'a', 'p', ';'
};
static PRUnichar const VALUE_1411[] = {
  0x22b8
};
static PRUnichar const NAME_1412[] = {
  'n', 'L', 'e', 'f', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_1412[] = {
  0x21cd
};
static PRUnichar const NAME_1413[] = {
  'n', 'L', 'e', 'f', 't', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_1413[] = {
  0x21ce
};
static PRUnichar const NAME_1414[] = {
  'n', 'R', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_1414[] = {
  0x21cf
};
static PRUnichar const NAME_1415[] = {
  'n', 'V', 'D', 'a', 's', 'h', ';'
};
static PRUnichar const VALUE_1415[] = {
  0x22af
};
static PRUnichar const NAME_1416[] = {
  'n', 'V', 'd', 'a', 's', 'h', ';'
};
static PRUnichar const VALUE_1416[] = {
  0x22ae
};
static PRUnichar const NAME_1417[] = {
  'n', 'a', 'b', 'l', 'a', ';'
};
static PRUnichar const VALUE_1417[] = {
  0x2207
};
static PRUnichar const NAME_1418[] = {
  'n', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_1418[] = {
  0x0144
};
static PRUnichar const NAME_1419[] = {
  'n', 'a', 'p', ';'
};
static PRUnichar const VALUE_1419[] = {
  0x2249
};
static PRUnichar const NAME_1420[] = {
  'n', 'a', 'p', 'o', 's', ';'
};
static PRUnichar const VALUE_1420[] = {
  0x0149
};
static PRUnichar const NAME_1421[] = {
  'n', 'a', 'p', 'p', 'r', 'o', 'x', ';'
};
static PRUnichar const VALUE_1421[] = {
  0x2249
};
static PRUnichar const NAME_1422[] = {
  'n', 'a', 't', 'u', 'r', ';'
};
static PRUnichar const VALUE_1422[] = {
  0x266e
};
static PRUnichar const NAME_1423[] = {
  'n', 'a', 't', 'u', 'r', 'a', 'l', ';'
};
static PRUnichar const VALUE_1423[] = {
  0x266e
};
static PRUnichar const NAME_1424[] = {
  'n', 'a', 't', 'u', 'r', 'a', 'l', 's', ';'
};
static PRUnichar const VALUE_1424[] = {
  0x2115
};
static PRUnichar const NAME_1425[] = {
  'n', 'b', 's', 'p'
};
static PRUnichar const VALUE_1425[] = {
  0x00a0
};
static PRUnichar const NAME_1426[] = {
  'n', 'b', 's', 'p', ';'
};
static PRUnichar const VALUE_1426[] = {
  0x00a0
};
static PRUnichar const NAME_1427[] = {
  'n', 'c', 'a', 'p', ';'
};
static PRUnichar const VALUE_1427[] = {
  0x2a43
};
static PRUnichar const NAME_1428[] = {
  'n', 'c', 'a', 'r', 'o', 'n', ';'
};
static PRUnichar const VALUE_1428[] = {
  0x0148
};
static PRUnichar const NAME_1429[] = {
  'n', 'c', 'e', 'd', 'i', 'l', ';'
};
static PRUnichar const VALUE_1429[] = {
  0x0146
};
static PRUnichar const NAME_1430[] = {
  'n', 'c', 'o', 'n', 'g', ';'
};
static PRUnichar const VALUE_1430[] = {
  0x2247
};
static PRUnichar const NAME_1431[] = {
  'n', 'c', 'u', 'p', ';'
};
static PRUnichar const VALUE_1431[] = {
  0x2a42
};
static PRUnichar const NAME_1432[] = {
  'n', 'c', 'y', ';'
};
static PRUnichar const VALUE_1432[] = {
  0x043d
};
static PRUnichar const NAME_1433[] = {
  'n', 'd', 'a', 's', 'h', ';'
};
static PRUnichar const VALUE_1433[] = {
  0x2013
};
static PRUnichar const NAME_1434[] = {
  'n', 'e', ';'
};
static PRUnichar const VALUE_1434[] = {
  0x2260
};
static PRUnichar const NAME_1435[] = {
  'n', 'e', 'A', 'r', 'r', ';'
};
static PRUnichar const VALUE_1435[] = {
  0x21d7
};
static PRUnichar const NAME_1436[] = {
  'n', 'e', 'a', 'r', 'h', 'k', ';'
};
static PRUnichar const VALUE_1436[] = {
  0x2924
};
static PRUnichar const NAME_1437[] = {
  'n', 'e', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1437[] = {
  0x2197
};
static PRUnichar const NAME_1438[] = {
  'n', 'e', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_1438[] = {
  0x2197
};
static PRUnichar const NAME_1439[] = {
  'n', 'e', 'q', 'u', 'i', 'v', ';'
};
static PRUnichar const VALUE_1439[] = {
  0x2262
};
static PRUnichar const NAME_1440[] = {
  'n', 'e', 's', 'e', 'a', 'r', ';'
};
static PRUnichar const VALUE_1440[] = {
  0x2928
};
static PRUnichar const NAME_1441[] = {
  'n', 'e', 'x', 'i', 's', 't', ';'
};
static PRUnichar const VALUE_1441[] = {
  0x2204
};
static PRUnichar const NAME_1442[] = {
  'n', 'e', 'x', 'i', 's', 't', 's', ';'
};
static PRUnichar const VALUE_1442[] = {
  0x2204
};
static PRUnichar const NAME_1443[] = {
  'n', 'f', 'r', ';'
};
static PRUnichar const VALUE_1443[] = {
  0xd835, 0xdd2b
};
static PRUnichar const NAME_1444[] = {
  'n', 'g', 'e', ';'
};
static PRUnichar const VALUE_1444[] = {
  0x2271
};
static PRUnichar const NAME_1445[] = {
  'n', 'g', 'e', 'q', ';'
};
static PRUnichar const VALUE_1445[] = {
  0x2271
};
static PRUnichar const NAME_1446[] = {
  'n', 'g', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1446[] = {
  0x2275
};
static PRUnichar const NAME_1447[] = {
  'n', 'g', 't', ';'
};
static PRUnichar const VALUE_1447[] = {
  0x226f
};
static PRUnichar const NAME_1448[] = {
  'n', 'g', 't', 'r', ';'
};
static PRUnichar const VALUE_1448[] = {
  0x226f
};
static PRUnichar const NAME_1449[] = {
  'n', 'h', 'A', 'r', 'r', ';'
};
static PRUnichar const VALUE_1449[] = {
  0x21ce
};
static PRUnichar const NAME_1450[] = {
  'n', 'h', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1450[] = {
  0x21ae
};
static PRUnichar const NAME_1451[] = {
  'n', 'h', 'p', 'a', 'r', ';'
};
static PRUnichar const VALUE_1451[] = {
  0x2af2
};
static PRUnichar const NAME_1452[] = {
  'n', 'i', ';'
};
static PRUnichar const VALUE_1452[] = {
  0x220b
};
static PRUnichar const NAME_1453[] = {
  'n', 'i', 's', ';'
};
static PRUnichar const VALUE_1453[] = {
  0x22fc
};
static PRUnichar const NAME_1454[] = {
  'n', 'i', 's', 'd', ';'
};
static PRUnichar const VALUE_1454[] = {
  0x22fa
};
static PRUnichar const NAME_1455[] = {
  'n', 'i', 'v', ';'
};
static PRUnichar const VALUE_1455[] = {
  0x220b
};
static PRUnichar const NAME_1456[] = {
  'n', 'j', 'c', 'y', ';'
};
static PRUnichar const VALUE_1456[] = {
  0x045a
};
static PRUnichar const NAME_1457[] = {
  'n', 'l', 'A', 'r', 'r', ';'
};
static PRUnichar const VALUE_1457[] = {
  0x21cd
};
static PRUnichar const NAME_1458[] = {
  'n', 'l', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1458[] = {
  0x219a
};
static PRUnichar const NAME_1459[] = {
  'n', 'l', 'd', 'r', ';'
};
static PRUnichar const VALUE_1459[] = {
  0x2025
};
static PRUnichar const NAME_1460[] = {
  'n', 'l', 'e', ';'
};
static PRUnichar const VALUE_1460[] = {
  0x2270
};
static PRUnichar const NAME_1461[] = {
  'n', 'l', 'e', 'f', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_1461[] = {
  0x219a
};
static PRUnichar const NAME_1462[] = {
  'n', 'l', 'e', 'f', 't', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_1462[] = {
  0x21ae
};
static PRUnichar const NAME_1463[] = {
  'n', 'l', 'e', 'q', ';'
};
static PRUnichar const VALUE_1463[] = {
  0x2270
};
static PRUnichar const NAME_1464[] = {
  'n', 'l', 'e', 's', 's', ';'
};
static PRUnichar const VALUE_1464[] = {
  0x226e
};
static PRUnichar const NAME_1465[] = {
  'n', 'l', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1465[] = {
  0x2274
};
static PRUnichar const NAME_1466[] = {
  'n', 'l', 't', ';'
};
static PRUnichar const VALUE_1466[] = {
  0x226e
};
static PRUnichar const NAME_1467[] = {
  'n', 'l', 't', 'r', 'i', ';'
};
static PRUnichar const VALUE_1467[] = {
  0x22ea
};
static PRUnichar const NAME_1468[] = {
  'n', 'l', 't', 'r', 'i', 'e', ';'
};
static PRUnichar const VALUE_1468[] = {
  0x22ec
};
static PRUnichar const NAME_1469[] = {
  'n', 'm', 'i', 'd', ';'
};
static PRUnichar const VALUE_1469[] = {
  0x2224
};
static PRUnichar const NAME_1470[] = {
  'n', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_1470[] = {
  0xd835, 0xdd5f
};
static PRUnichar const NAME_1471[] = {
  'n', 'o', 't'
};
static PRUnichar const VALUE_1471[] = {
  0x00ac
};
static PRUnichar const NAME_1472[] = {
  'n', 'o', 't', ';'
};
static PRUnichar const VALUE_1472[] = {
  0x00ac
};
static PRUnichar const NAME_1473[] = {
  'n', 'o', 't', 'i', 'n', ';'
};
static PRUnichar const VALUE_1473[] = {
  0x2209
};
static PRUnichar const NAME_1474[] = {
  'n', 'o', 't', 'i', 'n', 'v', 'a', ';'
};
static PRUnichar const VALUE_1474[] = {
  0x2209
};
static PRUnichar const NAME_1475[] = {
  'n', 'o', 't', 'i', 'n', 'v', 'b', ';'
};
static PRUnichar const VALUE_1475[] = {
  0x22f7
};
static PRUnichar const NAME_1476[] = {
  'n', 'o', 't', 'i', 'n', 'v', 'c', ';'
};
static PRUnichar const VALUE_1476[] = {
  0x22f6
};
static PRUnichar const NAME_1477[] = {
  'n', 'o', 't', 'n', 'i', ';'
};
static PRUnichar const VALUE_1477[] = {
  0x220c
};
static PRUnichar const NAME_1478[] = {
  'n', 'o', 't', 'n', 'i', 'v', 'a', ';'
};
static PRUnichar const VALUE_1478[] = {
  0x220c
};
static PRUnichar const NAME_1479[] = {
  'n', 'o', 't', 'n', 'i', 'v', 'b', ';'
};
static PRUnichar const VALUE_1479[] = {
  0x22fe
};
static PRUnichar const NAME_1480[] = {
  'n', 'o', 't', 'n', 'i', 'v', 'c', ';'
};
static PRUnichar const VALUE_1480[] = {
  0x22fd
};
static PRUnichar const NAME_1481[] = {
  'n', 'p', 'a', 'r', ';'
};
static PRUnichar const VALUE_1481[] = {
  0x2226
};
static PRUnichar const NAME_1482[] = {
  'n', 'p', 'a', 'r', 'a', 'l', 'l', 'e', 'l', ';'
};
static PRUnichar const VALUE_1482[] = {
  0x2226
};
static PRUnichar const NAME_1483[] = {
  'n', 'p', 'o', 'l', 'i', 'n', 't', ';'
};
static PRUnichar const VALUE_1483[] = {
  0x2a14
};
static PRUnichar const NAME_1484[] = {
  'n', 'p', 'r', ';'
};
static PRUnichar const VALUE_1484[] = {
  0x2280
};
static PRUnichar const NAME_1485[] = {
  'n', 'p', 'r', 'c', 'u', 'e', ';'
};
static PRUnichar const VALUE_1485[] = {
  0x22e0
};
static PRUnichar const NAME_1486[] = {
  'n', 'p', 'r', 'e', 'c', ';'
};
static PRUnichar const VALUE_1486[] = {
  0x2280
};
static PRUnichar const NAME_1487[] = {
  'n', 'r', 'A', 'r', 'r', ';'
};
static PRUnichar const VALUE_1487[] = {
  0x21cf
};
static PRUnichar const NAME_1488[] = {
  'n', 'r', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1488[] = {
  0x219b
};
static PRUnichar const NAME_1489[] = {
  'n', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_1489[] = {
  0x219b
};
static PRUnichar const NAME_1490[] = {
  'n', 'r', 't', 'r', 'i', ';'
};
static PRUnichar const VALUE_1490[] = {
  0x22eb
};
static PRUnichar const NAME_1491[] = {
  'n', 'r', 't', 'r', 'i', 'e', ';'
};
static PRUnichar const VALUE_1491[] = {
  0x22ed
};
static PRUnichar const NAME_1492[] = {
  'n', 's', 'c', ';'
};
static PRUnichar const VALUE_1492[] = {
  0x2281
};
static PRUnichar const NAME_1493[] = {
  'n', 's', 'c', 'c', 'u', 'e', ';'
};
static PRUnichar const VALUE_1493[] = {
  0x22e1
};
static PRUnichar const NAME_1494[] = {
  'n', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_1494[] = {
  0xd835, 0xdcc3
};
static PRUnichar const NAME_1495[] = {
  'n', 's', 'h', 'o', 'r', 't', 'm', 'i', 'd', ';'
};
static PRUnichar const VALUE_1495[] = {
  0x2224
};
static PRUnichar const NAME_1496[] = {
  'n', 's', 'h', 'o', 'r', 't', 'p', 'a', 'r', 'a', 'l', 'l', 'e', 'l', ';'
};
static PRUnichar const VALUE_1496[] = {
  0x2226
};
static PRUnichar const NAME_1497[] = {
  'n', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1497[] = {
  0x2241
};
static PRUnichar const NAME_1498[] = {
  'n', 's', 'i', 'm', 'e', ';'
};
static PRUnichar const VALUE_1498[] = {
  0x2244
};
static PRUnichar const NAME_1499[] = {
  'n', 's', 'i', 'm', 'e', 'q', ';'
};
static PRUnichar const VALUE_1499[] = {
  0x2244
};
static PRUnichar const NAME_1500[] = {
  'n', 's', 'm', 'i', 'd', ';'
};
static PRUnichar const VALUE_1500[] = {
  0x2224
};
static PRUnichar const NAME_1501[] = {
  'n', 's', 'p', 'a', 'r', ';'
};
static PRUnichar const VALUE_1501[] = {
  0x2226
};
static PRUnichar const NAME_1502[] = {
  'n', 's', 'q', 's', 'u', 'b', 'e', ';'
};
static PRUnichar const VALUE_1502[] = {
  0x22e2
};
static PRUnichar const NAME_1503[] = {
  'n', 's', 'q', 's', 'u', 'p', 'e', ';'
};
static PRUnichar const VALUE_1503[] = {
  0x22e3
};
static PRUnichar const NAME_1504[] = {
  'n', 's', 'u', 'b', ';'
};
static PRUnichar const VALUE_1504[] = {
  0x2284
};
static PRUnichar const NAME_1505[] = {
  'n', 's', 'u', 'b', 'e', ';'
};
static PRUnichar const VALUE_1505[] = {
  0x2288
};
static PRUnichar const NAME_1506[] = {
  'n', 's', 'u', 'b', 's', 'e', 't', 'e', 'q', ';'
};
static PRUnichar const VALUE_1506[] = {
  0x2288
};
static PRUnichar const NAME_1507[] = {
  'n', 's', 'u', 'c', 'c', ';'
};
static PRUnichar const VALUE_1507[] = {
  0x2281
};
static PRUnichar const NAME_1508[] = {
  'n', 's', 'u', 'p', ';'
};
static PRUnichar const VALUE_1508[] = {
  0x2285
};
static PRUnichar const NAME_1509[] = {
  'n', 's', 'u', 'p', 'e', ';'
};
static PRUnichar const VALUE_1509[] = {
  0x2289
};
static PRUnichar const NAME_1510[] = {
  'n', 's', 'u', 'p', 's', 'e', 't', 'e', 'q', ';'
};
static PRUnichar const VALUE_1510[] = {
  0x2289
};
static PRUnichar const NAME_1511[] = {
  'n', 't', 'g', 'l', ';'
};
static PRUnichar const VALUE_1511[] = {
  0x2279
};
static PRUnichar const NAME_1512[] = {
  'n', 't', 'i', 'l', 'd', 'e'
};
static PRUnichar const VALUE_1512[] = {
  0x00f1
};
static PRUnichar const NAME_1513[] = {
  'n', 't', 'i', 'l', 'd', 'e', ';'
};
static PRUnichar const VALUE_1513[] = {
  0x00f1
};
static PRUnichar const NAME_1514[] = {
  'n', 't', 'l', 'g', ';'
};
static PRUnichar const VALUE_1514[] = {
  0x2278
};
static PRUnichar const NAME_1515[] = {
  'n', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'l', 'e', 'f', 't', ';'
};
static PRUnichar const VALUE_1515[] = {
  0x22ea
};
static PRUnichar const NAME_1516[] = {
  'n', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'l', 'e', 'f', 't', 'e', 'q', ';'
};
static PRUnichar const VALUE_1516[] = {
  0x22ec
};
static PRUnichar const NAME_1517[] = {
  'n', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'r', 'i', 'g', 'h', 't', ';'
};
static PRUnichar const VALUE_1517[] = {
  0x22eb
};
static PRUnichar const NAME_1518[] = {
  'n', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'r', 'i', 'g', 'h', 't', 'e', 'q', ';'
};
static PRUnichar const VALUE_1518[] = {
  0x22ed
};
static PRUnichar const NAME_1519[] = {
  'n', 'u', ';'
};
static PRUnichar const VALUE_1519[] = {
  0x03bd
};
static PRUnichar const NAME_1520[] = {
  'n', 'u', 'm', ';'
};
static PRUnichar const VALUE_1520[] = {
  0x0023
};
static PRUnichar const NAME_1521[] = {
  'n', 'u', 'm', 'e', 'r', 'o', ';'
};
static PRUnichar const VALUE_1521[] = {
  0x2116
};
static PRUnichar const NAME_1522[] = {
  'n', 'u', 'm', 's', 'p', ';'
};
static PRUnichar const VALUE_1522[] = {
  0x2007
};
static PRUnichar const NAME_1523[] = {
  'n', 'v', 'D', 'a', 's', 'h', ';'
};
static PRUnichar const VALUE_1523[] = {
  0x22ad
};
static PRUnichar const NAME_1524[] = {
  'n', 'v', 'H', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1524[] = {
  0x2904
};
static PRUnichar const NAME_1525[] = {
  'n', 'v', 'd', 'a', 's', 'h', ';'
};
static PRUnichar const VALUE_1525[] = {
  0x22ac
};
static PRUnichar const NAME_1526[] = {
  'n', 'v', 'i', 'n', 'f', 'i', 'n', ';'
};
static PRUnichar const VALUE_1526[] = {
  0x29de
};
static PRUnichar const NAME_1527[] = {
  'n', 'v', 'l', 'A', 'r', 'r', ';'
};
static PRUnichar const VALUE_1527[] = {
  0x2902
};
static PRUnichar const NAME_1528[] = {
  'n', 'v', 'r', 'A', 'r', 'r', ';'
};
static PRUnichar const VALUE_1528[] = {
  0x2903
};
static PRUnichar const NAME_1529[] = {
  'n', 'w', 'A', 'r', 'r', ';'
};
static PRUnichar const VALUE_1529[] = {
  0x21d6
};
static PRUnichar const NAME_1530[] = {
  'n', 'w', 'a', 'r', 'h', 'k', ';'
};
static PRUnichar const VALUE_1530[] = {
  0x2923
};
static PRUnichar const NAME_1531[] = {
  'n', 'w', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1531[] = {
  0x2196
};
static PRUnichar const NAME_1532[] = {
  'n', 'w', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_1532[] = {
  0x2196
};
static PRUnichar const NAME_1533[] = {
  'n', 'w', 'n', 'e', 'a', 'r', ';'
};
static PRUnichar const VALUE_1533[] = {
  0x2927
};
static PRUnichar const NAME_1534[] = {
  'o', 'S', ';'
};
static PRUnichar const VALUE_1534[] = {
  0x24c8
};
static PRUnichar const NAME_1535[] = {
  'o', 'a', 'c', 'u', 't', 'e'
};
static PRUnichar const VALUE_1535[] = {
  0x00f3
};
static PRUnichar const NAME_1536[] = {
  'o', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_1536[] = {
  0x00f3
};
static PRUnichar const NAME_1537[] = {
  'o', 'a', 's', 't', ';'
};
static PRUnichar const VALUE_1537[] = {
  0x229b
};
static PRUnichar const NAME_1538[] = {
  'o', 'c', 'i', 'r', ';'
};
static PRUnichar const VALUE_1538[] = {
  0x229a
};
static PRUnichar const NAME_1539[] = {
  'o', 'c', 'i', 'r', 'c'
};
static PRUnichar const VALUE_1539[] = {
  0x00f4
};
static PRUnichar const NAME_1540[] = {
  'o', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_1540[] = {
  0x00f4
};
static PRUnichar const NAME_1541[] = {
  'o', 'c', 'y', ';'
};
static PRUnichar const VALUE_1541[] = {
  0x043e
};
static PRUnichar const NAME_1542[] = {
  'o', 'd', 'a', 's', 'h', ';'
};
static PRUnichar const VALUE_1542[] = {
  0x229d
};
static PRUnichar const NAME_1543[] = {
  'o', 'd', 'b', 'l', 'a', 'c', ';'
};
static PRUnichar const VALUE_1543[] = {
  0x0151
};
static PRUnichar const NAME_1544[] = {
  'o', 'd', 'i', 'v', ';'
};
static PRUnichar const VALUE_1544[] = {
  0x2a38
};
static PRUnichar const NAME_1545[] = {
  'o', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_1545[] = {
  0x2299
};
static PRUnichar const NAME_1546[] = {
  'o', 'd', 's', 'o', 'l', 'd', ';'
};
static PRUnichar const VALUE_1546[] = {
  0x29bc
};
static PRUnichar const NAME_1547[] = {
  'o', 'e', 'l', 'i', 'g', ';'
};
static PRUnichar const VALUE_1547[] = {
  0x0153
};
static PRUnichar const NAME_1548[] = {
  'o', 'f', 'c', 'i', 'r', ';'
};
static PRUnichar const VALUE_1548[] = {
  0x29bf
};
static PRUnichar const NAME_1549[] = {
  'o', 'f', 'r', ';'
};
static PRUnichar const VALUE_1549[] = {
  0xd835, 0xdd2c
};
static PRUnichar const NAME_1550[] = {
  'o', 'g', 'o', 'n', ';'
};
static PRUnichar const VALUE_1550[] = {
  0x02db
};
static PRUnichar const NAME_1551[] = {
  'o', 'g', 'r', 'a', 'v', 'e'
};
static PRUnichar const VALUE_1551[] = {
  0x00f2
};
static PRUnichar const NAME_1552[] = {
  'o', 'g', 'r', 'a', 'v', 'e', ';'
};
static PRUnichar const VALUE_1552[] = {
  0x00f2
};
static PRUnichar const NAME_1553[] = {
  'o', 'g', 't', ';'
};
static PRUnichar const VALUE_1553[] = {
  0x29c1
};
static PRUnichar const NAME_1554[] = {
  'o', 'h', 'b', 'a', 'r', ';'
};
static PRUnichar const VALUE_1554[] = {
  0x29b5
};
static PRUnichar const NAME_1555[] = {
  'o', 'h', 'm', ';'
};
static PRUnichar const VALUE_1555[] = {
  0x2126
};
static PRUnichar const NAME_1556[] = {
  'o', 'i', 'n', 't', ';'
};
static PRUnichar const VALUE_1556[] = {
  0x222e
};
static PRUnichar const NAME_1557[] = {
  'o', 'l', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1557[] = {
  0x21ba
};
static PRUnichar const NAME_1558[] = {
  'o', 'l', 'c', 'i', 'r', ';'
};
static PRUnichar const VALUE_1558[] = {
  0x29be
};
static PRUnichar const NAME_1559[] = {
  'o', 'l', 'c', 'r', 'o', 's', 's', ';'
};
static PRUnichar const VALUE_1559[] = {
  0x29bb
};
static PRUnichar const NAME_1560[] = {
  'o', 'l', 'i', 'n', 'e', ';'
};
static PRUnichar const VALUE_1560[] = {
  0x203e
};
static PRUnichar const NAME_1561[] = {
  'o', 'l', 't', ';'
};
static PRUnichar const VALUE_1561[] = {
  0x29c0
};
static PRUnichar const NAME_1562[] = {
  'o', 'm', 'a', 'c', 'r', ';'
};
static PRUnichar const VALUE_1562[] = {
  0x014d
};
static PRUnichar const NAME_1563[] = {
  'o', 'm', 'e', 'g', 'a', ';'
};
static PRUnichar const VALUE_1563[] = {
  0x03c9
};
static PRUnichar const NAME_1564[] = {
  'o', 'm', 'i', 'c', 'r', 'o', 'n', ';'
};
static PRUnichar const VALUE_1564[] = {
  0x03bf
};
static PRUnichar const NAME_1565[] = {
  'o', 'm', 'i', 'd', ';'
};
static PRUnichar const VALUE_1565[] = {
  0x29b6
};
static PRUnichar const NAME_1566[] = {
  'o', 'm', 'i', 'n', 'u', 's', ';'
};
static PRUnichar const VALUE_1566[] = {
  0x2296
};
static PRUnichar const NAME_1567[] = {
  'o', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_1567[] = {
  0xd835, 0xdd60
};
static PRUnichar const NAME_1568[] = {
  'o', 'p', 'a', 'r', ';'
};
static PRUnichar const VALUE_1568[] = {
  0x29b7
};
static PRUnichar const NAME_1569[] = {
  'o', 'p', 'e', 'r', 'p', ';'
};
static PRUnichar const VALUE_1569[] = {
  0x29b9
};
static PRUnichar const NAME_1570[] = {
  'o', 'p', 'l', 'u', 's', ';'
};
static PRUnichar const VALUE_1570[] = {
  0x2295
};
static PRUnichar const NAME_1571[] = {
  'o', 'r', ';'
};
static PRUnichar const VALUE_1571[] = {
  0x2228
};
static PRUnichar const NAME_1572[] = {
  'o', 'r', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1572[] = {
  0x21bb
};
static PRUnichar const NAME_1573[] = {
  'o', 'r', 'd', ';'
};
static PRUnichar const VALUE_1573[] = {
  0x2a5d
};
static PRUnichar const NAME_1574[] = {
  'o', 'r', 'd', 'e', 'r', ';'
};
static PRUnichar const VALUE_1574[] = {
  0x2134
};
static PRUnichar const NAME_1575[] = {
  'o', 'r', 'd', 'e', 'r', 'o', 'f', ';'
};
static PRUnichar const VALUE_1575[] = {
  0x2134
};
static PRUnichar const NAME_1576[] = {
  'o', 'r', 'd', 'f'
};
static PRUnichar const VALUE_1576[] = {
  0x00aa
};
static PRUnichar const NAME_1577[] = {
  'o', 'r', 'd', 'f', ';'
};
static PRUnichar const VALUE_1577[] = {
  0x00aa
};
static PRUnichar const NAME_1578[] = {
  'o', 'r', 'd', 'm'
};
static PRUnichar const VALUE_1578[] = {
  0x00ba
};
static PRUnichar const NAME_1579[] = {
  'o', 'r', 'd', 'm', ';'
};
static PRUnichar const VALUE_1579[] = {
  0x00ba
};
static PRUnichar const NAME_1580[] = {
  'o', 'r', 'i', 'g', 'o', 'f', ';'
};
static PRUnichar const VALUE_1580[] = {
  0x22b6
};
static PRUnichar const NAME_1581[] = {
  'o', 'r', 'o', 'r', ';'
};
static PRUnichar const VALUE_1581[] = {
  0x2a56
};
static PRUnichar const NAME_1582[] = {
  'o', 'r', 's', 'l', 'o', 'p', 'e', ';'
};
static PRUnichar const VALUE_1582[] = {
  0x2a57
};
static PRUnichar const NAME_1583[] = {
  'o', 'r', 'v', ';'
};
static PRUnichar const VALUE_1583[] = {
  0x2a5b
};
static PRUnichar const NAME_1584[] = {
  'o', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_1584[] = {
  0x2134
};
static PRUnichar const NAME_1585[] = {
  'o', 's', 'l', 'a', 's', 'h'
};
static PRUnichar const VALUE_1585[] = {
  0x00f8
};
static PRUnichar const NAME_1586[] = {
  'o', 's', 'l', 'a', 's', 'h', ';'
};
static PRUnichar const VALUE_1586[] = {
  0x00f8
};
static PRUnichar const NAME_1587[] = {
  'o', 's', 'o', 'l', ';'
};
static PRUnichar const VALUE_1587[] = {
  0x2298
};
static PRUnichar const NAME_1588[] = {
  'o', 't', 'i', 'l', 'd', 'e'
};
static PRUnichar const VALUE_1588[] = {
  0x00f5
};
static PRUnichar const NAME_1589[] = {
  'o', 't', 'i', 'l', 'd', 'e', ';'
};
static PRUnichar const VALUE_1589[] = {
  0x00f5
};
static PRUnichar const NAME_1590[] = {
  'o', 't', 'i', 'm', 'e', 's', ';'
};
static PRUnichar const VALUE_1590[] = {
  0x2297
};
static PRUnichar const NAME_1591[] = {
  'o', 't', 'i', 'm', 'e', 's', 'a', 's', ';'
};
static PRUnichar const VALUE_1591[] = {
  0x2a36
};
static PRUnichar const NAME_1592[] = {
  'o', 'u', 'm', 'l'
};
static PRUnichar const VALUE_1592[] = {
  0x00f6
};
static PRUnichar const NAME_1593[] = {
  'o', 'u', 'm', 'l', ';'
};
static PRUnichar const VALUE_1593[] = {
  0x00f6
};
static PRUnichar const NAME_1594[] = {
  'o', 'v', 'b', 'a', 'r', ';'
};
static PRUnichar const VALUE_1594[] = {
  0x233d
};
static PRUnichar const NAME_1595[] = {
  'p', 'a', 'r', ';'
};
static PRUnichar const VALUE_1595[] = {
  0x2225
};
static PRUnichar const NAME_1596[] = {
  'p', 'a', 'r', 'a'
};
static PRUnichar const VALUE_1596[] = {
  0x00b6
};
static PRUnichar const NAME_1597[] = {
  'p', 'a', 'r', 'a', ';'
};
static PRUnichar const VALUE_1597[] = {
  0x00b6
};
static PRUnichar const NAME_1598[] = {
  'p', 'a', 'r', 'a', 'l', 'l', 'e', 'l', ';'
};
static PRUnichar const VALUE_1598[] = {
  0x2225
};
static PRUnichar const NAME_1599[] = {
  'p', 'a', 'r', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1599[] = {
  0x2af3
};
static PRUnichar const NAME_1600[] = {
  'p', 'a', 'r', 's', 'l', ';'
};
static PRUnichar const VALUE_1600[] = {
  0x2afd
};
static PRUnichar const NAME_1601[] = {
  'p', 'a', 'r', 't', ';'
};
static PRUnichar const VALUE_1601[] = {
  0x2202
};
static PRUnichar const NAME_1602[] = {
  'p', 'c', 'y', ';'
};
static PRUnichar const VALUE_1602[] = {
  0x043f
};
static PRUnichar const NAME_1603[] = {
  'p', 'e', 'r', 'c', 'n', 't', ';'
};
static PRUnichar const VALUE_1603[] = {
  0x0025
};
static PRUnichar const NAME_1604[] = {
  'p', 'e', 'r', 'i', 'o', 'd', ';'
};
static PRUnichar const VALUE_1604[] = {
  0x002e
};
static PRUnichar const NAME_1605[] = {
  'p', 'e', 'r', 'm', 'i', 'l', ';'
};
static PRUnichar const VALUE_1605[] = {
  0x2030
};
static PRUnichar const NAME_1606[] = {
  'p', 'e', 'r', 'p', ';'
};
static PRUnichar const VALUE_1606[] = {
  0x22a5
};
static PRUnichar const NAME_1607[] = {
  'p', 'e', 'r', 't', 'e', 'n', 'k', ';'
};
static PRUnichar const VALUE_1607[] = {
  0x2031
};
static PRUnichar const NAME_1608[] = {
  'p', 'f', 'r', ';'
};
static PRUnichar const VALUE_1608[] = {
  0xd835, 0xdd2d
};
static PRUnichar const NAME_1609[] = {
  'p', 'h', 'i', ';'
};
static PRUnichar const VALUE_1609[] = {
  0x03c6
};
static PRUnichar const NAME_1610[] = {
  'p', 'h', 'i', 'v', ';'
};
static PRUnichar const VALUE_1610[] = {
  0x03c6
};
static PRUnichar const NAME_1611[] = {
  'p', 'h', 'm', 'm', 'a', 't', ';'
};
static PRUnichar const VALUE_1611[] = {
  0x2133
};
static PRUnichar const NAME_1612[] = {
  'p', 'h', 'o', 'n', 'e', ';'
};
static PRUnichar const VALUE_1612[] = {
  0x260e
};
static PRUnichar const NAME_1613[] = {
  'p', 'i', ';'
};
static PRUnichar const VALUE_1613[] = {
  0x03c0
};
static PRUnichar const NAME_1614[] = {
  'p', 'i', 't', 'c', 'h', 'f', 'o', 'r', 'k', ';'
};
static PRUnichar const VALUE_1614[] = {
  0x22d4
};
static PRUnichar const NAME_1615[] = {
  'p', 'i', 'v', ';'
};
static PRUnichar const VALUE_1615[] = {
  0x03d6
};
static PRUnichar const NAME_1616[] = {
  'p', 'l', 'a', 'n', 'c', 'k', ';'
};
static PRUnichar const VALUE_1616[] = {
  0x210f
};
static PRUnichar const NAME_1617[] = {
  'p', 'l', 'a', 'n', 'c', 'k', 'h', ';'
};
static PRUnichar const VALUE_1617[] = {
  0x210e
};
static PRUnichar const NAME_1618[] = {
  'p', 'l', 'a', 'n', 'k', 'v', ';'
};
static PRUnichar const VALUE_1618[] = {
  0x210f
};
static PRUnichar const NAME_1619[] = {
  'p', 'l', 'u', 's', ';'
};
static PRUnichar const VALUE_1619[] = {
  0x002b
};
static PRUnichar const NAME_1620[] = {
  'p', 'l', 'u', 's', 'a', 'c', 'i', 'r', ';'
};
static PRUnichar const VALUE_1620[] = {
  0x2a23
};
static PRUnichar const NAME_1621[] = {
  'p', 'l', 'u', 's', 'b', ';'
};
static PRUnichar const VALUE_1621[] = {
  0x229e
};
static PRUnichar const NAME_1622[] = {
  'p', 'l', 'u', 's', 'c', 'i', 'r', ';'
};
static PRUnichar const VALUE_1622[] = {
  0x2a22
};
static PRUnichar const NAME_1623[] = {
  'p', 'l', 'u', 's', 'd', 'o', ';'
};
static PRUnichar const VALUE_1623[] = {
  0x2214
};
static PRUnichar const NAME_1624[] = {
  'p', 'l', 'u', 's', 'd', 'u', ';'
};
static PRUnichar const VALUE_1624[] = {
  0x2a25
};
static PRUnichar const NAME_1625[] = {
  'p', 'l', 'u', 's', 'e', ';'
};
static PRUnichar const VALUE_1625[] = {
  0x2a72
};
static PRUnichar const NAME_1626[] = {
  'p', 'l', 'u', 's', 'm', 'n'
};
static PRUnichar const VALUE_1626[] = {
  0x00b1
};
static PRUnichar const NAME_1627[] = {
  'p', 'l', 'u', 's', 'm', 'n', ';'
};
static PRUnichar const VALUE_1627[] = {
  0x00b1
};
static PRUnichar const NAME_1628[] = {
  'p', 'l', 'u', 's', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1628[] = {
  0x2a26
};
static PRUnichar const NAME_1629[] = {
  'p', 'l', 'u', 's', 't', 'w', 'o', ';'
};
static PRUnichar const VALUE_1629[] = {
  0x2a27
};
static PRUnichar const NAME_1630[] = {
  'p', 'm', ';'
};
static PRUnichar const VALUE_1630[] = {
  0x00b1
};
static PRUnichar const NAME_1631[] = {
  'p', 'o', 'i', 'n', 't', 'i', 'n', 't', ';'
};
static PRUnichar const VALUE_1631[] = {
  0x2a15
};
static PRUnichar const NAME_1632[] = {
  'p', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_1632[] = {
  0xd835, 0xdd61
};
static PRUnichar const NAME_1633[] = {
  'p', 'o', 'u', 'n', 'd'
};
static PRUnichar const VALUE_1633[] = {
  0x00a3
};
static PRUnichar const NAME_1634[] = {
  'p', 'o', 'u', 'n', 'd', ';'
};
static PRUnichar const VALUE_1634[] = {
  0x00a3
};
static PRUnichar const NAME_1635[] = {
  'p', 'r', ';'
};
static PRUnichar const VALUE_1635[] = {
  0x227a
};
static PRUnichar const NAME_1636[] = {
  'p', 'r', 'E', ';'
};
static PRUnichar const VALUE_1636[] = {
  0x2ab3
};
static PRUnichar const NAME_1637[] = {
  'p', 'r', 'a', 'p', ';'
};
static PRUnichar const VALUE_1637[] = {
  0x2ab7
};
static PRUnichar const NAME_1638[] = {
  'p', 'r', 'c', 'u', 'e', ';'
};
static PRUnichar const VALUE_1638[] = {
  0x227c
};
static PRUnichar const NAME_1639[] = {
  'p', 'r', 'e', ';'
};
static PRUnichar const VALUE_1639[] = {
  0x2aaf
};
static PRUnichar const NAME_1640[] = {
  'p', 'r', 'e', 'c', ';'
};
static PRUnichar const VALUE_1640[] = {
  0x227a
};
static PRUnichar const NAME_1641[] = {
  'p', 'r', 'e', 'c', 'a', 'p', 'p', 'r', 'o', 'x', ';'
};
static PRUnichar const VALUE_1641[] = {
  0x2ab7
};
static PRUnichar const NAME_1642[] = {
  'p', 'r', 'e', 'c', 'c', 'u', 'r', 'l', 'y', 'e', 'q', ';'
};
static PRUnichar const VALUE_1642[] = {
  0x227c
};
static PRUnichar const NAME_1643[] = {
  'p', 'r', 'e', 'c', 'e', 'q', ';'
};
static PRUnichar const VALUE_1643[] = {
  0x2aaf
};
static PRUnichar const NAME_1644[] = {
  'p', 'r', 'e', 'c', 'n', 'a', 'p', 'p', 'r', 'o', 'x', ';'
};
static PRUnichar const VALUE_1644[] = {
  0x2ab9
};
static PRUnichar const NAME_1645[] = {
  'p', 'r', 'e', 'c', 'n', 'e', 'q', 'q', ';'
};
static PRUnichar const VALUE_1645[] = {
  0x2ab5
};
static PRUnichar const NAME_1646[] = {
  'p', 'r', 'e', 'c', 'n', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1646[] = {
  0x22e8
};
static PRUnichar const NAME_1647[] = {
  'p', 'r', 'e', 'c', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1647[] = {
  0x227e
};
static PRUnichar const NAME_1648[] = {
  'p', 'r', 'i', 'm', 'e', ';'
};
static PRUnichar const VALUE_1648[] = {
  0x2032
};
static PRUnichar const NAME_1649[] = {
  'p', 'r', 'i', 'm', 'e', 's', ';'
};
static PRUnichar const VALUE_1649[] = {
  0x2119
};
static PRUnichar const NAME_1650[] = {
  'p', 'r', 'n', 'E', ';'
};
static PRUnichar const VALUE_1650[] = {
  0x2ab5
};
static PRUnichar const NAME_1651[] = {
  'p', 'r', 'n', 'a', 'p', ';'
};
static PRUnichar const VALUE_1651[] = {
  0x2ab9
};
static PRUnichar const NAME_1652[] = {
  'p', 'r', 'n', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1652[] = {
  0x22e8
};
static PRUnichar const NAME_1653[] = {
  'p', 'r', 'o', 'd', ';'
};
static PRUnichar const VALUE_1653[] = {
  0x220f
};
static PRUnichar const NAME_1654[] = {
  'p', 'r', 'o', 'f', 'a', 'l', 'a', 'r', ';'
};
static PRUnichar const VALUE_1654[] = {
  0x232e
};
static PRUnichar const NAME_1655[] = {
  'p', 'r', 'o', 'f', 'l', 'i', 'n', 'e', ';'
};
static PRUnichar const VALUE_1655[] = {
  0x2312
};
static PRUnichar const NAME_1656[] = {
  'p', 'r', 'o', 'f', 's', 'u', 'r', 'f', ';'
};
static PRUnichar const VALUE_1656[] = {
  0x2313
};
static PRUnichar const NAME_1657[] = {
  'p', 'r', 'o', 'p', ';'
};
static PRUnichar const VALUE_1657[] = {
  0x221d
};
static PRUnichar const NAME_1658[] = {
  'p', 'r', 'o', 'p', 't', 'o', ';'
};
static PRUnichar const VALUE_1658[] = {
  0x221d
};
static PRUnichar const NAME_1659[] = {
  'p', 'r', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1659[] = {
  0x227e
};
static PRUnichar const NAME_1660[] = {
  'p', 'r', 'u', 'r', 'e', 'l', ';'
};
static PRUnichar const VALUE_1660[] = {
  0x22b0
};
static PRUnichar const NAME_1661[] = {
  'p', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_1661[] = {
  0xd835, 0xdcc5
};
static PRUnichar const NAME_1662[] = {
  'p', 's', 'i', ';'
};
static PRUnichar const VALUE_1662[] = {
  0x03c8
};
static PRUnichar const NAME_1663[] = {
  'p', 'u', 'n', 'c', 's', 'p', ';'
};
static PRUnichar const VALUE_1663[] = {
  0x2008
};
static PRUnichar const NAME_1664[] = {
  'q', 'f', 'r', ';'
};
static PRUnichar const VALUE_1664[] = {
  0xd835, 0xdd2e
};
static PRUnichar const NAME_1665[] = {
  'q', 'i', 'n', 't', ';'
};
static PRUnichar const VALUE_1665[] = {
  0x2a0c
};
static PRUnichar const NAME_1666[] = {
  'q', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_1666[] = {
  0xd835, 0xdd62
};
static PRUnichar const NAME_1667[] = {
  'q', 'p', 'r', 'i', 'm', 'e', ';'
};
static PRUnichar const VALUE_1667[] = {
  0x2057
};
static PRUnichar const NAME_1668[] = {
  'q', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_1668[] = {
  0xd835, 0xdcc6
};
static PRUnichar const NAME_1669[] = {
  'q', 'u', 'a', 't', 'e', 'r', 'n', 'i', 'o', 'n', 's', ';'
};
static PRUnichar const VALUE_1669[] = {
  0x210d
};
static PRUnichar const NAME_1670[] = {
  'q', 'u', 'a', 't', 'i', 'n', 't', ';'
};
static PRUnichar const VALUE_1670[] = {
  0x2a16
};
static PRUnichar const NAME_1671[] = {
  'q', 'u', 'e', 's', 't', ';'
};
static PRUnichar const VALUE_1671[] = {
  0x003f
};
static PRUnichar const NAME_1672[] = {
  'q', 'u', 'e', 's', 't', 'e', 'q', ';'
};
static PRUnichar const VALUE_1672[] = {
  0x225f
};
static PRUnichar const NAME_1673[] = {
  'q', 'u', 'o', 't'
};
static PRUnichar const VALUE_1673[] = {
  0x0022
};
static PRUnichar const NAME_1674[] = {
  'q', 'u', 'o', 't', ';'
};
static PRUnichar const VALUE_1674[] = {
  0x0022
};
static PRUnichar const NAME_1675[] = {
  'r', 'A', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1675[] = {
  0x21db
};
static PRUnichar const NAME_1676[] = {
  'r', 'A', 'r', 'r', ';'
};
static PRUnichar const VALUE_1676[] = {
  0x21d2
};
static PRUnichar const NAME_1677[] = {
  'r', 'A', 't', 'a', 'i', 'l', ';'
};
static PRUnichar const VALUE_1677[] = {
  0x291c
};
static PRUnichar const NAME_1678[] = {
  'r', 'B', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1678[] = {
  0x290f
};
static PRUnichar const NAME_1679[] = {
  'r', 'H', 'a', 'r', ';'
};
static PRUnichar const VALUE_1679[] = {
  0x2964
};
static PRUnichar const NAME_1680[] = {
  'r', 'a', 'c', 'e', ';'
};
static PRUnichar const VALUE_1680[] = {
  0x29da
};
static PRUnichar const NAME_1681[] = {
  'r', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_1681[] = {
  0x0155
};
static PRUnichar const NAME_1682[] = {
  'r', 'a', 'd', 'i', 'c', ';'
};
static PRUnichar const VALUE_1682[] = {
  0x221a
};
static PRUnichar const NAME_1683[] = {
  'r', 'a', 'e', 'm', 'p', 't', 'y', 'v', ';'
};
static PRUnichar const VALUE_1683[] = {
  0x29b3
};
static PRUnichar const NAME_1684[] = {
  'r', 'a', 'n', 'g', ';'
};
static PRUnichar const VALUE_1684[] = {
  0x27e9
};
static PRUnichar const NAME_1685[] = {
  'r', 'a', 'n', 'g', 'd', ';'
};
static PRUnichar const VALUE_1685[] = {
  0x2992
};
static PRUnichar const NAME_1686[] = {
  'r', 'a', 'n', 'g', 'e', ';'
};
static PRUnichar const VALUE_1686[] = {
  0x29a5
};
static PRUnichar const NAME_1687[] = {
  'r', 'a', 'n', 'g', 'l', 'e', ';'
};
static PRUnichar const VALUE_1687[] = {
  0x27e9
};
static PRUnichar const NAME_1688[] = {
  'r', 'a', 'q', 'u', 'o'
};
static PRUnichar const VALUE_1688[] = {
  0x00bb
};
static PRUnichar const NAME_1689[] = {
  'r', 'a', 'q', 'u', 'o', ';'
};
static PRUnichar const VALUE_1689[] = {
  0x00bb
};
static PRUnichar const NAME_1690[] = {
  'r', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1690[] = {
  0x2192
};
static PRUnichar const NAME_1691[] = {
  'r', 'a', 'r', 'r', 'a', 'p', ';'
};
static PRUnichar const VALUE_1691[] = {
  0x2975
};
static PRUnichar const NAME_1692[] = {
  'r', 'a', 'r', 'r', 'b', ';'
};
static PRUnichar const VALUE_1692[] = {
  0x21e5
};
static PRUnichar const NAME_1693[] = {
  'r', 'a', 'r', 'r', 'b', 'f', 's', ';'
};
static PRUnichar const VALUE_1693[] = {
  0x2920
};
static PRUnichar const NAME_1694[] = {
  'r', 'a', 'r', 'r', 'c', ';'
};
static PRUnichar const VALUE_1694[] = {
  0x2933
};
static PRUnichar const NAME_1695[] = {
  'r', 'a', 'r', 'r', 'f', 's', ';'
};
static PRUnichar const VALUE_1695[] = {
  0x291e
};
static PRUnichar const NAME_1696[] = {
  'r', 'a', 'r', 'r', 'h', 'k', ';'
};
static PRUnichar const VALUE_1696[] = {
  0x21aa
};
static PRUnichar const NAME_1697[] = {
  'r', 'a', 'r', 'r', 'l', 'p', ';'
};
static PRUnichar const VALUE_1697[] = {
  0x21ac
};
static PRUnichar const NAME_1698[] = {
  'r', 'a', 'r', 'r', 'p', 'l', ';'
};
static PRUnichar const VALUE_1698[] = {
  0x2945
};
static PRUnichar const NAME_1699[] = {
  'r', 'a', 'r', 'r', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1699[] = {
  0x2974
};
static PRUnichar const NAME_1700[] = {
  'r', 'a', 'r', 'r', 't', 'l', ';'
};
static PRUnichar const VALUE_1700[] = {
  0x21a3
};
static PRUnichar const NAME_1701[] = {
  'r', 'a', 'r', 'r', 'w', ';'
};
static PRUnichar const VALUE_1701[] = {
  0x219d
};
static PRUnichar const NAME_1702[] = {
  'r', 'a', 't', 'a', 'i', 'l', ';'
};
static PRUnichar const VALUE_1702[] = {
  0x291a
};
static PRUnichar const NAME_1703[] = {
  'r', 'a', 't', 'i', 'o', ';'
};
static PRUnichar const VALUE_1703[] = {
  0x2236
};
static PRUnichar const NAME_1704[] = {
  'r', 'a', 't', 'i', 'o', 'n', 'a', 'l', 's', ';'
};
static PRUnichar const VALUE_1704[] = {
  0x211a
};
static PRUnichar const NAME_1705[] = {
  'r', 'b', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1705[] = {
  0x290d
};
static PRUnichar const NAME_1706[] = {
  'r', 'b', 'b', 'r', 'k', ';'
};
static PRUnichar const VALUE_1706[] = {
  0x2773
};
static PRUnichar const NAME_1707[] = {
  'r', 'b', 'r', 'a', 'c', 'e', ';'
};
static PRUnichar const VALUE_1707[] = {
  0x007d
};
static PRUnichar const NAME_1708[] = {
  'r', 'b', 'r', 'a', 'c', 'k', ';'
};
static PRUnichar const VALUE_1708[] = {
  0x005d
};
static PRUnichar const NAME_1709[] = {
  'r', 'b', 'r', 'k', 'e', ';'
};
static PRUnichar const VALUE_1709[] = {
  0x298c
};
static PRUnichar const NAME_1710[] = {
  'r', 'b', 'r', 'k', 's', 'l', 'd', ';'
};
static PRUnichar const VALUE_1710[] = {
  0x298e
};
static PRUnichar const NAME_1711[] = {
  'r', 'b', 'r', 'k', 's', 'l', 'u', ';'
};
static PRUnichar const VALUE_1711[] = {
  0x2990
};
static PRUnichar const NAME_1712[] = {
  'r', 'c', 'a', 'r', 'o', 'n', ';'
};
static PRUnichar const VALUE_1712[] = {
  0x0159
};
static PRUnichar const NAME_1713[] = {
  'r', 'c', 'e', 'd', 'i', 'l', ';'
};
static PRUnichar const VALUE_1713[] = {
  0x0157
};
static PRUnichar const NAME_1714[] = {
  'r', 'c', 'e', 'i', 'l', ';'
};
static PRUnichar const VALUE_1714[] = {
  0x2309
};
static PRUnichar const NAME_1715[] = {
  'r', 'c', 'u', 'b', ';'
};
static PRUnichar const VALUE_1715[] = {
  0x007d
};
static PRUnichar const NAME_1716[] = {
  'r', 'c', 'y', ';'
};
static PRUnichar const VALUE_1716[] = {
  0x0440
};
static PRUnichar const NAME_1717[] = {
  'r', 'd', 'c', 'a', ';'
};
static PRUnichar const VALUE_1717[] = {
  0x2937
};
static PRUnichar const NAME_1718[] = {
  'r', 'd', 'l', 'd', 'h', 'a', 'r', ';'
};
static PRUnichar const VALUE_1718[] = {
  0x2969
};
static PRUnichar const NAME_1719[] = {
  'r', 'd', 'q', 'u', 'o', ';'
};
static PRUnichar const VALUE_1719[] = {
  0x201d
};
static PRUnichar const NAME_1720[] = {
  'r', 'd', 'q', 'u', 'o', 'r', ';'
};
static PRUnichar const VALUE_1720[] = {
  0x201d
};
static PRUnichar const NAME_1721[] = {
  'r', 'd', 's', 'h', ';'
};
static PRUnichar const VALUE_1721[] = {
  0x21b3
};
static PRUnichar const NAME_1722[] = {
  'r', 'e', 'a', 'l', ';'
};
static PRUnichar const VALUE_1722[] = {
  0x211c
};
static PRUnichar const NAME_1723[] = {
  'r', 'e', 'a', 'l', 'i', 'n', 'e', ';'
};
static PRUnichar const VALUE_1723[] = {
  0x211b
};
static PRUnichar const NAME_1724[] = {
  'r', 'e', 'a', 'l', 'p', 'a', 'r', 't', ';'
};
static PRUnichar const VALUE_1724[] = {
  0x211c
};
static PRUnichar const NAME_1725[] = {
  'r', 'e', 'a', 'l', 's', ';'
};
static PRUnichar const VALUE_1725[] = {
  0x211d
};
static PRUnichar const NAME_1726[] = {
  'r', 'e', 'c', 't', ';'
};
static PRUnichar const VALUE_1726[] = {
  0x25ad
};
static PRUnichar const NAME_1727[] = {
  'r', 'e', 'g'
};
static PRUnichar const VALUE_1727[] = {
  0x00ae
};
static PRUnichar const NAME_1728[] = {
  'r', 'e', 'g', ';'
};
static PRUnichar const VALUE_1728[] = {
  0x00ae
};
static PRUnichar const NAME_1729[] = {
  'r', 'f', 'i', 's', 'h', 't', ';'
};
static PRUnichar const VALUE_1729[] = {
  0x297d
};
static PRUnichar const NAME_1730[] = {
  'r', 'f', 'l', 'o', 'o', 'r', ';'
};
static PRUnichar const VALUE_1730[] = {
  0x230b
};
static PRUnichar const NAME_1731[] = {
  'r', 'f', 'r', ';'
};
static PRUnichar const VALUE_1731[] = {
  0xd835, 0xdd2f
};
static PRUnichar const NAME_1732[] = {
  'r', 'h', 'a', 'r', 'd', ';'
};
static PRUnichar const VALUE_1732[] = {
  0x21c1
};
static PRUnichar const NAME_1733[] = {
  'r', 'h', 'a', 'r', 'u', ';'
};
static PRUnichar const VALUE_1733[] = {
  0x21c0
};
static PRUnichar const NAME_1734[] = {
  'r', 'h', 'a', 'r', 'u', 'l', ';'
};
static PRUnichar const VALUE_1734[] = {
  0x296c
};
static PRUnichar const NAME_1735[] = {
  'r', 'h', 'o', ';'
};
static PRUnichar const VALUE_1735[] = {
  0x03c1
};
static PRUnichar const NAME_1736[] = {
  'r', 'h', 'o', 'v', ';'
};
static PRUnichar const VALUE_1736[] = {
  0x03f1
};
static PRUnichar const NAME_1737[] = {
  'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_1737[] = {
  0x2192
};
static PRUnichar const NAME_1738[] = {
  'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', 't', 'a', 'i', 'l', ';'
};
static PRUnichar const VALUE_1738[] = {
  0x21a3
};
static PRUnichar const NAME_1739[] = {
  'r', 'i', 'g', 'h', 't', 'h', 'a', 'r', 'p', 'o', 'o', 'n', 'd', 'o', 'w', 'n', ';'
};
static PRUnichar const VALUE_1739[] = {
  0x21c1
};
static PRUnichar const NAME_1740[] = {
  'r', 'i', 'g', 'h', 't', 'h', 'a', 'r', 'p', 'o', 'o', 'n', 'u', 'p', ';'
};
static PRUnichar const VALUE_1740[] = {
  0x21c0
};
static PRUnichar const NAME_1741[] = {
  'r', 'i', 'g', 'h', 't', 'l', 'e', 'f', 't', 'a', 'r', 'r', 'o', 'w', 's', ';'
};
static PRUnichar const VALUE_1741[] = {
  0x21c4
};
static PRUnichar const NAME_1742[] = {
  'r', 'i', 'g', 'h', 't', 'l', 'e', 'f', 't', 'h', 'a', 'r', 'p', 'o', 'o', 'n', 's', ';'
};
static PRUnichar const VALUE_1742[] = {
  0x21cc
};
static PRUnichar const NAME_1743[] = {
  'r', 'i', 'g', 'h', 't', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', 's', ';'
};
static PRUnichar const VALUE_1743[] = {
  0x21c9
};
static PRUnichar const NAME_1744[] = {
  'r', 'i', 'g', 'h', 't', 's', 'q', 'u', 'i', 'g', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_1744[] = {
  0x219d
};
static PRUnichar const NAME_1745[] = {
  'r', 'i', 'g', 'h', 't', 't', 'h', 'r', 'e', 'e', 't', 'i', 'm', 'e', 's', ';'
};
static PRUnichar const VALUE_1745[] = {
  0x22cc
};
static PRUnichar const NAME_1746[] = {
  'r', 'i', 'n', 'g', ';'
};
static PRUnichar const VALUE_1746[] = {
  0x02da
};
static PRUnichar const NAME_1747[] = {
  'r', 'i', 's', 'i', 'n', 'g', 'd', 'o', 't', 's', 'e', 'q', ';'
};
static PRUnichar const VALUE_1747[] = {
  0x2253
};
static PRUnichar const NAME_1748[] = {
  'r', 'l', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1748[] = {
  0x21c4
};
static PRUnichar const NAME_1749[] = {
  'r', 'l', 'h', 'a', 'r', ';'
};
static PRUnichar const VALUE_1749[] = {
  0x21cc
};
static PRUnichar const NAME_1750[] = {
  'r', 'l', 'm', ';'
};
static PRUnichar const VALUE_1750[] = {
  0x200f
};
static PRUnichar const NAME_1751[] = {
  'r', 'm', 'o', 'u', 's', 't', ';'
};
static PRUnichar const VALUE_1751[] = {
  0x23b1
};
static PRUnichar const NAME_1752[] = {
  'r', 'm', 'o', 'u', 's', 't', 'a', 'c', 'h', 'e', ';'
};
static PRUnichar const VALUE_1752[] = {
  0x23b1
};
static PRUnichar const NAME_1753[] = {
  'r', 'n', 'm', 'i', 'd', ';'
};
static PRUnichar const VALUE_1753[] = {
  0x2aee
};
static PRUnichar const NAME_1754[] = {
  'r', 'o', 'a', 'n', 'g', ';'
};
static PRUnichar const VALUE_1754[] = {
  0x27ed
};
static PRUnichar const NAME_1755[] = {
  'r', 'o', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1755[] = {
  0x21fe
};
static PRUnichar const NAME_1756[] = {
  'r', 'o', 'b', 'r', 'k', ';'
};
static PRUnichar const VALUE_1756[] = {
  0x27e7
};
static PRUnichar const NAME_1757[] = {
  'r', 'o', 'p', 'a', 'r', ';'
};
static PRUnichar const VALUE_1757[] = {
  0x2986
};
static PRUnichar const NAME_1758[] = {
  'r', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_1758[] = {
  0xd835, 0xdd63
};
static PRUnichar const NAME_1759[] = {
  'r', 'o', 'p', 'l', 'u', 's', ';'
};
static PRUnichar const VALUE_1759[] = {
  0x2a2e
};
static PRUnichar const NAME_1760[] = {
  'r', 'o', 't', 'i', 'm', 'e', 's', ';'
};
static PRUnichar const VALUE_1760[] = {
  0x2a35
};
static PRUnichar const NAME_1761[] = {
  'r', 'p', 'a', 'r', ';'
};
static PRUnichar const VALUE_1761[] = {
  0x0029
};
static PRUnichar const NAME_1762[] = {
  'r', 'p', 'a', 'r', 'g', 't', ';'
};
static PRUnichar const VALUE_1762[] = {
  0x2994
};
static PRUnichar const NAME_1763[] = {
  'r', 'p', 'p', 'o', 'l', 'i', 'n', 't', ';'
};
static PRUnichar const VALUE_1763[] = {
  0x2a12
};
static PRUnichar const NAME_1764[] = {
  'r', 'r', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1764[] = {
  0x21c9
};
static PRUnichar const NAME_1765[] = {
  'r', 's', 'a', 'q', 'u', 'o', ';'
};
static PRUnichar const VALUE_1765[] = {
  0x203a
};
static PRUnichar const NAME_1766[] = {
  'r', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_1766[] = {
  0xd835, 0xdcc7
};
static PRUnichar const NAME_1767[] = {
  'r', 's', 'h', ';'
};
static PRUnichar const VALUE_1767[] = {
  0x21b1
};
static PRUnichar const NAME_1768[] = {
  'r', 's', 'q', 'b', ';'
};
static PRUnichar const VALUE_1768[] = {
  0x005d
};
static PRUnichar const NAME_1769[] = {
  'r', 's', 'q', 'u', 'o', ';'
};
static PRUnichar const VALUE_1769[] = {
  0x2019
};
static PRUnichar const NAME_1770[] = {
  'r', 's', 'q', 'u', 'o', 'r', ';'
};
static PRUnichar const VALUE_1770[] = {
  0x2019
};
static PRUnichar const NAME_1771[] = {
  'r', 't', 'h', 'r', 'e', 'e', ';'
};
static PRUnichar const VALUE_1771[] = {
  0x22cc
};
static PRUnichar const NAME_1772[] = {
  'r', 't', 'i', 'm', 'e', 's', ';'
};
static PRUnichar const VALUE_1772[] = {
  0x22ca
};
static PRUnichar const NAME_1773[] = {
  'r', 't', 'r', 'i', ';'
};
static PRUnichar const VALUE_1773[] = {
  0x25b9
};
static PRUnichar const NAME_1774[] = {
  'r', 't', 'r', 'i', 'e', ';'
};
static PRUnichar const VALUE_1774[] = {
  0x22b5
};
static PRUnichar const NAME_1775[] = {
  'r', 't', 'r', 'i', 'f', ';'
};
static PRUnichar const VALUE_1775[] = {
  0x25b8
};
static PRUnichar const NAME_1776[] = {
  'r', 't', 'r', 'i', 'l', 't', 'r', 'i', ';'
};
static PRUnichar const VALUE_1776[] = {
  0x29ce
};
static PRUnichar const NAME_1777[] = {
  'r', 'u', 'l', 'u', 'h', 'a', 'r', ';'
};
static PRUnichar const VALUE_1777[] = {
  0x2968
};
static PRUnichar const NAME_1778[] = {
  'r', 'x', ';'
};
static PRUnichar const VALUE_1778[] = {
  0x211e
};
static PRUnichar const NAME_1779[] = {
  's', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_1779[] = {
  0x015b
};
static PRUnichar const NAME_1780[] = {
  's', 'b', 'q', 'u', 'o', ';'
};
static PRUnichar const VALUE_1780[] = {
  0x201a
};
static PRUnichar const NAME_1781[] = {
  's', 'c', ';'
};
static PRUnichar const VALUE_1781[] = {
  0x227b
};
static PRUnichar const NAME_1782[] = {
  's', 'c', 'E', ';'
};
static PRUnichar const VALUE_1782[] = {
  0x2ab4
};
static PRUnichar const NAME_1783[] = {
  's', 'c', 'a', 'p', ';'
};
static PRUnichar const VALUE_1783[] = {
  0x2ab8
};
static PRUnichar const NAME_1784[] = {
  's', 'c', 'a', 'r', 'o', 'n', ';'
};
static PRUnichar const VALUE_1784[] = {
  0x0161
};
static PRUnichar const NAME_1785[] = {
  's', 'c', 'c', 'u', 'e', ';'
};
static PRUnichar const VALUE_1785[] = {
  0x227d
};
static PRUnichar const NAME_1786[] = {
  's', 'c', 'e', ';'
};
static PRUnichar const VALUE_1786[] = {
  0x2ab0
};
static PRUnichar const NAME_1787[] = {
  's', 'c', 'e', 'd', 'i', 'l', ';'
};
static PRUnichar const VALUE_1787[] = {
  0x015f
};
static PRUnichar const NAME_1788[] = {
  's', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_1788[] = {
  0x015d
};
static PRUnichar const NAME_1789[] = {
  's', 'c', 'n', 'E', ';'
};
static PRUnichar const VALUE_1789[] = {
  0x2ab6
};
static PRUnichar const NAME_1790[] = {
  's', 'c', 'n', 'a', 'p', ';'
};
static PRUnichar const VALUE_1790[] = {
  0x2aba
};
static PRUnichar const NAME_1791[] = {
  's', 'c', 'n', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1791[] = {
  0x22e9
};
static PRUnichar const NAME_1792[] = {
  's', 'c', 'p', 'o', 'l', 'i', 'n', 't', ';'
};
static PRUnichar const VALUE_1792[] = {
  0x2a13
};
static PRUnichar const NAME_1793[] = {
  's', 'c', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1793[] = {
  0x227f
};
static PRUnichar const NAME_1794[] = {
  's', 'c', 'y', ';'
};
static PRUnichar const VALUE_1794[] = {
  0x0441
};
static PRUnichar const NAME_1795[] = {
  's', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_1795[] = {
  0x22c5
};
static PRUnichar const NAME_1796[] = {
  's', 'd', 'o', 't', 'b', ';'
};
static PRUnichar const VALUE_1796[] = {
  0x22a1
};
static PRUnichar const NAME_1797[] = {
  's', 'd', 'o', 't', 'e', ';'
};
static PRUnichar const VALUE_1797[] = {
  0x2a66
};
static PRUnichar const NAME_1798[] = {
  's', 'e', 'A', 'r', 'r', ';'
};
static PRUnichar const VALUE_1798[] = {
  0x21d8
};
static PRUnichar const NAME_1799[] = {
  's', 'e', 'a', 'r', 'h', 'k', ';'
};
static PRUnichar const VALUE_1799[] = {
  0x2925
};
static PRUnichar const NAME_1800[] = {
  's', 'e', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1800[] = {
  0x2198
};
static PRUnichar const NAME_1801[] = {
  's', 'e', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_1801[] = {
  0x2198
};
static PRUnichar const NAME_1802[] = {
  's', 'e', 'c', 't'
};
static PRUnichar const VALUE_1802[] = {
  0x00a7
};
static PRUnichar const NAME_1803[] = {
  's', 'e', 'c', 't', ';'
};
static PRUnichar const VALUE_1803[] = {
  0x00a7
};
static PRUnichar const NAME_1804[] = {
  's', 'e', 'm', 'i', ';'
};
static PRUnichar const VALUE_1804[] = {
  0x003b
};
static PRUnichar const NAME_1805[] = {
  's', 'e', 's', 'w', 'a', 'r', ';'
};
static PRUnichar const VALUE_1805[] = {
  0x2929
};
static PRUnichar const NAME_1806[] = {
  's', 'e', 't', 'm', 'i', 'n', 'u', 's', ';'
};
static PRUnichar const VALUE_1806[] = {
  0x2216
};
static PRUnichar const NAME_1807[] = {
  's', 'e', 't', 'm', 'n', ';'
};
static PRUnichar const VALUE_1807[] = {
  0x2216
};
static PRUnichar const NAME_1808[] = {
  's', 'e', 'x', 't', ';'
};
static PRUnichar const VALUE_1808[] = {
  0x2736
};
static PRUnichar const NAME_1809[] = {
  's', 'f', 'r', ';'
};
static PRUnichar const VALUE_1809[] = {
  0xd835, 0xdd30
};
static PRUnichar const NAME_1810[] = {
  's', 'f', 'r', 'o', 'w', 'n', ';'
};
static PRUnichar const VALUE_1810[] = {
  0x2322
};
static PRUnichar const NAME_1811[] = {
  's', 'h', 'a', 'r', 'p', ';'
};
static PRUnichar const VALUE_1811[] = {
  0x266f
};
static PRUnichar const NAME_1812[] = {
  's', 'h', 'c', 'h', 'c', 'y', ';'
};
static PRUnichar const VALUE_1812[] = {
  0x0449
};
static PRUnichar const NAME_1813[] = {
  's', 'h', 'c', 'y', ';'
};
static PRUnichar const VALUE_1813[] = {
  0x0448
};
static PRUnichar const NAME_1814[] = {
  's', 'h', 'o', 'r', 't', 'm', 'i', 'd', ';'
};
static PRUnichar const VALUE_1814[] = {
  0x2223
};
static PRUnichar const NAME_1815[] = {
  's', 'h', 'o', 'r', 't', 'p', 'a', 'r', 'a', 'l', 'l', 'e', 'l', ';'
};
static PRUnichar const VALUE_1815[] = {
  0x2225
};
static PRUnichar const NAME_1816[] = {
  's', 'h', 'y'
};
static PRUnichar const VALUE_1816[] = {
  0x00ad
};
static PRUnichar const NAME_1817[] = {
  's', 'h', 'y', ';'
};
static PRUnichar const VALUE_1817[] = {
  0x00ad
};
static PRUnichar const NAME_1818[] = {
  's', 'i', 'g', 'm', 'a', ';'
};
static PRUnichar const VALUE_1818[] = {
  0x03c3
};
static PRUnichar const NAME_1819[] = {
  's', 'i', 'g', 'm', 'a', 'f', ';'
};
static PRUnichar const VALUE_1819[] = {
  0x03c2
};
static PRUnichar const NAME_1820[] = {
  's', 'i', 'g', 'm', 'a', 'v', ';'
};
static PRUnichar const VALUE_1820[] = {
  0x03c2
};
static PRUnichar const NAME_1821[] = {
  's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1821[] = {
  0x223c
};
static PRUnichar const NAME_1822[] = {
  's', 'i', 'm', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_1822[] = {
  0x2a6a
};
static PRUnichar const NAME_1823[] = {
  's', 'i', 'm', 'e', ';'
};
static PRUnichar const VALUE_1823[] = {
  0x2243
};
static PRUnichar const NAME_1824[] = {
  's', 'i', 'm', 'e', 'q', ';'
};
static PRUnichar const VALUE_1824[] = {
  0x2243
};
static PRUnichar const NAME_1825[] = {
  's', 'i', 'm', 'g', ';'
};
static PRUnichar const VALUE_1825[] = {
  0x2a9e
};
static PRUnichar const NAME_1826[] = {
  's', 'i', 'm', 'g', 'E', ';'
};
static PRUnichar const VALUE_1826[] = {
  0x2aa0
};
static PRUnichar const NAME_1827[] = {
  's', 'i', 'm', 'l', ';'
};
static PRUnichar const VALUE_1827[] = {
  0x2a9d
};
static PRUnichar const NAME_1828[] = {
  's', 'i', 'm', 'l', 'E', ';'
};
static PRUnichar const VALUE_1828[] = {
  0x2a9f
};
static PRUnichar const NAME_1829[] = {
  's', 'i', 'm', 'n', 'e', ';'
};
static PRUnichar const VALUE_1829[] = {
  0x2246
};
static PRUnichar const NAME_1830[] = {
  's', 'i', 'm', 'p', 'l', 'u', 's', ';'
};
static PRUnichar const VALUE_1830[] = {
  0x2a24
};
static PRUnichar const NAME_1831[] = {
  's', 'i', 'm', 'r', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1831[] = {
  0x2972
};
static PRUnichar const NAME_1832[] = {
  's', 'l', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1832[] = {
  0x2190
};
static PRUnichar const NAME_1833[] = {
  's', 'm', 'a', 'l', 'l', 's', 'e', 't', 'm', 'i', 'n', 'u', 's', ';'
};
static PRUnichar const VALUE_1833[] = {
  0x2216
};
static PRUnichar const NAME_1834[] = {
  's', 'm', 'a', 's', 'h', 'p', ';'
};
static PRUnichar const VALUE_1834[] = {
  0x2a33
};
static PRUnichar const NAME_1835[] = {
  's', 'm', 'e', 'p', 'a', 'r', 's', 'l', ';'
};
static PRUnichar const VALUE_1835[] = {
  0x29e4
};
static PRUnichar const NAME_1836[] = {
  's', 'm', 'i', 'd', ';'
};
static PRUnichar const VALUE_1836[] = {
  0x2223
};
static PRUnichar const NAME_1837[] = {
  's', 'm', 'i', 'l', 'e', ';'
};
static PRUnichar const VALUE_1837[] = {
  0x2323
};
static PRUnichar const NAME_1838[] = {
  's', 'm', 't', ';'
};
static PRUnichar const VALUE_1838[] = {
  0x2aaa
};
static PRUnichar const NAME_1839[] = {
  's', 'm', 't', 'e', ';'
};
static PRUnichar const VALUE_1839[] = {
  0x2aac
};
static PRUnichar const NAME_1840[] = {
  's', 'o', 'f', 't', 'c', 'y', ';'
};
static PRUnichar const VALUE_1840[] = {
  0x044c
};
static PRUnichar const NAME_1841[] = {
  's', 'o', 'l', ';'
};
static PRUnichar const VALUE_1841[] = {
  0x002f
};
static PRUnichar const NAME_1842[] = {
  's', 'o', 'l', 'b', ';'
};
static PRUnichar const VALUE_1842[] = {
  0x29c4
};
static PRUnichar const NAME_1843[] = {
  's', 'o', 'l', 'b', 'a', 'r', ';'
};
static PRUnichar const VALUE_1843[] = {
  0x233f
};
static PRUnichar const NAME_1844[] = {
  's', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_1844[] = {
  0xd835, 0xdd64
};
static PRUnichar const NAME_1845[] = {
  's', 'p', 'a', 'd', 'e', 's', ';'
};
static PRUnichar const VALUE_1845[] = {
  0x2660
};
static PRUnichar const NAME_1846[] = {
  's', 'p', 'a', 'd', 'e', 's', 'u', 'i', 't', ';'
};
static PRUnichar const VALUE_1846[] = {
  0x2660
};
static PRUnichar const NAME_1847[] = {
  's', 'p', 'a', 'r', ';'
};
static PRUnichar const VALUE_1847[] = {
  0x2225
};
static PRUnichar const NAME_1848[] = {
  's', 'q', 'c', 'a', 'p', ';'
};
static PRUnichar const VALUE_1848[] = {
  0x2293
};
static PRUnichar const NAME_1849[] = {
  's', 'q', 'c', 'u', 'p', ';'
};
static PRUnichar const VALUE_1849[] = {
  0x2294
};
static PRUnichar const NAME_1850[] = {
  's', 'q', 's', 'u', 'b', ';'
};
static PRUnichar const VALUE_1850[] = {
  0x228f
};
static PRUnichar const NAME_1851[] = {
  's', 'q', 's', 'u', 'b', 'e', ';'
};
static PRUnichar const VALUE_1851[] = {
  0x2291
};
static PRUnichar const NAME_1852[] = {
  's', 'q', 's', 'u', 'b', 's', 'e', 't', ';'
};
static PRUnichar const VALUE_1852[] = {
  0x228f
};
static PRUnichar const NAME_1853[] = {
  's', 'q', 's', 'u', 'b', 's', 'e', 't', 'e', 'q', ';'
};
static PRUnichar const VALUE_1853[] = {
  0x2291
};
static PRUnichar const NAME_1854[] = {
  's', 'q', 's', 'u', 'p', ';'
};
static PRUnichar const VALUE_1854[] = {
  0x2290
};
static PRUnichar const NAME_1855[] = {
  's', 'q', 's', 'u', 'p', 'e', ';'
};
static PRUnichar const VALUE_1855[] = {
  0x2292
};
static PRUnichar const NAME_1856[] = {
  's', 'q', 's', 'u', 'p', 's', 'e', 't', ';'
};
static PRUnichar const VALUE_1856[] = {
  0x2290
};
static PRUnichar const NAME_1857[] = {
  's', 'q', 's', 'u', 'p', 's', 'e', 't', 'e', 'q', ';'
};
static PRUnichar const VALUE_1857[] = {
  0x2292
};
static PRUnichar const NAME_1858[] = {
  's', 'q', 'u', ';'
};
static PRUnichar const VALUE_1858[] = {
  0x25a1
};
static PRUnichar const NAME_1859[] = {
  's', 'q', 'u', 'a', 'r', 'e', ';'
};
static PRUnichar const VALUE_1859[] = {
  0x25a1
};
static PRUnichar const NAME_1860[] = {
  's', 'q', 'u', 'a', 'r', 'f', ';'
};
static PRUnichar const VALUE_1860[] = {
  0x25aa
};
static PRUnichar const NAME_1861[] = {
  's', 'q', 'u', 'f', ';'
};
static PRUnichar const VALUE_1861[] = {
  0x25aa
};
static PRUnichar const NAME_1862[] = {
  's', 'r', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1862[] = {
  0x2192
};
static PRUnichar const NAME_1863[] = {
  's', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_1863[] = {
  0xd835, 0xdcc8
};
static PRUnichar const NAME_1864[] = {
  's', 's', 'e', 't', 'm', 'n', ';'
};
static PRUnichar const VALUE_1864[] = {
  0x2216
};
static PRUnichar const NAME_1865[] = {
  's', 's', 'm', 'i', 'l', 'e', ';'
};
static PRUnichar const VALUE_1865[] = {
  0x2323
};
static PRUnichar const NAME_1866[] = {
  's', 's', 't', 'a', 'r', 'f', ';'
};
static PRUnichar const VALUE_1866[] = {
  0x22c6
};
static PRUnichar const NAME_1867[] = {
  's', 't', 'a', 'r', ';'
};
static PRUnichar const VALUE_1867[] = {
  0x2606
};
static PRUnichar const NAME_1868[] = {
  's', 't', 'a', 'r', 'f', ';'
};
static PRUnichar const VALUE_1868[] = {
  0x2605
};
static PRUnichar const NAME_1869[] = {
  's', 't', 'r', 'a', 'i', 'g', 'h', 't', 'e', 'p', 's', 'i', 'l', 'o', 'n', ';'
};
static PRUnichar const VALUE_1869[] = {
  0x03f5
};
static PRUnichar const NAME_1870[] = {
  's', 't', 'r', 'a', 'i', 'g', 'h', 't', 'p', 'h', 'i', ';'
};
static PRUnichar const VALUE_1870[] = {
  0x03d5
};
static PRUnichar const NAME_1871[] = {
  's', 't', 'r', 'n', 's', ';'
};
static PRUnichar const VALUE_1871[] = {
  0x00af
};
static PRUnichar const NAME_1872[] = {
  's', 'u', 'b', ';'
};
static PRUnichar const VALUE_1872[] = {
  0x2282
};
static PRUnichar const NAME_1873[] = {
  's', 'u', 'b', 'E', ';'
};
static PRUnichar const VALUE_1873[] = {
  0x2ac5
};
static PRUnichar const NAME_1874[] = {
  's', 'u', 'b', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_1874[] = {
  0x2abd
};
static PRUnichar const NAME_1875[] = {
  's', 'u', 'b', 'e', ';'
};
static PRUnichar const VALUE_1875[] = {
  0x2286
};
static PRUnichar const NAME_1876[] = {
  's', 'u', 'b', 'e', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_1876[] = {
  0x2ac3
};
static PRUnichar const NAME_1877[] = {
  's', 'u', 'b', 'm', 'u', 'l', 't', ';'
};
static PRUnichar const VALUE_1877[] = {
  0x2ac1
};
static PRUnichar const NAME_1878[] = {
  's', 'u', 'b', 'n', 'E', ';'
};
static PRUnichar const VALUE_1878[] = {
  0x2acb
};
static PRUnichar const NAME_1879[] = {
  's', 'u', 'b', 'n', 'e', ';'
};
static PRUnichar const VALUE_1879[] = {
  0x228a
};
static PRUnichar const NAME_1880[] = {
  's', 'u', 'b', 'p', 'l', 'u', 's', ';'
};
static PRUnichar const VALUE_1880[] = {
  0x2abf
};
static PRUnichar const NAME_1881[] = {
  's', 'u', 'b', 'r', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1881[] = {
  0x2979
};
static PRUnichar const NAME_1882[] = {
  's', 'u', 'b', 's', 'e', 't', ';'
};
static PRUnichar const VALUE_1882[] = {
  0x2282
};
static PRUnichar const NAME_1883[] = {
  's', 'u', 'b', 's', 'e', 't', 'e', 'q', ';'
};
static PRUnichar const VALUE_1883[] = {
  0x2286
};
static PRUnichar const NAME_1884[] = {
  's', 'u', 'b', 's', 'e', 't', 'e', 'q', 'q', ';'
};
static PRUnichar const VALUE_1884[] = {
  0x2ac5
};
static PRUnichar const NAME_1885[] = {
  's', 'u', 'b', 's', 'e', 't', 'n', 'e', 'q', ';'
};
static PRUnichar const VALUE_1885[] = {
  0x228a
};
static PRUnichar const NAME_1886[] = {
  's', 'u', 'b', 's', 'e', 't', 'n', 'e', 'q', 'q', ';'
};
static PRUnichar const VALUE_1886[] = {
  0x2acb
};
static PRUnichar const NAME_1887[] = {
  's', 'u', 'b', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1887[] = {
  0x2ac7
};
static PRUnichar const NAME_1888[] = {
  's', 'u', 'b', 's', 'u', 'b', ';'
};
static PRUnichar const VALUE_1888[] = {
  0x2ad5
};
static PRUnichar const NAME_1889[] = {
  's', 'u', 'b', 's', 'u', 'p', ';'
};
static PRUnichar const VALUE_1889[] = {
  0x2ad3
};
static PRUnichar const NAME_1890[] = {
  's', 'u', 'c', 'c', ';'
};
static PRUnichar const VALUE_1890[] = {
  0x227b
};
static PRUnichar const NAME_1891[] = {
  's', 'u', 'c', 'c', 'a', 'p', 'p', 'r', 'o', 'x', ';'
};
static PRUnichar const VALUE_1891[] = {
  0x2ab8
};
static PRUnichar const NAME_1892[] = {
  's', 'u', 'c', 'c', 'c', 'u', 'r', 'l', 'y', 'e', 'q', ';'
};
static PRUnichar const VALUE_1892[] = {
  0x227d
};
static PRUnichar const NAME_1893[] = {
  's', 'u', 'c', 'c', 'e', 'q', ';'
};
static PRUnichar const VALUE_1893[] = {
  0x2ab0
};
static PRUnichar const NAME_1894[] = {
  's', 'u', 'c', 'c', 'n', 'a', 'p', 'p', 'r', 'o', 'x', ';'
};
static PRUnichar const VALUE_1894[] = {
  0x2aba
};
static PRUnichar const NAME_1895[] = {
  's', 'u', 'c', 'c', 'n', 'e', 'q', 'q', ';'
};
static PRUnichar const VALUE_1895[] = {
  0x2ab6
};
static PRUnichar const NAME_1896[] = {
  's', 'u', 'c', 'c', 'n', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1896[] = {
  0x22e9
};
static PRUnichar const NAME_1897[] = {
  's', 'u', 'c', 'c', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1897[] = {
  0x227f
};
static PRUnichar const NAME_1898[] = {
  's', 'u', 'm', ';'
};
static PRUnichar const VALUE_1898[] = {
  0x2211
};
static PRUnichar const NAME_1899[] = {
  's', 'u', 'n', 'g', ';'
};
static PRUnichar const VALUE_1899[] = {
  0x266a
};
static PRUnichar const NAME_1900[] = {
  's', 'u', 'p', '1'
};
static PRUnichar const VALUE_1900[] = {
  0x00b9
};
static PRUnichar const NAME_1901[] = {
  's', 'u', 'p', '1', ';'
};
static PRUnichar const VALUE_1901[] = {
  0x00b9
};
static PRUnichar const NAME_1902[] = {
  's', 'u', 'p', '2'
};
static PRUnichar const VALUE_1902[] = {
  0x00b2
};
static PRUnichar const NAME_1903[] = {
  's', 'u', 'p', '2', ';'
};
static PRUnichar const VALUE_1903[] = {
  0x00b2
};
static PRUnichar const NAME_1904[] = {
  's', 'u', 'p', '3'
};
static PRUnichar const VALUE_1904[] = {
  0x00b3
};
static PRUnichar const NAME_1905[] = {
  's', 'u', 'p', '3', ';'
};
static PRUnichar const VALUE_1905[] = {
  0x00b3
};
static PRUnichar const NAME_1906[] = {
  's', 'u', 'p', ';'
};
static PRUnichar const VALUE_1906[] = {
  0x2283
};
static PRUnichar const NAME_1907[] = {
  's', 'u', 'p', 'E', ';'
};
static PRUnichar const VALUE_1907[] = {
  0x2ac6
};
static PRUnichar const NAME_1908[] = {
  's', 'u', 'p', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_1908[] = {
  0x2abe
};
static PRUnichar const NAME_1909[] = {
  's', 'u', 'p', 'd', 's', 'u', 'b', ';'
};
static PRUnichar const VALUE_1909[] = {
  0x2ad8
};
static PRUnichar const NAME_1910[] = {
  's', 'u', 'p', 'e', ';'
};
static PRUnichar const VALUE_1910[] = {
  0x2287
};
static PRUnichar const NAME_1911[] = {
  's', 'u', 'p', 'e', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_1911[] = {
  0x2ac4
};
static PRUnichar const NAME_1912[] = {
  's', 'u', 'p', 'h', 's', 'u', 'b', ';'
};
static PRUnichar const VALUE_1912[] = {
  0x2ad7
};
static PRUnichar const NAME_1913[] = {
  's', 'u', 'p', 'l', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1913[] = {
  0x297b
};
static PRUnichar const NAME_1914[] = {
  's', 'u', 'p', 'm', 'u', 'l', 't', ';'
};
static PRUnichar const VALUE_1914[] = {
  0x2ac2
};
static PRUnichar const NAME_1915[] = {
  's', 'u', 'p', 'n', 'E', ';'
};
static PRUnichar const VALUE_1915[] = {
  0x2acc
};
static PRUnichar const NAME_1916[] = {
  's', 'u', 'p', 'n', 'e', ';'
};
static PRUnichar const VALUE_1916[] = {
  0x228b
};
static PRUnichar const NAME_1917[] = {
  's', 'u', 'p', 'p', 'l', 'u', 's', ';'
};
static PRUnichar const VALUE_1917[] = {
  0x2ac0
};
static PRUnichar const NAME_1918[] = {
  's', 'u', 'p', 's', 'e', 't', ';'
};
static PRUnichar const VALUE_1918[] = {
  0x2283
};
static PRUnichar const NAME_1919[] = {
  's', 'u', 'p', 's', 'e', 't', 'e', 'q', ';'
};
static PRUnichar const VALUE_1919[] = {
  0x2287
};
static PRUnichar const NAME_1920[] = {
  's', 'u', 'p', 's', 'e', 't', 'e', 'q', 'q', ';'
};
static PRUnichar const VALUE_1920[] = {
  0x2ac6
};
static PRUnichar const NAME_1921[] = {
  's', 'u', 'p', 's', 'e', 't', 'n', 'e', 'q', ';'
};
static PRUnichar const VALUE_1921[] = {
  0x228b
};
static PRUnichar const NAME_1922[] = {
  's', 'u', 'p', 's', 'e', 't', 'n', 'e', 'q', 'q', ';'
};
static PRUnichar const VALUE_1922[] = {
  0x2acc
};
static PRUnichar const NAME_1923[] = {
  's', 'u', 'p', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1923[] = {
  0x2ac8
};
static PRUnichar const NAME_1924[] = {
  's', 'u', 'p', 's', 'u', 'b', ';'
};
static PRUnichar const VALUE_1924[] = {
  0x2ad4
};
static PRUnichar const NAME_1925[] = {
  's', 'u', 'p', 's', 'u', 'p', ';'
};
static PRUnichar const VALUE_1925[] = {
  0x2ad6
};
static PRUnichar const NAME_1926[] = {
  's', 'w', 'A', 'r', 'r', ';'
};
static PRUnichar const VALUE_1926[] = {
  0x21d9
};
static PRUnichar const NAME_1927[] = {
  's', 'w', 'a', 'r', 'h', 'k', ';'
};
static PRUnichar const VALUE_1927[] = {
  0x2926
};
static PRUnichar const NAME_1928[] = {
  's', 'w', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1928[] = {
  0x2199
};
static PRUnichar const NAME_1929[] = {
  's', 'w', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_1929[] = {
  0x2199
};
static PRUnichar const NAME_1930[] = {
  's', 'w', 'n', 'w', 'a', 'r', ';'
};
static PRUnichar const VALUE_1930[] = {
  0x292a
};
static PRUnichar const NAME_1931[] = {
  's', 'z', 'l', 'i', 'g'
};
static PRUnichar const VALUE_1931[] = {
  0x00df
};
static PRUnichar const NAME_1932[] = {
  's', 'z', 'l', 'i', 'g', ';'
};
static PRUnichar const VALUE_1932[] = {
  0x00df
};
static PRUnichar const NAME_1933[] = {
  't', 'a', 'r', 'g', 'e', 't', ';'
};
static PRUnichar const VALUE_1933[] = {
  0x2316
};
static PRUnichar const NAME_1934[] = {
  't', 'a', 'u', ';'
};
static PRUnichar const VALUE_1934[] = {
  0x03c4
};
static PRUnichar const NAME_1935[] = {
  't', 'b', 'r', 'k', ';'
};
static PRUnichar const VALUE_1935[] = {
  0x23b4
};
static PRUnichar const NAME_1936[] = {
  't', 'c', 'a', 'r', 'o', 'n', ';'
};
static PRUnichar const VALUE_1936[] = {
  0x0165
};
static PRUnichar const NAME_1937[] = {
  't', 'c', 'e', 'd', 'i', 'l', ';'
};
static PRUnichar const VALUE_1937[] = {
  0x0163
};
static PRUnichar const NAME_1938[] = {
  't', 'c', 'y', ';'
};
static PRUnichar const VALUE_1938[] = {
  0x0442
};
static PRUnichar const NAME_1939[] = {
  't', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_1939[] = {
  0x20db
};
static PRUnichar const NAME_1940[] = {
  't', 'e', 'l', 'r', 'e', 'c', ';'
};
static PRUnichar const VALUE_1940[] = {
  0x2315
};
static PRUnichar const NAME_1941[] = {
  't', 'f', 'r', ';'
};
static PRUnichar const VALUE_1941[] = {
  0xd835, 0xdd31
};
static PRUnichar const NAME_1942[] = {
  't', 'h', 'e', 'r', 'e', '4', ';'
};
static PRUnichar const VALUE_1942[] = {
  0x2234
};
static PRUnichar const NAME_1943[] = {
  't', 'h', 'e', 'r', 'e', 'f', 'o', 'r', 'e', ';'
};
static PRUnichar const VALUE_1943[] = {
  0x2234
};
static PRUnichar const NAME_1944[] = {
  't', 'h', 'e', 't', 'a', ';'
};
static PRUnichar const VALUE_1944[] = {
  0x03b8
};
static PRUnichar const NAME_1945[] = {
  't', 'h', 'e', 't', 'a', 's', 'y', 'm', ';'
};
static PRUnichar const VALUE_1945[] = {
  0x03d1
};
static PRUnichar const NAME_1946[] = {
  't', 'h', 'e', 't', 'a', 'v', ';'
};
static PRUnichar const VALUE_1946[] = {
  0x03d1
};
static PRUnichar const NAME_1947[] = {
  't', 'h', 'i', 'c', 'k', 'a', 'p', 'p', 'r', 'o', 'x', ';'
};
static PRUnichar const VALUE_1947[] = {
  0x2248
};
static PRUnichar const NAME_1948[] = {
  't', 'h', 'i', 'c', 'k', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1948[] = {
  0x223c
};
static PRUnichar const NAME_1949[] = {
  't', 'h', 'i', 'n', 's', 'p', ';'
};
static PRUnichar const VALUE_1949[] = {
  0x2009
};
static PRUnichar const NAME_1950[] = {
  't', 'h', 'k', 'a', 'p', ';'
};
static PRUnichar const VALUE_1950[] = {
  0x2248
};
static PRUnichar const NAME_1951[] = {
  't', 'h', 'k', 's', 'i', 'm', ';'
};
static PRUnichar const VALUE_1951[] = {
  0x223c
};
static PRUnichar const NAME_1952[] = {
  't', 'h', 'o', 'r', 'n'
};
static PRUnichar const VALUE_1952[] = {
  0x00fe
};
static PRUnichar const NAME_1953[] = {
  't', 'h', 'o', 'r', 'n', ';'
};
static PRUnichar const VALUE_1953[] = {
  0x00fe
};
static PRUnichar const NAME_1954[] = {
  't', 'i', 'l', 'd', 'e', ';'
};
static PRUnichar const VALUE_1954[] = {
  0x02dc
};
static PRUnichar const NAME_1955[] = {
  't', 'i', 'm', 'e', 's'
};
static PRUnichar const VALUE_1955[] = {
  0x00d7
};
static PRUnichar const NAME_1956[] = {
  't', 'i', 'm', 'e', 's', ';'
};
static PRUnichar const VALUE_1956[] = {
  0x00d7
};
static PRUnichar const NAME_1957[] = {
  't', 'i', 'm', 'e', 's', 'b', ';'
};
static PRUnichar const VALUE_1957[] = {
  0x22a0
};
static PRUnichar const NAME_1958[] = {
  't', 'i', 'm', 'e', 's', 'b', 'a', 'r', ';'
};
static PRUnichar const VALUE_1958[] = {
  0x2a31
};
static PRUnichar const NAME_1959[] = {
  't', 'i', 'm', 'e', 's', 'd', ';'
};
static PRUnichar const VALUE_1959[] = {
  0x2a30
};
static PRUnichar const NAME_1960[] = {
  't', 'i', 'n', 't', ';'
};
static PRUnichar const VALUE_1960[] = {
  0x222d
};
static PRUnichar const NAME_1961[] = {
  't', 'o', 'e', 'a', ';'
};
static PRUnichar const VALUE_1961[] = {
  0x2928
};
static PRUnichar const NAME_1962[] = {
  't', 'o', 'p', ';'
};
static PRUnichar const VALUE_1962[] = {
  0x22a4
};
static PRUnichar const NAME_1963[] = {
  't', 'o', 'p', 'b', 'o', 't', ';'
};
static PRUnichar const VALUE_1963[] = {
  0x2336
};
static PRUnichar const NAME_1964[] = {
  't', 'o', 'p', 'c', 'i', 'r', ';'
};
static PRUnichar const VALUE_1964[] = {
  0x2af1
};
static PRUnichar const NAME_1965[] = {
  't', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_1965[] = {
  0xd835, 0xdd65
};
static PRUnichar const NAME_1966[] = {
  't', 'o', 'p', 'f', 'o', 'r', 'k', ';'
};
static PRUnichar const VALUE_1966[] = {
  0x2ada
};
static PRUnichar const NAME_1967[] = {
  't', 'o', 's', 'a', ';'
};
static PRUnichar const VALUE_1967[] = {
  0x2929
};
static PRUnichar const NAME_1968[] = {
  't', 'p', 'r', 'i', 'm', 'e', ';'
};
static PRUnichar const VALUE_1968[] = {
  0x2034
};
static PRUnichar const NAME_1969[] = {
  't', 'r', 'a', 'd', 'e', ';'
};
static PRUnichar const VALUE_1969[] = {
  0x2122
};
static PRUnichar const NAME_1970[] = {
  't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', ';'
};
static PRUnichar const VALUE_1970[] = {
  0x25b5
};
static PRUnichar const NAME_1971[] = {
  't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'd', 'o', 'w', 'n', ';'
};
static PRUnichar const VALUE_1971[] = {
  0x25bf
};
static PRUnichar const NAME_1972[] = {
  't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'l', 'e', 'f', 't', ';'
};
static PRUnichar const VALUE_1972[] = {
  0x25c3
};
static PRUnichar const NAME_1973[] = {
  't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'l', 'e', 'f', 't', 'e', 'q', ';'
};
static PRUnichar const VALUE_1973[] = {
  0x22b4
};
static PRUnichar const NAME_1974[] = {
  't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'q', ';'
};
static PRUnichar const VALUE_1974[] = {
  0x225c
};
static PRUnichar const NAME_1975[] = {
  't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'r', 'i', 'g', 'h', 't', ';'
};
static PRUnichar const VALUE_1975[] = {
  0x25b9
};
static PRUnichar const NAME_1976[] = {
  't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'r', 'i', 'g', 'h', 't', 'e', 'q', ';'
};
static PRUnichar const VALUE_1976[] = {
  0x22b5
};
static PRUnichar const NAME_1977[] = {
  't', 'r', 'i', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_1977[] = {
  0x25ec
};
static PRUnichar const NAME_1978[] = {
  't', 'r', 'i', 'e', ';'
};
static PRUnichar const VALUE_1978[] = {
  0x225c
};
static PRUnichar const NAME_1979[] = {
  't', 'r', 'i', 'm', 'i', 'n', 'u', 's', ';'
};
static PRUnichar const VALUE_1979[] = {
  0x2a3a
};
static PRUnichar const NAME_1980[] = {
  't', 'r', 'i', 'p', 'l', 'u', 's', ';'
};
static PRUnichar const VALUE_1980[] = {
  0x2a39
};
static PRUnichar const NAME_1981[] = {
  't', 'r', 'i', 's', 'b', ';'
};
static PRUnichar const VALUE_1981[] = {
  0x29cd
};
static PRUnichar const NAME_1982[] = {
  't', 'r', 'i', 't', 'i', 'm', 'e', ';'
};
static PRUnichar const VALUE_1982[] = {
  0x2a3b
};
static PRUnichar const NAME_1983[] = {
  't', 'r', 'p', 'e', 'z', 'i', 'u', 'm', ';'
};
static PRUnichar const VALUE_1983[] = {
  0x23e2
};
static PRUnichar const NAME_1984[] = {
  't', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_1984[] = {
  0xd835, 0xdcc9
};
static PRUnichar const NAME_1985[] = {
  't', 's', 'c', 'y', ';'
};
static PRUnichar const VALUE_1985[] = {
  0x0446
};
static PRUnichar const NAME_1986[] = {
  't', 's', 'h', 'c', 'y', ';'
};
static PRUnichar const VALUE_1986[] = {
  0x045b
};
static PRUnichar const NAME_1987[] = {
  't', 's', 't', 'r', 'o', 'k', ';'
};
static PRUnichar const VALUE_1987[] = {
  0x0167
};
static PRUnichar const NAME_1988[] = {
  't', 'w', 'i', 'x', 't', ';'
};
static PRUnichar const VALUE_1988[] = {
  0x226c
};
static PRUnichar const NAME_1989[] = {
  't', 'w', 'o', 'h', 'e', 'a', 'd', 'l', 'e', 'f', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_1989[] = {
  0x219e
};
static PRUnichar const NAME_1990[] = {
  't', 'w', 'o', 'h', 'e', 'a', 'd', 'r', 'i', 'g', 'h', 't', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_1990[] = {
  0x21a0
};
static PRUnichar const NAME_1991[] = {
  'u', 'A', 'r', 'r', ';'
};
static PRUnichar const VALUE_1991[] = {
  0x21d1
};
static PRUnichar const NAME_1992[] = {
  'u', 'H', 'a', 'r', ';'
};
static PRUnichar const VALUE_1992[] = {
  0x2963
};
static PRUnichar const NAME_1993[] = {
  'u', 'a', 'c', 'u', 't', 'e'
};
static PRUnichar const VALUE_1993[] = {
  0x00fa
};
static PRUnichar const NAME_1994[] = {
  'u', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_1994[] = {
  0x00fa
};
static PRUnichar const NAME_1995[] = {
  'u', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_1995[] = {
  0x2191
};
static PRUnichar const NAME_1996[] = {
  'u', 'b', 'r', 'c', 'y', ';'
};
static PRUnichar const VALUE_1996[] = {
  0x045e
};
static PRUnichar const NAME_1997[] = {
  'u', 'b', 'r', 'e', 'v', 'e', ';'
};
static PRUnichar const VALUE_1997[] = {
  0x016d
};
static PRUnichar const NAME_1998[] = {
  'u', 'c', 'i', 'r', 'c'
};
static PRUnichar const VALUE_1998[] = {
  0x00fb
};
static PRUnichar const NAME_1999[] = {
  'u', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_1999[] = {
  0x00fb
};
static PRUnichar const NAME_2000[] = {
  'u', 'c', 'y', ';'
};
static PRUnichar const VALUE_2000[] = {
  0x0443
};
static PRUnichar const NAME_2001[] = {
  'u', 'd', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_2001[] = {
  0x21c5
};
static PRUnichar const NAME_2002[] = {
  'u', 'd', 'b', 'l', 'a', 'c', ';'
};
static PRUnichar const VALUE_2002[] = {
  0x0171
};
static PRUnichar const NAME_2003[] = {
  'u', 'd', 'h', 'a', 'r', ';'
};
static PRUnichar const VALUE_2003[] = {
  0x296e
};
static PRUnichar const NAME_2004[] = {
  'u', 'f', 'i', 's', 'h', 't', ';'
};
static PRUnichar const VALUE_2004[] = {
  0x297e
};
static PRUnichar const NAME_2005[] = {
  'u', 'f', 'r', ';'
};
static PRUnichar const VALUE_2005[] = {
  0xd835, 0xdd32
};
static PRUnichar const NAME_2006[] = {
  'u', 'g', 'r', 'a', 'v', 'e'
};
static PRUnichar const VALUE_2006[] = {
  0x00f9
};
static PRUnichar const NAME_2007[] = {
  'u', 'g', 'r', 'a', 'v', 'e', ';'
};
static PRUnichar const VALUE_2007[] = {
  0x00f9
};
static PRUnichar const NAME_2008[] = {
  'u', 'h', 'a', 'r', 'l', ';'
};
static PRUnichar const VALUE_2008[] = {
  0x21bf
};
static PRUnichar const NAME_2009[] = {
  'u', 'h', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_2009[] = {
  0x21be
};
static PRUnichar const NAME_2010[] = {
  'u', 'h', 'b', 'l', 'k', ';'
};
static PRUnichar const VALUE_2010[] = {
  0x2580
};
static PRUnichar const NAME_2011[] = {
  'u', 'l', 'c', 'o', 'r', 'n', ';'
};
static PRUnichar const VALUE_2011[] = {
  0x231c
};
static PRUnichar const NAME_2012[] = {
  'u', 'l', 'c', 'o', 'r', 'n', 'e', 'r', ';'
};
static PRUnichar const VALUE_2012[] = {
  0x231c
};
static PRUnichar const NAME_2013[] = {
  'u', 'l', 'c', 'r', 'o', 'p', ';'
};
static PRUnichar const VALUE_2013[] = {
  0x230f
};
static PRUnichar const NAME_2014[] = {
  'u', 'l', 't', 'r', 'i', ';'
};
static PRUnichar const VALUE_2014[] = {
  0x25f8
};
static PRUnichar const NAME_2015[] = {
  'u', 'm', 'a', 'c', 'r', ';'
};
static PRUnichar const VALUE_2015[] = {
  0x016b
};
static PRUnichar const NAME_2016[] = {
  'u', 'm', 'l'
};
static PRUnichar const VALUE_2016[] = {
  0x00a8
};
static PRUnichar const NAME_2017[] = {
  'u', 'm', 'l', ';'
};
static PRUnichar const VALUE_2017[] = {
  0x00a8
};
static PRUnichar const NAME_2018[] = {
  'u', 'o', 'g', 'o', 'n', ';'
};
static PRUnichar const VALUE_2018[] = {
  0x0173
};
static PRUnichar const NAME_2019[] = {
  'u', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_2019[] = {
  0xd835, 0xdd66
};
static PRUnichar const NAME_2020[] = {
  'u', 'p', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_2020[] = {
  0x2191
};
static PRUnichar const NAME_2021[] = {
  'u', 'p', 'd', 'o', 'w', 'n', 'a', 'r', 'r', 'o', 'w', ';'
};
static PRUnichar const VALUE_2021[] = {
  0x2195
};
static PRUnichar const NAME_2022[] = {
  'u', 'p', 'h', 'a', 'r', 'p', 'o', 'o', 'n', 'l', 'e', 'f', 't', ';'
};
static PRUnichar const VALUE_2022[] = {
  0x21bf
};
static PRUnichar const NAME_2023[] = {
  'u', 'p', 'h', 'a', 'r', 'p', 'o', 'o', 'n', 'r', 'i', 'g', 'h', 't', ';'
};
static PRUnichar const VALUE_2023[] = {
  0x21be
};
static PRUnichar const NAME_2024[] = {
  'u', 'p', 'l', 'u', 's', ';'
};
static PRUnichar const VALUE_2024[] = {
  0x228e
};
static PRUnichar const NAME_2025[] = {
  'u', 'p', 's', 'i', ';'
};
static PRUnichar const VALUE_2025[] = {
  0x03c5
};
static PRUnichar const NAME_2026[] = {
  'u', 'p', 's', 'i', 'h', ';'
};
static PRUnichar const VALUE_2026[] = {
  0x03d2
};
static PRUnichar const NAME_2027[] = {
  'u', 'p', 's', 'i', 'l', 'o', 'n', ';'
};
static PRUnichar const VALUE_2027[] = {
  0x03c5
};
static PRUnichar const NAME_2028[] = {
  'u', 'p', 'u', 'p', 'a', 'r', 'r', 'o', 'w', 's', ';'
};
static PRUnichar const VALUE_2028[] = {
  0x21c8
};
static PRUnichar const NAME_2029[] = {
  'u', 'r', 'c', 'o', 'r', 'n', ';'
};
static PRUnichar const VALUE_2029[] = {
  0x231d
};
static PRUnichar const NAME_2030[] = {
  'u', 'r', 'c', 'o', 'r', 'n', 'e', 'r', ';'
};
static PRUnichar const VALUE_2030[] = {
  0x231d
};
static PRUnichar const NAME_2031[] = {
  'u', 'r', 'c', 'r', 'o', 'p', ';'
};
static PRUnichar const VALUE_2031[] = {
  0x230e
};
static PRUnichar const NAME_2032[] = {
  'u', 'r', 'i', 'n', 'g', ';'
};
static PRUnichar const VALUE_2032[] = {
  0x016f
};
static PRUnichar const NAME_2033[] = {
  'u', 'r', 't', 'r', 'i', ';'
};
static PRUnichar const VALUE_2033[] = {
  0x25f9
};
static PRUnichar const NAME_2034[] = {
  'u', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_2034[] = {
  0xd835, 0xdcca
};
static PRUnichar const NAME_2035[] = {
  'u', 't', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_2035[] = {
  0x22f0
};
static PRUnichar const NAME_2036[] = {
  'u', 't', 'i', 'l', 'd', 'e', ';'
};
static PRUnichar const VALUE_2036[] = {
  0x0169
};
static PRUnichar const NAME_2037[] = {
  'u', 't', 'r', 'i', ';'
};
static PRUnichar const VALUE_2037[] = {
  0x25b5
};
static PRUnichar const NAME_2038[] = {
  'u', 't', 'r', 'i', 'f', ';'
};
static PRUnichar const VALUE_2038[] = {
  0x25b4
};
static PRUnichar const NAME_2039[] = {
  'u', 'u', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_2039[] = {
  0x21c8
};
static PRUnichar const NAME_2040[] = {
  'u', 'u', 'm', 'l'
};
static PRUnichar const VALUE_2040[] = {
  0x00fc
};
static PRUnichar const NAME_2041[] = {
  'u', 'u', 'm', 'l', ';'
};
static PRUnichar const VALUE_2041[] = {
  0x00fc
};
static PRUnichar const NAME_2042[] = {
  'u', 'w', 'a', 'n', 'g', 'l', 'e', ';'
};
static PRUnichar const VALUE_2042[] = {
  0x29a7
};
static PRUnichar const NAME_2043[] = {
  'v', 'A', 'r', 'r', ';'
};
static PRUnichar const VALUE_2043[] = {
  0x21d5
};
static PRUnichar const NAME_2044[] = {
  'v', 'B', 'a', 'r', ';'
};
static PRUnichar const VALUE_2044[] = {
  0x2ae8
};
static PRUnichar const NAME_2045[] = {
  'v', 'B', 'a', 'r', 'v', ';'
};
static PRUnichar const VALUE_2045[] = {
  0x2ae9
};
static PRUnichar const NAME_2046[] = {
  'v', 'D', 'a', 's', 'h', ';'
};
static PRUnichar const VALUE_2046[] = {
  0x22a8
};
static PRUnichar const NAME_2047[] = {
  'v', 'a', 'n', 'g', 'r', 't', ';'
};
static PRUnichar const VALUE_2047[] = {
  0x299c
};
static PRUnichar const NAME_2048[] = {
  'v', 'a', 'r', 'e', 'p', 's', 'i', 'l', 'o', 'n', ';'
};
static PRUnichar const VALUE_2048[] = {
  0x03b5
};
static PRUnichar const NAME_2049[] = {
  'v', 'a', 'r', 'k', 'a', 'p', 'p', 'a', ';'
};
static PRUnichar const VALUE_2049[] = {
  0x03f0
};
static PRUnichar const NAME_2050[] = {
  'v', 'a', 'r', 'n', 'o', 't', 'h', 'i', 'n', 'g', ';'
};
static PRUnichar const VALUE_2050[] = {
  0x2205
};
static PRUnichar const NAME_2051[] = {
  'v', 'a', 'r', 'p', 'h', 'i', ';'
};
static PRUnichar const VALUE_2051[] = {
  0x03c6
};
static PRUnichar const NAME_2052[] = {
  'v', 'a', 'r', 'p', 'i', ';'
};
static PRUnichar const VALUE_2052[] = {
  0x03d6
};
static PRUnichar const NAME_2053[] = {
  'v', 'a', 'r', 'p', 'r', 'o', 'p', 't', 'o', ';'
};
static PRUnichar const VALUE_2053[] = {
  0x221d
};
static PRUnichar const NAME_2054[] = {
  'v', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_2054[] = {
  0x2195
};
static PRUnichar const NAME_2055[] = {
  'v', 'a', 'r', 'r', 'h', 'o', ';'
};
static PRUnichar const VALUE_2055[] = {
  0x03f1
};
static PRUnichar const NAME_2056[] = {
  'v', 'a', 'r', 's', 'i', 'g', 'm', 'a', ';'
};
static PRUnichar const VALUE_2056[] = {
  0x03c2
};
static PRUnichar const NAME_2057[] = {
  'v', 'a', 'r', 't', 'h', 'e', 't', 'a', ';'
};
static PRUnichar const VALUE_2057[] = {
  0x03d1
};
static PRUnichar const NAME_2058[] = {
  'v', 'a', 'r', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'l', 'e', 'f', 't', ';'
};
static PRUnichar const VALUE_2058[] = {
  0x22b2
};
static PRUnichar const NAME_2059[] = {
  'v', 'a', 'r', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', 'r', 'i', 'g', 'h', 't', ';'
};
static PRUnichar const VALUE_2059[] = {
  0x22b3
};
static PRUnichar const NAME_2060[] = {
  'v', 'c', 'y', ';'
};
static PRUnichar const VALUE_2060[] = {
  0x0432
};
static PRUnichar const NAME_2061[] = {
  'v', 'd', 'a', 's', 'h', ';'
};
static PRUnichar const VALUE_2061[] = {
  0x22a2
};
static PRUnichar const NAME_2062[] = {
  'v', 'e', 'e', ';'
};
static PRUnichar const VALUE_2062[] = {
  0x2228
};
static PRUnichar const NAME_2063[] = {
  'v', 'e', 'e', 'b', 'a', 'r', ';'
};
static PRUnichar const VALUE_2063[] = {
  0x22bb
};
static PRUnichar const NAME_2064[] = {
  'v', 'e', 'e', 'e', 'q', ';'
};
static PRUnichar const VALUE_2064[] = {
  0x225a
};
static PRUnichar const NAME_2065[] = {
  'v', 'e', 'l', 'l', 'i', 'p', ';'
};
static PRUnichar const VALUE_2065[] = {
  0x22ee
};
static PRUnichar const NAME_2066[] = {
  'v', 'e', 'r', 'b', 'a', 'r', ';'
};
static PRUnichar const VALUE_2066[] = {
  0x007c
};
static PRUnichar const NAME_2067[] = {
  'v', 'e', 'r', 't', ';'
};
static PRUnichar const VALUE_2067[] = {
  0x007c
};
static PRUnichar const NAME_2068[] = {
  'v', 'f', 'r', ';'
};
static PRUnichar const VALUE_2068[] = {
  0xd835, 0xdd33
};
static PRUnichar const NAME_2069[] = {
  'v', 'l', 't', 'r', 'i', ';'
};
static PRUnichar const VALUE_2069[] = {
  0x22b2
};
static PRUnichar const NAME_2070[] = {
  'v', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_2070[] = {
  0xd835, 0xdd67
};
static PRUnichar const NAME_2071[] = {
  'v', 'p', 'r', 'o', 'p', ';'
};
static PRUnichar const VALUE_2071[] = {
  0x221d
};
static PRUnichar const NAME_2072[] = {
  'v', 'r', 't', 'r', 'i', ';'
};
static PRUnichar const VALUE_2072[] = {
  0x22b3
};
static PRUnichar const NAME_2073[] = {
  'v', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_2073[] = {
  0xd835, 0xdccb
};
static PRUnichar const NAME_2074[] = {
  'v', 'z', 'i', 'g', 'z', 'a', 'g', ';'
};
static PRUnichar const VALUE_2074[] = {
  0x299a
};
static PRUnichar const NAME_2075[] = {
  'w', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_2075[] = {
  0x0175
};
static PRUnichar const NAME_2076[] = {
  'w', 'e', 'd', 'b', 'a', 'r', ';'
};
static PRUnichar const VALUE_2076[] = {
  0x2a5f
};
static PRUnichar const NAME_2077[] = {
  'w', 'e', 'd', 'g', 'e', ';'
};
static PRUnichar const VALUE_2077[] = {
  0x2227
};
static PRUnichar const NAME_2078[] = {
  'w', 'e', 'd', 'g', 'e', 'q', ';'
};
static PRUnichar const VALUE_2078[] = {
  0x2259
};
static PRUnichar const NAME_2079[] = {
  'w', 'e', 'i', 'e', 'r', 'p', ';'
};
static PRUnichar const VALUE_2079[] = {
  0x2118
};
static PRUnichar const NAME_2080[] = {
  'w', 'f', 'r', ';'
};
static PRUnichar const VALUE_2080[] = {
  0xd835, 0xdd34
};
static PRUnichar const NAME_2081[] = {
  'w', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_2081[] = {
  0xd835, 0xdd68
};
static PRUnichar const NAME_2082[] = {
  'w', 'p', ';'
};
static PRUnichar const VALUE_2082[] = {
  0x2118
};
static PRUnichar const NAME_2083[] = {
  'w', 'r', ';'
};
static PRUnichar const VALUE_2083[] = {
  0x2240
};
static PRUnichar const NAME_2084[] = {
  'w', 'r', 'e', 'a', 't', 'h', ';'
};
static PRUnichar const VALUE_2084[] = {
  0x2240
};
static PRUnichar const NAME_2085[] = {
  'w', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_2085[] = {
  0xd835, 0xdccc
};
static PRUnichar const NAME_2086[] = {
  'x', 'c', 'a', 'p', ';'
};
static PRUnichar const VALUE_2086[] = {
  0x22c2
};
static PRUnichar const NAME_2087[] = {
  'x', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_2087[] = {
  0x25ef
};
static PRUnichar const NAME_2088[] = {
  'x', 'c', 'u', 'p', ';'
};
static PRUnichar const VALUE_2088[] = {
  0x22c3
};
static PRUnichar const NAME_2089[] = {
  'x', 'd', 't', 'r', 'i', ';'
};
static PRUnichar const VALUE_2089[] = {
  0x25bd
};
static PRUnichar const NAME_2090[] = {
  'x', 'f', 'r', ';'
};
static PRUnichar const VALUE_2090[] = {
  0xd835, 0xdd35
};
static PRUnichar const NAME_2091[] = {
  'x', 'h', 'A', 'r', 'r', ';'
};
static PRUnichar const VALUE_2091[] = {
  0x27fa
};
static PRUnichar const NAME_2092[] = {
  'x', 'h', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_2092[] = {
  0x27f7
};
static PRUnichar const NAME_2093[] = {
  'x', 'i', ';'
};
static PRUnichar const VALUE_2093[] = {
  0x03be
};
static PRUnichar const NAME_2094[] = {
  'x', 'l', 'A', 'r', 'r', ';'
};
static PRUnichar const VALUE_2094[] = {
  0x27f8
};
static PRUnichar const NAME_2095[] = {
  'x', 'l', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_2095[] = {
  0x27f5
};
static PRUnichar const NAME_2096[] = {
  'x', 'm', 'a', 'p', ';'
};
static PRUnichar const VALUE_2096[] = {
  0x27fc
};
static PRUnichar const NAME_2097[] = {
  'x', 'n', 'i', 's', ';'
};
static PRUnichar const VALUE_2097[] = {
  0x22fb
};
static PRUnichar const NAME_2098[] = {
  'x', 'o', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_2098[] = {
  0x2a00
};
static PRUnichar const NAME_2099[] = {
  'x', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_2099[] = {
  0xd835, 0xdd69
};
static PRUnichar const NAME_2100[] = {
  'x', 'o', 'p', 'l', 'u', 's', ';'
};
static PRUnichar const VALUE_2100[] = {
  0x2a01
};
static PRUnichar const NAME_2101[] = {
  'x', 'o', 't', 'i', 'm', 'e', ';'
};
static PRUnichar const VALUE_2101[] = {
  0x2a02
};
static PRUnichar const NAME_2102[] = {
  'x', 'r', 'A', 'r', 'r', ';'
};
static PRUnichar const VALUE_2102[] = {
  0x27f9
};
static PRUnichar const NAME_2103[] = {
  'x', 'r', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_2103[] = {
  0x27f6
};
static PRUnichar const NAME_2104[] = {
  'x', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_2104[] = {
  0xd835, 0xdccd
};
static PRUnichar const NAME_2105[] = {
  'x', 's', 'q', 'c', 'u', 'p', ';'
};
static PRUnichar const VALUE_2105[] = {
  0x2a06
};
static PRUnichar const NAME_2106[] = {
  'x', 'u', 'p', 'l', 'u', 's', ';'
};
static PRUnichar const VALUE_2106[] = {
  0x2a04
};
static PRUnichar const NAME_2107[] = {
  'x', 'u', 't', 'r', 'i', ';'
};
static PRUnichar const VALUE_2107[] = {
  0x25b3
};
static PRUnichar const NAME_2108[] = {
  'x', 'v', 'e', 'e', ';'
};
static PRUnichar const VALUE_2108[] = {
  0x22c1
};
static PRUnichar const NAME_2109[] = {
  'x', 'w', 'e', 'd', 'g', 'e', ';'
};
static PRUnichar const VALUE_2109[] = {
  0x22c0
};
static PRUnichar const NAME_2110[] = {
  'y', 'a', 'c', 'u', 't', 'e'
};
static PRUnichar const VALUE_2110[] = {
  0x00fd
};
static PRUnichar const NAME_2111[] = {
  'y', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_2111[] = {
  0x00fd
};
static PRUnichar const NAME_2112[] = {
  'y', 'a', 'c', 'y', ';'
};
static PRUnichar const VALUE_2112[] = {
  0x044f
};
static PRUnichar const NAME_2113[] = {
  'y', 'c', 'i', 'r', 'c', ';'
};
static PRUnichar const VALUE_2113[] = {
  0x0177
};
static PRUnichar const NAME_2114[] = {
  'y', 'c', 'y', ';'
};
static PRUnichar const VALUE_2114[] = {
  0x044b
};
static PRUnichar const NAME_2115[] = {
  'y', 'e', 'n'
};
static PRUnichar const VALUE_2115[] = {
  0x00a5
};
static PRUnichar const NAME_2116[] = {
  'y', 'e', 'n', ';'
};
static PRUnichar const VALUE_2116[] = {
  0x00a5
};
static PRUnichar const NAME_2117[] = {
  'y', 'f', 'r', ';'
};
static PRUnichar const VALUE_2117[] = {
  0xd835, 0xdd36
};
static PRUnichar const NAME_2118[] = {
  'y', 'i', 'c', 'y', ';'
};
static PRUnichar const VALUE_2118[] = {
  0x0457
};
static PRUnichar const NAME_2119[] = {
  'y', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_2119[] = {
  0xd835, 0xdd6a
};
static PRUnichar const NAME_2120[] = {
  'y', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_2120[] = {
  0xd835, 0xdcce
};
static PRUnichar const NAME_2121[] = {
  'y', 'u', 'c', 'y', ';'
};
static PRUnichar const VALUE_2121[] = {
  0x044e
};
static PRUnichar const NAME_2122[] = {
  'y', 'u', 'm', 'l'
};
static PRUnichar const VALUE_2122[] = {
  0x00ff
};
static PRUnichar const NAME_2123[] = {
  'y', 'u', 'm', 'l', ';'
};
static PRUnichar const VALUE_2123[] = {
  0x00ff
};
static PRUnichar const NAME_2124[] = {
  'z', 'a', 'c', 'u', 't', 'e', ';'
};
static PRUnichar const VALUE_2124[] = {
  0x017a
};
static PRUnichar const NAME_2125[] = {
  'z', 'c', 'a', 'r', 'o', 'n', ';'
};
static PRUnichar const VALUE_2125[] = {
  0x017e
};
static PRUnichar const NAME_2126[] = {
  'z', 'c', 'y', ';'
};
static PRUnichar const VALUE_2126[] = {
  0x0437
};
static PRUnichar const NAME_2127[] = {
  'z', 'd', 'o', 't', ';'
};
static PRUnichar const VALUE_2127[] = {
  0x017c
};
static PRUnichar const NAME_2128[] = {
  'z', 'e', 'e', 't', 'r', 'f', ';'
};
static PRUnichar const VALUE_2128[] = {
  0x2128
};
static PRUnichar const NAME_2129[] = {
  'z', 'e', 't', 'a', ';'
};
static PRUnichar const VALUE_2129[] = {
  0x03b6
};
static PRUnichar const NAME_2130[] = {
  'z', 'f', 'r', ';'
};
static PRUnichar const VALUE_2130[] = {
  0xd835, 0xdd37
};
static PRUnichar const NAME_2131[] = {
  'z', 'h', 'c', 'y', ';'
};
static PRUnichar const VALUE_2131[] = {
  0x0436
};
static PRUnichar const NAME_2132[] = {
  'z', 'i', 'g', 'r', 'a', 'r', 'r', ';'
};
static PRUnichar const VALUE_2132[] = {
  0x21dd
};
static PRUnichar const NAME_2133[] = {
  'z', 'o', 'p', 'f', ';'
};
static PRUnichar const VALUE_2133[] = {
  0xd835, 0xdd6b
};
static PRUnichar const NAME_2134[] = {
  'z', 's', 'c', 'r', ';'
};
static PRUnichar const VALUE_2134[] = {
  0xd835, 0xdccf
};
static PRUnichar const NAME_2135[] = {
  'z', 'w', 'j', ';'
};
static PRUnichar const VALUE_2135[] = {
  0x200d
};
static PRUnichar const NAME_2136[] = {
  'z', 'w', 'n', 'j', ';'
};
static PRUnichar const VALUE_2136[] = {
  0x200c
};
void
nsHtml5NamedCharacters::initializeStatics()
{
  NAMES = jArray<jArray<PRUnichar,PRInt32>,PRInt32>(2137);
  NAMES[0] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_0, 5);
  NAMES[1] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1, 6);
  NAMES[2] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2, 3);
  NAMES[3] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_3, 4);
  NAMES[4] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_4, 6);
  NAMES[5] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_5, 7);
  NAMES[6] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_6, 7);
  NAMES[7] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_7, 5);
  NAMES[8] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_8, 6);
  NAMES[9] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_9, 4);
  NAMES[10] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_10, 4);
  NAMES[11] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_11, 6);
  NAMES[12] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_12, 7);
  NAMES[13] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_13, 6);
  NAMES[14] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_14, 6);
  NAMES[15] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_15, 4);
  NAMES[16] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_16, 6);
  NAMES[17] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_17, 5);
  NAMES[18] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_18, 14);
  NAMES[19] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_19, 5);
  NAMES[20] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_20, 6);
  NAMES[21] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_21, 5);
  NAMES[22] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_22, 7);
  NAMES[23] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_23, 6);
  NAMES[24] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_24, 7);
  NAMES[25] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_25, 4);
  NAMES[26] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_26, 5);
  NAMES[27] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_27, 10);
  NAMES[28] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_28, 5);
  NAMES[29] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_29, 7);
  NAMES[30] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_30, 4);
  NAMES[31] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_31, 8);
  NAMES[32] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_32, 11);
  NAMES[33] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_33, 5);
  NAMES[34] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_34, 4);
  NAMES[35] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_35, 5);
  NAMES[36] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_36, 6);
  NAMES[37] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_37, 5);
  NAMES[38] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_38, 7);
  NAMES[39] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_39, 5);
  NAMES[40] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_40, 4);
  NAMES[41] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_41, 5);
  NAMES[42] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_42, 7);
  NAMES[43] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_43, 4);
  NAMES[44] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_44, 21);
  NAMES[45] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_45, 8);
  NAMES[46] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_46, 7);
  NAMES[47] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_47, 6);
  NAMES[48] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_48, 7);
  NAMES[49] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_49, 6);
  NAMES[50] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_50, 8);
  NAMES[51] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_51, 5);
  NAMES[52] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_52, 8);
  NAMES[53] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_53, 10);
  NAMES[54] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_54, 4);
  NAMES[55] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_55, 4);
  NAMES[56] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_56, 10);
  NAMES[57] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_57, 12);
  NAMES[58] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_58, 11);
  NAMES[59] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_59, 12);
  NAMES[60] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_60, 25);
  NAMES[61] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_61, 22);
  NAMES[62] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_62, 16);
  NAMES[63] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_63, 6);
  NAMES[64] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_64, 7);
  NAMES[65] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_65, 10);
  NAMES[66] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_66, 7);
  NAMES[67] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_67, 16);
  NAMES[68] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_68, 5);
  NAMES[69] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_69, 10);
  NAMES[70] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_70, 32);
  NAMES[71] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_71, 6);
  NAMES[72] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_72, 5);
  NAMES[73] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_73, 4);
  NAMES[74] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_74, 7);
  NAMES[75] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_75, 3);
  NAMES[76] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_76, 9);
  NAMES[77] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_77, 5);
  NAMES[78] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_78, 5);
  NAMES[79] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_79, 5);
  NAMES[80] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_80, 7);
  NAMES[81] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_81, 5);
  NAMES[82] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_82, 6);
  NAMES[83] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_83, 7);
  NAMES[84] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_84, 4);
  NAMES[85] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_85, 4);
  NAMES[86] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_86, 6);
  NAMES[87] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_87, 4);
  NAMES[88] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_88, 17);
  NAMES[89] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_89, 15);
  NAMES[90] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_90, 23);
  NAMES[91] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_91, 17);
  NAMES[92] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_92, 17);
  NAMES[93] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_93, 8);
  NAMES[94] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_94, 14);
  NAMES[95] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_95, 5);
  NAMES[96] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_96, 4);
  NAMES[97] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_97, 7);
  NAMES[98] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_98, 9);
  NAMES[99] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_99, 22);
  NAMES[100] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_100, 10);
  NAMES[101] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_101, 16);
  NAMES[102] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_102, 16);
  NAMES[103] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_103, 21);
  NAMES[104] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_104, 14);
  NAMES[105] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_105, 20);
  NAMES[106] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_106, 25);
  NAMES[107] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_107, 21);
  NAMES[108] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_108, 17);
  NAMES[109] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_109, 15);
  NAMES[110] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_110, 14);
  NAMES[111] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_111, 18);
  NAMES[112] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_112, 18);
  NAMES[113] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_113, 10);
  NAMES[114] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_114, 13);
  NAMES[115] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_115, 17);
  NAMES[116] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_116, 10);
  NAMES[117] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_117, 20);
  NAMES[118] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_118, 18);
  NAMES[119] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_119, 15);
  NAMES[120] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_120, 18);
  NAMES[121] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_121, 19);
  NAMES[122] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_122, 16);
  NAMES[123] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_123, 19);
  NAMES[124] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_124, 8);
  NAMES[125] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_125, 13);
  NAMES[126] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_126, 10);
  NAMES[127] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_127, 5);
  NAMES[128] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_128, 7);
  NAMES[129] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_129, 4);
  NAMES[130] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_130, 3);
  NAMES[131] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_131, 4);
  NAMES[132] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_132, 6);
  NAMES[133] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_133, 7);
  NAMES[134] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_134, 7);
  NAMES[135] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_135, 5);
  NAMES[136] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_136, 6);
  NAMES[137] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_137, 4);
  NAMES[138] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_138, 5);
  NAMES[139] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_139, 4);
  NAMES[140] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_140, 6);
  NAMES[141] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_141, 7);
  NAMES[142] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_142, 8);
  NAMES[143] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_143, 6);
  NAMES[144] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_144, 17);
  NAMES[145] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_145, 21);
  NAMES[146] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_146, 6);
  NAMES[147] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_147, 5);
  NAMES[148] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_148, 8);
  NAMES[149] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_149, 6);
  NAMES[150] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_150, 11);
  NAMES[151] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_151, 12);
  NAMES[152] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_152, 5);
  NAMES[153] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_153, 5);
  NAMES[154] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_154, 4);
  NAMES[155] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_155, 4);
  NAMES[156] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_156, 5);
  NAMES[157] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_157, 7);
  NAMES[158] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_158, 13);
  NAMES[159] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_159, 4);
  NAMES[160] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_160, 4);
  NAMES[161] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_161, 18);
  NAMES[162] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_162, 22);
  NAMES[163] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_163, 5);
  NAMES[164] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_164, 7);
  NAMES[165] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_165, 11);
  NAMES[166] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_166, 5);
  NAMES[167] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_167, 5);
  NAMES[168] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_168, 2);
  NAMES[169] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_169, 3);
  NAMES[170] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_170, 6);
  NAMES[171] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_171, 7);
  NAMES[172] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_172, 7);
  NAMES[173] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_173, 7);
  NAMES[174] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_174, 6);
  NAMES[175] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_175, 4);
  NAMES[176] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_176, 5);
  NAMES[177] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_177, 4);
  NAMES[178] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_178, 3);
  NAMES[179] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_179, 5);
  NAMES[180] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_180, 13);
  NAMES[181] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_181, 17);
  NAMES[182] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_182, 17);
  NAMES[183] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_183, 15);
  NAMES[184] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_184, 12);
  NAMES[185] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_185, 18);
  NAMES[186] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_186, 13);
  NAMES[187] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_187, 5);
  NAMES[188] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_188, 3);
  NAMES[189] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_189, 7);
  NAMES[190] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_190, 6);
  NAMES[191] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_191, 4);
  NAMES[192] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_192, 6);
  NAMES[193] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_193, 4);
  NAMES[194] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_194, 13);
  NAMES[195] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_195, 5);
  NAMES[196] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_196, 15);
  NAMES[197] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_197, 5);
  NAMES[198] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_198, 7);
  NAMES[199] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_199, 13);
  NAMES[200] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_200, 10);
  NAMES[201] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_201, 5);
  NAMES[202] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_202, 6);
  NAMES[203] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_203, 5);
  NAMES[204] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_204, 6);
  NAMES[205] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_205, 7);
  NAMES[206] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_206, 5);
  NAMES[207] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_207, 6);
  NAMES[208] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_208, 4);
  NAMES[209] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_209, 5);
  NAMES[210] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_210, 4);
  NAMES[211] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_211, 6);
  NAMES[212] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_212, 7);
  NAMES[213] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_213, 3);
  NAMES[214] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_214, 6);
  NAMES[215] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_215, 11);
  NAMES[216] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_216, 8);
  NAMES[217] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_217, 4);
  NAMES[218] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_218, 9);
  NAMES[219] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_219, 13);
  NAMES[220] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_220, 15);
  NAMES[221] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_221, 15);
  NAMES[222] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_222, 6);
  NAMES[223] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_223, 5);
  NAMES[224] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_224, 5);
  NAMES[225] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_225, 5);
  NAMES[226] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_226, 7);
  NAMES[227] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_227, 6);
  NAMES[228] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_228, 4);
  NAMES[229] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_229, 5);
  NAMES[230] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_230, 6);
  NAMES[231] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_231, 4);
  NAMES[232] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_232, 4);
  NAMES[233] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_233, 5);
  NAMES[234] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_234, 5);
  NAMES[235] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_235, 7);
  NAMES[236] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_236, 6);
  NAMES[237] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_237, 5);
  NAMES[238] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_238, 5);
  NAMES[239] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_239, 6);
  NAMES[240] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_240, 7);
  NAMES[241] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_241, 4);
  NAMES[242] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_242, 4);
  NAMES[243] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_243, 5);
  NAMES[244] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_244, 5);
  NAMES[245] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_245, 5);
  NAMES[246] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_246, 2);
  NAMES[247] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_247, 3);
  NAMES[248] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_248, 7);
  NAMES[249] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_249, 7);
  NAMES[250] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_250, 5);
  NAMES[251] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_251, 11);
  NAMES[252] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_252, 5);
  NAMES[253] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_253, 7);
  NAMES[254] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_254, 7);
  NAMES[255] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_255, 4);
  NAMES[256] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_256, 17);
  NAMES[257] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_257, 10);
  NAMES[258] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_258, 13);
  NAMES[259] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_259, 20);
  NAMES[260] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_260, 12);
  NAMES[261] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_261, 18);
  NAMES[262] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_262, 18);
  NAMES[263] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_263, 15);
  NAMES[264] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_264, 18);
  NAMES[265] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_265, 10);
  NAMES[266] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_266, 15);
  NAMES[267] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_267, 16);
  NAMES[268] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_268, 8);
  NAMES[269] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_269, 13);
  NAMES[270] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_270, 14);
  NAMES[271] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_271, 13);
  NAMES[272] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_272, 16);
  NAMES[273] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_273, 18);
  NAMES[274] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_274, 17);
  NAMES[275] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_275, 16);
  NAMES[276] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_276, 13);
  NAMES[277] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_277, 16);
  NAMES[278] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_278, 11);
  NAMES[279] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_279, 14);
  NAMES[280] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_280, 10);
  NAMES[281] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_281, 15);
  NAMES[282] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_282, 17);
  NAMES[283] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_283, 14);
  NAMES[284] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_284, 12);
  NAMES[285] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_285, 9);
  NAMES[286] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_286, 15);
  NAMES[287] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_287, 10);
  NAMES[288] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_288, 4);
  NAMES[289] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_289, 3);
  NAMES[290] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_290, 11);
  NAMES[291] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_291, 7);
  NAMES[292] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_292, 14);
  NAMES[293] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_293, 19);
  NAMES[294] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_294, 15);
  NAMES[295] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_295, 14);
  NAMES[296] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_296, 19);
  NAMES[297] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_297, 15);
  NAMES[298] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_298, 5);
  NAMES[299] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_299, 15);
  NAMES[300] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_300, 16);
  NAMES[301] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_301, 5);
  NAMES[302] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_302, 4);
  NAMES[303] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_303, 7);
  NAMES[304] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_304, 3);
  NAMES[305] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_305, 4);
  NAMES[306] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_306, 4);
  NAMES[307] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_307, 12);
  NAMES[308] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_308, 10);
  NAMES[309] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_309, 4);
  NAMES[310] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_310, 10);
  NAMES[311] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_311, 5);
  NAMES[312] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_312, 5);
  NAMES[313] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_313, 3);
  NAMES[314] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_314, 5);
  NAMES[315] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_315, 7);
  NAMES[316] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_316, 7);
  NAMES[317] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_317, 7);
  NAMES[318] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_318, 4);
  NAMES[319] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_319, 20);
  NAMES[320] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_320, 19);
  NAMES[321] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_321, 18);
  NAMES[322] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_322, 22);
  NAMES[323] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_323, 21);
  NAMES[324] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_324, 15);
  NAMES[325] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_325, 8);
  NAMES[326] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_326, 4);
  NAMES[327] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_327, 8);
  NAMES[328] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_328, 17);
  NAMES[329] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_329, 5);
  NAMES[330] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_330, 4);
  NAMES[331] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_331, 13);
  NAMES[332] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_332, 10);
  NAMES[333] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_333, 21);
  NAMES[334] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_334, 11);
  NAMES[335] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_335, 9);
  NAMES[336] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_336, 10);
  NAMES[337] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_337, 11);
  NAMES[338] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_338, 16);
  NAMES[339] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_339, 15);
  NAMES[340] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_340, 16);
  NAMES[341] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_341, 16);
  NAMES[342] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_342, 21);
  NAMES[343] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_343, 8);
  NAMES[344] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_344, 13);
  NAMES[345] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_345, 15);
  NAMES[346] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_346, 13);
  NAMES[347] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_347, 12);
  NAMES[348] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_348, 22);
  NAMES[349] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_349, 18);
  NAMES[350] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_350, 17);
  NAMES[351] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_351, 22);
  NAMES[352] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_352, 21);
  NAMES[353] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_353, 23);
  NAMES[354] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_354, 15);
  NAMES[355] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_355, 12);
  NAMES[356] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_356, 22);
  NAMES[357] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_357, 17);
  NAMES[358] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_358, 9);
  NAMES[359] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_359, 14);
  NAMES[360] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_360, 18);
  NAMES[361] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_361, 14);
  NAMES[362] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_362, 15);
  NAMES[363] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_363, 5);
  NAMES[364] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_364, 6);
  NAMES[365] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_365, 7);
  NAMES[366] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_366, 3);
  NAMES[367] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_367, 6);
  NAMES[368] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_368, 6);
  NAMES[369] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_369, 7);
  NAMES[370] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_370, 5);
  NAMES[371] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_371, 6);
  NAMES[372] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_372, 4);
  NAMES[373] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_373, 7);
  NAMES[374] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_374, 4);
  NAMES[375] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_375, 6);
  NAMES[376] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_376, 7);
  NAMES[377] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_377, 6);
  NAMES[378] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_378, 6);
  NAMES[379] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_379, 8);
  NAMES[380] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_380, 5);
  NAMES[381] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_381, 21);
  NAMES[382] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_382, 15);
  NAMES[383] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_383, 3);
  NAMES[384] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_384, 5);
  NAMES[385] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_385, 6);
  NAMES[386] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_386, 7);
  NAMES[387] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_387, 6);
  NAMES[388] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_388, 7);
  NAMES[389] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_389, 7);
  NAMES[390] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_390, 4);
  NAMES[391] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_391, 5);
  NAMES[392] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_392, 8);
  NAMES[393] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_393, 10);
  NAMES[394] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_394, 12);
  NAMES[395] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_395, 16);
  NAMES[396] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_396, 9);
  NAMES[397] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_397, 4);
  NAMES[398] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_398, 4);
  NAMES[399] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_399, 4);
  NAMES[400] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_400, 3);
  NAMES[401] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_401, 10);
  NAMES[402] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_402, 14);
  NAMES[403] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_403, 5);
  NAMES[404] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_404, 3);
  NAMES[405] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_405, 9);
  NAMES[406] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_406, 14);
  NAMES[407] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_407, 19);
  NAMES[408] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_408, 14);
  NAMES[409] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_409, 6);
  NAMES[410] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_410, 8);
  NAMES[411] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_411, 11);
  NAMES[412] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_412, 13);
  NAMES[413] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_413, 5);
  NAMES[414] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_414, 4);
  NAMES[415] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_415, 4);
  NAMES[416] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_416, 5);
  NAMES[417] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_417, 4);
  NAMES[418] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_418, 5);
  NAMES[419] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_419, 5);
  NAMES[420] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_420, 6);
  NAMES[421] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_421, 3);
  NAMES[422] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_422, 4);
  NAMES[423] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_423, 7);
  NAMES[424] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_424, 5);
  NAMES[425] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_425, 5);
  NAMES[426] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_426, 7);
  NAMES[427] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_427, 7);
  NAMES[428] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_428, 7);
  NAMES[429] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_429, 4);
  NAMES[430] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_430, 3);
  NAMES[431] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_431, 15);
  NAMES[432] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_432, 19);
  NAMES[433] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_433, 21);
  NAMES[434] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_434, 4);
  NAMES[435] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_435, 4);
  NAMES[436] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_436, 18);
  NAMES[437] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_437, 11);
  NAMES[438] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_438, 14);
  NAMES[439] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_439, 20);
  NAMES[440] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_440, 13);
  NAMES[441] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_441, 19);
  NAMES[442] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_442, 19);
  NAMES[443] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_443, 16);
  NAMES[444] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_444, 19);
  NAMES[445] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_445, 11);
  NAMES[446] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_446, 9);
  NAMES[447] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_447, 14);
  NAMES[448] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_448, 15);
  NAMES[449] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_449, 14);
  NAMES[450] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_450, 17);
  NAMES[451] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_451, 19);
  NAMES[452] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_452, 18);
  NAMES[453] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_453, 17);
  NAMES[454] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_454, 14);
  NAMES[455] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_455, 17);
  NAMES[456] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_456, 12);
  NAMES[457] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_457, 15);
  NAMES[458] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_458, 11);
  NAMES[459] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_459, 5);
  NAMES[460] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_460, 13);
  NAMES[461] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_461, 12);
  NAMES[462] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_462, 5);
  NAMES[463] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_463, 4);
  NAMES[464] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_464, 12);
  NAMES[465] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_465, 7);
  NAMES[466] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_466, 5);
  NAMES[467] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_467, 7);
  NAMES[468] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_468, 7);
  NAMES[469] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_469, 3);
  NAMES[470] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_470, 7);
  NAMES[471] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_471, 7);
  NAMES[472] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_472, 6);
  NAMES[473] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_473, 4);
  NAMES[474] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_474, 4);
  NAMES[475] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_475, 15);
  NAMES[476] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_476, 15);
  NAMES[477] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_477, 16);
  NAMES[478] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_478, 13);
  NAMES[479] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_479, 6);
  NAMES[480] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_480, 12);
  NAMES[481] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_481, 5);
  NAMES[482] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_482, 5);
  NAMES[483] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_483, 7);
  NAMES[484] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_484, 19);
  NAMES[485] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_485, 13);
  NAMES[486] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_486, 18);
  NAMES[487] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_487, 15);
  NAMES[488] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_488, 20);
  NAMES[489] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_489, 12);
  NAMES[490] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_490, 5);
  NAMES[491] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_491, 5);
  NAMES[492] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_492, 4);
  NAMES[493] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_493, 7);
  NAMES[494] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_494, 12);
  NAMES[495] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_495, 9);
  NAMES[496] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_496, 14);
  NAMES[497] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_497, 19);
  NAMES[498] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_498, 14);
  NAMES[499] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_499, 9);
  NAMES[500] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_500, 4);
  NAMES[501] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_501, 4);
  NAMES[502] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_502, 9);
  NAMES[503] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_503, 14);
  NAMES[504] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_504, 7);
  NAMES[505] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_505, 5);
  NAMES[506] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_506, 6);
  NAMES[507] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_507, 6);
  NAMES[508] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_508, 6);
  NAMES[509] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_509, 5);
  NAMES[510] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_510, 4);
  NAMES[511] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_511, 4);
  NAMES[512] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_512, 7);
  NAMES[513] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_513, 7);
  NAMES[514] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_514, 4);
  NAMES[515] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_515, 4);
  NAMES[516] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_516, 10);
  NAMES[517] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_517, 6);
  NAMES[518] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_518, 10);
  NAMES[519] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_519, 6);
  NAMES[520] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_520, 11);
  NAMES[521] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_521, 15);
  NAMES[522] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_522, 11);
  NAMES[523] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_523, 5);
  NAMES[524] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_524, 10);
  NAMES[525] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_525, 5);
  NAMES[526] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_526, 7);
  NAMES[527] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_527, 6);
  NAMES[528] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_528, 7);
  NAMES[529] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_529, 5);
  NAMES[530] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_530, 9);
  NAMES[531] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_531, 6);
  NAMES[532] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_532, 7);
  NAMES[533] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_533, 5);
  NAMES[534] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_534, 6);
  NAMES[535] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_535, 4);
  NAMES[536] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_536, 7);
  NAMES[537] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_537, 4);
  NAMES[538] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_538, 6);
  NAMES[539] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_539, 7);
  NAMES[540] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_540, 6);
  NAMES[541] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_541, 9);
  NAMES[542] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_542, 11);
  NAMES[543] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_543, 13);
  NAMES[544] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_544, 17);
  NAMES[545] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_545, 6);
  NAMES[546] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_546, 10);
  NAMES[547] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_547, 6);
  NAMES[548] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_548, 5);
  NAMES[549] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_549, 8);
  NAMES[550] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_550, 11);
  NAMES[551] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_551, 17);
  NAMES[552] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_552, 12);
  NAMES[553] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_553, 14);
  NAMES[554] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_554, 6);
  NAMES[555] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_555, 11);
  NAMES[556] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_556, 8);
  NAMES[557] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_557, 12);
  NAMES[558] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_558, 15);
  NAMES[559] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_559, 16);
  NAMES[560] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_560, 5);
  NAMES[561] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_561, 8);
  NAMES[562] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_562, 6);
  NAMES[563] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_563, 5);
  NAMES[564] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_564, 7);
  NAMES[565] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_565, 4);
  NAMES[566] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_566, 5);
  NAMES[567] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_567, 6);
  NAMES[568] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_568, 5);
  NAMES[569] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_569, 4);
  NAMES[570] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_570, 6);
  NAMES[571] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_571, 7);
  NAMES[572] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_572, 4);
  NAMES[573] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_573, 7);
  NAMES[574] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_574, 5);
  NAMES[575] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_575, 12);
  NAMES[576] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_576, 13);
  NAMES[577] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_577, 18);
  NAMES[578] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_578, 14);
  NAMES[579] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_579, 14);
  NAMES[580] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_580, 4);
  NAMES[581] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_581, 5);
  NAMES[582] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_582, 5);
  NAMES[583] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_583, 7);
  NAMES[584] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_584, 6);
  NAMES[585] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_585, 6);
  NAMES[586] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_586, 4);
  NAMES[587] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_587, 5);
  NAMES[588] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_588, 5);
  NAMES[589] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_589, 4);
  NAMES[590] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_590, 3);
  NAMES[591] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_591, 5);
  NAMES[592] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_592, 5);
  NAMES[593] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_593, 5);
  NAMES[594] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_594, 5);
  NAMES[595] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_595, 5);
  NAMES[596] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_596, 6);
  NAMES[597] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_597, 7);
  NAMES[598] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_598, 6);
  NAMES[599] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_599, 4);
  NAMES[600] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_600, 4);
  NAMES[601] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_601, 5);
  NAMES[602] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_602, 5);
  NAMES[603] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_603, 5);
  NAMES[604] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_604, 5);
  NAMES[605] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_605, 7);
  NAMES[606] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_606, 7);
  NAMES[607] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_607, 4);
  NAMES[608] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_608, 5);
  NAMES[609] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_609, 15);
  NAMES[610] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_610, 5);
  NAMES[611] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_611, 4);
  NAMES[612] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_612, 5);
  NAMES[613] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_613, 5);
  NAMES[614] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_614, 6);
  NAMES[615] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_615, 7);
  NAMES[616] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_616, 7);
  NAMES[617] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_617, 3);
  NAMES[618] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_618, 4);
  NAMES[619] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_619, 5);
  NAMES[620] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_620, 6);
  NAMES[621] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_621, 5);
  NAMES[622] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_622, 6);
  NAMES[623] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_623, 4);
  NAMES[624] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_624, 5);
  NAMES[625] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_625, 6);
  NAMES[626] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_626, 3);
  NAMES[627] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_627, 4);
  NAMES[628] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_628, 6);
  NAMES[629] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_629, 7);
  NAMES[630] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_630, 8);
  NAMES[631] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_631, 6);
  NAMES[632] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_632, 6);
  NAMES[633] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_633, 6);
  NAMES[634] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_634, 6);
  NAMES[635] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_635, 3);
  NAMES[636] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_636, 4);
  NAMES[637] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_637, 4);
  NAMES[638] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_638, 7);
  NAMES[639] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_639, 5);
  NAMES[640] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_640, 9);
  NAMES[641] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_641, 5);
  NAMES[642] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_642, 4);
  NAMES[643] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_643, 5);
  NAMES[644] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_644, 6);
  NAMES[645] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_645, 7);
  NAMES[646] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_646, 9);
  NAMES[647] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_647, 9);
  NAMES[648] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_648, 9);
  NAMES[649] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_649, 9);
  NAMES[650] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_650, 9);
  NAMES[651] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_651, 9);
  NAMES[652] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_652, 9);
  NAMES[653] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_653, 9);
  NAMES[654] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_654, 6);
  NAMES[655] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_655, 8);
  NAMES[656] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_656, 9);
  NAMES[657] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_657, 7);
  NAMES[658] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_658, 6);
  NAMES[659] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_659, 8);
  NAMES[660] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_660, 6);
  NAMES[661] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_661, 5);
  NAMES[662] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_662, 3);
  NAMES[663] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_663, 4);
  NAMES[664] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_664, 7);
  NAMES[665] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_665, 4);
  NAMES[666] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_666, 5);
  NAMES[667] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_667, 5);
  NAMES[668] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_668, 7);
  NAMES[669] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_669, 9);
  NAMES[670] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_670, 5);
  NAMES[671] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_671, 6);
  NAMES[672] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_672, 5);
  NAMES[673] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_673, 4);
  NAMES[674] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_674, 6);
  NAMES[675] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_675, 8);
  NAMES[676] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_676, 6);
  NAMES[677] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_677, 7);
  NAMES[678] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_678, 4);
  NAMES[679] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_679, 5);
  NAMES[680] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_680, 9);
  NAMES[681] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_681, 6);
  NAMES[682] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_682, 5);
  NAMES[683] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_683, 9);
  NAMES[684] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_684, 12);
  NAMES[685] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_685, 10);
  NAMES[686] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_686, 8);
  NAMES[687] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_687, 10);
  NAMES[688] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_688, 7);
  NAMES[689] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_689, 7);
  NAMES[690] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_690, 9);
  NAMES[691] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_691, 5);
  NAMES[692] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_692, 9);
  NAMES[693] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_693, 6);
  NAMES[694] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_694, 4);
  NAMES[695] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_695, 6);
  NAMES[696] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_696, 7);
  NAMES[697] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_697, 8);
  NAMES[698] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_698, 8);
  NAMES[699] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_699, 6);
  NAMES[700] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_700, 7);
  NAMES[701] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_701, 5);
  NAMES[702] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_702, 5);
  NAMES[703] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_703, 8);
  NAMES[704] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_704, 4);
  NAMES[705] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_705, 7);
  NAMES[706] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_706, 8);
  NAMES[707] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_707, 7);
  NAMES[708] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_708, 8);
  NAMES[709] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_709, 9);
  NAMES[710] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_710, 10);
  NAMES[711] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_711, 9);
  NAMES[712] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_712, 8);
  NAMES[713] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_713, 16);
  NAMES[714] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_714, 14);
  NAMES[715] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_715, 9);
  NAMES[716] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_716, 7);
  NAMES[717] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_717, 9);
  NAMES[718] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_718, 7);
  NAMES[719] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_719, 13);
  NAMES[720] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_720, 12);
  NAMES[721] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_721, 14);
  NAMES[722] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_722, 18);
  NAMES[723] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_723, 18);
  NAMES[724] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_724, 19);
  NAMES[725] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_725, 6);
  NAMES[726] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_726, 6);
  NAMES[727] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_727, 6);
  NAMES[728] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_728, 6);
  NAMES[729] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_729, 6);
  NAMES[730] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_730, 5);
  NAMES[731] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_731, 5);
  NAMES[732] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_732, 4);
  NAMES[733] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_733, 7);
  NAMES[734] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_734, 7);
  NAMES[735] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_735, 6);
  NAMES[736] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_736, 6);
  NAMES[737] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_737, 6);
  NAMES[738] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_738, 6);
  NAMES[739] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_739, 5);
  NAMES[740] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_740, 6);
  NAMES[741] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_741, 6);
  NAMES[742] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_742, 6);
  NAMES[743] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_743, 6);
  NAMES[744] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_744, 6);
  NAMES[745] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_745, 6);
  NAMES[746] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_746, 6);
  NAMES[747] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_747, 6);
  NAMES[748] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_748, 5);
  NAMES[749] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_749, 6);
  NAMES[750] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_750, 6);
  NAMES[751] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_751, 6);
  NAMES[752] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_752, 6);
  NAMES[753] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_753, 6);
  NAMES[754] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_754, 6);
  NAMES[755] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_755, 7);
  NAMES[756] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_756, 6);
  NAMES[757] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_757, 6);
  NAMES[758] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_758, 6);
  NAMES[759] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_759, 6);
  NAMES[760] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_760, 5);
  NAMES[761] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_761, 6);
  NAMES[762] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_762, 6);
  NAMES[763] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_763, 6);
  NAMES[764] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_764, 6);
  NAMES[765] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_765, 9);
  NAMES[766] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_766, 8);
  NAMES[767] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_767, 9);
  NAMES[768] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_768, 6);
  NAMES[769] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_769, 6);
  NAMES[770] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_770, 6);
  NAMES[771] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_771, 6);
  NAMES[772] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_772, 5);
  NAMES[773] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_773, 6);
  NAMES[774] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_774, 6);
  NAMES[775] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_775, 6);
  NAMES[776] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_776, 6);
  NAMES[777] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_777, 6);
  NAMES[778] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_778, 6);
  NAMES[779] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_779, 7);
  NAMES[780] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_780, 6);
  NAMES[781] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_781, 6);
  NAMES[782] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_782, 7);
  NAMES[783] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_783, 5);
  NAMES[784] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_784, 6);
  NAMES[785] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_785, 5);
  NAMES[786] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_786, 6);
  NAMES[787] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_787, 5);
  NAMES[788] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_788, 6);
  NAMES[789] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_789, 5);
  NAMES[790] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_790, 7);
  NAMES[791] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_791, 5);
  NAMES[792] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_792, 6);
  NAMES[793] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_793, 6);
  NAMES[794] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_794, 7);
  NAMES[795] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_795, 7);
  NAMES[796] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_796, 4);
  NAMES[797] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_797, 7);
  NAMES[798] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_798, 9);
  NAMES[799] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_799, 7);
  NAMES[800] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_800, 7);
  NAMES[801] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_801, 7);
  NAMES[802] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_802, 6);
  NAMES[803] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_803, 6);
  NAMES[804] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_804, 6);
  NAMES[805] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_805, 7);
  NAMES[806] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_806, 6);
  NAMES[807] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_807, 7);
  NAMES[808] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_808, 6);
  NAMES[809] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_809, 6);
  NAMES[810] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_810, 8);
  NAMES[811] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_811, 5);
  NAMES[812] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_812, 5);
  NAMES[813] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_813, 6);
  NAMES[814] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_814, 8);
  NAMES[815] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_815, 4);
  NAMES[816] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_816, 5);
  NAMES[817] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_817, 10);
  NAMES[818] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_818, 4);
  NAMES[819] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_819, 5);
  NAMES[820] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_820, 6);
  NAMES[821] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_821, 10);
  NAMES[822] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_822, 4);
  NAMES[823] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_823, 4);
  NAMES[824] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_824, 5);
  NAMES[825] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_825, 5);
  NAMES[826] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_826, 7);
  NAMES[827] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_827, 16);
  NAMES[828] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_828, 17);
  NAMES[829] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_829, 9);
  NAMES[830] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_830, 9);
  NAMES[831] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_831, 11);
  NAMES[832] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_832, 12);
  NAMES[833] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_833, 12);
  NAMES[834] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_834, 5);
  NAMES[835] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_835, 9);
  NAMES[836] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_836, 7);
  NAMES[837] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_837, 8);
  NAMES[838] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_838, 6);
  NAMES[839] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_839, 9);
  NAMES[840] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_840, 6);
  NAMES[841] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_841, 7);
  NAMES[842] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_842, 8);
  NAMES[843] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_843, 6);
  NAMES[844] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_844, 7);
  NAMES[845] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_845, 5);
  NAMES[846] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_846, 7);
  NAMES[847] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_847, 11);
  NAMES[848] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_848, 10);
  NAMES[849] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_849, 5);
  NAMES[850] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_850, 8);
  NAMES[851] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_851, 7);
  NAMES[852] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_852, 5);
  NAMES[853] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_853, 7);
  NAMES[854] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_854, 4);
  NAMES[855] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_855, 5);
  NAMES[856] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_856, 7);
  NAMES[857] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_857, 6);
  NAMES[858] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_858, 6);
  NAMES[859] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_859, 5);
  NAMES[860] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_860, 5);
  NAMES[861] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_861, 6);
  NAMES[862] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_862, 5);
  NAMES[863] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_863, 6);
  NAMES[864] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_864, 6);
  NAMES[865] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_865, 8);
  NAMES[866] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_866, 8);
  NAMES[867] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_867, 6);
  NAMES[868] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_868, 6);
  NAMES[869] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_869, 7);
  NAMES[870] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_870, 8);
  NAMES[871] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_871, 4);
  NAMES[872] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_872, 9);
  NAMES[873] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_873, 7);
  NAMES[874] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_874, 7);
  NAMES[875] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_875, 7);
  NAMES[876] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_876, 6);
  NAMES[877] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_877, 7);
  NAMES[878] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_878, 8);
  NAMES[879] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_879, 12);
  NAMES[880] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_880, 12);
  NAMES[881] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_881, 9);
  NAMES[882] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_882, 11);
  NAMES[883] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_883, 6);
  NAMES[884] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_884, 7);
  NAMES[885] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_885, 15);
  NAMES[886] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_886, 16);
  NAMES[887] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_887, 6);
  NAMES[888] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_888, 6);
  NAMES[889] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_889, 9);
  NAMES[890] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_890, 6);
  NAMES[891] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_891, 7);
  NAMES[892] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_892, 5);
  NAMES[893] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_893, 5);
  NAMES[894] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_894, 7);
  NAMES[895] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_895, 7);
  NAMES[896] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_896, 5);
  NAMES[897] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_897, 5);
  NAMES[898] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_898, 6);
  NAMES[899] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_899, 8);
  NAMES[900] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_900, 6);
  NAMES[901] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_901, 7);
  NAMES[902] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_902, 4);
  NAMES[903] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_903, 3);
  NAMES[904] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_904, 8);
  NAMES[905] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_905, 6);
  NAMES[906] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_906, 8);
  NAMES[907] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_907, 3);
  NAMES[908] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_908, 4);
  NAMES[909] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_909, 6);
  NAMES[910] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_910, 8);
  NAMES[911] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_911, 7);
  NAMES[912] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_912, 4);
  NAMES[913] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_913, 6);
  NAMES[914] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_914, 6);
  NAMES[915] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_915, 5);
  NAMES[916] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_916, 8);
  NAMES[917] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_917, 12);
  NAMES[918] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_918, 6);
  NAMES[919] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_919, 4);
  NAMES[920] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_920, 8);
  NAMES[921] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_921, 6);
  NAMES[922] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_922, 4);
  NAMES[923] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_923, 6);
  NAMES[924] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_924, 7);
  NAMES[925] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_925, 14);
  NAMES[926] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_926, 7);
  NAMES[927] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_927, 5);
  NAMES[928] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_928, 7);
  NAMES[929] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_929, 7);
  NAMES[930] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_930, 7);
  NAMES[931] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_931, 5);
  NAMES[932] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_932, 4);
  NAMES[933] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_933, 6);
  NAMES[934] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_934, 9);
  NAMES[935] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_935, 9);
  NAMES[936] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_936, 8);
  NAMES[937] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_937, 10);
  NAMES[938] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_938, 15);
  NAMES[939] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_939, 10);
  NAMES[940] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_940, 15);
  NAMES[941] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_941, 16);
  NAMES[942] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_942, 17);
  NAMES[943] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_943, 9);
  NAMES[944] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_944, 7);
  NAMES[945] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_945, 7);
  NAMES[946] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_946, 5);
  NAMES[947] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_947, 5);
  NAMES[948] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_948, 5);
  NAMES[949] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_949, 7);
  NAMES[950] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_950, 6);
  NAMES[951] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_951, 5);
  NAMES[952] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_952, 6);
  NAMES[953] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_953, 6);
  NAMES[954] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_954, 6);
  NAMES[955] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_955, 8);
  NAMES[956] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_956, 5);
  NAMES[957] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_957, 9);
  NAMES[958] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_958, 6);
  NAMES[959] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_959, 5);
  NAMES[960] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_960, 6);
  NAMES[961] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_961, 7);
  NAMES[962] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_962, 7);
  NAMES[963] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_963, 7);
  NAMES[964] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_964, 5);
  NAMES[965] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_965, 5);
  NAMES[966] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_966, 6);
  NAMES[967] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_967, 7);
  NAMES[968] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_968, 4);
  NAMES[969] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_969, 5);
  NAMES[970] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_970, 3);
  NAMES[971] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_971, 6);
  NAMES[972] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_972, 4);
  NAMES[973] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_973, 3);
  NAMES[974] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_974, 6);
  NAMES[975] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_975, 7);
  NAMES[976] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_976, 4);
  NAMES[977] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_977, 7);
  NAMES[978] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_978, 3);
  NAMES[979] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_979, 9);
  NAMES[980] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_980, 4);
  NAMES[981] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_981, 4);
  NAMES[982] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_982, 7);
  NAMES[983] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_983, 6);
  NAMES[984] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_984, 6);
  NAMES[985] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_985, 9);
  NAMES[986] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_986, 7);
  NAMES[987] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_987, 7);
  NAMES[988] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_988, 7);
  NAMES[989] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_989, 5);
  NAMES[990] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_990, 4);
  NAMES[991] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_991, 5);
  NAMES[992] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_992, 6);
  NAMES[993] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_993, 5);
  NAMES[994] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_994, 5);
  NAMES[995] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_995, 7);
  NAMES[996] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_996, 6);
  NAMES[997] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_997, 5);
  NAMES[998] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_998, 8);
  NAMES[999] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_999, 6);
  NAMES[1000] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1000, 7);
  NAMES[1001] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1001, 8);
  NAMES[1002] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1002, 6);
  NAMES[1003] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1003, 11);
  NAMES[1004] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1004, 12);
  NAMES[1005] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1005, 7);
  NAMES[1006] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1006, 7);
  NAMES[1007] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1007, 6);
  NAMES[1008] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1008, 8);
  NAMES[1009] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1009, 9);
  NAMES[1010] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1010, 6);
  NAMES[1011] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1011, 6);
  NAMES[1012] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1012, 5);
  NAMES[1013] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1013, 6);
  NAMES[1014] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1014, 5);
  NAMES[1015] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1015, 4);
  NAMES[1016] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1016, 3);
  NAMES[1017] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1017, 4);
  NAMES[1018] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1018, 4);
  NAMES[1019] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1019, 5);
  NAMES[1020] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1020, 5);
  NAMES[1021] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1021, 5);
  NAMES[1022] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1022, 6);
  NAMES[1023] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1023, 12);
  NAMES[1024] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1024, 13);
  NAMES[1025] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1025, 14);
  NAMES[1026] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1026, 4);
  NAMES[1027] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1027, 7);
  NAMES[1028] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1028, 7);
  NAMES[1029] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1029, 6);
  NAMES[1030] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1030, 7);
  NAMES[1031] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1031, 4);
  NAMES[1032] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1032, 6);
  NAMES[1033] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1033, 5);
  NAMES[1034] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1034, 6);
  NAMES[1035] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1035, 6);
  NAMES[1036] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1036, 5);
  NAMES[1037] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1037, 5);
  NAMES[1038] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1038, 7);
  NAMES[1039] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1039, 5);
  NAMES[1040] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1040, 6);
  NAMES[1041] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1041, 9);
  NAMES[1042] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1042, 6);
  NAMES[1043] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1043, 7);
  NAMES[1044] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1044, 7);
  NAMES[1045] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1045, 6);
  NAMES[1046] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1046, 7);
  NAMES[1047] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1047, 7);
  NAMES[1048] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1048, 7);
  NAMES[1049] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1049, 7);
  NAMES[1050] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1050, 7);
  NAMES[1051] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1051, 7);
  NAMES[1052] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1052, 6);
  NAMES[1053] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1053, 7);
  NAMES[1054] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1054, 7);
  NAMES[1055] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1055, 7);
  NAMES[1056] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1056, 7);
  NAMES[1057] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1057, 7);
  NAMES[1058] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1058, 7);
  NAMES[1059] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1059, 7);
  NAMES[1060] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1060, 6);
  NAMES[1061] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1061, 6);
  NAMES[1062] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1062, 5);
  NAMES[1063] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1063, 3);
  NAMES[1064] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1064, 4);
  NAMES[1065] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1065, 7);
  NAMES[1066] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1066, 6);
  NAMES[1067] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1067, 7);
  NAMES[1068] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1068, 4);
  NAMES[1069] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1069, 7);
  NAMES[1070] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1070, 6);
  NAMES[1071] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1071, 4);
  NAMES[1072] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1072, 5);
  NAMES[1073] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1073, 3);
  NAMES[1074] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1074, 4);
  NAMES[1075] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1075, 4);
  NAMES[1076] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1076, 5);
  NAMES[1077] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1077, 9);
  NAMES[1078] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1078, 4);
  NAMES[1079] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1079, 6);
  NAMES[1080] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1080, 7);
  NAMES[1081] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1081, 8);
  NAMES[1082] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1082, 9);
  NAMES[1083] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1083, 7);
  NAMES[1084] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1084, 4);
  NAMES[1085] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1085, 3);
  NAMES[1086] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1086, 4);
  NAMES[1087] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1087, 6);
  NAMES[1088] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1088, 5);
  NAMES[1089] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1089, 3);
  NAMES[1090] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1090, 4);
  NAMES[1091] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1091, 4);
  NAMES[1092] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1092, 4);
  NAMES[1093] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1093, 4);
  NAMES[1094] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1094, 5);
  NAMES[1095] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1095, 9);
  NAMES[1096] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1096, 4);
  NAMES[1097] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1097, 5);
  NAMES[1098] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1098, 6);
  NAMES[1099] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1099, 6);
  NAMES[1100] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1100, 5);
  NAMES[1101] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1101, 6);
  NAMES[1102] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1102, 5);
  NAMES[1103] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1103, 5);
  NAMES[1104] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1104, 6);
  NAMES[1105] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1105, 6);
  NAMES[1106] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1106, 2);
  NAMES[1107] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1107, 3);
  NAMES[1108] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1108, 5);
  NAMES[1109] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1109, 6);
  NAMES[1110] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1110, 6);
  NAMES[1111] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1111, 7);
  NAMES[1112] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1112, 8);
  NAMES[1113] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1113, 10);
  NAMES[1114] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1114, 7);
  NAMES[1115] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1115, 7);
  NAMES[1116] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1116, 10);
  NAMES[1117] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1117, 11);
  NAMES[1118] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1118, 8);
  NAMES[1119] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1119, 7);
  NAMES[1120] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1120, 5);
  NAMES[1121] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1121, 7);
  NAMES[1122] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1122, 5);
  NAMES[1123] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1123, 7);
  NAMES[1124] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1124, 7);
  NAMES[1125] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1125, 5);
  NAMES[1126] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1126, 8);
  NAMES[1127] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1127, 6);
  NAMES[1128] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1128, 5);
  NAMES[1129] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1129, 6);
  NAMES[1130] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1130, 7);
  NAMES[1131] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1131, 10);
  NAMES[1132] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1132, 7);
  NAMES[1133] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1133, 7);
  NAMES[1134] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1134, 4);
  NAMES[1135] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1135, 9);
  NAMES[1136] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1136, 9);
  NAMES[1137] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1137, 6);
  NAMES[1138] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1138, 7);
  NAMES[1139] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1139, 14);
  NAMES[1140] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1140, 15);
  NAMES[1141] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1141, 5);
  NAMES[1142] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1142, 7);
  NAMES[1143] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1143, 5);
  NAMES[1144] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1144, 7);
  NAMES[1145] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1145, 7);
  NAMES[1146] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1146, 7);
  NAMES[1147] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1147, 7);
  NAMES[1148] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1148, 6);
  NAMES[1149] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1149, 7);
  NAMES[1150] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1150, 3);
  NAMES[1151] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1151, 5);
  NAMES[1152] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1152, 6);
  NAMES[1153] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1153, 4);
  NAMES[1154] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1154, 5);
  NAMES[1155] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1155, 5);
  NAMES[1156] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1156, 6);
  NAMES[1157] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1157, 4);
  NAMES[1158] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1158, 4);
  NAMES[1159] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1159, 6);
  NAMES[1160] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1160, 7);
  NAMES[1161] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1161, 3);
  NAMES[1162] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1162, 7);
  NAMES[1163] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1163, 6);
  NAMES[1164] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1164, 7);
  NAMES[1165] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1165, 6);
  NAMES[1166] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1166, 6);
  NAMES[1167] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1167, 6);
  NAMES[1168] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1168, 6);
  NAMES[1169] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1169, 9);
  NAMES[1170] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1170, 9);
  NAMES[1171] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1171, 6);
  NAMES[1172] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1172, 5);
  NAMES[1173] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1173, 6);
  NAMES[1174] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1174, 3);
  NAMES[1175] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1175, 7);
  NAMES[1176] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1176, 6);
  NAMES[1177] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1177, 9);
  NAMES[1178] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1178, 7);
  NAMES[1179] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1179, 4);
  NAMES[1180] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1180, 7);
  NAMES[1181] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1181, 9);
  NAMES[1182] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1182, 9);
  NAMES[1183] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1183, 9);
  NAMES[1184] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1184, 8);
  NAMES[1185] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1185, 5);
  NAMES[1186] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1186, 6);
  NAMES[1187] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1187, 5);
  NAMES[1188] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1188, 5);
  NAMES[1189] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1189, 6);
  NAMES[1190] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1190, 6);
  NAMES[1191] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1191, 7);
  NAMES[1192] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1192, 5);
  NAMES[1193] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1193, 5);
  NAMES[1194] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1194, 6);
  NAMES[1195] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1195, 8);
  NAMES[1196] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1196, 6);
  NAMES[1197] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1197, 7);
  NAMES[1198] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1198, 6);
  NAMES[1199] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1199, 3);
  NAMES[1200] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1200, 7);
  NAMES[1201] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1201, 6);
  NAMES[1202] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1202, 4);
  NAMES[1203] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1203, 5);
  NAMES[1204] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1204, 6);
  NAMES[1205] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1205, 4);
  NAMES[1206] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1206, 4);
  NAMES[1207] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1207, 6);
  NAMES[1208] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1208, 5);
  NAMES[1209] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1209, 5);
  NAMES[1210] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1210, 7);
  NAMES[1211] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1211, 6);
  NAMES[1212] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1212, 6);
  NAMES[1213] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1213, 7);
  NAMES[1214] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1214, 7);
  NAMES[1215] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1215, 4);
  NAMES[1216] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1216, 4);
  NAMES[1217] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1217, 7);
  NAMES[1218] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1218, 5);
  NAMES[1219] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1219, 5);
  NAMES[1220] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1220, 5);
  NAMES[1221] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1221, 5);
  NAMES[1222] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1222, 6);
  NAMES[1223] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1223, 5);
  NAMES[1224] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1224, 7);
  NAMES[1225] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1225, 6);
  NAMES[1226] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1226, 3);
  NAMES[1227] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1227, 4);
  NAMES[1228] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1228, 5);
  NAMES[1229] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1229, 7);
  NAMES[1230] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1230, 9);
  NAMES[1231] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1231, 7);
  NAMES[1232] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1232, 7);
  NAMES[1233] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1233, 5);
  NAMES[1234] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1234, 6);
  NAMES[1235] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1235, 7);
  NAMES[1236] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1236, 4);
  NAMES[1237] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1237, 5);
  NAMES[1238] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1238, 6);
  NAMES[1239] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1239, 5);
  NAMES[1240] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1240, 6);
  NAMES[1241] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1241, 8);
  NAMES[1242] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1242, 7);
  NAMES[1243] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1243, 7);
  NAMES[1244] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1244, 7);
  NAMES[1245] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1245, 7);
  NAMES[1246] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1246, 8);
  NAMES[1247] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1247, 7);
  NAMES[1248] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1248, 4);
  NAMES[1249] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1249, 7);
  NAMES[1250] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1250, 5);
  NAMES[1251] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1251, 6);
  NAMES[1252] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1252, 6);
  NAMES[1253] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1253, 7);
  NAMES[1254] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1254, 7);
  NAMES[1255] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1255, 6);
  NAMES[1256] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1256, 8);
  NAMES[1257] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1257, 8);
  NAMES[1258] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1258, 7);
  NAMES[1259] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1259, 7);
  NAMES[1260] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1260, 6);
  NAMES[1261] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1261, 5);
  NAMES[1262] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1262, 4);
  NAMES[1263] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1263, 5);
  NAMES[1264] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1264, 6);
  NAMES[1265] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1265, 7);
  NAMES[1266] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1266, 8);
  NAMES[1267] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1267, 9);
  NAMES[1268] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1268, 5);
  NAMES[1269] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1269, 3);
  NAMES[1270] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1270, 10);
  NAMES[1271] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1271, 14);
  NAMES[1272] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1272, 16);
  NAMES[1273] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1273, 14);
  NAMES[1274] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1274, 15);
  NAMES[1275] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1275, 15);
  NAMES[1276] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1276, 16);
  NAMES[1277] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1277, 18);
  NAMES[1278] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1278, 20);
  NAMES[1279] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1279, 15);
  NAMES[1280] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1280, 4);
  NAMES[1281] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1281, 4);
  NAMES[1282] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1282, 5);
  NAMES[1283] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1283, 9);
  NAMES[1284] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1284, 4);
  NAMES[1285] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1285, 6);
  NAMES[1286] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1286, 7);
  NAMES[1287] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1287, 8);
  NAMES[1288] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1288, 9);
  NAMES[1289] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1289, 7);
  NAMES[1290] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1290, 11);
  NAMES[1291] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1291, 8);
  NAMES[1292] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1292, 10);
  NAMES[1293] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1293, 11);
  NAMES[1294] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1294, 8);
  NAMES[1295] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1295, 8);
  NAMES[1296] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1296, 7);
  NAMES[1297] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1297, 7);
  NAMES[1298] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1298, 4);
  NAMES[1299] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1299, 3);
  NAMES[1300] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1300, 4);
  NAMES[1301] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1301, 6);
  NAMES[1302] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1302, 6);
  NAMES[1303] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1303, 7);
  NAMES[1304] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1304, 6);
  NAMES[1305] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1305, 5);
  NAMES[1306] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1306, 3);
  NAMES[1307] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1307, 6);
  NAMES[1308] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1308, 9);
  NAMES[1309] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1309, 7);
  NAMES[1310] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1310, 6);
  NAMES[1311] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1311, 7);
  NAMES[1312] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1312, 7);
  NAMES[1313] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1313, 11);
  NAMES[1314] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1314, 4);
  NAMES[1315] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1315, 5);
  NAMES[1316] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1316, 9);
  NAMES[1317] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1317, 4);
  NAMES[1318] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1318, 5);
  NAMES[1319] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1319, 6);
  NAMES[1320] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1320, 6);
  NAMES[1321] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1321, 6);
  NAMES[1322] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1322, 6);
  NAMES[1323] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1323, 6);
  NAMES[1324] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1324, 14);
  NAMES[1325] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1325, 19);
  NAMES[1326] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1326, 11);
  NAMES[1327] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1327, 15);
  NAMES[1328] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1328, 14);
  NAMES[1329] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1329, 15);
  NAMES[1330] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1330, 6);
  NAMES[1331] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1331, 5);
  NAMES[1332] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1332, 7);
  NAMES[1333] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1333, 8);
  NAMES[1334] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1334, 7);
  NAMES[1335] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1335, 7);
  NAMES[1336] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1336, 4);
  NAMES[1337] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1337, 8);
  NAMES[1338] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1338, 5);
  NAMES[1339] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1339, 5);
  NAMES[1340] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1340, 7);
  NAMES[1341] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1341, 6);
  NAMES[1342] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1342, 9);
  NAMES[1343] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1343, 6);
  NAMES[1344] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1344, 7);
  NAMES[1345] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1345, 4);
  NAMES[1346] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1346, 6);
  NAMES[1347] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1347, 7);
  NAMES[1348] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1348, 5);
  NAMES[1349] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1349, 4);
  NAMES[1350] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1350, 5);
  NAMES[1351] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1351, 6);
  NAMES[1352] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1352, 6);
  NAMES[1353] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1353, 5);
  NAMES[1354] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1354, 6);
  NAMES[1355] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1355, 7);
  NAMES[1356] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1356, 7);
  NAMES[1357] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1357, 2);
  NAMES[1358] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1358, 3);
  NAMES[1359] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1359, 5);
  NAMES[1360] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1360, 6);
  NAMES[1361] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1361, 6);
  NAMES[1362] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1362, 7);
  NAMES[1363] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1363, 7);
  NAMES[1364] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1364, 7);
  NAMES[1365] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1365, 8);
  NAMES[1366] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1366, 7);
  NAMES[1367] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1367, 5);
  NAMES[1368] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1368, 6);
  NAMES[1369] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1369, 6);
  NAMES[1370] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1370, 9);
  NAMES[1371] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1371, 8);
  NAMES[1372] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1372, 6);
  NAMES[1373] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1373, 4);
  NAMES[1374] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1374, 5);
  NAMES[1375] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1375, 5);
  NAMES[1376] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1376, 5);
  NAMES[1377] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1377, 8);
  NAMES[1378] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1378, 4);
  NAMES[1379] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1379, 7);
  NAMES[1380] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1380, 11);
  NAMES[1381] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1381, 11);
  NAMES[1382] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1382, 9);
  NAMES[1383] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1383, 7);
  NAMES[1384] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1384, 7);
  NAMES[1385] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1385, 4);
  NAMES[1386] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1386, 6);
  NAMES[1387] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1387, 14);
  NAMES[1388] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1388, 4);
  NAMES[1389] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1389, 4);
  NAMES[1390] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1390, 5);
  NAMES[1391] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1391, 6);
  NAMES[1392] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1392, 4);
  NAMES[1393] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1393, 7);
  NAMES[1394] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1394, 7);
  NAMES[1395] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1395, 6);
  NAMES[1396] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1396, 7);
  NAMES[1397] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1397, 6);
  NAMES[1398] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1398, 7);
  NAMES[1399] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1399, 7);
  NAMES[1400] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1400, 8);
  NAMES[1401] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1401, 5);
  NAMES[1402] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1402, 5);
  NAMES[1403] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1403, 7);
  NAMES[1404] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1404, 7);
  NAMES[1405] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1405, 5);
  NAMES[1406] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1406, 3);
  NAMES[1407] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1407, 5);
  NAMES[1408] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1408, 7);
  NAMES[1409] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1409, 3);
  NAMES[1410] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1410, 9);
  NAMES[1411] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1411, 6);
  NAMES[1412] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1412, 11);
  NAMES[1413] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1413, 16);
  NAMES[1414] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1414, 12);
  NAMES[1415] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1415, 7);
  NAMES[1416] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1416, 7);
  NAMES[1417] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1417, 6);
  NAMES[1418] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1418, 7);
  NAMES[1419] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1419, 4);
  NAMES[1420] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1420, 6);
  NAMES[1421] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1421, 8);
  NAMES[1422] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1422, 6);
  NAMES[1423] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1423, 8);
  NAMES[1424] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1424, 9);
  NAMES[1425] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1425, 4);
  NAMES[1426] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1426, 5);
  NAMES[1427] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1427, 5);
  NAMES[1428] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1428, 7);
  NAMES[1429] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1429, 7);
  NAMES[1430] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1430, 6);
  NAMES[1431] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1431, 5);
  NAMES[1432] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1432, 4);
  NAMES[1433] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1433, 6);
  NAMES[1434] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1434, 3);
  NAMES[1435] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1435, 6);
  NAMES[1436] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1436, 7);
  NAMES[1437] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1437, 6);
  NAMES[1438] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1438, 8);
  NAMES[1439] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1439, 7);
  NAMES[1440] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1440, 7);
  NAMES[1441] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1441, 7);
  NAMES[1442] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1442, 8);
  NAMES[1443] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1443, 4);
  NAMES[1444] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1444, 4);
  NAMES[1445] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1445, 5);
  NAMES[1446] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1446, 6);
  NAMES[1447] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1447, 4);
  NAMES[1448] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1448, 5);
  NAMES[1449] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1449, 6);
  NAMES[1450] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1450, 6);
  NAMES[1451] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1451, 6);
  NAMES[1452] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1452, 3);
  NAMES[1453] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1453, 4);
  NAMES[1454] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1454, 5);
  NAMES[1455] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1455, 4);
  NAMES[1456] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1456, 5);
  NAMES[1457] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1457, 6);
  NAMES[1458] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1458, 6);
  NAMES[1459] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1459, 5);
  NAMES[1460] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1460, 4);
  NAMES[1461] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1461, 11);
  NAMES[1462] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1462, 16);
  NAMES[1463] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1463, 5);
  NAMES[1464] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1464, 6);
  NAMES[1465] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1465, 6);
  NAMES[1466] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1466, 4);
  NAMES[1467] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1467, 6);
  NAMES[1468] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1468, 7);
  NAMES[1469] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1469, 5);
  NAMES[1470] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1470, 5);
  NAMES[1471] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1471, 3);
  NAMES[1472] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1472, 4);
  NAMES[1473] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1473, 6);
  NAMES[1474] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1474, 8);
  NAMES[1475] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1475, 8);
  NAMES[1476] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1476, 8);
  NAMES[1477] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1477, 6);
  NAMES[1478] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1478, 8);
  NAMES[1479] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1479, 8);
  NAMES[1480] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1480, 8);
  NAMES[1481] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1481, 5);
  NAMES[1482] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1482, 10);
  NAMES[1483] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1483, 8);
  NAMES[1484] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1484, 4);
  NAMES[1485] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1485, 7);
  NAMES[1486] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1486, 6);
  NAMES[1487] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1487, 6);
  NAMES[1488] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1488, 6);
  NAMES[1489] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1489, 12);
  NAMES[1490] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1490, 6);
  NAMES[1491] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1491, 7);
  NAMES[1492] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1492, 4);
  NAMES[1493] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1493, 7);
  NAMES[1494] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1494, 5);
  NAMES[1495] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1495, 10);
  NAMES[1496] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1496, 15);
  NAMES[1497] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1497, 5);
  NAMES[1498] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1498, 6);
  NAMES[1499] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1499, 7);
  NAMES[1500] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1500, 6);
  NAMES[1501] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1501, 6);
  NAMES[1502] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1502, 8);
  NAMES[1503] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1503, 8);
  NAMES[1504] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1504, 5);
  NAMES[1505] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1505, 6);
  NAMES[1506] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1506, 10);
  NAMES[1507] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1507, 6);
  NAMES[1508] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1508, 5);
  NAMES[1509] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1509, 6);
  NAMES[1510] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1510, 10);
  NAMES[1511] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1511, 5);
  NAMES[1512] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1512, 6);
  NAMES[1513] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1513, 7);
  NAMES[1514] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1514, 5);
  NAMES[1515] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1515, 14);
  NAMES[1516] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1516, 16);
  NAMES[1517] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1517, 15);
  NAMES[1518] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1518, 17);
  NAMES[1519] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1519, 3);
  NAMES[1520] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1520, 4);
  NAMES[1521] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1521, 7);
  NAMES[1522] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1522, 6);
  NAMES[1523] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1523, 7);
  NAMES[1524] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1524, 7);
  NAMES[1525] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1525, 7);
  NAMES[1526] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1526, 8);
  NAMES[1527] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1527, 7);
  NAMES[1528] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1528, 7);
  NAMES[1529] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1529, 6);
  NAMES[1530] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1530, 7);
  NAMES[1531] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1531, 6);
  NAMES[1532] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1532, 8);
  NAMES[1533] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1533, 7);
  NAMES[1534] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1534, 3);
  NAMES[1535] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1535, 6);
  NAMES[1536] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1536, 7);
  NAMES[1537] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1537, 5);
  NAMES[1538] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1538, 5);
  NAMES[1539] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1539, 5);
  NAMES[1540] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1540, 6);
  NAMES[1541] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1541, 4);
  NAMES[1542] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1542, 6);
  NAMES[1543] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1543, 7);
  NAMES[1544] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1544, 5);
  NAMES[1545] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1545, 5);
  NAMES[1546] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1546, 7);
  NAMES[1547] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1547, 6);
  NAMES[1548] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1548, 6);
  NAMES[1549] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1549, 4);
  NAMES[1550] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1550, 5);
  NAMES[1551] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1551, 6);
  NAMES[1552] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1552, 7);
  NAMES[1553] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1553, 4);
  NAMES[1554] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1554, 6);
  NAMES[1555] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1555, 4);
  NAMES[1556] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1556, 5);
  NAMES[1557] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1557, 6);
  NAMES[1558] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1558, 6);
  NAMES[1559] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1559, 8);
  NAMES[1560] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1560, 6);
  NAMES[1561] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1561, 4);
  NAMES[1562] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1562, 6);
  NAMES[1563] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1563, 6);
  NAMES[1564] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1564, 8);
  NAMES[1565] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1565, 5);
  NAMES[1566] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1566, 7);
  NAMES[1567] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1567, 5);
  NAMES[1568] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1568, 5);
  NAMES[1569] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1569, 6);
  NAMES[1570] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1570, 6);
  NAMES[1571] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1571, 3);
  NAMES[1572] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1572, 6);
  NAMES[1573] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1573, 4);
  NAMES[1574] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1574, 6);
  NAMES[1575] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1575, 8);
  NAMES[1576] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1576, 4);
  NAMES[1577] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1577, 5);
  NAMES[1578] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1578, 4);
  NAMES[1579] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1579, 5);
  NAMES[1580] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1580, 7);
  NAMES[1581] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1581, 5);
  NAMES[1582] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1582, 8);
  NAMES[1583] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1583, 4);
  NAMES[1584] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1584, 5);
  NAMES[1585] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1585, 6);
  NAMES[1586] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1586, 7);
  NAMES[1587] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1587, 5);
  NAMES[1588] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1588, 6);
  NAMES[1589] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1589, 7);
  NAMES[1590] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1590, 7);
  NAMES[1591] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1591, 9);
  NAMES[1592] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1592, 4);
  NAMES[1593] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1593, 5);
  NAMES[1594] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1594, 6);
  NAMES[1595] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1595, 4);
  NAMES[1596] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1596, 4);
  NAMES[1597] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1597, 5);
  NAMES[1598] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1598, 9);
  NAMES[1599] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1599, 7);
  NAMES[1600] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1600, 6);
  NAMES[1601] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1601, 5);
  NAMES[1602] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1602, 4);
  NAMES[1603] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1603, 7);
  NAMES[1604] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1604, 7);
  NAMES[1605] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1605, 7);
  NAMES[1606] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1606, 5);
  NAMES[1607] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1607, 8);
  NAMES[1608] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1608, 4);
  NAMES[1609] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1609, 4);
  NAMES[1610] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1610, 5);
  NAMES[1611] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1611, 7);
  NAMES[1612] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1612, 6);
  NAMES[1613] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1613, 3);
  NAMES[1614] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1614, 10);
  NAMES[1615] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1615, 4);
  NAMES[1616] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1616, 7);
  NAMES[1617] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1617, 8);
  NAMES[1618] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1618, 7);
  NAMES[1619] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1619, 5);
  NAMES[1620] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1620, 9);
  NAMES[1621] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1621, 6);
  NAMES[1622] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1622, 8);
  NAMES[1623] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1623, 7);
  NAMES[1624] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1624, 7);
  NAMES[1625] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1625, 6);
  NAMES[1626] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1626, 6);
  NAMES[1627] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1627, 7);
  NAMES[1628] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1628, 8);
  NAMES[1629] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1629, 8);
  NAMES[1630] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1630, 3);
  NAMES[1631] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1631, 9);
  NAMES[1632] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1632, 5);
  NAMES[1633] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1633, 5);
  NAMES[1634] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1634, 6);
  NAMES[1635] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1635, 3);
  NAMES[1636] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1636, 4);
  NAMES[1637] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1637, 5);
  NAMES[1638] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1638, 6);
  NAMES[1639] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1639, 4);
  NAMES[1640] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1640, 5);
  NAMES[1641] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1641, 11);
  NAMES[1642] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1642, 12);
  NAMES[1643] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1643, 7);
  NAMES[1644] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1644, 12);
  NAMES[1645] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1645, 9);
  NAMES[1646] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1646, 9);
  NAMES[1647] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1647, 8);
  NAMES[1648] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1648, 6);
  NAMES[1649] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1649, 7);
  NAMES[1650] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1650, 5);
  NAMES[1651] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1651, 6);
  NAMES[1652] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1652, 7);
  NAMES[1653] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1653, 5);
  NAMES[1654] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1654, 9);
  NAMES[1655] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1655, 9);
  NAMES[1656] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1656, 9);
  NAMES[1657] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1657, 5);
  NAMES[1658] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1658, 7);
  NAMES[1659] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1659, 6);
  NAMES[1660] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1660, 7);
  NAMES[1661] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1661, 5);
  NAMES[1662] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1662, 4);
  NAMES[1663] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1663, 7);
  NAMES[1664] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1664, 4);
  NAMES[1665] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1665, 5);
  NAMES[1666] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1666, 5);
  NAMES[1667] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1667, 7);
  NAMES[1668] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1668, 5);
  NAMES[1669] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1669, 12);
  NAMES[1670] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1670, 8);
  NAMES[1671] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1671, 6);
  NAMES[1672] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1672, 8);
  NAMES[1673] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1673, 4);
  NAMES[1674] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1674, 5);
  NAMES[1675] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1675, 6);
  NAMES[1676] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1676, 5);
  NAMES[1677] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1677, 7);
  NAMES[1678] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1678, 6);
  NAMES[1679] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1679, 5);
  NAMES[1680] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1680, 5);
  NAMES[1681] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1681, 7);
  NAMES[1682] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1682, 6);
  NAMES[1683] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1683, 9);
  NAMES[1684] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1684, 5);
  NAMES[1685] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1685, 6);
  NAMES[1686] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1686, 6);
  NAMES[1687] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1687, 7);
  NAMES[1688] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1688, 5);
  NAMES[1689] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1689, 6);
  NAMES[1690] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1690, 5);
  NAMES[1691] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1691, 7);
  NAMES[1692] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1692, 6);
  NAMES[1693] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1693, 8);
  NAMES[1694] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1694, 6);
  NAMES[1695] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1695, 7);
  NAMES[1696] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1696, 7);
  NAMES[1697] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1697, 7);
  NAMES[1698] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1698, 7);
  NAMES[1699] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1699, 8);
  NAMES[1700] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1700, 7);
  NAMES[1701] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1701, 6);
  NAMES[1702] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1702, 7);
  NAMES[1703] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1703, 6);
  NAMES[1704] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1704, 10);
  NAMES[1705] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1705, 6);
  NAMES[1706] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1706, 6);
  NAMES[1707] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1707, 7);
  NAMES[1708] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1708, 7);
  NAMES[1709] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1709, 6);
  NAMES[1710] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1710, 8);
  NAMES[1711] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1711, 8);
  NAMES[1712] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1712, 7);
  NAMES[1713] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1713, 7);
  NAMES[1714] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1714, 6);
  NAMES[1715] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1715, 5);
  NAMES[1716] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1716, 4);
  NAMES[1717] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1717, 5);
  NAMES[1718] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1718, 8);
  NAMES[1719] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1719, 6);
  NAMES[1720] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1720, 7);
  NAMES[1721] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1721, 5);
  NAMES[1722] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1722, 5);
  NAMES[1723] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1723, 8);
  NAMES[1724] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1724, 9);
  NAMES[1725] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1725, 6);
  NAMES[1726] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1726, 5);
  NAMES[1727] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1727, 3);
  NAMES[1728] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1728, 4);
  NAMES[1729] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1729, 7);
  NAMES[1730] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1730, 7);
  NAMES[1731] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1731, 4);
  NAMES[1732] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1732, 6);
  NAMES[1733] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1733, 6);
  NAMES[1734] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1734, 7);
  NAMES[1735] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1735, 4);
  NAMES[1736] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1736, 5);
  NAMES[1737] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1737, 11);
  NAMES[1738] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1738, 15);
  NAMES[1739] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1739, 17);
  NAMES[1740] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1740, 15);
  NAMES[1741] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1741, 16);
  NAMES[1742] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1742, 18);
  NAMES[1743] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1743, 17);
  NAMES[1744] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1744, 16);
  NAMES[1745] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1745, 16);
  NAMES[1746] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1746, 5);
  NAMES[1747] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1747, 13);
  NAMES[1748] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1748, 6);
  NAMES[1749] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1749, 6);
  NAMES[1750] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1750, 4);
  NAMES[1751] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1751, 7);
  NAMES[1752] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1752, 11);
  NAMES[1753] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1753, 6);
  NAMES[1754] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1754, 6);
  NAMES[1755] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1755, 6);
  NAMES[1756] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1756, 6);
  NAMES[1757] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1757, 6);
  NAMES[1758] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1758, 5);
  NAMES[1759] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1759, 7);
  NAMES[1760] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1760, 8);
  NAMES[1761] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1761, 5);
  NAMES[1762] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1762, 7);
  NAMES[1763] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1763, 9);
  NAMES[1764] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1764, 6);
  NAMES[1765] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1765, 7);
  NAMES[1766] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1766, 5);
  NAMES[1767] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1767, 4);
  NAMES[1768] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1768, 5);
  NAMES[1769] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1769, 6);
  NAMES[1770] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1770, 7);
  NAMES[1771] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1771, 7);
  NAMES[1772] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1772, 7);
  NAMES[1773] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1773, 5);
  NAMES[1774] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1774, 6);
  NAMES[1775] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1775, 6);
  NAMES[1776] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1776, 9);
  NAMES[1777] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1777, 8);
  NAMES[1778] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1778, 3);
  NAMES[1779] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1779, 7);
  NAMES[1780] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1780, 6);
  NAMES[1781] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1781, 3);
  NAMES[1782] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1782, 4);
  NAMES[1783] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1783, 5);
  NAMES[1784] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1784, 7);
  NAMES[1785] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1785, 6);
  NAMES[1786] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1786, 4);
  NAMES[1787] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1787, 7);
  NAMES[1788] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1788, 6);
  NAMES[1789] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1789, 5);
  NAMES[1790] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1790, 6);
  NAMES[1791] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1791, 7);
  NAMES[1792] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1792, 9);
  NAMES[1793] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1793, 6);
  NAMES[1794] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1794, 4);
  NAMES[1795] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1795, 5);
  NAMES[1796] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1796, 6);
  NAMES[1797] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1797, 6);
  NAMES[1798] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1798, 6);
  NAMES[1799] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1799, 7);
  NAMES[1800] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1800, 6);
  NAMES[1801] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1801, 8);
  NAMES[1802] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1802, 4);
  NAMES[1803] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1803, 5);
  NAMES[1804] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1804, 5);
  NAMES[1805] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1805, 7);
  NAMES[1806] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1806, 9);
  NAMES[1807] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1807, 6);
  NAMES[1808] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1808, 5);
  NAMES[1809] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1809, 4);
  NAMES[1810] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1810, 7);
  NAMES[1811] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1811, 6);
  NAMES[1812] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1812, 7);
  NAMES[1813] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1813, 5);
  NAMES[1814] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1814, 9);
  NAMES[1815] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1815, 14);
  NAMES[1816] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1816, 3);
  NAMES[1817] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1817, 4);
  NAMES[1818] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1818, 6);
  NAMES[1819] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1819, 7);
  NAMES[1820] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1820, 7);
  NAMES[1821] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1821, 4);
  NAMES[1822] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1822, 7);
  NAMES[1823] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1823, 5);
  NAMES[1824] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1824, 6);
  NAMES[1825] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1825, 5);
  NAMES[1826] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1826, 6);
  NAMES[1827] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1827, 5);
  NAMES[1828] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1828, 6);
  NAMES[1829] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1829, 6);
  NAMES[1830] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1830, 8);
  NAMES[1831] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1831, 8);
  NAMES[1832] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1832, 6);
  NAMES[1833] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1833, 14);
  NAMES[1834] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1834, 7);
  NAMES[1835] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1835, 9);
  NAMES[1836] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1836, 5);
  NAMES[1837] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1837, 6);
  NAMES[1838] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1838, 4);
  NAMES[1839] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1839, 5);
  NAMES[1840] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1840, 7);
  NAMES[1841] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1841, 4);
  NAMES[1842] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1842, 5);
  NAMES[1843] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1843, 7);
  NAMES[1844] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1844, 5);
  NAMES[1845] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1845, 7);
  NAMES[1846] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1846, 10);
  NAMES[1847] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1847, 5);
  NAMES[1848] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1848, 6);
  NAMES[1849] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1849, 6);
  NAMES[1850] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1850, 6);
  NAMES[1851] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1851, 7);
  NAMES[1852] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1852, 9);
  NAMES[1853] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1853, 11);
  NAMES[1854] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1854, 6);
  NAMES[1855] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1855, 7);
  NAMES[1856] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1856, 9);
  NAMES[1857] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1857, 11);
  NAMES[1858] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1858, 4);
  NAMES[1859] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1859, 7);
  NAMES[1860] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1860, 7);
  NAMES[1861] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1861, 5);
  NAMES[1862] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1862, 6);
  NAMES[1863] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1863, 5);
  NAMES[1864] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1864, 7);
  NAMES[1865] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1865, 7);
  NAMES[1866] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1866, 7);
  NAMES[1867] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1867, 5);
  NAMES[1868] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1868, 6);
  NAMES[1869] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1869, 16);
  NAMES[1870] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1870, 12);
  NAMES[1871] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1871, 6);
  NAMES[1872] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1872, 4);
  NAMES[1873] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1873, 5);
  NAMES[1874] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1874, 7);
  NAMES[1875] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1875, 5);
  NAMES[1876] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1876, 8);
  NAMES[1877] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1877, 8);
  NAMES[1878] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1878, 6);
  NAMES[1879] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1879, 6);
  NAMES[1880] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1880, 8);
  NAMES[1881] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1881, 8);
  NAMES[1882] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1882, 7);
  NAMES[1883] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1883, 9);
  NAMES[1884] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1884, 10);
  NAMES[1885] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1885, 10);
  NAMES[1886] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1886, 11);
  NAMES[1887] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1887, 7);
  NAMES[1888] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1888, 7);
  NAMES[1889] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1889, 7);
  NAMES[1890] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1890, 5);
  NAMES[1891] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1891, 11);
  NAMES[1892] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1892, 12);
  NAMES[1893] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1893, 7);
  NAMES[1894] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1894, 12);
  NAMES[1895] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1895, 9);
  NAMES[1896] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1896, 9);
  NAMES[1897] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1897, 8);
  NAMES[1898] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1898, 4);
  NAMES[1899] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1899, 5);
  NAMES[1900] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1900, 4);
  NAMES[1901] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1901, 5);
  NAMES[1902] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1902, 4);
  NAMES[1903] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1903, 5);
  NAMES[1904] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1904, 4);
  NAMES[1905] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1905, 5);
  NAMES[1906] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1906, 4);
  NAMES[1907] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1907, 5);
  NAMES[1908] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1908, 7);
  NAMES[1909] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1909, 8);
  NAMES[1910] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1910, 5);
  NAMES[1911] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1911, 8);
  NAMES[1912] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1912, 8);
  NAMES[1913] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1913, 8);
  NAMES[1914] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1914, 8);
  NAMES[1915] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1915, 6);
  NAMES[1916] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1916, 6);
  NAMES[1917] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1917, 8);
  NAMES[1918] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1918, 7);
  NAMES[1919] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1919, 9);
  NAMES[1920] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1920, 10);
  NAMES[1921] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1921, 10);
  NAMES[1922] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1922, 11);
  NAMES[1923] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1923, 7);
  NAMES[1924] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1924, 7);
  NAMES[1925] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1925, 7);
  NAMES[1926] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1926, 6);
  NAMES[1927] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1927, 7);
  NAMES[1928] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1928, 6);
  NAMES[1929] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1929, 8);
  NAMES[1930] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1930, 7);
  NAMES[1931] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1931, 5);
  NAMES[1932] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1932, 6);
  NAMES[1933] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1933, 7);
  NAMES[1934] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1934, 4);
  NAMES[1935] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1935, 5);
  NAMES[1936] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1936, 7);
  NAMES[1937] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1937, 7);
  NAMES[1938] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1938, 4);
  NAMES[1939] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1939, 5);
  NAMES[1940] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1940, 7);
  NAMES[1941] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1941, 4);
  NAMES[1942] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1942, 7);
  NAMES[1943] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1943, 10);
  NAMES[1944] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1944, 6);
  NAMES[1945] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1945, 9);
  NAMES[1946] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1946, 7);
  NAMES[1947] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1947, 12);
  NAMES[1948] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1948, 9);
  NAMES[1949] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1949, 7);
  NAMES[1950] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1950, 6);
  NAMES[1951] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1951, 7);
  NAMES[1952] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1952, 5);
  NAMES[1953] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1953, 6);
  NAMES[1954] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1954, 6);
  NAMES[1955] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1955, 5);
  NAMES[1956] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1956, 6);
  NAMES[1957] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1957, 7);
  NAMES[1958] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1958, 9);
  NAMES[1959] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1959, 7);
  NAMES[1960] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1960, 5);
  NAMES[1961] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1961, 5);
  NAMES[1962] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1962, 4);
  NAMES[1963] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1963, 7);
  NAMES[1964] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1964, 7);
  NAMES[1965] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1965, 5);
  NAMES[1966] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1966, 8);
  NAMES[1967] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1967, 5);
  NAMES[1968] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1968, 7);
  NAMES[1969] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1969, 6);
  NAMES[1970] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1970, 9);
  NAMES[1971] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1971, 13);
  NAMES[1972] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1972, 13);
  NAMES[1973] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1973, 15);
  NAMES[1974] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1974, 10);
  NAMES[1975] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1975, 14);
  NAMES[1976] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1976, 16);
  NAMES[1977] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1977, 7);
  NAMES[1978] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1978, 5);
  NAMES[1979] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1979, 9);
  NAMES[1980] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1980, 8);
  NAMES[1981] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1981, 6);
  NAMES[1982] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1982, 8);
  NAMES[1983] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1983, 9);
  NAMES[1984] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1984, 5);
  NAMES[1985] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1985, 5);
  NAMES[1986] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1986, 6);
  NAMES[1987] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1987, 7);
  NAMES[1988] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1988, 6);
  NAMES[1989] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1989, 17);
  NAMES[1990] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1990, 18);
  NAMES[1991] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1991, 5);
  NAMES[1992] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1992, 5);
  NAMES[1993] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1993, 6);
  NAMES[1994] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1994, 7);
  NAMES[1995] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1995, 5);
  NAMES[1996] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1996, 6);
  NAMES[1997] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1997, 7);
  NAMES[1998] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1998, 5);
  NAMES[1999] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_1999, 6);
  NAMES[2000] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2000, 4);
  NAMES[2001] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2001, 6);
  NAMES[2002] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2002, 7);
  NAMES[2003] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2003, 6);
  NAMES[2004] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2004, 7);
  NAMES[2005] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2005, 4);
  NAMES[2006] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2006, 6);
  NAMES[2007] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2007, 7);
  NAMES[2008] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2008, 6);
  NAMES[2009] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2009, 6);
  NAMES[2010] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2010, 6);
  NAMES[2011] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2011, 7);
  NAMES[2012] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2012, 9);
  NAMES[2013] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2013, 7);
  NAMES[2014] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2014, 6);
  NAMES[2015] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2015, 6);
  NAMES[2016] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2016, 3);
  NAMES[2017] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2017, 4);
  NAMES[2018] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2018, 6);
  NAMES[2019] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2019, 5);
  NAMES[2020] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2020, 8);
  NAMES[2021] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2021, 12);
  NAMES[2022] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2022, 14);
  NAMES[2023] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2023, 15);
  NAMES[2024] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2024, 6);
  NAMES[2025] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2025, 5);
  NAMES[2026] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2026, 6);
  NAMES[2027] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2027, 8);
  NAMES[2028] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2028, 11);
  NAMES[2029] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2029, 7);
  NAMES[2030] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2030, 9);
  NAMES[2031] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2031, 7);
  NAMES[2032] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2032, 6);
  NAMES[2033] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2033, 6);
  NAMES[2034] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2034, 5);
  NAMES[2035] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2035, 6);
  NAMES[2036] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2036, 7);
  NAMES[2037] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2037, 5);
  NAMES[2038] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2038, 6);
  NAMES[2039] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2039, 6);
  NAMES[2040] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2040, 4);
  NAMES[2041] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2041, 5);
  NAMES[2042] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2042, 8);
  NAMES[2043] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2043, 5);
  NAMES[2044] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2044, 5);
  NAMES[2045] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2045, 6);
  NAMES[2046] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2046, 6);
  NAMES[2047] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2047, 7);
  NAMES[2048] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2048, 11);
  NAMES[2049] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2049, 9);
  NAMES[2050] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2050, 11);
  NAMES[2051] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2051, 7);
  NAMES[2052] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2052, 6);
  NAMES[2053] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2053, 10);
  NAMES[2054] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2054, 5);
  NAMES[2055] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2055, 7);
  NAMES[2056] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2056, 9);
  NAMES[2057] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2057, 9);
  NAMES[2058] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2058, 16);
  NAMES[2059] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2059, 17);
  NAMES[2060] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2060, 4);
  NAMES[2061] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2061, 6);
  NAMES[2062] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2062, 4);
  NAMES[2063] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2063, 7);
  NAMES[2064] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2064, 6);
  NAMES[2065] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2065, 7);
  NAMES[2066] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2066, 7);
  NAMES[2067] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2067, 5);
  NAMES[2068] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2068, 4);
  NAMES[2069] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2069, 6);
  NAMES[2070] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2070, 5);
  NAMES[2071] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2071, 6);
  NAMES[2072] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2072, 6);
  NAMES[2073] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2073, 5);
  NAMES[2074] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2074, 8);
  NAMES[2075] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2075, 6);
  NAMES[2076] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2076, 7);
  NAMES[2077] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2077, 6);
  NAMES[2078] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2078, 7);
  NAMES[2079] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2079, 7);
  NAMES[2080] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2080, 4);
  NAMES[2081] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2081, 5);
  NAMES[2082] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2082, 3);
  NAMES[2083] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2083, 3);
  NAMES[2084] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2084, 7);
  NAMES[2085] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2085, 5);
  NAMES[2086] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2086, 5);
  NAMES[2087] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2087, 6);
  NAMES[2088] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2088, 5);
  NAMES[2089] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2089, 6);
  NAMES[2090] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2090, 4);
  NAMES[2091] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2091, 6);
  NAMES[2092] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2092, 6);
  NAMES[2093] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2093, 3);
  NAMES[2094] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2094, 6);
  NAMES[2095] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2095, 6);
  NAMES[2096] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2096, 5);
  NAMES[2097] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2097, 5);
  NAMES[2098] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2098, 6);
  NAMES[2099] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2099, 5);
  NAMES[2100] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2100, 7);
  NAMES[2101] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2101, 7);
  NAMES[2102] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2102, 6);
  NAMES[2103] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2103, 6);
  NAMES[2104] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2104, 5);
  NAMES[2105] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2105, 7);
  NAMES[2106] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2106, 7);
  NAMES[2107] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2107, 6);
  NAMES[2108] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2108, 5);
  NAMES[2109] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2109, 7);
  NAMES[2110] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2110, 6);
  NAMES[2111] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2111, 7);
  NAMES[2112] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2112, 5);
  NAMES[2113] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2113, 6);
  NAMES[2114] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2114, 4);
  NAMES[2115] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2115, 3);
  NAMES[2116] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2116, 4);
  NAMES[2117] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2117, 4);
  NAMES[2118] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2118, 5);
  NAMES[2119] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2119, 5);
  NAMES[2120] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2120, 5);
  NAMES[2121] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2121, 5);
  NAMES[2122] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2122, 4);
  NAMES[2123] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2123, 5);
  NAMES[2124] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2124, 7);
  NAMES[2125] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2125, 7);
  NAMES[2126] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2126, 4);
  NAMES[2127] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2127, 5);
  NAMES[2128] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2128, 7);
  NAMES[2129] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2129, 5);
  NAMES[2130] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2130, 4);
  NAMES[2131] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2131, 5);
  NAMES[2132] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2132, 8);
  NAMES[2133] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2133, 5);
  NAMES[2134] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2134, 5);
  NAMES[2135] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2135, 4);
  NAMES[2136] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_2136, 5);
  VALUES = new jArray<PRUnichar,PRInt32>[2137];
  VALUES[0] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_0, 1);
  VALUES[1] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1, 1);
  VALUES[2] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2, 1);
  VALUES[3] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_3, 1);
  VALUES[4] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_4, 1);
  VALUES[5] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_5, 1);
  VALUES[6] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_6, 1);
  VALUES[7] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_7, 1);
  VALUES[8] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_8, 1);
  VALUES[9] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_9, 1);
  VALUES[10] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_10, 2);
  VALUES[11] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_11, 1);
  VALUES[12] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_12, 1);
  VALUES[13] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_13, 1);
  VALUES[14] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_14, 1);
  VALUES[15] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_15, 1);
  VALUES[16] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_16, 1);
  VALUES[17] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_17, 2);
  VALUES[18] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_18, 1);
  VALUES[19] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_19, 1);
  VALUES[20] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_20, 1);
  VALUES[21] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_21, 2);
  VALUES[22] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_22, 1);
  VALUES[23] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_23, 1);
  VALUES[24] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_24, 1);
  VALUES[25] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_25, 1);
  VALUES[26] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_26, 1);
  VALUES[27] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_27, 1);
  VALUES[28] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_28, 1);
  VALUES[29] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_29, 1);
  VALUES[30] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_30, 1);
  VALUES[31] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_31, 1);
  VALUES[32] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_32, 1);
  VALUES[33] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_33, 1);
  VALUES[34] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_34, 2);
  VALUES[35] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_35, 2);
  VALUES[36] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_36, 1);
  VALUES[37] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_37, 1);
  VALUES[38] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_38, 1);
  VALUES[39] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_39, 1);
  VALUES[40] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_40, 1);
  VALUES[41] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_41, 1);
  VALUES[42] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_42, 1);
  VALUES[43] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_43, 1);
  VALUES[44] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_44, 1);
  VALUES[45] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_45, 1);
  VALUES[46] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_46, 1);
  VALUES[47] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_47, 1);
  VALUES[48] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_48, 1);
  VALUES[49] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_49, 1);
  VALUES[50] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_50, 1);
  VALUES[51] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_51, 1);
  VALUES[52] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_52, 1);
  VALUES[53] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_53, 1);
  VALUES[54] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_54, 1);
  VALUES[55] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_55, 1);
  VALUES[56] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_56, 1);
  VALUES[57] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_57, 1);
  VALUES[58] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_58, 1);
  VALUES[59] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_59, 1);
  VALUES[60] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_60, 1);
  VALUES[61] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_61, 1);
  VALUES[62] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_62, 1);
  VALUES[63] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_63, 1);
  VALUES[64] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_64, 1);
  VALUES[65] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_65, 1);
  VALUES[66] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_66, 1);
  VALUES[67] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_67, 1);
  VALUES[68] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_68, 1);
  VALUES[69] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_69, 1);
  VALUES[70] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_70, 1);
  VALUES[71] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_71, 1);
  VALUES[72] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_72, 2);
  VALUES[73] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_73, 1);
  VALUES[74] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_74, 1);
  VALUES[75] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_75, 1);
  VALUES[76] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_76, 1);
  VALUES[77] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_77, 1);
  VALUES[78] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_78, 1);
  VALUES[79] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_79, 1);
  VALUES[80] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_80, 1);
  VALUES[81] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_81, 1);
  VALUES[82] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_82, 1);
  VALUES[83] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_83, 1);
  VALUES[84] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_84, 1);
  VALUES[85] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_85, 1);
  VALUES[86] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_86, 1);
  VALUES[87] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_87, 2);
  VALUES[88] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_88, 1);
  VALUES[89] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_89, 1);
  VALUES[90] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_90, 1);
  VALUES[91] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_91, 1);
  VALUES[92] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_92, 1);
  VALUES[93] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_93, 1);
  VALUES[94] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_94, 1);
  VALUES[95] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_95, 2);
  VALUES[96] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_96, 1);
  VALUES[97] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_97, 1);
  VALUES[98] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_98, 1);
  VALUES[99] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_99, 1);
  VALUES[100] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_100, 1);
  VALUES[101] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_101, 1);
  VALUES[102] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_102, 1);
  VALUES[103] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_103, 1);
  VALUES[104] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_104, 1);
  VALUES[105] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_105, 1);
  VALUES[106] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_106, 1);
  VALUES[107] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_107, 1);
  VALUES[108] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_108, 1);
  VALUES[109] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_109, 1);
  VALUES[110] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_110, 1);
  VALUES[111] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_111, 1);
  VALUES[112] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_112, 1);
  VALUES[113] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_113, 1);
  VALUES[114] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_114, 1);
  VALUES[115] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_115, 1);
  VALUES[116] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_116, 1);
  VALUES[117] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_117, 1);
  VALUES[118] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_118, 1);
  VALUES[119] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_119, 1);
  VALUES[120] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_120, 1);
  VALUES[121] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_121, 1);
  VALUES[122] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_122, 1);
  VALUES[123] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_123, 1);
  VALUES[124] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_124, 1);
  VALUES[125] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_125, 1);
  VALUES[126] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_126, 1);
  VALUES[127] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_127, 2);
  VALUES[128] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_128, 1);
  VALUES[129] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_129, 1);
  VALUES[130] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_130, 1);
  VALUES[131] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_131, 1);
  VALUES[132] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_132, 1);
  VALUES[133] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_133, 1);
  VALUES[134] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_134, 1);
  VALUES[135] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_135, 1);
  VALUES[136] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_136, 1);
  VALUES[137] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_137, 1);
  VALUES[138] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_138, 1);
  VALUES[139] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_139, 2);
  VALUES[140] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_140, 1);
  VALUES[141] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_141, 1);
  VALUES[142] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_142, 1);
  VALUES[143] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_143, 1);
  VALUES[144] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_144, 1);
  VALUES[145] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_145, 1);
  VALUES[146] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_146, 1);
  VALUES[147] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_147, 2);
  VALUES[148] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_148, 1);
  VALUES[149] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_149, 1);
  VALUES[150] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_150, 1);
  VALUES[151] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_151, 1);
  VALUES[152] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_152, 1);
  VALUES[153] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_153, 1);
  VALUES[154] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_154, 1);
  VALUES[155] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_155, 1);
  VALUES[156] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_156, 1);
  VALUES[157] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_157, 1);
  VALUES[158] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_158, 1);
  VALUES[159] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_159, 1);
  VALUES[160] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_160, 2);
  VALUES[161] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_161, 1);
  VALUES[162] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_162, 1);
  VALUES[163] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_163, 2);
  VALUES[164] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_164, 1);
  VALUES[165] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_165, 1);
  VALUES[166] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_166, 1);
  VALUES[167] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_167, 1);
  VALUES[168] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_168, 1);
  VALUES[169] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_169, 1);
  VALUES[170] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_170, 1);
  VALUES[171] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_171, 1);
  VALUES[172] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_172, 1);
  VALUES[173] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_173, 1);
  VALUES[174] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_174, 1);
  VALUES[175] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_175, 1);
  VALUES[176] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_176, 1);
  VALUES[177] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_177, 2);
  VALUES[178] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_178, 1);
  VALUES[179] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_179, 2);
  VALUES[180] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_180, 1);
  VALUES[181] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_181, 1);
  VALUES[182] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_182, 1);
  VALUES[183] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_183, 1);
  VALUES[184] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_184, 1);
  VALUES[185] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_185, 1);
  VALUES[186] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_186, 1);
  VALUES[187] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_187, 2);
  VALUES[188] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_188, 1);
  VALUES[189] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_189, 1);
  VALUES[190] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_190, 1);
  VALUES[191] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_191, 1);
  VALUES[192] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_192, 1);
  VALUES[193] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_193, 1);
  VALUES[194] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_194, 1);
  VALUES[195] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_195, 1);
  VALUES[196] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_196, 1);
  VALUES[197] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_197, 1);
  VALUES[198] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_198, 1);
  VALUES[199] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_199, 1);
  VALUES[200] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_200, 1);
  VALUES[201] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_201, 1);
  VALUES[202] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_202, 1);
  VALUES[203] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_203, 1);
  VALUES[204] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_204, 1);
  VALUES[205] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_205, 1);
  VALUES[206] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_206, 1);
  VALUES[207] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_207, 1);
  VALUES[208] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_208, 1);
  VALUES[209] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_209, 1);
  VALUES[210] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_210, 1);
  VALUES[211] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_211, 1);
  VALUES[212] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_212, 1);
  VALUES[213] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_213, 1);
  VALUES[214] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_214, 1);
  VALUES[215] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_215, 1);
  VALUES[216] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_216, 1);
  VALUES[217] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_217, 1);
  VALUES[218] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_218, 1);
  VALUES[219] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_219, 1);
  VALUES[220] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_220, 1);
  VALUES[221] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_221, 1);
  VALUES[222] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_222, 1);
  VALUES[223] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_223, 2);
  VALUES[224] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_224, 1);
  VALUES[225] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_225, 1);
  VALUES[226] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_226, 1);
  VALUES[227] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_227, 1);
  VALUES[228] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_228, 1);
  VALUES[229] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_229, 1);
  VALUES[230] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_230, 1);
  VALUES[231] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_231, 1);
  VALUES[232] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_232, 2);
  VALUES[233] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_233, 2);
  VALUES[234] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_234, 2);
  VALUES[235] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_235, 1);
  VALUES[236] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_236, 1);
  VALUES[237] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_237, 1);
  VALUES[238] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_238, 1);
  VALUES[239] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_239, 1);
  VALUES[240] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_240, 1);
  VALUES[241] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_241, 1);
  VALUES[242] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_242, 2);
  VALUES[243] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_243, 2);
  VALUES[244] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_244, 2);
  VALUES[245] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_245, 1);
  VALUES[246] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_246, 1);
  VALUES[247] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_247, 1);
  VALUES[248] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_248, 1);
  VALUES[249] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_249, 1);
  VALUES[250] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_250, 1);
  VALUES[251] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_251, 1);
  VALUES[252] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_252, 1);
  VALUES[253] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_253, 1);
  VALUES[254] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_254, 1);
  VALUES[255] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_255, 1);
  VALUES[256] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_256, 1);
  VALUES[257] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_257, 1);
  VALUES[258] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_258, 1);
  VALUES[259] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_259, 1);
  VALUES[260] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_260, 1);
  VALUES[261] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_261, 1);
  VALUES[262] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_262, 1);
  VALUES[263] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_263, 1);
  VALUES[264] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_264, 1);
  VALUES[265] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_265, 1);
  VALUES[266] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_266, 1);
  VALUES[267] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_267, 1);
  VALUES[268] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_268, 1);
  VALUES[269] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_269, 1);
  VALUES[270] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_270, 1);
  VALUES[271] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_271, 1);
  VALUES[272] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_272, 1);
  VALUES[273] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_273, 1);
  VALUES[274] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_274, 1);
  VALUES[275] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_275, 1);
  VALUES[276] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_276, 1);
  VALUES[277] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_277, 1);
  VALUES[278] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_278, 1);
  VALUES[279] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_279, 1);
  VALUES[280] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_280, 1);
  VALUES[281] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_281, 1);
  VALUES[282] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_282, 1);
  VALUES[283] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_283, 1);
  VALUES[284] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_284, 1);
  VALUES[285] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_285, 1);
  VALUES[286] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_286, 1);
  VALUES[287] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_287, 1);
  VALUES[288] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_288, 2);
  VALUES[289] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_289, 1);
  VALUES[290] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_290, 1);
  VALUES[291] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_291, 1);
  VALUES[292] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_292, 1);
  VALUES[293] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_293, 1);
  VALUES[294] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_294, 1);
  VALUES[295] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_295, 1);
  VALUES[296] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_296, 1);
  VALUES[297] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_297, 1);
  VALUES[298] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_298, 2);
  VALUES[299] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_299, 1);
  VALUES[300] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_300, 1);
  VALUES[301] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_301, 1);
  VALUES[302] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_302, 1);
  VALUES[303] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_303, 1);
  VALUES[304] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_304, 1);
  VALUES[305] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_305, 1);
  VALUES[306] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_306, 1);
  VALUES[307] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_307, 1);
  VALUES[308] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_308, 1);
  VALUES[309] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_309, 2);
  VALUES[310] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_310, 1);
  VALUES[311] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_311, 2);
  VALUES[312] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_312, 1);
  VALUES[313] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_313, 1);
  VALUES[314] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_314, 1);
  VALUES[315] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_315, 1);
  VALUES[316] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_316, 1);
  VALUES[317] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_317, 1);
  VALUES[318] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_318, 1);
  VALUES[319] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_319, 1);
  VALUES[320] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_320, 1);
  VALUES[321] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_321, 1);
  VALUES[322] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_322, 1);
  VALUES[323] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_323, 1);
  VALUES[324] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_324, 1);
  VALUES[325] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_325, 1);
  VALUES[326] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_326, 2);
  VALUES[327] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_327, 1);
  VALUES[328] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_328, 1);
  VALUES[329] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_329, 1);
  VALUES[330] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_330, 1);
  VALUES[331] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_331, 1);
  VALUES[332] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_332, 1);
  VALUES[333] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_333, 1);
  VALUES[334] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_334, 1);
  VALUES[335] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_335, 1);
  VALUES[336] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_336, 1);
  VALUES[337] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_337, 1);
  VALUES[338] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_338, 1);
  VALUES[339] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_339, 1);
  VALUES[340] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_340, 1);
  VALUES[341] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_341, 1);
  VALUES[342] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_342, 1);
  VALUES[343] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_343, 1);
  VALUES[344] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_344, 1);
  VALUES[345] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_345, 1);
  VALUES[346] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_346, 1);
  VALUES[347] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_347, 1);
  VALUES[348] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_348, 1);
  VALUES[349] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_349, 1);
  VALUES[350] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_350, 1);
  VALUES[351] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_351, 1);
  VALUES[352] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_352, 1);
  VALUES[353] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_353, 1);
  VALUES[354] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_354, 1);
  VALUES[355] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_355, 1);
  VALUES[356] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_356, 1);
  VALUES[357] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_357, 1);
  VALUES[358] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_358, 1);
  VALUES[359] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_359, 1);
  VALUES[360] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_360, 1);
  VALUES[361] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_361, 1);
  VALUES[362] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_362, 1);
  VALUES[363] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_363, 2);
  VALUES[364] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_364, 1);
  VALUES[365] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_365, 1);
  VALUES[366] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_366, 1);
  VALUES[367] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_367, 1);
  VALUES[368] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_368, 1);
  VALUES[369] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_369, 1);
  VALUES[370] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_370, 1);
  VALUES[371] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_371, 1);
  VALUES[372] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_372, 1);
  VALUES[373] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_373, 1);
  VALUES[374] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_374, 2);
  VALUES[375] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_375, 1);
  VALUES[376] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_376, 1);
  VALUES[377] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_377, 1);
  VALUES[378] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_378, 1);
  VALUES[379] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_379, 1);
  VALUES[380] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_380, 2);
  VALUES[381] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_381, 1);
  VALUES[382] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_382, 1);
  VALUES[383] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_383, 1);
  VALUES[384] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_384, 2);
  VALUES[385] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_385, 1);
  VALUES[386] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_386, 1);
  VALUES[387] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_387, 1);
  VALUES[388] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_388, 1);
  VALUES[389] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_389, 1);
  VALUES[390] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_390, 1);
  VALUES[391] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_391, 1);
  VALUES[392] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_392, 1);
  VALUES[393] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_393, 1);
  VALUES[394] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_394, 1);
  VALUES[395] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_395, 1);
  VALUES[396] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_396, 1);
  VALUES[397] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_397, 1);
  VALUES[398] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_398, 2);
  VALUES[399] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_399, 1);
  VALUES[400] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_400, 1);
  VALUES[401] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_401, 1);
  VALUES[402] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_402, 1);
  VALUES[403] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_403, 1);
  VALUES[404] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_404, 1);
  VALUES[405] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_405, 1);
  VALUES[406] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_406, 1);
  VALUES[407] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_407, 1);
  VALUES[408] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_408, 1);
  VALUES[409] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_409, 1);
  VALUES[410] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_410, 1);
  VALUES[411] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_411, 1);
  VALUES[412] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_412, 1);
  VALUES[413] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_413, 2);
  VALUES[414] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_414, 1);
  VALUES[415] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_415, 1);
  VALUES[416] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_416, 1);
  VALUES[417] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_417, 2);
  VALUES[418] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_418, 1);
  VALUES[419] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_419, 2);
  VALUES[420] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_420, 1);
  VALUES[421] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_421, 1);
  VALUES[422] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_422, 1);
  VALUES[423] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_423, 1);
  VALUES[424] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_424, 1);
  VALUES[425] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_425, 1);
  VALUES[426] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_426, 1);
  VALUES[427] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_427, 1);
  VALUES[428] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_428, 1);
  VALUES[429] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_429, 1);
  VALUES[430] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_430, 1);
  VALUES[431] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_431, 1);
  VALUES[432] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_432, 1);
  VALUES[433] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_433, 1);
  VALUES[434] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_434, 1);
  VALUES[435] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_435, 1);
  VALUES[436] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_436, 1);
  VALUES[437] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_437, 1);
  VALUES[438] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_438, 1);
  VALUES[439] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_439, 1);
  VALUES[440] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_440, 1);
  VALUES[441] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_441, 1);
  VALUES[442] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_442, 1);
  VALUES[443] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_443, 1);
  VALUES[444] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_444, 1);
  VALUES[445] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_445, 1);
  VALUES[446] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_446, 1);
  VALUES[447] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_447, 1);
  VALUES[448] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_448, 1);
  VALUES[449] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_449, 1);
  VALUES[450] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_450, 1);
  VALUES[451] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_451, 1);
  VALUES[452] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_452, 1);
  VALUES[453] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_453, 1);
  VALUES[454] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_454, 1);
  VALUES[455] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_455, 1);
  VALUES[456] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_456, 1);
  VALUES[457] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_457, 1);
  VALUES[458] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_458, 1);
  VALUES[459] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_459, 1);
  VALUES[460] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_460, 1);
  VALUES[461] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_461, 1);
  VALUES[462] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_462, 1);
  VALUES[463] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_463, 1);
  VALUES[464] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_464, 1);
  VALUES[465] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_465, 1);
  VALUES[466] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_466, 1);
  VALUES[467] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_467, 1);
  VALUES[468] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_468, 1);
  VALUES[469] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_469, 1);
  VALUES[470] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_470, 1);
  VALUES[471] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_471, 1);
  VALUES[472] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_472, 1);
  VALUES[473] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_473, 1);
  VALUES[474] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_474, 2);
  VALUES[475] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_475, 1);
  VALUES[476] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_476, 1);
  VALUES[477] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_477, 1);
  VALUES[478] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_478, 1);
  VALUES[479] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_479, 1);
  VALUES[480] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_480, 1);
  VALUES[481] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_481, 2);
  VALUES[482] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_482, 1);
  VALUES[483] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_483, 1);
  VALUES[484] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_484, 1);
  VALUES[485] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_485, 1);
  VALUES[486] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_486, 1);
  VALUES[487] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_487, 1);
  VALUES[488] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_488, 1);
  VALUES[489] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_489, 1);
  VALUES[490] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_490, 2);
  VALUES[491] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_491, 1);
  VALUES[492] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_492, 1);
  VALUES[493] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_493, 1);
  VALUES[494] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_494, 1);
  VALUES[495] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_495, 1);
  VALUES[496] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_496, 1);
  VALUES[497] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_497, 1);
  VALUES[498] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_498, 1);
  VALUES[499] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_499, 1);
  VALUES[500] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_500, 1);
  VALUES[501] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_501, 1);
  VALUES[502] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_502, 1);
  VALUES[503] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_503, 1);
  VALUES[504] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_504, 1);
  VALUES[505] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_505, 1);
  VALUES[506] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_506, 1);
  VALUES[507] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_507, 1);
  VALUES[508] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_508, 1);
  VALUES[509] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_509, 1);
  VALUES[510] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_510, 1);
  VALUES[511] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_511, 1);
  VALUES[512] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_512, 1);
  VALUES[513] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_513, 1);
  VALUES[514] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_514, 1);
  VALUES[515] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_515, 2);
  VALUES[516] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_516, 1);
  VALUES[517] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_517, 1);
  VALUES[518] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_518, 1);
  VALUES[519] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_519, 1);
  VALUES[520] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_520, 1);
  VALUES[521] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_521, 1);
  VALUES[522] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_522, 1);
  VALUES[523] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_523, 2);
  VALUES[524] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_524, 1);
  VALUES[525] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_525, 2);
  VALUES[526] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_526, 1);
  VALUES[527] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_527, 1);
  VALUES[528] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_528, 1);
  VALUES[529] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_529, 1);
  VALUES[530] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_530, 1);
  VALUES[531] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_531, 1);
  VALUES[532] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_532, 1);
  VALUES[533] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_533, 1);
  VALUES[534] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_534, 1);
  VALUES[535] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_535, 1);
  VALUES[536] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_536, 1);
  VALUES[537] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_537, 2);
  VALUES[538] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_538, 1);
  VALUES[539] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_539, 1);
  VALUES[540] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_540, 1);
  VALUES[541] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_541, 1);
  VALUES[542] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_542, 1);
  VALUES[543] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_543, 1);
  VALUES[544] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_544, 1);
  VALUES[545] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_545, 1);
  VALUES[546] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_546, 1);
  VALUES[547] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_547, 1);
  VALUES[548] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_548, 2);
  VALUES[549] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_549, 1);
  VALUES[550] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_550, 1);
  VALUES[551] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_551, 1);
  VALUES[552] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_552, 1);
  VALUES[553] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_553, 1);
  VALUES[554] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_554, 1);
  VALUES[555] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_555, 1);
  VALUES[556] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_556, 1);
  VALUES[557] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_557, 1);
  VALUES[558] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_558, 1);
  VALUES[559] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_559, 1);
  VALUES[560] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_560, 1);
  VALUES[561] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_561, 1);
  VALUES[562] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_562, 1);
  VALUES[563] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_563, 2);
  VALUES[564] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_564, 1);
  VALUES[565] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_565, 1);
  VALUES[566] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_566, 1);
  VALUES[567] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_567, 1);
  VALUES[568] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_568, 1);
  VALUES[569] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_569, 1);
  VALUES[570] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_570, 1);
  VALUES[571] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_571, 1);
  VALUES[572] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_572, 1);
  VALUES[573] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_573, 1);
  VALUES[574] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_574, 1);
  VALUES[575] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_575, 1);
  VALUES[576] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_576, 1);
  VALUES[577] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_577, 1);
  VALUES[578] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_578, 1);
  VALUES[579] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_579, 1);
  VALUES[580] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_580, 2);
  VALUES[581] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_581, 2);
  VALUES[582] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_582, 2);
  VALUES[583] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_583, 1);
  VALUES[584] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_584, 1);
  VALUES[585] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_585, 1);
  VALUES[586] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_586, 2);
  VALUES[587] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_587, 2);
  VALUES[588] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_588, 2);
  VALUES[589] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_589, 2);
  VALUES[590] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_590, 1);
  VALUES[591] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_591, 2);
  VALUES[592] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_592, 2);
  VALUES[593] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_593, 1);
  VALUES[594] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_594, 1);
  VALUES[595] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_595, 1);
  VALUES[596] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_596, 1);
  VALUES[597] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_597, 1);
  VALUES[598] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_598, 1);
  VALUES[599] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_599, 1);
  VALUES[600] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_600, 2);
  VALUES[601] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_601, 2);
  VALUES[602] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_602, 2);
  VALUES[603] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_603, 1);
  VALUES[604] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_604, 1);
  VALUES[605] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_605, 1);
  VALUES[606] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_606, 1);
  VALUES[607] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_607, 1);
  VALUES[608] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_608, 1);
  VALUES[609] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_609, 1);
  VALUES[610] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_610, 1);
  VALUES[611] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_611, 1);
  VALUES[612] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_612, 1);
  VALUES[613] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_613, 2);
  VALUES[614] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_614, 1);
  VALUES[615] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_615, 1);
  VALUES[616] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_616, 1);
  VALUES[617] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_617, 1);
  VALUES[618] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_618, 1);
  VALUES[619] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_619, 1);
  VALUES[620] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_620, 1);
  VALUES[621] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_621, 1);
  VALUES[622] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_622, 1);
  VALUES[623] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_623, 1);
  VALUES[624] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_624, 1);
  VALUES[625] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_625, 1);
  VALUES[626] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_626, 1);
  VALUES[627] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_627, 2);
  VALUES[628] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_628, 1);
  VALUES[629] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_629, 1);
  VALUES[630] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_630, 1);
  VALUES[631] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_631, 1);
  VALUES[632] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_632, 1);
  VALUES[633] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_633, 1);
  VALUES[634] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_634, 1);
  VALUES[635] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_635, 1);
  VALUES[636] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_636, 1);
  VALUES[637] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_637, 1);
  VALUES[638] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_638, 1);
  VALUES[639] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_639, 1);
  VALUES[640] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_640, 1);
  VALUES[641] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_641, 1);
  VALUES[642] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_642, 1);
  VALUES[643] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_643, 1);
  VALUES[644] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_644, 1);
  VALUES[645] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_645, 1);
  VALUES[646] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_646, 1);
  VALUES[647] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_647, 1);
  VALUES[648] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_648, 1);
  VALUES[649] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_649, 1);
  VALUES[650] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_650, 1);
  VALUES[651] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_651, 1);
  VALUES[652] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_652, 1);
  VALUES[653] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_653, 1);
  VALUES[654] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_654, 1);
  VALUES[655] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_655, 1);
  VALUES[656] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_656, 1);
  VALUES[657] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_657, 1);
  VALUES[658] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_658, 1);
  VALUES[659] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_659, 1);
  VALUES[660] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_660, 1);
  VALUES[661] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_661, 2);
  VALUES[662] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_662, 1);
  VALUES[663] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_663, 1);
  VALUES[664] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_664, 1);
  VALUES[665] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_665, 1);
  VALUES[666] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_666, 1);
  VALUES[667] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_667, 1);
  VALUES[668] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_668, 1);
  VALUES[669] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_669, 1);
  VALUES[670] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_670, 1);
  VALUES[671] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_671, 1);
  VALUES[672] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_672, 2);
  VALUES[673] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_673, 1);
  VALUES[674] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_674, 1);
  VALUES[675] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_675, 1);
  VALUES[676] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_676, 1);
  VALUES[677] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_677, 1);
  VALUES[678] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_678, 1);
  VALUES[679] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_679, 1);
  VALUES[680] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_680, 1);
  VALUES[681] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_681, 1);
  VALUES[682] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_682, 1);
  VALUES[683] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_683, 1);
  VALUES[684] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_684, 1);
  VALUES[685] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_685, 1);
  VALUES[686] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_686, 1);
  VALUES[687] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_687, 1);
  VALUES[688] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_688, 1);
  VALUES[689] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_689, 1);
  VALUES[690] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_690, 1);
  VALUES[691] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_691, 1);
  VALUES[692] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_692, 1);
  VALUES[693] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_693, 1);
  VALUES[694] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_694, 1);
  VALUES[695] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_695, 1);
  VALUES[696] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_696, 1);
  VALUES[697] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_697, 1);
  VALUES[698] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_698, 1);
  VALUES[699] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_699, 1);
  VALUES[700] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_700, 1);
  VALUES[701] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_701, 1);
  VALUES[702] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_702, 1);
  VALUES[703] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_703, 1);
  VALUES[704] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_704, 2);
  VALUES[705] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_705, 1);
  VALUES[706] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_706, 1);
  VALUES[707] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_707, 1);
  VALUES[708] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_708, 1);
  VALUES[709] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_709, 1);
  VALUES[710] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_710, 1);
  VALUES[711] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_711, 1);
  VALUES[712] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_712, 1);
  VALUES[713] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_713, 1);
  VALUES[714] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_714, 1);
  VALUES[715] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_715, 1);
  VALUES[716] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_716, 1);
  VALUES[717] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_717, 1);
  VALUES[718] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_718, 1);
  VALUES[719] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_719, 1);
  VALUES[720] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_720, 1);
  VALUES[721] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_721, 1);
  VALUES[722] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_722, 1);
  VALUES[723] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_723, 1);
  VALUES[724] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_724, 1);
  VALUES[725] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_725, 1);
  VALUES[726] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_726, 1);
  VALUES[727] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_727, 1);
  VALUES[728] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_728, 1);
  VALUES[729] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_729, 1);
  VALUES[730] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_730, 1);
  VALUES[731] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_731, 2);
  VALUES[732] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_732, 1);
  VALUES[733] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_733, 1);
  VALUES[734] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_734, 1);
  VALUES[735] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_735, 1);
  VALUES[736] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_736, 1);
  VALUES[737] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_737, 1);
  VALUES[738] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_738, 1);
  VALUES[739] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_739, 1);
  VALUES[740] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_740, 1);
  VALUES[741] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_741, 1);
  VALUES[742] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_742, 1);
  VALUES[743] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_743, 1);
  VALUES[744] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_744, 1);
  VALUES[745] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_745, 1);
  VALUES[746] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_746, 1);
  VALUES[747] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_747, 1);
  VALUES[748] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_748, 1);
  VALUES[749] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_749, 1);
  VALUES[750] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_750, 1);
  VALUES[751] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_751, 1);
  VALUES[752] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_752, 1);
  VALUES[753] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_753, 1);
  VALUES[754] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_754, 1);
  VALUES[755] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_755, 1);
  VALUES[756] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_756, 1);
  VALUES[757] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_757, 1);
  VALUES[758] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_758, 1);
  VALUES[759] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_759, 1);
  VALUES[760] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_760, 1);
  VALUES[761] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_761, 1);
  VALUES[762] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_762, 1);
  VALUES[763] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_763, 1);
  VALUES[764] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_764, 1);
  VALUES[765] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_765, 1);
  VALUES[766] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_766, 1);
  VALUES[767] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_767, 1);
  VALUES[768] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_768, 1);
  VALUES[769] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_769, 1);
  VALUES[770] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_770, 1);
  VALUES[771] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_771, 1);
  VALUES[772] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_772, 1);
  VALUES[773] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_773, 1);
  VALUES[774] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_774, 1);
  VALUES[775] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_775, 1);
  VALUES[776] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_776, 1);
  VALUES[777] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_777, 1);
  VALUES[778] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_778, 1);
  VALUES[779] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_779, 1);
  VALUES[780] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_780, 1);
  VALUES[781] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_781, 1);
  VALUES[782] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_782, 1);
  VALUES[783] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_783, 2);
  VALUES[784] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_784, 1);
  VALUES[785] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_785, 1);
  VALUES[786] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_786, 1);
  VALUES[787] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_787, 1);
  VALUES[788] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_788, 1);
  VALUES[789] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_789, 1);
  VALUES[790] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_790, 1);
  VALUES[791] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_791, 1);
  VALUES[792] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_792, 1);
  VALUES[793] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_793, 1);
  VALUES[794] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_794, 1);
  VALUES[795] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_795, 1);
  VALUES[796] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_796, 1);
  VALUES[797] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_797, 1);
  VALUES[798] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_798, 1);
  VALUES[799] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_799, 1);
  VALUES[800] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_800, 1);
  VALUES[801] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_801, 1);
  VALUES[802] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_802, 1);
  VALUES[803] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_803, 1);
  VALUES[804] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_804, 1);
  VALUES[805] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_805, 1);
  VALUES[806] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_806, 1);
  VALUES[807] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_807, 1);
  VALUES[808] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_808, 1);
  VALUES[809] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_809, 1);
  VALUES[810] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_810, 1);
  VALUES[811] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_811, 1);
  VALUES[812] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_812, 1);
  VALUES[813] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_813, 1);
  VALUES[814] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_814, 1);
  VALUES[815] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_815, 1);
  VALUES[816] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_816, 1);
  VALUES[817] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_817, 1);
  VALUES[818] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_818, 2);
  VALUES[819] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_819, 1);
  VALUES[820] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_820, 1);
  VALUES[821] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_821, 1);
  VALUES[822] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_822, 1);
  VALUES[823] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_823, 1);
  VALUES[824] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_824, 1);
  VALUES[825] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_825, 1);
  VALUES[826] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_826, 1);
  VALUES[827] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_827, 1);
  VALUES[828] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_828, 1);
  VALUES[829] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_829, 1);
  VALUES[830] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_830, 1);
  VALUES[831] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_831, 1);
  VALUES[832] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_832, 1);
  VALUES[833] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_833, 1);
  VALUES[834] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_834, 1);
  VALUES[835] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_835, 1);
  VALUES[836] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_836, 1);
  VALUES[837] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_837, 1);
  VALUES[838] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_838, 1);
  VALUES[839] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_839, 1);
  VALUES[840] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_840, 1);
  VALUES[841] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_841, 1);
  VALUES[842] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_842, 1);
  VALUES[843] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_843, 1);
  VALUES[844] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_844, 1);
  VALUES[845] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_845, 1);
  VALUES[846] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_846, 1);
  VALUES[847] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_847, 1);
  VALUES[848] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_848, 1);
  VALUES[849] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_849, 1);
  VALUES[850] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_850, 1);
  VALUES[851] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_851, 1);
  VALUES[852] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_852, 2);
  VALUES[853] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_853, 1);
  VALUES[854] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_854, 1);
  VALUES[855] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_855, 1);
  VALUES[856] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_856, 1);
  VALUES[857] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_857, 1);
  VALUES[858] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_858, 1);
  VALUES[859] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_859, 2);
  VALUES[860] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_860, 1);
  VALUES[861] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_861, 1);
  VALUES[862] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_862, 1);
  VALUES[863] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_863, 1);
  VALUES[864] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_864, 1);
  VALUES[865] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_865, 1);
  VALUES[866] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_866, 1);
  VALUES[867] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_867, 1);
  VALUES[868] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_868, 1);
  VALUES[869] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_869, 1);
  VALUES[870] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_870, 1);
  VALUES[871] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_871, 1);
  VALUES[872] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_872, 1);
  VALUES[873] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_873, 1);
  VALUES[874] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_874, 1);
  VALUES[875] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_875, 1);
  VALUES[876] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_876, 1);
  VALUES[877] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_877, 1);
  VALUES[878] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_878, 1);
  VALUES[879] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_879, 1);
  VALUES[880] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_880, 1);
  VALUES[881] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_881, 1);
  VALUES[882] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_882, 1);
  VALUES[883] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_883, 1);
  VALUES[884] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_884, 1);
  VALUES[885] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_885, 1);
  VALUES[886] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_886, 1);
  VALUES[887] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_887, 1);
  VALUES[888] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_888, 1);
  VALUES[889] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_889, 1);
  VALUES[890] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_890, 1);
  VALUES[891] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_891, 1);
  VALUES[892] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_892, 1);
  VALUES[893] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_893, 1);
  VALUES[894] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_894, 1);
  VALUES[895] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_895, 1);
  VALUES[896] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_896, 1);
  VALUES[897] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_897, 1);
  VALUES[898] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_898, 1);
  VALUES[899] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_899, 1);
  VALUES[900] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_900, 1);
  VALUES[901] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_901, 1);
  VALUES[902] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_902, 1);
  VALUES[903] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_903, 1);
  VALUES[904] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_904, 1);
  VALUES[905] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_905, 1);
  VALUES[906] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_906, 1);
  VALUES[907] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_907, 1);
  VALUES[908] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_908, 1);
  VALUES[909] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_909, 1);
  VALUES[910] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_910, 1);
  VALUES[911] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_911, 1);
  VALUES[912] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_912, 2);
  VALUES[913] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_913, 1);
  VALUES[914] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_914, 1);
  VALUES[915] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_915, 1);
  VALUES[916] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_916, 1);
  VALUES[917] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_917, 1);
  VALUES[918] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_918, 1);
  VALUES[919] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_919, 1);
  VALUES[920] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_920, 1);
  VALUES[921] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_921, 1);
  VALUES[922] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_922, 1);
  VALUES[923] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_923, 1);
  VALUES[924] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_924, 1);
  VALUES[925] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_925, 1);
  VALUES[926] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_926, 1);
  VALUES[927] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_927, 1);
  VALUES[928] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_928, 1);
  VALUES[929] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_929, 1);
  VALUES[930] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_930, 1);
  VALUES[931] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_931, 2);
  VALUES[932] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_932, 1);
  VALUES[933] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_933, 1);
  VALUES[934] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_934, 1);
  VALUES[935] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_935, 1);
  VALUES[936] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_936, 1);
  VALUES[937] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_937, 1);
  VALUES[938] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_938, 1);
  VALUES[939] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_939, 1);
  VALUES[940] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_940, 1);
  VALUES[941] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_941, 1);
  VALUES[942] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_942, 1);
  VALUES[943] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_943, 1);
  VALUES[944] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_944, 1);
  VALUES[945] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_945, 1);
  VALUES[946] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_946, 2);
  VALUES[947] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_947, 1);
  VALUES[948] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_948, 1);
  VALUES[949] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_949, 1);
  VALUES[950] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_950, 1);
  VALUES[951] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_951, 1);
  VALUES[952] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_952, 1);
  VALUES[953] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_953, 1);
  VALUES[954] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_954, 1);
  VALUES[955] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_955, 1);
  VALUES[956] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_956, 1);
  VALUES[957] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_957, 1);
  VALUES[958] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_958, 1);
  VALUES[959] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_959, 1);
  VALUES[960] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_960, 1);
  VALUES[961] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_961, 1);
  VALUES[962] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_962, 1);
  VALUES[963] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_963, 1);
  VALUES[964] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_964, 1);
  VALUES[965] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_965, 1);
  VALUES[966] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_966, 1);
  VALUES[967] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_967, 1);
  VALUES[968] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_968, 1);
  VALUES[969] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_969, 1);
  VALUES[970] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_970, 1);
  VALUES[971] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_971, 1);
  VALUES[972] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_972, 2);
  VALUES[973] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_973, 1);
  VALUES[974] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_974, 1);
  VALUES[975] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_975, 1);
  VALUES[976] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_976, 1);
  VALUES[977] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_977, 1);
  VALUES[978] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_978, 1);
  VALUES[979] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_979, 1);
  VALUES[980] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_980, 1);
  VALUES[981] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_981, 1);
  VALUES[982] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_982, 1);
  VALUES[983] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_983, 1);
  VALUES[984] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_984, 1);
  VALUES[985] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_985, 1);
  VALUES[986] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_986, 1);
  VALUES[987] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_987, 1);
  VALUES[988] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_988, 1);
  VALUES[989] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_989, 1);
  VALUES[990] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_990, 1);
  VALUES[991] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_991, 1);
  VALUES[992] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_992, 1);
  VALUES[993] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_993, 2);
  VALUES[994] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_994, 1);
  VALUES[995] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_995, 1);
  VALUES[996] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_996, 1);
  VALUES[997] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_997, 1);
  VALUES[998] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_998, 1);
  VALUES[999] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_999, 1);
  VALUES[1000] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1000, 1);
  VALUES[1001] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1001, 1);
  VALUES[1002] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1002, 1);
  VALUES[1003] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1003, 1);
  VALUES[1004] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1004, 1);
  VALUES[1005] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1005, 1);
  VALUES[1006] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1006, 1);
  VALUES[1007] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1007, 1);
  VALUES[1008] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1008, 1);
  VALUES[1009] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1009, 1);
  VALUES[1010] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1010, 1);
  VALUES[1011] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1011, 1);
  VALUES[1012] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1012, 1);
  VALUES[1013] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1013, 1);
  VALUES[1014] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1014, 1);
  VALUES[1015] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1015, 1);
  VALUES[1016] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1016, 1);
  VALUES[1017] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1017, 1);
  VALUES[1018] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1018, 1);
  VALUES[1019] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1019, 1);
  VALUES[1020] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1020, 1);
  VALUES[1021] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1021, 1);
  VALUES[1022] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1022, 1);
  VALUES[1023] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1023, 1);
  VALUES[1024] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1024, 1);
  VALUES[1025] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1025, 1);
  VALUES[1026] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1026, 1);
  VALUES[1027] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1027, 1);
  VALUES[1028] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1028, 1);
  VALUES[1029] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1029, 1);
  VALUES[1030] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1030, 1);
  VALUES[1031] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1031, 2);
  VALUES[1032] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1032, 1);
  VALUES[1033] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1033, 1);
  VALUES[1034] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1034, 1);
  VALUES[1035] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1035, 1);
  VALUES[1036] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1036, 1);
  VALUES[1037] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1037, 2);
  VALUES[1038] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1038, 1);
  VALUES[1039] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1039, 1);
  VALUES[1040] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1040, 1);
  VALUES[1041] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1041, 1);
  VALUES[1042] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1042, 1);
  VALUES[1043] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1043, 1);
  VALUES[1044] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1044, 1);
  VALUES[1045] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1045, 1);
  VALUES[1046] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1046, 1);
  VALUES[1047] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1047, 1);
  VALUES[1048] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1048, 1);
  VALUES[1049] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1049, 1);
  VALUES[1050] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1050, 1);
  VALUES[1051] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1051, 1);
  VALUES[1052] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1052, 1);
  VALUES[1053] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1053, 1);
  VALUES[1054] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1054, 1);
  VALUES[1055] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1055, 1);
  VALUES[1056] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1056, 1);
  VALUES[1057] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1057, 1);
  VALUES[1058] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1058, 1);
  VALUES[1059] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1059, 1);
  VALUES[1060] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1060, 1);
  VALUES[1061] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1061, 1);
  VALUES[1062] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1062, 2);
  VALUES[1063] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1063, 1);
  VALUES[1064] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1064, 1);
  VALUES[1065] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1065, 1);
  VALUES[1066] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1066, 1);
  VALUES[1067] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1067, 1);
  VALUES[1068] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1068, 1);
  VALUES[1069] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1069, 1);
  VALUES[1070] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1070, 1);
  VALUES[1071] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1071, 1);
  VALUES[1072] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1072, 1);
  VALUES[1073] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1073, 1);
  VALUES[1074] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1074, 1);
  VALUES[1075] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1075, 1);
  VALUES[1076] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1076, 1);
  VALUES[1077] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1077, 1);
  VALUES[1078] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1078, 1);
  VALUES[1079] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1079, 1);
  VALUES[1080] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1080, 1);
  VALUES[1081] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1081, 1);
  VALUES[1082] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1082, 1);
  VALUES[1083] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1083, 1);
  VALUES[1084] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1084, 2);
  VALUES[1085] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1085, 1);
  VALUES[1086] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1086, 1);
  VALUES[1087] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1087, 1);
  VALUES[1088] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1088, 1);
  VALUES[1089] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1089, 1);
  VALUES[1090] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1090, 1);
  VALUES[1091] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1091, 1);
  VALUES[1092] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1092, 1);
  VALUES[1093] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1093, 1);
  VALUES[1094] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1094, 1);
  VALUES[1095] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1095, 1);
  VALUES[1096] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1096, 1);
  VALUES[1097] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1097, 1);
  VALUES[1098] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1098, 1);
  VALUES[1099] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1099, 1);
  VALUES[1100] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1100, 2);
  VALUES[1101] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1101, 1);
  VALUES[1102] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1102, 1);
  VALUES[1103] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1103, 1);
  VALUES[1104] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1104, 1);
  VALUES[1105] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1105, 1);
  VALUES[1106] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1106, 1);
  VALUES[1107] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1107, 1);
  VALUES[1108] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1108, 1);
  VALUES[1109] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1109, 1);
  VALUES[1110] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1110, 1);
  VALUES[1111] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1111, 1);
  VALUES[1112] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1112, 1);
  VALUES[1113] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1113, 1);
  VALUES[1114] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1114, 1);
  VALUES[1115] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1115, 1);
  VALUES[1116] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1116, 1);
  VALUES[1117] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1117, 1);
  VALUES[1118] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1118, 1);
  VALUES[1119] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1119, 1);
  VALUES[1120] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1120, 1);
  VALUES[1121] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1121, 1);
  VALUES[1122] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1122, 1);
  VALUES[1123] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1123, 1);
  VALUES[1124] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1124, 1);
  VALUES[1125] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1125, 1);
  VALUES[1126] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1126, 1);
  VALUES[1127] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1127, 1);
  VALUES[1128] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1128, 1);
  VALUES[1129] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1129, 1);
  VALUES[1130] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1130, 1);
  VALUES[1131] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1131, 1);
  VALUES[1132] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1132, 1);
  VALUES[1133] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1133, 1);
  VALUES[1134] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1134, 2);
  VALUES[1135] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1135, 1);
  VALUES[1136] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1136, 1);
  VALUES[1137] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1137, 1);
  VALUES[1138] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1138, 1);
  VALUES[1139] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1139, 1);
  VALUES[1140] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1140, 1);
  VALUES[1141] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1141, 2);
  VALUES[1142] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1142, 1);
  VALUES[1143] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1143, 2);
  VALUES[1144] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1144, 1);
  VALUES[1145] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1145, 1);
  VALUES[1146] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1146, 1);
  VALUES[1147] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1147, 1);
  VALUES[1148] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1148, 1);
  VALUES[1149] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1149, 1);
  VALUES[1150] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1150, 1);
  VALUES[1151] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1151, 1);
  VALUES[1152] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1152, 1);
  VALUES[1153] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1153, 1);
  VALUES[1154] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1154, 1);
  VALUES[1155] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1155, 1);
  VALUES[1156] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1156, 1);
  VALUES[1157] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1157, 1);
  VALUES[1158] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1158, 2);
  VALUES[1159] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1159, 1);
  VALUES[1160] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1160, 1);
  VALUES[1161] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1161, 1);
  VALUES[1162] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1162, 1);
  VALUES[1163] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1163, 1);
  VALUES[1164] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1164, 1);
  VALUES[1165] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1165, 1);
  VALUES[1166] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1166, 1);
  VALUES[1167] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1167, 1);
  VALUES[1168] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1168, 1);
  VALUES[1169] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1169, 1);
  VALUES[1170] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1170, 1);
  VALUES[1171] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1171, 1);
  VALUES[1172] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1172, 1);
  VALUES[1173] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1173, 1);
  VALUES[1174] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1174, 1);
  VALUES[1175] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1175, 1);
  VALUES[1176] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1176, 1);
  VALUES[1177] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1177, 1);
  VALUES[1178] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1178, 1);
  VALUES[1179] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1179, 1);
  VALUES[1180] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1180, 1);
  VALUES[1181] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1181, 1);
  VALUES[1182] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1182, 1);
  VALUES[1183] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1183, 1);
  VALUES[1184] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1184, 1);
  VALUES[1185] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1185, 1);
  VALUES[1186] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1186, 1);
  VALUES[1187] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1187, 2);
  VALUES[1188] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1188, 1);
  VALUES[1189] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1189, 1);
  VALUES[1190] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1190, 1);
  VALUES[1191] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1191, 1);
  VALUES[1192] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1192, 2);
  VALUES[1193] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1193, 1);
  VALUES[1194] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1194, 1);
  VALUES[1195] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1195, 1);
  VALUES[1196] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1196, 1);
  VALUES[1197] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1197, 1);
  VALUES[1198] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1198, 1);
  VALUES[1199] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1199, 1);
  VALUES[1200] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1200, 1);
  VALUES[1201] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1201, 1);
  VALUES[1202] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1202, 1);
  VALUES[1203] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1203, 1);
  VALUES[1204] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1204, 1);
  VALUES[1205] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1205, 1);
  VALUES[1206] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1206, 2);
  VALUES[1207] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1207, 1);
  VALUES[1208] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1208, 2);
  VALUES[1209] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1209, 2);
  VALUES[1210] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1210, 1);
  VALUES[1211] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1211, 1);
  VALUES[1212] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1212, 1);
  VALUES[1213] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1213, 1);
  VALUES[1214] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1214, 1);
  VALUES[1215] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1215, 1);
  VALUES[1216] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1216, 2);
  VALUES[1217] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1217, 1);
  VALUES[1218] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1218, 1);
  VALUES[1219] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1219, 1);
  VALUES[1220] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1220, 2);
  VALUES[1221] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1221, 2);
  VALUES[1222] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1222, 1);
  VALUES[1223] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1223, 1);
  VALUES[1224] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1224, 1);
  VALUES[1225] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1225, 1);
  VALUES[1226] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1226, 1);
  VALUES[1227] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1227, 1);
  VALUES[1228] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1228, 1);
  VALUES[1229] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1229, 1);
  VALUES[1230] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1230, 1);
  VALUES[1231] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1231, 1);
  VALUES[1232] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1232, 1);
  VALUES[1233] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1233, 1);
  VALUES[1234] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1234, 1);
  VALUES[1235] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1235, 1);
  VALUES[1236] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1236, 1);
  VALUES[1237] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1237, 1);
  VALUES[1238] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1238, 1);
  VALUES[1239] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1239, 1);
  VALUES[1240] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1240, 1);
  VALUES[1241] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1241, 1);
  VALUES[1242] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1242, 1);
  VALUES[1243] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1243, 1);
  VALUES[1244] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1244, 1);
  VALUES[1245] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1245, 1);
  VALUES[1246] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1246, 1);
  VALUES[1247] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1247, 1);
  VALUES[1248] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1248, 1);
  VALUES[1249] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1249, 1);
  VALUES[1250] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1250, 1);
  VALUES[1251] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1251, 1);
  VALUES[1252] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1252, 1);
  VALUES[1253] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1253, 1);
  VALUES[1254] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1254, 1);
  VALUES[1255] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1255, 1);
  VALUES[1256] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1256, 1);
  VALUES[1257] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1257, 1);
  VALUES[1258] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1258, 1);
  VALUES[1259] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1259, 1);
  VALUES[1260] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1260, 1);
  VALUES[1261] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1261, 1);
  VALUES[1262] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1262, 1);
  VALUES[1263] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1263, 1);
  VALUES[1264] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1264, 1);
  VALUES[1265] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1265, 1);
  VALUES[1266] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1266, 1);
  VALUES[1267] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1267, 1);
  VALUES[1268] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1268, 1);
  VALUES[1269] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1269, 1);
  VALUES[1270] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1270, 1);
  VALUES[1271] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1271, 1);
  VALUES[1272] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1272, 1);
  VALUES[1273] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1273, 1);
  VALUES[1274] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1274, 1);
  VALUES[1275] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1275, 1);
  VALUES[1276] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1276, 1);
  VALUES[1277] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1277, 1);
  VALUES[1278] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1278, 1);
  VALUES[1279] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1279, 1);
  VALUES[1280] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1280, 1);
  VALUES[1281] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1281, 1);
  VALUES[1282] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1282, 1);
  VALUES[1283] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1283, 1);
  VALUES[1284] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1284, 1);
  VALUES[1285] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1285, 1);
  VALUES[1286] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1286, 1);
  VALUES[1287] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1287, 1);
  VALUES[1288] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1288, 1);
  VALUES[1289] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1289, 1);
  VALUES[1290] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1290, 1);
  VALUES[1291] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1291, 1);
  VALUES[1292] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1292, 1);
  VALUES[1293] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1293, 1);
  VALUES[1294] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1294, 1);
  VALUES[1295] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1295, 1);
  VALUES[1296] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1296, 1);
  VALUES[1297] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1297, 1);
  VALUES[1298] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1298, 2);
  VALUES[1299] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1299, 1);
  VALUES[1300] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1300, 1);
  VALUES[1301] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1301, 1);
  VALUES[1302] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1302, 1);
  VALUES[1303] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1303, 1);
  VALUES[1304] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1304, 1);
  VALUES[1305] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1305, 1);
  VALUES[1306] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1306, 1);
  VALUES[1307] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1307, 1);
  VALUES[1308] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1308, 1);
  VALUES[1309] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1309, 1);
  VALUES[1310] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1310, 1);
  VALUES[1311] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1311, 1);
  VALUES[1312] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1312, 1);
  VALUES[1313] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1313, 1);
  VALUES[1314] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1314, 1);
  VALUES[1315] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1315, 1);
  VALUES[1316] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1316, 1);
  VALUES[1317] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1317, 1);
  VALUES[1318] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1318, 1);
  VALUES[1319] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1319, 1);
  VALUES[1320] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1320, 1);
  VALUES[1321] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1321, 1);
  VALUES[1322] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1322, 1);
  VALUES[1323] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1323, 1);
  VALUES[1324] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1324, 1);
  VALUES[1325] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1325, 1);
  VALUES[1326] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1326, 1);
  VALUES[1327] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1327, 1);
  VALUES[1328] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1328, 1);
  VALUES[1329] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1329, 1);
  VALUES[1330] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1330, 1);
  VALUES[1331] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1331, 2);
  VALUES[1332] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1332, 1);
  VALUES[1333] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1333, 1);
  VALUES[1334] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1334, 1);
  VALUES[1335] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1335, 1);
  VALUES[1336] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1336, 1);
  VALUES[1337] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1337, 1);
  VALUES[1338] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1338, 1);
  VALUES[1339] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1339, 1);
  VALUES[1340] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1340, 1);
  VALUES[1341] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1341, 1);
  VALUES[1342] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1342, 1);
  VALUES[1343] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1343, 1);
  VALUES[1344] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1344, 1);
  VALUES[1345] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1345, 1);
  VALUES[1346] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1346, 1);
  VALUES[1347] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1347, 1);
  VALUES[1348] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1348, 2);
  VALUES[1349] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1349, 1);
  VALUES[1350] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1350, 1);
  VALUES[1351] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1351, 1);
  VALUES[1352] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1352, 1);
  VALUES[1353] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1353, 1);
  VALUES[1354] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1354, 1);
  VALUES[1355] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1355, 1);
  VALUES[1356] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1356, 1);
  VALUES[1357] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1357, 1);
  VALUES[1358] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1358, 1);
  VALUES[1359] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1359, 1);
  VALUES[1360] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1360, 1);
  VALUES[1361] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1361, 1);
  VALUES[1362] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1362, 1);
  VALUES[1363] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1363, 1);
  VALUES[1364] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1364, 1);
  VALUES[1365] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1365, 1);
  VALUES[1366] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1366, 1);
  VALUES[1367] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1367, 1);
  VALUES[1368] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1368, 1);
  VALUES[1369] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1369, 1);
  VALUES[1370] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1370, 1);
  VALUES[1371] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1371, 1);
  VALUES[1372] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1372, 1);
  VALUES[1373] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1373, 1);
  VALUES[1374] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1374, 1);
  VALUES[1375] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1375, 1);
  VALUES[1376] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1376, 1);
  VALUES[1377] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1377, 1);
  VALUES[1378] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1378, 1);
  VALUES[1379] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1379, 1);
  VALUES[1380] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1380, 1);
  VALUES[1381] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1381, 1);
  VALUES[1382] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1382, 1);
  VALUES[1383] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1383, 1);
  VALUES[1384] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1384, 1);
  VALUES[1385] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1385, 1);
  VALUES[1386] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1386, 1);
  VALUES[1387] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1387, 1);
  VALUES[1388] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1388, 2);
  VALUES[1389] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1389, 1);
  VALUES[1390] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1390, 1);
  VALUES[1391] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1391, 1);
  VALUES[1392] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1392, 1);
  VALUES[1393] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1393, 1);
  VALUES[1394] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1394, 1);
  VALUES[1395] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1395, 1);
  VALUES[1396] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1396, 1);
  VALUES[1397] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1397, 1);
  VALUES[1398] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1398, 1);
  VALUES[1399] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1399, 1);
  VALUES[1400] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1400, 1);
  VALUES[1401] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1401, 1);
  VALUES[1402] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1402, 1);
  VALUES[1403] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1403, 1);
  VALUES[1404] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1404, 1);
  VALUES[1405] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1405, 2);
  VALUES[1406] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1406, 1);
  VALUES[1407] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1407, 2);
  VALUES[1408] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1408, 1);
  VALUES[1409] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1409, 1);
  VALUES[1410] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1410, 1);
  VALUES[1411] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1411, 1);
  VALUES[1412] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1412, 1);
  VALUES[1413] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1413, 1);
  VALUES[1414] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1414, 1);
  VALUES[1415] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1415, 1);
  VALUES[1416] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1416, 1);
  VALUES[1417] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1417, 1);
  VALUES[1418] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1418, 1);
  VALUES[1419] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1419, 1);
  VALUES[1420] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1420, 1);
  VALUES[1421] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1421, 1);
  VALUES[1422] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1422, 1);
  VALUES[1423] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1423, 1);
  VALUES[1424] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1424, 1);
  VALUES[1425] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1425, 1);
  VALUES[1426] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1426, 1);
  VALUES[1427] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1427, 1);
  VALUES[1428] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1428, 1);
  VALUES[1429] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1429, 1);
  VALUES[1430] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1430, 1);
  VALUES[1431] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1431, 1);
  VALUES[1432] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1432, 1);
  VALUES[1433] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1433, 1);
  VALUES[1434] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1434, 1);
  VALUES[1435] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1435, 1);
  VALUES[1436] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1436, 1);
  VALUES[1437] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1437, 1);
  VALUES[1438] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1438, 1);
  VALUES[1439] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1439, 1);
  VALUES[1440] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1440, 1);
  VALUES[1441] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1441, 1);
  VALUES[1442] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1442, 1);
  VALUES[1443] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1443, 2);
  VALUES[1444] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1444, 1);
  VALUES[1445] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1445, 1);
  VALUES[1446] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1446, 1);
  VALUES[1447] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1447, 1);
  VALUES[1448] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1448, 1);
  VALUES[1449] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1449, 1);
  VALUES[1450] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1450, 1);
  VALUES[1451] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1451, 1);
  VALUES[1452] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1452, 1);
  VALUES[1453] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1453, 1);
  VALUES[1454] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1454, 1);
  VALUES[1455] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1455, 1);
  VALUES[1456] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1456, 1);
  VALUES[1457] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1457, 1);
  VALUES[1458] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1458, 1);
  VALUES[1459] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1459, 1);
  VALUES[1460] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1460, 1);
  VALUES[1461] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1461, 1);
  VALUES[1462] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1462, 1);
  VALUES[1463] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1463, 1);
  VALUES[1464] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1464, 1);
  VALUES[1465] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1465, 1);
  VALUES[1466] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1466, 1);
  VALUES[1467] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1467, 1);
  VALUES[1468] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1468, 1);
  VALUES[1469] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1469, 1);
  VALUES[1470] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1470, 2);
  VALUES[1471] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1471, 1);
  VALUES[1472] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1472, 1);
  VALUES[1473] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1473, 1);
  VALUES[1474] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1474, 1);
  VALUES[1475] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1475, 1);
  VALUES[1476] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1476, 1);
  VALUES[1477] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1477, 1);
  VALUES[1478] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1478, 1);
  VALUES[1479] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1479, 1);
  VALUES[1480] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1480, 1);
  VALUES[1481] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1481, 1);
  VALUES[1482] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1482, 1);
  VALUES[1483] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1483, 1);
  VALUES[1484] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1484, 1);
  VALUES[1485] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1485, 1);
  VALUES[1486] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1486, 1);
  VALUES[1487] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1487, 1);
  VALUES[1488] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1488, 1);
  VALUES[1489] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1489, 1);
  VALUES[1490] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1490, 1);
  VALUES[1491] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1491, 1);
  VALUES[1492] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1492, 1);
  VALUES[1493] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1493, 1);
  VALUES[1494] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1494, 2);
  VALUES[1495] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1495, 1);
  VALUES[1496] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1496, 1);
  VALUES[1497] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1497, 1);
  VALUES[1498] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1498, 1);
  VALUES[1499] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1499, 1);
  VALUES[1500] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1500, 1);
  VALUES[1501] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1501, 1);
  VALUES[1502] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1502, 1);
  VALUES[1503] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1503, 1);
  VALUES[1504] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1504, 1);
  VALUES[1505] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1505, 1);
  VALUES[1506] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1506, 1);
  VALUES[1507] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1507, 1);
  VALUES[1508] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1508, 1);
  VALUES[1509] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1509, 1);
  VALUES[1510] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1510, 1);
  VALUES[1511] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1511, 1);
  VALUES[1512] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1512, 1);
  VALUES[1513] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1513, 1);
  VALUES[1514] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1514, 1);
  VALUES[1515] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1515, 1);
  VALUES[1516] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1516, 1);
  VALUES[1517] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1517, 1);
  VALUES[1518] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1518, 1);
  VALUES[1519] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1519, 1);
  VALUES[1520] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1520, 1);
  VALUES[1521] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1521, 1);
  VALUES[1522] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1522, 1);
  VALUES[1523] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1523, 1);
  VALUES[1524] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1524, 1);
  VALUES[1525] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1525, 1);
  VALUES[1526] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1526, 1);
  VALUES[1527] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1527, 1);
  VALUES[1528] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1528, 1);
  VALUES[1529] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1529, 1);
  VALUES[1530] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1530, 1);
  VALUES[1531] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1531, 1);
  VALUES[1532] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1532, 1);
  VALUES[1533] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1533, 1);
  VALUES[1534] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1534, 1);
  VALUES[1535] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1535, 1);
  VALUES[1536] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1536, 1);
  VALUES[1537] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1537, 1);
  VALUES[1538] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1538, 1);
  VALUES[1539] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1539, 1);
  VALUES[1540] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1540, 1);
  VALUES[1541] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1541, 1);
  VALUES[1542] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1542, 1);
  VALUES[1543] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1543, 1);
  VALUES[1544] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1544, 1);
  VALUES[1545] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1545, 1);
  VALUES[1546] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1546, 1);
  VALUES[1547] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1547, 1);
  VALUES[1548] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1548, 1);
  VALUES[1549] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1549, 2);
  VALUES[1550] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1550, 1);
  VALUES[1551] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1551, 1);
  VALUES[1552] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1552, 1);
  VALUES[1553] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1553, 1);
  VALUES[1554] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1554, 1);
  VALUES[1555] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1555, 1);
  VALUES[1556] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1556, 1);
  VALUES[1557] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1557, 1);
  VALUES[1558] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1558, 1);
  VALUES[1559] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1559, 1);
  VALUES[1560] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1560, 1);
  VALUES[1561] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1561, 1);
  VALUES[1562] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1562, 1);
  VALUES[1563] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1563, 1);
  VALUES[1564] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1564, 1);
  VALUES[1565] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1565, 1);
  VALUES[1566] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1566, 1);
  VALUES[1567] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1567, 2);
  VALUES[1568] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1568, 1);
  VALUES[1569] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1569, 1);
  VALUES[1570] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1570, 1);
  VALUES[1571] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1571, 1);
  VALUES[1572] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1572, 1);
  VALUES[1573] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1573, 1);
  VALUES[1574] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1574, 1);
  VALUES[1575] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1575, 1);
  VALUES[1576] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1576, 1);
  VALUES[1577] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1577, 1);
  VALUES[1578] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1578, 1);
  VALUES[1579] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1579, 1);
  VALUES[1580] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1580, 1);
  VALUES[1581] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1581, 1);
  VALUES[1582] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1582, 1);
  VALUES[1583] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1583, 1);
  VALUES[1584] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1584, 1);
  VALUES[1585] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1585, 1);
  VALUES[1586] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1586, 1);
  VALUES[1587] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1587, 1);
  VALUES[1588] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1588, 1);
  VALUES[1589] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1589, 1);
  VALUES[1590] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1590, 1);
  VALUES[1591] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1591, 1);
  VALUES[1592] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1592, 1);
  VALUES[1593] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1593, 1);
  VALUES[1594] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1594, 1);
  VALUES[1595] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1595, 1);
  VALUES[1596] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1596, 1);
  VALUES[1597] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1597, 1);
  VALUES[1598] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1598, 1);
  VALUES[1599] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1599, 1);
  VALUES[1600] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1600, 1);
  VALUES[1601] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1601, 1);
  VALUES[1602] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1602, 1);
  VALUES[1603] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1603, 1);
  VALUES[1604] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1604, 1);
  VALUES[1605] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1605, 1);
  VALUES[1606] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1606, 1);
  VALUES[1607] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1607, 1);
  VALUES[1608] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1608, 2);
  VALUES[1609] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1609, 1);
  VALUES[1610] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1610, 1);
  VALUES[1611] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1611, 1);
  VALUES[1612] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1612, 1);
  VALUES[1613] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1613, 1);
  VALUES[1614] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1614, 1);
  VALUES[1615] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1615, 1);
  VALUES[1616] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1616, 1);
  VALUES[1617] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1617, 1);
  VALUES[1618] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1618, 1);
  VALUES[1619] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1619, 1);
  VALUES[1620] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1620, 1);
  VALUES[1621] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1621, 1);
  VALUES[1622] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1622, 1);
  VALUES[1623] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1623, 1);
  VALUES[1624] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1624, 1);
  VALUES[1625] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1625, 1);
  VALUES[1626] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1626, 1);
  VALUES[1627] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1627, 1);
  VALUES[1628] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1628, 1);
  VALUES[1629] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1629, 1);
  VALUES[1630] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1630, 1);
  VALUES[1631] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1631, 1);
  VALUES[1632] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1632, 2);
  VALUES[1633] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1633, 1);
  VALUES[1634] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1634, 1);
  VALUES[1635] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1635, 1);
  VALUES[1636] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1636, 1);
  VALUES[1637] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1637, 1);
  VALUES[1638] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1638, 1);
  VALUES[1639] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1639, 1);
  VALUES[1640] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1640, 1);
  VALUES[1641] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1641, 1);
  VALUES[1642] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1642, 1);
  VALUES[1643] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1643, 1);
  VALUES[1644] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1644, 1);
  VALUES[1645] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1645, 1);
  VALUES[1646] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1646, 1);
  VALUES[1647] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1647, 1);
  VALUES[1648] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1648, 1);
  VALUES[1649] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1649, 1);
  VALUES[1650] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1650, 1);
  VALUES[1651] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1651, 1);
  VALUES[1652] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1652, 1);
  VALUES[1653] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1653, 1);
  VALUES[1654] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1654, 1);
  VALUES[1655] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1655, 1);
  VALUES[1656] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1656, 1);
  VALUES[1657] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1657, 1);
  VALUES[1658] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1658, 1);
  VALUES[1659] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1659, 1);
  VALUES[1660] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1660, 1);
  VALUES[1661] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1661, 2);
  VALUES[1662] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1662, 1);
  VALUES[1663] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1663, 1);
  VALUES[1664] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1664, 2);
  VALUES[1665] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1665, 1);
  VALUES[1666] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1666, 2);
  VALUES[1667] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1667, 1);
  VALUES[1668] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1668, 2);
  VALUES[1669] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1669, 1);
  VALUES[1670] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1670, 1);
  VALUES[1671] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1671, 1);
  VALUES[1672] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1672, 1);
  VALUES[1673] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1673, 1);
  VALUES[1674] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1674, 1);
  VALUES[1675] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1675, 1);
  VALUES[1676] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1676, 1);
  VALUES[1677] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1677, 1);
  VALUES[1678] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1678, 1);
  VALUES[1679] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1679, 1);
  VALUES[1680] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1680, 1);
  VALUES[1681] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1681, 1);
  VALUES[1682] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1682, 1);
  VALUES[1683] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1683, 1);
  VALUES[1684] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1684, 1);
  VALUES[1685] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1685, 1);
  VALUES[1686] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1686, 1);
  VALUES[1687] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1687, 1);
  VALUES[1688] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1688, 1);
  VALUES[1689] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1689, 1);
  VALUES[1690] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1690, 1);
  VALUES[1691] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1691, 1);
  VALUES[1692] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1692, 1);
  VALUES[1693] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1693, 1);
  VALUES[1694] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1694, 1);
  VALUES[1695] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1695, 1);
  VALUES[1696] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1696, 1);
  VALUES[1697] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1697, 1);
  VALUES[1698] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1698, 1);
  VALUES[1699] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1699, 1);
  VALUES[1700] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1700, 1);
  VALUES[1701] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1701, 1);
  VALUES[1702] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1702, 1);
  VALUES[1703] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1703, 1);
  VALUES[1704] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1704, 1);
  VALUES[1705] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1705, 1);
  VALUES[1706] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1706, 1);
  VALUES[1707] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1707, 1);
  VALUES[1708] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1708, 1);
  VALUES[1709] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1709, 1);
  VALUES[1710] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1710, 1);
  VALUES[1711] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1711, 1);
  VALUES[1712] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1712, 1);
  VALUES[1713] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1713, 1);
  VALUES[1714] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1714, 1);
  VALUES[1715] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1715, 1);
  VALUES[1716] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1716, 1);
  VALUES[1717] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1717, 1);
  VALUES[1718] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1718, 1);
  VALUES[1719] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1719, 1);
  VALUES[1720] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1720, 1);
  VALUES[1721] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1721, 1);
  VALUES[1722] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1722, 1);
  VALUES[1723] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1723, 1);
  VALUES[1724] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1724, 1);
  VALUES[1725] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1725, 1);
  VALUES[1726] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1726, 1);
  VALUES[1727] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1727, 1);
  VALUES[1728] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1728, 1);
  VALUES[1729] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1729, 1);
  VALUES[1730] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1730, 1);
  VALUES[1731] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1731, 2);
  VALUES[1732] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1732, 1);
  VALUES[1733] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1733, 1);
  VALUES[1734] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1734, 1);
  VALUES[1735] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1735, 1);
  VALUES[1736] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1736, 1);
  VALUES[1737] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1737, 1);
  VALUES[1738] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1738, 1);
  VALUES[1739] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1739, 1);
  VALUES[1740] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1740, 1);
  VALUES[1741] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1741, 1);
  VALUES[1742] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1742, 1);
  VALUES[1743] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1743, 1);
  VALUES[1744] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1744, 1);
  VALUES[1745] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1745, 1);
  VALUES[1746] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1746, 1);
  VALUES[1747] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1747, 1);
  VALUES[1748] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1748, 1);
  VALUES[1749] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1749, 1);
  VALUES[1750] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1750, 1);
  VALUES[1751] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1751, 1);
  VALUES[1752] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1752, 1);
  VALUES[1753] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1753, 1);
  VALUES[1754] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1754, 1);
  VALUES[1755] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1755, 1);
  VALUES[1756] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1756, 1);
  VALUES[1757] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1757, 1);
  VALUES[1758] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1758, 2);
  VALUES[1759] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1759, 1);
  VALUES[1760] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1760, 1);
  VALUES[1761] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1761, 1);
  VALUES[1762] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1762, 1);
  VALUES[1763] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1763, 1);
  VALUES[1764] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1764, 1);
  VALUES[1765] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1765, 1);
  VALUES[1766] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1766, 2);
  VALUES[1767] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1767, 1);
  VALUES[1768] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1768, 1);
  VALUES[1769] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1769, 1);
  VALUES[1770] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1770, 1);
  VALUES[1771] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1771, 1);
  VALUES[1772] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1772, 1);
  VALUES[1773] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1773, 1);
  VALUES[1774] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1774, 1);
  VALUES[1775] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1775, 1);
  VALUES[1776] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1776, 1);
  VALUES[1777] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1777, 1);
  VALUES[1778] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1778, 1);
  VALUES[1779] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1779, 1);
  VALUES[1780] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1780, 1);
  VALUES[1781] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1781, 1);
  VALUES[1782] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1782, 1);
  VALUES[1783] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1783, 1);
  VALUES[1784] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1784, 1);
  VALUES[1785] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1785, 1);
  VALUES[1786] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1786, 1);
  VALUES[1787] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1787, 1);
  VALUES[1788] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1788, 1);
  VALUES[1789] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1789, 1);
  VALUES[1790] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1790, 1);
  VALUES[1791] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1791, 1);
  VALUES[1792] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1792, 1);
  VALUES[1793] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1793, 1);
  VALUES[1794] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1794, 1);
  VALUES[1795] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1795, 1);
  VALUES[1796] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1796, 1);
  VALUES[1797] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1797, 1);
  VALUES[1798] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1798, 1);
  VALUES[1799] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1799, 1);
  VALUES[1800] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1800, 1);
  VALUES[1801] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1801, 1);
  VALUES[1802] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1802, 1);
  VALUES[1803] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1803, 1);
  VALUES[1804] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1804, 1);
  VALUES[1805] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1805, 1);
  VALUES[1806] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1806, 1);
  VALUES[1807] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1807, 1);
  VALUES[1808] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1808, 1);
  VALUES[1809] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1809, 2);
  VALUES[1810] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1810, 1);
  VALUES[1811] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1811, 1);
  VALUES[1812] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1812, 1);
  VALUES[1813] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1813, 1);
  VALUES[1814] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1814, 1);
  VALUES[1815] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1815, 1);
  VALUES[1816] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1816, 1);
  VALUES[1817] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1817, 1);
  VALUES[1818] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1818, 1);
  VALUES[1819] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1819, 1);
  VALUES[1820] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1820, 1);
  VALUES[1821] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1821, 1);
  VALUES[1822] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1822, 1);
  VALUES[1823] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1823, 1);
  VALUES[1824] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1824, 1);
  VALUES[1825] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1825, 1);
  VALUES[1826] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1826, 1);
  VALUES[1827] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1827, 1);
  VALUES[1828] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1828, 1);
  VALUES[1829] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1829, 1);
  VALUES[1830] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1830, 1);
  VALUES[1831] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1831, 1);
  VALUES[1832] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1832, 1);
  VALUES[1833] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1833, 1);
  VALUES[1834] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1834, 1);
  VALUES[1835] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1835, 1);
  VALUES[1836] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1836, 1);
  VALUES[1837] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1837, 1);
  VALUES[1838] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1838, 1);
  VALUES[1839] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1839, 1);
  VALUES[1840] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1840, 1);
  VALUES[1841] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1841, 1);
  VALUES[1842] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1842, 1);
  VALUES[1843] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1843, 1);
  VALUES[1844] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1844, 2);
  VALUES[1845] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1845, 1);
  VALUES[1846] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1846, 1);
  VALUES[1847] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1847, 1);
  VALUES[1848] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1848, 1);
  VALUES[1849] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1849, 1);
  VALUES[1850] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1850, 1);
  VALUES[1851] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1851, 1);
  VALUES[1852] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1852, 1);
  VALUES[1853] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1853, 1);
  VALUES[1854] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1854, 1);
  VALUES[1855] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1855, 1);
  VALUES[1856] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1856, 1);
  VALUES[1857] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1857, 1);
  VALUES[1858] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1858, 1);
  VALUES[1859] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1859, 1);
  VALUES[1860] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1860, 1);
  VALUES[1861] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1861, 1);
  VALUES[1862] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1862, 1);
  VALUES[1863] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1863, 2);
  VALUES[1864] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1864, 1);
  VALUES[1865] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1865, 1);
  VALUES[1866] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1866, 1);
  VALUES[1867] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1867, 1);
  VALUES[1868] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1868, 1);
  VALUES[1869] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1869, 1);
  VALUES[1870] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1870, 1);
  VALUES[1871] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1871, 1);
  VALUES[1872] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1872, 1);
  VALUES[1873] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1873, 1);
  VALUES[1874] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1874, 1);
  VALUES[1875] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1875, 1);
  VALUES[1876] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1876, 1);
  VALUES[1877] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1877, 1);
  VALUES[1878] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1878, 1);
  VALUES[1879] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1879, 1);
  VALUES[1880] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1880, 1);
  VALUES[1881] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1881, 1);
  VALUES[1882] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1882, 1);
  VALUES[1883] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1883, 1);
  VALUES[1884] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1884, 1);
  VALUES[1885] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1885, 1);
  VALUES[1886] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1886, 1);
  VALUES[1887] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1887, 1);
  VALUES[1888] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1888, 1);
  VALUES[1889] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1889, 1);
  VALUES[1890] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1890, 1);
  VALUES[1891] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1891, 1);
  VALUES[1892] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1892, 1);
  VALUES[1893] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1893, 1);
  VALUES[1894] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1894, 1);
  VALUES[1895] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1895, 1);
  VALUES[1896] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1896, 1);
  VALUES[1897] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1897, 1);
  VALUES[1898] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1898, 1);
  VALUES[1899] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1899, 1);
  VALUES[1900] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1900, 1);
  VALUES[1901] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1901, 1);
  VALUES[1902] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1902, 1);
  VALUES[1903] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1903, 1);
  VALUES[1904] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1904, 1);
  VALUES[1905] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1905, 1);
  VALUES[1906] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1906, 1);
  VALUES[1907] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1907, 1);
  VALUES[1908] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1908, 1);
  VALUES[1909] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1909, 1);
  VALUES[1910] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1910, 1);
  VALUES[1911] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1911, 1);
  VALUES[1912] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1912, 1);
  VALUES[1913] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1913, 1);
  VALUES[1914] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1914, 1);
  VALUES[1915] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1915, 1);
  VALUES[1916] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1916, 1);
  VALUES[1917] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1917, 1);
  VALUES[1918] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1918, 1);
  VALUES[1919] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1919, 1);
  VALUES[1920] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1920, 1);
  VALUES[1921] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1921, 1);
  VALUES[1922] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1922, 1);
  VALUES[1923] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1923, 1);
  VALUES[1924] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1924, 1);
  VALUES[1925] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1925, 1);
  VALUES[1926] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1926, 1);
  VALUES[1927] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1927, 1);
  VALUES[1928] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1928, 1);
  VALUES[1929] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1929, 1);
  VALUES[1930] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1930, 1);
  VALUES[1931] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1931, 1);
  VALUES[1932] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1932, 1);
  VALUES[1933] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1933, 1);
  VALUES[1934] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1934, 1);
  VALUES[1935] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1935, 1);
  VALUES[1936] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1936, 1);
  VALUES[1937] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1937, 1);
  VALUES[1938] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1938, 1);
  VALUES[1939] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1939, 1);
  VALUES[1940] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1940, 1);
  VALUES[1941] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1941, 2);
  VALUES[1942] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1942, 1);
  VALUES[1943] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1943, 1);
  VALUES[1944] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1944, 1);
  VALUES[1945] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1945, 1);
  VALUES[1946] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1946, 1);
  VALUES[1947] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1947, 1);
  VALUES[1948] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1948, 1);
  VALUES[1949] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1949, 1);
  VALUES[1950] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1950, 1);
  VALUES[1951] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1951, 1);
  VALUES[1952] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1952, 1);
  VALUES[1953] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1953, 1);
  VALUES[1954] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1954, 1);
  VALUES[1955] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1955, 1);
  VALUES[1956] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1956, 1);
  VALUES[1957] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1957, 1);
  VALUES[1958] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1958, 1);
  VALUES[1959] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1959, 1);
  VALUES[1960] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1960, 1);
  VALUES[1961] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1961, 1);
  VALUES[1962] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1962, 1);
  VALUES[1963] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1963, 1);
  VALUES[1964] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1964, 1);
  VALUES[1965] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1965, 2);
  VALUES[1966] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1966, 1);
  VALUES[1967] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1967, 1);
  VALUES[1968] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1968, 1);
  VALUES[1969] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1969, 1);
  VALUES[1970] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1970, 1);
  VALUES[1971] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1971, 1);
  VALUES[1972] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1972, 1);
  VALUES[1973] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1973, 1);
  VALUES[1974] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1974, 1);
  VALUES[1975] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1975, 1);
  VALUES[1976] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1976, 1);
  VALUES[1977] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1977, 1);
  VALUES[1978] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1978, 1);
  VALUES[1979] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1979, 1);
  VALUES[1980] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1980, 1);
  VALUES[1981] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1981, 1);
  VALUES[1982] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1982, 1);
  VALUES[1983] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1983, 1);
  VALUES[1984] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1984, 2);
  VALUES[1985] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1985, 1);
  VALUES[1986] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1986, 1);
  VALUES[1987] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1987, 1);
  VALUES[1988] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1988, 1);
  VALUES[1989] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1989, 1);
  VALUES[1990] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1990, 1);
  VALUES[1991] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1991, 1);
  VALUES[1992] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1992, 1);
  VALUES[1993] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1993, 1);
  VALUES[1994] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1994, 1);
  VALUES[1995] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1995, 1);
  VALUES[1996] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1996, 1);
  VALUES[1997] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1997, 1);
  VALUES[1998] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1998, 1);
  VALUES[1999] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_1999, 1);
  VALUES[2000] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2000, 1);
  VALUES[2001] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2001, 1);
  VALUES[2002] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2002, 1);
  VALUES[2003] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2003, 1);
  VALUES[2004] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2004, 1);
  VALUES[2005] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2005, 2);
  VALUES[2006] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2006, 1);
  VALUES[2007] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2007, 1);
  VALUES[2008] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2008, 1);
  VALUES[2009] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2009, 1);
  VALUES[2010] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2010, 1);
  VALUES[2011] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2011, 1);
  VALUES[2012] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2012, 1);
  VALUES[2013] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2013, 1);
  VALUES[2014] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2014, 1);
  VALUES[2015] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2015, 1);
  VALUES[2016] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2016, 1);
  VALUES[2017] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2017, 1);
  VALUES[2018] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2018, 1);
  VALUES[2019] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2019, 2);
  VALUES[2020] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2020, 1);
  VALUES[2021] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2021, 1);
  VALUES[2022] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2022, 1);
  VALUES[2023] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2023, 1);
  VALUES[2024] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2024, 1);
  VALUES[2025] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2025, 1);
  VALUES[2026] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2026, 1);
  VALUES[2027] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2027, 1);
  VALUES[2028] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2028, 1);
  VALUES[2029] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2029, 1);
  VALUES[2030] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2030, 1);
  VALUES[2031] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2031, 1);
  VALUES[2032] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2032, 1);
  VALUES[2033] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2033, 1);
  VALUES[2034] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2034, 2);
  VALUES[2035] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2035, 1);
  VALUES[2036] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2036, 1);
  VALUES[2037] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2037, 1);
  VALUES[2038] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2038, 1);
  VALUES[2039] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2039, 1);
  VALUES[2040] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2040, 1);
  VALUES[2041] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2041, 1);
  VALUES[2042] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2042, 1);
  VALUES[2043] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2043, 1);
  VALUES[2044] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2044, 1);
  VALUES[2045] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2045, 1);
  VALUES[2046] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2046, 1);
  VALUES[2047] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2047, 1);
  VALUES[2048] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2048, 1);
  VALUES[2049] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2049, 1);
  VALUES[2050] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2050, 1);
  VALUES[2051] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2051, 1);
  VALUES[2052] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2052, 1);
  VALUES[2053] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2053, 1);
  VALUES[2054] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2054, 1);
  VALUES[2055] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2055, 1);
  VALUES[2056] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2056, 1);
  VALUES[2057] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2057, 1);
  VALUES[2058] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2058, 1);
  VALUES[2059] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2059, 1);
  VALUES[2060] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2060, 1);
  VALUES[2061] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2061, 1);
  VALUES[2062] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2062, 1);
  VALUES[2063] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2063, 1);
  VALUES[2064] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2064, 1);
  VALUES[2065] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2065, 1);
  VALUES[2066] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2066, 1);
  VALUES[2067] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2067, 1);
  VALUES[2068] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2068, 2);
  VALUES[2069] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2069, 1);
  VALUES[2070] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2070, 2);
  VALUES[2071] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2071, 1);
  VALUES[2072] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2072, 1);
  VALUES[2073] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2073, 2);
  VALUES[2074] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2074, 1);
  VALUES[2075] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2075, 1);
  VALUES[2076] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2076, 1);
  VALUES[2077] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2077, 1);
  VALUES[2078] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2078, 1);
  VALUES[2079] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2079, 1);
  VALUES[2080] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2080, 2);
  VALUES[2081] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2081, 2);
  VALUES[2082] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2082, 1);
  VALUES[2083] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2083, 1);
  VALUES[2084] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2084, 1);
  VALUES[2085] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2085, 2);
  VALUES[2086] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2086, 1);
  VALUES[2087] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2087, 1);
  VALUES[2088] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2088, 1);
  VALUES[2089] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2089, 1);
  VALUES[2090] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2090, 2);
  VALUES[2091] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2091, 1);
  VALUES[2092] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2092, 1);
  VALUES[2093] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2093, 1);
  VALUES[2094] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2094, 1);
  VALUES[2095] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2095, 1);
  VALUES[2096] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2096, 1);
  VALUES[2097] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2097, 1);
  VALUES[2098] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2098, 1);
  VALUES[2099] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2099, 2);
  VALUES[2100] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2100, 1);
  VALUES[2101] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2101, 1);
  VALUES[2102] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2102, 1);
  VALUES[2103] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2103, 1);
  VALUES[2104] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2104, 2);
  VALUES[2105] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2105, 1);
  VALUES[2106] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2106, 1);
  VALUES[2107] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2107, 1);
  VALUES[2108] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2108, 1);
  VALUES[2109] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2109, 1);
  VALUES[2110] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2110, 1);
  VALUES[2111] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2111, 1);
  VALUES[2112] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2112, 1);
  VALUES[2113] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2113, 1);
  VALUES[2114] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2114, 1);
  VALUES[2115] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2115, 1);
  VALUES[2116] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2116, 1);
  VALUES[2117] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2117, 2);
  VALUES[2118] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2118, 1);
  VALUES[2119] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2119, 2);
  VALUES[2120] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2120, 2);
  VALUES[2121] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2121, 1);
  VALUES[2122] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2122, 1);
  VALUES[2123] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2123, 1);
  VALUES[2124] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2124, 1);
  VALUES[2125] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2125, 1);
  VALUES[2126] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2126, 1);
  VALUES[2127] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2127, 1);
  VALUES[2128] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2128, 1);
  VALUES[2129] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2129, 1);
  VALUES[2130] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2130, 2);
  VALUES[2131] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2131, 1);
  VALUES[2132] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2132, 1);
  VALUES[2133] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2133, 2);
  VALUES[2134] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2134, 2);
  VALUES[2135] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2135, 1);
  VALUES[2136] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_2136, 1);

  WINDOWS_1252 = new PRUnichar*[32];
  for (PRInt32 i = 0; i < 32; ++i) {
    WINDOWS_1252[i] = (PRUnichar*)&(WINDOWS_1252_DATA[i]);
  }
}

void
nsHtml5NamedCharacters::releaseStatics()
{
  NAMES.release();
  delete[] VALUES;
}

