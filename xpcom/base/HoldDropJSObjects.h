/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HoldDropJSObjects_h
#define mozilla_HoldDropJSObjects_h

#include "nsCycleCollectionParticipant.h"

class nsISupports;
class nsScriptObjectTracer;

// Only HoldJSObjects and DropJSObjects should be called directly.

namespace mozilla {
namespace cyclecollector {

void HoldJSObjectsImpl(void* aHolder, nsScriptObjectTracer* aTracer,
                       JS::Zone* aZone = nullptr);
void HoldJSObjectsImpl(nsISupports* aHolder);
void DropJSObjectsImpl(void* aHolder);
void DropJSObjectsImpl(nsISupports* aHolder);

}  // namespace cyclecollector

template <class T, bool isISupports = std::is_base_of<nsISupports, T>::value>
struct HoldDropJSObjectsHelper {
  static void Hold(T* aHolder) {
    cyclecollector::HoldJSObjectsImpl(aHolder,
                                      NS_CYCLE_COLLECTION_PARTICIPANT(T));
  }
  static void Drop(T* aHolder) { cyclecollector::DropJSObjectsImpl(aHolder); }
};

template <class T>
struct HoldDropJSObjectsHelper<T, true> {
  static void Hold(T* aHolder) {
    cyclecollector::HoldJSObjectsImpl(ToSupports(aHolder));
  }
  static void Drop(T* aHolder) {
    cyclecollector::DropJSObjectsImpl(ToSupports(aHolder));
  }
};

template <class T>
void HoldJSObjects(T* aHolder) {
  HoldDropJSObjectsHelper<T>::Hold(aHolder);
}

template <class T>
void DropJSObjects(T* aHolder) {
  HoldDropJSObjectsHelper<T>::Drop(aHolder);
}

}  // namespace mozilla

#endif  // mozilla_HoldDropJSObjects_h
