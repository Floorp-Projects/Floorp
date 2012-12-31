/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=50 et cin tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/nsMemoryInfoDumper.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/FileUtils.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/unused.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentChild.h"
#include "nsIConsoleService.h"
#include "nsICycleCollectorListener.h"
#include "nsDirectoryServiceDefs.h"
#include "nsGZFileWriter.h"
#include "nsJSEnvironment.h"
#include "nsPrintfCString.h"

#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

#ifdef XP_LINUX
#include <fcntl.h>
#endif

#ifdef ANDROID
#include <sys/stat.h>
#endif

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {
namespace {

class DumpMemoryReportsRunnable : public nsRunnable
{
public:
  DumpMemoryReportsRunnable(const nsAString& aIdentifier,
                            bool aMinimizeMemoryUsage,
                            bool aDumpChildProcesses)

      : mIdentifier(aIdentifier)
      , mMinimizeMemoryUsage(aMinimizeMemoryUsage)
      , mDumpChildProcesses(aDumpChildProcesses)
  {}

  NS_IMETHOD Run()
  {
    nsCOMPtr<nsIMemoryInfoDumper> dumper = do_GetService("@mozilla.org/memory-info-dumper;1");
    dumper->DumpMemoryReportsToFile(
      mIdentifier, mMinimizeMemoryUsage, mDumpChildProcesses);
    return NS_OK;
  }

private:
  const nsString mIdentifier;
  const bool mMinimizeMemoryUsage;
  const bool mDumpChildProcesses;
};

class GCAndCCLogDumpRunnable : public nsRunnable
{
public:
  GCAndCCLogDumpRunnable(const nsAString& aIdentifier,
                         bool aDumpChildProcesses)
    : mIdentifier(aIdentifier)
    , mDumpChildProcesses(aDumpChildProcesses)
  {}

  NS_IMETHOD Run()
  {
    nsCOMPtr<nsIMemoryInfoDumper> dumper = do_GetService("@mozilla.org/memory-info-dumper;1");
    dumper->DumpGCAndCCLogsToFile(
      mIdentifier, mDumpChildProcesses);
    return NS_OK;
  }

private:
  const nsString mIdentifier;
  const bool mDumpChildProcesses;
};

} // anonymous namespace

#ifdef XP_LINUX // {
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
static int sDumpAboutMemorySignum;         // SIGRTMIN
static int sDumpAboutMemoryAfterMMUSignum; // SIGRTMIN + 1
static int sGCAndCCDumpSignum;             // SIGRTMIN + 2

// This is the write-end of a pipe that we use to notice when a
// dump-about-memory signal occurs.
static int sDumpAboutMemoryPipeWriteFd;

void
DumpAboutMemorySignalHandler(int aSignum)
{
  // This is a signal handler, so everything in here needs to be
  // async-signal-safe.  Be careful!

  if (sDumpAboutMemoryPipeWriteFd != 0) {
    uint8_t signum = static_cast<int>(aSignum);
    write(sDumpAboutMemoryPipeWriteFd, &signum, sizeof(signum));
  }
}

class SignalPipeWatcher : public MessageLoopForIO::Watcher
{
public:
  SignalPipeWatcher()
  {}

  ~SignalPipeWatcher()
  {
    // This is somewhat paranoid, but we want to avoid the race condition where
    // we close sDumpAboutMemoryPipeWriteFd before setting it to 0, then we
    // reuse that fd for some other file, and then the signal handler runs.
    int pipeWriteFd = sDumpAboutMemoryPipeWriteFd;
    PR_ATOMIC_SET(&sDumpAboutMemoryPipeWriteFd, 0);

    // Stop watching the pipe's file descriptor /before/ we close it!
    mReadWatcher.StopWatchingFileDescriptor();

    close(pipeWriteFd);
    close(mPipeReadFd);
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SignalPipeWatcher)

  bool Start()
  {
    MOZ_ASSERT(XRE_GetIOMessageLoop() == MessageLoopForIO::current());

    sDumpAboutMemorySignum = SIGRTMIN;
    sDumpAboutMemoryAfterMMUSignum = SIGRTMIN + 1;
    sGCAndCCDumpSignum = SIGRTMIN + 2;

    // Create a pipe.  When we receive a signal in our signal handler, we'll
    // write the signum to the write-end of this pipe.
    int pipeFds[2];
    if (pipe(pipeFds)) {
      NS_WARNING("Failed to create pipe.");
      return false;
    }

    // Close this pipe on calls to exec().
    fcntl(pipeFds[0], F_SETFD, FD_CLOEXEC);
    fcntl(pipeFds[1], F_SETFD, FD_CLOEXEC);

    mPipeReadFd = pipeFds[0];
    sDumpAboutMemoryPipeWriteFd = pipeFds[1];

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);
    action.sa_handler = DumpAboutMemorySignalHandler;

    if (sigaction(sDumpAboutMemorySignum, &action, nullptr)) {
      NS_WARNING("Failed to register about:memory dump signal handler.");
    }
    if (sigaction(sDumpAboutMemoryAfterMMUSignum, &action, nullptr)) {
      NS_WARNING("Failed to register about:memory dump after MMU signal handler.");
    }
    if (sigaction(sGCAndCCDumpSignum, &action, nullptr)) {
      NS_WARNING("Failed to register GC+CC dump signal handler.");
    }

    // Start watching the read end of the pipe on the IO thread.
    return MessageLoopForIO::current()->WatchFileDescriptor(
      mPipeReadFd, /* persistent = */ true,
      MessageLoopForIO::WATCH_READ,
      &mReadWatcher, this);
  }

  virtual void OnFileCanReadWithoutBlocking(int aFd)
  {
    MOZ_ASSERT(XRE_GetIOMessageLoop() == MessageLoopForIO::current());

    uint8_t signum;
    ssize_t numReceived = read(aFd, &signum, sizeof(signum));
    if (numReceived != sizeof(signum)) {
      NS_WARNING("Error reading from buffer in "
                 "SignalPipeWatcher::OnFileCanReadWithoutBlocking.");
      return;
    }

    if (signum == sDumpAboutMemorySignum ||
        signum == sDumpAboutMemoryAfterMMUSignum) {
      // Dump our memory reports (but run this on the main thread!).
      bool doMMUFirst = signum == sDumpAboutMemoryAfterMMUSignum;
      nsRefPtr<DumpMemoryReportsRunnable> runnable =
        new DumpMemoryReportsRunnable(
            /* identifier = */ EmptyString(),
            doMMUFirst,
            /* dumpChildProcesses = */ true);
      NS_DispatchToMainThread(runnable);
    }
    else if (signum == sGCAndCCDumpSignum) {
      // Dump GC and CC logs (from the main thread).
      nsRefPtr<GCAndCCLogDumpRunnable> runnable =
        new GCAndCCLogDumpRunnable(
            /* identifier = */ EmptyString(),
            /* dumpChildProcesses = */ true);
      NS_DispatchToMainThread(runnable);
    }
    else {
      NS_WARNING("Got unexpected signum.");
    }
  }

  virtual void OnFileCanWriteWithoutBlocking(int aFd)
  {}

private:
  int mPipeReadFd;
  MessageLoopForIO::FileDescriptorWatcher mReadWatcher;
};

StaticRefPtr<SignalPipeWatcher> sSignalPipeWatcher;

void
InitializeSignalWatcher()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sSignalPipeWatcher);

  sSignalPipeWatcher = new SignalPipeWatcher();
  ClearOnShutdown(&sSignalPipeWatcher);

  XRE_GetIOMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(sSignalPipeWatcher.get(),
                        &SignalPipeWatcher::Start));
}

} // anonymous namespace
#endif // } XP_LINUX

} // namespace mozilla

NS_IMPL_ISUPPORTS1(nsMemoryInfoDumper, nsIMemoryInfoDumper)

nsMemoryInfoDumper::nsMemoryInfoDumper()
{
}

nsMemoryInfoDumper::~nsMemoryInfoDumper()
{
}

/* static */ void
nsMemoryInfoDumper::Initialize()
{
#ifdef XP_LINUX
  InitializeSignalWatcher();
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

NS_IMETHODIMP
nsMemoryInfoDumper::DumpMemoryReportsToFile(
    const nsAString& aIdentifier,
    bool aMinimizeMemoryUsage,
    bool aDumpChildProcesses)
{
  nsString identifier(aIdentifier);
  EnsureNonEmptyIdentifier(identifier);

  // Kick off memory report dumps in our child processes, if applicable.  We
  // do this before doing our own report because writing a report may be I/O
  // bound, in which case we want to busy the CPU with other reports while we
  // work on our own.
  if (aDumpChildProcesses) {
    nsTArray<ContentParent*> children;
    ContentParent::GetAll(children);
    for (uint32_t i = 0; i < children.Length(); i++) {
      unused << children[i]->SendDumpMemoryReportsToFile(
          identifier, aMinimizeMemoryUsage, aDumpChildProcesses);
    }
  }

  if (aMinimizeMemoryUsage) {
    // Minimize memory usage, then run DumpMemoryReportsToFile again.
    nsRefPtr<DumpMemoryReportsRunnable> callback =
      new DumpMemoryReportsRunnable(identifier,
          /* minimizeMemoryUsage = */ false,
          /* dumpChildProcesses = */ false);
    nsCOMPtr<nsIMemoryReporterManager> mgr =
      do_GetService("@mozilla.org/memory-reporter-manager;1");
    NS_ENSURE_TRUE(mgr, NS_ERROR_FAILURE);
    nsCOMPtr<nsICancelableRunnable> runnable;
    mgr->MinimizeMemoryUsage(callback, getter_AddRefs(runnable));
    return NS_OK;
  }

  return DumpMemoryReportsToFileImpl(identifier);
}

NS_IMETHODIMP
nsMemoryInfoDumper::DumpGCAndCCLogsToFile(
  const nsAString& aIdentifier,
  bool aDumpChildProcesses)
{
  nsString identifier(aIdentifier);
  EnsureNonEmptyIdentifier(identifier);

  if (aDumpChildProcesses) {
    nsTArray<ContentParent*> children;
    ContentParent::GetAll(children);
    for (uint32_t i = 0; i < children.Length(); i++) {
      unused << children[i]->SendDumpGCAndCCLogsToFile(
          identifier, aDumpChildProcesses);
    }
  }

  nsCOMPtr<nsICycleCollectorListener> logger =
    do_CreateInstance("@mozilla.org/cycle-collector-logger;1");
  logger->SetFilenameIdentifier(identifier);

  nsJSContext::CycleCollectNow(logger);
  return NS_OK;
}

namespace mozilla {

#define DUMP(o, s) \
  do { \
    nsresult rv = (o)->Write(s); \
    NS_ENSURE_SUCCESS(rv, rv); \
  } while (0)

static nsresult
DumpReport(nsIGZFileWriter *aWriter, bool aIsFirst,
  const nsACString &aProcess, const nsACString &aPath, int32_t aKind,
  int32_t aUnits, int64_t aAmount, const nsACString &aDescription)
{
  DUMP(aWriter, aIsFirst ? "[" : ",");

  // We only want to dump reports for this process.  If |aProcess| is
  // non-NULL that means we've received it from another process in response
  // to a "child-memory-reporter-request" event;  ignore such reports.
  if (!aProcess.IsEmpty()) {
    return NS_OK;
  }

  // Generate the process identifier, which is of the form "$PROCESS_NAME
  // (pid $PID)", or just "(pid $PID)" if we don't have a process name.  If
  // we're the main process, we let $PROCESS_NAME be "Main Process".
  nsAutoCString processId;
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    // We're the main process.
    processId.AssignLiteral("Main Process ");
  } else if (ContentChild *cc = ContentChild::GetSingleton()) {
    // Try to get the process name from ContentChild.
    nsAutoString processName;
    cc->GetProcessName(processName);
    processId.Assign(NS_ConvertUTF16toUTF8(processName));
    if (!processId.IsEmpty()) {
      processId.AppendLiteral(" ");
    }
  }

  // Add the PID to the identifier.
  unsigned pid = getpid();
  processId.Append(nsPrintfCString("(pid %u)", pid));

  DUMP(aWriter, "\n    {\"process\": \"");
  DUMP(aWriter, processId);

  DUMP(aWriter, "\", \"path\": \"");
  nsCString path(aPath);
  path.ReplaceSubstring("\\", "\\\\");    /* <backslash> --> \\ */
  path.ReplaceSubstring("\"", "\\\"");    // " --> \"
  DUMP(aWriter, path);

  DUMP(aWriter, "\", \"kind\": ");
  DUMP(aWriter, nsPrintfCString("%d", aKind));

  DUMP(aWriter, ", \"units\": ");
  DUMP(aWriter, nsPrintfCString("%d", aUnits));

  DUMP(aWriter, ", \"amount\": ");
  DUMP(aWriter, nsPrintfCString("%lld", aAmount));

  nsCString description(aDescription);
  description.ReplaceSubstring("\\", "\\\\");    /* <backslash> --> \\ */
  description.ReplaceSubstring("\"", "\\\"");    // " --> \"
  description.ReplaceSubstring("\n", "\\n");     // <newline> --> \n
  DUMP(aWriter, ", \"description\": \"");
  DUMP(aWriter, description);
  DUMP(aWriter, "\"}");

  return NS_OK;
}

class DumpMultiReporterCallback MOZ_FINAL : public nsIMemoryMultiReporterCallback
{
  public:
    NS_DECL_ISUPPORTS

      NS_IMETHOD Callback(const nsACString &aProcess, const nsACString &aPath,
          int32_t aKind, int32_t aUnits, int64_t aAmount,
          const nsACString &aDescription,
          nsISupports *aData)
      {
        nsCOMPtr<nsIGZFileWriter> writer = do_QueryInterface(aData);
        NS_ENSURE_TRUE(writer, NS_ERROR_FAILURE);

        // The |isFirst = false| assumes that at least one single reporter is
        // present and so will have been processed in
        // DumpMemoryReportsToFileImpl() below.
        return DumpReport(writer, /* isFirst = */ false, aProcess, aPath,
            aKind, aUnits, aAmount, aDescription);
        return NS_OK;
      }
};

NS_IMPL_ISUPPORTS1(
    DumpMultiReporterCallback
    , nsIMemoryMultiReporterCallback
    )

} // namespace mozilla

static void
MakeFilename(const char *aPrefix, const nsAString &aIdentifier,
             const char *aSuffix, nsACString &aResult)
{
  aResult = nsPrintfCString("%s-%s-%d.%s",
                            aPrefix,
                            NS_ConvertUTF16toUTF8(aIdentifier).get(),
                            getpid(), aSuffix);
}

static nsresult
OpenTempFile(const nsACString &aFilename, nsIFile* *aFile)
{
  nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, aFile);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> file(*aFile);

  rv = file->AppendNative(aFilename);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = file->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0644);
  NS_ENSURE_SUCCESS(rv, rv);
#ifdef ANDROID
  {
    // On android the default system umask is 0077 which makes these files
    // unreadable to the shell user. In order to pull the dumps off a non-rooted
    // device we need to chmod them to something world-readable.
    // XXX why not logFile->SetPermissions(0644);
    nsAutoCString path;
    rv = file->GetNativePath(path);
    if (NS_SUCCEEDED(rv)) {
      chmod(path.get(), 0644);
    }
  }
#endif
  return NS_OK;
}

#ifdef MOZ_DMD
struct DMDWriteState
{
  static const size_t kBufSize = 4096;
  char mBuf[kBufSize];
  nsRefPtr<nsGZFileWriter> mGZWriter;

  DMDWriteState(nsGZFileWriter *aGZWriter)
    : mGZWriter(aGZWriter)
  {}
};

static void DMDWrite(void* aState, const char* aFmt, va_list ap)
{
  DMDWriteState *state = (DMDWriteState*)aState;
  vsnprintf(state->mBuf, state->kBufSize, aFmt, ap);
  unused << state->mGZWriter->Write(state->mBuf);
}
#endif

/* static */ nsresult
nsMemoryInfoDumper::DumpMemoryReportsToFileImpl(
  const nsAString& aIdentifier)
{
  MOZ_ASSERT(!aIdentifier.IsEmpty());

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

  // Note that |mrFilename| is missing the "incomplete-" prefix; we'll tack
  // that on in a moment.
  nsCString mrFilename;
  MakeFilename("memory-report", aIdentifier, "json.gz", mrFilename);

  nsCOMPtr<nsIFile> mrTmpFile;
  nsresult rv;
  rv = OpenTempFile(NS_LITERAL_CSTRING("incomplete-") + mrFilename,
                    getter_AddRefs(mrTmpFile));
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<nsGZFileWriter> writer = new nsGZFileWriter();
  rv = writer->Init(mrTmpFile);
  NS_ENSURE_SUCCESS(rv, rv);

  // Dump the memory reports to the file.

  // Increment this number if the format changes.
  DUMP(writer, "{\n  \"version\": 1,\n");

  DUMP(writer, "  \"hasMozMallocUsableSize\": ");

  nsCOMPtr<nsIMemoryReporterManager> mgr =
    do_GetService("@mozilla.org/memory-reporter-manager;1");
  NS_ENSURE_STATE(mgr);

  DUMP(writer, mgr->GetHasMozMallocUsableSize() ? "true" : "false");
  DUMP(writer, ",\n");
  DUMP(writer, "  \"reports\": ");

  // Process single reporters.
  bool isFirst = true;
  bool more;
  nsCOMPtr<nsISimpleEnumerator> e;
  mgr->EnumerateReporters(getter_AddRefs(e));
  while (NS_SUCCEEDED(e->HasMoreElements(&more)) && more) {
    nsCOMPtr<nsIMemoryReporter> r;
    e->GetNext(getter_AddRefs(r));

    nsCString process;
    rv = r->GetProcess(process);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCString path;
    rv = r->GetPath(path);
    NS_ENSURE_SUCCESS(rv, rv);

    int32_t kind;
    rv = r->GetKind(&kind);
    NS_ENSURE_SUCCESS(rv, rv);

    int32_t units;
    rv = r->GetUnits(&units);
    NS_ENSURE_SUCCESS(rv, rv);

    int64_t amount;
    rv = r->GetAmount(&amount);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCString description;
    rv = r->GetDescription(description);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = DumpReport(writer, isFirst, process, path, kind, units, amount,
                    description);
    NS_ENSURE_SUCCESS(rv, rv);

    isFirst = false;
  }

  // Process multi-reporters.
  nsCOMPtr<nsISimpleEnumerator> e2;
  mgr->EnumerateMultiReporters(getter_AddRefs(e2));
  nsRefPtr<DumpMultiReporterCallback> cb = new DumpMultiReporterCallback();
  while (NS_SUCCEEDED(e2->HasMoreElements(&more)) && more) {
    nsCOMPtr<nsIMemoryMultiReporter> r;
    e2->GetNext(getter_AddRefs(r));
    r->CollectReports(cb, writer);
  }

  DUMP(writer, "\n  ]\n}");

  rv = writer->Finish();
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef MOZ_DMD
  // Open a new file named something like
  //
  //   dmd-<identifier>-<pid>.txt.gz
  //
  // in NS_OS_TEMP_DIR for writing, and dump DMD output to it.  This must occur
  // after the memory reporters have been run (above), but before the
  // memory-reports file has been renamed (so scripts can detect the DMD file,
  // if present).

  nsCString dmdFilename;
  MakeFilename("dmd", aIdentifier, "txt.gz", dmdFilename);

  nsCOMPtr<nsIFile> dmdFile;
  rv = OpenTempFile(dmdFilename, getter_AddRefs(dmdFile));
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<nsGZFileWriter> dmdWriter = new nsGZFileWriter();
  rv = dmdWriter->Init(dmdFile);
  NS_ENSURE_SUCCESS(rv, rv);

  // Dump DMD output to the file.

  DMDWriteState state(dmdWriter);
  dmd::Writer w(DMDWrite, &state);
  mozilla::dmd::Dump(w);

  rv = dmdWriter->Finish();
  NS_ENSURE_SUCCESS(rv, rv);
#endif  // MOZ_DMD

  // Rename the file, now that we're done dumping the report.  The file's
  // ultimate destination is "memory-report<-identifier>-<pid>.json.gz".

  nsCOMPtr<nsIFile> mrFinalFile;
  rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(mrFinalFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mrFinalFile->AppendNative(mrFilename);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mrFinalFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString mrActualFinalFilename;
  rv = mrFinalFile->GetLeafName(mrActualFinalFilename);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mrTmpFile->MoveTo(/* directory */ nullptr, mrActualFinalFilename);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIConsoleService> cs =
    do_GetService(NS_CONSOLESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsString path;
  mrTmpFile->GetPath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  nsString msg = NS_LITERAL_STRING(
    "nsIMemoryInfoDumper dumped reports to ");
  msg.Append(path);
  return cs->LogStringMessage(msg.get());
}

#undef DUMP
