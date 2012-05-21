/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include <stdio.h>
#include <stdlib.h>
#include "nsString.h"
#include "nsStringBuffer.h"
#include "nsReadableUtils.h"
#include "UTFStrings.h"
#include "nsUnicharUtils.h"
#include "mozilla/HashFunctions.h"

using namespace mozilla;

namespace TestUTF {

bool
test_valid()
{
  for (unsigned int i = 0; i < ArrayLength(ValidStrings); ++i) {
    nsDependentCString str8(ValidStrings[i].m8);
    nsDependentString str16(ValidStrings[i].m16);

    if (!NS_ConvertUTF16toUTF8(str16).Equals(str8))
      return false;

    if (!NS_ConvertUTF8toUTF16(str8).Equals(str16))
      return false;

    nsCString tmp8("string ");
    AppendUTF16toUTF8(str16, tmp8);
    if (!tmp8.Equals(NS_LITERAL_CSTRING("string ") + str8))
      return false;

    nsString tmp16(NS_LITERAL_STRING("string "));
    AppendUTF8toUTF16(str8, tmp16);
    if (!tmp16.Equals(NS_LITERAL_STRING("string ") + str16))
      return false;

    if (CompareUTF8toUTF16(str8, str16) != 0)
      return false;
  }
  
  return true;
}

bool
test_invalid16()
{
  for (unsigned int i = 0; i < ArrayLength(Invalid16Strings); ++i) {
    nsDependentString str16(Invalid16Strings[i].m16);
    nsDependentCString str8(Invalid16Strings[i].m8);

    if (!NS_ConvertUTF16toUTF8(str16).Equals(str8))
      return false;

    nsCString tmp8("string ");
    AppendUTF16toUTF8(str16, tmp8);
    if (!tmp8.Equals(NS_LITERAL_CSTRING("string ") + str8))
      return false;

    if (CompareUTF8toUTF16(str8, str16) != 0)
      return false;
  }
  
  return true;
}

bool
test_invalid8()
{
  for (unsigned int i = 0; i < ArrayLength(Invalid8Strings); ++i) {
    nsDependentString str16(Invalid8Strings[i].m16);
    nsDependentCString str8(Invalid8Strings[i].m8);

    if (!NS_ConvertUTF8toUTF16(str8).Equals(str16))
      return false;

    nsString tmp16(NS_LITERAL_STRING("string "));
    AppendUTF8toUTF16(str8, tmp16);
    if (!tmp16.Equals(NS_LITERAL_STRING("string ") + str16))
      return false;

    if (CompareUTF8toUTF16(str8, str16) != 0)
      return false;
  }
  
  return true;
}

bool
test_malformed8()
{
// Don't run this test in debug builds as that intentionally asserts.
#ifndef DEBUG
  for (unsigned int i = 0; i < ArrayLength(Malformed8Strings); ++i) {
    nsDependentCString str8(Malformed8Strings[i]);

    if (!NS_ConvertUTF8toUTF16(str8).IsEmpty())
      return false;

    nsString tmp16(NS_LITERAL_STRING("string"));
    AppendUTF8toUTF16(str8, tmp16);
    if (!tmp16.Equals(NS_LITERAL_STRING("string")))
      return false;

    if (CompareUTF8toUTF16(str8, EmptyString()) == 0)
      return false;
  }
#endif
  
  return true;
}

bool
test_hashas16()
{
  for (unsigned int i = 0; i < ArrayLength(ValidStrings); ++i) {
    nsDependentCString str8(ValidStrings[i].m8);
    bool err;
    if (HashString(ValidStrings[i].m16) !=
        HashUTF8AsUTF16(str8.get(), str8.Length(), &err) ||
        err)
      return false;
  }

  for (unsigned int i = 0; i < ArrayLength(Invalid8Strings); ++i) {
    nsDependentCString str8(Invalid8Strings[i].m8);
    bool err;
    if (HashString(Invalid8Strings[i].m16) !=
        HashUTF8AsUTF16(str8.get(), str8.Length(), &err) ||
        err)
      return false;
  }

// Don't run this test in debug builds as that intentionally asserts.
#ifndef DEBUG
  for (unsigned int i = 0; i < ArrayLength(Malformed8Strings); ++i) {
    nsDependentCString str8(Malformed8Strings[i]);
    bool err;
    if (HashUTF8AsUTF16(str8.get(), str8.Length(), &err) != 0 ||
        !err)
      return false;
  }
#endif

  return true;
}

typedef bool (*TestFunc)();

static const struct Test
  {
    const char* name;
    TestFunc    func;
  }
tests[] =
  {
    { "test_valid", test_valid },
    { "test_invalid16", test_invalid16 },
    { "test_invalid8", test_invalid8 },
    { "test_malformed8", test_malformed8 },
    { "test_hashas16", test_hashas16 },
    { nsnull, nsnull }
  };

}

using namespace TestUTF;

int main(int argc, char **argv)
  {
    int count = 1;
    if (argc > 1)
      count = atoi(argv[1]);

    while (count--)
      {
        for (const Test* t = tests; t->name != nsnull; ++t)
          {
            printf("%25s : %s\n", t->name, t->func() ? "SUCCESS" : "FAILURE <--");
          }
      }
    
    return 0;
  }
