/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsElementTable.h"

struct HTMLElement
{
#ifdef DEBUG
  nsHTMLTag mTagID;
#endif
  bool mIsBlock;
  bool mIsContainer;
};

#ifdef DEBUG
#define ELEM(tag, block, container) { eHTMLTag_##tag, block, container },
#else
#define ELEM(tag, block, container) { block, container },
#endif

#define ____ false    // This makes the table easier to read.

// Note that the mIsBlock field disagrees with
// https://developer.mozilla.org/en-US/docs/Web/HTML/Block-level_elements for
// the following elements: center, details, dialog, dir, dt, figcaption,
// listing, menu, multicol, noscript, output, summary, tfoot, video.
//
// mrbkap thinks that the field values were pulled from the old HTML4 DTD and
// then got modified in mostly random ways to make the old parser's behavior
// compatible with the web. So it might make sense to change the mIsBlock
// values for the abovementioned tags at some point.
//
static const HTMLElement gHTMLElements[] = {
  // clang-format off
  ELEM(unknown,     ____, ____)
  ELEM(a,           ____, true)
  ELEM(abbr,        ____, true)
  ELEM(acronym,     ____, true)
  ELEM(address,     true, true)
  ELEM(applet,      ____, true)
  ELEM(area,        ____, ____)
  ELEM(article,     true, true)
  ELEM(aside,       true, true)
  ELEM(audio,       ____, true)
  ELEM(b,           ____, true)
  ELEM(base,        ____, ____)
  ELEM(basefont,    ____, ____)
  ELEM(bdo,         ____, true)
  ELEM(bgsound,     ____, ____)
  ELEM(big,         ____, true)
  ELEM(blockquote,  true, true)
  ELEM(body,        ____, true)
  ELEM(br,          ____, ____)
  ELEM(button,      ____, true)
  ELEM(canvas,      ____, true)
  ELEM(caption,     ____, true)
  ELEM(center,      true, true)
  ELEM(cite,        ____, true)
  ELEM(code,        ____, true)
  ELEM(col,         ____, ____)
  ELEM(colgroup,    ____, true)
  ELEM(data,        ____, true)
  ELEM(datalist,    ____, true)
  ELEM(dd,          ____, true)
  ELEM(del,         ____, true)
  ELEM(details,     true, true)
  ELEM(dfn,         ____, true)
  ELEM(dialog,      true, true)
  ELEM(dir,         true, true)
  ELEM(div,         true, true)
  ELEM(dl,          true, true)
  ELEM(dt,          ____, true)
  ELEM(em,          ____, true)
  ELEM(embed,       ____, ____)
  ELEM(fieldset,    true, true)
  ELEM(figcaption,  ____, true)
  ELEM(figure,      true, true)
  ELEM(font,        ____, true)
  ELEM(footer,      true, true)
  ELEM(form,        true, true)
  ELEM(frame,       ____, ____)
  ELEM(frameset,    ____, true)
  ELEM(h1,          true, true)
  ELEM(h2,          true, true)
  ELEM(h3,          true, true)
  ELEM(h4,          true, true)
  ELEM(h5,          true, true)
  ELEM(h6,          true, true)
  ELEM(head,        ____, true)
  ELEM(header,      true, true)
  ELEM(hgroup,      true, true)
  ELEM(hr,          true, ____)
  ELEM(html,        ____, true)
  ELEM(i,           ____, true)
  ELEM(iframe,      ____, true)
  ELEM(image,       ____, ____)
  ELEM(img,         ____, ____)
  ELEM(input,       ____, ____)
  ELEM(ins,         ____, true)
  ELEM(kbd,         ____, true)
  ELEM(keygen,      ____, ____)
  ELEM(label,       ____, true)
  ELEM(legend,      ____, true)
  ELEM(li,          true, true)
  ELEM(link,        ____, ____)
  ELEM(listing,     true, true)
  ELEM(main,        true, true)
  ELEM(map,         ____, true)
  ELEM(mark,        ____, true)
  ELEM(marquee,     ____, true)
  ELEM(menu,        true, true)
  ELEM(menuitem,    ____, true)
  ELEM(meta,        ____, ____)
  ELEM(meter,       ____, true)
  ELEM(multicol,    true, true)
  ELEM(nav,         true, true)
  ELEM(nobr,        ____, true)
  ELEM(noembed,     ____, true)
  ELEM(noframes,    ____, true)
  ELEM(noscript,    ____, true)
  ELEM(object,      ____, true)
  ELEM(ol,          true, true)
  ELEM(optgroup,    ____, true)
  ELEM(option,      ____, true)
  ELEM(output,      ____, true)
  ELEM(p,           true, true)
  ELEM(param,       ____, ____)
  ELEM(picture,     ____, true)
  ELEM(plaintext,   ____, true)
  ELEM(pre,         true, true)
  ELEM(progress,    ____, true)
  ELEM(q,           ____, true)
  ELEM(rb,          ____, true)
  ELEM(rp,          ____, true)
  ELEM(rt,          ____, true)
  ELEM(rtc,         ____, true)
  ELEM(ruby,        ____, true)
  ELEM(s,           ____, true)
  ELEM(samp,        ____, true)
  ELEM(script,      ____, true)
  ELEM(section,     true, true)
  ELEM(select,      ____, true)
  ELEM(small,       ____, true)
  ELEM(slot,        ____, true)
  ELEM(source,      ____, ____)
  ELEM(span,        ____, true)
  ELEM(strike,      ____, true)
  ELEM(strong,      ____, true)
  ELEM(style,       ____, true)
  ELEM(sub,         ____, true)
  ELEM(summary,     true, true)
  ELEM(sup,         ____, true)
  ELEM(table,       true, true)
  ELEM(tbody,       ____, true)
  ELEM(td,          ____, true)
  ELEM(textarea,    ____, true)
  ELEM(tfoot,       ____, true)
  ELEM(th,          ____, true)
  ELEM(thead,       ____, true)
  ELEM(template,    ____, true)
  ELEM(time,        ____, true)
  ELEM(title,       ____, true)
  ELEM(tr,          ____, true)
  ELEM(track,       ____, ____)
  ELEM(tt,          ____, true)
  ELEM(u,           ____, true)
  ELEM(ul,          true, true)
  ELEM(var,         ____, true)
  ELEM(video,       ____, true)
  ELEM(wbr,         ____, ____)
  ELEM(xmp,         ____, true)
  ELEM(text,        ____, ____)
  ELEM(whitespace,  ____, ____)
  ELEM(newline,     ____, ____)
  ELEM(comment,     ____, true)
  ELEM(entity,      ____, true)
  ELEM(doctypeDecl, ____, true)
  ELEM(markupDecl,  ____, true)
  ELEM(instruction, ____, true)
  ELEM(userdefined, ____, true)
  // clang-format on
};

#undef ELEM
#undef ____

bool
nsHTMLElement::IsContainer(nsHTMLTag aId)
{
  return gHTMLElements[aId].mIsContainer;
}

bool
nsHTMLElement::IsBlock(nsHTMLTag aId)
{
  return gHTMLElements[aId].mIsBlock;
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
