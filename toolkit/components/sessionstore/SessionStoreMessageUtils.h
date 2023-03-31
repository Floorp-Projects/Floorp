/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStoreMessageUtils_h
#define mozilla_dom_SessionStoreMessageUtils_h

#include "ipc/IPCMessageUtils.h"
#include "mozilla/ipc/IPDLParamTraits.h"
#include "mozilla/ipc/URIUtils.h"
#include "SessionStoreData.h"
#include "SessionStoreUtils.h"
#include "SessionStoreRestoreData.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::CollectedNonMultipleSelectValue> {
  typedef mozilla::dom::CollectedNonMultipleSelectValue paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mSelectedIndex);
    WriteParam(aWriter, aParam.mValue);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mSelectedIndex) &&
           ReadParam(aReader, &aResult->mValue);
  }
};

template <>
struct ParamTraits<CollectedInputDataValue> {
  typedef CollectedInputDataValue paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.id);
    WriteParam(aWriter, aParam.type);
    WriteParam(aWriter, aParam.value);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->id) &&
           ReadParam(aReader, &aResult->type) &&
           ReadParam(aReader, &aResult->value);
  }
};

template <>
struct ParamTraits<InputFormData> {
  typedef InputFormData paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.descendants);
    WriteParam(aWriter, aParam.innerHTML);
    WriteParam(aWriter, aParam.url);
    WriteParam(aWriter, aParam.numId);
    WriteParam(aWriter, aParam.numXPath);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->descendants) &&
           ReadParam(aReader, &aResult->innerHTML) &&
           ReadParam(aReader, &aResult->url) &&
           ReadParam(aReader, &aResult->numId) &&
           ReadParam(aReader, &aResult->numXPath);
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

  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    mozilla::dom::SessionStoreRestoreData* aParam) {
    bool isNull = !aParam;
    WriteIPDLParam(aWriter, aActor, isNull);
    if (isNull) {
      return;
    }
    WriteIPDLParam(aWriter, aActor, aParam->mURI);
    WriteIPDLParam(aWriter, aActor, aParam->mInnerHTML);
    WriteIPDLParam(aWriter, aActor, aParam->mScroll);
    WriteIPDLParam(aWriter, aActor, aParam->mEntries);
  }

  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   RefPtr<mozilla::dom::SessionStoreRestoreData>* aResult) {
    *aResult = nullptr;
    bool isNull;
    if (!ReadIPDLParam(aReader, aActor, &isNull)) {
      return false;
    }
    if (isNull) {
      return true;
    }
    auto data = MakeRefPtr<mozilla::dom::SessionStoreRestoreData>();
    if (!ReadIPDLParam(aReader, aActor, &data->mURI) ||
        !ReadIPDLParam(aReader, aActor, &data->mInnerHTML) ||
        !ReadIPDLParam(aReader, aActor, &data->mScroll) ||
        !ReadIPDLParam(aReader, aActor, &data->mEntries)) {
      return false;
    }
    *aResult = std::move(data);
    return true;
  }
};

template <>
struct IPDLParamTraits<mozilla::dom::SessionStoreRestoreData::Entry> {
  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    mozilla::dom::SessionStoreRestoreData::Entry aParam) {
    WriteIPDLParam(aWriter, aActor, aParam.mData);
    WriteIPDLParam(aWriter, aActor, aParam.mIsXPath);
  }

  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   mozilla::dom::SessionStoreRestoreData::Entry* aResult) {
    return ReadIPDLParam(aReader, aActor, &aResult->mData) &&
           ReadIPDLParam(aReader, aActor, &aResult->mIsXPath);
  }
};

}  // namespace ipc
}  // namespace mozilla

#endif  // mozilla_dom_SessionStoreMessageUtils_h
