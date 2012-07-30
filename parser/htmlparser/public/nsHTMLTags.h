/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHTMLTags_h___
#define nsHTMLTags_h___

#include "nsStringGlue.h"
#include "plhash.h"

class nsIAtom;

/*
   Declare the enum list using the magic of preprocessing
   enum values are "eHTMLTag_foo" (where foo is the tag)

   To change the list of tags, see nsHTMLTagList.h

 */
#define HTML_TAG(_tag, _classname) eHTMLTag_##_tag,
#define HTML_HTMLELEMENT_TAG(_tag) eHTMLTag_##_tag,
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
#undef HTML_HTMLELEMENT_TAG
#undef HTML_OTHER

// All tags before eHTMLTag_text are HTML tags
#define NS_HTML_TAG_MAX PRInt32(eHTMLTag_text - 1)

class nsHTMLTags {
public:
  static nsresult AddRefTable(void);
  static void ReleaseTable(void);

  // Functions for converting string or atom to id
  static nsHTMLTag LookupTag(const nsAString& aTagName);
  static nsHTMLTag CaseSensitiveLookupTag(const PRUnichar* aTagName)
  {
    NS_ASSERTION(gTagTable, "no lookup table, needs addref");
    NS_ASSERTION(aTagName, "null tagname!");

    void* tag = PL_HashTableLookupConst(gTagTable, aTagName);

    return tag ? (nsHTMLTag)NS_PTR_TO_INT32(tag) : eHTMLTag_userdefined;
  }
  static nsHTMLTag CaseSensitiveLookupTag(nsIAtom* aTagName)
  {
    NS_ASSERTION(gTagAtomTable, "no lookup table, needs addref");
    NS_ASSERTION(aTagName, "null tagname!");

    void* tag = PL_HashTableLookupConst(gTagAtomTable, aTagName);

    return tag ? (nsHTMLTag)NS_PTR_TO_INT32(tag) : eHTMLTag_userdefined;
  }

  // Functions for converting an id to a string or atom
  static const PRUnichar *GetStringValue(nsHTMLTag aEnum)
  {
    return aEnum <= eHTMLTag_unknown || aEnum > NS_HTML_TAG_MAX ?
      nullptr : sTagUnicodeTable[aEnum - 1];
  }
  static nsIAtom *GetAtom(nsHTMLTag aEnum)
  {
    return aEnum <= eHTMLTag_unknown || aEnum > NS_HTML_TAG_MAX ?
      nullptr : sTagAtomTable[aEnum - 1];
  }

#ifdef DEBUG
  static void TestTagTable();
#endif

private:
  static nsIAtom* sTagAtomTable[eHTMLTag_userdefined - 1];
  static const PRUnichar* const sTagUnicodeTable[];

  static PRInt32 gTableRefCount;
  static PLHashTable* gTagTable;
  static PLHashTable* gTagAtomTable;
};

#define eHTMLTags nsHTMLTag

#endif /* nsHTMLTags_h___ */
