/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WidgetMessageUtils_h
#define mozilla_WidgetMessageUtils_h

#include "ipc/IPCMessageUtils.h"
#include "mozilla/LookAndFeel.h"
#include "nsIWidget.h"

namespace IPC {

template <>
struct ParamTraits<LookAndFeelInt> {
  typedef LookAndFeelInt paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, static_cast<int32_t>(aParam.id));
    WriteParam(aMsg, aParam.value);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    int32_t id, value;
    if (ReadParam(aMsg, aIter, &id) && ReadParam(aMsg, aIter, &value)) {
      aResult->id = static_cast<mozilla::LookAndFeel::IntID>(id);
      aResult->value = value;
      return true;
    }
    return false;
  }
};

template <>
struct ParamTraits<LookAndFeelFont> {
  typedef LookAndFeelFont paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.haveFont);
    WriteParam(aMsg, aParam.fontName);
    WriteParam(aMsg, aParam.pixelHeight);
    WriteParam(aMsg, aParam.italic);
    WriteParam(aMsg, aParam.bold);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->haveFont) &&
           ReadParam(aMsg, aIter, &aResult->fontName) &&
           ReadParam(aMsg, aIter, &aResult->pixelHeight) &&
           ReadParam(aMsg, aIter, &aResult->italic) &&
           ReadParam(aMsg, aIter, &aResult->bold);
  }
};

template <>
struct ParamTraits<LookAndFeelColor> {
  using paramType = LookAndFeelColor;
  using idType = std::underlying_type<mozilla::LookAndFeel::ColorID>::type;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, static_cast<idType>(aParam.id));
    WriteParam(aMsg, aParam.color);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    idType id;
    nscolor color;
    if (ReadParam(aMsg, aIter, &id) && ReadParam(aMsg, aIter, &color)) {
      aResult->id = static_cast<mozilla::LookAndFeel::ColorID>(id);
      aResult->color = color;
      return true;
    }
    return false;
  }
};

template <>
struct ParamTraits<LookAndFeelCache> {
  typedef LookAndFeelCache paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mInts);
    WriteParam(aMsg, aParam.mFonts);
    WriteParam(aMsg, aParam.mColors);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mInts) &&
           ReadParam(aMsg, aIter, &aResult->mFonts) &&
           ReadParam(aMsg, aIter, &aResult->mColors);
  }
};

template <>
struct ParamTraits<nsTransparencyMode>
    : public ContiguousEnumSerializerInclusive<nsTransparencyMode,
                                               eTransparencyOpaque,
                                               eTransparencyBorderlessGlass> {};

template <>
struct ParamTraits<nsCursor>
    : public ContiguousEnumSerializer<nsCursor, eCursor_standard,
                                      eCursorCount> {};

}  // namespace IPC

#endif  // WidgetMessageUtils_h
