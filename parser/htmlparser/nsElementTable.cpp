/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsIAtom.h"
#include "nsElementTable.h"

/***************************************************************************** 
  Now it's time to list all the html elements all with their capabilities...
******************************************************************************/

// The Element Table (sung to the tune of Modern Major General)

const nsHTMLElement gHTMLElements[] = {
  {
    /*tag*/         eHTMLTag_unknown,
    /*parent,leaf*/ kNone, true
  },
  {
    /*tag*/         eHTMLTag_a,
    /*parent,leaf*/ kSpecial, false
  },
  {
    /*tag*/         eHTMLTag_abbr,
    /*parent,leaf*/ kPhrase, false
  },
  {
    /*tag*/         eHTMLTag_acronym,
    /*parent,leaf*/ kPhrase, false
  },
  {
    /*tag*/         eHTMLTag_address,
    /*parent,leaf*/ kBlock, false
  },
  {
    /*tag*/         eHTMLTag_applet,
    /*parent,leaf*/ kSpecial, false
  },
  {
    /*tag*/         eHTMLTag_area,
    /*parent,leaf*/ kNone, true
  },
  {
    /*tag*/         eHTMLTag_article,
    /*parent,leaf*/ kBlock, false
  },
  {
    /*tag*/         eHTMLTag_aside,
    /*parent,leaf*/ kBlock, false
  },
  {
    /*tag*/         eHTMLTag_audio,
    /*parent,leaf*/ kSpecial, false
  },
  {
    /*tag*/         eHTMLTag_b,
    /*parent,leaf*/ kFontStyle, false
  },
  {
    /*tag*/         eHTMLTag_base,
    /*parent,leaf*/ kHeadContent, true
  },
  {
    /*tag*/         eHTMLTag_basefont,
    /*parent,leaf*/ kSpecial, true
  },
  {
    /*tag*/         eHTMLTag_bdo,
    /*parent,leaf*/ kSpecial, false
  },
  {
    /*tag*/         eHTMLTag_bgsound,
    /*parent,leaf*/ (kFlowEntity|kHeadMisc), true
  },
  {
    /*tag*/         eHTMLTag_big,
    /*parent,leaf*/ kFontStyle, false
  },
  {
    /*tag*/         eHTMLTag_blockquote,
    /*parent,leaf*/ kBlock, false
  },
  {
    /*tag*/         eHTMLTag_body,
    /*parent,leaf*/ kHTMLContent, false
  },
  {
    /*tag*/         eHTMLTag_br,
    /*parent,leaf*/ kSpecial, true
  },
  {
    /*tag*/         eHTMLTag_button,
    /*parent,leaf*/ kFormControl, false
  },
  {
    /*tag*/         eHTMLTag_canvas,
    /*parent,leaf*/ kSpecial, false
  },
  {
    /*tag*/         eHTMLTag_caption,
    /*parent,leaf*/ kNone, false
  },
  {
    /*tag*/         eHTMLTag_center,
    /*parent,leaf*/ kBlock, false
  },
  {
    /*tag*/         eHTMLTag_cite,
    /*parent,leaf*/ kPhrase, false
  },
  {
    /*tag*/         eHTMLTag_code,
    /*parent,leaf*/ kPhrase, false
  },
  {
    /*tag*/         eHTMLTag_col,
    /*parent,leaf*/ kNone, true
  },
  {
    /*tag*/         eHTMLTag_colgroup,
    /*parent,leaf*/ kNone, false
  },
  {
    /*tag*/         eHTMLTag_content,
    /*parent,leaf*/ kNone, false
  },
  {
    /*tag*/         eHTMLTag_data,
    /*parent,leaf*/ kPhrase, false
  },
  {
    /*tag*/         eHTMLTag_datalist,
    /*parent,leaf*/ kSpecial, false
  },
  {
    /*tag*/         eHTMLTag_dd,
    /*parent,leaf*/ kInlineEntity, false
  },
  {
    /*tag*/         eHTMLTag_del,
    /*parent,leaf*/ kFlowEntity, false
  },
  {
    /*tag*/         eHTMLTag_details,
    /*parent,leaf*/ kBlock, false
  },
  {
    /*tag*/         eHTMLTag_dfn,
    /*parent,leaf*/ kPhrase, false
  },
  {
    /*tag*/         eHTMLTag_dir,
    /*parent,leaf*/ kList, false
  },
  {
    /*tag*/         eHTMLTag_div,
    /*parent,leaf*/ kBlock, false
  },
  {
    /*tag*/         eHTMLTag_dl,
    /*parent,leaf*/ kBlock, false
  },
  {
    /*tag*/         eHTMLTag_dt,
    /*parent,leaf*/ kInlineEntity, false
  },
  {
    /*tag*/         eHTMLTag_em,
    /*parent,leaf*/ kPhrase, false
  },
  {
    /*tag*/         eHTMLTag_embed,
    /*parent,leaf*/ kSpecial, true
  },
  {
    /*tag*/         eHTMLTag_fieldset,
    /*parent,leaf*/ kBlock, false
  },
  {
    /*tag*/         eHTMLTag_figcaption,
    /*parent,leaf*/ kPhrase, false
  },
  {
    /*tag*/         eHTMLTag_figure,
    /*parent,leaf*/ kBlock, false
  },
  {
    /*tag*/         eHTMLTag_font,
    /*parent,leaf*/ kFontStyle, false
  },
  {
    /*tag*/         eHTMLTag_footer,
    /*parent,leaf*/ kBlock, false
  },
  {
    /*tag*/         eHTMLTag_form,
    /*parent,leaf*/ kBlock, false
  },
  {
    /*tag*/         eHTMLTag_frame,
    /*parent,leaf*/ kNone, true
  },
  {
    /*tag*/         eHTMLTag_frameset,
    /*parent,leaf*/ kHTMLContent, false
  },
  {
    /*tag*/         eHTMLTag_h1,
    /*parent,leaf*/ kHeading, false
  },
  {
    /*tag*/         eHTMLTag_h2,
    /*parent,leaf*/ kHeading, false
  },
  {
    /*tag*/         eHTMLTag_h3,
    /*parent,leaf*/ kHeading, false
  },
  {
    /*tag*/         eHTMLTag_h4,
    /*parent,leaf*/ kHeading, false
  },
  {
    /*tag*/         eHTMLTag_h5,
    /*parent,leaf*/ kHeading, false
  },
  {
    /*tag*/         eHTMLTag_h6,
    /*parent,leaf*/ kHeading, false
  },
  {
    /*tag*/         eHTMLTag_head,
    /*parent,leaf*/ kHTMLContent, false
  },
  {
    /*tag*/         eHTMLTag_header,
    /*parent,leaf*/ kBlock, false
  },
  {
    /*tag*/         eHTMLTag_hgroup,
    /*parent,leaf*/ kBlock, false
  },
  {
    /*tag*/         eHTMLTag_hr,
    /*parent,leaf*/ kBlock, true
  },
  {
    /*tag*/         eHTMLTag_html,
    /*parent,leaf*/ kNone, false
  },
  {
    /*tag*/         eHTMLTag_i,
    /*parent,leaf*/ kFontStyle, false
  },
  {
    /*tag*/         eHTMLTag_iframe,
    /*parent,leaf*/ kSpecial, false
  },
  {
    /*tag*/         eHTMLTag_image,
    /*parent,leaf*/ kSpecial, true
  },
  {
    /*tag*/         eHTMLTag_img,
    /*parent,leaf*/ kSpecial, true
  },
  {
    /*tag*/         eHTMLTag_input,
    /*parent,leaf*/ kFormControl, true
  },
  {
    /*tag*/         eHTMLTag_ins,
    /*parent,leaf*/ kFlowEntity, false
  },
  {
    /*tag*/         eHTMLTag_kbd,
    /*parent,leaf*/ kPhrase, false
  },
  {
    /*tag*/         eHTMLTag_keygen,
    /*parent,leaf*/ kFlowEntity, true
  },
  {
    /*tag*/         eHTMLTag_label,
    /*parent,leaf*/ kFormControl, false
  },
  {
    /*tag*/         eHTMLTag_legend,
    /*parent,leaf*/ kNone, false
  },
  {
    /*tag*/         eHTMLTag_li,
    /*parent,leaf*/ kBlockEntity, false
  },
  {
    /*tag*/         eHTMLTag_link,
    /*parent,leaf*/ kAllTags - kHeadContent, true
  },
  {
    /*tag*/         eHTMLTag_listing,
    /*parent,leaf*/ kPreformatted, false
  },
  {
    /*tag*/         eHTMLTag_main,
    /*parent,leaf*/ kBlock, false
  },
  {
    /*tag*/         eHTMLTag_map,
    /*parent,leaf*/ kSpecial, false
  },
  {
    /*tag*/         eHTMLTag_mark,
    /*parent,leaf*/ kSpecial, false
  },
  {
    /*tag*/         eHTMLTag_marquee,
    /*parent,leaf*/ kSpecial, false
  },
  {
    /*tag*/         eHTMLTag_menu,
    /*parent,leaf*/ kList, false
  },
  {
    /*tag*/         eHTMLTag_menuitem,
    /*parent,leaf*/ kFlowEntity, false
  },
  {
    /*tag*/         eHTMLTag_meta,
    /*parent,leaf*/ kHeadContent, true
  },
  {
    /*tag*/         eHTMLTag_meter,
    /*parent,leaf*/ kFormControl, false
  },
  {
    /*tag*/         eHTMLTag_multicol,
    /*parent,leaf*/ kBlock, false
  },
  {
    /*tag*/         eHTMLTag_nav,
    /*parent,leaf*/ kBlock, false
  },
  {
    /*tag*/         eHTMLTag_nobr,
    /*parent,leaf*/ kExtensions, false
  },
  {
    /*tag*/         eHTMLTag_noembed,
    /*parent,leaf*/ kFlowEntity, false
  },
  {
    /*tag*/         eHTMLTag_noframes,
    /*parent,leaf*/ kFlowEntity, false
  },
  {
    /*tag*/         eHTMLTag_noscript,
    /*parent,leaf*/ kFlowEntity|kHeadMisc, false
  },
  {
    /*tag*/         eHTMLTag_object,
    /*parent,leaf*/ kSpecial, false
  },
  {
    /*tag*/         eHTMLTag_ol,
    /*parent,leaf*/ kList, false
  },
  {
    /*tag*/         eHTMLTag_optgroup,
    /*parent,leaf*/ kNone, false
  },
  {
    /*tag*/         eHTMLTag_option,
    /*parent,leaf*/ kNone, false
  },
  {
    /*tag*/         eHTMLTag_output,
    /*parent,leaf*/ kSpecial, false
  },
  {
    /*tag*/         eHTMLTag_p,
    /*parent,leaf*/ kBlock, false
  },
  {
    /*tag*/         eHTMLTag_param,
    /*parent,leaf*/ kSpecial, true
  },
  {
    /*tag*/         eHTMLTag_picture,
    /*parent,leaf*/ kSpecial, false
  },
  {
    /*tag*/         eHTMLTag_plaintext,
    /*parent,leaf*/ kExtensions, false
  },
  {
    /*tag*/         eHTMLTag_pre,
    /*parent,leaf*/ kBlock|kPreformatted, false
  },
  {
    /*tag*/         eHTMLTag_progress,
    /*parent,leaf*/ kFormControl, false
  },
  {
    /*tag*/         eHTMLTag_q,
    /*parent,leaf*/ kSpecial, false
  },
  {
    /*tag*/         eHTMLTag_rb,
    /*parent,leaf*/ kPhrase, false
  },
  {
    /*tag*/         eHTMLTag_rp,
    /*parent,leaf*/ kPhrase, false
  },
  {
    /*tag*/         eHTMLTag_rt,
    /*parent,leaf*/ kPhrase, false
  },
  {
    /*tag*/         eHTMLTag_rtc,
    /*parent,leaf*/ kPhrase, false
  },
  {
    /*tag*/         eHTMLTag_ruby,
    /*parent,leaf*/ kPhrase, false
  },
  {
    /*tag*/         eHTMLTag_s,
    /*parent,leaf*/ kFontStyle, false
  },
  {
    /*tag*/         eHTMLTag_samp,
    /*parent,leaf*/ kPhrase, false
  },
  {
    /*tag*/         eHTMLTag_script,
    /*parent,leaf*/ (kSpecial|kHeadContent), false
  },
  {
    /*tag*/         eHTMLTag_section,
    /*parent,leaf*/ kBlock, false
  },
  {
    /*tag*/         eHTMLTag_select,
    /*parent,leaf*/ kFormControl, false
  },
  {
    /*tag*/         eHTMLTag_shadow,
    /*parent,leaf*/ kFlowEntity, false
  },
  {
    /*tag*/         eHTMLTag_small,
    /*parent,leaf*/ kFontStyle, false
  },
  {
    /*tag*/         eHTMLTag_source,
    /*parent,leaf*/ kSpecial, true
  },
  {
    /*tag*/         eHTMLTag_span,
    /*parent,leaf*/ kSpecial, false
  },
  {
    /*tag*/         eHTMLTag_strike,
    /*parent,leaf*/ kFontStyle, false
  },
  {
    /*tag*/         eHTMLTag_strong,
    /*parent,leaf*/ kPhrase, false
  },
  {
    /*tag*/         eHTMLTag_style,
    /*parent,leaf*/ kAllTags - kHeadContent, false
  },
  {
    /*tag*/         eHTMLTag_sub,
    /*parent,leaf*/ kSpecial, false
  },
  {
    /*tag*/         eHTMLTag_summary,
    /*parent,leaf*/ kBlock, false
  },
  {
    /*tag*/         eHTMLTag_sup,
    /*parent,leaf*/ kSpecial, false
  },
  {
    /*tag*/         eHTMLTag_table,
    /*parent,leaf*/ kBlock, false
  },
  {
    /*tag*/         eHTMLTag_tbody,
    /*parent,leaf*/ kNone, false
  },
  {
    /*tag*/         eHTMLTag_td,
    /*parent,leaf*/ kNone, false
  },
  {
    /*tag*/         eHTMLTag_textarea,
    /*parent,leaf*/ kFormControl, false
  },
  {
    /*tag*/         eHTMLTag_tfoot,
    /*parent,leaf*/ kNone, false
  },
  {
    /*tag*/         eHTMLTag_th,
    /*parent,leaf*/ kNone, false
  },
  {
    /*tag*/         eHTMLTag_thead,
    /*parent,leaf*/ kNone, false
  },
  {
    /*tag*/         eHTMLTag_template,
    /*parent,leaf*/ kNone, false
  },
  {
    /*tag*/         eHTMLTag_time,
    /*parent,leaf*/ kPhrase, false
  },
  {
    /*tag*/         eHTMLTag_title,
    /*parent,leaf*/ kHeadContent, false
  },
  {
    /*tag*/         eHTMLTag_tr,
    /*parent,leaf*/ kNone, false
  },
  {
    /*tag*/         eHTMLTag_track,
    /*parent,leaf*/ kSpecial, true
  },
  {
    /*tag*/         eHTMLTag_tt,
    /*parent,leaf*/ kFontStyle, false
  },
  {
    /*tag*/         eHTMLTag_u,
    /*parent,leaf*/ kFontStyle, false
  },
  {
    /*tag*/         eHTMLTag_ul,
    /*parent,leaf*/ kList, false
  },
  {
    /*tag*/         eHTMLTag_var,
    /*parent,leaf*/ kPhrase, false
  },
  {
    /*tag*/         eHTMLTag_video,
    /*parent,leaf*/ kSpecial, false
  },
  {
    /*tag*/         eHTMLTag_wbr,
    /*parent,leaf*/ kExtensions, true
  },
  {
    /*tag*/         eHTMLTag_xmp,
    /*parent,leaf*/ kInlineEntity|kPreformatted, false
  },
  {
    /*tag*/         eHTMLTag_text,
    /*parent,leaf*/ kFlowEntity, true
  },
  {
    /*tag*/         eHTMLTag_whitespace,
    /*parent,leaf*/ kFlowEntity|kHeadMisc, true
  },
  {
    /*tag*/         eHTMLTag_newline,
    /*parent,leaf*/ kFlowEntity|kHeadMisc, true
  },
  {
    /*tag*/         eHTMLTag_comment,
    /*parent,leaf*/ kFlowEntity|kHeadMisc, false
  },
  {
    /*tag*/         eHTMLTag_entity,
    /*parent,leaf*/ kFlowEntity, false
  },
  {
    /*tag*/         eHTMLTag_doctypeDecl,
    /*parent,leaf*/ kFlowEntity, false
  },
  {
    /*tag*/         eHTMLTag_markupDecl,
    /*parent,leaf*/ kFlowEntity, false
  },
  {
    /*tag*/         eHTMLTag_instruction,
    /*parent,leaf*/ kFlowEntity, false
  },
  {
    /*tag*/         eHTMLTag_userdefined,
    /*parent,leaf*/ (kFlowEntity|kHeadMisc), false
  },
};

/*********************************************************************************************/

bool nsHTMLElement::IsContainer(eHTMLTags aChild)
{
  return !gHTMLElements[aChild].mLeaf;
}

bool nsHTMLElement::IsMemberOf(int32_t aSet) const
{
  return TestBits(aSet,mParentBits);
}

#ifdef DEBUG
void CheckElementTable()
{
  for (eHTMLTags t = eHTMLTag_unknown; t <= eHTMLTag_userdefined; t = eHTMLTags(t + 1)) {
    NS_ASSERTION(gHTMLElements[t].mTagID == t, "gHTMLElements entries does match tag list.");
  }
}
#endif
