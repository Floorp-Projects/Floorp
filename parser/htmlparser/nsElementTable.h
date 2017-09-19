/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 * MODULE NOTES:
 * @update  gess 4/1/98
 * 
 */



#ifndef _NSELEMENTABLE
#define _NSELEMENTABLE

#include "nsHTMLTags.h"
#include "nsIDTD.h"

#ifdef DEBUG
extern void CheckElementTable();
#endif


/**
 * We're asking the question: is aTest a member of bitset. 
 *
 * @param 
 * @return TRUE or FALSE
 */
inline bool TestBits(int aBitset,int aTest) {
  if(aTest) {
    int32_t result=(aBitset & aTest);
    return bool(result==aTest);
  }
  return false;
}

struct nsHTMLElement {
  bool            IsMemberOf(int32_t aType) const;

#ifdef DEBUG
  nsHTMLTag       mTagID;
#endif
  int             mParentBits;        //defines groups that can contain this element
  bool            mLeaf;

  static  bool    IsContainer(nsHTMLTag aTag);
  static  bool    IsBlock(nsHTMLTag aTag);
};

#endif
