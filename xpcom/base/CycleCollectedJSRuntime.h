/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CycleCollectedJSRuntime_h__
#define mozilla_CycleCollectedJSRuntime_h__

#include "jsprvtd.h"
#include "jsapi.h"

#include "nsCycleCollectionParticipant.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"

class nsCycleCollectionNoteRootCallback;
class nsScriptObjectTracer;

namespace mozilla {

class JSGCThingParticipant: public nsCycleCollectionParticipant
{
public:
  static NS_METHOD RootImpl(void *n)
  {
    return NS_OK;
  }

  static NS_METHOD UnlinkImpl(void *n)
  {
    return NS_OK;
  }

  static NS_METHOD UnrootImpl(void *n)
  {
    return NS_OK;
  }

  static NS_METHOD_(void) UnmarkIfPurpleImpl(void *n)
  {
  }

  static NS_METHOD TraverseImpl(JSGCThingParticipant *that, void *n,
                                nsCycleCollectionTraversalCallback &cb);
};

class JSZoneParticipant : public nsCycleCollectionParticipant
{
public:

  static NS_METHOD RootImpl(void *p)
  {
    return NS_OK;
  }

  static NS_METHOD UnlinkImpl(void *p)
  {
    return NS_OK;
  }

  static NS_METHOD UnrootImpl(void *p)
  {
    return NS_OK;
  }

  static NS_METHOD_(void) UnmarkIfPurpleImpl(void *n)
  {
  }

  static NS_METHOD TraverseImpl(JSZoneParticipant *that, void *p,
                                nsCycleCollectionTraversalCallback &cb);
};

class CycleCollectedJSRuntime
{
  friend class JSGCThingParticipant;
  friend class JSZoneParticipant;
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

  virtual void TraverseAdditionalNativeRoots(nsCycleCollectionNoteRootCallback& aCb) = 0;
  virtual void TraceAdditionalNativeRoots(JSTracer* aTracer) = 0;

private:

  void
  DescribeGCThing(bool aIsMarked, void* aThing, JSGCTraceKind aTraceKind,
                  nsCycleCollectionTraversalCallback& aCb) const;

  virtual bool
  DescribeCustomObjects(JSObject* aObject, js::Class* aClasp,
                        char (&aName)[72]) const = 0;

  void
  NoteGCThingJSChildren(void* aThing, JSGCTraceKind aTraceKind,
                        nsCycleCollectionTraversalCallback& aCb) const;

  void
  NoteGCThingXPCOMChildren(js::Class* aClasp, JSObject* aObj,
                           nsCycleCollectionTraversalCallback& aCb) const;

  virtual bool
  NoteCustomGCThingXPCOMChildren(js::Class* aClasp, JSObject* aObj,
                                 nsCycleCollectionTraversalCallback& aCb) const = 0;


  enum TraverseSelect {
      TRAVERSE_CPP,
      TRAVERSE_FULL
  };

  void
  TraverseGCThing(TraverseSelect aTs, void* aThing,
                  JSGCTraceKind aTraceKind,
                  nsCycleCollectionTraversalCallback& aCb);

  void
  TraverseZone(JS::Zone* aZone, nsCycleCollectionTraversalCallback& aCb);

  static void
  TraverseObjectShim(void* aData, void* aThing);

  void MaybeTraverseGlobals(nsCycleCollectionNoteRootCallback& aCb) const;

  void TraverseNativeRoots(nsCycleCollectionNoteRootCallback& aCb);

  void MaybeTraceGlobals(JSTracer* aTracer) const;

  static void TraceGrayJS(JSTracer* aTracer, void* aData);

  void TraceNativeRoots(JSTracer* aTracer);

public:
  void AddJSHolder(void* aHolder, nsScriptObjectTracer* aTracer);
  void RemoveJSHolder(void* aHolder);
#ifdef DEBUG
  bool TestJSHolder(void* aHolder);
  void SetObjectToUnlink(void* aObject) { mObjectToUnlink = aObject; }
  void AssertNoObjectsToTrace(void* aPossibleJSHolder);
#endif

  // This returns the singleton nsCycleCollectionParticipant for JSContexts.
  static nsCycleCollectionParticipant* JSContextParticipant();

  nsCycleCollectionParticipant* GCThingParticipant() const;
  nsCycleCollectionParticipant* ZoneParticipant() const;

  bool NotifyLeaveMainThread() const;
  void NotifyEnterCycleCollectionThread() const;
  void NotifyLeaveCycleCollectionThread() const;
  void NotifyEnterMainThread() const;
  nsresult BeginCycleCollection(nsCycleCollectionNoteRootCallback &aCb);
  bool UsefulToMergeZones() const;
  void FixWeakMappingGrayBits() const;
  bool NeedCollect() const;
  void Collect(uint32_t reason) const;

// XXXkhuey should be private
protected:
  nsDataHashtable<nsPtrHashKey<void>, nsScriptObjectTracer*> mJSHolders;

private:
  typedef const CCParticipantVTable<JSGCThingParticipant>::Type GCThingParticipantVTable;
  const GCThingParticipantVTable mGCThingCycleCollectorGlobal;

  typedef const CCParticipantVTable<JSZoneParticipant>::Type JSZoneParticipantVTable;
  const JSZoneParticipantVTable mJSZoneCycleCollectorGlobal;

  JSRuntime* mJSRuntime;

#ifdef DEBUG
  void* mObjectToUnlink;
  bool mExpectUnrootedGlobals;
#endif
};

} // namespace mozilla

#endif // mozilla_CycleCollectedJSRuntime_h__
