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
namespace HangMonitor {

// Chrome hang annotators. This can go away once BHR has completely replaced
// ChromeHangs.
static StaticAutoPtr<Observer::Annotators> gChromehangAnnotators;

void
HangAnnotations::AddAnnotation(const nsAString& aName, const int32_t aData)
{
  nsAutoString dataString;
  dataString.AppendInt(aData);
  AppendElement(Annotation(aName, dataString));
}

void
HangAnnotations::AddAnnotation(const nsAString& aName, const double aData)
{
  nsAutoString dataString;
  dataString.AppendFloat(aData);
  AppendElement(Annotation(aName, dataString));
}

void
HangAnnotations::AddAnnotation(const nsAString& aName, const nsAString& aData)
{
  AppendElement(Annotation(aName, aData));
}

void
HangAnnotations::AddAnnotation(const nsAString& aName, const nsACString& aData)
{
  NS_ConvertUTF8toUTF16 dataString(aData);
  AppendElement(Annotation(aName, dataString));
}

void
HangAnnotations::AddAnnotation(const nsAString& aName, const bool aData)
{
  if (aData) {
    AppendElement(Annotation(aName, NS_LITERAL_STRING("true")));
  } else {
    AppendElement(Annotation(aName, NS_LITERAL_STRING("false")));
  }
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

HangAnnotations
Annotators::GatherAnnotations()
{
  HangAnnotations annotations;
  { // Scope for lock
    MutexAutoLock lock(mMutex);
    for (std::set<Annotator*>::iterator i = mAnnotators.begin(),
                                        e = mAnnotators.end();
         i != e; ++i) {
      (*i)->AnnotateHang(annotations);
    }
  }
  return annotations;
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

HangAnnotations
ChromeHangAnnotatorCallout()
{
  if (!gChromehangAnnotators) {
    return HangAnnotations();
  }
  return gChromehangAnnotators->GatherAnnotations();
}

} // namespace HangMonitor
} // namespace mozilla

namespace IPC {

using mozilla::HangMonitor::Annotation;

void
ParamTraits<Annotation>::Write(Message* aMsg, const Annotation& aParam)
{
  WriteParam(aMsg, aParam.mName);
  WriteParam(aMsg, aParam.mValue);
}

bool
ParamTraits<Annotation>::Read(const Message* aMsg,
                              PickleIterator* aIter,
                              Annotation* aResult)
{
  if (!ReadParam(aMsg, aIter, &aResult->mName)) {
    return false;
  }
  if (!ReadParam(aMsg, aIter, &aResult->mValue)) {
    return false;
  }
  return true;
}

} // namespace IPC
