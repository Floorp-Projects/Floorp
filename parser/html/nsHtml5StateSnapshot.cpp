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

#define nsHtml5StateSnapshot_cpp__

#include "nsAtom.h"
#include "nsHtml5AtomTable.h"
#include "nsHtml5String.h"
#include "nsNameSpaceManager.h"
#include "nsIContent.h"
#include "nsTraceRefcnt.h"
#include "jArray.h"
#include "nsHtml5ArrayCopy.h"
#include "nsAHtml5TreeBuilderState.h"
#include "nsGkAtoms.h"
#include "nsHtml5ByteReadable.h"
#include "nsHtml5Macros.h"
#include "nsIContentHandle.h"
#include "nsHtml5Portability.h"
#include "nsHtml5ContentCreatorFunction.h"

#include "nsHtml5AttributeName.h"
#include "nsHtml5ElementName.h"
#include "nsHtml5Tokenizer.h"
#include "nsHtml5TreeBuilder.h"
#include "nsHtml5MetaScanner.h"
#include "nsHtml5StackNode.h"
#include "nsHtml5UTF16Buffer.h"
#include "nsHtml5Portability.h"

#include "nsHtml5StateSnapshot.h"

nsHtml5StateSnapshot::nsHtml5StateSnapshot(
  jArray<nsHtml5StackNode*, int32_t> stack,
  jArray<nsHtml5StackNode*, int32_t> listOfActiveFormattingElements,
  jArray<int32_t, int32_t> templateModeStack,
  nsIContentHandle* formPointer,
  nsIContentHandle* headPointer,
  nsIContentHandle* deepTreeSurrogateParent,
  int32_t mode,
  int32_t originalMode,
  bool framesetOk,
  bool needToDropLF,
  bool quirks)
  : stack(stack)
  , listOfActiveFormattingElements(listOfActiveFormattingElements)
  , templateModeStack(templateModeStack)
  , formPointer(formPointer)
  , headPointer(headPointer)
  , deepTreeSurrogateParent(deepTreeSurrogateParent)
  , mode(mode)
  , originalMode(originalMode)
  , framesetOk(framesetOk)
  , needToDropLF(needToDropLF)
  , quirks(quirks)
{
  MOZ_COUNT_CTOR(nsHtml5StateSnapshot);
}

jArray<nsHtml5StackNode*, int32_t>
nsHtml5StateSnapshot::getStack()
{
  return stack;
}

jArray<int32_t, int32_t>
nsHtml5StateSnapshot::getTemplateModeStack()
{
  return templateModeStack;
}

jArray<nsHtml5StackNode*, int32_t>
nsHtml5StateSnapshot::getListOfActiveFormattingElements()
{
  return listOfActiveFormattingElements;
}

nsIContentHandle*
nsHtml5StateSnapshot::getFormPointer()
{
  return formPointer;
}

nsIContentHandle*
nsHtml5StateSnapshot::getHeadPointer()
{
  return headPointer;
}

nsIContentHandle*
nsHtml5StateSnapshot::getDeepTreeSurrogateParent()
{
  return deepTreeSurrogateParent;
}

int32_t
nsHtml5StateSnapshot::getMode()
{
  return mode;
}

int32_t
nsHtml5StateSnapshot::getOriginalMode()
{
  return originalMode;
}

bool
nsHtml5StateSnapshot::isFramesetOk()
{
  return framesetOk;
}

bool
nsHtml5StateSnapshot::isNeedToDropLF()
{
  return needToDropLF;
}

bool
nsHtml5StateSnapshot::isQuirks()
{
  return quirks;
}

int32_t
nsHtml5StateSnapshot::getListOfActiveFormattingElementsLength()
{
  return listOfActiveFormattingElements.length;
}

int32_t
nsHtml5StateSnapshot::getStackLength()
{
  return stack.length;
}

int32_t
nsHtml5StateSnapshot::getTemplateModeStackLength()
{
  return templateModeStack.length;
}

nsHtml5StateSnapshot::~nsHtml5StateSnapshot()
{
  MOZ_COUNT_DTOR(nsHtml5StateSnapshot);
  for (int32_t i = 0; i < stack.length; i++) {
    stack[i]->release(nullptr);
  }
  for (int32_t i = 0; i < listOfActiveFormattingElements.length; i++) {
    if (listOfActiveFormattingElements[i]) {
      listOfActiveFormattingElements[i]->release(nullptr);
    }
  }
}

void
nsHtml5StateSnapshot::initializeStatics()
{
}

void
nsHtml5StateSnapshot::releaseStatics()
{
}
