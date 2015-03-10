/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HangAnnotations_h
#define mozilla_HangAnnotations_h

#include <set>

#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"
#include "nsString.h"

namespace mozilla {
namespace HangMonitor {

/**
 * This class declares an abstraction for a data type that encapsulates all
 * of the annotations being reported by a registered hang Annotator.
 */
class HangAnnotations
{
public:
  virtual ~HangAnnotations() {}

  virtual void AddAnnotation(const nsAString& aName, const int32_t aData) = 0;
  virtual void AddAnnotation(const nsAString& aName, const double aData) = 0;
  virtual void AddAnnotation(const nsAString& aName, const nsAString& aData) = 0;
  virtual void AddAnnotation(const nsAString& aName, const nsACString& aData) = 0;
  virtual void AddAnnotation(const nsAString& aName, const bool aData) = 0;

  class Enumerator
  {
  public:
    virtual ~Enumerator() {}
    virtual bool Next(nsAString& aOutName, nsAString& aOutValue) = 0;
  };

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const = 0;
  virtual bool IsEmpty() const = 0;
  virtual UniquePtr<Enumerator> GetEnumerator() = 0;
};

typedef UniquePtr<HangAnnotations> HangAnnotationsPtr;
typedef Vector<HangAnnotationsPtr> HangAnnotationsVector;

class Annotator
{
public:
  /**
   * NB: This function is always called by the HangMonitor thread.
   *     Plan accordingly.
   */
  virtual void AnnotateHang(HangAnnotations& aAnnotations) = 0;
};

/**
 * Registers an Annotator to be called when a hang is detected.
 * @param aAnnotator Reference to an object that implements the
 * HangMonitor::Annotator interface.
 */
void RegisterAnnotator(Annotator& aAnnotator);

/**
 * Registers an Annotator that was previously registered via RegisterAnnotator.
 * @param aAnnotator Reference to an object that implements the
 * HangMonitor::Annotator interface.
 */
void UnregisterAnnotator(Annotator& aAnnotator);

/**
 * Gathers annotations. This function should be called by ChromeHangs.
 * @return UniquePtr to HangAnnotations object or nullptr if none.
 */
HangAnnotationsPtr ChromeHangAnnotatorCallout();

namespace Observer {

class Annotators
{
public:
  Annotators();
  ~Annotators();

  bool Register(Annotator& aAnnotator);
  bool Unregister(Annotator& aAnnotator);

  HangAnnotationsPtr GatherAnnotations();

private:
  Mutex                mMutex;
  std::set<Annotator*> mAnnotators;
};

} // namespace Observer

} // namespace HangMonitor
} // namespace mozilla

#endif // mozilla_HangAnnotations_h
