/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5AttributeEntry_h
#define nsHtml5AttributeEntry_h

#include "nsHtml5AttributeName.h"

class nsHtml5AttributeEntry final {
 public:
  nsHtml5AttributeEntry(nsHtml5AttributeName* aName, nsHtml5String aValue,
                        int32_t aLine)
      : mLine(aLine), mValue(aValue) {
    // Let's hope the compiler coalesces the following as appropriate.
    mLocals[0] = aName->getLocal(0);
    mLocals[1] = aName->getLocal(1);
    mLocals[2] = aName->getLocal(2);
    mPrefixes[0] = aName->getPrefix(0);
    mPrefixes[1] = aName->getPrefix(1);
    mPrefixes[2] = aName->getPrefix(2);
    mUris[0] = aName->getUri(0);
    mUris[1] = aName->getUri(1);
    mUris[2] = aName->getUri(2);
  }

  // Isindex-only so doesn't need to deal with SVG and MathML
  nsHtml5AttributeEntry(nsAtom* aName, nsHtml5String aValue, int32_t aLine)
      : mLine(aLine), mValue(aValue) {
    // Let's hope the compiler coalesces the following as appropriate.
    mLocals[0] = aName;
    mLocals[1] = aName;
    mLocals[2] = aName;
    mPrefixes[0] = nullptr;
    mPrefixes[1] = nullptr;
    mPrefixes[2] = nullptr;
    mUris[0] = kNameSpaceID_None;
    mUris[1] = kNameSpaceID_None;
    mUris[2] = kNameSpaceID_None;
  }

  inline nsAtom* GetLocal(int32_t aMode) { return mLocals[aMode]; }

  inline nsAtom* GetPrefix(int32_t aMode) { return mPrefixes[aMode]; }

  inline int32_t GetUri(int32_t aMode) { return mUris[aMode]; }

  inline nsHtml5String GetValue() { return mValue; }

  inline int32_t GetLine() { return mLine; }

  inline void ReleaseValue() { mValue.Release(); }

  inline nsHtml5AttributeEntry Clone() {
    // Copy the memory
    nsHtml5AttributeEntry clone(*this);
    // Increment refcount for value
    clone.mValue = this->mValue.Clone();
    return clone;
  }

 private:
  RefPtr<nsAtom> mLocals[3];
  RefPtr<nsAtom> mPrefixes[3];
  int32_t mUris[3];
  int32_t mLine;
  nsHtml5String mValue;
};

#endif  // nsHtml5AttributeEntry_h
