/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsHTMLTags.h"

#include "nsString.h"
#include "nsAVLTree.h"

// define an array of all HTML tags
#define HTML_TAG(_tag) #_tag,
static const char* kTagTable[] = {
#include "nsHTMLTagList.h"
};
#undef HTML_TAG


struct TagNode {
  TagNode(void)
    : mStr(),
      mEnum(eHTMLTag_unknown)
  {}

  TagNode(const nsCString& aString) : mStr(aString), mEnum(eHTMLTag_unknown) { 
  }

  nsCAutoString mStr;
  nsHTMLTag     mEnum;
};

class TagComparitor: public nsAVLNodeComparitor {
public:
  virtual ~TagComparitor(void) {}
  virtual PRInt32 operator()(void* anItem1,void* anItem2) {
    TagNode* one = (TagNode*)anItem1;
    TagNode* two = (TagNode*)anItem2;
    return one->mStr.Compare(two->mStr, PR_TRUE);
  }
}; 


static PRInt32    gTableRefCount;
static TagNode*   gTagArray;
static nsAVLTree* gTagTree;
static TagComparitor* gComparitor;

void
nsHTMLTags::AddRefTable(void) 
{
  if (0 == gTableRefCount++) {
    if (! gTagArray) {
      gTagArray= new TagNode[NS_HTML_TAG_MAX];
      gComparitor = new TagComparitor();
      if (gComparitor) {
        gTagTree = new nsAVLTree(*gComparitor, nsnull);
      }
      if (gTagArray && gTagTree) {
        PRInt32 index = -1;
        while (++index < NS_HTML_TAG_MAX) {
          gTagArray[index].mStr = kTagTable[index];
          gTagArray[index].mEnum = nsHTMLTag(index + 1);  // 1 based
          gTagTree->AddItem(&(gTagArray[index]));
        }
      }
    }
  }
}

void
nsHTMLTags::ReleaseTable(void) 
{
  if (0 == --gTableRefCount) {
    if (gTagArray) {
      delete [] gTagArray;
      gTagArray = nsnull;
    }
    if (gTagTree) {
      delete gTagTree;
      gTagTree = nsnull;
    }
    if (gComparitor) {
      delete gComparitor;
      gComparitor = nsnull;
    }
  }
}


nsHTMLTag 
nsHTMLTags::LookupTag(const nsCString& aTag)
{
  NS_ASSERTION(gTagTree, "no lookup table, needs addref");
  if (gTagTree) {
    TagNode node(aTag);
    TagNode*  found = (TagNode*)gTagTree->FindItem(&node);
    if (found) {
      NS_ASSERTION(found->mStr.Equals(aTag,IGNORE_CASE), "bad tree");
      return found->mEnum;
    }
    else {
    // hack: this can come out when rickg provides a way for the editor to ask
    // CanContain() questions without having to first fetch the parsers
    // internal enum values for a tag name.
      if (aTag.Equals("__moz_text"))
        return eHTMLTag_text;
    }
  }
  return eHTMLTag_userdefined;
}

nsHTMLTag 
nsHTMLTags::LookupTag(const nsString& aTag) {
  nsCAutoString theTag(aTag);
  return LookupTag(theTag);
}



const nsCString& 
nsHTMLTags::GetStringValue(nsHTMLTag aTag)
{
  NS_ASSERTION(gTagArray, "no lookup table, needs addref");
  if ((eHTMLTag_unknown < aTag) && 
      (aTag < NS_HTML_TAG_MAX) && gTagArray) {
    return gTagArray[aTag - 1].mStr;
  }
  else {
    static const nsCString* kNullStr=0;
    if(!kNullStr)
      kNullStr=new nsCString("");
    return *kNullStr;
  }
}

const char* nsHTMLTags::GetCStringValue(nsHTMLTag aTag) {
  NS_ASSERTION(gTagArray, "no lookup table, needs addref");
  if ((eHTMLTag_unknown < aTag) && 
      (aTag < NS_HTML_TAG_MAX) && gTagArray) {
    return gTagArray[aTag - 1].mStr.mStr;
  }
  else {
    static const char* kNullStr="";
    return kNullStr;
  }
}


#ifdef NS_DEBUG
#include <stdio.h>
#include "nsCRT.h"

class nsTestTagTable {
public:
   nsTestTagTable() {
     const char *tag;
     nsHTMLTag id;

     nsHTMLTags::AddRefTable();
     // Make sure we can find everything we are supposed to
     for (int i = 0; i < NS_HTML_TAG_MAX; i++) {
       tag = kTagTable[i];
       id = nsHTMLTags::LookupTag(nsCAutoString(tag));
       NS_ASSERTION(id != eHTMLTag_userdefined, "can't find tag id");
       const char* check = nsHTMLTags::GetStringValue(id);
       NS_ASSERTION(0 == nsCRT::strcmp(check, tag), "can't map id back to tag");
     }

     // Make sure we don't find things that aren't there
     id = nsHTMLTags::LookupTag(nsCAutoString("@"));
     NS_ASSERTION(id == eHTMLTag_userdefined, "found @");
     id = nsHTMLTags::LookupTag(nsCAutoString("zzzzz"));
     NS_ASSERTION(id == eHTMLTag_userdefined, "found zzzzz");

     const nsCAutoString  kNull;
     tag = nsHTMLTags::GetStringValue((nsHTMLTag) 0);
     NS_ASSERTION(kNull.Equals(tag), "found enum 0");
     tag = nsHTMLTags::GetStringValue((nsHTMLTag) -1);
     NS_ASSERTION(kNull.Equals(tag), "found enum -1");
     tag = nsHTMLTags::GetStringValue((nsHTMLTag) (NS_HTML_TAG_MAX + 1));
     NS_ASSERTION(kNull.Equals(tag), "found past max enum");
     nsHTMLTags::ReleaseTable();
   }
};
//nsTestTagTable validateTagTable;
#endif

