/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


/**
 * MODULE NOTES:
 * @update  gess 4/1/98
 * 
 */ 

#include "nsElementTable.h"


/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
PRBool CTagList::Contains(eHTMLTags aTag){
  if(mTagList) {
    return FindTagInSet(aTag,mTagList,mCount);
  }
  return FindTagInSet(aTag,mTags,mCount);
}

/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
PRInt32 CTagList::GetTopmostIndexOf(nsTagStack& aTagStack){
  int max=aTagStack.mCount;
  int index;
  for(index=max-1;index>0;index--){
    if(FindTagInSet(aTagStack.mTags[index],mTags,mCount)) {
      return index;
    }
  }
  return kNotFound;
}

/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
PRInt32 CTagList::GetBottommostIndexOf(nsTagStack& aTagStack,PRInt32 aStartOffset){
  int max=aTagStack.mCount;
  int index;
  for(index=aStartOffset;index<max;index++){
    if(FindTagInSet(aTagStack.mTags[index],mTags,mCount)) {
      return index;
    }
  }
  return kNotFound;
}


//*********************************************************************************************
// The following ints define the standard groups of HTML elements...
//*********************************************************************************************

static const int kNone= 0x0;

static const int kHTMLContent   = 0x0001; //  HEAD, (FRAMESET | BODY)
static const int kHeadContent   = 0x0002; //  TITLE, ISINDEX, BASE
static const int kHeadMisc      = 0x0004; //  SCRIPT, STYLE, META,  LINK, OBJECT

static const int kSpecial       = 0x0008; //  A,    IMG,  APPLET, OBJECT, FONT, BASEFONT, BR, SCRIPT, 
                                          //  MAP,  Q,    SUB,    SUP,    SPAN, BDO,      IFRAME

static const int kFormControl   = 0x0010; //  INPUT SELECT  TEXTAREA  LABEL BUTTON
static const int kPreformatted  = 0x0011; //  PRE
static const int kPreExclusion  = 0x0012; //  IMG,  OBJECT, APPLET, BIG,  SMALL,  SUB,  SUP,  FONT, BASEFONT
static const int kFontStyle     = 0x0014; //  TT, I, B, U, S, STRIKE, BIG, SMALL
static const int kPhrase        = 0x0018; //  EM, STRONG, DFN, CODE, SAMP, KBD, VAR, CITE, ABBR, ACRONYM
static const int kHeading       = 0x0020; //  H1..H6
static const int kBlockMisc     = 0x0021; //  P, DL, DIV, CENTER, NOSCRIPT, NOFRAMES, BLOCKQUOTE
                                          //  FORM, ISINDEX, HR, TABLE, FIELDSET, ADDRESS

static const int kList          = 0x0024; //  UL, OL, DIR, MENU
static const int kPCDATA        = 0x0028; //  just plain text...
static const int kSelf          = 0x0040; //  whatever THIS tag is...

static const int kInline        = (kPCDATA|kFontStyle|kPhrase|kSpecial|kFormControl);  //  #PCDATA, %fontstyle, %phrase, %special, %formctrl
static const int kBlock         = (kHeading|kList|kPreformatted|kBlockMisc); //  %heading, %list, %preformatted, %blockmisc
static const int kFlow          = (kBlock|kInline); //  %block, %inline

 
static eHTMLTags gStyleTags[]={
  eHTMLTag_a,       eHTMLTag_acronym,   eHTMLTag_b,
  eHTMLTag_bdo,     eHTMLTag_big,       eHTMLTag_blink,
  eHTMLTag_center,  eHTMLTag_cite,      eHTMLTag_code,
  eHTMLTag_del,     eHTMLTag_dfn,       eHTMLTag_em,
  eHTMLTag_font,    eHTMLTag_i,         eHTMLTag_ins,
  eHTMLTag_kbd,     eHTMLTag_nobr,      eHTMLTag_q,
  eHTMLTag_s,       eHTMLTag_samp,      eHTMLTag_small,
  eHTMLTag_span,    eHTMLTag_strike,    eHTMLTag_strong,
  eHTMLTag_sub,     eHTMLTag_sup,       eHTMLTag_tt,
  eHTMLTag_u,       eHTMLTag_var};

  
/***************************************************************************** 
  Now it's time to list all the html elements all with their capabilities...
******************************************************************************/


//First, define the set of taglists for tags with special parents...
CTagList  gAParents(1,0,eHTMLTag_map);
CTagList  gInAddress(1,0,eHTMLTag_address);
CTagList  gInHead(1,0,eHTMLTag_head);
CTagList  gInTable(1,0,eHTMLTag_table);
CTagList  gInHTML(1,0,eHTMLTag_html);
CTagList  gInBody(1,0,eHTMLTag_body);
CTagList  gInForm(1,0,eHTMLTag_form);
CTagList  gInFieldset(1,0,eHTMLTag_fieldset);
CTagList  gInTR(1,0,eHTMLTag_tr);
CTagList  gInDL(1,0,eHTMLTag_dl);
CTagList  gInFrameset(1,0,eHTMLTag_frameset);
CTagList  gInNoframes(1,0,eHTMLTag_noframes);
CTagList  gOptgroupParents(2,0,eHTMLTag_optgroup,eHTMLTag_select);
CTagList  gBodyParents(2,0,eHTMLTag_html,eHTMLTag_noframes);
CTagList  gColParents(2,0,eHTMLTag_colgroup,eHTMLTag_table);
CTagList  gFramesetParents(2,0,eHTMLTag_frameset,eHTMLTag_html);
CTagList  gLegendParents(1,0,eHTMLTag_fieldset);
CTagList  gAreaParent(1,0,eHTMLTag_map);
CTagList  gParamParents(2,0,eHTMLTag_applet,eHTMLTag_object);
CTagList  gTRParents(4,0,eHTMLTag_tbody,eHTMLTag_tfoot,eHTMLTag_thead,eHTMLTag_table);


//*********************************************************************************************
//  Next, define the set of taglists for tags with special kids...
//*********************************************************************************************

CTagList  gContainsText(4,0,eHTMLTag_text,eHTMLTag_newline,eHTMLTag_whitespace,eHTMLTag_entity);
CTagList  gContainsHTML(1,0,eHTMLTag_html);
CTagList  gContainsOpts(2,0,eHTMLTag_option,eHTMLTag_optgroup);
CTagList  gContainsParam(1,0,eHTMLTag_param);
CTagList  gColgroupKids(1,0,eHTMLTag_col); 
CTagList  gAddressKids(1,0,eHTMLTag_p);
CTagList  gBodyKids(4,0,eHTMLTag_del,eHTMLTag_ins,eHTMLTag_noscript,eHTMLTag_script);
CTagList  gButtonKids(2,0,eHTMLTag_caption,eHTMLTag_legend);
CTagList  gDLKids(2,0,eHTMLTag_dd,eHTMLTag_dt);
CTagList  gDTKids(1,0,eHTMLTag_dt);
CTagList  gFieldsetKids(2,0,eHTMLTag_legend,eHTMLTag_text);
CTagList  gFontKids(2,0,eHTMLTag_legend,eHTMLTag_text);
CTagList  gFormKids(1,0,eHTMLTag_keygen);
CTagList  gFramesetKids(3,0,eHTMLTag_frame,eHTMLTag_frameset,eHTMLTag_noframes);

static eHTMLTags gHTMLKidList[]={eHTMLTag_body,eHTMLTag_frameset,eHTMLTag_head,eHTMLTag_map,eHTMLTag_noscript,eHTMLTag_script};
CTagList  gHtmlKids(sizeof(gHTMLKidList)/sizeof(eHTMLTag_unknown),gHTMLKidList);

static eHTMLTags gHeadKidList[]={eHTMLTag_base,eHTMLTag_bgsound,eHTMLTag_link,eHTMLTag_meta,eHTMLTag_script,eHTMLTag_style,eHTMLTag_title};
CTagList  gHeadKids(sizeof(gHeadKidList)/sizeof(eHTMLTag_unknown),gHeadKidList);

CTagList  gLIKids(2,0,eHTMLTag_ol,eHTMLTag_ul);
CTagList  gMapKids(1,0,eHTMLTag_area);
CTagList  gNoframesKids(1,0,eHTMLTag_body);
CTagList  gPreKids(1,0,eHTMLTag_hr);

static eHTMLTags gTableList[]={eHTMLTag_caption,eHTMLTag_col,eHTMLTag_colgroup,eHTMLTag_form,eHTMLTag_map,eHTMLTag_thead,eHTMLTag_tbody,eHTMLTag_tfoot,eHTMLTag_script};
CTagList  gTableKids(sizeof(gTableList)/sizeof(eHTMLTag_unknown),gTableList);

static eHTMLTags gTableElemList[]={eHTMLTag_form,eHTMLTag_map,eHTMLTag_noscript,eHTMLTag_script,eHTMLTag_td,eHTMLTag_th,eHTMLTag_tr};
CTagList  gTableElemKids(sizeof(gTableElemList)/sizeof(eHTMLTag_unknown),gTableElemList);

static eHTMLTags gTRList[]={eHTMLTag_caption,eHTMLTag_col,eHTMLTag_colgroup,eHTMLTag_form,eHTMLTag_map,eHTMLTag_thead,eHTMLTag_tbody,eHTMLTag_tfoot,eHTMLTag_script};
CTagList  gTRKids(sizeof(gTRList)/sizeof(eHTMLTag_unknown),gTRList);

CTagList  gTBodyKids(1,0,eHTMLTag_tr);
CTagList  gULKids(2,0,eHTMLTag_li,eHTMLTag_p);


//*********************************************************************************************
// The following tag lists are used to define common set of root notes for the HTML elements...
//*********************************************************************************************

CTagList  gRootTags(2,0,eHTMLTag_body,eHTMLTag_td);
CTagList  gHTMLRootTags(1,0,eHTMLTag_unknown);

static eHTMLTags gLIList[]={eHTMLTag_ul,eHTMLTag_ol,eHTMLTag_dir,eHTMLTag_menu,eHTMLTag_p,eHTMLTag_body,eHTMLTag_td};
CTagList  gLIRootTags(sizeof(gLIList)/sizeof(eHTMLTag_unknown),gLIList);

CTagList  gOLRootTags(3,0,eHTMLTag_body,eHTMLTag_li,eHTMLTag_td);
CTagList  gTextRootTags(2,0,eHTMLTag_p,eHTMLTag_body);
CTagList  gTDRootTags(3,0,eHTMLTag_tr,eHTMLTag_tbody,eHTMLTag_table);
CTagList  gNoframeRoot(2,0,eHTMLTag_body,eHTMLTag_frameset);


//*********************************************************************************************
// The following tag lists are used to define the autoclose properties of the html elements...
//*********************************************************************************************

CTagList  gAutoClose(2,0,eHTMLTag_body,eHTMLTag_td);
CTagList  gBodyAutoClose(1,0,eHTMLTag_head);
CTagList  gCaptionAutoClose(1,0,eHTMLTag_tbody);
CTagList  gLIAutoClose(2,0,eHTMLTag_p,eHTMLTag_li);
CTagList  gPAutoClose(2,0,eHTMLTag_p,eHTMLTag_li);
CTagList  gOLAutoClose(3,0,eHTMLTag_p,eHTMLTag_ol,eHTMLTag_ul);

static eHTMLTags gHxList[]={eHTMLTag_h1,eHTMLTag_h2,eHTMLTag_h3,eHTMLTag_h4,eHTMLTag_h5,eHTMLTag_h6};
CTagList  gHxAutoClose(sizeof(gHxList)/sizeof(eHTMLTag_unknown),gHxList);

CTagList  gTRCloseTags(3,0,eHTMLTag_tr,eHTMLTag_td,eHTMLTag_th);
CTagList  gTDCloseTags(2,0,eHTMLTag_td,eHTMLTag_th);
CTagList  gDTCloseTags(2,0,eHTMLTag_dt,eHTMLTag_dd);
CTagList  gULCloseTags(1,0,eHTMLTag_li);


//*********************************************************************************************
//Lastly, bind tags with their rules, their special parents and special kids.
//*********************************************************************************************


nsHTMLElement gHTMLElements[] = {

  { /*tag*/                             eHTMLTag_unknown,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,&gContainsHTML,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_a,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kSpecial, kInline, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_abbr,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kPhrase, kInline, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_acronym,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kInline|kSelf), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_address,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kBlock, kInline, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,&gAddressKids,eHTMLTag_unknown}, 

  { /*tag*/                             eHTMLTag_applet,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kSpecial, kFlow, kNone,	
    /*control bits*/                    0,
    /*special kids: <PARAM>*/           0,&gContainsParam,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_area,
	  /*rootnodes*/                       &gAreaParent,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFontStyle, kInline, kSelf,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gAreaParent,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_b,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kFlow|kSelf), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_base,
	  /*rootnodes*/                       &gInHead,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gInHead,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_basefont,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          (kSpecial|kPreExclusion), kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_bdo,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kSpecial, (kSelf|kInline), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_bgsound,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFlow, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_big,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          (kFontStyle|kPreformatted), (kFlow|kSelf), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_blink,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          (kFontStyle|kPreformatted), (kFlow|kSelf), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_blockquote,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kBlock, kFlow, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_body,
	  /*rootnodes*/                       &gInHTML,	
    /*autoclose starttags and endtags*/ &gBodyAutoClose,0,
    /*parent,incl,exclgroups*/          kHTMLContent, kFlow, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gInNoframes,&gBodyKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_br,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kSpecial, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_button,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFormControl, kFlow, kFormControl,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,&gButtonKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_caption,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ &gCaptionAutoClose,0,
    /*parent,incl,exclgroups*/          kNone, kInline, kSelf,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gInTable,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_center,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kBlock, (kSelf|kFlow), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_cite,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kInline|kSelf), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_code,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kSelf|kInline), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_col,
	  /*rootnodes*/                       &gColParents,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gColParents,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_colgroup,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gInTable,&gColgroupKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_dd,
	  /*rootnodes*/                       &gInDL,	
    /*autoclose starttags and endtags*/ &gDTCloseTags,0,
    /*parent,incl,exclgroups*/          kNone, kFlow, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gInDL,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_del,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, (kSelf|kFlow), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gInBody,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_dfn,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kSelf|kInline), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_dir,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kBlock, (kSelf|kFlow), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,&gULKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_div,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kBlock, (kSelf|kFlow), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_dl,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kBlock, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,&gDLKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_dt,
	  /*rootnodes*/                       &gInDL,	
    /*autoclose starttags and endtags*/ &gDTCloseTags,0,
    /*parent,incl,exclgroups*/          kNone, kInline, kNone,	
    /*control bits*/                    0,
    /*special parents, kids <DT>*/      &gInDL,&gDTKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_em,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kSelf|kInline), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_embed,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFlow, kFlow, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,&gContainsParam,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_fieldset,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kBlock, kFlow, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,&gFieldsetKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_font,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kSpecial, (kSelf|kFlow), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,&gFontKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_form,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kBlock, kFlow, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,&gFormKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_frame,
	  /*rootnodes*/                       &gInFrameset,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gInFrameset,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_frameset,
	  /*rootnodes*/                       &gInHTML,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, kSelf, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gInHTML,&gFramesetKids,eHTMLTag_unknown},


  { /*tag*/                             eHTMLTag_h1,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ &gHxAutoClose,  &gHxAutoClose,
    /*parent,incl,exclgroups*/          kBlock, kInline, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_h2,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ &gHxAutoClose,  &gHxAutoClose,
    /*parent,incl,exclgroups*/          kBlock, kInline, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_h3,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ &gHxAutoClose,  &gHxAutoClose,
    /*parent,incl,exclgroups*/          kBlock, kInline, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_h4,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ &gHxAutoClose,  &gHxAutoClose,
    /*parent,incl,exclgroups*/          kBlock, kInline, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_h5,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ &gHxAutoClose,  &gHxAutoClose,
    /*parent,incl,exclgroups*/          kBlock, kInline, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_h6,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ &gHxAutoClose,  &gHxAutoClose,
    /*parent,incl,exclgroups*/          kBlock, kInline, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_head,
	  /*rootnodes*/                       &gInHTML,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, (kHeadContent|kHeadMisc), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gInHTML,&gHeadKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_hr,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kBlock, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_html,
	  /*rootnodes*/                       &gHTMLRootTags,	
    /*autoclose starttags and endtags*/ &gAutoClose,0,
    /*parent,incl,exclgroups*/          kNone, kHTMLContent, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,&gHtmlKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_i,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kSelf|kFlow), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_iframe,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kSpecial, kFlow, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_ilayer,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_img,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kSpecial, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_input,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFormControl, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_ins,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, (kSelf|kNone), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_isindex,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          (kBlock|kHeadContent), kFlow, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gInBody,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_kbd,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kSelf|kFlow), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_keygen,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_label,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFormControl, kInline, kSelf,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_layer,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kBlock, kFlow, kSelf,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_legend,
	  /*rootnodes*/                       &gInFieldset,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, kInline, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gInFieldset,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_li,
	  /*rootnodes*/                       &gLIRootTags,	
    /*autoclose starttags and endtags*/ &gLIAutoClose,0,
    /*parent,incl,exclgroups*/          kList, kFlow, kSelf,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,&gLIKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_link,
	  /*rootnodes*/                       &gInHead,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kHeadMisc, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gInHead,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_listing,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_map,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kSpecial, kBlock, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,&gMapKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_menu,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kList, (kSelf|kFlow), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,&gULKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_meta,
	  /*rootnodes*/                       &gInHead,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kHeadMisc, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gInHead,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_multicol,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFlow, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_nobr,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFlow, (kFlow|kSelf), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_noembed, 
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_noembed},

  { /*tag*/                             eHTMLTag_noframes,
	  /*rootnodes*/                       &gNoframeRoot,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kBlock, kFlow, kNone,	
    /*control bits*/                    kDiscardTag,
    /*special parents,kids,skip*/       &gNoframeRoot,&gNoframesKids,eHTMLTag_noframes},

  { /*tag*/                             eHTMLTag_nolayer,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*control bits*/                    kDiscardTag,
    /*special parents,kids,skip*/       0,0,eHTMLTag_nolayer},

  { /*tag*/                             eHTMLTag_noscript,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kBlock, kFlow, kNone,	
    /*control bits*/                    kDiscardTag,
    /*special parents,kids,skip*/       0,0,eHTMLTag_noscript},

  { /*tag*/                             eHTMLTag_object,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          (kHeadMisc|kSpecial), kFlow, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,&gContainsParam,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_ol,
	  /*rootnodes*/                       &gOLRootTags,	
    /*autoclose starttags and endtags*/ &gOLAutoClose,&gULCloseTags,
    /*parent,incl,exclgroups*/          kBlock, (kFlow|kSelf), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,&gULKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_optgroup,
	  /*rootnodes*/                       &gOptgroupParents,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gOptgroupParents,&gContainsOpts,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_option,
	  /*rootnodes*/                       &gOptgroupParents,	 
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, kPCDATA, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gOptgroupParents,&gContainsText,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_p,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kBlock, kInline, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,&gInAddress,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_param,
	  /*rootnodes*/                       &gParamParents,	
    /*autoclose starttags and endtags*/ &gPAutoClose,0,
    /*parent,incl,exclgroups*/          kFlow, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gParamParents,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_plaintext,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFlow, kFlow, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_html},

  { /*tag*/                             eHTMLTag_pre,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kPreformatted, kInline, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,&gPreKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_q,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kSpecial, (kSelf|kInline), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_s,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kSelf|kFlow), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_samp,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kSelf|kInline), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_script,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          (kSpecial|kHeadMisc), kPCDATA, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,&gContainsText,eHTMLTag_script},

  { /*tag*/                             eHTMLTag_select,
	  /*rootnodes*/                       &gInForm,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFormControl, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gInForm,&gContainsOpts,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_server,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_small,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kSelf|kFlow), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_sound,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_spacer,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFlow, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_span,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kSpecial, (kSelf|kInline), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_strike,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kSelf|kFlow), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_strong,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          (kPhrase|kFontStyle), (kSelf|kFlow), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,&gContainsText,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_style,
	  /*rootnodes*/                       &gInHead,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kHeadMisc, kPCDATA, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gInHead,0,eHTMLTag_style},

  { /*tag*/                             eHTMLTag_sub,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kSelf|kFlow), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_sup,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kSelf|kFlow), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_table,
	  /*rootnodes*/                       &gInBody,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kBlock, kNone, kSelf,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,&gTableKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_tbody,
	  /*rootnodes*/                       &gInTable,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kSelf,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gInTable,&gTBodyKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_td,
	  /*rootnodes*/                       &gTDRootTags,	
    /*autoclose starttags and endtags*/ &gTDCloseTags,0,
    /*parent,incl,exclgroups*/          kNone, kFlow, kSelf,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gTDRootTags,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_textarea,
	  /*rootnodes*/                       &gInForm,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFormControl, kPCDATA, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gInForm,&gContainsText,eHTMLTag_textarea},

  { /*tag*/                             eHTMLTag_tfoot,
	  /*rootnodes*/                       &gInTable,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kSelf,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gInTable,&gTableElemKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_th,
	  /*rootnodes*/                       &gTDRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, kFlow, kSelf,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gTDRootTags,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_thead,
	  /*rootnodes*/                       &gInTable,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kSelf,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gInTable,&gTableElemKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_title,
	  /*rootnodes*/                       &gInHead,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, (kHeadMisc|kPCDATA), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gInHead,&gContainsText,eHTMLTag_title},

  { /*tag*/                             eHTMLTag_tr,
	  /*rootnodes*/                       &gTRParents,	
    /*autoclose starttags and endtags*/ &gTRCloseTags,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       &gTRParents,&gTRKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_tt,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kSelf|kFlow), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_u,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kSelf|kFlow), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_ul,
	  /*rootnodes*/                       &gOLRootTags,	
    /*autoclose starttags and endtags*/ &gOLAutoClose,&gULCloseTags,
    /*parent,incl,exclgroups*/          kBlock, (kFlow|kSelf), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,&gULKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_var,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kSelf|kInline), kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_wbr,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFlow, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_xmp,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFlow, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/        0,0,eHTMLTag_xmp},

  { /*tag*/                             eHTMLTag_text,
	  /*rootnodes*/                       &gTextRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFlow, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_whitespace,
	  /*rootnodes*/                       &gTextRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFlow, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_newline,
	  /*rootnodes*/                       &gTextRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFlow, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_comment,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFlow, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_entity,
	  /*rootnodes*/                       &gTextRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kFlow, kNone, kNone,	
    /*control bits*/                    0,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_userdefined,
	  /*rootnodes*/                       &gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*control bits*/                    0, 
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},
};

/**
 * This class is here to finalize the initialization of the
 * nsHTMLElement table.
 */
class CTableInitializer {
public:
  CTableInitializer(){

    /*now initalize tags that can contain themselves...
    int max=sizeof(gStyleTags)/sizeof(eHTMLTag_unknown);
    for(index=0;index<max;index++){
      gHTMLElements[gStyleTags[index]].mSelfContained=PR_TRUE;
    }
    gHTMLElements[eHTMLTag_a].mSelfContained=PR_FALSE;
    gHTMLElements[eHTMLTag_div].mSelfContained=PR_TRUE;
    gHTMLElements[eHTMLTag_frameset].mSelfContained=PR_TRUE;
    gHTMLElements[eHTMLTag_ol].mSelfContained=PR_TRUE;
    gHTMLElements[eHTMLTag_ul].mSelfContained=PR_TRUE;
    */
  }
};
CTableInitializer gTableInitializer;

/**
 * 
 * @update	gess12/13/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsContainer(eHTMLTags aChild) {
  PRBool result=(eHTMLTag_unknown==aChild);

  if(!result){
    static eHTMLTags gNonContainers[]={
      eHTMLTag_unknown,
      eHTMLTag_area,    eHTMLTag_base,    eHTMLTag_basefont,
      eHTMLTag_br,      eHTMLTag_col,     
      eHTMLTag_frame,   eHTMLTag_hr,      eHTMLTag_whitespace,
      eHTMLTag_input,   eHTMLTag_link,    eHTMLTag_isindex,
      eHTMLTag_meta,    eHTMLTag_param,   eHTMLTag_plaintext,
      eHTMLTag_style,   eHTMLTag_spacer,  eHTMLTag_wbr,   
      eHTMLTag_newline, eHTMLTag_text,    eHTMLTag_img,   
      eHTMLTag_unknown, eHTMLTag_xmp};

    result=!FindTagInSet(aChild,gNonContainers,sizeof(gNonContainers)/sizeof(eHTMLTag_unknown));
  }
  return result;
}


/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
inline PRBool TestBits(int aBitset,int aTest) {
  return PRBool(aBitset & aTest);
}

/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsContainerType(eHTMLTags aTag,int aType) {
  PRBool result=PR_FALSE;

  if((aTag>=eHTMLTag_unknown) & (aTag<=eHTMLTag_userdefined)){
    result=TestBits(gHTMLElements[aTag].mInclusionBits,aType);
  } 
  return result;
}

/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsBlockElement(eHTMLTags aTag){
  PRBool result=PR_FALSE;

  if((aTag>=eHTMLTag_unknown) & (aTag<=eHTMLTag_userdefined)){
    result=TestBits(gHTMLElements[aTag].mParentBits,kBlock);
  } 
  return result;
}

/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsBlockCloser(eHTMLTags aTag){
  PRBool result=PR_FALSE;
    
  if((aTag>=eHTMLTag_unknown) & (aTag<=eHTMLTag_userdefined)){

    result=IsFlowElement(aTag);
    if(!result) {

      static eHTMLTags gClosers[]={ eHTMLTag_table,eHTMLTag_caption,eHTMLTag_dd,eHTMLTag_dt,
                                    eHTMLTag_td,eHTMLTag_tfoot,eHTMLTag_th,eHTMLTag_thead,eHTMLTag_tr};
      result=FindTagInSet(aTag,gClosers,sizeof(gClosers)/sizeof(eHTMLTag_body));
    }
  }
  return result;
}


/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsInlineElement(eHTMLTags aTag){
  PRBool result=PR_FALSE;
  if((aTag>=eHTMLTag_unknown) & (aTag<=eHTMLTag_userdefined)){
    result=TestBits(gHTMLElements[aTag].mParentBits,kInline);
  } 
  return result;
}

/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsFlowElement(eHTMLTags aTag){
  PRBool result=PR_FALSE;

  if((aTag>=eHTMLTag_unknown) & (aTag<=eHTMLTag_userdefined)){
    result=TestBits(gHTMLElements[aTag].mParentBits,kFlow);
  } 
  return result;
}

/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsBlockParent(eHTMLTags aTag){
  PRBool result=PR_FALSE;
  if((aTag>=eHTMLTag_unknown) & (aTag<=eHTMLTag_userdefined)){
    result=TestBits(gHTMLElements[aTag].mInclusionBits,kBlock);
  } 
  return result;
}


/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsInlineParent(eHTMLTags aTag){
  PRBool result=PR_FALSE;
  if((aTag>=eHTMLTag_unknown) & (aTag<=eHTMLTag_userdefined)){
    result=TestBits(gHTMLElements[aTag].mInclusionBits,kInline);
  } 
  return result;
}


/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsFlowParent(eHTMLTags aTag){
  PRBool result=PR_FALSE;
  if((aTag>=eHTMLTag_unknown) & (aTag<=eHTMLTag_userdefined)){
    result=TestBits(gHTMLElements[aTag].mInclusionBits,kFlow);
  } 
  return result;
}

/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
PRBool nsHTMLElement::CanContain(eHTMLTags aParent,eHTMLTags aChild){
  PRBool result=PR_FALSE;
  if((aParent>=eHTMLTag_unknown) & (aParent<=eHTMLTag_userdefined)){
    result=gHTMLElements[aParent].CanContain(aChild);
  } 
  return result;
}

/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
PRBool nsHTMLElement::CanOmitEndTag(eHTMLTags aChild) const{
  PRBool result=PR_FALSE;
  return result;
}

/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
PRBool nsHTMLElement::CanOmitStartTag(eHTMLTags aChild) const{
  PRBool result=PR_FALSE;
  return result;
}

/**
 * 
 * @update	gess12/13/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsChildOfHead(eHTMLTags aChild) {
  PRBool result=FindTagInSet(aChild,gHeadKidList,sizeof(gHeadKidList)/sizeof(eHTMLTag_body));
  return result;
}

/**
 * 
 * @update	gess12/13/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsStyleTag(eHTMLTags aChild) {
  PRBool result=FindTagInSet(aChild,gStyleTags,sizeof(gStyleTags)/sizeof(eHTMLTag_body));
  return result;
}

/**
 * 
 * @update	gess12/13/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsTextTag(eHTMLTags aChild) {
  static eHTMLTags gTextTags[]={eHTMLTag_text,eHTMLTag_entity,eHTMLTag_newline, eHTMLTag_whitespace};
  PRBool result=FindTagInSet(aChild,gTextTags,sizeof(gTextTags)/sizeof(eHTMLTag_body));
  return result;
}

PRBool nsHTMLElement::CanContainSelf() const {
  PRBool result=PRBool(TestBits(mInclusionBits,kSelf)!=0);
  return result;
}

/**
 * See whether this tag can DIRECTLY contain the given child.
 * @update	gess12/13/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::CanContain(eHTMLTags aChild) const{

  if(IsContainer(mTagID)){

    if(mTagID==aChild) {
      return CanContainSelf();  //not many tags can contain themselves...
    }

    CTagList* theCloseTags=gHTMLElements[aChild].GetAutoCloseStartTags();
    if(theCloseTags){
      if(theCloseTags->Contains(mTagID))
        return PR_FALSE;
    }

    if(nsHTMLElement::IsInlineElement(aChild)){
      if(nsHTMLElement::IsInlineParent(mTagID)){
        return PR_TRUE;
      }
    }

    if(nsHTMLElement::IsFlowElement(aChild)) {
      if(nsHTMLElement::IsFlowParent(mTagID)){
        return PR_TRUE;
      }
    }

    if(nsHTMLElement::IsTextTag(aChild)) {
      if(nsHTMLElement::IsInlineParent(mTagID)){
        return PR_TRUE;
      }
    }

    if(nsHTMLElement::IsBlockElement(aChild)){
      if(nsHTMLElement::IsBlockParent(mTagID) || IsStyleTag(mTagID)){
        return PR_TRUE;
      }
    }

    if(mSpecialKids) {
      if(mSpecialKids->Contains(aChild)) {
        return PR_TRUE;
      }
    }

    if(mTagID!=eHTMLTag_server){
      int x=5;
    }
  }
  
  return PR_FALSE;
}


/**
 * 
 * @update	gess1/21/99
 * @param 
 * @return
 */
PRBool nsHTMLElement::HasSpecialProperty(PRInt32 aProperty) const{
  PRBool result=TestBits(mSpecialProperties,aProperty);
  return result;
}

