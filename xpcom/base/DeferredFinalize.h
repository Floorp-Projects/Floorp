/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_DeferredFinalize_h
#define mozilla_DeferredFinalize_h

#include <cstdint>

class nsISupports;

namespace mozilla {

// Called back from DeferredFinalize.  Should add 'thing' to the array of smart
// pointers in 'pointers', creating the array if 'pointers' is null, and return
// the array.
typedef void* (*DeferredFinalizeAppendFunction)(void* aPointers, void* aThing);

// Called to finalize a number of objects. Slice is the number of objects to
// finalize. The return value indicates whether it finalized all objects in the
// buffer. If it returns true, the function will not be called again, so the
// function should free aData.
typedef bool (*DeferredFinalizeFunction)(uint32_t aSlice, void* aData);

void DeferredFinalize(DeferredFinalizeAppendFunction aAppendFunc,
                      DeferredFinalizeFunction aFunc, void* aThing);

MOZ_NEVER_INLINE void DeferredFinalize(nsISupports* aSupports);

}  // namespace mozilla

#endif  // mozilla_DeferredFinalize_h
