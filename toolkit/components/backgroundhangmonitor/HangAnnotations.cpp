/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HangAnnotations.h"

#include <vector>

#include "MainThreadUtils.h"
#include "mozilla/DebugOnly.h"
#include "nsXULAppAPI.h"
#include "mozilla/BackgroundHangMonitor.h"

namespace mozilla {

void BackgroundHangAnnotations::AddAnnotation(const nsString& aName,
                                              const int32_t aData) {
  nsAutoString dataString;
  dataString.AppendInt(aData);
  AppendElement(HangAnnotation(aName, dataString));
}

void BackgroundHangAnnotations::AddAnnotation(const nsString& aName,
                                              const double aData) {
  nsAutoString dataString;
  dataString.AppendFloat(aData);
  AppendElement(HangAnnotation(aName, dataString));
}

void BackgroundHangAnnotations::AddAnnotation(const nsString& aName,
                                              const nsString& aData) {
  AppendElement(HangAnnotation(aName, aData));
}

void BackgroundHangAnnotations::AddAnnotation(const nsString& aName,
                                              const nsCString& aData) {
  NS_ConvertUTF8toUTF16 dataString(aData);
  AppendElement(HangAnnotation(aName, dataString));
}

void BackgroundHangAnnotations::AddAnnotation(const nsString& aName,
                                              const bool aData) {
  if (aData) {
    AppendElement(HangAnnotation(aName, u"true"_ns));
  } else {
    AppendElement(HangAnnotation(aName, u"false"_ns));
  }
}

BackgroundHangAnnotators::BackgroundHangAnnotators()
    : mMutex("BackgroundHangAnnotators::mMutex") {
  MOZ_COUNT_CTOR(BackgroundHangAnnotators);
}

BackgroundHangAnnotators::~BackgroundHangAnnotators() {
  MOZ_ASSERT(mAnnotators.empty());
  MOZ_COUNT_DTOR(BackgroundHangAnnotators);
}

bool BackgroundHangAnnotators::Register(BackgroundHangAnnotator& aAnnotator) {
  MutexAutoLock lock(mMutex);
  auto result = mAnnotators.insert(&aAnnotator);
  return result.second;
}

bool BackgroundHangAnnotators::Unregister(BackgroundHangAnnotator& aAnnotator) {
  MutexAutoLock lock(mMutex);
  DebugOnly<std::set<BackgroundHangAnnotator*>::size_type> numErased;
  numErased = mAnnotators.erase(&aAnnotator);
  MOZ_ASSERT(numErased == 1);
  return mAnnotators.empty();
}

BackgroundHangAnnotations BackgroundHangAnnotators::GatherAnnotations() {
  BackgroundHangAnnotations annotations;
  {  // Scope for lock
    MutexAutoLock lock(mMutex);
    for (std::set<BackgroundHangAnnotator*>::iterator i = mAnnotators.begin(),
                                                      e = mAnnotators.end();
         i != e; ++i) {
      (*i)->AnnotateHang(annotations);
    }
  }
  return annotations;
}

}  // namespace mozilla
