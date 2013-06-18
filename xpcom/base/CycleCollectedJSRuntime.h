/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CycleCollectedJSRuntime_h__
#define mozilla_CycleCollectedJSRuntime_h__

#include "jsapi.h"

#include "nsDataHashtable.h"
#include "nsHashKeys.h"

class nsScriptObjectTracer;

namespace mozilla {

class CycleCollectedJSRuntime
{
protected:
  CycleCollectedJSRuntime(uint32_t aMaxbytes,
                          JSUseHelperThreads aUseHelperThreads);
  virtual ~CycleCollectedJSRuntime();

  JSRuntime* Runtime() const
  {
    MOZ_ASSERT(mJSRuntime);
    return mJSRuntime;
  }

public:
  void AddJSHolder(void* aHolder, nsScriptObjectTracer* aTracer);
  void RemoveJSHolder(void* aHolder);
#ifdef DEBUG
  bool TestJSHolder(void* aHolder);
  void SetObjectToUnlink(void* aObject) { mObjectToUnlink = aObject; }
  void AssertNoObjectsToTrace(void* aPossibleJSHolder);
#endif

// XXXkhuey should be private
protected:
  nsDataHashtable<nsPtrHashKey<void>, nsScriptObjectTracer*> mJSHolders;

private:
  JSRuntime* mJSRuntime;

#ifdef DEBUG
  void* mObjectToUnlink;
#endif
};

} // namespace mozilla

#endif // mozilla_CycleCollectedJSRuntime_h__
