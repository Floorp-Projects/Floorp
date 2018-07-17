/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/JSONWriter.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/nsMemoryInfoDumper.h"
#include "mozilla/DebugOnly.h"
#include "nsDumpUtils.h"

#include "mozilla/Unused.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentChild.h"
#include "nsIConsoleService.h"
#include "nsCycleCollector.h"
#include "nsICycleCollectorListener.h"
#include "nsIMemoryReporter.h"
#include "nsDirectoryServiceDefs.h"
#include "nsGZFileWriter.h"
#include "nsJSEnvironment.h"
#include "nsPrintfCString.h"
#include "nsISimpleEnumerator.h"
#include "nsServiceManagerUtils.h"
#include "nsIFile.h"

#ifdef XP_WIN
#include <process.h>
#ifndef getpid
#define getpid _getpid
#endif
#else
#include <unistd.h>
#endif

#ifdef XP_UNIX
#define MOZ_SUPPORTS_FIFO 1
#endif

// Some Android devices seem to send RT signals to Firefox so we want to avoid
// consuming those as they're not user triggered.
#if !defined(ANDROID) && (defined(XP_LINUX) || defined(__FreeBSD__))
#define MOZ_SUPPORTS_RT_SIGNALS 1
#endif

#if defined(MOZ_SUPPORTS_RT_SIGNALS)
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#if defined(MOZ_SUPPORTS_FIFO)
#include "mozilla/Preferences.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

namespace {

class DumpMemoryInfoToTempDirRunnable : public Runnable
{
public:
  DumpMemoryInfoToTempDirRunnable(const nsAString& aIdentifier,
                                  bool aAnonymize,
                                  bool aMinimizeMemoryUsage)
    : mozilla::Runnable("DumpMemoryInfoToTempDirRunnable")
    , mIdentifier(aIdentifier)
    , mAnonymize(aAnonymize)
    , mMinimizeMemoryUsage(aMinimizeMemoryUsage)
  {
  }

  NS_IMETHOD Run() override
  {
    nsCOMPtr<nsIMemoryInfoDumper> dumper =
      do_GetService("@mozilla.org/memory-info-dumper;1");
    dumper->DumpMemoryInfoToTempDir(mIdentifier, mAnonymize,
                                    mMinimizeMemoryUsage);
    return NS_OK;
  }

private:
  const nsString mIdentifier;
  const bool mAnonymize;
  const bool mMinimizeMemoryUsage;
};

class GCAndCCLogDumpRunnable final
  : public Runnable
  , public nsIDumpGCAndCCLogsCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  GCAndCCLogDumpRunnable(const nsAString& aIdentifier,
                         bool aDumpAllTraces,
                         bool aDumpChildProcesses)
    : mozilla::Runnable("GCAndCCLogDumpRunnable")
    , mIdentifier(aIdentifier)
    , mDumpAllTraces(aDumpAllTraces)
    , mDumpChildProcesses(aDumpChildProcesses)
  {
  }

  NS_IMETHOD Run() override
  {
    nsCOMPtr<nsIMemoryInfoDumper> dumper =
      do_GetService("@mozilla.org/memory-info-dumper;1");

    dumper->DumpGCAndCCLogsToFile(mIdentifier, mDumpAllTraces,
                                  mDumpChildProcesses, this);
    return NS_OK;
  }

  NS_IMETHOD OnDump(nsIFile* aGCLog, nsIFile* aCCLog, bool aIsParent) override
  {
    return NS_OK;
  }

  NS_IMETHOD OnFinish() override
  {
    return NS_OK;
  }

private:
  ~GCAndCCLogDumpRunnable() {}

  const nsString mIdentifier;
  const bool mDumpAllTraces;
  const bool mDumpChildProcesses;
};

NS_IMPL_ISUPPORTS_INHERITED(GCAndCCLogDumpRunnable, Runnable,
                            nsIDumpGCAndCCLogsCallback)

} // namespace

#if defined(MOZ_SUPPORTS_RT_SIGNALS) // {
namespace {

/*
 * The following code supports dumping about:memory upon receiving a signal.
 *
 * We listen for the following signals:
 *
 *  - SIGRTMIN:     Dump our memory reporters (and those of our child
 *                  processes),
 *  - SIGRTMIN + 1: Dump our memory reporters (and those of our child
 *                  processes) after minimizing memory usage, and
 *  - SIGRTMIN + 2: Dump the GC and CC logs in this and our child processes.
 *
 * When we receive one of these signals, we write the signal number to a pipe.
 * The IO thread then notices that the pipe has been written to, and kicks off
 * the appropriate task on the main thread.
 *
 * This scheme is similar to using signalfd(), except it's portable and it
 * doesn't require the use of sigprocmask, which is problematic because it
 * masks signals received by child processes.
 *
 * In theory, we could use Chromium's MessageLoopForIO::CatchSignal() for this.
 * But that uses libevent, which does not handle the realtime signals (bug
 * 794074).
 */

// It turns out that at least on some systems, SIGRTMIN is not a compile-time
// constant, so these have to be set at runtime.
static uint8_t sDumpAboutMemorySignum;         // SIGRTMIN
static uint8_t sDumpAboutMemoryAfterMMUSignum; // SIGRTMIN + 1
static uint8_t sGCAndCCDumpSignum;             // SIGRTMIN + 2

void doMemoryReport(const uint8_t aRecvSig)
{
  // Dump our memory reports (but run this on the main thread!).
  bool minimize = aRecvSig == sDumpAboutMemoryAfterMMUSignum;
  LOG("SignalWatcher(sig %d) dispatching memory report runnable.", aRecvSig);
  RefPtr<DumpMemoryInfoToTempDirRunnable> runnable =
    new DumpMemoryInfoToTempDirRunnable(/* identifier = */ EmptyString(),
                                        /* anonymize = */ false,
                                        minimize);
  NS_DispatchToMainThread(runnable);
}

void doGCCCDump(const uint8_t aRecvSig)
{
  LOG("SignalWatcher(sig %d) dispatching GC/CC log runnable.", aRecvSig);
  // Dump GC and CC logs (from the main thread).
  RefPtr<GCAndCCLogDumpRunnable> runnable =
    new GCAndCCLogDumpRunnable(/* identifier = */ EmptyString(),
                               /* allTraces = */ true,
                               /* dumpChildProcesses = */ true);
  NS_DispatchToMainThread(runnable);
}

} // namespace
#endif // MOZ_SUPPORTS_RT_SIGNALS }

#if defined(MOZ_SUPPORTS_FIFO) // {
namespace {

void
doMemoryReport(const nsCString& aInputStr)
{
  bool minimize = aInputStr.EqualsLiteral("minimize memory report");
  LOG("FifoWatcher(command:%s) dispatching memory report runnable.",
      aInputStr.get());
  RefPtr<DumpMemoryInfoToTempDirRunnable> runnable =
    new DumpMemoryInfoToTempDirRunnable(/* identifier = */ EmptyString(),
                                        /* anonymize = */ false,
                                        minimize);
  NS_DispatchToMainThread(runnable);
}

void
doGCCCDump(const nsCString& aInputStr)
{
  bool doAllTracesGCCCDump = aInputStr.EqualsLiteral("gc log");
  LOG("FifoWatcher(command:%s) dispatching GC/CC log runnable.", aInputStr.get());
  RefPtr<GCAndCCLogDumpRunnable> runnable =
    new GCAndCCLogDumpRunnable(/* identifier = */ EmptyString(),
                               doAllTracesGCCCDump,
                               /* dumpChildProcesses = */ true);
  NS_DispatchToMainThread(runnable);
}

bool
SetupFifo()
{
#ifdef DEBUG
  static bool fifoCallbacksRegistered = false;
#endif

  if (!FifoWatcher::MaybeCreate()) {
    return false;
  }

  MOZ_ASSERT(!fifoCallbacksRegistered,
             "FifoWatcher callbacks should be registered only once");

  FifoWatcher* fw = FifoWatcher::GetSingleton();
  // Dump our memory reports (but run this on the main thread!).
  fw->RegisterCallback(NS_LITERAL_CSTRING("memory report"),
                       doMemoryReport);
  fw->RegisterCallback(NS_LITERAL_CSTRING("minimize memory report"),
                       doMemoryReport);
  // Dump GC and CC logs (from the main thread).
  fw->RegisterCallback(NS_LITERAL_CSTRING("gc log"),
                       doGCCCDump);
  fw->RegisterCallback(NS_LITERAL_CSTRING("abbreviated gc log"),
                       doGCCCDump);

#ifdef DEBUG
  fifoCallbacksRegistered = true;
#endif
  return true;
}

void
OnFifoEnabledChange(const char* /*unused*/, void* /*unused*/)
{
  LOG("%s changed", FifoWatcher::kPrefName);
  if (SetupFifo()) {
    Preferences::UnregisterCallback(OnFifoEnabledChange,
                                    FifoWatcher::kPrefName);
  }
}

} // namespace
#endif // MOZ_SUPPORTS_FIFO }

NS_IMPL_ISUPPORTS(nsMemoryInfoDumper, nsIMemoryInfoDumper)

nsMemoryInfoDumper::nsMemoryInfoDumper()
{
}

nsMemoryInfoDumper::~nsMemoryInfoDumper()
{
}

/* static */ void
nsMemoryInfoDumper::Initialize()
{
#if defined(MOZ_SUPPORTS_RT_SIGNALS)
  SignalPipeWatcher* sw = SignalPipeWatcher::GetSingleton();

  // Dump memory reporters (and those of our child processes)
  sDumpAboutMemorySignum = SIGRTMIN;
  sw->RegisterCallback(sDumpAboutMemorySignum, doMemoryReport);
  // Dump our memory reporters after minimizing memory usage
  sDumpAboutMemoryAfterMMUSignum = SIGRTMIN + 1;
  sw->RegisterCallback(sDumpAboutMemoryAfterMMUSignum, doMemoryReport);
  // Dump the GC and CC logs in this and our child processes.
  sGCAndCCDumpSignum = SIGRTMIN + 2;
  sw->RegisterCallback(sGCAndCCDumpSignum, doGCCCDump);
#endif

#if defined(MOZ_SUPPORTS_FIFO)
  if (!SetupFifo()) {
    // NB: This gets loaded early enough that it's possible there is a user pref
    //     set to enable the fifo watcher that has not been loaded yet. Register
    //     to attempt to initialize if the fifo watcher becomes enabled by
    //     a user pref.
    Preferences::RegisterCallback(OnFifoEnabledChange,
                                  FifoWatcher::kPrefName);
  }
#endif
}

static void
EnsureNonEmptyIdentifier(nsAString& aIdentifier)
{
  if (!aIdentifier.IsEmpty()) {
    return;
  }

  // If the identifier is empty, set it to the number of whole seconds since the
  // epoch.  This identifier will appear in the files that this process
  // generates and also the files generated by this process's children, allowing
  // us to identify which files are from the same memory report request.
  aIdentifier.AppendInt(static_cast<int64_t>(PR_Now()) / 1000000);
}

// Use XPCOM refcounting to fire |onFinish| when all reference-holders
// (remote dump actors or the |DumpGCAndCCLogsToFile| activation itself)
// have gone away.
class nsDumpGCAndCCLogsCallbackHolder final
  : public nsIDumpGCAndCCLogsCallback
{
public:
  NS_DECL_ISUPPORTS

  explicit nsDumpGCAndCCLogsCallbackHolder(nsIDumpGCAndCCLogsCallback* aCallback)
    : mCallback(aCallback)
  {
  }

  NS_IMETHOD OnFinish() override
  {
    return NS_ERROR_UNEXPECTED;
  }

  NS_IMETHOD OnDump(nsIFile* aGCLog, nsIFile* aCCLog, bool aIsParent) override
  {
    return mCallback->OnDump(aGCLog, aCCLog, aIsParent);
  }

private:
  ~nsDumpGCAndCCLogsCallbackHolder()
  {
    Unused << mCallback->OnFinish();
  }

  nsCOMPtr<nsIDumpGCAndCCLogsCallback> mCallback;
};

NS_IMPL_ISUPPORTS(nsDumpGCAndCCLogsCallbackHolder, nsIDumpGCAndCCLogsCallback)

NS_IMETHODIMP
nsMemoryInfoDumper::DumpGCAndCCLogsToFile(const nsAString& aIdentifier,
                                          bool aDumpAllTraces,
                                          bool aDumpChildProcesses,
                                          nsIDumpGCAndCCLogsCallback* aCallback)
{
  nsString identifier(aIdentifier);
  EnsureNonEmptyIdentifier(identifier);
  nsCOMPtr<nsIDumpGCAndCCLogsCallback> callbackHolder =
    new nsDumpGCAndCCLogsCallbackHolder(aCallback);

  if (aDumpChildProcesses) {
    nsTArray<ContentParent*> children;
    ContentParent::GetAll(children);
    for (uint32_t i = 0; i < children.Length(); i++) {
      ContentParent* cp = children[i];
      nsCOMPtr<nsICycleCollectorLogSink> logSink =
        nsCycleCollector_createLogSink();

      logSink->SetFilenameIdentifier(identifier);
      logSink->SetProcessIdentifier(cp->Pid());

      Unused << cp->CycleCollectWithLogs(aDumpAllTraces, logSink,
                                         callbackHolder);
    }
  }

  nsCOMPtr<nsICycleCollectorListener> logger =
    do_CreateInstance("@mozilla.org/cycle-collector-logger;1");

  if (aDumpAllTraces) {
    nsCOMPtr<nsICycleCollectorListener> allTracesLogger;
    logger->AllTraces(getter_AddRefs(allTracesLogger));
    logger = allTracesLogger;
  }

  nsCOMPtr<nsICycleCollectorLogSink> logSink;
  logger->GetLogSink(getter_AddRefs(logSink));

  logSink->SetFilenameIdentifier(identifier);

  nsJSContext::CycleCollectNow(logger);

  nsCOMPtr<nsIFile> gcLog, ccLog;
  logSink->GetGcLog(getter_AddRefs(gcLog));
  logSink->GetCcLog(getter_AddRefs(ccLog));
  callbackHolder->OnDump(gcLog, ccLog, /* parent = */ true);

  return NS_OK;
}

NS_IMETHODIMP
nsMemoryInfoDumper::DumpGCAndCCLogsToSink(bool aDumpAllTraces,
                                          nsICycleCollectorLogSink* aSink)
{
  nsCOMPtr<nsICycleCollectorListener> logger =
    do_CreateInstance("@mozilla.org/cycle-collector-logger;1");

  if (aDumpAllTraces) {
    nsCOMPtr<nsICycleCollectorListener> allTracesLogger;
    logger->AllTraces(getter_AddRefs(allTracesLogger));
    logger = allTracesLogger;
  }

  logger->SetLogSink(aSink);

  nsJSContext::CycleCollectNow(logger);

  return NS_OK;
}

static void
MakeFilename(const char* aPrefix, const nsAString& aIdentifier,
             int aPid, const char* aSuffix, nsACString& aResult)
{
  aResult = nsPrintfCString("%s-%s-%d.%s",
                            aPrefix,
                            NS_ConvertUTF16toUTF8(aIdentifier).get(),
                            aPid, aSuffix);
}

// This class wraps GZFileWriter so it can be used with JSONWriter, overcoming
// the following two problems:
// - It provides a JSONWriterFunc::Write() that calls nsGZFileWriter::Write().
// - It can be stored as a UniquePtr, whereas nsGZFileWriter is refcounted.
class GZWriterWrapper : public JSONWriteFunc
{
public:
  explicit GZWriterWrapper(nsGZFileWriter* aGZWriter)
    : mGZWriter(aGZWriter)
  {}

  void Write(const char* aStr) override
  {
    // Ignore any failure because JSONWriteFunc doesn't have a mechanism for
    // handling errors.
    Unused << mGZWriter->Write(aStr);
  }

  nsresult Finish() { return mGZWriter->Finish(); }

private:
  RefPtr<nsGZFileWriter> mGZWriter;
};

// We need two callbacks: one that handles reports, and one that is called at
// the end of reporting. Both the callbacks need access to the same JSONWriter,
// so we implement both of them in this one class.
class HandleReportAndFinishReportingCallbacks final
  : public nsIHandleReportCallback, public nsIFinishReportingCallback
{
public:
  NS_DECL_ISUPPORTS

  HandleReportAndFinishReportingCallbacks(UniquePtr<JSONWriter> aWriter,
                                          nsIFinishDumpingCallback* aFinishDumping,
                                          nsISupports* aFinishDumpingData)
    : mWriter(std::move(aWriter))
    , mFinishDumping(aFinishDumping)
    , mFinishDumpingData(aFinishDumpingData)
  {
  }

  // This is the callback for nsIHandleReportCallback.
  NS_IMETHOD Callback(const nsACString& aProcess, const nsACString& aPath,
                      int32_t aKind, int32_t aUnits, int64_t aAmount,
                      const nsACString& aDescription,
                      nsISupports* aData) override
  {
    nsAutoCString process;
    if (aProcess.IsEmpty()) {
      // If the process is empty, the report originated with the process doing
      // the dumping.  In that case, generate the process identifier, which is
      // of the form "$PROCESS_NAME (pid $PID)", or just "(pid $PID)" if we
      // don't have a process name.  If we're the main process, we let
      // $PROCESS_NAME be "Main Process".
      if (XRE_IsParentProcess()) {
        // We're the main process.
        process.AssignLiteral("Main Process");
      } else if (ContentChild* cc = ContentChild::GetSingleton()) {
        // Try to get the process name from ContentChild.
        cc->GetProcessName(process);
      }
      ContentChild::AppendProcessId(process);

    } else {
      // Otherwise, the report originated with another process and already has a
      // process name.  Just use that.
      process = aProcess;
    }

    mWriter->StartObjectElement();
    {
      mWriter->StringProperty("process", process.get());
      mWriter->StringProperty("path", PromiseFlatCString(aPath).get());
      mWriter->IntProperty("kind", aKind);
      mWriter->IntProperty("units", aUnits);
      mWriter->IntProperty("amount", aAmount);
      mWriter->StringProperty("description",
                              PromiseFlatCString(aDescription).get());
    }
    mWriter->EndObject();

    return NS_OK;
  }

  // This is the callback for nsIFinishReportingCallback.
  NS_IMETHOD Callback(nsISupports* aData) override
  {
    mWriter->EndArray();  // end of "reports" array
    mWriter->End();

    // The call to Finish() deallocates the memory allocated by the first Write
    // call. Because that memory was live while the memory reporters ran and
    // was measured by them -- by "heap-allocated" if nothing else -- we want
    // DMD to see it as well. So we deliberately don't call Finish() until
    // after DMD finishes.
    nsresult rv = static_cast<GZWriterWrapper*>(mWriter->WriteFunc())->Finish();
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mFinishDumping) {
      return NS_OK;
    }

    return mFinishDumping->Callback(mFinishDumpingData);
  }

private:
  ~HandleReportAndFinishReportingCallbacks() {}

  UniquePtr<JSONWriter> mWriter;
  nsCOMPtr<nsIFinishDumpingCallback> mFinishDumping;
  nsCOMPtr<nsISupports> mFinishDumpingData;
};

NS_IMPL_ISUPPORTS(HandleReportAndFinishReportingCallbacks,
                  nsIHandleReportCallback, nsIFinishReportingCallback)

class TempDirFinishCallback final : public nsIFinishDumpingCallback
{
public:
  NS_DECL_ISUPPORTS

  TempDirFinishCallback(nsIFile* aReportsTmpFile,
                        const nsCString& aReportsFinalFilename)
    : mReportsTmpFile(aReportsTmpFile)
    , mReportsFilename(aReportsFinalFilename)
  {
  }

  NS_IMETHOD Callback(nsISupports* aData) override
  {
    // Rename the memory reports file, now that we're done writing all the
    // files. Its final name is "memory-report<-identifier>-<pid>.json.gz".

    nsCOMPtr<nsIFile> reportsFinalFile;
    nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR,
                                         getter_AddRefs(reportsFinalFile));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

  #ifdef ANDROID
    rv = reportsFinalFile->AppendNative(NS_LITERAL_CSTRING("memory-reports"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  #endif

    rv = reportsFinalFile->AppendNative(mReportsFilename);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = reportsFinalFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsAutoString reportsFinalFilename;
    rv = reportsFinalFile->GetLeafName(reportsFinalFilename);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = mReportsTmpFile->MoveTo(/* directory */ nullptr,
                                 reportsFinalFilename);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Write a message to the console.

    nsCOMPtr<nsIConsoleService> cs =
      do_GetService(NS_CONSOLESERVICE_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsString path;
    mReportsTmpFile->GetPath(path);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsString msg = NS_LITERAL_STRING("nsIMemoryInfoDumper dumped reports to ");
    msg.Append(path);
    return cs->LogStringMessage(msg.get());
  }

private:
  ~TempDirFinishCallback() {}

  nsCOMPtr<nsIFile> mReportsTmpFile;
  nsCString mReportsFilename;
};

NS_IMPL_ISUPPORTS(TempDirFinishCallback, nsIFinishDumpingCallback)

static nsresult
DumpMemoryInfoToFile(
  nsIFile* aReportsFile,
  nsIFinishDumpingCallback* aFinishDumping,
  nsISupports* aFinishDumpingData,
  bool aAnonymize,
  bool aMinimizeMemoryUsage,
  nsAString& aDMDIdentifier)
{
  RefPtr<nsGZFileWriter> gzWriter = new nsGZFileWriter();
  nsresult rv = gzWriter->Init(aReportsFile);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  auto jsonWriter =
    MakeUnique<JSONWriter>(MakeUnique<GZWriterWrapper>(gzWriter));

  nsCOMPtr<nsIMemoryReporterManager> mgr =
    do_GetService("@mozilla.org/memory-reporter-manager;1");

  // This is the first write to the file, and it causes |aWriter| to allocate
  // over 200 KiB of memory.
  jsonWriter->Start();
  {
    // Increment this number if the format changes.
    jsonWriter->IntProperty("version", 1);
    jsonWriter->BoolProperty("hasMozMallocUsableSize",
                             mgr->GetHasMozMallocUsableSize());
    jsonWriter->StartArrayProperty("reports");
  }

  RefPtr<HandleReportAndFinishReportingCallbacks>
    handleReportAndFinishReporting =
      new HandleReportAndFinishReportingCallbacks(std::move(jsonWriter),
                                                  aFinishDumping,
                                                  aFinishDumpingData);
  rv = mgr->GetReportsExtended(handleReportAndFinishReporting, nullptr,
                               handleReportAndFinishReporting, nullptr,
                               aAnonymize,
                               aMinimizeMemoryUsage,
                               aDMDIdentifier);
  return rv;
}

NS_IMETHODIMP
nsMemoryInfoDumper::DumpMemoryReportsToNamedFile(
  const nsAString& aFilename,
  nsIFinishDumpingCallback* aFinishDumping,
  nsISupports* aFinishDumpingData,
  bool aAnonymize)
{
  MOZ_ASSERT(!aFilename.IsEmpty());

  // Create the file.

  nsCOMPtr<nsIFile> reportsFile;
  nsresult rv = NS_NewLocalFile(aFilename, false, getter_AddRefs(reportsFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  reportsFile->InitWithPath(aFilename);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = reportsFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    rv = reportsFile->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  nsString dmdIdent = EmptyString();
  return DumpMemoryInfoToFile(reportsFile, aFinishDumping, aFinishDumpingData,
                              aAnonymize, /* minimizeMemoryUsage = */ false,
                              dmdIdent);
}

NS_IMETHODIMP
nsMemoryInfoDumper::DumpMemoryInfoToTempDir(const nsAString& aIdentifier,
                                            bool aAnonymize,
                                            bool aMinimizeMemoryUsage)
{
  nsString identifier(aIdentifier);
  EnsureNonEmptyIdentifier(identifier);

  // Open a new file named something like
  //
  //   incomplete-memory-report-<identifier>-<pid>.json.gz
  //
  // in NS_OS_TEMP_DIR for writing.  When we're finished writing the report,
  // we'll rename this file and get rid of the "incomplete-" prefix.
  //
  // We do this because we don't want scripts which poll the filesystem
  // looking for memory report dumps to grab a file before we're finished
  // writing to it.

  // The "unified" indicates that we merge the memory reports from all
  // processes and write out one file, rather than a separate file for
  // each process as was the case before bug 946407.  This is so that
  // the get_about_memory.py script in the B2G repository can
  // determine when it's done waiting for files to appear.
  nsCString reportsFinalFilename;
  MakeFilename("unified-memory-report", identifier, getpid(), "json.gz",
               reportsFinalFilename);

  nsCOMPtr<nsIFile> reportsTmpFile;
  nsresult rv;
  // In Android case, this function will open a file named aFilename under
  // specific folder (/data/local/tmp/memory-reports). Otherwise, it will
  // open a file named aFilename under "NS_OS_TEMP_DIR".
  rv = nsDumpUtils::OpenTempFile(NS_LITERAL_CSTRING("incomplete-") +
                                 reportsFinalFilename,
                                 getter_AddRefs(reportsTmpFile),
                                 NS_LITERAL_CSTRING("memory-reports"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<TempDirFinishCallback> finishDumping =
    new TempDirFinishCallback(reportsTmpFile, reportsFinalFilename);

  return DumpMemoryInfoToFile(reportsTmpFile, finishDumping, nullptr,
                              aAnonymize, aMinimizeMemoryUsage, identifier);
}

#ifdef MOZ_DMD
dmd::DMDFuncs::Singleton dmd::DMDFuncs::sSingleton;

nsresult
nsMemoryInfoDumper::OpenDMDFile(const nsAString& aIdentifier, int aPid,
                                FILE** aOutFile)
{
  if (!dmd::IsRunning()) {
    *aOutFile = nullptr;
    return NS_OK;
  }

  // Create a filename like dmd-<identifier>-<pid>.json.gz, which will be used
  // if DMD is enabled.
  nsCString dmdFilename;
  MakeFilename("dmd", aIdentifier, aPid, "json.gz", dmdFilename);

  // Open a new DMD file named |dmdFilename| in NS_OS_TEMP_DIR for writing,
  // and dump DMD output to it.  This must occur after the memory reporters
  // have been run (above), but before the memory-reports file has been
  // renamed (so scripts can detect the DMD file, if present).

  nsresult rv;
  nsCOMPtr<nsIFile> dmdFile;
  rv = nsDumpUtils::OpenTempFile(dmdFilename,
                                 getter_AddRefs(dmdFile),
                                 NS_LITERAL_CSTRING("memory-reports"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  rv = dmdFile->OpenANSIFileDesc("wb", aOutFile);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "OpenANSIFileDesc failed");

  // Print the path, because on some platforms (e.g. Mac) it's not obvious.
  dmd::StatusMsg("opened %s for writing\n",
                 dmdFile->HumanReadablePath().get());

  return rv;
}

nsresult
nsMemoryInfoDumper::DumpDMDToFile(FILE* aFile)
{
  RefPtr<nsGZFileWriter> gzWriter = new nsGZFileWriter();
  nsresult rv = gzWriter->InitANSIFileDesc(aFile);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Dump DMD's memory reports analysis to the file.
  dmd::Analyze(MakeUnique<GZWriterWrapper>(gzWriter));

  rv = gzWriter->Finish();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Finish failed");
  return rv;
}
#endif  // MOZ_DMD

