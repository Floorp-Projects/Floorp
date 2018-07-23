/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_DeferredFinalize_h
#define mozilla_DeferredFinalize_h

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
                      DeferredFinalizeFunction aFunc,
                      void* aThing);

void DeferredFinalize(nsISupports* aSupports);

// When recording or replaying, deferred finalizers are forced to run at the
// same point during replay that they ran at while recording, even if there is
// a JSObject associated with the reference which has not been collected yet
// (since at this point the JSObject has been collected during the recording,
// that JSObject will never be used again and its reference can be released).
//
// This requires that RecordReplayRegisterDeferredFinalizeThing() be called for
// every thing which DeferredFinalize() will be called for at a later time.
// Calls to these functions must be 1:1. When not recording or replaying, this
// function is a no-op.
void RecordReplayRegisterDeferredFinalizeThing(DeferredFinalizeAppendFunction aAppendFunc,
                                               DeferredFinalizeFunction aFunc,
                                               void* aThing);

} // namespace mozilla

#endif // mozilla_DeferredFinalize_h
