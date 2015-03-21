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

namespace mozilla {
namespace HangMonitor {

// Chrome hang annotators. This can go away once BHR has completely replaced
// ChromeHangs.
static StaticAutoPtr<Observer::Annotators> gChromehangAnnotators;

class BrowserHangAnnotations : public HangAnnotations
{
public:
  BrowserHangAnnotations();
  ~BrowserHangAnnotations();

  void AddAnnotation(const nsAString& aName, const int32_t aData) override;
  void AddAnnotation(const nsAString& aName, const double aData) override;
  void AddAnnotation(const nsAString& aName, const nsAString& aData) override;
  void AddAnnotation(const nsAString& aName, const nsACString& aData) override;
  void AddAnnotation(const nsAString& aName, const bool aData) override;

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;
  bool IsEmpty() const override;
  UniquePtr<Enumerator> GetEnumerator() override;

  typedef std::pair<nsString, nsString> AnnotationType;
  typedef std::vector<AnnotationType> VectorType;
  typedef VectorType::const_iterator IteratorType;

private:
  VectorType  mAnnotations;
};

BrowserHangAnnotations::BrowserHangAnnotations()
{
  MOZ_COUNT_CTOR(BrowserHangAnnotations);
}

BrowserHangAnnotations::~BrowserHangAnnotations()
{
  MOZ_COUNT_DTOR(BrowserHangAnnotations);
}

void
BrowserHangAnnotations::AddAnnotation(const nsAString& aName, const int32_t aData)
{
  nsString dataString;
  dataString.AppendInt(aData);
  AnnotationType annotation = std::make_pair(nsString(aName), dataString);
  mAnnotations.push_back(annotation);
}

void
BrowserHangAnnotations::AddAnnotation(const nsAString& aName, const double aData)
{
  nsString dataString;
  dataString.AppendFloat(aData);
  AnnotationType annotation = std::make_pair(nsString(aName), dataString);
  mAnnotations.push_back(annotation);
}

void
BrowserHangAnnotations::AddAnnotation(const nsAString& aName, const nsAString& aData)
{
  AnnotationType annotation = std::make_pair(nsString(aName), nsString(aData));
  mAnnotations.push_back(annotation);
}

void
BrowserHangAnnotations::AddAnnotation(const nsAString& aName, const nsACString& aData)
{
  nsString dataString;
  AppendUTF8toUTF16(aData, dataString);
  AnnotationType annotation = std::make_pair(nsString(aName), dataString);
  mAnnotations.push_back(annotation);
}

void
BrowserHangAnnotations::AddAnnotation(const nsAString& aName, const bool aData)
{
  nsString dataString;
  dataString += aData ? NS_LITERAL_STRING("true") : NS_LITERAL_STRING("false");
  AnnotationType annotation = std::make_pair(nsString(aName), dataString);
  mAnnotations.push_back(annotation);
}

/**
 * This class itself does not use synchronization but it (and its parent object)
 * should be protected by mutual exclusion in some way. In Telemetry the chrome
 * hang data is protected via TelemetryImpl::mHangReportsMutex.
 */
class ChromeHangAnnotationEnumerator : public HangAnnotations::Enumerator
{
public:
  explicit ChromeHangAnnotationEnumerator(const BrowserHangAnnotations::VectorType& aAnnotations);
  ~ChromeHangAnnotationEnumerator();

  virtual bool Next(nsAString& aOutName, nsAString& aOutValue);

private:
  BrowserHangAnnotations::IteratorType mIterator;
  BrowserHangAnnotations::IteratorType mEnd;
};

ChromeHangAnnotationEnumerator::ChromeHangAnnotationEnumerator(
                          const BrowserHangAnnotations::VectorType& aAnnotations)
  : mIterator(aAnnotations.begin())
  , mEnd(aAnnotations.end())
{
  MOZ_COUNT_CTOR(ChromeHangAnnotationEnumerator);
}

ChromeHangAnnotationEnumerator::~ChromeHangAnnotationEnumerator()
{
  MOZ_COUNT_DTOR(ChromeHangAnnotationEnumerator);
}

bool
ChromeHangAnnotationEnumerator::Next(nsAString& aOutName, nsAString& aOutValue)
{
  aOutName.Truncate();
  aOutValue.Truncate();
  if (mIterator == mEnd) {
    return false;
  }
  aOutName = mIterator->first;
  aOutValue = mIterator->second;
  ++mIterator;
  return true;
}

bool
BrowserHangAnnotations::IsEmpty() const
{
  return mAnnotations.empty();
}

size_t
BrowserHangAnnotations::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t result = sizeof(mAnnotations) +
                  mAnnotations.capacity() * sizeof(AnnotationType);
  for (IteratorType i = mAnnotations.begin(), e = mAnnotations.end(); i != e;
       ++i) {
    result += i->first.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    result += i->second.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

  return result;
}

UniquePtr<HangAnnotations::Enumerator>
BrowserHangAnnotations::GetEnumerator()
{
  if (mAnnotations.empty()) {
    return nullptr;
  }
  return MakeUnique<ChromeHangAnnotationEnumerator>(mAnnotations);
}

namespace Observer {

Annotators::Annotators()
  : mMutex("HangMonitor::Annotators::mMutex")
{
  MOZ_COUNT_CTOR(Annotators);
}

Annotators::~Annotators()
{
  MOZ_ASSERT(mAnnotators.empty());
  MOZ_COUNT_DTOR(Annotators);
}

bool
Annotators::Register(Annotator& aAnnotator)
{
  MutexAutoLock lock(mMutex);
  auto result = mAnnotators.insert(&aAnnotator);
  return result.second;
}

bool
Annotators::Unregister(Annotator& aAnnotator)
{
  MutexAutoLock lock(mMutex);
  DebugOnly<std::set<Annotator*>::size_type> numErased;
  numErased = mAnnotators.erase(&aAnnotator);
  MOZ_ASSERT(numErased == 1);
  return mAnnotators.empty();
}

UniquePtr<HangAnnotations>
Annotators::GatherAnnotations()
{
  auto annotations = MakeUnique<BrowserHangAnnotations>();
  { // Scope for lock
    MutexAutoLock lock(mMutex);
    for (std::set<Annotator*>::iterator i = mAnnotators.begin(),
                                        e = mAnnotators.end();
         i != e; ++i) {
      (*i)->AnnotateHang(*annotations);
    }
  }
  if (annotations->IsEmpty()) {
    return nullptr;
  }
  return Move(annotations);
}

} // namespace Observer

void
RegisterAnnotator(Annotator& aAnnotator)
{
  BackgroundHangMonitor::RegisterAnnotator(aAnnotator);
  // We still register annotators for ChromeHangs
  if (NS_IsMainThread() &&
      GeckoProcessType_Default == XRE_GetProcessType()) {
    if (!gChromehangAnnotators) {
      gChromehangAnnotators = new Observer::Annotators();
    }
    gChromehangAnnotators->Register(aAnnotator);
  }
}

void
UnregisterAnnotator(Annotator& aAnnotator)
{
  BackgroundHangMonitor::UnregisterAnnotator(aAnnotator);
  // We still register annotators for ChromeHangs
  if (NS_IsMainThread() &&
      GeckoProcessType_Default == XRE_GetProcessType()) {
    if (gChromehangAnnotators->Unregister(aAnnotator)) {
      gChromehangAnnotators = nullptr;
    }
  }
}

UniquePtr<HangAnnotations>
ChromeHangAnnotatorCallout()
{
  if (!gChromehangAnnotators) {
    return nullptr;
  }
  return gChromehangAnnotators->GatherAnnotations();
}

} // namespace HangMonitor
} // namespace mozilla
