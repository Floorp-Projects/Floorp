/*
 * Copyright (c) 2008-2010 Mozilla Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

#define nsHtml5NamedCharacters_cpp_
#include "jArray.h"
#include "nscore.h"
#include "nsDebug.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Logging.h"

#include "nsHtml5NamedCharacters.h"

const char16_t nsHtml5NamedCharacters::VALUES[][2] = {
#define NAMED_CHARACTER_REFERENCE(N, CHARS, LEN, FLAG, VALUE) \
{ VALUE },
#include "nsHtml5NamedCharactersInclude.h"
#undef NAMED_CHARACTER_REFERENCE
{0, 0} };

char16_t** nsHtml5NamedCharacters::WINDOWS_1252;
static char16_t const WINDOWS_1252_DATA[] = {
  0x20AC,
  0x0081,
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
  0x008D,
  0x017D,
  0x008F,
  0x0090,
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
  0x009D,
  0x017E,
  0x0178
};

/**
 * To avoid having lots of pointers in the |charData| array, below,
 * which would cause us to have to do lots of relocations at library
 * load time, store all the string data for the names in one big array.
 * Then use tricks with enums to help us build an array that contains
 * the positions of each within the big arrays.
 */

static const int8_t ALL_NAMES[] = {
#define NAMED_CHARACTER_REFERENCE(N, CHARS, LEN, FLAG, VALUE) \
CHARS ,
#include "nsHtml5NamedCharactersInclude.h"
#undef NAMED_CHARACTER_REFERENCE
};

enum NamePositions {
  DUMMY_INITIAL_NAME_POSITION = 0,
/* enums don't take up space, so generate _START and _END */
#define NAMED_CHARACTER_REFERENCE(N, CHARS, LEN, FLAG, VALUE) \
NAME_##N##_DUMMY, /* automatically one higher than previous */ \
NAME_##N##_START = NAME_##N##_DUMMY - 1, \
NAME_##N##_END = NAME_##N##_START + LEN + FLAG,
#include "nsHtml5NamedCharactersInclude.h"
#undef NAMED_CHARACTER_REFERENCE
  DUMMY_FINAL_NAME_VALUE
};

/* check that the start positions will fit in 16 bits */
PR_STATIC_ASSERT(MOZ_ARRAY_LENGTH(ALL_NAMES) < 0x10000);

const nsHtml5CharacterName nsHtml5NamedCharacters::NAMES[] = {
#ifdef DEBUG
  #define NAMED_CHARACTER_REFERENCE(N, CHARS, LEN, FLAG, VALUE) \
{ NAME_##N##_START, LEN, N },
#else
  #define NAMED_CHARACTER_REFERENCE(N, CHARS, LEN, FLAG, VALUE) \
{ NAME_##N##_START, LEN, },
#endif
#include "nsHtml5NamedCharactersInclude.h"
#undef NAMED_CHARACTER_REFERENCE
};

int32_t
nsHtml5CharacterName::length() const
{
  return nameLen;
}

char16_t
nsHtml5CharacterName::charAt(int32_t index) const
{
  return static_cast<char16_t> (ALL_NAMES[nameStart + index]);
}

void
nsHtml5NamedCharacters::initializeStatics()
{
  WINDOWS_1252 = new char16_t*[32];
  for (int32_t i = 0; i < 32; ++i) {
    WINDOWS_1252[i] = (char16_t*)&(WINDOWS_1252_DATA[i]);
  }
}

void
nsHtml5NamedCharacters::releaseStatics()
{
  delete[] WINDOWS_1252;
}
