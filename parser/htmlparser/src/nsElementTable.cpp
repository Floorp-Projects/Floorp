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
  PRBool result=PR_FALSE;
  if(mTagList) {
    result=FindTagInSet(aTag,mTagList,mCount);
  }
  else result=FindTagInSet(aTag,mTags,mCount);
  return result;
}

/**
 * 
 * @update	gess 01/04/99
 * @param  
 * @return
 */
PRInt32 CTagList::GetTopmostIndexOf(nsEntryStack& aTagStack){
  int max = aTagStack.GetCount();
  int index;
  for(index=max-1;index>=0;index--){
    if(Contains(aTagStack[index])) {
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
PRInt32 CTagList::GetBottommostIndexOf(nsEntryStack& aTagStack,PRInt32 aStartOffset){
  int max = aTagStack.GetCount();
  int index;
  for(index=aStartOffset;index<max;index++){
    if(Contains(aTagStack[index])) {
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
eHTMLTags CTagList::GetTagAt(PRUint32 anIndex) const{
  eHTMLTags result=eHTMLTag_unknown;
  if(anIndex<mCount){
    result=(mTagList) ? mTagList[anIndex] : mTags[anIndex];
  }
  return result;
}

 
static eHTMLTags gStyleTags[]={
  eHTMLTag_a,       eHTMLTag_acronym,   eHTMLTag_b,
  eHTMLTag_bdo,     eHTMLTag_big,       eHTMLTag_blink,
  eHTMLTag_center,  eHTMLTag_cite,      eHTMLTag_code,
  eHTMLTag_del,     eHTMLTag_dfn,       eHTMLTag_em,
  eHTMLTag_font,    eHTMLTag_i,         eHTMLTag_ins,
  eHTMLTag_kbd,     eHTMLTag_q,
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
CTagList  gInDL(2,0,eHTMLTag_dl,eHTMLTag_body);
CTagList  gInFrameset(1,0,eHTMLTag_frameset);
CTagList  gInNoframes(1,0,eHTMLTag_noframes);
CTagList  gInP(4,0,eHTMLTag_address,eHTMLTag_form,eHTMLTag_span,eHTMLTag_table);
CTagList  gOptgroupParents(2,0,eHTMLTag_optgroup,eHTMLTag_select);
CTagList  gBodyParents(2,0,eHTMLTag_html,eHTMLTag_noframes);
CTagList  gColParents(2,0,eHTMLTag_table,eHTMLTag_colgroup);
CTagList  gFramesetParents(2,0,eHTMLTag_html,eHTMLTag_frameset);
CTagList  gLegendParents(1,0,eHTMLTag_fieldset);
CTagList  gAreaParent(1,0,eHTMLTag_map);
CTagList  gParamParents(2,0,eHTMLTag_applet,eHTMLTag_object);
CTagList  gTRParents(4,0,eHTMLTag_tbody,eHTMLTag_tfoot,eHTMLTag_thead,eHTMLTag_table);

static eHTMLTags gTREndParentList[]={eHTMLTag_tbody,eHTMLTag_tfoot,eHTMLTag_thead,eHTMLTag_table,eHTMLTag_td,eHTMLTag_th};
CTagList  gTREndParents(sizeof(gTREndParentList)/sizeof(eHTMLTag_unknown),gTREndParentList);


//*********************************************************************************************
//  Next, define the set of taglists for tags with special kids...
//*********************************************************************************************

CTagList  gContainsText(4,0,eHTMLTag_text,eHTMLTag_newline,eHTMLTag_whitespace,eHTMLTag_entity);
CTagList  gUnknownKids(2,0,eHTMLTag_html,eHTMLTag_frameset);
CTagList  gContainsOpts(3,0,eHTMLTag_option,eHTMLTag_optgroup,eHTMLTag_script);
CTagList  gContainsParam(1,0,eHTMLTag_param);
CTagList  gColgroupKids(1,0,eHTMLTag_col); 
CTagList  gAddressKids(1,0,eHTMLTag_p);
CTagList  gBodyKids(5,0,eHTMLTag_del,eHTMLTag_ins,eHTMLTag_noscript,eHTMLTag_script,eHTMLTag_li);
CTagList  gButtonKids(2,0,eHTMLTag_caption,eHTMLTag_legend);
CTagList  gDLKids(2,0,eHTMLTag_dd,eHTMLTag_dt);
CTagList  gDTKids(1,0,eHTMLTag_dt);
CTagList  gFieldsetKids(2,0,eHTMLTag_legend,eHTMLTag_text);
CTagList  gFontKids(2,0,eHTMLTag_legend,eHTMLTag_text);
CTagList  gFormKids(1,0,eHTMLTag_keygen);
CTagList  gFramesetKids(3,0,eHTMLTag_frame,eHTMLTag_frameset,eHTMLTag_noframes);

static eHTMLTags gHTMLKidList[]={eHTMLTag_body,eHTMLTag_frameset,eHTMLTag_head,eHTMLTag_map,eHTMLTag_noscript,eHTMLTag_script};
CTagList  gHtmlKids(sizeof(gHTMLKidList)/sizeof(eHTMLTag_unknown),gHTMLKidList);

static eHTMLTags gHeadKidList[]=
  {eHTMLTag_base,eHTMLTag_bgsound,eHTMLTag_link,eHTMLTag_meta,eHTMLTag_script,eHTMLTag_style,eHTMLTag_title,eHTMLTag_noembed,eHTMLTag_noscript};
CTagList  gHeadKids(sizeof(gHeadKidList)/sizeof(eHTMLTag_unknown),gHeadKidList);

CTagList  gLIKids(2,0,eHTMLTag_ol,eHTMLTag_ul);
CTagList  gMapKids(1,0,eHTMLTag_area);
CTagList  gNoframesKids(1,0,eHTMLTag_body);
CTagList  gPreKids(2,0,eHTMLTag_hr,eHTMLTag_center);  //note that CENTER is here for backward compatibility; it's not 4.0 spec.

static eHTMLTags gTableList[]={ eHTMLTag_caption,eHTMLTag_col,eHTMLTag_colgroup,eHTMLTag_form,
                                // eHTMLTag_td,
                                eHTMLTag_thead,eHTMLTag_tbody,eHTMLTag_tfoot,
                                eHTMLTag_map,eHTMLTag_script,eHTMLTag_input};
CTagList  gTableKids(sizeof(gTableList)/sizeof(eHTMLTag_unknown),gTableList);

static eHTMLTags gTableElemList[]={eHTMLTag_form,eHTMLTag_map,eHTMLTag_noscript,eHTMLTag_script,eHTMLTag_td,eHTMLTag_th,eHTMLTag_tr};
CTagList  gTableElemKids(sizeof(gTableElemList)/sizeof(eHTMLTag_unknown),gTableElemList);

static eHTMLTags gTRList[]=   {eHTMLTag_td,eHTMLTag_th,eHTMLTag_map,eHTMLTag_form,eHTMLTag_script,eHTMLTag_input};
CTagList  gTRKids(sizeof(gTRList)/sizeof(eHTMLTag_unknown),gTRList);
 
CTagList  gTBodyKids(3,0,eHTMLTag_tr,eHTMLTag_form,eHTMLTag_input);
CTagList  gULKids(2,0,eHTMLTag_li,eHTMLTag_p);


//*********************************************************************************************
// The following tag lists are used to define common set of root notes for the HTML elements...
//*********************************************************************************************

CTagList  gRootTags(3,0,eHTMLTag_body,eHTMLTag_td,eHTMLTag_table);
CTagList  gHTMLRootTags(1,0,eHTMLTag_unknown);

static eHTMLTags gLIList[]={eHTMLTag_ul,eHTMLTag_ol,eHTMLTag_dir,eHTMLTag_menu,eHTMLTag_p,eHTMLTag_body,eHTMLTag_td};
CTagList  gLIRootTags(sizeof(gLIList)/sizeof(eHTMLTag_unknown),gLIList);

CTagList  gOLRootTags(3,0,eHTMLTag_body,eHTMLTag_li,eHTMLTag_td);
CTagList  gTDRootTags(3,0,eHTMLTag_tr,eHTMLTag_tbody,eHTMLTag_table);
CTagList  gNoframeRoot(2,0,eHTMLTag_body,eHTMLTag_frameset);


//*********************************************************************************************
// The following tag lists are used to define the autoclose properties of the html elements...
//*********************************************************************************************

CTagList  gAutoClose(2,0,eHTMLTag_body,eHTMLTag_td);
CTagList  gBodyAutoClose(1,0,eHTMLTag_head);
CTagList  gTBodyAutoClose(3,0,eHTMLTag_thead,eHTMLTag_tfoot,eHTMLTag_tbody);
CTagList  gCaptionAutoClose(1,0,eHTMLTag_tbody);
CTagList  gLIAutoClose(2,0,eHTMLTag_p,eHTMLTag_li);
CTagList  gPAutoClose(2,0,eHTMLTag_p,eHTMLTag_li);
CTagList  gHRAutoClose(1,0,eHTMLTag_p);
CTagList  gOLAutoClose(3,0,eHTMLTag_p,eHTMLTag_ol,eHTMLTag_ul);
CTagList  gDivAutoClose(1,0,eHTMLTag_p);

static eHTMLTags gHxList[]={eHTMLTag_h1,eHTMLTag_h2,eHTMLTag_h3,eHTMLTag_h4,eHTMLTag_h5,eHTMLTag_h6};
CTagList  gHeadingTags(sizeof(gHxList)/sizeof(eHTMLTag_unknown),gHxList);

CTagList  gTRCloseTags(3,0,eHTMLTag_tr,eHTMLTag_td,eHTMLTag_th);
CTagList  gTDCloseTags(2,0,eHTMLTag_td,eHTMLTag_th);
CTagList  gDTCloseTags(3,0,eHTMLTag_dt,eHTMLTag_dd,eHTMLTag_p);
CTagList  gULCloseTags(1,0,eHTMLTag_li);


//*********************************************************************************************
//Lastly, bind tags with their rules, their special parents and special kids.
//*********************************************************************************************


#define FSTYPE kInlineEntity
#define SPECIALTYPE kInlineEntity

const int kNoPropRange=0;
const int kDefaultPropRange=1;
const int kBodyPropRange=2;

nsHTMLElement gHTMLElements[] = {

  { /*tag*/                             eHTMLTag_unknown,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,		
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*special props, prop-range*/       kOmitWS, 10,
    /*special parents,kids,skip*/       0,&gUnknownKids,eHTMLTag_unknown},


  /*************************************************
    Note: I changed A to contain flow elements
          since it's such a popular (but illegal) 
          idiom.
   *************************************************/

  { /*tag*/                             eHTMLTag_a,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, SPECIALTYPE|kBlockEntity, kNone,	  
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_abbr,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kSelf|kFlowEntity), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_acronym,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kFlowEntity|kSelf), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_address,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kBlock, kInlineEntity, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gAddressKids,eHTMLTag_unknown}, 

  { /*tag*/                             eHTMLTag_applet,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, (kSelf|SPECIALTYPE), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gContainsParam,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_area,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gAreaParent,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kNone, kInlineEntity, kSelf,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       &gAreaParent,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_b,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (FSTYPE|kSelf), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_base,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gInHead,	&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*special props, prop-range*/       0, kNoPropRange,
    /*special parents,kids,skip*/       &gInHead,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_basefont,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, SPECIALTYPE, kNone,	
    /*special props, prop-range*/       0, kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_bdo,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, (kSelf|SPECIALTYPE), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_bgsound,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kExtensions, kNone, kNone,	
    /*special props, prop-range*/       0,kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_big,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (FSTYPE|kSelf), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_blink,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kFlowEntity|kSelf), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_blockquote,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kBlock, (kSelf|kFlowEntity), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_body,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_frameset,
	  /*rootnodes,endrootnodes*/          &gInHTML,	&gInHTML,
    /*autoclose starttags and endtags*/ &gBodyAutoClose,0,0,
    /*parent,incl,exclgroups*/          kHTMLContent,(kFlowEntity|kSelf), kNone,	
    /*special props, prop-range*/       kOmitEndTag|kLegalOpen, kBodyPropRange,
    /*special parents,kids,skip*/       &gInNoframes,&gBodyKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_br,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, kNone, kNone,	
    /*special props, prop-range*/       0, kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_button,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kFormControl, kFlowEntity, kFormControl,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gButtonKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_caption,
    /*req-parent excl-parent*/          eHTMLTag_table,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gInTable,&gInTable,	
    /*autoclose starttags and endtags*/ &gCaptionAutoClose,0,0,
    /*parent,incl,exclgroups*/          kNone, kFlowEntity, kSelf,	
    /*special props, prop-range*/       kNoPropagate,kDefaultPropRange,
    /*special parents,kids,skip*/       &gInTable,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_center,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kBlock, (kInlineEntity|kSelf|kFlowEntity), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_cite,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kFlowEntity|kSelf), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_code,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kSelf|kFlowEntity), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_col,
    /*req-parent excl-parent*/          eHTMLTag_table,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gColParents,&gColParents,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*special props, prop-range*/       kNoPropagate|kOmitWS,kDefaultPropRange,
    /*special parents,kids,skip*/       &gColParents,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_colgroup,
    /*req-parent excl-parent*/          eHTMLTag_table,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gInTable,&gInTable,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*special props, prop-range*/       kOmitWS|kNoPropagate,kDefaultPropRange,
    /*special parents,kids,skip*/       &gInTable,&gColgroupKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_dd,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,	&gRootTags,	
    /*autoclose starttags and endtags*/ &gDTCloseTags,0,0,
    /*parent,incl,exclgroups*/          kInlineEntity, kFlowEntity, kNone,	
    /*special props, prop-range*/       kNoPropagate|kMustCloseSelf,kDefaultPropRange,
    /*special parents,kids,skip*/       &gInDL,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_del,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kFlowEntity, (kSelf|kFlowEntity), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       &gInBody,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_dfn,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kSelf|kFlowEntity), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_dir,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kList, (kSelf|kFlowEntity), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gULKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_div,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ &gDivAutoClose,0,0,
    /*parent,incl,exclgroups*/          kBlock, (kSelf|kFlowEntity), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_dl,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kBlock, kSelf|kFlowEntity, kNone,	
    /*special props, prop-range*/       kOmitWS, kNoPropRange,
    /*special parents,kids,skip*/       0,&gDLKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_dt,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,	&gRootTags,	
    /*autoclose starttags and endtags*/ &gDTCloseTags,0,0,
    /*parent,incl,exclgroups*/          kInlineEntity, kFlowEntity, kNone,	
    /*special props, prop-range*/       (kNoPropagate|kMustCloseSelf),kDefaultPropRange,
    /*special parents,kids,skip*/       &gInDL,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_em,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kSelf|kFlowEntity), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_embed,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kBlockEntity, kNone, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gContainsParam,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_endnote,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kFlowEntity, kFlowEntity, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_fieldset,
    /*requiredAncestor*/                eHTMLTag_form,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kBlock, (kSelf|kFlowEntity), kNone,	
    /*special props, prop-range*/       kNoPropagate|kOmitWS,kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gFieldsetKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_font,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, (kSelf|SPECIALTYPE), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gFontKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_form,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kBlock, kFlowEntity, kNone,	
    /*special props, prop-range*/       kNoStyleLeaksIn, kNoPropRange,
    /*special parents,kids,skip*/       0,&gFormKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_frame, 
    /*req-parent excl-parent*/          eHTMLTag_frameset,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gInFrameset,&gInFrameset,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*special props, prop-range*/       kNoPropagate|kNoStyleLeaksIn, kNoPropRange,
    /*special parents,kids,skip*/       &gInFrameset,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_frameset,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_body,
	  /*rootnodes,endrootnodes*/          &gFramesetParents,&gInHTML,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kHTMLContent, kSelf, kAllTags,	
    /*special props, prop-range*/       kOmitWS|kNoPropagate|kNoStyleLeaksIn, kNoPropRange,
    /*special parents,kids,skip*/       &gInHTML,&gFramesetKids,eHTMLTag_unknown},


  { /*tag*/                             eHTMLTag_h1,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ &gHeadingTags,  &gHeadingTags, &gHeadingTags,
    /*parent,incl,exclgroups*/          kHeading, kFlowEntity, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_h2,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ &gHeadingTags,  &gHeadingTags, &gHeadingTags,
    /*parent,incl,exclgroups*/          kHeading, kFlowEntity, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_h3,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ &gHeadingTags,  &gHeadingTags, &gHeadingTags,
    /*parent,incl,exclgroups*/          kHeading, kFlowEntity, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_h4,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ &gHeadingTags,  &gHeadingTags, &gHeadingTags,
    /*parent,incl,exclgroups*/          kHeading, kFlowEntity, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_h5,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ &gHeadingTags,  &gHeadingTags, &gHeadingTags,
    /*parent,incl,exclgroups*/          kHeading, kFlowEntity, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_h6,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ &gHeadingTags,  &gHeadingTags, &gHeadingTags,
    /*parent,incl,exclgroups*/          kHeading, kFlowEntity, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_head,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gInHTML,	&gInHTML,
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kHTMLContent, (kHeadContent|kHeadMisc), kNone,	
    /*special props, prop-range*/       kOmitWS|kNoStyleLeaksIn, kDefaultPropRange,
    /*special parents,kids,skip*/       &gInHTML,&gHeadKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_hr,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ &gHRAutoClose,0,0,
    /*parent,incl,exclgroups*/          kBlock, kNone, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_html,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_html,
	  /*rootnodes,endrootnodes*/          &gHTMLRootTags,	&gHTMLRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kNone, kHTMLContent, kNone,	
    /*special props, prop-range*/       kSaveMisplaced|kOmitEndTag|kOmitWS|kNoStyleLeaksIn, kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gHtmlKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_i,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kSelf|FSTYPE), kNone,	
    /*special props, prop-range*/       0, kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_iframe,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, (kSelf|kFlowEntity), kNone,	
    /*special props, prop-range*/       kNoStyleLeaksIn, kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_ilayer,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kBlock, kFlowEntity, kNone,	
    /*special props, prop-range*/       kNoStyleLeaksIn, kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_image,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, kNone, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_img,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, kNone, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_input,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kFormControl, kNone, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_ins,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kFlowEntity, (kSelf|kFlowEntity), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_isindex,
    /*requiredAncestor*/                eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          (kBlock|kHeadContent), kFlowEntity, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       &gInBody,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_kbd,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kSelf|kFlowEntity), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_keygen,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_label,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kFormControl, kInlineEntity, kSelf,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_layer,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kBlock, kFlowEntity|kSelf, kSelf,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_legend,
    /*requiredAncestor*/                eHTMLTag_form,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gInFieldset,&gInFieldset,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kNone, kInlineEntity, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       &gInFieldset,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_li,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gLIRootTags,&gLIRootTags,	
    /*autoclose starttags and endtags*/ &gLIAutoClose,0,0,
    /*parent,incl,exclgroups*/          kFlowEntity, kFlowEntity, kSelf,	
    /*special props, prop-range*/       kNoPropagate, kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gLIKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_link,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gInHead,&gInHead,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kHeadMisc, kNone, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       &gInHead,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_listing,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kPreformatted, kNone, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_map,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, SPECIALTYPE|kBlockEntity, kNone,	
    /*special props, prop-range*/       kOmitWS, kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gMapKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_menu,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kList, (kSelf|kFlowEntity), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gULKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_meta,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gInHead,	&gInHead,
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kHeadMisc, kNone, kNone,	
    /*special props, prop-range*/       kNoStyleLeaksIn, kDefaultPropRange,
    /*special parents,kids,skip*/       &gInHead,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_multicol,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kExtensions, kNone, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_nobr,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kExtensions, (kFlowEntity), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_noembed, 
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*special props, prop-range*/       kDiscardTag, kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_noembed},

  { /*tag*/                             eHTMLTag_noframes,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gNoframeRoot,&gNoframeRoot,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kBlock, kFlowEntity, kNone,	
    /*special props, prop-range*/       kDiscardTag, kNoPropRange,
    /*special parents,kids,skip*/       &gNoframeRoot,&gNoframesKids,eHTMLTag_noframes},

  { /*tag*/                             eHTMLTag_nolayer,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*special props, prop-range*/       kDiscardTag, kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_nolayer},

  { /*tag*/                             eHTMLTag_noscript,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kBlock, kFlowEntity, kNone,	
    /*special props, prop-range*/       kDiscardTag, kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_noscript},

  { /*tag*/                             eHTMLTag_object,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          (kHeadMisc|kSpecial), (SPECIALTYPE|kSelf), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gContainsParam,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_ol,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gOLRootTags,&gOLRootTags,	
    /*autoclose starttags and endtags*/ &gOLAutoClose, &gULCloseTags, 0,
    /*parent,incl,exclgroups*/          kList, (kFlowEntity|kSelf), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gULKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_optgroup,
    /*requiredAncestor*/                eHTMLTag_select,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gOptgroupParents,&gOptgroupParents,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       &gOptgroupParents,&gContainsOpts,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_option,
    /*requiredAncestor*/                eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gOptgroupParents,&gOptgroupParents,	 
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kNone, kPCDATA, kFlowEntity,	
    /*special props, prop-range*/       kNoPropagate|kNoStyleLeaksIn, kDefaultPropRange,
    /*special parents,kids,skip*/       &gOptgroupParents,&gContainsText,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_p,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gInBody,&gInBody,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kBlock, kInlineEntity, kNone,	  //this used to contain FLOW. But it's really an inline container.
    /*special props, prop-range*/       0,kDefaultPropRange,                      //otherwise it tries to contain things like H1..H6
    /*special parents,kids,skip*/       0,&gInP,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_param,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gParamParents,	&gParamParents,	
    /*autoclose starttags and endtags*/ &gPAutoClose,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*special props, prop-range*/       0, kNoPropRange,
    /*special parents,kids,skip*/       &gParamParents,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_parsererror,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ &gDivAutoClose,0,0,
    /*parent,incl,exclgroups*/          kNone, (kSelf|kFlowEntity), kNone,	
    /*special props, prop-range*/       0, kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_plaintext,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kExtensions, kFlowEntity, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_html},

  { /*tag*/                             eHTMLTag_pre,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kPreformatted, kFlowEntity, kNone,	//I'm allowing WAY too much in here. Spec says inline.
    /*special props, prop-range*/       kNoStyleLeaksIn, kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gPreKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_q,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, (kSelf|SPECIALTYPE), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},
 
  { /*tag*/                             eHTMLTag_s,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kSelf|FSTYPE), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_samp,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kSelf|kFlowEntity), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_script,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          (kSpecial|kHeadMisc), kPCDATA, kNone,	
    /*special props, prop-range*/       kNoStyleLeaksIn, kNoPropRange,
    /*special parents,kids,skip*/       0,&gContainsText,eHTMLTag_script},

  { /*tag*/                             eHTMLTag_select,
    /*requiredAncestor*/                eHTMLTag_unknown, eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gInForm,&gInForm,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kFormControl, kNone, kFlowEntity,	
    /*special props, prop-range*/       kNoPropagate|kOmitWS|kNoStyleLeaksIn, kDefaultPropRange,
    /*special parents,kids,skip*/       &gInForm,&gContainsOpts,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_server,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*special props, prop-range*/       kNoStyleLeaksIn, kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_small,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kSelf|FSTYPE), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_sound,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_sourcetext,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ &gDivAutoClose,0,0,
    /*parent,incl,exclgroups*/          kNone, (kSelf|kFlowEntity), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_spacer,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kExtensions, kNone, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_span,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kBlock, (kInlineEntity|kSelf|kFlowEntity), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_strike,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kSelf|FSTYPE), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_strong,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kSelf|kFlowEntity), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gContainsText,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_style,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gInHead,	&gInHead,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kHeadMisc, kPCDATA, kNone,	
    /*special props, prop-range*/       kNoStyleLeaksIn, kNoPropRange,
    /*special parents,kids,skip*/       &gInHead,0,eHTMLTag_style},

  { /*tag*/                             eHTMLTag_sub,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, (kSelf|SPECIALTYPE), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_sup,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, (kSelf|SPECIALTYPE), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_table,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gInBody,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kBlock, kNone, (kSelf|kInlineEntity),	
    /*special props, prop-range*/       (kOmitWS|kBadContentWatch|kNoStyleLeaksIn), 2,
    /*special parents,kids,skip*/       0,&gTableKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_tbody,
    /*requiredAncestor*/                eHTMLTag_table, eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gInTable,	&gInTable,	
    /*autoclose starttags and endtags*/ &gTBodyAutoClose,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, (kSelf|kInlineEntity),	
    /*special props, prop-range*/       (kNoPropagate|kOmitWS|kBadContentWatch|kNoStyleLeaksIn), kDefaultPropRange,
    /*special parents,kids,skip*/       &gInTable,&gTBodyKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_td,
    /*requiredAncestor*/                eHTMLTag_table, eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gTDRootTags,&gTDRootTags,	
    /*autoclose starttags and endtags*/ &gTDCloseTags,&gTDCloseTags,0,
    /*parent,incl,exclgroups*/          kNone, kFlowEntity, kSelf,	
    /*special props, prop-range*/       kNoStyleLeaksIn, kDefaultPropRange,
    /*special parents,kids,skip*/       &gTDRootTags,&gBodyKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_textarea,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gInForm,	&gInForm,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kFormControl, kPCDATA, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       &gInForm,&gContainsText,eHTMLTag_textarea},

  { /*tag*/                             eHTMLTag_tfoot,
    /*requiredAncestor*/                eHTMLTag_table, eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gInTable,	&gInTable,
    /*autoclose starttags and endtags*/ &gTBodyAutoClose,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kSelf,	
    /*special props, prop-range*/       (kNoPropagate|kOmitWS|kBadContentWatch|kNoStyleLeaksIn), kNoPropRange,
    /*special parents,kids,skip*/       &gInTable,&gTableElemKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_th, 
    /*requiredAncestor*/                eHTMLTag_table, eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gTDRootTags,&gTDRootTags,	
    /*autoclose starttags and endtags*/ &gTDCloseTags,&gTDCloseTags,0,
    /*parent,incl,exclgroups*/          kNone, kFlowEntity, kSelf,	
    /*special props, prop-range*/       kNoStyleLeaksIn, kDefaultPropRange,
    /*special parents,kids,skip*/       &gTDRootTags,&gBodyKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_thead,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gInTable,&gInTable,		
    /*autoclose starttags and endtags*/ &gTBodyAutoClose,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kSelf,	
    /*special props, prop-range*/       (kNoPropagate|kOmitWS|kBadContentWatch|kNoStyleLeaksIn), kNoPropRange,
    /*special parents,kids,skip*/       &gInTable,&gTableElemKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_title,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gInHead,&gInHead,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kHeadMisc,kPCDATA, kNone,	
    /*special props, prop-range*/       kOmitWS|kNoStyleLeaksIn, kNoPropRange,
    /*special parents,kids,skip*/       &gInHead,&gContainsText,eHTMLTag_title},

  { /*tag*/                             eHTMLTag_tr,
    /*requiredAncestor*/                eHTMLTag_table, eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gTRParents,&gTREndParents,	
    /*autoclose starttags and endtags*/ &gTRCloseTags,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kInlineEntity,	
    /*special props, prop-range*/       (kOmitWS|kBadContentWatch|kNoStyleLeaksIn), kNoPropRange,
    /*special parents,kids,skip*/       &gTRParents,&gTRKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_tt,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kSelf|FSTYPE), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_u,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kSelf|FSTYPE), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_ul,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gOLRootTags,&gOLRootTags,	
    /*autoclose starttags and endtags*/ &gOLAutoClose,&gULCloseTags,0,
    /*parent,incl,exclgroups*/          kList, (kFlowEntity|kSelf), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gULKids,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_var,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kSelf|kFlowEntity), kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_wbr,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kExtensions, kNone, kNone,	
    /*special props, prop-range*/       0,kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_xmp,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kPreformatted, kNone, kNone,	
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_xmp},

  { /*tag*/                             eHTMLTag_text,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gInBody,&gInBody,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kFlowEntity, kNone, kNone,	
    /*special props, prop-range*/       0,kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_whitespace, 
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gInBody,&gInBody,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kFlowEntity, kNone, kNone,	
    /*special props, prop-range*/       0,kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_newline,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gInBody,&gInBody,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kFlowEntity, kNone, kNone,	
    /*special props, prop-range*/       0, kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_comment,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kFlowEntity, kNone, kNone,	
    /*special props, prop-range*/       kOmitEndTag,kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_entity,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gInBody,&gInBody,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kFlowEntity, kNone, kNone,	
    /*special props, prop-range*/       0, kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown},

  { /*tag*/                             eHTMLTag_userdefined,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
	  /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,	
    /*autoclose starttags and endtags*/ 0,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,	
    /*special props, prop-range*/       kOmitEndTag, kNoPropRange,
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

int nsHTMLElement::GetSynonymousGroups(int aGroup) {
  int result=0;

  switch(aGroup) {

    case kPhrase:
    case kSpecial:
    case kFontStyle: 
      result=aGroup; 
      break;

    case kHTMLContent:
    case kHeadContent:
    case kHeadMisc:
    case kFormControl:
    case kPreformatted:
    case kHeading:
    case kBlockMisc:
    case kBlock:
    case kList:
    case kPCDATA:
    case kExtensions:
    case kTable:
    case kSelf:
    case kInlineEntity:
    case kBlockEntity:
    case kFlowEntity:
    case kAllTags:
    default:
      break;
  }

  return result;
}

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
      eHTMLTag_area,    eHTMLTag_base,      eHTMLTag_basefont,
      eHTMLTag_br,      eHTMLTag_col,       eHTMLTag_embed,
      eHTMLTag_frame,   eHTMLTag_hr,        
      eHTMLTag_img,     eHTMLTag_image,     eHTMLTag_input,   
      eHTMLTag_link,    eHTMLTag_isindex,   eHTMLTag_meta,    
      eHTMLTag_newline, eHTMLTag_param,     eHTMLTag_plaintext, 
      eHTMLTag_style,   eHTMLTag_spacer,    eHTMLTag_text,    
      eHTMLTag_unknown, eHTMLTag_wbr,       eHTMLTag_whitespace,
      eHTMLTag_xmp};


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
  PRInt32 result=aBitset & aTest;
  return (aTest) ? PRBool(result==aTest) : PR_FALSE;  //was aTest
}


/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsBlockEntity(eHTMLTags aTag){
  PRBool result=PR_FALSE;

  if((aTag>=eHTMLTag_unknown) & (aTag<=eHTMLTag_userdefined)){
    result=TestBits(gHTMLElements[aTag].mParentBits,kBlockEntity);
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

//    result=IsFlowElement(aTag);
    result=gHTMLElements[aTag].IsMemberOf(kBlockEntity);  //was kFlowEntity...
    if(!result) {

      static eHTMLTags gClosers[]={ eHTMLTag_table,eHTMLTag_tbody,eHTMLTag_caption,eHTMLTag_dd,eHTMLTag_dt,
                                    eHTMLTag_td,eHTMLTag_tfoot,eHTMLTag_th,eHTMLTag_thead,eHTMLTag_tr,
                                    eHTMLTag_optgroup};
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
PRBool nsHTMLElement::IsInlineEntity(eHTMLTags aTag){
  PRBool result=PR_FALSE;
  if((aTag>=eHTMLTag_unknown) & (aTag<=eHTMLTag_userdefined)){
    result=TestBits(gHTMLElements[aTag].mParentBits,kInlineEntity);
  } 
  return result;
}

/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsFlowEntity(eHTMLTags aTag){
  PRBool result=PR_FALSE;

  if((aTag>=eHTMLTag_unknown) & (aTag<=eHTMLTag_userdefined)){
    result=TestBits(gHTMLElements[aTag].mParentBits,kFlowEntity);
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
    result=TestBits(gHTMLElements[aTag].mInclusionBits,kBlockEntity);
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
    result=TestBits(gHTMLElements[aTag].mInclusionBits,kInlineEntity);
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
    result=TestBits(gHTMLElements[aTag].mInclusionBits,kFlowEntity);
  } 
  return result;
}

/**
 * Tells us whether the given tag opens a section
 * @update	gess 01/04/99
 * @param   id of tag
 * @return  TRUE if opens section
 */
PRBool nsHTMLElement::IsSectionTag(eHTMLTags aTag){
  PRBool result=PR_FALSE;
  switch(aTag){
    case eHTMLTag_html:
    case eHTMLTag_frameset:
    case eHTMLTag_body:
    case eHTMLTag_head:
      result=PR_TRUE;
      break;
    default:
      result=PR_FALSE;
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
PRBool nsHTMLElement::CanExclude(eHTMLTags aChild) const{
  PRBool result=PR_FALSE;

    //Note that special kids takes precedence over exclusions...
  if(mSpecialKids) {
    if(mSpecialKids->Contains(aChild)) {
      return PR_FALSE;
    }
  }

  if(eHTMLTag_unknown!=mExclusionBits){
    if(gHTMLElements[aChild].IsMemberOf(mExclusionBits)) {
      result=PR_TRUE;
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
PRBool nsHTMLElement::CanOmitEndTag(void) const{
  PRBool result=!IsContainer(mTagID);
  if(!result)
    result=TestBits(mSpecialProperties,kOmitEndTag);
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
PRBool nsHTMLElement::SectionContains(eHTMLTags aChild,PRBool allowDepthSearch) {
  PRBool result=PR_FALSE;
  CTagList* theRootTags=gHTMLElements[aChild].GetRootTags();
  if(theRootTags){
    if(!theRootTags->Contains(mTagID)){
      eHTMLTags theRootBase=(theRootTags->mTagList) ? theRootTags->mTagList[0] : theRootTags->mTags[0];
      if((eHTMLTag_unknown!=theRootBase) && (allowDepthSearch))
        result=SectionContains(theRootBase,allowDepthSearch);
    }
    else result=PR_TRUE;
  }
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
PRBool nsHTMLElement::IsHeadingTag(eHTMLTags aChild) {
  return gHeadingTags.Contains(aChild);
}


/**
 * 
 * @update	gess12/13/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::CanContainType(PRInt32 aType) const{
  PRInt32 answer=mInclusionBits & aType;
  PRBool  result=PRBool(0!=answer);
  return result;
}

/**
 * 
 * @update	gess12/13/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsMemberOf(PRInt32 aSet) const{
  PRBool result=(aSet && TestBits(aSet,mParentBits));
  return result;
}

/**
 * 
 * @update	gess12/13/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsWhitespaceTag(eHTMLTags aChild) {
  static eHTMLTags gWSTags[]={eHTMLTag_newline, eHTMLTag_whitespace};
  PRBool result=FindTagInSet(aChild,gWSTags,sizeof(gWSTags)/sizeof(eHTMLTag_body));
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

/**
 * 
 * @update	gess12/13/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::CanContainSelf(void) const {
  PRBool result=PRBool(TestBits(mInclusionBits,kSelf)!=0);
  return result;
}

/**
 * 
 * @update	gess12/13/98
 * @param 
 * @return
 */
eHTMLTags nsHTMLElement::GetCloseTargetForEndTag(nsEntryStack& aTagStack,PRInt32 anIndex) const{
  eHTMLTags result=eHTMLTag_unknown;

  int theCount=aTagStack.GetCount();
  int theIndex=theCount;
  if(IsMemberOf(kPhrase)){
    while((--theIndex>=anIndex) && (eHTMLTag_unknown==result)){
      eHTMLTags theTag=aTagStack.TagAt(theIndex);
      if(theTag!=mTagID) {
        //phrasal elements can close other phrasals, along with fontstyle and special tags...
        if(!gHTMLElements[theTag].IsMemberOf(kSpecial|kFontStyle|kPhrase)) {
          break; //it's not something I can close
        }
      }
      else {
        result=theTag; //stop because you just found yourself on the stack
        break;
      }
    }
  }
  else if(IsMemberOf(kSpecial)){
    while((--theIndex>=anIndex) && (eHTMLTag_unknown==result)){
      eHTMLTags theTag=aTagStack.TagAt(theIndex);
      if(theTag!=mTagID) {
        //phrasal elements can close other phrasals, along with fontstyle and special tags...
        if(gHTMLElements[theTag].IsMemberOf(kSpecial) ||
           gHTMLElements[theTag].IsMemberOf(kFontStyle)){
        }
        else {
          break; //it's not something I can close
        }
      }
      else {
        result=theTag; //stop because you just found yourself on the stack
        break;
      }
    }
  }
  else if(IsMemberOf(kFormControl|kExtensions)){
    while((--theIndex>=anIndex) && (eHTMLTag_unknown==result)){
      eHTMLTags theTag=aTagStack.TagAt(theIndex);
      if(theTag!=mTagID) {
        if(!CanContain(theTag)) {
          break; //it's not something I can close
        }
      }
      else {
        result=theTag; //stop because you just found yourself on the stack
        break;
      }
    }
  }
  else if(IsMemberOf(kFontStyle)){
    eHTMLTags theTag=aTagStack.Last();
    if(gHTMLElements[theTag].IsMemberOf(kFontStyle)) {
      result=theTag;
    }
  }
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


    if(nsHTMLElement::IsInlineEntity(aChild)){
      if(nsHTMLElement::IsInlineParent(mTagID)){
        return PR_TRUE;
      }
    }

    if(nsHTMLElement::IsFlowEntity(aChild)) {
      if(nsHTMLElement::IsFlowParent(mTagID)){
        return PR_TRUE;
      }
    }

    if(nsHTMLElement::IsTextTag(aChild)) {
      if(nsHTMLElement::IsInlineParent(mTagID)){
        return PR_TRUE;
      }
    }

    if(nsHTMLElement::IsBlockEntity(aChild)){
      if(nsHTMLElement::IsBlockParent(mTagID) || IsStyleTag(mTagID)){
        return PR_TRUE;
      }
    }

    if(CanContainType(gHTMLElements[aChild].mParentBits)) {
      return PR_TRUE;
    }
 
    if(mSpecialKids) {
      if(mSpecialKids->Contains(aChild)) {
        return PR_TRUE;
      }
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

void nsHTMLElement::DebugDumpContainment(const char* aFilename,const char* aTitle){
#ifdef  RICKG_DEBUG

  PRBool t=CanContain(eHTMLTag_address,eHTMLTag_object);

  const char* prefix="     ";
  fstream out(aFilename,ios::out);
  out << "==================================================" << endl;
  out << aTitle << endl;
  out << "==================================================";
  int i,j=0;
  int written;
  int linenum=5;
  for(i=1;i<eHTMLTag_text;i++){

    const char* tag=nsHTMLTags::GetStringValue((eHTMLTags)i);
    out << endl << endl << "Tag: <" << tag << ">" << endl;
    out << prefix;
    linenum+=3;
    written=0;
    char startChar=0;
    if(IsContainer((eHTMLTags)i)) {
      for(j=1;j<eHTMLTag_text;j++){
        tag=nsHTMLTags::GetStringValue((eHTMLTags)j);
        if(tag) {
          if(!startChar)
            startChar=tag[0];
          if(12==written){
            out << endl << prefix;
            linenum++;
            written=0;
          }
          if(CanContain((eHTMLTags)i,(eHTMLTags)j)){
            if(tag[0]!=startChar){
              out << endl << prefix;
              linenum++;
              written=0;
              startChar=tag[0];
            }
            out<< tag << ", ";
            written++;
          }

        }//if
      }//for
    }
    else {
      out<<"(not container)" << endl;
      linenum++;
    }
  } //for
#endif
}

void nsHTMLElement::DebugDumpMembership(const char* aFilename){
#ifdef  RICKG_DEBUG

  const char* prefix="             ";
  const char* suffix="       ";
  const char* shortSuffix="     ";
  fstream out(aFilename,ios::out);
  int i,j=0;
  int written=0;

  out << "============================================================================================================" << endl;
  out << "Membership..." << endl;
  out << "============================================================================================================" << endl;
  out << prefix << "Block   Block   Inline  Flow    Font    Phrase  Special Head    List    Form    Pre" << endl;
  out << prefix << "Element Entity  Entity  Entity  Element Element Element Element Element Element Element" << endl;
  out << "============================================================================================================" << endl;

  char* answer[]={".","Y"};
  char startChar=0;
  for(i=1;i<eHTMLTag_text;i++){
    const char* tag=nsHTMLTags::GetStringValue((eHTMLTags)i);
    out << tag; 
    int len=strlen(tag);

    while(len++<15){
      out<<" ";
    }

    int b=kBlock;
    b=kBlockEntity;
   
    out << answer[gHTMLElements[eHTMLTags(i)].mParentBits==kBlock] << suffix ;
    out << answer[gHTMLElements[eHTMLTags(i)].IsMemberOf(kBlockEntity)] << suffix;
    out << answer[gHTMLElements[eHTMLTags(i)].IsMemberOf(kInlineEntity)] << suffix ;
    out << answer[gHTMLElements[eHTMLTags(i)].IsMemberOf(kFlowEntity)] << suffix ;

    out << answer[gHTMLElements[eHTMLTags(i)].IsMemberOf(kFontStyle)] << suffix ;
    out << answer[gHTMLElements[eHTMLTags(i)].IsMemberOf(kPhrase)] << suffix;
    out << answer[gHTMLElements[eHTMLTags(i)].IsMemberOf(kSpecial)] << suffix;
    out << answer[gHTMLElements[eHTMLTags(i)].IsMemberOf(kHeading)] << suffix; 
    out << answer[gHTMLElements[eHTMLTags(i)].IsMemberOf(kList)] << suffix;
    out << answer[gHTMLElements[eHTMLTags(i)].IsMemberOf(kFormControl)] << suffix;
    out << answer[gHTMLElements[eHTMLTags(i)].IsMemberOf(kPreformatted)] << suffix << endl;

/*
    out << answer[gHTMLElements[eHTMLTags(i)].mParentBits==kFontStyle] << suffix ;
    out << answer[gHTMLElements[eHTMLTags(i)].mParentBits==kPhrase] << suffix;
    out << answer[gHTMLElements[eHTMLTags(i)].mParentBits==kSpecial] << suffix;
    out << answer[gHTMLElements[eHTMLTags(i)].mParentBits==kHeading] << suffix;
    out << answer[gHTMLElements[eHTMLTags(i)].mParentBits==kList] << suffix;
    out << answer[gHTMLElements[eHTMLTags(i)].mParentBits==kFormControl] << suffix;
    out << answer[gHTMLElements[eHTMLTags(i)].mParentBits==kPreformatted] << suffix << endl;
*/
  } //for
  out<<endl<<endl;
#endif
}

void nsHTMLElement::DebugDumpContainType(const char* aFilename){
#ifdef RICKG_DEBUG

  const char* prefix="             ";
  const char* suffix="       ";
  const char* shortSuffix="     ";
  fstream out(aFilename,ios::out);
  int i,j=0;
  int written=0;

  out << "============================================================================================================" << endl;
  out << "ContainType..."<<endl;
  out << "============================================================================================================" << endl;
  out << prefix << "Block   Block   Inline  Flow    Font    Phrase  Special Head    List    Form    Pre" << endl;
  out << prefix << "Element Entity  Entity  Entity  Element Element Element Element Element Element Element" << endl;
  out << "============================================================================================================" << endl;

  char* answer[]={".","Y"};
  char startChar=0;
  for(i=1;i<eHTMLTag_text;i++){
    const char* tag=nsHTMLTags::GetStringValue((eHTMLTags)i);
    out << tag; 
    int len=strlen(tag);

    while(len++<15){
      out<<" ";
    }

    out << answer[gHTMLElements[eHTMLTags(i)].CanContainType(kBlock)] << suffix ;
    out << answer[gHTMLElements[eHTMLTags(i)].CanContainType(kBlockEntity)] << suffix;
    out << answer[gHTMLElements[eHTMLTags(i)].CanContainType(kInlineEntity)] << suffix ;
    out << answer[gHTMLElements[eHTMLTags(i)].CanContainType(kFlowEntity)] << suffix ;
    out << answer[gHTMLElements[eHTMLTags(i)].CanContainType(kFontStyle)] << suffix ;
    out << answer[gHTMLElements[eHTMLTags(i)].CanContainType(kPhrase)] << suffix;
    out << answer[gHTMLElements[eHTMLTags(i)].CanContainType(kSpecial)] << suffix;
    out << answer[gHTMLElements[eHTMLTags(i)].CanContainType(kHeading)] << suffix; 
    out << answer[gHTMLElements[eHTMLTags(i)].CanContainType(kList)] << suffix;
    out << answer[gHTMLElements[eHTMLTags(i)].CanContainType(kFormControl)] << suffix;
    out << answer[gHTMLElements[eHTMLTags(i)].CanContainType(kPreformatted)] << suffix << endl;

  } //for
  out<<endl<<endl;
#endif
}
