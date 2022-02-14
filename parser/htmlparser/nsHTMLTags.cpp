/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHTMLTags.h"
#include "nsCRT.h"
#include "nsElementTable.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "mozilla/HashFunctions.h"
#include <algorithm>

using namespace mozilla;

// static array of unicode tag names
#define HTML_TAG(_tag, _classname, _interfacename) (u"" #_tag),
#define HTML_OTHER(_tag)
const char16_t* const nsHTMLTags::sTagNames[] = {
#include "nsHTMLTagList.h"
};
#undef HTML_TAG
#undef HTML_OTHER

int32_t nsHTMLTags::gTableRefCount;
nsHTMLTags::TagStringHash* nsHTMLTags::gTagTable;
nsHTMLTags::TagAtomHash* nsHTMLTags::gTagAtomTable;

#define NS_HTMLTAG_NAME_MAX_LENGTH 10

// static
nsresult nsHTMLTags::AddRefTable(void) {
  if (gTableRefCount++ == 0) {
    NS_ASSERTION(!gTagTable && !gTagAtomTable, "pre existing hash!");

    gTagTable = new TagStringHash(64);
    gTagAtomTable = new TagAtomHash(64);

    // Fill in gTagTable with the above static char16_t strings as
    // keys and the value of the corresponding enum as the value in
    // the table.

    for (int32_t i = 0; i < NS_HTML_TAG_MAX; ++i) {
      const char16_t* tagName = sTagNames[i];
      const nsHTMLTag tagValue = static_cast<nsHTMLTag>(i + 1);

      // We use AssignLiteral here to avoid a string copy. This is okay
      // because this is truly static data.
      nsString tmp;
      tmp.AssignLiteral(tagName, nsString::char_traits::length(tagName));
      gTagTable->InsertOrUpdate(tmp, tagValue);

      // All the HTML tag names are static atoms within nsGkAtoms, and they are
      // registered before this code is reached.
      nsStaticAtom* atom = NS_GetStaticAtom(tmp);
      MOZ_ASSERT(atom);
      gTagAtomTable->InsertOrUpdate(atom, tagValue);
    }

#ifdef DEBUG
    // Check all tagNames are lowercase, and that NS_HTMLTAG_NAME_MAX_LENGTH is
    // correct.
    uint32_t maxTagNameLength = 0;
    for (int i = 0; i < NS_HTML_TAG_MAX; ++i) {
      const char16_t* tagName = sTagNames[i];

      nsAutoString lowerTagName(tagName);
      ToLowerCase(lowerTagName);
      MOZ_ASSERT(lowerTagName.Equals(tagName));

      maxTagNameLength = std::max(NS_strlen(tagName), maxTagNameLength);
    }

    MOZ_ASSERT(maxTagNameLength == NS_HTMLTAG_NAME_MAX_LENGTH);

    CheckElementTable();
    TestTagTable();
#endif
  }

  return NS_OK;
}

// static
void nsHTMLTags::ReleaseTable(void) {
  if (0 == --gTableRefCount) {
    delete gTagTable;
    delete gTagAtomTable;
    gTagTable = nullptr;
    gTagAtomTable = nullptr;
  }
}

// static
nsHTMLTag nsHTMLTags::StringTagToId(const nsAString& aTagName) {
  uint32_t length = aTagName.Length();

  if (length > NS_HTMLTAG_NAME_MAX_LENGTH) {
    return eHTMLTag_userdefined;
  }

  // Setup a stack allocated string buffer with the appropriate length.
  nsAutoString lowerCase;
  lowerCase.SetLength(length);

  // Operate on the raw buffers to avoid bounds checks.
  auto src = aTagName.BeginReading();
  auto dst = lowerCase.BeginWriting();

  // Fast lowercasing-while-copying of ASCII characters into a
  // nsString buffer.

  for (uint32_t i = 0; i < length; i++) {
    char16_t c = src[i];

    if (c <= 'Z' && c >= 'A') {
      c |= 0x20;  // Lowercase the ASCII character.
    }

    dst[i] = c;  // Copy ASCII character.
  }

  return CaseSensitiveStringTagToId(lowerCase);
}

#ifdef DEBUG
void nsHTMLTags::TestTagTable() {
  const char16_t* tag;
  nsHTMLTag id;
  RefPtr<nsAtom> atom;

  nsHTMLTags::AddRefTable();
  // Make sure we can find everything we are supposed to
  for (int i = 0; i < NS_HTML_TAG_MAX; ++i) {
    tag = sTagNames[i];
    const nsAString& tagString = nsDependentString(tag);
    id = StringTagToId(tagString);
    NS_ASSERTION(id != eHTMLTag_userdefined, "can't find tag id");

    nsAutoString uname(tagString);
    ToUpperCase(uname);
    NS_ASSERTION(id == StringTagToId(uname), "wrong id");

    NS_ASSERTION(id == CaseSensitiveStringTagToId(tagString), "wrong id");

    atom = NS_Atomize(tag);
    NS_ASSERTION(id == CaseSensitiveAtomTagToId(atom), "wrong id");
  }

  // Make sure we don't find things that aren't there
  id = StringTagToId(u"@"_ns);
  NS_ASSERTION(id == eHTMLTag_userdefined, "found @");
  id = StringTagToId(u"zzzzz"_ns);
  NS_ASSERTION(id == eHTMLTag_userdefined, "found zzzzz");

  atom = NS_Atomize("@");
  id = CaseSensitiveAtomTagToId(atom);
  NS_ASSERTION(id == eHTMLTag_userdefined, "found @");
  atom = NS_Atomize("zzzzz");
  id = CaseSensitiveAtomTagToId(atom);
  NS_ASSERTION(id == eHTMLTag_userdefined, "found zzzzz");

  ReleaseTable();
}

#endif  // DEBUG
