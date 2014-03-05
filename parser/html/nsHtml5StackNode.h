/*
 * Copyright (c) 2007 Henri Sivonen
 * Copyright (c) 2007-2011 Mozilla Foundation
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
 * Please edit StackNode.java instead and regenerate.
 */

#ifndef nsHtml5StackNode_h
#define nsHtml5StackNode_h

#include "nsIAtom.h"
#include "nsHtml5AtomTable.h"
#include "nsString.h"
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

class nsHtml5StreamParser;

class nsHtml5Tokenizer;
class nsHtml5TreeBuilder;
class nsHtml5MetaScanner;
class nsHtml5AttributeName;
class nsHtml5ElementName;
class nsHtml5HtmlAttributes;
class nsHtml5UTF16Buffer;
class nsHtml5StateSnapshot;
class nsHtml5Portability;


class nsHtml5StackNode
{
  public:
    int32_t flags;
    nsIAtom* name;
    nsIAtom* popName;
    int32_t ns;
    nsIContentHandle* node;
    nsHtml5HtmlAttributes* attributes;
  private:
    int32_t refcount;
  public:
    inline int32_t getFlags()
    {
      return flags;
    }

    int32_t getGroup();
    bool isScoping();
    bool isSpecial();
    bool isFosterParenting();
    bool isHtmlIntegrationPoint();
    nsHtml5StackNode(int32_t flags, int32_t ns, nsIAtom* name, nsIContentHandle* node, nsIAtom* popName, nsHtml5HtmlAttributes* attributes);
    nsHtml5StackNode(nsHtml5ElementName* elementName, nsIContentHandle* node);
    nsHtml5StackNode(nsHtml5ElementName* elementName, nsIContentHandle* node, nsHtml5HtmlAttributes* attributes);
    nsHtml5StackNode(nsHtml5ElementName* elementName, nsIContentHandle* node, nsIAtom* popName);
    nsHtml5StackNode(nsHtml5ElementName* elementName, nsIAtom* popName, nsIContentHandle* node);
    nsHtml5StackNode(nsHtml5ElementName* elementName, nsIContentHandle* node, nsIAtom* popName, bool markAsIntegrationPoint);
  private:
    static int32_t prepareSvgFlags(int32_t flags);
    static int32_t prepareMathFlags(int32_t flags, bool markAsIntegrationPoint);
  public:
    ~nsHtml5StackNode();
    void dropAttributes();
    void retain();
    void release();
    static void initializeStatics();
    static void releaseStatics();
};



#endif

