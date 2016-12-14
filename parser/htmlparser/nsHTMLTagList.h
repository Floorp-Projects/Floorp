/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// IWYU pragma: private, include "nsHTMLTags.h"

/******

  This file contains the list of all HTML tags.
  See nsHTMLTags.h for access to the enum values for tags.

  It is designed to be used as inline input to nsHTMLTags.cpp and
  nsHTMLContentSink *only* through the magic of C preprocessing.

  All entries must be enclosed in the macro HTML_TAG which will have cruel
  and unusual things done to it.

  It is recommended (but not strictly necessary) to keep all entries
  in alphabetical order.

  The first argument to HTML_TAG is both the enum identifier of the
  property and the string value. The second argument is the "creator"
  method of the form NS_New$TAGNAMEElement, that will be used by
  nsHTMLContentSink.cpp to create a content object for a tag of that
  type. Use NOTUSED, if the particular tag has a non-standard creator.

  The HTML_OTHER macro is for values in the nsHTMLTag enum that are
  not strictly tags.

  Entries *must* use only lowercase characters.

  Don't forget to update /editor/libeditor/HTMLEditUtils.cpp as well.

  ** Break these invariants and bad things will happen. **

 ******/
HTML_TAG(a, Anchor)
HTML_HTMLELEMENT_TAG(abbr)
HTML_HTMLELEMENT_TAG(acronym)
HTML_HTMLELEMENT_TAG(address)
HTML_TAG(applet, SharedObject)
HTML_TAG(area, Area)
HTML_HTMLELEMENT_TAG(article)
HTML_HTMLELEMENT_TAG(aside)
HTML_TAG(audio, Audio)
HTML_HTMLELEMENT_TAG(b)
HTML_TAG(base, Shared)
HTML_HTMLELEMENT_TAG(basefont)
HTML_HTMLELEMENT_TAG(bdo)
HTML_TAG(bgsound, Unknown)
HTML_HTMLELEMENT_TAG(big)
HTML_TAG(blockquote, Shared)
HTML_TAG(body, Body)
HTML_TAG(br, BR)
HTML_TAG(button, Button)
HTML_TAG(canvas, Canvas)
HTML_TAG(caption, TableCaption)
HTML_HTMLELEMENT_TAG(center)
HTML_HTMLELEMENT_TAG(cite)
HTML_HTMLELEMENT_TAG(code)
HTML_TAG(col, TableCol)
HTML_TAG(colgroup, TableCol)
HTML_TAG(content, Content)
HTML_TAG(data, Data)
HTML_TAG(datalist, DataList)
HTML_HTMLELEMENT_TAG(dd)
HTML_TAG(del, Mod)
HTML_TAG(details, Details)
HTML_HTMLELEMENT_TAG(dfn)
HTML_TAG(dir, Shared)
HTML_TAG(div, Div)
HTML_TAG(dl, SharedList)
HTML_HTMLELEMENT_TAG(dt)
HTML_HTMLELEMENT_TAG(em)
HTML_TAG(embed, SharedObject)
HTML_TAG(fieldset, FieldSet)
HTML_HTMLELEMENT_TAG(figcaption)
HTML_HTMLELEMENT_TAG(figure)
HTML_TAG(font, Font)
HTML_HTMLELEMENT_TAG(footer)
HTML_TAG(form, Form)
HTML_TAG(frame, Frame)
HTML_TAG(frameset, FrameSet)
HTML_TAG(h1, Heading)
HTML_TAG(h2, Heading)
HTML_TAG(h3, Heading)
HTML_TAG(h4, Heading)
HTML_TAG(h5, Heading)
HTML_TAG(h6, Heading)
HTML_TAG(head, Shared)
HTML_HTMLELEMENT_TAG(header)
HTML_HTMLELEMENT_TAG(hgroup)
HTML_TAG(hr, HR)
HTML_TAG(html, Shared)
HTML_HTMLELEMENT_TAG(i)
HTML_TAG(iframe, IFrame)
HTML_HTMLELEMENT_TAG(image)
HTML_TAG(img, Image)
HTML_TAG(input, Input)
HTML_TAG(ins, Mod)
HTML_HTMLELEMENT_TAG(kbd)
HTML_TAG(keygen, Span)
HTML_TAG(label, Label)
HTML_TAG(legend, Legend)
HTML_TAG(li, LI)
HTML_TAG(link, Link)
HTML_TAG(listing, Pre)
HTML_HTMLELEMENT_TAG(main)
HTML_TAG(map, Map)
HTML_HTMLELEMENT_TAG(mark)
HTML_TAG(marquee, Div)
HTML_TAG(menu, Menu)
HTML_TAG(menuitem, MenuItem)
HTML_TAG(meta, Meta)
HTML_TAG(meter, Meter)
HTML_TAG(multicol, Unknown)
HTML_HTMLELEMENT_TAG(nav)
HTML_HTMLELEMENT_TAG(nobr)
HTML_HTMLELEMENT_TAG(noembed)
HTML_HTMLELEMENT_TAG(noframes)
HTML_HTMLELEMENT_TAG(noscript)
HTML_TAG(object, Object)
HTML_TAG(ol, SharedList)
HTML_TAG(optgroup, OptGroup)
HTML_TAG(option, Option)
HTML_TAG(output, Output)
HTML_TAG(p, Paragraph)
HTML_TAG(param, Shared)
HTML_TAG(picture, Picture)
HTML_HTMLELEMENT_TAG(plaintext)
HTML_TAG(pre, Pre)
HTML_TAG(progress, Progress)
HTML_TAG(q, Shared)
HTML_HTMLELEMENT_TAG(rb)
HTML_HTMLELEMENT_TAG(rp)
HTML_HTMLELEMENT_TAG(rt)
HTML_HTMLELEMENT_TAG(rtc)
HTML_HTMLELEMENT_TAG(ruby)
HTML_HTMLELEMENT_TAG(s)
HTML_HTMLELEMENT_TAG(samp)
HTML_TAG(script, Script)
HTML_HTMLELEMENT_TAG(section)
HTML_TAG(select, Select)
HTML_TAG(shadow, Shadow)
HTML_HTMLELEMENT_TAG(small)
HTML_TAG(source, Source)
HTML_TAG(span, Span)
HTML_HTMLELEMENT_TAG(strike)
HTML_HTMLELEMENT_TAG(strong)
HTML_TAG(style, Style)
HTML_HTMLELEMENT_TAG(sub)
HTML_TAG(summary, Summary)
HTML_HTMLELEMENT_TAG(sup)
HTML_TAG(table, Table)
HTML_TAG(tbody, TableSection)
HTML_TAG(td, TableCell)
HTML_TAG(textarea, TextArea)
HTML_TAG(tfoot, TableSection)
HTML_TAG(th, TableCell)
HTML_TAG(thead, TableSection)
HTML_TAG(template, Template)
HTML_TAG(time, Time)
HTML_TAG(title, Title)
HTML_TAG(tr, TableRow)
HTML_TAG(track, Track)
HTML_HTMLELEMENT_TAG(tt)
HTML_HTMLELEMENT_TAG(u)
HTML_TAG(ul, SharedList)
HTML_HTMLELEMENT_TAG(var)
HTML_TAG(video, Video)
HTML_HTMLELEMENT_TAG(wbr)
HTML_TAG(xmp, Pre)


/* These are not for tags. But they will be included in the nsHTMLTag
   enum anyway */

HTML_OTHER(text)
HTML_OTHER(whitespace)
HTML_OTHER(newline)
HTML_OTHER(comment)
HTML_OTHER(entity)
HTML_OTHER(doctypeDecl)
HTML_OTHER(markupDecl)
HTML_OTHER(instruction)
