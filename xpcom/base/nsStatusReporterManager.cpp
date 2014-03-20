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

#if defined(XP_LINUX) || defined(__FreeBSD__)
#define DO_STATUS_REPORT 1
#else
#define DO_STATUS_REPORT 0
#endif

#if DO_STATUS_REPORT // {
namespace {

class DumpStatusInfoToTempDirRunnable : public nsRunnable
{
public:
  DumpStatusInfoToTempDirRunnable()
  {}

  NS_IMETHOD Run()
  {
    nsCOMPtr<nsIStatusReporterManager> mgr =
      do_GetService("@mozilla.org/status-reporter-manager;1");
    mgr->DumpReports();
    return NS_OK;
  }
};

void doStatusReport(const nsCString& inputStr)
{
  LOG("FifoWatcher(%s) dispatching status report runnable.", inputStr.get());
  nsRefPtr<DumpStatusInfoToTempDirRunnable> runnable =
    new DumpStatusInfoToTempDirRunnable();
  NS_DispatchToMainThread(runnable);
}

} //anonymous namespace
#endif // DO_STATUS_REPORT }

static bool gStatusReportProgress = 0;
static int gNumReporters = 0;

nsresult getStatus(nsACString& desc)
{
  if(!gStatusReportProgress)
    desc.AssignLiteral("Init");
  else {
    desc.AssignLiteral("Running:\nThere are ");
    desc.AppendInt(gNumReporters);
    desc.AppendLiteral(" reporters");
  }
  return NS_OK;
}

NS_STATUS_REPORTER_IMPLEMENT(StatusReporter, "StatusReporter State", getStatus)

#define DUMP(o, s) \
    do { \
        const char* s2 = (s); \
        uint32_t dummy; \
        nsresult rv = (o)->Write((s2), strlen(s2), &dummy); \
        if (NS_WARN_IF(NS_FAILED(rv))) \
          return rv; \
    } while (0)

static nsresult
DumpReport(nsIFileOutputStream* aOStream, const nsCString& aProcess,
           const nsCString& aName, const nsCString& aDescription)
{
  int pid;
  if (aProcess.IsEmpty()) {
    pid = getpid();
    nsPrintfCString pidStr("PID %u", pid);
    DUMP(aOStream, "\n    {\"Process\": \"");
    DUMP(aOStream, pidStr.get());
  } else {
    pid = 0;
    DUMP(aOStream, "\n    {\"Unknown Process\": \"");
  }

  DUMP(aOStream, "\", \"Reporter name\": \"");
  DUMP(aOStream, aName.get());

  DUMP(aOStream, "\", \"Status Description\": \"");
  DUMP(aOStream, aDescription.get());
  DUMP(aOStream, "\"}");

  return NS_OK;
}

/**
 ** nsStatusReporterManager implementation
 **/

NS_IMPL_ISUPPORTS1(nsStatusReporterManager, nsIStatusReporterManager)

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

#if DO_STATUS_REPORT
  if (FifoWatcher::MaybeCreate()) {
    FifoWatcher* fw = FifoWatcher::GetSingleton();
    fw->RegisterCallback(NS_LITERAL_CSTRING("status report"),doStatusReport);
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
  filename.AppendInt(getpid());
  filename.AppendLiteral("-");
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
  if (NS_WARN_IF(NS_FAILED(rv)))
    return rv;

  nsCOMPtr<nsIFileOutputStream> ostream =
    do_CreateInstance("@mozilla.org/network/file-output-stream;1");
  rv = ostream->Init(tmpFile, -1, -1, 0);
  if (NS_WARN_IF(NS_FAILED(rv)))
    return rv;

  //Write the reports to the file

  DUMP(ostream, "  [Sysdump Report Start]: ");

  nsCOMPtr<nsISimpleEnumerator> e;
  bool more;
  EnumerateReporters(getter_AddRefs(e));
  while (NS_SUCCEEDED(e->HasMoreElements(&more)) && more) {
    nsCOMPtr<nsISupports> supports;
    e->GetNext(getter_AddRefs(supports));
    nsCOMPtr<nsIStatusReporter> r = do_QueryInterface(supports);

    nsCString process;
    rv = r->GetProcess(process);
    if (NS_WARN_IF(NS_FAILED(rv)))
      return rv;

    nsCString name;
    rv = r->GetName(name);
    if (NS_WARN_IF(NS_FAILED(rv)))
      return rv;

    nsCString description;
    rv = r->GetDescription(description);
    if (NS_WARN_IF(NS_FAILED(rv)))
      return rv;

    rv = DumpReport(ostream, process, name, description);
    if (NS_WARN_IF(NS_FAILED(rv)))
      return rv;
  }

  DUMP(ostream, "\n  [Sysdump Report End] ");

  rv = ostream->Close();
  if (NS_WARN_IF(NS_FAILED(rv)))
    return rv;

  // Rename the status reports file
  nsCOMPtr<nsIFile> srFinalFile;
  rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(srFinalFile));
  if (NS_WARN_IF(NS_FAILED(rv)))
    return rv;

#ifdef ANDROID
  rv = srFinalFile->AppendNative(NS_LITERAL_CSTRING("status-reports"));
  if (NS_WARN_IF(NS_FAILED(rv)))
    return rv;
#endif

  rv = srFinalFile->AppendNative(filename);
  if (NS_WARN_IF(NS_FAILED(rv)))
    return rv;

  rv = srFinalFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
  if (NS_WARN_IF(NS_FAILED(rv)))
    return rv;

  nsAutoString srActualFinalFilename;
  rv = srFinalFile->GetLeafName(srActualFinalFilename);
  if (NS_WARN_IF(NS_FAILED(rv)))
    return rv;

  rv = tmpFile->MoveTo(/* directory */ nullptr, srActualFinalFilename);

  if (NS_WARN_IF(NS_FAILED(rv)))
    return rv;

  return NS_OK;
}

NS_IMETHODIMP
nsStatusReporterManager::EnumerateReporters(nsISimpleEnumerator** result)
{
  return NS_NewArrayEnumerator(result, mReporters);
}

NS_IMETHODIMP
nsStatusReporterManager::RegisterReporter(nsIStatusReporter* reporter)
{
  if (mReporters.IndexOf(reporter) != -1)
    return NS_ERROR_FAILURE;

  mReporters.AppendObject(reporter);
  gNumReporters++;
  return NS_OK;
}

NS_IMETHODIMP
nsStatusReporterManager::UnregisterReporter(nsIStatusReporter* reporter)
{
  if (!mReporters.RemoveObject(reporter))
    return NS_ERROR_FAILURE;
  gNumReporters--;
  return NS_OK;
}

nsresult
NS_RegisterStatusReporter (nsIStatusReporter* reporter)
{
  nsCOMPtr<nsIStatusReporterManager> mgr =
    do_GetService("@mozilla.org/status-reporter-manager;1");
  if (mgr == nullptr)
    return NS_ERROR_FAILURE;
  return mgr->RegisterReporter(reporter);
}

nsresult
NS_UnregisterStatusReporter (nsIStatusReporter* reporter)
{
  nsCOMPtr<nsIStatusReporterManager> mgr =
    do_GetService("@mozilla.org/status-reporter-manager;1");
  if (mgr == nullptr)
    return NS_ERROR_FAILURE;
  return mgr->UnregisterReporter(reporter);
}

nsresult
NS_DumpStatusReporter ()
{
  nsCOMPtr<nsIStatusReporterManager> mgr =
    do_GetService("@mozilla.org/status-reporter-manager;1");
  if (mgr == nullptr)
    return NS_ERROR_FAILURE;
  return mgr->DumpReports();
}
