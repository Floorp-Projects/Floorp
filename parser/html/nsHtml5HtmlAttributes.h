/*
 * Copyright (c) 2007 Henri Sivonen
 * Copyright (c) 2008-2017 Mozilla Foundation
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

#ifndef nsHtml5HtmlAttributes_h
#define nsHtml5HtmlAttributes_h

#include "nsIAtom.h"
#include "nsHtml5AtomTable.h"
#include "nsHtml5String.h"
#include "nsNameSpaceManager.h"
#include "nsIContent.h"
#include "nsTraceRefcnt.h"
#include "jArray.h"
#include "nsHtml5ArrayCopy.h"
#include "nsAHtml5TreeBuilderState.h"
#include "nsHtml5Atoms.h"
#include "nsHtml5ByteReadable.h"
#include "nsIUnicodeDecoder.h"
#include "nsHtml5Macros.h"
#include "nsIContentHandle.h"
#include "nsTArray.h"
#include "nsHtml5AttributeEntry.h"

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
    AutoTArray<nsHtml5AttributeEntry, 5> mStorage;
    int32_t mMode;
    void AddEntry(nsHtml5AttributeEntry&& aEntry);

  public:
    explicit nsHtml5HtmlAttributes(int32_t aMode);
    ~nsHtml5HtmlAttributes();

    // Remove getIndex when removing isindex support
    int32_t getIndex(nsHtml5AttributeName* aName);

    nsHtml5String getValue(nsHtml5AttributeName* aName);
    int32_t getLength();
    nsIAtom* getLocalNameNoBoundsCheck(int32_t aIndex);
    int32_t getURINoBoundsCheck(int32_t aIndex);
    nsIAtom* getPrefixNoBoundsCheck(int32_t aIndex);
    nsHtml5String getValueNoBoundsCheck(int32_t aIndex);
    nsHtml5AttributeName* getAttributeNameNoBoundsCheck(int32_t aIndex);
    int32_t getLineNoBoundsCheck(int32_t aIndex);
    void addAttribute(nsHtml5AttributeName* aName,
                      nsHtml5String aValue,
                      int32_t aLine);
    void AddAttributeWithLocal(nsIAtom* aName,
                               nsHtml5String aValue,
                               int32_t aLine);
    void clear(int32_t aMode);
    void releaseValue(int32_t aIndex);
    void clearWithoutReleasingContents();
    bool contains(nsHtml5AttributeName* aName);
    void adjustForMath();
    void adjustForSvg();
    nsHtml5HtmlAttributes* cloneAttributes(nsHtml5AtomTable* aInterner);
    bool equalsAnother(nsHtml5HtmlAttributes* aOther);
    static void initializeStatics();
    static void releaseStatics();
};



#endif

