/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHTMLTags.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStaticAtom.h"
#include "nsUnicharUtils.h"
#include "mozilla/HashFunctions.h"
#include <algorithm>

using namespace mozilla;

// static array of unicode tag names
#define HTML_TAG(_tag, _classname, _interfacename) (u"" #_tag),
#define HTML_OTHER(_tag)
const char16_t* const nsHTMLTags::sTagUnicodeTable[] = {
#include "nsHTMLTagList.h"
};
#undef HTML_TAG
#undef HTML_OTHER

// static array of tag atoms
nsAtom* nsHTMLTags::sTagAtomTable[eHTMLTag_userdefined - 1];

int32_t nsHTMLTags::gTableRefCount;
PLHashTable* nsHTMLTags::gTagTable;
PLHashTable* nsHTMLTags::gTagAtomTable;


// char16_t* -> id hash
static PLHashNumber
HTMLTagsHashCodeUCPtr(const void *key)
{
  return HashString(static_cast<const char16_t*>(key));
}

static int
HTMLTagsKeyCompareUCPtr(const void *key1, const void *key2)
{
  const char16_t *str1 = (const char16_t *)key1;
  const char16_t *str2 = (const char16_t *)key2;

  return nsCRT::strcmp(str1, str2) == 0;
}

// nsAtom* -> id hash
static PLHashNumber
HTMLTagsHashCodeAtom(const void *key)
{
  return NS_PTR_TO_INT32(key) >> 2;
}

#define NS_HTMLTAG_NAME_MAX_LENGTH 10

// static
void
nsHTMLTags::RegisterAtoms(void)
{
#define HTML_TAG(_tag, _classname, _interfacename) NS_STATIC_ATOM_BUFFER(Atombuffer_##_tag, #_tag)
#define HTML_OTHER(_tag)
#include "nsHTMLTagList.h"
#undef HTML_TAG
#undef HTML_OTHER

// static array of tag StaticAtom structs
#define HTML_TAG(_tag, _classname, _interfacename) NS_STATIC_ATOM(Atombuffer_##_tag, &nsHTMLTags::sTagAtomTable[eHTMLTag_##_tag - 1]),
#define HTML_OTHER(_tag)
  static const nsStaticAtom sTagAtoms_info[] = {
#include "nsHTMLTagList.h"
  };
#undef HTML_TAG
#undef HTML_OTHER

  // Fill in our static atom pointers
  NS_RegisterStaticAtoms(sTagAtoms_info);


#if defined(DEBUG)
  {
    // let's verify that all names in the the table are lowercase...
    for (int32_t i = 0; i < NS_HTML_TAG_MAX; ++i) {
      nsAutoString temp1((char16_t*)sTagAtoms_info[i].mString);
      nsAutoString temp2((char16_t*)sTagAtoms_info[i].mString);
      ToLowerCase(temp1);
      NS_ASSERTION(temp1.Equals(temp2), "upper case char in table");
    }

    // let's verify that all names in the unicode strings above are
    // correct.
    for (int32_t i = 0; i < NS_HTML_TAG_MAX; ++i) {
      nsAutoString temp1(sTagUnicodeTable[i]);
      nsAutoString temp2((char16_t*)sTagAtoms_info[i].mString);
      NS_ASSERTION(temp1.Equals(temp2), "Bad unicode tag name!");
    }

    // let's verify that NS_HTMLTAG_NAME_MAX_LENGTH is correct
    uint32_t maxTagNameLength = 0;
    for (int32_t i = 0; i < NS_HTML_TAG_MAX; ++i) {
      uint32_t len = NS_strlen(sTagUnicodeTable[i]);
      maxTagNameLength = std::max(len, maxTagNameLength);
    }
    NS_ASSERTION(maxTagNameLength == NS_HTMLTAG_NAME_MAX_LENGTH,
                 "NS_HTMLTAG_NAME_MAX_LENGTH not set correctly!");
  }
#endif
}

// static
nsresult
nsHTMLTags::AddRefTable(void)
{
  if (gTableRefCount++ == 0) {
    NS_ASSERTION(!gTagTable && !gTagAtomTable, "pre existing hash!");

    gTagTable = PL_NewHashTable(64, HTMLTagsHashCodeUCPtr,
                                HTMLTagsKeyCompareUCPtr, PL_CompareValues,
                                nullptr, nullptr);
    NS_ENSURE_TRUE(gTagTable, NS_ERROR_OUT_OF_MEMORY);

    gTagAtomTable = PL_NewHashTable(64, HTMLTagsHashCodeAtom,
                                    PL_CompareValues, PL_CompareValues,
                                    nullptr, nullptr);
    NS_ENSURE_TRUE(gTagAtomTable, NS_ERROR_OUT_OF_MEMORY);

    // Fill in gTagTable with the above static char16_t strings as
    // keys and the value of the corresponding enum as the value in
    // the table.

    int32_t i;
    for (i = 0; i < NS_HTML_TAG_MAX; ++i) {
      PL_HashTableAdd(gTagTable, sTagUnicodeTable[i],
                      NS_INT32_TO_PTR(i + 1));

      PL_HashTableAdd(gTagAtomTable, sTagAtomTable[i],
                      NS_INT32_TO_PTR(i + 1));
    }
  }

  return NS_OK;
}

// static
void
nsHTMLTags::ReleaseTable(void)
{
  if (0 == --gTableRefCount) {
    if (gTagTable) {
      // Nothing to delete/free in this table, just destroy the table.

      PL_HashTableDestroy(gTagTable);
      PL_HashTableDestroy(gTagAtomTable);
      gTagTable = nullptr;
      gTagAtomTable = nullptr;
    }
  }
}

// static
nsHTMLTag
nsHTMLTags::StringTagToId(const nsAString& aTagName)
{
  uint32_t length = aTagName.Length();

  if (length > NS_HTMLTAG_NAME_MAX_LENGTH) {
    return eHTMLTag_userdefined;
  }

  char16_t buf[NS_HTMLTAG_NAME_MAX_LENGTH + 1];

  nsAString::const_iterator iter;
  uint32_t i = 0;
  char16_t c;

  aTagName.BeginReading(iter);

  // Fast lowercasing-while-copying of ASCII characters into a
  // char16_t buffer

  while (i < length) {
    c = *iter;

    if (c <= 'Z' && c >= 'A') {
      c |= 0x20; // Lowercase the ASCII character.
    }

    buf[i] = c; // Copy ASCII character.

    ++i;
    ++iter;
  }

  buf[i] = 0;

  return CaseSensitiveStringTagToId(buf);
}

#ifdef DEBUG
void
nsHTMLTags::TestTagTable()
{
     const char16_t *tag;
     nsHTMLTag id;
     RefPtr<nsAtom> atom;

     nsHTMLTags::AddRefTable();
     // Make sure we can find everything we are supposed to
     for (int i = 0; i < NS_HTML_TAG_MAX; ++i) {
       tag = sTagUnicodeTable[i];
       id = StringTagToId(nsDependentString(tag));
       NS_ASSERTION(id != eHTMLTag_userdefined, "can't find tag id");

       nsAutoString uname(tag);
       ToUpperCase(uname);
       NS_ASSERTION(id == StringTagToId(uname), "wrong id");

       NS_ASSERTION(id == CaseSensitiveStringTagToId(tag), "wrong id");

       atom = NS_Atomize(tag);
       NS_ASSERTION(id == CaseSensitiveAtomTagToId(atom), "wrong id");
     }

     // Make sure we don't find things that aren't there
     id = StringTagToId(NS_LITERAL_STRING("@"));
     NS_ASSERTION(id == eHTMLTag_userdefined, "found @");
     id = StringTagToId(NS_LITERAL_STRING("zzzzz"));
     NS_ASSERTION(id == eHTMLTag_userdefined, "found zzzzz");

     atom = NS_Atomize("@");
     id = CaseSensitiveAtomTagToId(atom);
     NS_ASSERTION(id == eHTMLTag_userdefined, "found @");
     atom = NS_Atomize("zzzzz");
     id = CaseSensitiveAtomTagToId(atom);
     NS_ASSERTION(id == eHTMLTag_userdefined, "found zzzzz");

     ReleaseTable();
}

#endif // DEBUG
