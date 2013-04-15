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

#include "nsHTMLTokens.h"
#include "nsDTDUtils.h"


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
 * @update	gess 01/04/99
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


/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
struct nsHTMLElement {

#ifdef DEBUG
  static  void    DebugDumpMembership(const char* aFilename);
  static  void    DebugDumpContainment(const char* aFilename,const char* aTitle);
  static  void    DebugDumpContainType(const char* aFilename);
#endif

  static  bool    IsInlineEntity(eHTMLTags aTag);
  static  bool    IsFlowEntity(eHTMLTags aTag);
  static  bool    IsBlockCloser(eHTMLTags aTag);

  inline  bool    IsBlock(void) const { 
                    if((mTagID>=eHTMLTag_unknown) & (mTagID<=eHTMLTag_xmp)){
                      return TestBits(mParentBits,kBlock);
                    } 
                    return false;
                  }

  inline  bool    IsBlockEntity(void) const { 
                    if((mTagID>=eHTMLTag_unknown) & (mTagID<=eHTMLTag_xmp)){
                      return TestBits(mParentBits,kBlockEntity);
                    } 
                    return false;
                  }

  inline  bool    IsSpecialEntity(void) const { 
                    if((mTagID>=eHTMLTag_unknown) & (mTagID<=eHTMLTag_xmp)){
                      return TestBits(mParentBits,kSpecial);
                    } 
                    return false;
                  }

  inline  bool    IsPhraseEntity(void) const { 
                    if((mTagID>=eHTMLTag_unknown) & (mTagID<=eHTMLTag_xmp)){
                      return TestBits(mParentBits,kPhrase);
                    } 
                    return false;
                  }

  inline  bool    IsFontStyleEntity(void) const { 
                    if((mTagID>=eHTMLTag_unknown) & (mTagID<=eHTMLTag_xmp)){
                      return TestBits(mParentBits,kFontStyle);
                    } 
                    return false;
                  }
  
  inline  bool    IsTableElement(void) const {  //return yes if it's a table or child of a table...
                    bool result=false;

                    switch(mTagID) {
                      case eHTMLTag_table:
                      case eHTMLTag_thead:
                      case eHTMLTag_tbody:
                      case eHTMLTag_tfoot:
                      case eHTMLTag_caption:
                      case eHTMLTag_tr:
                      case eHTMLTag_td:
                      case eHTMLTag_th:
                      case eHTMLTag_col:
                      case eHTMLTag_colgroup:
                        result=true;
                        break;
                      default:
                        result=false;
                    }
                    return result;
                  }


  static  int32_t GetIndexOfChildOrSynonym(nsDTDContext& aContext,eHTMLTags aChildTag);

  const TagList*  GetSynonymousTags(void) const {return mSynonymousTags;}
  const TagList*  GetRootTags(void) const {return mRootNodes;}
  const TagList*  GetEndRootTags(void) const {return mEndRootNodes;}
  const TagList*  GetAutoCloseStartTags(void) const {return mAutocloseStart;}
  const TagList*  GetAutoCloseEndTags(void) const {return mAutocloseEnd;}
  eHTMLTags       GetCloseTargetForEndTag(nsDTDContext& aContext,int32_t anIndex,nsDTDMode aMode) const;

  const TagList*        GetSpecialChildren(void) const {return mSpecialKids;}
  const TagList*        GetSpecialParents(void) const {return mSpecialParents;}

  bool            IsMemberOf(int32_t aType) const;
  bool            ContainsSet(int32_t aType) const;
  bool            CanContainType(int32_t aType) const;

  bool            CanContain(eHTMLTags aChild,nsDTDMode aMode) const;
  bool            CanExclude(eHTMLTags aChild) const;
  bool            CanOmitEndTag(void) const;
  bool            CanContainSelf(void) const;
  bool            CanAutoCloseTag(nsDTDContext& aContext,int32_t aIndex,eHTMLTags aTag) const;
  bool            HasSpecialProperty(int32_t aProperty) const;
  bool            IsSpecialParent(eHTMLTags aTag) const;
  bool            IsExcludableParent(eHTMLTags aParent) const;
  bool            SectionContains(eHTMLTags aTag,bool allowDepthSearch) const;
  bool            ShouldVerifyHierarchy() const;

  static  bool    CanContain(eHTMLTags aParent,eHTMLTags aChild,nsDTDMode aMode);
  static  bool    IsContainer(eHTMLTags aTag) ;
  static  bool    IsResidualStyleTag(eHTMLTags aTag) ;
  static  bool    IsTextTag(eHTMLTags aTag);
  static  bool    IsWhitespaceTag(eHTMLTags aTag);

  static  bool    IsBlockParent(eHTMLTags aTag);
  static  bool    IsInlineParent(eHTMLTags aTag); 
  static  bool    IsFlowParent(eHTMLTags aTag);
  static  bool    IsSectionTag(eHTMLTags aTag);
  static  bool    IsChildOfHead(eHTMLTags aTag,bool& aExclusively) ;

  eHTMLTags       mTagID;
  eHTMLTags       mRequiredAncestor;
  eHTMLTags       mExcludingAncestor; //If set, the presence of the excl-ancestor prevents this from opening.
  const TagList*  mRootNodes;         //These are the tags above which you many not autoclose a START tag
  const TagList*        mEndRootNodes;      //These are the tags above which you many not autoclose an END tag
  const TagList*        mAutocloseStart;    //these are the start tags that you can automatically close with this START tag
  const TagList*        mAutocloseEnd;      //these are the start tags that you can automatically close with this END tag
  const TagList*        mSynonymousTags;    //These are morally equivalent; an end tag for one can close a start tag for another (like <Hn>)
  const TagList*        mExcludableParents; //These are the TAGS that cannot contain you
  int             mParentBits;        //defines groups that can contain this element
  int             mInclusionBits;     //defines parental and containment rules
  int             mExclusionBits;     //defines things you CANNOT contain
  int             mSpecialProperties; //used for various special purposes...
  uint32_t        mPropagateRange;    //tells us how far a parent is willing to prop. badly formed children
  const TagList*  mSpecialParents;    //These are the special tags that contain this tag (directly)
  const TagList*  mSpecialKids;       //These are the extra things you can contain
}; 

extern const nsHTMLElement gHTMLElements[];

//special property bits...
static const int kPreferBody       = 0x0001; //this kHeadMisc tag prefers to be in the body if there isn't an explicit <head>
static const int kOmitEndTag       = 0x0002; //safely ignore end tag
static const int kLegalOpen        = 0x0004; //Lets BODY, TITLE, SCRIPT to reopen
static const int kNoPropagate      = 0x0008; //If set, this tag won't propagate as a child
static const int kBadContentWatch  = 0x0010; 

static const int kNoStyleLeaksIn   = 0x0020; 
static const int kNoStyleLeaksOut  = 0x0040; 

static const int kMustCloseSelf    = 0x0080; 
static const int kSaveMisplaced    = 0x0100; //If set, then children this tag can't contain are pushed onto the misplaced stack
static const int kNonContainer     = 0x0200; //If set, then this tag is not a container.
static const int kHandleStrayTag   = 0x0400; //If set, we automatically open a start tag
static const int kRequiresBody     = 0x0800; //If set, then in case of no BODY one will be opened up immediately.
static const int kVerifyHierarchy  = 0x1000; //If set, check to see if the tag is a child or a sibling..
static const int kPreferHead       = 0x2000; //This kHeadMisc tag prefers to be in the head if there isn't an explicit <body>

#endif
