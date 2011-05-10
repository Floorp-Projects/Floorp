/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/******

  This file contains the list of all HTML tags 
  See nsHTMLTags.h for access to the enum values for tags

  It is designed to be used as inline input to nsHTMLTags.cpp and
  nsHTMLContentSink *only* through the magic of C preprocessing.

  All entries must be enclosed in the macro HTML_TAG which will have cruel
  and unusual things done to it

  It is recommended (but not strictly necessary) to keep all entries
  in alphabetical order

  The first argument to HTML_TAG is both the enum identifier of the
  property and the string value. The second argument is the "creator"
  method of the form NS_New$TAGNAMEElement, that will be used by
  nsHTMLContentSink.cpp to create a content object for a tag of that
  type. Use NOTUSED, if the particular tag has a non-standard creator.

  The HTML_OTHER macro is for values in the nsHTMLTag enum that are
  not strictly tags.

  Entries *must* use only lowercase characters.

  Don't forget to update /editor/libeditor/html/nsHTMLEditUtils.cpp as well.

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
#if defined(MOZ_MEDIA)
HTML_TAG(audio, Audio)
#endif
HTML_HTMLELEMENT_TAG(b)
HTML_TAG(base, Shared)
HTML_TAG(basefont, Span)
HTML_HTMLELEMENT_TAG(bdo)
HTML_TAG(bgsound, Span)
HTML_HTMLELEMENT_TAG(big)
HTML_HTMLELEMENT_TAG(blink)
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
HTML_TAG(datalist, DataList)
HTML_HTMLELEMENT_TAG(dd)
HTML_TAG(del, Mod)
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
HTML_TAG(image, Span)
HTML_TAG(img, Image)
HTML_TAG(input, Input)
HTML_TAG(ins, Mod)
HTML_TAG(isindex, IsIndex)
HTML_HTMLELEMENT_TAG(kbd)
HTML_TAG(keygen, Span)
HTML_TAG(label, Label)
HTML_TAG(legend, Legend)
HTML_TAG(li, LI)
HTML_TAG(link, Link)
HTML_HTMLELEMENT_TAG(listing)
HTML_TAG(map, Map)
HTML_HTMLELEMENT_TAG(mark)
HTML_TAG(marquee, Div)
HTML_TAG(menu, Shared)
HTML_TAG(meta, Meta)
HTML_TAG(multicol, Span)
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
HTML_HTMLELEMENT_TAG(plaintext)
HTML_TAG(pre, Pre)
HTML_TAG(q, Shared)
HTML_HTMLELEMENT_TAG(s)
HTML_HTMLELEMENT_TAG(samp)
HTML_TAG(script, Script)
HTML_HTMLELEMENT_TAG(section)
HTML_TAG(select, Select)
HTML_HTMLELEMENT_TAG(small)
#if defined(MOZ_MEDIA)
HTML_TAG(source, Source)
#endif
HTML_TAG(span, Span)
HTML_HTMLELEMENT_TAG(strike)
HTML_HTMLELEMENT_TAG(strong)
HTML_TAG(style, Style)
HTML_HTMLELEMENT_TAG(sub)
HTML_HTMLELEMENT_TAG(sup)
HTML_TAG(table, Table)
HTML_TAG(tbody, TableSection)
HTML_TAG(td, TableCell)
HTML_TAG(textarea, TextArea)
HTML_TAG(tfoot, TableSection)
HTML_TAG(th, TableCell)
HTML_TAG(thead, TableSection)
HTML_TAG(title, Title)
HTML_TAG(tr, TableRow)
HTML_HTMLELEMENT_TAG(tt)
HTML_HTMLELEMENT_TAG(u)
HTML_TAG(ul, SharedList)
HTML_HTMLELEMENT_TAG(var)
#if defined(MOZ_MEDIA)
HTML_TAG(video, Video)
#endif
HTML_HTMLELEMENT_TAG(wbr)
HTML_HTMLELEMENT_TAG(xmp)


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
