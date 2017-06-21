/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStatusReporterManager.h"
#include "nsCOMPtr.h"
#include "nsDirectoryServiceDefs.h"
#include "nsArrayEnumerator.h"
#include "nsISimpleEnumerator.h"
#include "nsIFile.h"
#include "nsDumpUtils.h"
#include "nsIFileStreams.h"
#include "nsPrintfCString.h"

#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

#ifdef XP_UNIX
#define DO_STATUS_REPORT 1
#endif

#ifdef DO_STATUS_REPORT // {
namespace {

class DumpStatusInfoToTempDirRunnable : public mozilla::Runnable
{
public:
  DumpStatusInfoToTempDirRunnable()
  {
  }

  NS_IMETHOD Run() override
  {
    nsCOMPtr<nsIStatusReporterManager> mgr =
      do_GetService("@mozilla.org/status-reporter-manager;1");
    mgr->DumpReports();
    return NS_OK;
  }
};

void
doStatusReport(const nsCString& aInputStr)
{
  LOG("FifoWatcher(%s) dispatching status report runnable.", aInputStr.get());
  RefPtr<DumpStatusInfoToTempDirRunnable> runnable =
    new DumpStatusInfoToTempDirRunnable();
  NS_DispatchToMainThread(runnable);
}

} //anonymous namespace
#endif // DO_STATUS_REPORT }

static bool gStatusReportProgress = 0;
static int gNumReporters = 0;

nsresult
getStatus(nsACString& aDesc)
{
  if (!gStatusReportProgress) {
    aDesc.AssignLiteral("Init");
  } else {
    aDesc.AssignLiteral("Running: There are ");
    aDesc.AppendInt(gNumReporters);
    aDesc.AppendLiteral(" reporters");
  }
  return NS_OK;
}

NS_STATUS_REPORTER_IMPLEMENT(StatusReporter, "StatusReporter State", getStatus)

#define DUMP(o, s) \
  do { \
    const char* s2 = (s); \
    uint32_t dummy; \
    nsresult rvDump = (o)->Write((s2), strlen(s2), &dummy); \
    if (NS_WARN_IF(NS_FAILED(rvDump))) \
      return rvDump; \
  } while (0)

static nsresult
DumpReport(nsIFileOutputStream* aOStream, const nsCString& aProcess,
           const nsCString& aName, const nsCString& aDescription)
{
  if (aProcess.IsEmpty()) {
    int pid = getpid();
    nsPrintfCString pidStr("PID %u", pid);
    DUMP(aOStream, "\n  {\n  \"Process\": \"");
    DUMP(aOStream, pidStr.get());
  } else {
    DUMP(aOStream, "\n  {  \"Unknown Process\": \"");
  }

  DUMP(aOStream, "\",\n  \"Reporter name\": \"");
  DUMP(aOStream, aName.get());

  DUMP(aOStream, "\",\n  \"Status Description\": [\"");
  nsCString desc = aDescription;
  desc.ReplaceSubstring("|", "\",\"");
  DUMP(aOStream, desc.get());

  DUMP(aOStream, "\"]\n  }");

  return NS_OK;
}

/**
 ** nsStatusReporterManager implementation
 **/

NS_IMPL_ISUPPORTS(nsStatusReporterManager, nsIStatusReporterManager)

nsStatusReporterManager::nsStatusReporterManager()
{
}

nsStatusReporterManager::~nsStatusReporterManager()
{
}

NS_IMETHODIMP
nsStatusReporterManager::Init()
{
  RegisterReporter(new NS_STATUS_REPORTER_NAME(StatusReporter));
  gStatusReportProgress = 1;

#ifdef DO_STATUS_REPORT
  if (FifoWatcher::MaybeCreate()) {
    FifoWatcher* fw = FifoWatcher::GetSingleton();
    fw->RegisterCallback(NS_LITERAL_CSTRING("status report"), doStatusReport);
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsStatusReporterManager::DumpReports()
{
  static unsigned number = 1;
  nsresult rv;

  nsCString filename("status-reports-");
  filename.AppendInt((uint32_t)getpid());
  filename.Append('-');
  filename.AppendInt(number++);
  filename.AppendLiteral(".json");

  // Open a file in NS_OS_TEMP_DIR for writing.
  // The file is initialized as "incomplete-status-reports-pid-number.json" in the
  // begining, it will be rename as "status-reports-pid-number.json" in the end.
  nsCOMPtr<nsIFile> tmpFile;
  rv = nsDumpUtils::OpenTempFile(NS_LITERAL_CSTRING("incomplete-") +
                                 filename,
                                 getter_AddRefs(tmpFile),
                                 NS_LITERAL_CSTRING("status-reports"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIFileOutputStream> ostream =
    do_CreateInstance("@mozilla.org/network/file-output-stream;1");
  rv = ostream->Init(tmpFile, -1, -1, 0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  //Write the reports to the file

  DUMP(ostream, "{\n\"subject\":\"about:service reports\",\n");
  DUMP(ostream, "\"reporters\": [ ");

  nsCOMPtr<nsISimpleEnumerator> e;
  bool more, first = true;
  EnumerateReporters(getter_AddRefs(e));
  while (NS_SUCCEEDED(e->HasMoreElements(&more)) && more) {
    nsCOMPtr<nsISupports> supports;
    e->GetNext(getter_AddRefs(supports));
    nsCOMPtr<nsIStatusReporter> r = do_QueryInterface(supports);

    nsCString process;
    rv = r->GetProcess(process);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCString name;
    rv = r->GetName(name);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCString description;
    rv = r->GetDescription(description);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (first) {
      first = false;
    } else {
      DUMP(ostream, ",");
    }

    rv = DumpReport(ostream, process, name, description);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  DUMP(ostream, "\n]\n}\n");

  rv = ostream->Close();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Rename the status reports file
  nsCOMPtr<nsIFile> srFinalFile;
  rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(srFinalFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifdef ANDROID
  rv = srFinalFile->AppendNative(NS_LITERAL_CSTRING("status-reports"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
#endif

  rv = srFinalFile->AppendNative(filename);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = srFinalFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoString srActualFinalFilename;
  rv = srFinalFile->GetLeafName(srActualFinalFilename);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = tmpFile->MoveTo(/* directory */ nullptr, srActualFinalFilename);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsStatusReporterManager::EnumerateReporters(nsISimpleEnumerator** aResult)
{
  return NS_NewArrayEnumerator(aResult, mReporters);
}

NS_IMETHODIMP
nsStatusReporterManager::RegisterReporter(nsIStatusReporter* aReporter)
{
  if (mReporters.IndexOf(aReporter) != -1) {
    return NS_ERROR_FAILURE;
  }

  mReporters.AppendObject(aReporter);
  gNumReporters++;
  return NS_OK;
}

NS_IMETHODIMP
nsStatusReporterManager::UnregisterReporter(nsIStatusReporter* aReporter)
{
  if (!mReporters.RemoveObject(aReporter)) {
    return NS_ERROR_FAILURE;
  }
  gNumReporters--;
  return NS_OK;
}

nsresult
NS_RegisterStatusReporter(nsIStatusReporter* aReporter)
{
  nsCOMPtr<nsIStatusReporterManager> mgr =
    do_GetService("@mozilla.org/status-reporter-manager;1");
  if (!mgr) {
    return NS_ERROR_FAILURE;
  }
  return mgr->RegisterReporter(aReporter);
}

nsresult
NS_UnregisterStatusReporter(nsIStatusReporter* aReporter)
{
  nsCOMPtr<nsIStatusReporterManager> mgr =
    do_GetService("@mozilla.org/status-reporter-manager;1");
  if (!mgr) {
    return NS_ERROR_FAILURE;
  }
  return mgr->UnregisterReporter(aReporter);
}

nsresult
NS_DumpStatusReporter()
{
  nsCOMPtr<nsIStatusReporterManager> mgr =
    do_GetService("@mozilla.org/status-reporter-manager;1");
  if (!mgr) {
    return NS_ERROR_FAILURE;
  }
  return mgr->DumpReports();
}
