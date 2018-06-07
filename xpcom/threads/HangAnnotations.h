/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HangAnnotations_h
#define mozilla_HangAnnotations_h

#include <set>

#include "ipc/IPCMessageUtils.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"
#include "mozilla/Vector.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {
namespace HangMonitor {

/**
 * This type represents an individual hang annotation.
 */
class Annotation
{
public:
  Annotation() {}
  Annotation(const nsAString& aName, const nsAString& aValue)
    : mName(aName), mValue(aValue)
  {}

  nsString mName;
  nsString mValue;
};

/**
 * This class extends nsTArray<Annotation> with some methods for adding
 * annotations being reported by a registered hang Annotator.
 */
class HangAnnotations : public nsTArray<Annotation>
{
public:
  void AddAnnotation(const nsAString& aName, const int32_t aData);
  void AddAnnotation(const nsAString& aName, const double aData);
  void AddAnnotation(const nsAString& aName, const nsAString& aData);
  void AddAnnotation(const nsAString& aName, const nsACString& aData);
  void AddAnnotation(const nsAString& aName, const bool aData);
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
 * Gathers annotations. This function should be called by ChromeHangs.
 * @return HangAnnotations object.
 */
HangAnnotations ChromeHangAnnotatorCallout();

namespace Observer {

class Annotators
{
public:
  Annotators();
  ~Annotators();

  bool Register(Annotator& aAnnotator);
  bool Unregister(Annotator& aAnnotator);

  HangAnnotations GatherAnnotations();

private:
  Mutex                mMutex;
  std::set<Annotator*> mAnnotators;
};

} // namespace Observer

} // namespace HangMonitor
} // namespace mozilla

namespace IPC {

template<>
class ParamTraits<mozilla::HangMonitor::HangAnnotations>
  : public ParamTraits<nsTArray<mozilla::HangMonitor::Annotation>>
{
public:
  typedef mozilla::HangMonitor::HangAnnotations paramType;
};

template<>
class ParamTraits<mozilla::HangMonitor::Annotation>
{
public:
  typedef mozilla::HangMonitor::Annotation paramType;
  static void Write(Message* aMsg, const paramType& aParam);
  static bool Read(const Message* aMsg,
                   PickleIterator* aIter,
                   paramType* aResult);
};

} // namespace IPC

#endif // mozilla_HangAnnotations_h
