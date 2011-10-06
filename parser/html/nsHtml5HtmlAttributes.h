/*
 * Copyright (c) 2007 Henri Sivonen
 * Copyright (c) 2008-2011 Mozilla Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * THIS IS A GENERATED FILE. PLEASE DO NOT EDIT.
 * Please edit HtmlAttributes.java instead and regenerate.
 */

#ifndef nsHtml5HtmlAttributes_h__
#define nsHtml5HtmlAttributes_h__

#include "prtypes.h"
#include "nsIAtom.h"
#include "nsHtml5AtomTable.h"
#include "nsString.h"
#include "nsINameSpaceManager.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsTraceRefcnt.h"
#include "jArray.h"
#include "nsHtml5DocumentMode.h"
#include "nsHtml5ArrayCopy.h"
#include "nsHtml5NamedCharacters.h"
#include "nsHtml5NamedCharactersAccel.h"
#include "nsHtml5Atoms.h"
#include "nsHtml5ByteReadable.h"
#include "nsIUnicodeDecoder.h"
#include "nsAHtml5TreeBuilderState.h"
#include "nsHtml5Macros.h"

class nsHtml5StreamParser;

class nsHtml5Tokenizer;
class nsHtml5TreeBuilder;
class nsHtml5MetaScanner;
class nsHtml5AttributeName;
class nsHtml5ElementName;
class nsHtml5UTF16Buffer;
class nsHtml5StateSnapshot;
class nsHtml5Portability;


class nsHtml5HtmlAttributes
{
  public:
    static nsHtml5HtmlAttributes* EMPTY_ATTRIBUTES;
  private:
    PRInt32 mode;
    PRInt32 length;
    autoJArray<nsHtml5AttributeName*,PRInt32> names;
    autoJArray<nsString*,PRInt32> values;
  public:
    nsHtml5HtmlAttributes(PRInt32 mode);
    ~nsHtml5HtmlAttributes();
    PRInt32 getIndex(nsHtml5AttributeName* name);
    PRInt32 getLength();
    nsIAtom* getLocalName(PRInt32 index);
    nsHtml5AttributeName* getAttributeName(PRInt32 index);
    PRInt32 getURI(PRInt32 index);
    nsIAtom* getPrefix(PRInt32 index);
    nsString* getValue(PRInt32 index);
    nsString* getValue(nsHtml5AttributeName* name);
    void addAttribute(nsHtml5AttributeName* name, nsString* value);
    void clear(PRInt32 m);
    void releaseValue(PRInt32 i);
    void clearWithoutReleasingContents();
    bool contains(nsHtml5AttributeName* name);
    void adjustForMath();
    void adjustForSvg();
    nsHtml5HtmlAttributes* cloneAttributes(nsHtml5AtomTable* interner);
    bool equalsAnother(nsHtml5HtmlAttributes* other);
    static void initializeStatics();
    static void releaseStatics();
};



#endif

