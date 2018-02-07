/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "CertAnnotator.h"

#include "mozilla/JSONWriter.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Services.h"
#include "mozilla/WinDllServices.h"
#include "nsExceptionHandler.h"
#include "nsIFile.h"
#include "nsIObserverService.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "nsXULAppAPI.h"
#include "nsWindowsHelpers.h"

#include <tlhelp32.h>

namespace mozilla {

NS_IMPL_ISUPPORTS(CertAnnotator, nsIObserver)

CertAnnotator::~CertAnnotator()
{
  if (mAnnotatorThread) {
    mAnnotatorThread->Shutdown();
  }
}

bool
CertAnnotator::Init()
{
  if (mAnnotatorThread) {
    return true;
  }

  nsCOMPtr<nsIRunnable> initialEvent =
    NewRunnableMethod("mozilla::CertAnnotator::RecordInitialCertInfo", this,
                      &CertAnnotator::RecordInitialCertInfo);

  nsresult rv = NS_NewNamedThread("Cert Annotator",
                                  getter_AddRefs(mAnnotatorThread),
                                  initialEvent);
  return NS_SUCCEEDED(rv);
}

NS_IMETHODIMP
CertAnnotator::Observe(nsISupports* aSubject, const char* aTopic,
                       const char16_t* aData)
{
  MOZ_ASSERT(!strcmp(aTopic, DllServices::kTopicDllLoadedMainThread) ||
             !strcmp(aTopic, DllServices::kTopicDllLoadedNonMainThread));
  MOZ_ASSERT(mAnnotatorThread);

  if (!mAnnotatorThread) {
    return NS_OK;
  }

  nsCOMPtr<nsIRunnable> event =
    NewRunnableMethod<nsString, bool>("mozilla::CertAnnotator::RecordCertInfo",
                                      this, &CertAnnotator::RecordCertInfo,
                                      aData, true);

  mAnnotatorThread->Dispatch(event, NS_DISPATCH_NORMAL);

  return NS_OK;
}

void
CertAnnotator::RecordInitialCertInfo()
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsAutoHandle snapshot(::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0));
  MOZ_ASSERT(snapshot != INVALID_HANDLE_VALUE);
  if (snapshot == INVALID_HANDLE_VALUE) {
    return;
  }

  MODULEENTRY32W moduleEntry = { sizeof(moduleEntry) };

  if (!::Module32FirstW(snapshot, &moduleEntry)) {
    return;
  }

  do {
    RecordCertInfo(nsDependentString(moduleEntry.szExePath), false);
  } while(::Module32NextW(snapshot, &moduleEntry));

  Serialize();
}

void
CertAnnotator::RecordCertInfo(const nsAString& aLibPath, const bool aDoSerialize)
{
  MOZ_ASSERT(!NS_IsMainThread());

  // (1) Get Lowercase Module Name
  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_NewLocalFile(aLibPath, false, getter_AddRefs(file));
  if (NS_FAILED(rv)) {
    return;
  }

  nsAutoString key;
  rv = file->GetLeafName(key);
  if (NS_FAILED(rv)) {
    return;
  }

  ToLowerCase(key);

  // (2) Get cert subject info
  auto flatLibPath = PromiseFlatString(aLibPath);

  RefPtr<mozilla::DllServices> dllSvc(mozilla::DllServices::Get());
  auto orgName = dllSvc->GetBinaryOrgName(flatLibPath.get());
  if (!orgName) {
    return;
  }

  // (3) Insert into hash table
  auto& modulesForThisSubject = mCertTable.GetOrInsert(nsString(orgName.get()));

  if (modulesForThisSubject.ContainsSorted(key)) {
    return;
  }

  modulesForThisSubject.InsertElementSorted(Move(key));

  if (!aDoSerialize) {
    return;
  }

  // (4) Serialize and annotate
  Serialize();
}

} // namespace mozilla

namespace {

class Writer final : public mozilla::JSONWriteFunc
{
public:
  virtual void Write(const char* aStr) override
  {
    mStr += aStr;
  }

  const nsCString& Get() const
  {
    return mStr;
  }

private:
  nsCString mStr;
};

} // anonymous namespace

namespace mozilla {

void
CertAnnotator::Serialize()
{
  MOZ_ASSERT(!NS_IsMainThread());

  JSONWriter json(MakeUnique<Writer>());

#if defined(DEBUG)
  const JSONWriter::CollectionStyle style = JSONWriter::MultiLineStyle;
#else
  const JSONWriter::CollectionStyle style = JSONWriter::SingleLineStyle;
#endif

  json.Start(style);

  for (auto subjectItr = mCertTable.Iter(); !subjectItr.Done(); subjectItr.Next()) {
    json.StartArrayProperty(NS_ConvertUTF16toUTF8(subjectItr.Key()).get());
    auto& modules = subjectItr.Data();
    for (auto&& module : modules) {
      json.StringElement(NS_ConvertUTF16toUTF8(module).get());
    }
    json.EndArray();
  }

  json.End();

  const nsCString& serialized = static_cast<Writer*>(json.WriteFunc())->Get();

  if (XRE_IsParentProcess()) {
    // Safe to do off main thread in the parent process
    Annotate(serialized);
    return;
  }

  nsCOMPtr<nsIRunnable> event =
    NewRunnableMethod<nsCString>("mozilla::CertAnnotator::Annotate", this,
                                 &CertAnnotator::Annotate, serialized);
  NS_DispatchToMainThread(event);
}

void
CertAnnotator::Annotate(const nsCString& aAnnotation)
{
  nsAutoCString annotationKey;
  annotationKey.AppendLiteral("ModuleSignatureInfo");
  if (XRE_IsParentProcess()) {
    annotationKey.AppendLiteral("Parent");
  } else {
    MOZ_ASSERT(NS_IsMainThread());
    annotationKey.AppendLiteral("Child");
  }

  CrashReporter::AnnotateCrashReport(annotationKey, aAnnotation);
}

void
CertAnnotator::Register()
{
  RefPtr<CertAnnotator> annotator(new CertAnnotator());
  if (!annotator->Init()) {
    return;
  }

  nsCOMPtr<nsIObserverService> obsServ(services::GetObserverService());
  obsServ->AddObserver(annotator, DllServices::kTopicDllLoadedMainThread,
                       false);
  obsServ->AddObserver(annotator, DllServices::kTopicDllLoadedNonMainThread,
                       false);
}

} // namespace mozilla
