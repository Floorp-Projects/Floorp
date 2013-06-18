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

class nsCycleCollectionNoteRootCallback;
class nsScriptObjectTracer;

namespace mozilla {

class CycleCollectedJSRuntime
{
protected:
  CycleCollectedJSRuntime(uint32_t aMaxbytes,
                          JSUseHelperThreads aUseHelperThreads,
                          bool aExpectRootedGlobals);
  virtual ~CycleCollectedJSRuntime();

  JSRuntime* Runtime() const
  {
    MOZ_ASSERT(mJSRuntime);
    return mJSRuntime;
  }

  void MaybeTraceGlobals(JSTracer* aTracer) const;
  virtual void TraverseAdditionalNativeRoots(nsCycleCollectionNoteRootCallback& aCb) = 0;
private:
  void MaybeTraverseGlobals(nsCycleCollectionNoteRootCallback& aCb) const;

  void TraverseNativeRoots(nsCycleCollectionNoteRootCallback& aCb);

public:
  void AddJSHolder(void* aHolder, nsScriptObjectTracer* aTracer);
  void RemoveJSHolder(void* aHolder);
#ifdef DEBUG
  bool TestJSHolder(void* aHolder);
  void SetObjectToUnlink(void* aObject) { mObjectToUnlink = aObject; }
  void AssertNoObjectsToTrace(void* aPossibleJSHolder);
#endif

  // This returns the singleton nsCycleCollectionParticipant for JSContexts.
  static nsCycleCollectionParticipant *JSContextParticipant();

  bool NotifyLeaveMainThread() const;
  void NotifyEnterCycleCollectionThread() const;
  void NotifyLeaveCycleCollectionThread() const;
  void NotifyEnterMainThread() const;
  nsresult BeginCycleCollection(nsCycleCollectionNoteRootCallback &aCb);
  void FixWeakMappingGrayBits() const;
  bool NeedCollect() const;

// XXXkhuey should be private
protected:
  nsDataHashtable<nsPtrHashKey<void>, nsScriptObjectTracer*> mJSHolders;

private:
  JSRuntime* mJSRuntime;

#ifdef DEBUG
  void* mObjectToUnlink;
  bool mExpectUnrootedGlobals;
#endif
};

} // namespace mozilla

#endif // mozilla_CycleCollectedJSRuntime_h__
