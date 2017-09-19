/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsElementTable.h"

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

// Is aTest a member of aBitset?
static bool
TestBits(int32_t aBitset, int32_t aTest)
{
  if (aTest) {
    int32_t result = aBitset & aTest;
    return result == aTest;
  }
  return false;
}

struct HTMLElement
{
  bool IsMemberOf(int32_t aBitset) const
  {
    return TestBits(aBitset, mParentBits);
  }

#ifdef DEBUG
  nsHTMLTag mTagID;
#endif
  int mParentBits;  // defines groups that can contain this element
  bool mLeaf;
};

#ifdef DEBUG
#define ELEM(tag, parent, leaf) { eHTMLTag_##tag, parent, leaf },
#else
#define ELEM(tag, parent, leaf) { parent, leaf },
#endif

static const HTMLElement gHTMLElements[] = {
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

bool
nsHTMLElement::IsContainer(nsHTMLTag aId)
{
  return !gHTMLElements[aId].mLeaf;
}

bool
nsHTMLElement::IsBlock(nsHTMLTag aId)
{
  return gHTMLElements[aId].IsMemberOf(kBlock)       ||
         gHTMLElements[aId].IsMemberOf(kBlockEntity) ||
         gHTMLElements[aId].IsMemberOf(kHeading)     ||
         gHTMLElements[aId].IsMemberOf(kPreformatted)||
         gHTMLElements[aId].IsMemberOf(kList);
}

#ifdef DEBUG
void
CheckElementTable()
{
  for (nsHTMLTag t = eHTMLTag_unknown;
       t <= eHTMLTag_userdefined;
       t = nsHTMLTag(t + 1)) {
    MOZ_ASSERT(gHTMLElements[t].mTagID == t,
               "gHTMLElements entries does match tag list.");
  }
}
#endif
