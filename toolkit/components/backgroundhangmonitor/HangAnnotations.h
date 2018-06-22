/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HangAnnotations_h
#define mozilla_HangAnnotations_h

#include <set>

#include "mozilla/HangTypes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"
#include "mozilla/Vector.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/ipc/IPDLParamTraits.h"

namespace mozilla {

/**
 * This class extends nsTArray<HangAnnotation> with some methods for adding
 * annotations being reported by a registered hang Annotator.
 */
class BackgroundHangAnnotations : public nsTArray<HangAnnotation>
{
public:
  void AddAnnotation(const nsString& aName, const int32_t aData);
  void AddAnnotation(const nsString& aName, const double aData);
  void AddAnnotation(const nsString& aName, const nsString& aData);
  void AddAnnotation(const nsString& aName, const nsCString& aData);
  void AddAnnotation(const nsString& aName, const bool aData);
};

class BackgroundHangAnnotator
{
public:
  /**
   * NB: This function is always called by the BackgroundHangMonitor thread.
   *     Plan accordingly.
   */
  virtual void AnnotateHang(BackgroundHangAnnotations& aAnnotations) = 0;
};

class BackgroundHangAnnotators
{
public:
  BackgroundHangAnnotators();
  ~BackgroundHangAnnotators();

  bool Register(BackgroundHangAnnotator& aAnnotator);
  bool Unregister(BackgroundHangAnnotator& aAnnotator);

  BackgroundHangAnnotations GatherAnnotations();

private:
  Mutex mMutex;
  std::set<BackgroundHangAnnotator*> mAnnotators;
};

namespace ipc {

template<>
struct IPDLParamTraits<mozilla::BackgroundHangAnnotations>
  : public IPDLParamTraits<nsTArray<mozilla::HangAnnotation>>
{
  typedef mozilla::BackgroundHangAnnotations paramType;
};

} // namespace ipc

} // namespace mozilla

#endif // mozilla_HangAnnotations_h
