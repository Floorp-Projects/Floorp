/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStoreMessageUtils_h
#define mozilla_dom_SessionStoreMessageUtils_h

#include "ipc/IPCMessageUtils.h"
#include "SessionStoreData.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::CollectedNonMultipleSelectValue> {
  typedef mozilla::dom::CollectedNonMultipleSelectValue paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mSelectedIndex);
    WriteParam(aMsg, aParam.mValue);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mSelectedIndex) &&
           ReadParam(aMsg, aIter, &aResult->mValue);
  }
};

template <>
struct ParamTraits<CollectedInputDataValue> {
  typedef CollectedInputDataValue paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.id);
    WriteParam(aMsg, aParam.type);
    WriteParam(aMsg, aParam.value);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->id) &&
           ReadParam(aMsg, aIter, &aResult->type) &&
           ReadParam(aMsg, aIter, &aResult->value);
  }
};

template <>
struct ParamTraits<InputFormData> {
  typedef InputFormData paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.descendants);
    WriteParam(aMsg, aParam.innerHTML);
    WriteParam(aMsg, aParam.url);
    WriteParam(aMsg, aParam.numId);
    WriteParam(aMsg, aParam.numXPath);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->descendants) &&
           ReadParam(aMsg, aIter, &aResult->innerHTML) &&
           ReadParam(aMsg, aIter, &aResult->url) &&
           ReadParam(aMsg, aIter, &aResult->numId) &&
           ReadParam(aMsg, aIter, &aResult->numXPath);
  }
};

}  // namespace IPC

#endif  // mozilla_dom_SessionStoreMessageUtils_h
