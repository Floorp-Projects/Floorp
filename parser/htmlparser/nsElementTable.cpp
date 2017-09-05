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

#ifdef DEBUG
#define ELEM(tag, parent, leaf) { eHTMLTag_##tag, parent, leaf },
#else
#define ELEM(tag, parent, leaf) { parent, leaf },
#endif

const nsHTMLElement gHTMLElements[] = {
  ELEM(unknown,     kNone,                       true)
  ELEM(a,           kSpecial,                    false)
  ELEM(abbr,        kPhrase,                     false)
  ELEM(acronym,     kPhrase,                     false)
  ELEM(address,     kBlock,                      false)
  ELEM(applet,      kSpecial,                    false)
  ELEM(area,        kNone,                       true)
  ELEM(article,     kBlock,                      false)
  ELEM(aside,       kBlock,                      false)
  ELEM(audio,       kSpecial,                    false)
  ELEM(b,           kFontStyle,                  false)
  ELEM(base,        kHeadContent,                true)
  ELEM(basefont,    kSpecial,                    true)
  ELEM(bdo,         kSpecial,                    false)
  ELEM(bgsound,     (kFlowEntity|kHeadMisc),     true)
  ELEM(big,         kFontStyle,                  false)
  ELEM(blockquote,  kBlock,                      false)
  ELEM(body,        kHTMLContent,                false)
  ELEM(br,          kSpecial,                    true)
  ELEM(button,      kFormControl,                false)
  ELEM(canvas,      kSpecial,                    false)
  ELEM(caption,     kNone,                       false)
  ELEM(center,      kBlock,                      false)
  ELEM(cite,        kPhrase,                     false)
  ELEM(code,        kPhrase,                     false)
  ELEM(col,         kNone,                       true)
  ELEM(colgroup,    kNone,                       false)
  ELEM(content,     kNone,                       false)
  ELEM(data,        kPhrase,                     false)
  ELEM(datalist,    kSpecial,                    false)
  ELEM(dd,          kInlineEntity,               false)
  ELEM(del,         kFlowEntity,                 false)
  ELEM(details,     kBlock,                      false)
  ELEM(dfn,         kPhrase,                     false)
  ELEM(dialog,      kBlock,                      false)
  ELEM(dir,         kList,                       false)
  ELEM(div,         kBlock,                      false)
  ELEM(dl,          kBlock,                      false)
  ELEM(dt,          kInlineEntity,               false)
  ELEM(em,          kPhrase,                     false)
  ELEM(embed,       kSpecial,                    true)
  ELEM(fieldset,    kBlock,                      false)
  ELEM(figcaption,  kPhrase,                     false)
  ELEM(figure,      kBlock,                      false)
  ELEM(font,        kFontStyle,                  false)
  ELEM(footer,      kBlock,                      false)
  ELEM(form,        kBlock,                      false)
  ELEM(frame,       kNone,                       true)
  ELEM(frameset,    kHTMLContent,                false)
  ELEM(h1,          kHeading,                    false)
  ELEM(h2,          kHeading,                    false)
  ELEM(h3,          kHeading,                    false)
  ELEM(h4,          kHeading,                    false)
  ELEM(h5,          kHeading,                    false)
  ELEM(h6,          kHeading,                    false)
  ELEM(head,        kHTMLContent,                false)
  ELEM(header,      kBlock,                      false)
  ELEM(hgroup,      kBlock,                      false)
  ELEM(hr,          kBlock,                      true)
  ELEM(html,        kNone,                       false)
  ELEM(i,           kFontStyle,                  false)
  ELEM(iframe,      kSpecial,                    false)
  ELEM(image,       kSpecial,                    true)
  ELEM(img,         kSpecial,                    true)
  ELEM(input,       kFormControl,                true)
  ELEM(ins,         kFlowEntity,                 false)
  ELEM(kbd,         kPhrase,                     false)
  ELEM(keygen,      kFlowEntity,                 true)
  ELEM(label,       kFormControl,                false)
  ELEM(legend,      kNone,                       false)
  ELEM(li,          kBlockEntity,                false)
  ELEM(link,        kAllTags - kHeadContent,     true)
  ELEM(listing,     kPreformatted,               false)
  ELEM(main,        kBlock,                      false)
  ELEM(map,         kSpecial,                    false)
  ELEM(mark,        kSpecial,                    false)
  ELEM(marquee,     kSpecial,                    false)
  ELEM(menu,        kList,                       false)
  ELEM(menuitem,    kFlowEntity,                 false)
  ELEM(meta,        kHeadContent,                true)
  ELEM(meter,       kFormControl,                false)
  ELEM(multicol,    kBlock,                      false)
  ELEM(nav,         kBlock,                      false)
  ELEM(nobr,        kExtensions,                 false)
  ELEM(noembed,     kFlowEntity,                 false)
  ELEM(noframes,    kFlowEntity,                 false)
  ELEM(noscript,    kFlowEntity|kHeadMisc,       false)
  ELEM(object,      kSpecial,                    false)
  ELEM(ol,          kList,                       false)
  ELEM(optgroup,    kNone,                       false)
  ELEM(option,      kNone,                       false)
  ELEM(output,      kSpecial,                    false)
  ELEM(p,           kBlock,                      false)
  ELEM(param,       kSpecial,                    true)
  ELEM(picture,     kSpecial,                    false)
  ELEM(plaintext,   kExtensions,                 false)
  ELEM(pre,         kBlock|kPreformatted,        false)
  ELEM(progress,    kFormControl,                false)
  ELEM(q,           kSpecial,                    false)
  ELEM(rb,          kPhrase,                     false)
  ELEM(rp,          kPhrase,                     false)
  ELEM(rt,          kPhrase,                     false)
  ELEM(rtc,         kPhrase,                     false)
  ELEM(ruby,        kPhrase,                     false)
  ELEM(s,           kFontStyle,                  false)
  ELEM(samp,        kPhrase,                     false)
  ELEM(script,      (kSpecial|kHeadContent),     false)
  ELEM(section,     kBlock,                      false)
  ELEM(select,      kFormControl,                false)
  ELEM(shadow,      kFlowEntity,                 false)
  ELEM(small,       kFontStyle,                  false)
  ELEM(source,      kSpecial,                    true)
  ELEM(span,        kSpecial,                    false)
  ELEM(strike,      kFontStyle,                  false)
  ELEM(strong,      kPhrase,                     false)
  ELEM(style,       kAllTags - kHeadContent,     false)
  ELEM(sub,         kSpecial,                    false)
  ELEM(summary,     kBlock,                      false)
  ELEM(sup,         kSpecial,                    false)
  ELEM(table,       kBlock,                      false)
  ELEM(tbody,       kNone,                       false)
  ELEM(td,          kNone,                       false)
  ELEM(textarea,    kFormControl,                false)
  ELEM(tfoot,       kNone,                       false)
  ELEM(th,          kNone,                       false)
  ELEM(thead,       kNone,                       false)
  ELEM(template,    kNone,                       false)
  ELEM(time,        kPhrase,                     false)
  ELEM(title,       kHeadContent,                false)
  ELEM(tr,          kNone,                       false)
  ELEM(track,       kSpecial,                    true)
  ELEM(tt,          kFontStyle,                  false)
  ELEM(u,           kFontStyle,                  false)
  ELEM(ul,          kList,                       false)
  ELEM(var,         kPhrase,                     false)
  ELEM(video,       kSpecial,                    false)
  ELEM(wbr,         kExtensions,                 true)
  ELEM(xmp,         kInlineEntity|kPreformatted, false)
  ELEM(text,        kFlowEntity,                 true)
  ELEM(whitespace,  kFlowEntity|kHeadMisc,       true)
  ELEM(newline,     kFlowEntity|kHeadMisc,       true)
  ELEM(comment,     kFlowEntity|kHeadMisc,       false)
  ELEM(entity,      kFlowEntity,                 false)
  ELEM(doctypeDecl, kFlowEntity,                 false)
  ELEM(markupDecl,  kFlowEntity,                 false)
  ELEM(instruction, kFlowEntity,                 false)
  ELEM(userdefined, (kFlowEntity|kHeadMisc),     false)
};

#undef ELEM

/*********************************************************************************************/

bool nsHTMLElement::IsMemberOf(int32_t aSet) const
{
  return TestBits(aSet, mParentBits);
}

bool nsHTMLElement::IsContainer(eHTMLTags aId)
{
  return !gHTMLElements[aId].mLeaf;
}

bool nsHTMLElement::IsBlock(eHTMLTags aId)
{
  return gHTMLElements[aId].IsMemberOf(kBlock)       ||
         gHTMLElements[aId].IsMemberOf(kBlockEntity) ||
         gHTMLElements[aId].IsMemberOf(kHeading)     ||
         gHTMLElements[aId].IsMemberOf(kPreformatted)||
         gHTMLElements[aId].IsMemberOf(kList);
}

#ifdef DEBUG
void CheckElementTable()
{
  for (eHTMLTags t = eHTMLTag_unknown; t <= eHTMLTag_userdefined; t = eHTMLTags(t + 1)) {
    NS_ASSERTION(gHTMLElements[t].mTagID == t, "gHTMLElements entries does match tag list.");
  }
}
#endif
