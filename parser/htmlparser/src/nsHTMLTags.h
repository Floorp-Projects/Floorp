/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsHTMLTags_h___
#define nsHTMLTags_h___

#include "nshtmlpars.h"

struct nsStr;
class nsCString;

/*
   Declare the enum list using the magic of preprocessing
   enum values are "eHTMLTag_foo" (where foo is the tag)

   To change the list of tags, see nsHTMLTagList.h

 */
#define HTML_TAG(_tag) eHTMLTag_##_tag,
enum nsHTMLTag {
  /* this enum must be first and must be zero */
  eHTMLTag_unknown = 0,
#include "nsHTMLTagList.h"

  /* The remaining enums are not for tags */
  eHTMLTag_text, eHTMLTag_whitespace, eHTMLTag_newline, 
  eHTMLTag_comment, eHTMLTag_entity, eHTMLTag_userdefined
};
#undef HTML_TAG

#define NS_HTML_TAG_MAX PRInt32(eHTMLTag_text - 1)

class NS_HTMLPARS nsHTMLTags {
public:
  static void AddRefTable(void);
  static void ReleaseTable(void);

  static nsHTMLTag LookupTag(const nsStr& aTag);
  static const nsCString& GetStringValue(nsHTMLTag aEnum);
};

#endif /* nsHTMLTags_h___ */
