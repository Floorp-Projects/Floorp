/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WeakPointer.h"

#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/StaticMutex.h"
#include "jsapi.h"

#include <unordered_map>

namespace mozilla {
namespace recordreplay {

typedef std::unordered_map<const void*, UniquePtr<JS::PersistentRootedObject>> WeakPointerRootMap;
static WeakPointerRootMap* gWeakPointerRootMap;

static StaticMutexNotRecorded gWeakPointerMutex;

static UniquePtr<JS::PersistentRootedObject>
NewRoot(JSObject* aJSObj)
{
  MOZ_RELEASE_ASSERT(aJSObj);
  JSContext* cx = dom::danger::GetJSContext();
  UniquePtr<JS::PersistentRootedObject> root = MakeUnique<JS::PersistentRootedObject>(cx);
  *root = aJSObj;
  return root;
}

extern "C" {

MOZ_EXPORT void
RecordReplayInterface_SetWeakPointerJSRoot(const void* aPtr, JSObject* aJSObj)
{
  MOZ_RELEASE_ASSERT(IsReplaying());

  StaticMutexAutoLock lock(gWeakPointerMutex);

  auto iter = gWeakPointerRootMap->find(aPtr);
  if (iter != gWeakPointerRootMap->end()) {
    if (aJSObj) {
      *iter->second = aJSObj;
    } else {
      gWeakPointerRootMap->erase(aPtr);
    }
  } else if (aJSObj) {
    gWeakPointerRootMap->insert(WeakPointerRootMap::value_type(aPtr, NewRoot(aJSObj)));
  }
}

} // extern "C"

void
InitializeWeakPointers()
{
  gWeakPointerRootMap = new WeakPointerRootMap();
}

} // namespace recordreplay
} // namespace mozilla
