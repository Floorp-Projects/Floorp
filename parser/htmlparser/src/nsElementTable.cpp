/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Blake Kaplan <mrbkap@gmail.com>
 *   Mats Palmgren <mats.palmgren@bredband.net>
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


#include "nsIAtom.h"
#include "nsElementTable.h"

/***************************************************************************** 
  Now it's time to list all the html elements all with their capabilities...
******************************************************************************/

#define DECL_TAG_LIST(name_, list_)                                           \
  static const eHTMLTags name_##list[] = list_;                               \
  static const TagList name_ = { NS_ARRAY_LENGTH(name_##list), name_##list };

#define COMMA ,

//First, define the set of taglists for tags with special parents...
DECL_TAG_LIST(gAParents,{eHTMLTag_map})
DECL_TAG_LIST(gInAddress,{eHTMLTag_address})
DECL_TAG_LIST(gInHead,{eHTMLTag_head})
DECL_TAG_LIST(gInTable,{eHTMLTag_table})
DECL_TAG_LIST(gInHTML,{eHTMLTag_html})
DECL_TAG_LIST(gInBody,{eHTMLTag_body})
DECL_TAG_LIST(gInForm,{eHTMLTag_form})
DECL_TAG_LIST(gInFieldset,{eHTMLTag_fieldset})
DECL_TAG_LIST(gInTR,{eHTMLTag_tr})
DECL_TAG_LIST(gInDL,{eHTMLTag_dl COMMA eHTMLTag_body})
DECL_TAG_LIST(gInFrameset,{eHTMLTag_frameset})
DECL_TAG_LIST(gInNoframes,{eHTMLTag_noframes})
//Removed ADDRESS to solve 24885
// gInP: nsHTMLElement::CanContain() also allows table in Quirks mode for bug 43678, removed FORM bug 94269
DECL_TAG_LIST(gInP,{eHTMLTag_span})
DECL_TAG_LIST(gOptgroupParents,{eHTMLTag_select COMMA eHTMLTag_optgroup})
DECL_TAG_LIST(gBodyParents,{eHTMLTag_html COMMA eHTMLTag_noframes})
DECL_TAG_LIST(gColParents,{eHTMLTag_table COMMA eHTMLTag_colgroup})
DECL_TAG_LIST(gFramesetParents,{eHTMLTag_html COMMA eHTMLTag_frameset})
DECL_TAG_LIST(gLegendParents,{eHTMLTag_fieldset})
DECL_TAG_LIST(gAreaParent,{eHTMLTag_map})
DECL_TAG_LIST(gParamParents,{eHTMLTag_applet COMMA eHTMLTag_object})
DECL_TAG_LIST(gTRParents,{eHTMLTag_tbody COMMA eHTMLTag_tfoot COMMA eHTMLTag_thead COMMA eHTMLTag_table})
DECL_TAG_LIST(gTREndParents,{eHTMLTag_tbody COMMA eHTMLTag_tfoot COMMA eHTMLTag_thead COMMA eHTMLTag_table COMMA eHTMLTag_applet})

//*********************************************************************************************
//  Next, define the set of taglists for tags with special kids...
//*********************************************************************************************

DECL_TAG_LIST(gContainsText,{eHTMLTag_text COMMA eHTMLTag_newline COMMA eHTMLTag_whitespace COMMA eHTMLTag_entity})
DECL_TAG_LIST(gUnknownKids,{eHTMLTag_html COMMA eHTMLTag_frameset})

// The presence of <input>, <select>, and <textarea> in gContainsOpts is due to
// the exclgroup that <select> sets...  If I don't include those here, they
// just get dropped automatically, since they are not allowed to open inside
// <select>.  Note that we are NOT allowing them to actually open without
// closing the select -- see gInputAutoClose.  Further note that I'm not
// including <button> in the list because in IE it doesn't autoclose <select>!
DECL_TAG_LIST(gContainsOpts,{eHTMLTag_option COMMA eHTMLTag_optgroup COMMA eHTMLTag_script COMMA eHTMLTag_input COMMA eHTMLTag_select COMMA eHTMLTag_textarea })
// Similar deal for <option> except it allows all of gContainsText _and_ the things that should autoclose selects.
DECL_TAG_LIST(gContainedInOpt,{eHTMLTag_text COMMA eHTMLTag_newline COMMA eHTMLTag_whitespace COMMA eHTMLTag_entity COMMA eHTMLTag_input COMMA eHTMLTag_select COMMA eHTMLTag_textarea})
DECL_TAG_LIST(gContainsParam,{eHTMLTag_param})
DECL_TAG_LIST(gColgroupKids,{eHTMLTag_col}) 
DECL_TAG_LIST(gAddressKids,{eHTMLTag_p})
DECL_TAG_LIST(gBodyKids,{eHTMLTag_dd COMMA eHTMLTag_del COMMA eHTMLTag_dt COMMA eHTMLTag_ins COMMA  eHTMLTag_noscript COMMA eHTMLTag_script COMMA eHTMLTag_li COMMA eHTMLTag_param}) // Added PARAM for bug 54448
DECL_TAG_LIST(gButtonKids,{eHTMLTag_caption COMMA eHTMLTag_legend})

DECL_TAG_LIST(gDLRootTags,{eHTMLTag_body COMMA eHTMLTag_td COMMA eHTMLTag_table COMMA eHTMLTag_applet COMMA eHTMLTag_dd})
DECL_TAG_LIST(gDLKids,{eHTMLTag_dd COMMA eHTMLTag_dt})
DECL_TAG_LIST(gDTKids,{eHTMLTag_dt})
DECL_TAG_LIST(gFieldsetKids,{eHTMLTag_legend COMMA eHTMLTag_text})
DECL_TAG_LIST(gFontKids,{eHTMLTag_legend COMMA eHTMLTag_table COMMA eHTMLTag_text COMMA eHTMLTag_li}) // Added table to fix bug 93365, li to fix bug 96031
DECL_TAG_LIST(gFormKids,{eHTMLTag_keygen})
DECL_TAG_LIST(gFramesetKids,{eHTMLTag_frame COMMA eHTMLTag_frameset COMMA eHTMLTag_noframes})

DECL_TAG_LIST(gHtmlKids,{eHTMLTag_body COMMA eHTMLTag_frameset COMMA eHTMLTag_head COMMA eHTMLTag_noscript COMMA eHTMLTag_noframes COMMA eHTMLTag_script COMMA eHTMLTag_newline COMMA eHTMLTag_whitespace})

DECL_TAG_LIST(gLabelKids,{eHTMLTag_span})
DECL_TAG_LIST(gLIKids,{eHTMLTag_ol COMMA eHTMLTag_ul})
DECL_TAG_LIST(gMapKids,{eHTMLTag_area})
DECL_TAG_LIST(gPreKids,{eHTMLTag_hr COMMA eHTMLTag_center})  //note that CENTER is here for backward compatibility; it's not 4.0 spec.

DECL_TAG_LIST(gTableKids,{eHTMLTag_caption COMMA eHTMLTag_col COMMA eHTMLTag_colgroup COMMA eHTMLTag_form COMMA  eHTMLTag_thead COMMA eHTMLTag_tbody COMMA eHTMLTag_tfoot COMMA eHTMLTag_script})// Removed INPUT - Ref. Bug 20087, 25382
  
DECL_TAG_LIST(gTableElemKids,{eHTMLTag_form COMMA eHTMLTag_noscript COMMA eHTMLTag_script COMMA eHTMLTag_td COMMA eHTMLTag_th COMMA eHTMLTag_tr})
DECL_TAG_LIST(gTRKids,{eHTMLTag_td COMMA eHTMLTag_th COMMA eHTMLTag_form COMMA eHTMLTag_script})// Removed INPUT - Ref. Bug 20087, 25382 |  Removed MAP to fix 58942
DECL_TAG_LIST(gTBodyKids,{eHTMLTag_tr COMMA eHTMLTag_form}) // Removed INPUT - Ref. Bug 20087, 25382
DECL_TAG_LIST(gULKids,{eHTMLTag_li COMMA eHTMLTag_p})


//*********************************************************************************************
// The following tag lists are used to define common set of root notes for the HTML elements...
//*********************************************************************************************

DECL_TAG_LIST(gRootTags,{eHTMLTag_body COMMA eHTMLTag_td COMMA eHTMLTag_table COMMA eHTMLTag_applet COMMA eHTMLTag_select}) // Added SELECT to fix bug 98645
DECL_TAG_LIST(gTableRootTags,{eHTMLTag_applet COMMA eHTMLTag_body COMMA eHTMLTag_dl COMMA eHTMLTag_ol COMMA eHTMLTag_td COMMA eHTMLTag_th})
DECL_TAG_LIST(gHTMLRootTags,{eHTMLTag_unknown})
 
DECL_TAG_LIST(gLIRootTags,{eHTMLTag_ul COMMA eHTMLTag_ol COMMA eHTMLTag_dir COMMA eHTMLTag_menu COMMA eHTMLTag_p COMMA eHTMLTag_body COMMA eHTMLTag_td COMMA eHTMLTag_th})

DECL_TAG_LIST(gOLRootTags,{eHTMLTag_body COMMA eHTMLTag_li COMMA eHTMLTag_td COMMA eHTMLTag_th COMMA eHTMLTag_select})
DECL_TAG_LIST(gTDRootTags,{eHTMLTag_tr COMMA eHTMLTag_tbody COMMA eHTMLTag_thead COMMA eHTMLTag_tfoot COMMA eHTMLTag_table COMMA eHTMLTag_applet})
DECL_TAG_LIST(gNoframeRoot,{eHTMLTag_body COMMA eHTMLTag_frameset})

//*********************************************************************************************
// The following tag lists are used to define the autoclose properties of the html elements...
//*********************************************************************************************

DECL_TAG_LIST(gBodyAutoClose,{eHTMLTag_head})
DECL_TAG_LIST(gTBodyAutoClose,{eHTMLTag_thead COMMA eHTMLTag_tfoot COMMA eHTMLTag_tbody COMMA eHTMLTag_td COMMA eHTMLTag_th})  // TD|TH inclusion - Bug# 24112
DECL_TAG_LIST(gCaptionAutoClose,{eHTMLTag_tbody})
DECL_TAG_LIST(gLIAutoClose,{eHTMLTag_p COMMA eHTMLTag_li})
DECL_TAG_LIST(gPAutoClose,{eHTMLTag_p COMMA eHTMLTag_li})
DECL_TAG_LIST(gHRAutoClose,{eHTMLTag_p})
DECL_TAG_LIST(gOLAutoClose,{eHTMLTag_p COMMA eHTMLTag_ol})
DECL_TAG_LIST(gDivAutoClose,{eHTMLTag_p})
// Form controls that autoclose <select> use this
DECL_TAG_LIST(gInputAutoClose,{eHTMLTag_select COMMA eHTMLTag_optgroup COMMA eHTMLTag_option})

DECL_TAG_LIST(gHeadingTags,{eHTMLTag_h1 COMMA eHTMLTag_h2 COMMA eHTMLTag_h3 COMMA eHTMLTag_h4 COMMA eHTMLTag_h5 COMMA eHTMLTag_h6})

DECL_TAG_LIST(gTableCloseTags,{eHTMLTag_td COMMA eHTMLTag_tr COMMA eHTMLTag_th COMMA eHTMLTag_tbody COMMA eHTMLTag_thead COMMA eHTMLTag_tfoot})
DECL_TAG_LIST(gTRCloseTags,{eHTMLTag_tr COMMA eHTMLTag_td COMMA eHTMLTag_th})
DECL_TAG_LIST(gTDCloseTags,{eHTMLTag_td COMMA eHTMLTag_th})
DECL_TAG_LIST(gDTCloseTags,{eHTMLTag_p COMMA eHTMLTag_dd COMMA eHTMLTag_dt})
DECL_TAG_LIST(gULCloseTags,{eHTMLTag_li})
DECL_TAG_LIST(gULAutoClose,{eHTMLTag_p COMMA eHTMLTag_ul}) //fix bug 50261..


DECL_TAG_LIST(gExcludableParents,{eHTMLTag_pre}) // Ref Bug 22913
DECL_TAG_LIST(gCaptionExcludableParents,{eHTMLTag_td}) //Ref Bug 26488

//*********************************************************************************************
//Lastly, bind tags with their rules, their special parents and special kids.
//*********************************************************************************************


const int kNoPropRange=0;
const int kDefaultPropRange=1;
const int kBodyPropRange=2;

PRBool CanBeContainedLI(eHTMLTags aChildTag,nsDTDContext &aContext);

//*********************************************************************************************
//
//        Now let's declare the element table...
//
//*********************************************************************************************


const nsHTMLElement gHTMLElements[] = {
  {
    /*tag*/                             eHTMLTag_unknown,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,  
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,
    /*special props, prop-range*/       kNonContainer, 10,
    /*special parents,kids,skip*/       0,&gUnknownKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_a,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, kInlineEntity, kNone,  
    /*special props, prop-range*/       kVerifyHierarchy, kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_abbr,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kSelf|kInlineEntity), kNone,
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_acronym,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kSelf|kInlineEntity), kNone,
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_address,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kBlock, kInlineEntity, kNone,
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gAddressKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_applet,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, (kSelf|kInlineEntity|kFlowEntity), kNone,
    /*special props, prop-range*/       kRequiresBody,kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gContainsParam,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_area,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gAreaParent,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kNone, kInlineEntity, kSelf,
    /*special props, prop-range*/       kNonContainer,kDefaultPropRange,
    /*special parents,kids,skip*/       &gAreaParent,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_b,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kInlineEntity|kSelf), kNone,
    /*special props, prop-range*/       0, kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_base,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gInHead,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kHeadContent, kNone, kNone,
    /*special props, prop-range*/       kNonContainer, kNoPropRange,
    /*special parents,kids,skip*/       &gInHead,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_basefont,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, kInlineEntity, kNone,
    /*special props, prop-range*/       kNonContainer, kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_bdo,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, (kSelf|kInlineEntity), kNone,
    /*special props, prop-range*/       0, kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_bgsound,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          (kFlowEntity|kHeadMisc), kNone, kNone,
    /*special props, prop-range*/       kNonContainer,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_big,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kInlineEntity|kSelf), kNone,
    /*special props, prop-range*/       0, kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_blink,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kFlowEntity|kSelf), kNone,
    /*special props, prop-range*/       0, kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_blockquote,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,  //remove excludeable parents to fix bug 53473
    /*parent,incl,exclgroups*/          kBlock, (kSelf|kFlowEntity), kNone,
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_body,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_frameset,
    /*rootnodes,endrootnodes*/          &gInHTML,&gInHTML,
    /*autoclose starttags and endtags*/ &gBodyAutoClose,0,0,0,
    /*parent,incl,exclgroups*/          kHTMLContent,(kFlowEntity|kSelf), kNone,
    /*special props, prop-range*/       kOmitEndTag, kBodyPropRange,
    /*special parents,kids,skip*/       0,&gBodyKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_br,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, kNone, kNone,
    /*special props, prop-range*/       kRequiresBody|kNonContainer, kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_button,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFormControl, kFlowEntity, kFormControl,
    /*special props, prop-range*/       kRequiresBody,kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gButtonKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_canvas,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, (kFlowEntity|kSelf), kNone,
    /*special props, prop-range*/       0, kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_caption,
    /*req-parent excl-parent*/          eHTMLTag_table,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gInTable,&gInTable,
    /*autoclose starttags and endtags*/ &gCaptionAutoClose,0,0,0,
    /*parent,incl,exclgroups*/          kNone, kFlowEntity, kSelf,
    /*special props, prop-range*/       (kNoPropagate|kNoStyleLeaksOut),kDefaultPropRange,
    /*special parents,kids,skip*/       &gInTable,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_center,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kBlock, (kSelf|kFlowEntity), kNone,
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_cite,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kSelf|kInlineEntity), kNone,
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_code,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kSelf|kInlineEntity), kNone,
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_col,
    /*req-parent excl-parent*/          eHTMLTag_table,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gColParents,&gColParents,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,
    /*special props, prop-range*/       kNoPropagate|kNonContainer,kDefaultPropRange,
    /*special parents,kids,skip*/       &gColParents,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_colgroup,
    /*req-parent excl-parent*/          eHTMLTag_table,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gInTable,&gInTable,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,
    /*special props, prop-range*/       kNoPropagate,kDefaultPropRange,
    /*special parents,kids,skip*/       &gInTable,&gColgroupKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_dd,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ &gDTCloseTags,0,&gDLKids,0,
    /*parent,incl,exclgroups*/          kInlineEntity, kFlowEntity, kNone,
    /*special props, prop-range*/       kNoPropagate|kMustCloseSelf|kVerifyHierarchy|kRequiresBody,kDefaultPropRange,
    /*special parents,kids,skip*/       &gInDL,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_del,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFlowEntity, (kSelf|kFlowEntity), kNone,
    /*special props, prop-range*/       0, kDefaultPropRange,
    /*special parents,kids,skip*/       &gInBody,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_dfn,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kSelf|kInlineEntity), kNone,
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_dir,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gOLRootTags,&gOLRootTags,
    /*autoclose starttags and endtags*/ &gOLAutoClose, &gULCloseTags, 0,0,
    /*parent,incl,exclgroups*/          kList, (kFlowEntity|kSelf), kNone,
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gULKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_div,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ &gDivAutoClose,0,0,0,
    /*parent,incl,exclgroups*/          kBlock, (kSelf|kFlowEntity), kNone,
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_dl,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gDLRootTags,&gRootTags,  //fix bug 57634
    /*autoclose starttags and endtags*/ 0,0,0,&gDTKids,           // DT should not contain DL - bug 100466
    /*parent,incl,exclgroups*/          kBlock, kSelf|kFlowEntity, kNone,
    /*special props, prop-range*/       0, kNoPropRange,
    /*special parents,kids,skip*/       0,&gDLKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_dt,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ &gDTCloseTags,0,&gDLKids,0,
    /*parent,incl,exclgroups*/          kInlineEntity, (kFlowEntity-kHeading), kNone,  // dt's parent group is inline - bug 65467
    /*special props, prop-range*/       (kNoPropagate|kMustCloseSelf|kVerifyHierarchy|kRequiresBody),kDefaultPropRange,
    /*special parents,kids,skip*/       &gInDL,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_em,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kSelf|kInlineEntity), kNone,
    /*special props, prop-range*/       0, kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_embed,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kBlockEntity, kNone, kNone,
    /*special props, prop-range*/       kNonContainer|kRequiresBody,kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gContainsParam,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_fieldset,
    /*requiredAncestor*/                eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kBlock, (kSelf|kFlowEntity), kNone,
    /*special props, prop-range*/       kNoPropagate,kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gFieldsetKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_font,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kSelf|kInlineEntity), kNone,
    /*special props, prop-range*/       0, kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gFontKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_form,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kBlock, kFlowEntity, kNone,
    /*special props, prop-range*/       kNoStyleLeaksIn, kNoPropRange,
    /*special parents,kids,skip*/       0,&gFormKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_frame, 
    /*req-parent excl-parent*/          eHTMLTag_frameset,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gInFrameset,&gInFrameset,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,
    /*special props, prop-range*/       kNoPropagate|kNoStyleLeaksIn|kNonContainer, kNoPropRange,
    /*special parents,kids,skip*/       &gInFrameset,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_frameset,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_body,
    /*rootnodes,endrootnodes*/          &gFramesetParents,&gInHTML,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kHTMLContent, kSelf, kAllTags,
    /*special props, prop-range*/       kNoPropagate|kNoStyleLeaksIn, kNoPropRange,
    /*special parents,kids,skip*/       &gInHTML,&gFramesetKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },

  {
    /*tag*/                             eHTMLTag_h1,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ &gHeadingTags,  &gHeadingTags, &gHeadingTags,0,
    /*parent,incl,exclgroups*/          kHeading, kFlowEntity, kNone,
    /*special props, prop-range*/       kVerifyHierarchy,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_h2,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ &gHeadingTags,  &gHeadingTags, &gHeadingTags,0,
    /*parent,incl,exclgroups*/          kHeading, kFlowEntity, kNone,
    /*special props, prop-range*/       kVerifyHierarchy,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_h3,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ &gHeadingTags,  &gHeadingTags, &gHeadingTags,0,
    /*parent,incl,exclgroups*/          kHeading, kFlowEntity, kNone,
    /*special props, prop-range*/       kVerifyHierarchy,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_h4,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ &gHeadingTags,  &gHeadingTags, &gHeadingTags,0,
    /*parent,incl,exclgroups*/          kHeading, kFlowEntity, kNone,
    /*special props, prop-range*/       kVerifyHierarchy,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_h5,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ &gHeadingTags,  &gHeadingTags, &gHeadingTags,0,
    /*parent,incl,exclgroups*/          kHeading, kFlowEntity, kNone,
    /*special props, prop-range*/       kVerifyHierarchy,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_h6,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ &gHeadingTags,  &gHeadingTags, &gHeadingTags,0,
    /*parent,incl,exclgroups*/          kHeading, kFlowEntity, kNone,
    /*special props, prop-range*/       kVerifyHierarchy,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_head,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gInHTML,&gInHTML,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kHTMLContent, (kHeadContent|kHeadMisc), kNone,
    /*special props, prop-range*/       kNoStyleLeaksIn, kDefaultPropRange,
    /*special parents,kids,skip*/       &gInHTML,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_hr,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ &gHRAutoClose,0,0,0,
    /*parent,incl,exclgroups*/          kBlock, kNone, kNone,
    /*special props, prop-range*/       kNonContainer|kRequiresBody,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_html,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_html,
    /*rootnodes,endrootnodes*/          &gHTMLRootTags,&gHTMLRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kNone, kHTMLContent, kNone,
    /*special props, prop-range*/       kSaveMisplaced|kOmitEndTag|kNoStyleLeaksIn, kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gHtmlKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_i,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kSelf|kInlineEntity), kNone,
    /*special props, prop-range*/       0, kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_iframe,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, (kSelf|kFlowEntity), kNone,
    /*special props, prop-range*/       kNoStyleLeaksIn, kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_image,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, kNone, kNone,
    /*special props, prop-range*/       kNonContainer,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_img,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, kNone, kNone,
    /*special props, prop-range*/       kNonContainer|kRequiresBody,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_input,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ &gInputAutoClose,0,0,0,
    /*parent,incl,exclgroups*/          kFormControl, kNone, kNone,
    /*special props, prop-range*/       kNonContainer|kRequiresBody,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_ins,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFlowEntity, (kSelf|kFlowEntity), kNone,
    /*special props, prop-range*/       0, kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_isindex,
    /*requiredAncestor*/                eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kBlock, kFlowEntity, kNone,
    /*special props, prop-range*/       kNonContainer|kRequiresBody,kDefaultPropRange,
    /*special parents,kids,skip*/       &gInBody,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_kbd,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kSelf|kInlineEntity), kNone,
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_keygen,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFlowEntity, kNone, kNone,
    /*special props, prop-range*/       kNonContainer,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_label,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFormControl, kInlineEntity, kSelf,
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gLabelKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_legend,
    /*requiredAncestor*/                eHTMLTag_fieldset,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gInFieldset,&gInFieldset,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kNone, kInlineEntity, kNone,
    /*special props, prop-range*/       kRequiresBody,kDefaultPropRange,
    /*special parents,kids,skip*/       &gInFieldset,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_li,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gLIRootTags,&gLIRootTags,
    /*autoclose starttags and endtags*/ &gLIAutoClose,0,0,0,
    /*parent,incl,exclgroups*/          kBlockEntity, kFlowEntity, kSelf, // changed this back to kBlockEntity so we enable RS handling for phrasals. ref bug 181697
    /*special props, prop-range*/       kNoPropagate|kVerifyHierarchy|kRequiresBody, kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gLIKids,eHTMLTag_unknown,
    /*contain-func*/                    &CanBeContainedLI
  },
  {
    /*tag*/                             eHTMLTag_link,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gInHead,&gInHead,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kHeadContent, kNone, kNone,
    /*special props, prop-range*/       kNonContainer,kDefaultPropRange,
    /*special parents,kids,skip*/       &gInHead,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_listing,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kPreformatted, (kSelf|kFlowEntity), kNone,  //add flowentity to fix 54993
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_map,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, kInlineEntity|kBlockEntity, kNone,
    /*special props, prop-range*/       0, kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gMapKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_marquee,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, (kSelf|kFlowEntity), kNone,
    /*special props, prop-range*/       kRequiresBody, kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_menu,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kList, (kSelf|kFlowEntity), kNone,
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gULKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_meta,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gInHead,&gInHead,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kHeadContent, kNone, kNone,
    /*special props, prop-range*/       kNoStyleLeaksIn|kNonContainer, kDefaultPropRange,
    /*special parents,kids,skip*/       &gInHead,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_multicol,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kBlock, kFlowEntity, kNone,
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_nobr,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kExtensions, kFlowEntity, kNone,
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_noembed, 
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFlowEntity, kFlowEntity, kNone,
    /*special props, prop-range*/       0, kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_noframes,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gNoframeRoot,&gNoframeRoot,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFlowEntity, kFlowEntity, kNone,
    /*special props, prop-range*/       0, kNoPropRange,
    /*special parents,kids,skip*/       &gNoframeRoot,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_noscript,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFlowEntity, kFlowEntity|kSelf, kNone,
    /*special props, prop-range*/       0, kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_object,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, (kFlowEntity|kSelf), kNone,
    /*special props, prop-range*/       kNoStyleLeaksOut|kPreferBody,kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gContainsParam,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_ol,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gOLRootTags,&gOLRootTags,
    /*autoclose starttags and endtags*/ &gOLAutoClose, &gULCloseTags, 0,0,
    /*parent,incl,exclgroups*/          kList, (kFlowEntity|kSelf), kNone,
    /*special props, prop-range*/       0,kDefaultPropRange,   
    /*special parents,kids,skip*/       0,&gULKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_optgroup,
    /*requiredAncestor*/                eHTMLTag_select,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gOptgroupParents,&gOptgroupParents,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       &gOptgroupParents,&gContainsOpts,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_option,
    /*requiredAncestor*/                eHTMLTag_select,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gOptgroupParents,&gOptgroupParents, 
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kNone, kPCDATA, kFlowEntity|kHeadMisc,
    /*special props, prop-range*/       kNoStyleLeaksIn|kNoPropagate, kDefaultPropRange,
    /*special parents,kids,skip*/       &gOptgroupParents,&gContainedInOpt,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_p,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kBlock, kInlineEntity, kNone,      //this used to contain FLOW. But it's really an inline container.
    /*special props, prop-range*/       kHandleStrayTag,kDefaultPropRange, //otherwise it tries to contain things like H1..H6
    /*special parents,kids,skip*/       0,&gInP,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_param,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gParamParents,&gParamParents,
    /*autoclose starttags and endtags*/ &gPAutoClose,0,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kNone,
    /*special props, prop-range*/       kNonContainer, kNoPropRange,
    /*special parents,kids,skip*/       &gParamParents,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_plaintext,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kExtensions, kCDATA, kNone,
    /*special props, prop-range*/       0, kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_pre,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kBlock|kPreformatted, (kSelf|kFlowEntity), kNone,  // Note: PRE is a block level element - bug 80009
    /*special props, prop-range*/       kRequiresBody, kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gPreKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_q,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, (kSelf|kInlineEntity), kNone,
    /*special props, prop-range*/       0, kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_s,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kSelf|kInlineEntity), kNone,
    /*special props, prop-range*/       0, kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_samp,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kSelf|kInlineEntity), kNone,
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_script,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          (kSpecial|kHeadContent), kCDATA, kNone,   // note: this is kHeadContent since shipping this breaks things.
    /*special props, prop-range*/       kNoStyleLeaksIn|kLegalOpen, kNoPropRange,
    /*special parents,kids,skip*/       0,&gContainsText,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_select,
    /*requiredAncestor*/                eHTMLTag_unknown, eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gInForm,&gInForm,
    /*autoclose starttags and endtags*/ &gInputAutoClose,0,0,0,
    /*parent,incl,exclgroups*/          kFormControl, kNone, kFlowEntity|kDLChild|kHeadMisc, // Added kHeadMisc to fix bug 287349
    /*special props, prop-range*/       kNoPropagate|kNoStyleLeaksIn|kRequiresBody, kDefaultPropRange,
    /*special parents,kids,skip*/       &gInForm,&gContainsOpts,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_small,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kSelf|kInlineEntity), kNone,
    /*special props, prop-range*/       0, kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    
    /*tag*/                             eHTMLTag_spacer,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kExtensions, kNone, kNone,
    /*special props, prop-range*/       kNonContainer,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    
          // I made span a special% tag again, (instead of inline).
          // This fixes the case:  <font color="blue"><p><span>text</span>

    /*tag*/                             eHTMLTag_span,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, (kInlineEntity|kSelf|kFlowEntity), kNone,
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    
    /*tag*/                             eHTMLTag_strike,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kSelf|kInlineEntity), kNone,
    /*special props, prop-range*/       0, kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    
    /*tag*/                             eHTMLTag_strong,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kSelf|kInlineEntity), kNone,  //changed this to inline per spec; fix bug 44584.
    /*special props, prop-range*/       0, kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gContainsText,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    
    /*tag*/                             eHTMLTag_style,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gInHead,&gInHead,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kHeadContent, kCDATA, kNone,
    /*special props, prop-range*/       kNoStyleLeaksIn, kNoPropRange,
    /*special parents,kids,skip*/       &gInHead,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_sub,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, (kSelf|kInlineEntity), kNone,
    /*special props, prop-range*/       0, kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    
    /*tag*/                             eHTMLTag_sup,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kSpecial, (kSelf|kInlineEntity), kNone,
    /*special props, prop-range*/       0, kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_table,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gTableRootTags,&gTableRootTags,
    /*autoclose starttags and endtags*/ 0,&gTableCloseTags,0,0,
    /*parent,incl,exclgroups*/          kBlock, kNone, (kSelf|kInlineEntity),
    /*special props, prop-range*/       (kBadContentWatch|kNoStyleLeaksIn|kRequiresBody), 2,
    /*special parents,kids,skip*/       0,&gTableKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_tbody,
    /*requiredAncestor*/                eHTMLTag_table, eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gInTable,&gInTable,
    /*autoclose starttags and endtags*/ &gTBodyAutoClose,0,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, (kSelf|kInlineEntity),
    /*special props, prop-range*/       (kNoPropagate|kBadContentWatch|kNoStyleLeaksIn|kNoStyleLeaksOut), kDefaultPropRange,
    /*special parents,kids,skip*/       &gInTable,&gTBodyKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_td,
    /*requiredAncestor*/                eHTMLTag_table, eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gTDRootTags,&gTDRootTags,
    /*autoclose starttags and endtags*/ &gTDCloseTags,&gTDCloseTags,0,&gExcludableParents,
    /*parent,incl,exclgroups*/          kNone, kFlowEntity, kSelf,
    /*special props, prop-range*/       kNoStyleLeaksIn|kNoStyleLeaksOut, kDefaultPropRange,
    /*special parents,kids,skip*/       &gTDRootTags,&gBodyKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_textarea,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gInForm,&gInForm,
    /*autoclose starttags and endtags*/ &gInputAutoClose,0,0,0,
    /*parent,incl,exclgroups*/          kFormControl, kPCDATA, kNone,
    /*special props, prop-range*/       kRequiresBody|kNoStyleLeaksIn,kDefaultPropRange,
    /*special parents,kids,skip*/       &gInForm,&gContainsText,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_tfoot,
    /*requiredAncestor*/                eHTMLTag_table, eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gInTable,&gInTable,
    /*autoclose starttags and endtags*/ &gTBodyAutoClose,0,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kSelf,
    /*special props, prop-range*/       (kNoPropagate|kBadContentWatch|kNoStyleLeaksIn|kNoStyleLeaksOut), kNoPropRange,
    /*special parents,kids,skip*/       &gInTable,&gTableElemKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_th, 
    /*requiredAncestor*/                eHTMLTag_table, eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gTDRootTags,&gTDRootTags,
    /*autoclose starttags and endtags*/ &gTDCloseTags,&gTDCloseTags,0,0,
    /*parent,incl,exclgroups*/          kNone, kFlowEntity, kSelf,
    /*special props, prop-range*/       (kNoStyleLeaksIn|kNoStyleLeaksOut), kDefaultPropRange,
    /*special parents,kids,skip*/       &gTDRootTags,&gBodyKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_thead,
    /*req-parent excl-parent*/          eHTMLTag_table,eHTMLTag_unknown,  //fix bug 54840...
    /*rootnodes,endrootnodes*/          &gInTable,&gInTable,  
    /*autoclose starttags and endtags*/ &gTBodyAutoClose,0,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kSelf,
    /*special props, prop-range*/       (kNoPropagate|kBadContentWatch|kNoStyleLeaksIn|kNoStyleLeaksOut), kNoPropRange,
    /*special parents,kids,skip*/       &gInTable,&gTableElemKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_title,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gInHead,&gInHead,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kHeadContent,kPCDATA, kNone,
    /*special props, prop-range*/       kNoStyleLeaksIn, kNoPropRange,
    /*special parents,kids,skip*/       &gInHead,&gContainsText,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_tr,
    /*requiredAncestor*/                eHTMLTag_table, eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gTRParents,&gTREndParents,
    /*autoclose starttags and endtags*/ &gTRCloseTags,0,0,0,
    /*parent,incl,exclgroups*/          kNone, kNone, kInlineEntity,
    /*special props, prop-range*/       (kBadContentWatch|kNoStyleLeaksIn|kNoStyleLeaksOut), kNoPropRange,
    /*special parents,kids,skip*/       &gTRParents,&gTRKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_tt,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kSelf|kInlineEntity), kNone,
    /*special props, prop-range*/       0, kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_u,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFontStyle, (kSelf|kInlineEntity), kNone,
    /*special props, prop-range*/       0, kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_ul,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gOLRootTags,&gOLRootTags,
    /*autoclose starttags and endtags*/ &gULAutoClose,&gULCloseTags,0,0,
    /*parent,incl,exclgroups*/          kList, (kFlowEntity|kSelf), kNone,
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,&gULKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_var,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kPhrase, (kSelf|kInlineEntity), kNone,
    /*special props, prop-range*/       0,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_wbr,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kExtensions, kNone, kNone,
    /*special props, prop-range*/       kNonContainer|kRequiresBody,kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_xmp,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kInlineEntity|kPreformatted, kCDATA, kNone,
    /*special props, prop-range*/       kNone,kDefaultPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_text,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gInBody,&gInBody,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFlowEntity, kNone, kNone,
    /*special props, prop-range*/       kNonContainer|kRequiresBody,kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
          // Whitespace must have a parent model of kHeadMisc to ensure that we
          // do the right thing for whitespace in the head section of a document.
          // (i.e., it must be non-exclusively a child of the head).

    /*tag*/                             eHTMLTag_whitespace, 
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gInBody,&gInBody,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFlowEntity|kHeadMisc, kNone, kNone,
    /*special props, prop-range*/       kNonContainer|kLegalOpen,kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
          // Newlines must have a parent model of kHeadMisc to ensure that we
          // do the right thing for whitespace in the head section of a document.
          // (i.e., it must be non-exclusively a child of the head).

    /*tag*/                             eHTMLTag_newline,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gInBody,&gInBody,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFlowEntity|kHeadMisc, kNone, kNone,
    /*special props, prop-range*/       kNonContainer|kLegalOpen, kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
          // Comments must have a parent model of kHeadMisc to ensure that we
          // do the right thing for whitespace in the head section of a document
          // (i.e., it must be non-exclusively a child of the head).

    /*tag*/                             eHTMLTag_comment,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFlowEntity|kHeadMisc, kNone, kNone,
    /*special props, prop-range*/       kOmitEndTag|kLegalOpen,kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_entity,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gInBody,&gInBody,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFlowEntity, kNone, kNone,
    /*special props, prop-range*/       0, kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_doctypeDecl,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFlowEntity, kNone, kNone,
    /*special props, prop-range*/       kOmitEndTag,kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_markupDecl,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFlowEntity, kNone, kNone,
    /*special props, prop-range*/       kOmitEndTag,kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
    /*tag*/                             eHTMLTag_instruction,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_unknown,
    /*rootnodes,endrootnodes*/          0,0,
    /*autoclose starttags and endtags*/ 0,0,0,0,
    /*parent,incl,exclgroups*/          kFlowEntity, kNone, kNone,
    /*special props, prop-range*/       kOmitEndTag,kNoPropRange,
    /*special parents,kids,skip*/       0,0,eHTMLTag_unknown,
    /*contain-func*/                    0
  },
  {
          // Userdefined tags must have a parent model of kHeadMisc to ensure that
          // we do the right thing for whitespace in the head section of a document.
          // (i.e., it must be non-exclusively a child of the head).

    /*tag*/                             eHTMLTag_userdefined,
    /*req-parent excl-parent*/          eHTMLTag_unknown,eHTMLTag_frameset,
    /*rootnodes,endrootnodes*/          &gRootTags,&gRootTags,
    /*autoclose starttags and endtags*/ &gBodyAutoClose,0,0,0,
    /*parent,incl,exclgroups*/          (kFlowEntity|kHeadMisc), (kInlineEntity|kSelf), kNone,  // Treat userdefined as inline element - Ref bug 56245,66772
    /*special props, prop-range*/       kPreferBody, kBodyPropRange,
    /*special parents,kids,skip*/       &gInNoframes,&gBodyKids,eHTMLTag_unknown,
    /*contain-func*/                    0
  }
};

#ifdef NS_DEBUG  
void CheckElementTable() {
  for (eHTMLTags t = eHTMLTag_unknown; t <= eHTMLTag_userdefined; t = eHTMLTags(t + 1)) {
    NS_ASSERTION(gHTMLElements[t].mTagID == t, "gHTMLElements entries does match tag list.");
  }
}
#endif

/**
 * This is called to answer the CanBeContained question when LI is the parent
 * @update	gess 10/13/00
 * @param 
 * @return
 */ 
PRBool CanBeContainedLI(eHTMLTags aChildTag,nsDTDContext &aContext) {
  PRBool result=PR_TRUE;

  //walk the parent hierarchy, to see if the LI is in a block or an inline.
  PRInt32 anIndex=aContext.GetCount();
  PRBool  theLIHasABlockParent=PR_FALSE;

  PRBool theChildIsBlock=PR_FALSE;
  
  if((aChildTag>eHTMLTag_unknown) && (aChildTag<eHTMLTag_userdefined)) {
    theChildIsBlock=  (eHTMLTag_dt==aChildTag) || 
                      (eHTMLTag_dd==aChildTag) ||
                      (gHTMLElements[aChildTag].IsMemberOf(kBlock))       || 
                      (gHTMLElements[aChildTag].IsMemberOf(kBlockEntity)) || 
                      (gHTMLElements[aChildTag].IsMemberOf(kHeading))     || 
                      (gHTMLElements[aChildTag].IsMemberOf(kPreformatted))|| 
                      (gHTMLElements[aChildTag].IsMemberOf(kList)); 
  }

  if(theChildIsBlock) {

    while(--anIndex>0) {
      eHTMLTags aParent=aContext.TagAt(anIndex);
      if((eHTMLTag_ul==aParent) ||
         (eHTMLTag_ol==aParent) || 
         (eHTMLTag_table==aParent) ||
         (eHTMLTag_dir==aParent)) {
        theLIHasABlockParent=PR_TRUE;
        break;
      }
    }
    result=theLIHasABlockParent;
  }    
  else {
    result=PR_TRUE;
  }
  return result;
}

/**
 * This is the default implementation, that was moved out of CNavDTD, so that it can be made
 * to behave in a more flexible manner. At this point, the code itself has not changed.
 * NOTE: This is not called YET. It's just a placeholder for future changes.
 *
 * @update	gess 10/13/00
 * @param 
 * @return
 */ 
PRBool nsHTMLElement::CanBeContained(eHTMLTags aChildTag,nsDTDContext &aContext) const {
  PRBool result=PR_TRUE;
  if(!mCanBeContained) {

    /* #    Interesting test cases:       Result:
     * 1.   <UL><LI>..<B>..<LI>           inner <LI> closes outer <LI>
     * 2.   <CENTER><DL><DT><A><CENTER>   allow nested <CENTER>
     * 3.   <TABLE><TR><TD><TABLE>...     allow nested <TABLE>
     * 4.   <FRAMESET> ... <FRAMESET>
     */

    //Note: This method is going away. First we need to get the elementtable to do closures right, and
    //      therefore we must get residual style handling to work.

    //the changes to this method were added to fix bug 54651...

    PRInt32 theCount=aContext.GetCount();
    result=PR_TRUE;

    if(0<theCount){
      const TagList* theRootTags=gHTMLElements[aChildTag].GetRootTags();
      const TagList* theSpecialParents=gHTMLElements[aChildTag].GetSpecialParents();
      if(theRootTags) {
        PRInt32 theRootIndex=LastOf(aContext,*theRootTags);
        PRInt32 theSPIndex=(theSpecialParents) ? LastOf(aContext,*theSpecialParents) : kNotFound;  
        PRInt32 theChildIndex=GetIndexOfChildOrSynonym(aContext,aChildTag);
        PRInt32 theTargetIndex=(theRootIndex>theSPIndex) ? theRootIndex : theSPIndex;

        if((theTargetIndex==theCount-1) ||
          ((theTargetIndex==theChildIndex) && gHTMLElements[aChildTag].CanContainSelf())) {
          result=PR_TRUE;
        }
        else {
        
          result=PR_FALSE;

          static eHTMLTags gTableElements[]={eHTMLTag_td,eHTMLTag_th};

          PRInt32 theIndex=theCount-1;
          while(theChildIndex<theIndex) {
            eHTMLTags theParentTag=aContext.TagAt(theIndex--);
            if (gHTMLElements[theParentTag].IsMemberOf(kBlockEntity)  || 
                gHTMLElements[theParentTag].IsMemberOf(kHeading)      || 
                gHTMLElements[theParentTag].IsMemberOf(kPreformatted) || 
                gHTMLElements[theParentTag].IsMemberOf(kFormControl) || //added this to fix bug 44479
                gHTMLElements[theParentTag].IsMemberOf(kList)) {
              if(!HasOptionalEndTag(theParentTag)) {
                result=PR_TRUE;
                break;
              }
            }
            else if(FindTagInSet(theParentTag,gTableElements,sizeof(gTableElements)/sizeof(eHTMLTag_unknown))){
              result=PR_TRUE;  //added this to catch a case we missed; bug 57173.
              break;
            }
          }
        }
      }
    }

  }
  else result=(*mCanBeContained)(aChildTag,aContext);
  return result;
}

/**
 *  Call this to find the index of a given child, or (if not found)
 *  the index of its nearest synonym.
 *   
 *  @update  gess 3/25/98
 *  @param   aTagStack -- list of open tags
 *  @param   aTag -- tag to test for containership
 *  @return  index of kNotFound
 */
PRInt32 nsHTMLElement::GetIndexOfChildOrSynonym(nsDTDContext& aContext,eHTMLTags aChildTag) {
  PRInt32 theChildIndex=aContext.LastOf(aChildTag);
  if(kNotFound==theChildIndex) {
    const TagList* theSynTags=gHTMLElements[aChildTag].GetSynonymousTags(); //get the list of tags that THIS tag can close
    if(theSynTags) {
      theChildIndex=LastOf(aContext,*theSynTags);
    } 
  }
  return theChildIndex;
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

/**
 * 
 * @update	gess12/13/98
 * @param 
 * @return
 */ 
PRBool nsHTMLElement::IsContainer(eHTMLTags aChild) {
  PRBool result=(eHTMLTag_unknown==aChild);

  if(!result){
    result=!TestBits(gHTMLElements[aChild].mSpecialProperties,kNonContainer);
  }
  return result;
}

/**
 * This tests whether all the bits in the parentbits
 * are included in the given set. It may be too 
 * broad a question for most cases.
 *
 * @update	gess12/13/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsMemberOf(PRInt32 aSet) const{
  return TestBits(aSet,mParentBits);
}

/**
 * This tests whether all the bits in the parentbits
 * are included in the given set. It may be too 
 * broad a question for most cases.
 *
 * @update	gess12/13/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::ContainsSet(PRInt32 aSet) const{
  return TestBits(mParentBits,aSet);
}

/** 
 * This method determines whether the given tag closes other blocks.
 * 
 * @update	gess 12/20/99 -- added H1..H6 to this list.
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsBlockCloser(eHTMLTags aTag){
  PRBool result=PR_FALSE;
    
  if((aTag>=eHTMLTag_unknown) & (aTag<=eHTMLTag_xmp)){

    result=(gHTMLElements[aTag].IsBlock() || 
            gHTMLElements[aTag].IsBlockEntity() ||
            (kHeading==gHTMLElements[aTag].mParentBits));
    if(!result) {
      // NOBR is a block closure   - Ref. Bug# 24462
      // DIR is a block closure    - Ref. Bug# 25845
      // TD is a block closure     - Ref. Bug# 27490
      // TR is a block closure     - Ref. Bug# 26488
      // OBJECT is a block closure - Ref. Bug# 88992

      static eHTMLTags gClosers[]={ eHTMLTag_table,eHTMLTag_tbody,
                                    eHTMLTag_td,eHTMLTag_th,
                                    eHTMLTag_tr,eHTMLTag_caption,
                                    eHTMLTag_object,eHTMLTag_applet,
                                    eHTMLTag_ol, eHTMLTag_ul,
                                    eHTMLTag_optgroup,
                                    eHTMLTag_nobr,eHTMLTag_dir};

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
  if((aTag>=eHTMLTag_unknown) & (aTag<=eHTMLTag_xmp)){
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

  if((aTag>=eHTMLTag_unknown) & (aTag<=eHTMLTag_xmp)){
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
  if((aTag>=eHTMLTag_unknown) & (aTag<=eHTMLTag_xmp)){
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
  if((aTag>=eHTMLTag_unknown) & (aTag<=eHTMLTag_xmp)){
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
  if((aTag>=eHTMLTag_unknown) & (aTag<=eHTMLTag_xmp)){
    result=TestBits(gHTMLElements[aTag].mInclusionBits,kFlowEntity);
  } 
  return result;
}

/**
 * 
 * @update	harishd 11/19/99
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsSpecialParent(eHTMLTags aTag) const{
  PRBool result=PR_FALSE;
  if(mSpecialParents) {
    if(FindTagInSet(aTag,mSpecialParents->mTags,mSpecialParents->mCount))
        result=PR_TRUE;
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
PRBool nsHTMLElement::CanContain(eHTMLTags aParent,eHTMLTags aChild,nsDTDMode aMode){
  PRBool result=PR_FALSE;
  if((aParent>=eHTMLTag_unknown) && (aParent<=eHTMLTag_userdefined)){
    result=gHTMLElements[aParent].CanContain(aChild,aMode);
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

  if(gHTMLElements[aChild].HasSpecialProperty(kLegalOpen)) {
    // Some tags could be opened anywhere, in the document, as they please.
    return PR_FALSE;
  }

  //Note that special kids takes precedence over exclusions...
  if(mSpecialKids) {
    if(FindTagInSet(aChild,mSpecialKids->mTags,mSpecialKids->mCount)) {
      return PR_FALSE;
    }
  }

  if(mExclusionBits){
    if(gHTMLElements[aChild].IsMemberOf(mExclusionBits)) {
      result=PR_TRUE;
    }
  }
  return result;
}

/**
 * 
 * @update	harishd 03/01/00
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsExcludableParent(eHTMLTags aParent) const{
  PRBool result=PR_FALSE;

  if(!IsTextTag(mTagID)) {
    if(mExcludableParents) {
      const TagList* theParents=mExcludableParents;
      if(FindTagInSet(aParent,theParents->mTags,theParents->mCount))
        result=PR_TRUE;
    }
    if(!result) {
      // If you're a block parent make sure that you're not the
      // parent of a TABLE element. ex. <table><tr><td><div><td></tr></table>
      // IE & Nav. render this as table with two cells ( which I think is correct ).
      // NOTE: If need arise we could use the root node to solve this problem
      if(nsHTMLElement::IsBlockParent(aParent)){
        switch(mTagID) {
          case eHTMLTag_caption:
          case eHTMLTag_thead:
          case eHTMLTag_tbody:
          case eHTMLTag_tfoot:
          case eHTMLTag_td:
          case eHTMLTag_th:
          case eHTMLTag_tr:
            result=PR_TRUE;
          default:
            break;
        }
      }
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
 * Returns whether a given tag can be a direct child of the <head> node of
 * an HTML document.
 *
 * @param aChild The tag in question.
 * @param aExclusively [out]Whether or not this tag can *only* appear in the
 *                     head (as opposed to things like <object> which can be
                       either in the body or the head).
 * @return Whether this tag can appear in the head.
 */
PRBool nsHTMLElement::IsChildOfHead(eHTMLTags aChild,PRBool& aExclusively) {
  aExclusively = PR_TRUE;

  // Is this a head-only tag?
  if (gHTMLElements[aChild].mParentBits & kHeadContent) {
    return PR_TRUE;
  }


  // If not, check if it can appear in the head.
  if (gHTMLElements[aChild].mParentBits & kHeadMisc) {
    aExclusively = PR_FALSE;
    return PR_TRUE;
  }

  return PR_FALSE;
}



/**
 * 
 * @update	gess12/13/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::SectionContains(eHTMLTags aChild,PRBool allowDepthSearch) const {
  PRBool result=PR_FALSE;
  const TagList* theRootTags=gHTMLElements[aChild].GetRootTags();

  if(theRootTags){
    if(!FindTagInSet(mTagID,theRootTags->mTags,theRootTags->mCount)){
      eHTMLTags theRootBase=theRootTags->mTags[0];
      if((eHTMLTag_unknown!=theRootBase) && (allowDepthSearch))
        result=SectionContains(theRootBase,allowDepthSearch);
    }
    else result=PR_TRUE;
  }
  return result;
}

/**
 * This method should be called to determine if the a tags
 * hierarchy needs to be validated.
 * 
 * @update	harishd 04/19/00
 * @param 
 * @return
 */

PRBool nsHTMLElement::ShouldVerifyHierarchy() const {
  PRBool result=PR_FALSE;
  
  // If the tag cannot contain itself then we need to make sure that
  // anywhere in the hierarchy we don't nest accidently.
  // Ex: <H1><LI><H1><LI>. Inner LI has the potential of getting nested
  // inside outer LI.If the tag can contain self, Ex: <A><B><A>,
  // ( B can contain self )then ask the child (<A>) if it requires a containment check.
  if(mTagID!=eHTMLTag_userdefined) {
    result=HasSpecialProperty(kVerifyHierarchy);
  }
  return result;
}
  
/**
 * 
 * @update	gess12/13/98
 * @param 
 * @return
 */
PRBool nsHTMLElement::IsResidualStyleTag(eHTMLTags aChild) {
  PRBool result=PR_FALSE;
  switch(aChild) {
    case eHTMLTag_a:       
    case eHTMLTag_b:
    case eHTMLTag_bdo:     
    case eHTMLTag_big:       
    case eHTMLTag_blink:
    case eHTMLTag_del:
    case eHTMLTag_em:
    case eHTMLTag_font:    
    case eHTMLTag_i:         
    case eHTMLTag_ins:
    case eHTMLTag_q:
    case eHTMLTag_s:       
    case eHTMLTag_small:
    case eHTMLTag_strong:
    case eHTMLTag_strike:    
    case eHTMLTag_sub:     
    case eHTMLTag_sup:       
    case eHTMLTag_tt:
    case eHTMLTag_u:       
      result=PR_TRUE;
      break;

    case eHTMLTag_abbr:
    case eHTMLTag_acronym:   
    case eHTMLTag_center:  
    case eHTMLTag_cite:      
    case eHTMLTag_code:
    case eHTMLTag_dfn:       
    case eHTMLTag_kbd:     
    case eHTMLTag_samp:      
    case eHTMLTag_span:    
    case eHTMLTag_var:
      result=PR_FALSE;
    default:
      break;
  };
  return result;
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
PRBool nsHTMLElement::IsWhitespaceTag(eHTMLTags aChild) {
  PRBool result=PR_FALSE;

  switch(aChild) {
    case eHTMLTag_newline:
    case eHTMLTag_whitespace:
      result=PR_TRUE;
      break;
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
PRBool nsHTMLElement::IsTextTag(eHTMLTags aChild) {
  PRBool result=PR_FALSE;

  switch(aChild) {
    case eHTMLTag_text:
    case eHTMLTag_entity:
    case eHTMLTag_newline:
    case eHTMLTag_whitespace:
      result=PR_TRUE;
      break;
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
PRBool nsHTMLElement::CanContainSelf(void) const {
  PRBool result=PRBool(TestBits(mInclusionBits,kSelf)!=0);
  return result;
}

/**
 * This method is called to determine (once and for all) whether a start tag
 * can close another tag on the stack. This method will return
 * false if something prevents aParentTag from closing.
 *
 * @update	gess 12/20/99
 * @param   aContext is the tag stack we're testing against
 * @param   aIndex is the index of the tag we want to close
 * @param   aChildTag is the child we're trying to close
 * @return  TRUE if we can autoclose the start tag; FALSE otherwise
 */
PRBool nsHTMLElement::CanAutoCloseTag(nsDTDContext& aContext,PRInt32 aIndex,
                                      eHTMLTags aChildTag) const{

  PRInt32 thePos;
  PRBool  result = PR_TRUE;
  eHTMLTags thePrevTag;

  for(thePos = aContext.GetCount() - 1; thePos >= aIndex; thePos--) {
    thePrevTag = aContext.TagAt(thePos);

    if (thePrevTag == eHTMLTag_applet ||
        thePrevTag == eHTMLTag_td) {
          result = PR_FALSE;
          break;
    }
  }
  
  return result;
}

/**
 * 
 * @update	gess 10.17.2000
 * @param 
 * @return  
 */
eHTMLTags nsHTMLElement::GetCloseTargetForEndTag(nsDTDContext& aContext,PRInt32 anIndex,nsDTDMode aMode) const{
  eHTMLTags result=eHTMLTag_unknown;

  int theCount=aContext.GetCount();
  int theIndex=theCount;

  if(IsMemberOf(kPhrase)){

    while((--theIndex>=anIndex) && (eHTMLTag_unknown==result)){
      eHTMLTags theTag = aContext.TagAt(theIndex);
      if(theTag != mTagID) {
        // Allow phrasals to close userdefined tags. bug 256731
        if(eHTMLTag_userdefined == theTag) {
          continue; // We can close this.
        }

        // Fixes a derivative of bug 22842...
        if(CanContainType(kBlock)) { // INS/DEL can contain blocks.
          if(gHTMLElements[eHTMLTags(theTag)].IsMemberOf(kBlockEntity) || 
             gHTMLElements[eHTMLTags(theTag)].IsMemberOf(kFlowEntity)) {
            if(HasOptionalEndTag(theTag)) {
              continue; // Then I can close it.
            }
          }
        }

        // Phrasal elements can close other phrasals, along with fontstyle,
        // extensions, and special tags...
        if(!gHTMLElements[theTag].IsMemberOf(kSpecial | 
                                             kFontStyle |
                                             kPhrase |
                                             kExtensions)) {  //fix bug 56665
          break; // It's not something I can close
        }
      }
      else {
        result=theTag; // Stop because you just found yourself on the stack
        break;
      }
    }
  }
  
  else if(IsMemberOf(kSpecial)){

    while((--theIndex>=anIndex) && (eHTMLTag_unknown==result)){
      eHTMLTags theTag=aContext.TagAt(theIndex);
      if(theTag!=mTagID) {
        // Special elements can close other specials, along with fontstyle 
        // extensions, and phrasal tags...

        // Added Phrasal to fix bug 26347
        if((eHTMLTag_userdefined==theTag) ||
            gHTMLElements[theTag].IsSpecialEntity()  || 
            gHTMLElements[theTag].IsFontStyleEntity()||
            gHTMLElements[theTag].IsPhraseEntity()   ||
            gHTMLElements[theTag].IsMemberOf(kExtensions)) {
          continue;
        }
        else {

          // Fixes bug 22842...
          if(CanContainType(kBlock)) {
            if(gHTMLElements[eHTMLTags(theTag)].IsMemberOf(kBlockEntity) || 
               gHTMLElements[eHTMLTags(theTag)].IsMemberOf(kFlowEntity)) {
              if(HasOptionalEndTag(theTag)) {
                continue; // Then I can close it.
              }
            }
          }
          break; // It's not something I can close
        }
      }
      else {
        result=theTag; // Stop because you just found yourself on the stack
        break;
      }
    }
  }

  else if(ContainsSet(kPreformatted) ||  
          IsMemberOf(kFormControl|kExtensions|kPreformatted)){  //bug54834...

    while((--theIndex>=anIndex) && (eHTMLTag_unknown==result)){
      eHTMLTags theTag=aContext.TagAt(theIndex);
      if(theTag!=mTagID) {
        if(!CanContain(theTag,aMode)) {
          break; //it's not something I can close
        }
      }
      else {
        result=theTag; //stop because you just found yourself on the stack
        break; 
      }
    }
  }

  else if(IsMemberOf(kList)){

    while((--theIndex>=anIndex) && (eHTMLTag_unknown==result)){
      eHTMLTags theTag=aContext.TagAt(theIndex);
      if(theTag!=mTagID) {
        if(!CanContain(theTag,aMode)) {
          break; //it's not something I can close
        }
      }
      else {
        result=theTag; //stop because you just found yourself on the stack
        break; 
      }
    }
  }

  else if(IsResidualStyleTag(mTagID)){
    
    // Before finding a close target, for the current tag, make sure
    // that the tag above does not gate.
    // Note: we intentionally make 2 passes: 
    // The first pass tries to exactly match, the 2nd pass matches the group.

    const TagList* theRootTags=gHTMLElements[mTagID].GetEndRootTags();
    PRInt32 theIndexCopy=theIndex;
    while(--theIndex>=anIndex){
      eHTMLTags theTag=aContext.TagAt(theIndex);
      if(theTag == mTagID) {
        return theTag; // we found our target.
      }
      else if (!CanContain(theTag,aMode) || 
               (theRootTags && FindTagInSet(theTag,theRootTags->mTags,theRootTags->mCount))) {
        // If you cannot contain this tag then
        // you cannot close it either. It looks like
        // the tag trying to close is misplaced.
        // In the following Exs. notice the misplaced /font:
        // Ex. <font><table><tr><td></font></td></tr></table. -- Ref. bug 56245
        // Ex. <font><select><option></font></select> -- Ref. bug 37618
        // Ex. <font><select></font><option></select> -- Ref. bug 98187
        return eHTMLTag_unknown;
      }
    }

    theIndex=theIndexCopy;
    while(--theIndex>=anIndex){
      eHTMLTags theTag=aContext.TagAt(theIndex);
      if(gHTMLElements[theTag].IsMemberOf(mParentBits)) {
        return theTag;
      }
    }    
  }

  else if(gHTMLElements[mTagID].IsTableElement()) {
    
      //This fixes 57378...
      //example: <TABLE><THEAD><TR><TH></THEAD> which didn't close the <THEAD>

    PRInt32 theLastTable=aContext.LastOf(eHTMLTag_table);
    PRInt32 theLastOfMe=aContext.LastOf(mTagID);
    if(theLastTable<theLastOfMe) {
      return mTagID;
    }
        
  }

  else if(mTagID == eHTMLTag_legend)  {
    while((--theIndex>=anIndex) && (eHTMLTag_unknown==result)){
      eHTMLTags theTag = aContext.TagAt(theIndex);
      if (theTag == mTagID) {
        result = theTag;
        break;
      }

      if (!CanContain(theTag, aMode)) {
        break;
      }
    }
  }

  else if (mTagID == eHTMLTag_head) {
    while (--theIndex >= anIndex) {
      eHTMLTags tag = aContext.TagAt(theIndex);
      if (tag == eHTMLTag_html) {
        // HTML gates head closing, but the head should never be the parent of
        // an html tag.
        break;
      }

      if (tag == eHTMLTag_head) {
        result = eHTMLTag_head;
        break;
      }
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
PRBool nsHTMLElement::CanContain(eHTMLTags aChild,nsDTDMode aMode) const{


  if(IsContainer(mTagID)){

    if(gHTMLElements[aChild].HasSpecialProperty(kLegalOpen)) {
      // Some tags could be opened anywhere, in the document, as they please.
      return PR_TRUE;
    }

    if(mTagID==aChild) {
      return CanContainSelf();  //not many tags can contain themselves...
    }

    const TagList* theCloseTags=gHTMLElements[aChild].GetAutoCloseStartTags();
    if(theCloseTags){
      if(FindTagInSet(mTagID,theCloseTags->mTags,theCloseTags->mCount))
        return PR_FALSE;
    }

    if(gHTMLElements[aChild].mExcludableParents) {
      const TagList* theParents=gHTMLElements[aChild].mExcludableParents;
      if(FindTagInSet(mTagID,theParents->mTags,theParents->mCount))
        return PR_FALSE;
    }
    
    if(gHTMLElements[aChild].IsExcludableParent(mTagID))
      return PR_FALSE;

    if(gHTMLElements[aChild].IsBlockCloser(aChild)){
      if(nsHTMLElement::IsBlockParent(mTagID)){
        return PR_TRUE;
      }
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
      // Allow <xmp> to contain text.
      if(nsHTMLElement::IsInlineParent(mTagID) || CanContainType(kCDATA)){
        return PR_TRUE;
      }
    }

    if(CanContainType(gHTMLElements[aChild].mParentBits)) {
      return PR_TRUE;
    }
 
    if(mSpecialKids) {
      if(FindTagInSet(aChild,mSpecialKids->mTags,mSpecialKids->mCount)) {
        return PR_TRUE;
      }
    }

    // Allow <p> to contain <table> only in Quirks mode, bug 43678 and bug 91927
    if (aChild == eHTMLTag_table && mTagID == eHTMLTag_p && aMode == eDTDMode_quirks) {
      return PR_TRUE;
    }
  }
  
  return PR_FALSE;
}

#ifdef DEBUG
void nsHTMLElement::DebugDumpContainment(const char* aFilename,const char* aTitle){
}

void nsHTMLElement::DebugDumpMembership(const char* aFilename){
}

void nsHTMLElement::DebugDumpContainType(const char* aFilename){
}
#endif
