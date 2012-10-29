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

#define nsHtml5HtmlAttributes_cpp__

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

#include "nsHtml5Tokenizer.h"
#include "nsHtml5TreeBuilder.h"
#include "nsHtml5MetaScanner.h"
#include "nsHtml5AttributeName.h"
#include "nsHtml5ElementName.h"
#include "nsHtml5StackNode.h"
#include "nsHtml5UTF16Buffer.h"
#include "nsHtml5StateSnapshot.h"
#include "nsHtml5Portability.h"

#include "nsHtml5HtmlAttributes.h"

nsHtml5HtmlAttributes* nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES = nullptr;

nsHtml5HtmlAttributes::nsHtml5HtmlAttributes(int32_t mode)
  : mode(mode),
    length(0),
    names(jArray<nsHtml5AttributeName*,int32_t>::newJArray(5)),
    values(jArray<nsString*,int32_t>::newJArray(5))
{
  MOZ_COUNT_CTOR(nsHtml5HtmlAttributes);
}


nsHtml5HtmlAttributes::~nsHtml5HtmlAttributes()
{
  MOZ_COUNT_DTOR(nsHtml5HtmlAttributes);
  clear(0);
}

int32_t 
nsHtml5HtmlAttributes::getIndex(nsHtml5AttributeName* name)
{
  for (int32_t i = 0; i < length; i++) {
    if (names[i] == name) {
      return i;
    }
  }
  return -1;
}

nsString* 
nsHtml5HtmlAttributes::getValue(nsHtml5AttributeName* name)
{
  int32_t index = getIndex(name);
  if (index == -1) {
    return nullptr;
  } else {
    return getValueNoBoundsCheck(index);
  }
}

int32_t 
nsHtml5HtmlAttributes::getLength()
{
  return length;
}

nsIAtom* 
nsHtml5HtmlAttributes::getLocalNameNoBoundsCheck(int32_t index)
{
  MOZ_ASSERT(index < length && index >= 0, "Index out of bounds");
  return names[index]->getLocal(mode);
}

int32_t 
nsHtml5HtmlAttributes::getURINoBoundsCheck(int32_t index)
{
  MOZ_ASSERT(index < length && index >= 0, "Index out of bounds");
  return names[index]->getUri(mode);
}

nsIAtom* 
nsHtml5HtmlAttributes::getPrefixNoBoundsCheck(int32_t index)
{
  MOZ_ASSERT(index < length && index >= 0, "Index out of bounds");
  return names[index]->getPrefix(mode);
}

nsString* 
nsHtml5HtmlAttributes::getValueNoBoundsCheck(int32_t index)
{
  MOZ_ASSERT(index < length && index >= 0, "Index out of bounds");
  return values[index];
}

nsHtml5AttributeName* 
nsHtml5HtmlAttributes::getAttributeNameNoBoundsCheck(int32_t index)
{
  MOZ_ASSERT(index < length && index >= 0, "Index out of bounds");
  return names[index];
}

void 
nsHtml5HtmlAttributes::addAttribute(nsHtml5AttributeName* name, nsString* value)
{
  if (names.length == length) {
    int32_t newLen = length << 1;
    jArray<nsHtml5AttributeName*,int32_t> newNames = jArray<nsHtml5AttributeName*,int32_t>::newJArray(newLen);
    nsHtml5ArrayCopy::arraycopy(names, newNames, names.length);
    names = newNames;
    jArray<nsString*,int32_t> newValues = jArray<nsString*,int32_t>::newJArray(newLen);
    nsHtml5ArrayCopy::arraycopy(values, newValues, values.length);
    values = newValues;
  }
  names[length] = name;
  values[length] = value;
  length++;
}

void 
nsHtml5HtmlAttributes::clear(int32_t m)
{
  for (int32_t i = 0; i < length; i++) {
    names[i]->release();
    names[i] = nullptr;
    nsHtml5Portability::releaseString(values[i]);
    values[i] = nullptr;
  }
  length = 0;
  mode = m;
}

void 
nsHtml5HtmlAttributes::releaseValue(int32_t i)
{
  nsHtml5Portability::releaseString(values[i]);
}

void 
nsHtml5HtmlAttributes::clearWithoutReleasingContents()
{
  for (int32_t i = 0; i < length; i++) {
    names[i] = nullptr;
    values[i] = nullptr;
  }
  length = 0;
}

bool 
nsHtml5HtmlAttributes::contains(nsHtml5AttributeName* name)
{
  for (int32_t i = 0; i < length; i++) {
    if (name->equalsAnother(names[i])) {
      return true;
    }
  }
  return false;
}

void 
nsHtml5HtmlAttributes::adjustForMath()
{
  mode = NS_HTML5ATTRIBUTE_NAME_MATHML;
}

void 
nsHtml5HtmlAttributes::adjustForSvg()
{
  mode = NS_HTML5ATTRIBUTE_NAME_SVG;
}

nsHtml5HtmlAttributes* 
nsHtml5HtmlAttributes::cloneAttributes(nsHtml5AtomTable* interner)
{
  MOZ_ASSERT((!length) || !mode || mode == 3);
  nsHtml5HtmlAttributes* clone = new nsHtml5HtmlAttributes(0);
  for (int32_t i = 0; i < length; i++) {
    clone->addAttribute(names[i]->cloneAttributeName(interner), nsHtml5Portability::newStringFromString(values[i]));
  }
  return clone;
}

bool 
nsHtml5HtmlAttributes::equalsAnother(nsHtml5HtmlAttributes* other)
{
  MOZ_ASSERT(!mode || mode == 3, "Trying to compare attributes in foreign content.");
  int32_t otherLength = other->getLength();
  if (length != otherLength) {
    return false;
  }
  for (int32_t i = 0; i < length; i++) {
    bool found = false;
    nsIAtom* ownLocal = names[i]->getLocal(NS_HTML5ATTRIBUTE_NAME_HTML);
    for (int32_t j = 0; j < otherLength; j++) {
      if (ownLocal == other->names[j]->getLocal(NS_HTML5ATTRIBUTE_NAME_HTML)) {
        found = true;
        if (!nsHtml5Portability::stringEqualsString(values[i], other->values[j])) {
          return false;
        }
      }
    }
    if (!found) {
      return false;
    }
  }
  return true;
}

void
nsHtml5HtmlAttributes::initializeStatics()
{
  EMPTY_ATTRIBUTES = new nsHtml5HtmlAttributes(NS_HTML5ATTRIBUTE_NAME_HTML);
}

void
nsHtml5HtmlAttributes::releaseStatics()
{
  delete EMPTY_ATTRIBUTES;
}


