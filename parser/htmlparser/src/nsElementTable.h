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

//*********************************************************************************************
// The following ints define the standard groups of HTML elements...
//*********************************************************************************************

static const int kNone= 0x0;

static const int kHTMLContent   = 0x0001; //  HEAD, (FRAMESET | BODY)
static const int kHeadContent   = 0x0002; //  Elements that *must* be in the head.
static const int kHeadMisc      = 0x0004; //  Elements that *can* be in the head.

static const int kSpecial       = 0x0008; //  A,    IMG,  APPLET, OBJECT, FONT, BASEFONT, BR, SCRIPT, 
                                          //  MAP,  Q,    SUB,    SUP,    SPAN, BDO,      IFRAME

static const int kFormControl   = 0x0010; //  INPUT SELECT  TEXTAREA  LABEL BUTTON
static const int kPreformatted  = 0x0020; //  PRE
static const int kPreExclusion  = 0x0040; //  IMG,  OBJECT, APPLET, BIG,  SMALL,  SUB,  SUP,  FONT, BASEFONT
static const int kFontStyle     = 0x0080; //  TT, I, B, U, S, STRIKE, BIG, SMALL
static const int kPhrase        = 0x0100; //  EM, STRONG, DFN, CODE, SAMP, KBD, VAR, CITE, ABBR, ACRONYM
static const int kHeading       = 0x0200; //  H1..H6
static const int kBlockMisc     = 0x0400; //  OBJECT, SCRIPT
static const int kBlock         = 0x0800; //  ADDRESS, BLOCKQUOTE, CENTER, DIV, DL, FIELDSET, FORM, 
                                          //  ISINDEX, HR, NOSCRIPT, NOFRAMES, P, TABLE
static const int kList          = 0x1000; //  UL, OL, DIR, MENU
static const int kPCDATA        = 0x2000; //  plain text and entities...
static const int kSelf          = 0x4000; //  whatever THIS tag is...
static const int kExtensions    = 0x8000; //  BGSOUND, WBR, NOBR
static const int kTable         = 0x10000;//  TR,TD,THEAD,TBODY,TFOOT,CAPTION,TH
static const int kDLChild       = 0x20000;//  DL, DT
static const int kCDATA         = 0x40000;//  just plain text...

static const int kInlineEntity  = (kPCDATA|kFontStyle|kPhrase|kSpecial|kFormControl|kExtensions);  //  #PCDATA, %fontstyle, %phrase, %special, %formctrl
static const int kBlockEntity   = (kHeading|kList|kPreformatted|kBlock); //  %heading, %list, %preformatted, %block
static const int kFlowEntity    = (kBlockEntity|kInlineEntity); //  %blockentity, %inlineentity
static const int kAllTags       = 0xffffff;


//*********************************************************************************************
// The following ints define the standard groups of HTML elements...
//*********************************************************************************************

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

  eHTMLTags       mTagID;
  int             mParentBits;        //defines groups that can contain this element
  bool            mLeaf;

  static  bool    IsContainer(eHTMLTags aTag);
}; 

extern const nsHTMLElement gHTMLElements[];

#endif
