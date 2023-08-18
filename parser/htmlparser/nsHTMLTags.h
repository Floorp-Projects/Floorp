/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHTMLTags_h___
#define nsHTMLTags_h___

#include "nsAtomHashKeys.h"
#include "nsString.h"
#include "nsTHashMap.h"
#include "nsHashKeys.h"

/*
   Declare the enum list using the magic of preprocessing
   enum values are "eHTMLTag_foo" (where foo is the tag)

   To change the list of tags, see nsHTMLTagList.h

   These enum values are used as the index of array in various places.
   If we change the structure of the enum by adding entries to it or removing
   entries from it _directly_, not via nsHTMLTagList.h, don't forget to update
   dom/bindings/BindingUtils.cpp and dom/html/nsHTMLContentSink.cpp as well.
 */
#define HTML_TAG(_tag, _classname, _interfacename) eHTMLTag_##_tag,
#define HTML_OTHER(_tag) eHTMLTag_##_tag,
enum nsHTMLTag {
  /* this enum must be first and must be zero */
  eHTMLTag_unknown = 0,
#include "nsHTMLTagList.h"

  /* can't be moved into nsHTMLTagList since gcc3.4 doesn't like a
     comma at the end of enum list*/
  eHTMLTag_userdefined
};
#undef HTML_TAG
#undef HTML_OTHER

// All tags before eHTMLTag_text are HTML tags
#define NS_HTML_TAG_MAX int32_t(eHTMLTag_text - 1)

class nsHTMLTags {
 public:
  using TagStringHash = nsTHashMap<nsStringHashKey, nsHTMLTag>;
  // Can be weak, because we know these are always static, see AddRefTable.
  using TagAtomHash = nsTHashMap<nsAtom*, nsHTMLTag>;

  static nsresult AddRefTable(void);
  static void ReleaseTable(void);

  // Functions for converting string or atom to id
  static nsHTMLTag StringTagToId(const nsAString& aTagName);
  static nsHTMLTag AtomTagToId(nsAtom* aTagName) {
    return StringTagToId(nsDependentAtomString(aTagName));
  }

  static nsHTMLTag CaseSensitiveStringTagToId(const nsAString& aTagName) {
    NS_ASSERTION(gTagTable, "no lookup table, needs addref");

    return gTagTable->MaybeGet(aTagName).valueOr(eHTMLTag_userdefined);
  }
  static nsHTMLTag CaseSensitiveAtomTagToId(nsAtom* aTagName) {
    NS_ASSERTION(gTagAtomTable, "no lookup table, needs addref");
    NS_ASSERTION(aTagName, "null tagname!");

    return gTagAtomTable->MaybeGet(aTagName).valueOr(eHTMLTag_userdefined);
  }

#ifdef DEBUG
  static void TestTagTable();
#endif

 private:
  static const char16_t* const sTagNames[];

  static int32_t gTableRefCount;
  static TagStringHash* gTagTable;
  static TagAtomHash* gTagAtomTable;
};

#endif /* nsHTMLTags_h___ */
