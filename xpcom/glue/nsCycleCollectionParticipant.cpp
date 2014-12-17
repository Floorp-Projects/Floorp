/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCycleCollectionParticipant.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "nsCOMPtr.h"
#include "jsapi.h"
#include "jsfriendapi.h"

#ifdef MOZILLA_INTERNAL_API
#include "nsString.h"
#else
#include "nsStringAPI.h"
#endif

void
nsScriptObjectTracer::NoteJSChild(JS::GCCellPtr aGCThing, const char* aName,
                                  void* aClosure)
{
  nsCycleCollectionTraversalCallback* cb =
    static_cast<nsCycleCollectionTraversalCallback*>(aClosure);
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb, aName);
  if (aGCThing.isObject()) {
    cb->NoteJSObject(aGCThing.toObject());
  } else if (aGCThing.isScript()) {
    cb->NoteJSScript(aGCThing.toScript());
  } else {
    MOZ_ASSERT(!mozilla::AddToCCKind(aGCThing.kind()));
  }
}

NS_IMETHODIMP_(void)
nsXPCOMCycleCollectionParticipant::Root(void* aPtr)
{
  nsISupports* s = static_cast<nsISupports*>(aPtr);
  NS_ADDREF(s);
}

NS_IMETHODIMP_(void)
nsXPCOMCycleCollectionParticipant::Unroot(void* aPtr)
{
  nsISupports* s = static_cast<nsISupports*>(aPtr);
  NS_RELEASE(s);
}

// We define a default trace function because some participants don't need
// to trace anything, so it is okay for them not to define one.
NS_IMETHODIMP_(void)
nsXPCOMCycleCollectionParticipant::Trace(void* aPtr, const TraceCallbacks& aCb,
                                         void* aClosure)
{
}

bool
nsXPCOMCycleCollectionParticipant::CheckForRightISupports(nsISupports* aSupports)
{
  nsISupports* foo;
  aSupports->QueryInterface(NS_GET_IID(nsCycleCollectionISupports),
                            reinterpret_cast<void**>(&foo));
  return aSupports == foo;
}

void
CycleCollectionNoteEdgeNameImpl(nsCycleCollectionTraversalCallback& aCallback,
                                const char* aName,
                                uint32_t aFlags)
{
  nsAutoCString arrayEdgeName(aName);
  if (aFlags & CycleCollectionEdgeNameArrayFlag) {
    arrayEdgeName.AppendLiteral("[i]");
  }
  aCallback.NoteNextEdgeName(arrayEdgeName.get());
}

void
TraceCallbackFunc::Trace(JS::Heap<JS::Value>* aPtr, const char* aName,
                         void* aClosure) const
{
  if (aPtr->isMarkable()) {
    mCallback(JS::GCCellPtr(*aPtr), aName, aClosure);
  }
}

void
TraceCallbackFunc::Trace(JS::Heap<jsid>* aPtr, const char* aName,
                         void* aClosure) const
{
  if (JSID_IS_GCTHING(*aPtr)) {
    mCallback(JSID_TO_GCTHING(*aPtr), aName, aClosure);
  }
}

void
TraceCallbackFunc::Trace(JS::Heap<JSObject*>* aPtr, const char* aName,
                         void* aClosure) const
{
  mCallback(JS::GCCellPtr(*aPtr), aName, aClosure);
}

void
TraceCallbackFunc::Trace(JS::TenuredHeap<JSObject*>* aPtr, const char* aName,
                         void* aClosure) const
{
  mCallback(JS::GCCellPtr(*aPtr), aName, aClosure);
}

void
TraceCallbackFunc::Trace(JS::Heap<JSFunction*>* aPtr, const char* aName,
                         void* aClosure) const
{
  mCallback(JS::GCCellPtr(*aPtr), aName, aClosure);
}

void
TraceCallbackFunc::Trace(JS::Heap<JSString*>* aPtr, const char* aName,
                         void* aClosure) const
{
  mCallback(JS::GCCellPtr(*aPtr), aName, aClosure);
}

void
TraceCallbackFunc::Trace(JS::Heap<JSScript*>* aPtr, const char* aName,
                         void* aClosure) const
{
  mCallback(JS::GCCellPtr(*aPtr), aName, aClosure);
}
