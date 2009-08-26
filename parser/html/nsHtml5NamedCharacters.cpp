#define nsHtml5NamedCharacters_cpp__
#include "prtypes.h"
#include "jArray.h"
#include "nscore.h"

#include "nsHtml5NamedCharacters.h"

jArray<jArray<PRUnichar,PRInt32>,PRInt32> nsHtml5NamedCharacters::NAMES;
jArray<PRUnichar,PRInt32>* nsHtml5NamedCharacters::VALUES;
PRUnichar** nsHtml5NamedCharacters::WINDOWS_1252;
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

#define NAMED_CHARACTER_REFERENCE(N, CHARS, LEN, VALUE, SIZE) \
static PRUnichar const NAME_##N[] = { CHARS };
#include "nsHtml5NamedCharactersInclude.h"
#undef NAMED_CHARACTER_REFERENCE

#define NAMED_CHARACTER_REFERENCE(N, CHARS, LEN, VALUE, SIZE) \
static PRUnichar const VALUE_##N[] = { VALUE };
#include "nsHtml5NamedCharactersInclude.h"
#undef NAMED_CHARACTER_REFERENCE

// XXX bug 501082: for some reason, msvc takes forever to optimize this function
#ifdef _MSC_VER
#pragma optimize("", off)
#endif

void
nsHtml5NamedCharacters::initializeStatics()
{
  NAMES = jArray<jArray<PRUnichar,PRInt32>,PRInt32>(2137);

#define NAMED_CHARACTER_REFERENCE(N, CHARS, LEN, VALUE, SIZE) \
  NAMES[N] = jArray<PRUnichar,PRInt32>((PRUnichar*)NAME_##N, LEN);
#include "nsHtml5NamedCharactersInclude.h"
#undef NAMED_CHARACTER_REFERENCE

  VALUES = new jArray<PRUnichar,PRInt32>[2137];

#define NAMED_CHARACTER_REFERENCE(N, CHARS, LEN, VALUE, SIZE) \
  VALUES[N] = jArray<PRUnichar,PRInt32>((PRUnichar*)VALUE_##N, SIZE);
#include "nsHtml5NamedCharactersInclude.h"
#undef NAMED_CHARACTER_REFERENCE

  WINDOWS_1252 = new PRUnichar*[32];
  for (PRInt32 i = 0; i < 32; ++i) {
    WINDOWS_1252[i] = (PRUnichar*)&(WINDOWS_1252_DATA[i]);
  }
}

#ifdef _MSC_VER
#pragma optimize("", on)
#endif

void
nsHtml5NamedCharacters::releaseStatics()
{
  NAMES.release();
  delete[] VALUES;
  delete[] WINDOWS_1252;
}
