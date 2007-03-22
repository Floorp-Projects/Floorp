/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
      nsnull : sTagUnicodeTable[aEnum - 1];
  }
  static nsIAtom *GetAtom(nsHTMLTag aEnum)
  {
    return aEnum <= eHTMLTag_unknown || aEnum > NS_HTML_TAG_MAX ?
      nsnull : sTagAtomTable[aEnum - 1];
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
