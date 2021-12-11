/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStoreMessageUtils_h
#define mozilla_dom_SessionStoreMessageUtils_h

#include "ipc/IPCMessageUtils.h"
#include "SessionStoreData.h"
#include "SessionStoreUtils.h"
#include "SessionStoreRestoreData.h"

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

namespace mozilla {
namespace ipc {

template <>
struct IPDLParamTraits<mozilla::dom::SessionStoreRestoreData*> {
  // Note that we intentionally don't de/serialize mChildren here. The receiver
  // won't be doing anything with the children lists, and it avoids sending form
  // data for subframes to the content processes of their embedders.

  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    mozilla::dom::SessionStoreRestoreData* aParam) {
    bool isNull = !aParam;
    WriteIPDLParam(aMsg, aActor, isNull);
    if (isNull) {
      return;
    }
    WriteIPDLParam(aMsg, aActor, aParam->mURI);
    WriteIPDLParam(aMsg, aActor, aParam->mInnerHTML);
    WriteIPDLParam(aMsg, aActor, aParam->mScroll);
    WriteIPDLParam(aMsg, aActor, aParam->mEntries);
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor,
                   RefPtr<mozilla::dom::SessionStoreRestoreData>* aResult) {
    *aResult = nullptr;
    bool isNull;
    if (!ReadIPDLParam(aMsg, aIter, aActor, &isNull)) {
      return false;
    }
    if (isNull) {
      return true;
    }
    auto data = MakeRefPtr<mozilla::dom::SessionStoreRestoreData>();
    if (!ReadIPDLParam(aMsg, aIter, aActor, &data->mURI) ||
        !ReadIPDLParam(aMsg, aIter, aActor, &data->mInnerHTML) ||
        !ReadIPDLParam(aMsg, aIter, aActor, &data->mScroll) ||
        !ReadIPDLParam(aMsg, aIter, aActor, &data->mEntries)) {
      return false;
    }
    *aResult = std::move(data);
    return true;
  }
};

template <>
struct IPDLParamTraits<mozilla::dom::SessionStoreRestoreData::Entry> {
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    mozilla::dom::SessionStoreRestoreData::Entry aParam) {
    WriteIPDLParam(aMsg, aActor, aParam.mData);
    WriteIPDLParam(aMsg, aActor, aParam.mIsXPath);
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor,
                   mozilla::dom::SessionStoreRestoreData::Entry* aResult) {
    return ReadIPDLParam(aMsg, aIter, aActor, &aResult->mData) &&
           ReadIPDLParam(aMsg, aIter, aActor, &aResult->mIsXPath);
  }
};

}  // namespace ipc
}  // namespace mozilla

#endif  // mozilla_dom_SessionStoreMessageUtils_h
