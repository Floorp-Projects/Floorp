/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCycleCollectionParticipant.h"
#include "nsString.h"
#include "nsWrapperCache.h"
#include "jsapi.h"
#include "jsfriendapi.h"

void CycleCollectionNoteEdgeNameImpl(
    nsCycleCollectionTraversalCallback& aCallback, const char* aName,
    uint32_t aFlags) {
  nsAutoCString arrayEdgeName(aName);
  if (aFlags & CycleCollectionEdgeNameArrayFlag) {
    arrayEdgeName.AppendLiteral("[i]");
  }
  aCallback.NoteNextEdgeName(arrayEdgeName.get());
}

void nsCycleCollectionParticipant::NoteJSChild(JS::GCCellPtr aGCThing,
                                               const char* aName,
                                               void* aClosure) {
  nsCycleCollectionTraversalCallback* cb =
      static_cast<nsCycleCollectionTraversalCallback*>(aClosure);
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb, aName);
  if (JS::IsCCTraceKind(aGCThing.kind())) {
    cb->NoteJSChild(aGCThing);
  }
}

void TraceCallbackFunc::Trace(JS::Heap<JS::Value>* aPtr, const char* aName,
                              void* aClosure) const {
  if (aPtr->unbarrieredGet().isGCThing()) {
    mCallback(JS::GCCellPtr(aPtr->unbarrieredGet()), aName, aClosure);
  }
}

void TraceCallbackFunc::Trace(JS::Heap<jsid>* aPtr, const char* aName,
                              void* aClosure) const {
  if (aPtr->unbarrieredGet().isGCThing()) {
    mCallback(aPtr->unbarrieredGet().toGCCellPtr(), aName, aClosure);
  }
}

void TraceCallbackFunc::Trace(JS::Heap<JSObject*>* aPtr, const char* aName,
                              void* aClosure) const {
  if (*aPtr) {
    mCallback(JS::GCCellPtr(aPtr->unbarrieredGet()), aName, aClosure);
  }
}

void TraceCallbackFunc::Trace(nsWrapperCache* aPtr, const char* aName,
                              void* aClosure) const {
  JSObject* obj = aPtr->GetWrapperPreserveColor();
  if (obj) {
    mCallback(JS::GCCellPtr(obj), aName, aClosure);
  }
}

void TraceCallbackFunc::Trace(JS::TenuredHeap<JSObject*>* aPtr,
                              const char* aName, void* aClosure) const {
  if (*aPtr) {
    mCallback(JS::GCCellPtr(aPtr->unbarrieredGetPtr()), aName, aClosure);
  }
}

void TraceCallbackFunc::Trace(JS::Heap<JSFunction*>* aPtr, const char* aName,
                              void* aClosure) const {
  if (*aPtr) {
    mCallback(JS::GCCellPtr(aPtr->unbarrieredGet()), aName, aClosure);
  }
}

void TraceCallbackFunc::Trace(JS::Heap<JSString*>* aPtr, const char* aName,
                              void* aClosure) const {
  if (*aPtr) {
    mCallback(JS::GCCellPtr(aPtr->unbarrieredGet()), aName, aClosure);
  }
}

void TraceCallbackFunc::Trace(JS::Heap<JSScript*>* aPtr, const char* aName,
                              void* aClosure) const {
  if (*aPtr) {
    mCallback(JS::GCCellPtr(aPtr->unbarrieredGet()), aName, aClosure);
  }
}
