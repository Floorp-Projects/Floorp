/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// IWYU pragma: private, include "nsHTMLTags.h"

/******

  This file contains the list of all HTML tags.
  See nsHTMLTags.h for access to the enum values for tags.

  It is designed to be used as input to various places that will define the
  HTML_TAG macro in useful ways through the magic of C preprocessing.

  All entries must be enclosed in the macro HTML_TAG which will have cruel
  and unusual things done to it.

  It is recommended (but not strictly necessary) to keep all entries
  in alphabetical order.

  The first argument to HTML_TAG is the tag name. The second argument is the
  "creator" method of the form NS_New$TAGNAMEElement, that will be used by
  nsHTMLContentSink.cpp to create a content object for a tag of that
  type. Use NOTUSED, if the particular tag has a non-standard creator.
  The third argument is the interface name specified for this element
  in the HTML specification. It can be empty if the relevant interface name
  is "HTMLElement".

  The HTML_OTHER macro is for values in the nsHTMLTag enum that are
  not strictly tags.

  Entries *must* use only lowercase characters.

  Don't forget to update /editor/libeditor/HTMLEditUtils.cpp as well.

  ** Break these invariants and bad things will happen. **

 ******/

#define HTML_HTMLELEMENT_TAG(_tag) HTML_TAG(_tag, , )

HTML_TAG(a, Anchor, Anchor)
HTML_HTMLELEMENT_TAG(abbr)
HTML_HTMLELEMENT_TAG(acronym)
HTML_HTMLELEMENT_TAG(address)
HTML_TAG(applet, Unknown, Unknown)
HTML_TAG(area, Area, Area)
HTML_HTMLELEMENT_TAG(article)
HTML_HTMLELEMENT_TAG(aside)
HTML_TAG(audio, Audio, Audio)
HTML_HTMLELEMENT_TAG(b)
HTML_TAG(base, Shared, Base)
HTML_HTMLELEMENT_TAG(basefont)
HTML_HTMLELEMENT_TAG(bdo)
HTML_TAG(bgsound, Unknown, Unknown)
HTML_HTMLELEMENT_TAG(big)
HTML_TAG(blockquote, Shared, Quote)
HTML_TAG(body, Body, Body)
HTML_TAG(br, BR, BR)
HTML_TAG(button, Button, Button)
HTML_TAG(canvas, Canvas, Canvas)
HTML_TAG(caption, TableCaption, TableCaption)
HTML_HTMLELEMENT_TAG(center)
HTML_HTMLELEMENT_TAG(cite)
HTML_HTMLELEMENT_TAG(code)
HTML_TAG(col, TableCol, TableCol)
HTML_TAG(colgroup, TableCol, TableCol)
HTML_TAG(content, Content, Content)
HTML_TAG(data, Data, Data)
HTML_TAG(datalist, DataList, DataList)
HTML_HTMLELEMENT_TAG(dd)
HTML_TAG(del, Mod, Mod)
HTML_TAG(details, Details, Details)
HTML_HTMLELEMENT_TAG(dfn)
HTML_TAG(dialog, Dialog, Dialog)
HTML_TAG(dir, Shared, Directory)
HTML_TAG(div, Div, Div)
HTML_TAG(dl, SharedList, DList)
HTML_HTMLELEMENT_TAG(dt)
HTML_HTMLELEMENT_TAG(em)
HTML_TAG(embed, Embed, Embed)
HTML_TAG(fieldset, FieldSet, FieldSet)
HTML_HTMLELEMENT_TAG(figcaption)
HTML_HTMLELEMENT_TAG(figure)
HTML_TAG(font, Font, Font)
HTML_HTMLELEMENT_TAG(footer)
HTML_TAG(form, Form, Form)
HTML_TAG(frame, Frame, Frame)
HTML_TAG(frameset, FrameSet, FrameSet)
HTML_TAG(h1, Heading, Heading)
HTML_TAG(h2, Heading, Heading)
HTML_TAG(h3, Heading, Heading)
HTML_TAG(h4, Heading, Heading)
HTML_TAG(h5, Heading, Heading)
HTML_TAG(h6, Heading, Heading)
HTML_TAG(head, Shared, Head)
HTML_HTMLELEMENT_TAG(header)
HTML_HTMLELEMENT_TAG(hgroup)
HTML_TAG(hr, HR, HR)
HTML_TAG(html, Shared, Html)
HTML_HTMLELEMENT_TAG(i)
HTML_TAG(iframe, IFrame, IFrame)
HTML_HTMLELEMENT_TAG(image)
HTML_TAG(img, Image, Image)
HTML_TAG(input, Input, Input)
HTML_TAG(ins, Mod, Mod)
HTML_HTMLELEMENT_TAG(kbd)
HTML_TAG(keygen, Span, Span)
HTML_TAG(label, Label, Label)
HTML_TAG(legend, Legend, Legend)
HTML_TAG(li, LI, LI)
HTML_TAG(link, Link, Link)
HTML_TAG(listing, Pre, Pre)
HTML_HTMLELEMENT_TAG(main)
HTML_TAG(map, Map, Map)
HTML_HTMLELEMENT_TAG(mark)
HTML_TAG(marquee, Div, Div)
HTML_TAG(menu, Menu, Menu)
HTML_TAG(menuitem, MenuItem, MenuItem)
HTML_TAG(meta, Meta, Meta)
HTML_TAG(meter, Meter, Meter)
HTML_TAG(multicol, Unknown, Unknown)
HTML_HTMLELEMENT_TAG(nav)
HTML_HTMLELEMENT_TAG(nobr)
HTML_HTMLELEMENT_TAG(noembed)
HTML_HTMLELEMENT_TAG(noframes)
HTML_HTMLELEMENT_TAG(noscript)
HTML_TAG(object, Object, Object)
HTML_TAG(ol, SharedList, OList)
HTML_TAG(optgroup, OptGroup, OptGroup)
HTML_TAG(option, Option, Option)
HTML_TAG(output, Output, Output)
HTML_TAG(p, Paragraph, Paragraph)
HTML_TAG(param, Shared, Param)
HTML_TAG(picture, Picture, Picture)
HTML_HTMLELEMENT_TAG(plaintext)
HTML_TAG(pre, Pre, Pre)
HTML_TAG(progress, Progress, Progress)
HTML_TAG(q, Shared, Quote)
HTML_HTMLELEMENT_TAG(rb)
HTML_HTMLELEMENT_TAG(rp)
HTML_HTMLELEMENT_TAG(rt)
HTML_HTMLELEMENT_TAG(rtc)
HTML_HTMLELEMENT_TAG(ruby)
HTML_HTMLELEMENT_TAG(s)
HTML_HTMLELEMENT_TAG(samp)
HTML_TAG(script, Script, Script)
HTML_HTMLELEMENT_TAG(section)
HTML_TAG(select, Select, Select)
HTML_TAG(shadow, Shadow, Shadow)
HTML_HTMLELEMENT_TAG(small)
HTML_TAG(source, Source, Source)
HTML_TAG(span, Span, Span)
HTML_HTMLELEMENT_TAG(strike)
HTML_HTMLELEMENT_TAG(strong)
HTML_TAG(style, Style, Style)
HTML_HTMLELEMENT_TAG(sub)
HTML_TAG(summary, Summary, )
HTML_HTMLELEMENT_TAG(sup)
HTML_TAG(table, Table, Table)
HTML_TAG(tbody, TableSection, TableSection)
HTML_TAG(td, TableCell, TableCell)
HTML_TAG(textarea, TextArea, TextArea)
HTML_TAG(tfoot, TableSection, TableSection)
HTML_TAG(th, TableCell, TableCell)
HTML_TAG(thead, TableSection, TableSection)
HTML_TAG(template, Template, Template)
HTML_TAG(time, Time, Time)
HTML_TAG(title, Title, Title)
HTML_TAG(tr, TableRow, TableRow)
HTML_TAG(track, Track, Track)
HTML_HTMLELEMENT_TAG(tt)
HTML_HTMLELEMENT_TAG(u)
HTML_TAG(ul, SharedList, UList)
HTML_HTMLELEMENT_TAG(var)
HTML_TAG(video, Video, Video)
HTML_HTMLELEMENT_TAG(wbr)
HTML_TAG(xmp, Pre, Pre)


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

#undef HTML_HTMLELEMENT_TAG
