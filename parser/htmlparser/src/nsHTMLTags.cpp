/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsHTMLTags.h"

#include "nsString.h"
#include "nsStaticNameTable.h"

// define an array of all HTML tags
#define HTML_TAG(_tag) #_tag,
static const char* kTagTable[] = {
#include "nsHTMLTagList.h"
};
#undef HTML_TAG

static PRInt32 gTableRefCount;
static nsStaticCaseInsensitiveNameTable* gTagTable;

void
nsHTMLTags::AddRefTable(void) 
{
  if (0 == gTableRefCount++) {
    NS_ASSERTION(!gTagTable, "pre existing array!");
    gTagTable = new nsStaticCaseInsensitiveNameTable();
    if (gTagTable) {
#ifdef DEBUG
    {
      // let's verify the table...
      for (PRInt32 index = 0; index < NS_HTML_TAG_MAX; ++index) {
        nsCAutoString temp1(kTagTable[index]);
        nsCAutoString temp2(kTagTable[index]);
        temp1.ToLowerCase();
        NS_ASSERTION(temp1.Equals(temp2), "upper case char in table");
      }
    }
#endif      
      gTagTable->Init(kTagTable, NS_HTML_TAG_MAX); 
    }
  }
}

void
nsHTMLTags::ReleaseTable(void) 
{
  if (0 == --gTableRefCount) {
    if (gTagTable) {
      delete gTagTable;
      gTagTable = nsnull;
    }
  }
}

nsHTMLTag 
nsHTMLTags::LookupTag(const nsCString& aTag)
{
  NS_ASSERTION(gTagTable, "no lookup table, needs addref");
  if (gTagTable) {
    // table is zero based, but tags are one based
    nsHTMLTag tag = nsHTMLTag(gTagTable->Lookup(aTag)+1);
    
    // hack: this can come out when rickg provides a way for the editor to ask
    // CanContain() questions without having to first fetch the parsers
    // internal enum values for a tag name.
    
    if (tag == eHTMLTag_unknown) {
      if(aTag.Equals("__moz_text")) {
        tag = eHTMLTag_text;
      }
      else {
        tag = eHTMLTag_userdefined;
      }
    }
    return tag;
  }  
  return eHTMLTag_userdefined;
}

nsHTMLTag 
nsHTMLTags::LookupTag(const nsString& aTag)
{
  NS_ASSERTION(gTagTable, "no lookup table, needs addref");
  if (gTagTable) {
    // table is zero based, but tags are one based
    nsHTMLTag tag = nsHTMLTag(gTagTable->Lookup(aTag)+1);
    
    // hack: this can come out when rickg provides a way for the editor to ask
    // CanContain() questions without having to first fetch the parsers
    // internal enum values for a tag name.
    
    if (tag == eHTMLTag_unknown) {
      nsCAutoString theTag; 
      theTag.AssignWithConversion(aTag);
      if (theTag.Equals("__moz_text")) {
        tag = eHTMLTag_text;
      }
      else if (theTag.Equals("#text")) {
        tag = eHTMLTag_text;
      }
      else {
        tag = eHTMLTag_userdefined;
      }
    }
    return tag;
  }  
  return eHTMLTag_userdefined;
}

const nsCString& 
nsHTMLTags::GetStringValue(nsHTMLTag aTag)
{
  NS_ASSERTION(gTagTable, "no lookup table, needs addref");
  if (gTagTable) {
    // table is zero based, but tags are one based
    return gTagTable->GetStringValue(PRInt32(aTag)-1);
  } else {
    static nsCString kNullStr;
    return kNullStr;
  }
}

const char* 
nsHTMLTags::GetCStringValue(nsHTMLTag aTag) {
  NS_ASSERTION(gTagTable, "no lookup table, needs addref");
  // Note: NS_HTML_TAG_MAX=113
  if ((eHTMLTag_unknown < aTag) && 
      (aTag <= NS_HTML_TAG_MAX)) {
    return kTagTable[PRInt32(aTag)-1];
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
