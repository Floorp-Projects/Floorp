/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef UntrustedModulesDataSerializer_h__
#define UntrustedModulesDataSerializer_h__

#include "js/TypeDecls.h"
#include "mozilla/UntrustedModulesData.h"
#include "mozilla/Vector.h"
#include "nsTHashMap.h"
#include "UntrustedModulesBackupService.h"

namespace mozilla {
namespace Telemetry {

// This class owns a JS object and serializes a given UntrustedModulesData
// into it.  Because this class uses JSAPI, an AutoJSAPI instance must be
// on the stack before instanciating the class.
class MOZ_RAII UntrustedModulesDataSerializer final {
  using IndexMap = nsTHashMap<nsStringHashKey, uint32_t>;

  nsresult mCtorResult;
  JSContext* mCx;
  JS::Rooted<JSObject*> mMainObj;
  JS::Rooted<JSObject*> mModulesArray;
  JS::Rooted<JSObject*> mPerProcObjContainer;

  IndexMap mIndexMap;
  const uint32_t mMaxModulesArrayLen;
  uint32_t mCurModulesArrayIdx;

  // Combinations of the flags defined under nsITelemetry.
  // (See "Flags for getUntrustedModuleLoadEvents" in nsITelemetry.idl)
  const uint32_t mFlags;

  static bool SerializeEvent(
      JSContext* aCx, JS::MutableHandle<JS::Value> aElement,
      const ProcessedModuleLoadEventContainer& aEventContainer,
      const IndexMap& aModuleIndices);
  nsresult GetPerProcObject(const UntrustedModulesData& aData,
                            JS::MutableHandle<JSObject*> aObj);
  nsresult AddLoadEvents(const UntrustedModuleLoadingEvents& aEvents,
                         JS::MutableHandle<JSObject*> aPerProcObj);
  nsresult AddSingleData(const UntrustedModulesData& aData);

 public:
  UntrustedModulesDataSerializer(JSContext* aCx, uint32_t aMaxModulesArrayLen,
                                 uint32_t aFlags = 0);
  explicit operator bool() const;

  /**
   * Retrieves the JS object.
   *
   * @param  aRet  [out] This gets assigned to the newly created object.
   */
  void GetObject(JS::MutableHandle<JS::Value> aRet);

  /**
   * Adds data to the JS object.
   *
   * When the process of a given UntrustedModulesData collides with a key in
   * the JS object, the entire UntrustedModulesData instance in the JS object
   * will be replaced unless EXCLUDE_STACKINFO_FROM_LOADEVENTS is set.
   * When EXCLUDE_STACKINFO_FROM_LOADEVENTS is set, the given loading events
   * are appended into the JS object, keeping the existing data as is.
   *
   * @param  aData [in] The source objects to add.
   * @return nsresult
   */
  nsresult Add(const UntrustedModulesBackupData& aData);
};

}  // namespace Telemetry
}  // namespace mozilla

#endif  // UntrustedModulesDataSerializer_h__
