/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfileAdditionalInformation.h"

#include "jsapi.h"
#include "js/JSON.h"
#include "js/PropertyAndElement.h"
#include "js/Value.h"
#include "mozilla/JSONStringWriteFuncs.h"
#include "mozilla/ipc/IPDLParamTraits.h"

#ifdef MOZ_GECKO_PROFILER
#  include "platform.h"

void mozilla::ProfileGenerationAdditionalInformation::ToJSValue(
    JSContext* aCx, JS::MutableHandle<JS::Value> aRetVal) const {
  // Get the shared libraries array.
  JS::Rooted<JS::Value> sharedLibrariesVal(aCx);
  {
    JSONStringWriteFunc<nsCString> buffer;
    JSONWriter w(buffer, JSONWriter::SingleLineStyle);
    w.StartArrayElement();
    AppendSharedLibraries(w, mSharedLibraries);
    w.EndArray();
    NS_ConvertUTF8toUTF16 buffer16(buffer.StringCRef());
    MOZ_ALWAYS_TRUE(JS_ParseJSON(aCx,
                                 static_cast<const char16_t*>(buffer16.get()),
                                 buffer16.Length(), &sharedLibrariesVal));
  }

  JS::Rooted<JSObject*> additionalInfoObj(aCx, JS_NewPlainObject(aCx));
  JS_SetProperty(aCx, additionalInfoObj, "sharedLibraries", sharedLibrariesVal);
  aRetVal.setObject(*additionalInfoObj);
}
#endif  // MOZ_GECKO_PROFILER

namespace IPC {

void IPC::ParamTraits<SharedLibrary>::Write(MessageWriter* aWriter,
                                            const paramType& aParam) {
  WriteParam(aWriter, aParam.mStart);
  WriteParam(aWriter, aParam.mEnd);
  WriteParam(aWriter, aParam.mOffset);
  WriteParam(aWriter, aParam.mBreakpadId);
  WriteParam(aWriter, aParam.mCodeId);
  WriteParam(aWriter, aParam.mModuleName);
  WriteParam(aWriter, aParam.mModulePath);
  WriteParam(aWriter, aParam.mDebugName);
  WriteParam(aWriter, aParam.mDebugPath);
  WriteParam(aWriter, aParam.mVersion);
  WriteParam(aWriter, aParam.mArch);
}

bool IPC::ParamTraits<SharedLibrary>::Read(MessageReader* aReader,
                                           paramType* aResult) {
  return ReadParam(aReader, &aResult->mStart) &&
         ReadParam(aReader, &aResult->mEnd) &&
         ReadParam(aReader, &aResult->mOffset) &&
         ReadParam(aReader, &aResult->mBreakpadId) &&
         ReadParam(aReader, &aResult->mCodeId) &&
         ReadParam(aReader, &aResult->mModuleName) &&
         ReadParam(aReader, &aResult->mModulePath) &&
         ReadParam(aReader, &aResult->mDebugName) &&
         ReadParam(aReader, &aResult->mDebugPath) &&
         ReadParam(aReader, &aResult->mVersion) &&
         ReadParam(aReader, &aResult->mArch);
}

void IPC::ParamTraits<SharedLibraryInfo>::Write(MessageWriter* aWriter,
                                                const paramType& aParam) {
  paramType& p = const_cast<paramType&>(aParam);
  WriteParam(aWriter, p.mEntries);
}

bool IPC::ParamTraits<SharedLibraryInfo>::Read(MessageReader* aReader,
                                               paramType* aResult) {
  return ReadParam(aReader, &aResult->mEntries);
}

void IPC::ParamTraits<mozilla::ProfileGenerationAdditionalInformation>::Write(
    MessageWriter* aWriter, const paramType& aParam) {
  WriteParam(aWriter, aParam.mSharedLibraries);
}

bool IPC::ParamTraits<mozilla::ProfileGenerationAdditionalInformation>::Read(
    MessageReader* aReader, paramType* aResult) {
  return ReadParam(aReader, &aResult->mSharedLibraries);
}

}  // namespace IPC
