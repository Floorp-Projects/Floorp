/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HangMonitor_h
#define mozilla_HangMonitor_h

#include "mozilla/MemoryReporting.h"
#include "nsString.h"

namespace mozilla {
namespace HangMonitor {

/**
 * Signifies the type of activity in question
*/
enum ActivityType
{
  /* There is activity and it is known to be UI related activity. */
  kUIActivity,

  /* There is non UI activity and no UI activity is pending */
  kActivityNoUIAVail,

  /* There is non UI activity and UI activity is known to be pending */
  kActivityUIAVail,

  /* There is non UI activity and UI activity pending is unknown */
  kGeneralActivity
};

/**
 * Start monitoring hangs. Should be called by the XPCOM startup process only.
 */
void Startup();

/**
 * Stop monitoring hangs and join the thread.
 */
void Shutdown();

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
  virtual bool GetEnumerator(Enumerator **aOutEnum) = 0;
};

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
 * Notify the hang monitor of activity which will reset its internal timer.
 *
 * @param activityType The type of activity being reported.
 * @see ActivityType
 */
void NotifyActivity(ActivityType activityType = kGeneralActivity);

/*
 * Notify the hang monitor that the browser is now idle and no detection should
 * be done.
 */
void Suspend();

} // namespace HangMonitor
} // namespace mozilla

#endif // mozilla_HangMonitor_h
