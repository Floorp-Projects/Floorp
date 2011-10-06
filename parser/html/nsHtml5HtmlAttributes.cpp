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

nsHtml5HtmlAttributes* nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES = nsnull;

nsHtml5HtmlAttributes::nsHtml5HtmlAttributes(PRInt32 mode)
  : mode(mode),
    length(0),
    names(jArray<nsHtml5AttributeName*,PRInt32>::newJArray(5)),
    values(jArray<nsString*,PRInt32>::newJArray(5))
{
  MOZ_COUNT_CTOR(nsHtml5HtmlAttributes);
}


nsHtml5HtmlAttributes::~nsHtml5HtmlAttributes()
{
  MOZ_COUNT_DTOR(nsHtml5HtmlAttributes);
  clear(0);
}

PRInt32 
nsHtml5HtmlAttributes::getIndex(nsHtml5AttributeName* name)
{
  for (PRInt32 i = 0; i < length; i++) {
    if (names[i] == name) {
      return i;
    }
  }
  return -1;
}

PRInt32 
nsHtml5HtmlAttributes::getLength()
{
  return length;
}

nsIAtom* 
nsHtml5HtmlAttributes::getLocalName(PRInt32 index)
{
  if (index < length && index >= 0) {
    return names[index]->getLocal(mode);
  } else {
    return nsnull;
  }
}

nsHtml5AttributeName* 
nsHtml5HtmlAttributes::getAttributeName(PRInt32 index)
{
  if (index < length && index >= 0) {
    return names[index];
  } else {
    return nsnull;
  }
}

PRInt32 
nsHtml5HtmlAttributes::getURI(PRInt32 index)
{
  if (index < length && index >= 0) {
    return names[index]->getUri(mode);
  } else {
    return nsnull;
  }
}

nsIAtom* 
nsHtml5HtmlAttributes::getPrefix(PRInt32 index)
{
  if (index < length && index >= 0) {
    return names[index]->getPrefix(mode);
  } else {
    return nsnull;
  }
}

nsString* 
nsHtml5HtmlAttributes::getValue(PRInt32 index)
{
  if (index < length && index >= 0) {
    return values[index];
  } else {
    return nsnull;
  }
}

nsString* 
nsHtml5HtmlAttributes::getValue(nsHtml5AttributeName* name)
{
  PRInt32 index = getIndex(name);
  if (index == -1) {
    return nsnull;
  } else {
    return getValue(index);
  }
}

void 
nsHtml5HtmlAttributes::addAttribute(nsHtml5AttributeName* name, nsString* value)
{
  if (names.length == length) {
    PRInt32 newLen = length << 1;
    jArray<nsHtml5AttributeName*,PRInt32> newNames = jArray<nsHtml5AttributeName*,PRInt32>::newJArray(newLen);
    nsHtml5ArrayCopy::arraycopy(names, newNames, names.length);
    names = newNames;
    jArray<nsString*,PRInt32> newValues = jArray<nsString*,PRInt32>::newJArray(newLen);
    nsHtml5ArrayCopy::arraycopy(values, newValues, values.length);
    values = newValues;
  }
  names[length] = name;
  values[length] = value;
  length++;
}

void 
nsHtml5HtmlAttributes::clear(PRInt32 m)
{
  for (PRInt32 i = 0; i < length; i++) {
    names[i]->release();
    names[i] = nsnull;
    nsHtml5Portability::releaseString(values[i]);
    values[i] = nsnull;
  }
  length = 0;
  mode = m;
}

void 
nsHtml5HtmlAttributes::releaseValue(PRInt32 i)
{
  nsHtml5Portability::releaseString(values[i]);
}

void 
nsHtml5HtmlAttributes::clearWithoutReleasingContents()
{
  for (PRInt32 i = 0; i < length; i++) {
    names[i] = nsnull;
    values[i] = nsnull;
  }
  length = 0;
}

bool 
nsHtml5HtmlAttributes::contains(nsHtml5AttributeName* name)
{
  for (PRInt32 i = 0; i < length; i++) {
    if (name->equalsAnother(names[i])) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
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

  nsHtml5HtmlAttributes* clone = new nsHtml5HtmlAttributes(0);
  for (PRInt32 i = 0; i < length; i++) {
    clone->addAttribute(names[i]->cloneAttributeName(interner), nsHtml5Portability::newStringFromString(values[i]));
  }
  return clone;
}

bool 
nsHtml5HtmlAttributes::equalsAnother(nsHtml5HtmlAttributes* other)
{

  PRInt32 otherLength = other->getLength();
  if (length != otherLength) {
    return PR_FALSE;
  }
  for (PRInt32 i = 0; i < length; i++) {
    bool found = false;
    nsIAtom* ownLocal = names[i]->getLocal(NS_HTML5ATTRIBUTE_NAME_HTML);
    for (PRInt32 j = 0; j < otherLength; j++) {
      if (ownLocal == other->names[j]->getLocal(NS_HTML5ATTRIBUTE_NAME_HTML)) {
        found = PR_TRUE;
        if (!nsHtml5Portability::stringEqualsString(values[i], other->values[j])) {
          return PR_FALSE;
        }
      }
    }
    if (!found) {
      return PR_FALSE;
    }
  }
  return PR_TRUE;
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


