/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ContentAnalysisIPCTypes_h
#define mozilla_ContentAnalysisIPCTypes_h

#include "ipc/EnumSerializer.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "js/PropertyAndElement.h"
#include "mozilla/Variant.h"
#include "nsIContentAnalysis.h"

namespace mozilla {
namespace contentanalysis {

enum class NoContentAnalysisResult : uint8_t {
  AGENT_NOT_PRESENT,
  NO_PARENT_BROWSER,
  CANCELED,
  ERROR_INVALID_JSON_RESPONSE,
  ERROR_COULD_NOT_GET_DATA,
  ERROR_OTHER,
  LAST_VALUE
};

class ContentAnalysisResult : public nsIContentAnalysisResult {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICONTENTANALYSISRESULT
 public:
  static RefPtr<ContentAnalysisResult> FromAction(uint32_t aAction) {
    auto result =
        RefPtr<ContentAnalysisResult>(new ContentAnalysisResult(aAction));
    return result;
  }

  static RefPtr<ContentAnalysisResult> FromNoResult(
      NoContentAnalysisResult aResult) {
    auto result =
        RefPtr<ContentAnalysisResult>(new ContentAnalysisResult(aResult));
    return result;
  }

  static RefPtr<ContentAnalysisResult> FromJSONResponse(
      const JS::Handle<JS::Value>& aValue, JSContext* aCx) {
    if (aValue.isObject()) {
      auto* obj = aValue.toObjectOrNull();
      JS::Handle<JSObject*> handle =
          JS::Handle<JSObject*>::fromMarkedLocation(&obj);
      JS::Rooted<JS::Value> actionValue(aCx);
      if (JS_GetProperty(aCx, handle, "action", &actionValue)) {
        if (actionValue.isNumber()) {
          double actionNumber = actionValue.toNumber();
          return FromAction(static_cast<uint32_t>(actionNumber));
        }
      }
    }
    return FromNoResult(NoContentAnalysisResult::ERROR_INVALID_JSON_RESPONSE);
  }

  static RefPtr<ContentAnalysisResult> FromJSONContentAnalysisResponse(
      const JS::Handle<JS::Value>& aValue, JSContext* aCx) {
    if (aValue.isObject()) {
      auto* obj = aValue.toObjectOrNull();
      JS::Handle<JSObject*> handle =
          JS::Handle<JSObject*>::fromMarkedLocation(&obj);
      JS::Rooted<JS::Value> shouldAllowValue(aCx);
      if (JS_GetProperty(aCx, handle, "shouldAllowContent",
                         &shouldAllowValue)) {
        if (shouldAllowValue.isTrue()) {
          return FromAction(nsIContentAnalysisResponse::ALLOW);
        } else if (shouldAllowValue.isFalse()) {
          return FromAction(nsIContentAnalysisResponse::BLOCK);
        } else {
          return FromNoResult(NoContentAnalysisResult::ERROR_OTHER);
        }
      }
    }
    return FromNoResult(NoContentAnalysisResult::ERROR_INVALID_JSON_RESPONSE);
  }

 private:
  explicit ContentAnalysisResult(uint32_t aAction) : mValue(aAction) {}
  explicit ContentAnalysisResult(NoContentAnalysisResult aResult)
      : mValue(aResult) {}
  virtual ~ContentAnalysisResult() = default;
  Variant<uint32_t, NoContentAnalysisResult> mValue;
  friend struct IPC::ParamTraits<ContentAnalysisResult>;
  friend struct IPC::ParamTraits<ContentAnalysisResult*>;
  friend struct IPC::ParamTraits<RefPtr<ContentAnalysisResult>>;
};

}  // namespace contentanalysis
}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::contentanalysis::NoContentAnalysisResult>
    : public ContiguousEnumSerializer<
          mozilla::contentanalysis::NoContentAnalysisResult,
          static_cast<mozilla::contentanalysis::NoContentAnalysisResult>(0),
          mozilla::contentanalysis::NoContentAnalysisResult::LAST_VALUE> {};

template <>
struct ParamTraits<mozilla::contentanalysis::ContentAnalysisResult> {
  typedef mozilla::contentanalysis::ContentAnalysisResult paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mValue);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &(aResult->mValue));
  }
};

template <>
struct ParamTraits<mozilla::contentanalysis::ContentAnalysisResult*> {
  typedef mozilla::contentanalysis::ContentAnalysisResult paramType;

  static void Write(MessageWriter* aWriter, const paramType* aParam) {
    if (!aParam) {
      WriteParam(aWriter, true);
      return;
    }
    WriteParam(aWriter, false);
    WriteParam(aWriter, aParam->mValue);
  }

  static bool Read(MessageReader* aReader, RefPtr<paramType>* aResult) {
    bool isNull = false;
    if (!ReadParam(aReader, &isNull)) {
      return false;
    }
    if (isNull) {
      *aResult = nullptr;
      return true;
    }
    *aResult = mozilla::contentanalysis::ContentAnalysisResult::FromNoResult(
        mozilla::contentanalysis::NoContentAnalysisResult::ERROR_OTHER);
    return ReadParam(aReader, &((*aResult)->mValue));
  }
};

}  // namespace IPC

#endif  // mozilla_ContentAnalysisIPCTypes_h
