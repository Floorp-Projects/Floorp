/*
 * Copyright (c) 2009-2010 Mozilla Foundation
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
 * Please edit StateSnapshot.java instead and regenerate.
 */

#ifndef nsHtml5StateSnapshot_h__
#define nsHtml5StateSnapshot_h__

#include "prtypes.h"
#include "nsIAtom.h"
#include "nsHtml5AtomTable.h"
#include "nsString.h"
#include "nsINameSpaceManager.h"
#include "nsIContent.h"
#include "nsTraceRefcnt.h"
#include "jArray.h"
#include "nsHtml5ArrayCopy.h"
#include "nsAHtml5TreeBuilderState.h"
#include "nsHtml5Atoms.h"
#include "nsHtml5ByteReadable.h"
#include "nsIUnicodeDecoder.h"
#include "nsHtml5Macros.h"

class nsHtml5StreamParser;

class nsHtml5Tokenizer;
class nsHtml5TreeBuilder;
class nsHtml5MetaScanner;
class nsHtml5AttributeName;
class nsHtml5ElementName;
class nsHtml5HtmlAttributes;
class nsHtml5UTF16Buffer;
class nsHtml5Portability;


class nsHtml5StateSnapshot : public nsAHtml5TreeBuilderState
{
  private:
    autoJArray<nsHtml5StackNode*,PRInt32> stack;
    autoJArray<nsHtml5StackNode*,PRInt32> listOfActiveFormattingElements;
    nsIContent** formPointer;
    nsIContent** headPointer;
    nsIContent** deepTreeSurrogateParent;
    PRInt32 mode;
    PRInt32 originalMode;
    bool framesetOk;
    bool needToDropLF;
    bool quirks;
  public:
    nsHtml5StateSnapshot(jArray<nsHtml5StackNode*,PRInt32> stack, jArray<nsHtml5StackNode*,PRInt32> listOfActiveFormattingElements, nsIContent** formPointer, nsIContent** headPointer, nsIContent** deepTreeSurrogateParent, PRInt32 mode, PRInt32 originalMode, bool framesetOk, bool needToDropLF, bool quirks);
    jArray<nsHtml5StackNode*,PRInt32> getStack();
    jArray<nsHtml5StackNode*,PRInt32> getListOfActiveFormattingElements();
    nsIContent** getFormPointer();
    nsIContent** getHeadPointer();
    nsIContent** getDeepTreeSurrogateParent();
    PRInt32 getMode();
    PRInt32 getOriginalMode();
    bool isFramesetOk();
    bool isNeedToDropLF();
    bool isQuirks();
    PRInt32 getListOfActiveFormattingElementsLength();
    PRInt32 getStackLength();
    ~nsHtml5StateSnapshot();
    static void initializeStatics();
    static void releaseStatics();
};



#endif

