/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WidgetMessageUtils_h
#define mozilla_WidgetMessageUtils_h

#include "ipc/IPCMessageUtils.h"
#include "mozilla/LookAndFeel.h"
#include "nsIWidget.h"

namespace IPC {

template<>
struct ParamTraits<LookAndFeelInt>
{
  typedef LookAndFeelInt paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.id);
    WriteParam(aMsg, aParam.value);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    int32_t id, value;
    if (ReadParam(aMsg, aIter, &id) &&
        ReadParam(aMsg, aIter, &value)) {
      aResult->id = id;
      aResult->value = value;
      return true;
    }
    return false;
  }
};

template<>
struct ParamTraits<nsTransparencyMode> : public ContiguousEnumSerializerInclusive<nsTransparencyMode, eTransparencyOpaque, eTransparencyBorderlessGlass>
{ };

template<>
struct ParamTraits<nsCursor>
  : public ContiguousEnumSerializer<nsCursor, eCursor_standard, eCursorCount>
{
};

} // namespace IPC

#endif // WidgetMessageUtils_h
