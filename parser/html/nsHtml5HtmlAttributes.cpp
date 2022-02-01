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

#define nsHtml5HtmlAttributes_cpp__

#include "jArray.h"
#include "nsAHtml5TreeBuilderState.h"
#include "nsAtom.h"
#include "nsHtml5ArrayCopy.h"
#include "nsHtml5AtomTable.h"
#include "nsHtml5ByteReadable.h"
#include "nsHtml5Macros.h"
#include "nsHtml5String.h"
#include "nsIContent.h"
#include "nsIContentHandle.h"
#include "nsNameSpaceManager.h"
#include "nsTraceRefcnt.h"

#include "nsHtml5AttributeName.h"
#include "nsHtml5ElementName.h"
#include "nsHtml5Portability.h"
#include "nsHtml5StackNode.h"
#include "nsHtml5StateSnapshot.h"
#include "nsHtml5Tokenizer.h"
#include "nsHtml5TreeBuilder.h"
#include "nsHtml5UTF16Buffer.h"

#include "nsHtml5HtmlAttributes.h"

nsHtml5HtmlAttributes* nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES = nullptr;

nsHtml5HtmlAttributes::nsHtml5HtmlAttributes(int32_t aMode) : mMode(aMode) {
  MOZ_COUNT_CTOR(nsHtml5HtmlAttributes);
}

nsHtml5HtmlAttributes::~nsHtml5HtmlAttributes() {
  MOZ_COUNT_DTOR(nsHtml5HtmlAttributes);
  clear(0);
}

int32_t nsHtml5HtmlAttributes::getIndex(nsHtml5AttributeName* aName) {
  for (size_t i = 0; i < mStorage.Length(); i++) {
    if (mStorage[i].GetLocal(nsHtml5AttributeName::HTML) ==
        aName->getLocal(nsHtml5AttributeName::HTML)) {
      // It's release asserted elsewhere that i can't be too large.
      return i;
    }
  }
  return -1;
}

nsHtml5String nsHtml5HtmlAttributes::getValue(nsHtml5AttributeName* aName) {
  int32_t index = getIndex(aName);
  if (index == -1) {
    return nullptr;
  } else {
    return getValueNoBoundsCheck(index);
  }
}

int32_t nsHtml5HtmlAttributes::getLength() { return mStorage.Length(); }

nsAtom* nsHtml5HtmlAttributes::getLocalNameNoBoundsCheck(int32_t aIndex) {
  MOZ_ASSERT(aIndex < int32_t(mStorage.Length()) && aIndex >= 0,
             "Index out of bounds");
  return mStorage[aIndex].GetLocal(mMode);
}

int32_t nsHtml5HtmlAttributes::getURINoBoundsCheck(int32_t aIndex) {
  MOZ_ASSERT(aIndex < int32_t(mStorage.Length()) && aIndex >= 0,
             "Index out of bounds");
  return mStorage[aIndex].GetUri(mMode);
}

nsAtom* nsHtml5HtmlAttributes::getPrefixNoBoundsCheck(int32_t aIndex) {
  MOZ_ASSERT(aIndex < int32_t(mStorage.Length()) && aIndex >= 0,
             "Index out of bounds");
  return mStorage[aIndex].GetPrefix(mMode);
}

nsHtml5String nsHtml5HtmlAttributes::getValueNoBoundsCheck(int32_t aIndex) {
  MOZ_ASSERT(aIndex < int32_t(mStorage.Length()) && aIndex >= 0,
             "Index out of bounds");
  return mStorage[aIndex].GetValue();
}

int32_t nsHtml5HtmlAttributes::getLineNoBoundsCheck(int32_t aIndex) {
  MOZ_ASSERT(aIndex < int32_t(mStorage.Length()) && aIndex >= 0,
             "Index out of bounds");
  return mStorage[aIndex].GetLine();
}

void nsHtml5HtmlAttributes::addAttribute(nsHtml5AttributeName* aName,
                                         nsHtml5String aValue, int32_t aLine) {
  mStorage.AppendElement(nsHtml5AttributeEntry(aName, aValue, aLine));
  MOZ_RELEASE_ASSERT(mStorage.Length() <= INT32_MAX,
                     "Can't handle this many attributes.");
}

// Isindex-only, so doesn't need to deal with SVG and MathML
void nsHtml5HtmlAttributes::AddAttributeWithLocal(nsAtom* aName,
                                                  nsHtml5String aValue,
                                                  int32_t aLine) {
  mStorage.AppendElement(nsHtml5AttributeEntry(aName, aValue, aLine));
  MOZ_RELEASE_ASSERT(mStorage.Length() <= INT32_MAX,
                     "Can't handle this many attributes.");
}

void nsHtml5HtmlAttributes::clear(int32_t aMode) {
  for (nsHtml5AttributeEntry& entry : mStorage) {
    entry.ReleaseValue();
  }
  mStorage.TruncateLength(0);
  mMode = aMode;
}

void nsHtml5HtmlAttributes::releaseValue(int32_t aIndex) {
  mStorage[aIndex].ReleaseValue();
}

void nsHtml5HtmlAttributes::clearWithoutReleasingContents() {
  mStorage.TruncateLength(0);
}

bool nsHtml5HtmlAttributes::contains(nsHtml5AttributeName* aName) {
  for (size_t i = 0; i < mStorage.Length(); i++) {
    if (mStorage[i].GetLocal(nsHtml5AttributeName::HTML) ==
        aName->getLocal(nsHtml5AttributeName::HTML)) {
      return true;
    }
  }
  return false;
}

void nsHtml5HtmlAttributes::adjustForMath() {
  mMode = nsHtml5AttributeName::MATHML;
}

void nsHtml5HtmlAttributes::adjustForSvg() {
  mMode = nsHtml5AttributeName::SVG;
}

nsHtml5HtmlAttributes* nsHtml5HtmlAttributes::cloneAttributes() {
  MOZ_ASSERT(mStorage.IsEmpty() || !mMode);
  nsHtml5HtmlAttributes* clone =
      new nsHtml5HtmlAttributes(nsHtml5AttributeName::HTML);
  for (nsHtml5AttributeEntry& entry : mStorage) {
    clone->AddEntry(entry.Clone());
  }
  return clone;
}

bool nsHtml5HtmlAttributes::equalsAnother(nsHtml5HtmlAttributes* aOther) {
  MOZ_ASSERT(!mMode, "Trying to compare attributes in foreign content.");
  if (mStorage.Length() != aOther->mStorage.Length()) {
    return false;
  }
  for (nsHtml5AttributeEntry& entry : mStorage) {
    bool found = false;
    nsAtom* ownLocal = entry.GetLocal(nsHtml5AttributeName::HTML);
    for (nsHtml5AttributeEntry& otherEntry : aOther->mStorage) {
      if (ownLocal == otherEntry.GetLocal(nsHtml5AttributeName::HTML)) {
        found = true;
        if (!entry.GetValue().Equals(otherEntry.GetValue())) {
          return false;
        }
        break;
      }
    }
    if (!found) {
      return false;
    }
  }
  return true;
}

void nsHtml5HtmlAttributes::AddEntry(nsHtml5AttributeEntry&& aEntry) {
  mStorage.AppendElement(aEntry);
}

void nsHtml5HtmlAttributes::initializeStatics() {
  EMPTY_ATTRIBUTES = new nsHtml5HtmlAttributes(nsHtml5AttributeName::HTML);
}

void nsHtml5HtmlAttributes::releaseStatics() { delete EMPTY_ATTRIBUTES; }
