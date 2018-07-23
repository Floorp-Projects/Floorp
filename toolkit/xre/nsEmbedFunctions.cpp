/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "base/basictypes.h"

#include "nsXULAppAPI.h"

#include <stdlib.h>
#if defined(MOZ_WIDGET_GTK)
#include <glib.h>
#endif

#include "prenv.h"

#include "nsIAppShell.h"
#include "nsIAppStartupNotifier.h"
#include "nsIDirectoryService.h"
#include "nsIFile.h"
#include "nsIToolkitChromeRegistry.h"
#include "nsIToolkitProfile.h"

#ifdef XP_WIN
#include <process.h>
#include <shobjidl.h>
#include "mozilla/ipc/WindowsMessageLoop.h"
#endif

#include "nsAppDirectoryServiceDefs.h"
#include "nsAppRunner.h"
#include "nsAutoRef.h"
#include "nsDirectoryServiceDefs.h"
#include "nsExceptionHandler.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsJSUtils.h"
#include "nsWidgetsCID.h"
#include "nsXREDirProvider.h"
#include "ThreadAnnotation.h"

#include "mozilla/Omnijar.h"
#if defined(XP_MACOSX)
#include "nsVersionComparator.h"
#include "chrome/common/mach_ipc_mac.h"
#endif
#include "nsX11ErrorHandler.h"
#include "nsGDKErrorHandler.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "chrome/common/child_process.h"
#if defined(MOZ_WIDGET_ANDROID)
#include "chrome/common/ipc_channel.h"
#include "mozilla/jni/Utils.h"
#endif //  defined(MOZ_WIDGET_ANDROID)

#include "mozilla/AbstractThread.h"
#include "mozilla/FilePreferences.h"

#include "mozilla/ipc/BrowserProcessSubThread.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/ipc/IOThreadChild.h"
#include "mozilla/ipc/ProcessChild.h"
#include "mozilla/recordreplay/ChildIPC.h"
#include "mozilla/recordreplay/ParentIPC.h"
#include "ScopedXREEmbed.h"

#include "mozilla/plugins/PluginProcessChild.h"
#include "mozilla/dom/ContentProcess.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentChild.h"

#include "mozilla/ipc/TestShellParent.h"
#include "mozilla/ipc/XPCShellEnvironment.h"
#include "mozilla/Scheduler.h"
#include "mozilla/WindowsDllBlocklist.h"

#include "GMPProcessChild.h"
#include "mozilla/gfx/GPUProcessImpl.h"

#include "GeckoProfiler.h"

#if defined(MOZ_SANDBOX) && defined(XP_WIN)
#include "mozilla/sandboxTarget.h"
#include "mozilla/sandboxing/loggingCallbacks.h"
#endif

#if defined(MOZ_CONTENT_SANDBOX)
#include "mozilla/SandboxSettings.h"
#include "mozilla/Preferences.h"
#endif

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
#include "mozilla/Sandbox.h"
#endif

#if defined(XP_LINUX)
#include <sys/prctl.h>
#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif
#ifndef PR_SET_PTRACER_ANY
#define PR_SET_PTRACER_ANY ((unsigned long)-1)
#endif
#endif

#ifdef MOZ_IPDL_TESTS
#include "mozilla/_ipdltest/IPDLUnitTests.h"
#include "mozilla/_ipdltest/IPDLUnitTestProcessChild.h"

using mozilla::_ipdltest::IPDLUnitTestProcessChild;
#endif  // ifdef MOZ_IPDL_TESTS

#ifdef MOZ_JPROF
#include "jprof.h"
#endif

#if defined(XP_WIN) && defined(MOZ_ENABLE_SKIA_PDF)
#include "mozilla/widget/PDFiumProcessChild.h"
#endif

using namespace mozilla;

using mozilla::ipc::BrowserProcessSubThread;
using mozilla::ipc::GeckoChildProcessHost;
using mozilla::ipc::IOThreadChild;
using mozilla::ipc::ProcessChild;
using mozilla::ipc::ScopedXREEmbed;

using mozilla::plugins::PluginProcessChild;
using mozilla::dom::ContentProcess;
using mozilla::dom::ContentParent;
using mozilla::dom::ContentChild;

using mozilla::gmp::GMPProcessChild;

using mozilla::ipc::TestShellParent;
using mozilla::ipc::TestShellCommandParent;
using mozilla::ipc::XPCShellEnvironment;

using mozilla::startup::sChildProcessType;

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

nsresult
XRE_LockProfileDirectory(nsIFile* aDirectory,
                         nsISupports* *aLockObject)
{
  nsCOMPtr<nsIProfileLock> lock;

  nsresult rv = NS_LockProfilePath(aDirectory, nullptr, nullptr,
                                   getter_AddRefs(lock));
  if (NS_SUCCEEDED(rv))
    NS_ADDREF(*aLockObject = lock);

  return rv;
}

static int32_t sInitCounter;

nsresult
XRE_InitEmbedding2(nsIFile *aLibXULDirectory,
                   nsIFile *aAppDirectory,
                   nsIDirectoryServiceProvider *aAppDirProvider)
{
  // Initialize some globals to make nsXREDirProvider happy
  static char* kNullCommandLine[] = { nullptr };
  gArgv = kNullCommandLine;
  gArgc = 0;

  NS_ENSURE_ARG(aLibXULDirectory);

  if (++sInitCounter > 1) // XXXbsmedberg is this really the right solution?
    return NS_OK;

  if (!aAppDirectory)
    aAppDirectory = aLibXULDirectory;

  nsresult rv;

  new nsXREDirProvider; // This sets gDirServiceProvider
  if (!gDirServiceProvider)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = gDirServiceProvider->Initialize(aAppDirectory, aLibXULDirectory,
                                       aAppDirProvider);
  if (NS_FAILED(rv))
    return rv;

  rv = NS_InitXPCOM2(nullptr, aAppDirectory, gDirServiceProvider);
  if (NS_FAILED(rv))
    return rv;

  // We do not need to autoregister components here. The CheckCompatibility()
  // bits in nsAppRunner.cpp check for an invalidation flag in
  // compatibility.ini.
  // If the app wants to autoregister every time (for instance, if it's debug),
  // it can do so after we return from this function.

  nsCOMPtr<nsIObserver> startupNotifier
    (do_CreateInstance(NS_APPSTARTUPNOTIFIER_CONTRACTID));
  if (!startupNotifier)
    return NS_ERROR_FAILURE;

  startupNotifier->Observe(nullptr, APPSTARTUP_TOPIC, nullptr);

  return NS_OK;
}

void
XRE_NotifyProfile()
{
  NS_ASSERTION(gDirServiceProvider, "XRE_InitEmbedding was not called!");
  gDirServiceProvider->DoStartup();
}

void
XRE_TermEmbedding()
{
  if (--sInitCounter != 0)
    return;

  NS_ASSERTION(gDirServiceProvider,
               "XRE_TermEmbedding without XRE_InitEmbedding");

  gDirServiceProvider->DoShutdown();
  NS_ShutdownXPCOM(nullptr);
  delete gDirServiceProvider;
}

const char*
XRE_ChildProcessTypeToString(GeckoProcessType aProcessType)
{
  return (aProcessType < GeckoProcessType_End) ?
    kGeckoProcessTypeString[aProcessType] : "invalid";
}

namespace mozilla {
namespace startup {
GeckoProcessType sChildProcessType = GeckoProcessType_Default;
} // namespace startup
} // namespace mozilla

#if defined(MOZ_WIDGET_ANDROID)
void
XRE_SetAndroidChildFds (JNIEnv* env, const XRE_AndroidChildFds& fds)
{
  mozilla::jni::SetGeckoThreadEnv(env);
  mozilla::dom::SetPrefsFd(fds.mPrefsFd);
  mozilla::dom::SetPrefMapFd(fds.mPrefMapFd);
  IPC::Channel::SetClientChannelFd(fds.mIpcFd);
  CrashReporter::SetNotificationPipeForChild(fds.mCrashFd);
  CrashReporter::SetCrashAnnotationPipeForChild(fds.mCrashAnnotationFd);
}
#endif // defined(MOZ_WIDGET_ANDROID)

void
XRE_SetProcessType(const char* aProcessTypeString)
{
  static bool called = false;
  if (called) {
    MOZ_CRASH();
  }
  called = true;

  sChildProcessType = GeckoProcessType_Invalid;
  for (int i = 0;
       i < (int) ArrayLength(kGeckoProcessTypeString);
       ++i) {
    if (!strcmp(kGeckoProcessTypeString[i], aProcessTypeString)) {
      sChildProcessType = static_cast<GeckoProcessType>(i);
      return;
    }
  }
}

// FIXME/bug 539522: this out-of-place function is stuck here because
// IPDL wants access to this crashreporter interface, and
// crashreporter is built in such a way to make that awkward
bool
XRE_TakeMinidumpForChild(uint32_t aChildPid, nsIFile** aDump,
                         uint32_t* aSequence)
{
  return CrashReporter::TakeMinidumpForChild(aChildPid, aDump, aSequence);
}

bool
#if defined(XP_WIN)
XRE_SetRemoteExceptionHandler(const char* aPipe /*= 0*/,
                              uintptr_t aCrashTimeAnnotationFile)
#else
XRE_SetRemoteExceptionHandler(const char* aPipe /*= 0*/)
#endif
{
#if defined(XP_WIN)
  return CrashReporter::SetRemoteExceptionHandler(nsDependentCString(aPipe),
                                                  aCrashTimeAnnotationFile);
#elif defined(XP_MACOSX)
  return CrashReporter::SetRemoteExceptionHandler(nsDependentCString(aPipe));
#else
  return CrashReporter::SetRemoteExceptionHandler();
#endif
}

#if defined(XP_WIN)
void
SetTaskbarGroupId(const nsString& aId)
{
    if (FAILED(SetCurrentProcessExplicitAppUserModelID(aId.get()))) {
        NS_WARNING("SetCurrentProcessExplicitAppUserModelID failed for child process.");
    }
}
#endif

#if defined(MOZ_CONTENT_SANDBOX)
void
AddContentSandboxLevelAnnotation()
{
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    int level = GetEffectiveContentSandboxLevel();
    nsAutoCString levelString;
    levelString.AppendInt(level);
    CrashReporter::AnnotateCrashReport(
      NS_LITERAL_CSTRING("ContentSandboxLevel"), levelString);
  }
}
#endif /* MOZ_CONTENT_SANDBOX */

namespace {

int GetDebugChildPauseTime() {
  auto pauseStr = PR_GetEnv("MOZ_DEBUG_CHILD_PAUSE");
  if (pauseStr && *pauseStr) {
    int pause = atoi(pauseStr);
    if (pause != 1) { // must be !=1 since =1 enables the default pause time
#if defined(OS_WIN)
      pause *= 1000; // convert to ms
#endif
      return pause;
    }
  }
#ifdef OS_POSIX
  return 30; // seconds
#elif defined(OS_WIN)
  return 10000; // milliseconds
#else
  return 0;
#endif
}

} // namespace

nsresult
XRE_InitChildProcess(int aArgc,
                     char* aArgv[],
                     const XREChildData* aChildData)
{
  NS_ENSURE_ARG_MIN(aArgc, 2);
  NS_ENSURE_ARG_POINTER(aArgv);
  NS_ENSURE_ARG_POINTER(aArgv[0]);
  MOZ_ASSERT(aChildData);

  recordreplay::Initialize(aArgc, aArgv);

#ifdef MOZ_ASAN_REPORTER
  // In ASan reporter builds, we need to set ASan's log_path as early as
  // possible, so it dumps its errors into files there instead of using
  // the default stderr location. Since this is crucial for ASan reporter
  // to work at all (and we don't want people to use a non-functional
  // ASan reporter build), all failures while setting log_path are fatal.
  //
  // We receive this log_path via the ASAN_REPORTER_PATH environment variable
  // because there is no other way to generically get the necessary profile
  // directory in all child types without adding support for that in each
  // child process type class (at the risk of missing this in a child).
  //
  // In certain cases (e.g. child startup through xpcshell or gtests), this
  // code needs to remain disabled, as no ASAN_REPORTER_PATH would be available.
  if (!PR_GetEnv("MOZ_DISABLE_ASAN_REPORTER") &&
      !PR_GetEnv("MOZ_RUN_GTEST")) {
    nsCOMPtr<nsIFile> asanReporterPath = GetFileFromEnv("ASAN_REPORTER_PATH");
    if (!asanReporterPath) {
      MOZ_CRASH("Child did not receive ASAN_REPORTER_PATH!");
    }
    setASanReporterPath(asanReporterPath);
  }
#endif

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
  // This has to happen before glib thread pools are started.
  mozilla::SandboxEarlyInit();
#endif

#ifdef MOZ_JPROF
  // Call the code to install our handler
  setupProfilingStuff();
#endif

#if defined(XP_WIN)
  // From the --attach-console support in nsNativeAppSupportWin.cpp, but
  // here we are a content child process, so we always attempt to attach
  // to the parent's (ie, the browser's) console.
  // Try to attach console to the parent process.
  // It will succeed when the parent process is a command line,
  // so that stdio will be displayed in it.
  if (AttachConsole(ATTACH_PARENT_PROCESS)) {
    // Change std handles to refer to new console handles.
    // Before doing so, ensure that stdout/stderr haven't been
    // redirected to a valid file
    if (_fileno(stdout) == -1 ||
        _get_osfhandle(fileno(stdout)) == -1)
        freopen("CONOUT$", "w", stdout);
    // Merge stderr into CONOUT$ since there isn't any `CONERR$`.
    // http://msdn.microsoft.com/en-us/library/windows/desktop/ms683231%28v=vs.85%29.aspx
    if (_fileno(stderr) == -1 ||
        _get_osfhandle(fileno(stderr)) == -1)
        freopen("CONOUT$", "w", stderr);
    if (_fileno(stdin) == -1 || _get_osfhandle(fileno(stdin)) == -1)
        freopen("CONIN$", "r", stdin);
  }

#if defined(MOZ_SANDBOX)
  if (aChildData->sandboxTargetServices) {
    SandboxTarget::Instance()->SetTargetServices(aChildData->sandboxTargetServices);
  }
#endif
#endif

  // NB: This must be called before profiler_init
  ScopedLogging logger;

  mozilla::LogModule::Init(aArgc, aArgv);

  AUTO_PROFILER_INIT;
  AUTO_PROFILER_LABEL("XRE_InitChildProcess", OTHER);

  // Ensure AbstractThread is minimally setup, so async IPC messages
  // work properly.
  AbstractThread::InitTLS();

  // Complete 'task_t' exchange for Mac OS X. This structure has the same size
  // regardless of architecture so we don't have any cross-arch issues here.
#ifdef XP_MACOSX
  if (aArgc < 1)
    return NS_ERROR_FAILURE;
  const char* const mach_port_name = aArgv[--aArgc];

  Maybe<recordreplay::AutoPassThroughThreadEvents> pt;
  pt.emplace();

  const int kTimeoutMs = 1000;

  MachSendMessage child_message(0);
  if (!child_message.AddDescriptor(MachMsgPortDescriptor(mach_task_self()))) {
    NS_WARNING("child AddDescriptor(mach_task_self()) failed.");
    return NS_ERROR_FAILURE;
  }

  ReceivePort child_recv_port;
  mach_port_t raw_child_recv_port = child_recv_port.GetPort();
  if (!child_message.AddDescriptor(MachMsgPortDescriptor(raw_child_recv_port))) {
    NS_WARNING("Adding descriptor to message failed");
    return NS_ERROR_FAILURE;
  }

  ReceivePort* ports_out_receiver = new ReceivePort();
  if (!child_message.AddDescriptor(MachMsgPortDescriptor(ports_out_receiver->GetPort()))) {
    NS_WARNING("Adding descriptor to message failed");
    return NS_ERROR_FAILURE;
  }

  ReceivePort* ports_in_receiver = new ReceivePort();
  if (!child_message.AddDescriptor(MachMsgPortDescriptor(ports_in_receiver->GetPort()))) {
    NS_WARNING("Adding descriptor to message failed");
    return NS_ERROR_FAILURE;
  }

  MachPortSender child_sender(mach_port_name);
  kern_return_t err = child_sender.SendMessage(child_message, kTimeoutMs);
  if (err != KERN_SUCCESS) {
    NS_WARNING("child SendMessage() failed");
    return NS_ERROR_FAILURE;
  }

  MachReceiveMessage parent_message;
  err = child_recv_port.WaitForMessage(&parent_message, kTimeoutMs);
  if (err != KERN_SUCCESS) {
    NS_WARNING("child WaitForMessage() failed");
    return NS_ERROR_FAILURE;
  }

  if (parent_message.GetTranslatedPort(0) == MACH_PORT_NULL) {
    NS_WARNING("child GetTranslatedPort(0) failed");
    return NS_ERROR_FAILURE;
  }

  err = task_set_bootstrap_port(mach_task_self(),
                                parent_message.GetTranslatedPort(0));

  if (parent_message.GetTranslatedPort(1) == MACH_PORT_NULL) {
    NS_WARNING("child GetTranslatedPort(1) failed");
    return NS_ERROR_FAILURE;
  }
  MachPortSender* ports_out_sender = new MachPortSender(parent_message.GetTranslatedPort(1));

  if (parent_message.GetTranslatedPort(2) == MACH_PORT_NULL) {
    NS_WARNING("child GetTranslatedPort(2) failed");
    return NS_ERROR_FAILURE;
  }
  MachPortSender* ports_in_sender = new MachPortSender(parent_message.GetTranslatedPort(2));

  if (err != KERN_SUCCESS) {
    NS_WARNING("child task_set_bootstrap_port() failed");
    return NS_ERROR_FAILURE;
  }

  pt.reset();
#endif

  SetupErrorHandling(aArgv[0]);

  if (!CrashReporter::IsDummy()) {
#if defined(XP_WIN)
    if (aArgc < 1) {
      return NS_ERROR_FAILURE;
    }
    const char* const crashTimeAnnotationArg = aArgv[--aArgc];
    uintptr_t crashTimeAnnotationFile =
      static_cast<uintptr_t>(std::stoul(std::string(crashTimeAnnotationArg)));
#endif

    if (aArgc < 1)
      return NS_ERROR_FAILURE;
    const char* const crashReporterArg = aArgv[--aArgc];

#if defined(XP_MACOSX)
    // on windows and mac, |crashReporterArg| is the named pipe on which the
    // server is listening for requests, or "-" if crash reporting is
    // disabled.
    if (0 != strcmp("-", crashReporterArg) &&
        !XRE_SetRemoteExceptionHandler(crashReporterArg)) {
      // Bug 684322 will add better visibility into this condition
      NS_WARNING("Could not setup crash reporting\n");
    }
#elif defined(XP_WIN)
    if (0 != strcmp("-", crashReporterArg) &&
        !XRE_SetRemoteExceptionHandler(crashReporterArg,
                                       crashTimeAnnotationFile)) {
      // Bug 684322 will add better visibility into this condition
      NS_WARNING("Could not setup crash reporting\n");
    }
#else
    // on POSIX, |crashReporterArg| is "true" if crash reporting is
    // enabled, false otherwise
    if (0 != strcmp("false", crashReporterArg) &&
        !XRE_SetRemoteExceptionHandler(nullptr)) {
      // Bug 684322 will add better visibility into this condition
      NS_WARNING("Could not setup crash reporting\n");
    }
#endif
  }

  // For Init/Shutdown thread name annotations in the crash reporter.
  CrashReporter::InitThreadAnnotationRAII annotation;

  gArgv = aArgv;
  gArgc = aArgc;

#ifdef MOZ_X11
  XInitThreads();
#endif
#ifdef MOZ_WIDGET_GTK
  // Setting the name here avoids the need to pass this through to gtk_init().
  g_set_prgname(aArgv[0]);
#endif

#ifdef OS_POSIX
  if (PR_GetEnv("MOZ_DEBUG_CHILD_PROCESS") ||
      PR_GetEnv("MOZ_DEBUG_CHILD_PAUSE")) {
#if defined(XP_LINUX) && defined(DEBUG)
    if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0) != 0) {
      printf_stderr("Could not allow ptrace from any process.\n");
    }
#endif
    printf_stderr("\n\nCHILDCHILDCHILDCHILD (process type %s)\n  debug me @ %d\n\n",
                  XRE_ChildProcessTypeToString(XRE_GetProcessType()),
                  base::GetCurrentProcId());
    sleep(GetDebugChildPauseTime());
  }
#elif defined(OS_WIN)
  if (PR_GetEnv("MOZ_DEBUG_CHILD_PROCESS")) {
    NS_DebugBreak(NS_DEBUG_BREAK,
                  "Invoking NS_DebugBreak() to debug child process",
                  nullptr, __FILE__, __LINE__);
  } else if (PR_GetEnv("MOZ_DEBUG_CHILD_PAUSE")) {
    printf_stderr("\n\nCHILDCHILDCHILDCHILD (process type %s)\n  debug me @ %d\n\n",
                  XRE_ChildProcessTypeToString(XRE_GetProcessType()),
                  base::GetCurrentProcId());
    ::Sleep(GetDebugChildPauseTime());
  }
#endif

  // child processes launched by GeckoChildProcessHost get this magic
  // argument appended to their command lines
  const char* const parentPIDString = aArgv[aArgc-1];
  MOZ_ASSERT(parentPIDString, "NULL parent PID");
  --aArgc;

  char* end = 0;
  base::ProcessId parentPID = strtol(parentPIDString, &end, 10);
  MOZ_ASSERT(!*end, "invalid parent PID");

  nsCOMPtr<nsIFile> crashReportTmpDir;
  if (XRE_GetProcessType() == GeckoProcessType_GPU) {
    aArgc--;
    if (strlen(aArgv[aArgc])) { // if it's empty, ignore it
      nsresult rv = XRE_GetFileFromPath(aArgv[aArgc], getter_AddRefs(crashReportTmpDir));
      if (NS_FAILED(rv)) {
        // If we don't have a valid tmp dir we can probably still run ok, but
        // crash report .extra files might not get picked up by the parent
        // process. Debug-assert because this shouldn't happen in practice.
        MOZ_ASSERT(false, "GPU process started without valid tmp dir!");
      }
    }
  }

  // While replaying, use the parent PID that existed while recording.
  parentPID = recordreplay::RecordReplayValue(parentPID);

#ifdef XP_MACOSX
  mozilla::ipc::SharedMemoryBasic::SetupMachMemory(parentPID, ports_in_receiver, ports_in_sender,
                                                   ports_out_sender, ports_out_receiver, true);
#endif

#if defined(XP_WIN)
  // On Win7+, register the application user model id passed in by
  // parent. This insures windows created by the container properly
  // group with the parent app on the Win7 taskbar.
  const char* const appModelUserId = aArgv[--aArgc];
  if (appModelUserId) {
    // '-' implies no support
    if (*appModelUserId != '-') {
      nsString appId;
      CopyASCIItoUTF16(nsDependentCString(appModelUserId), appId);
      // The version string is encased in quotes
      appId.Trim("\"");
      // Set the id
      SetTaskbarGroupId(appId);
    }
  }
#endif

  base::AtExitManager exitManager;

  nsresult rv = XRE_InitCommandLine(aArgc, aArgv);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  MessageLoop::Type uiLoopType;
  switch (XRE_GetProcessType()) {
  case GeckoProcessType_Content:
  case GeckoProcessType_GPU:
      // Content processes need the XPCOM/chromium frankenventloop
      uiLoopType = MessageLoop::TYPE_MOZILLA_CHILD;
      break;
  case GeckoProcessType_GMPlugin:
  case GeckoProcessType_PDFium:
      uiLoopType = MessageLoop::TYPE_DEFAULT;
      break;
  default:
      uiLoopType = MessageLoop::TYPE_UI;
      break;
  }

  // If we are recording or replaying, initialize state and update arguments
  // according to those which were captured by the MiddlemanProcessChild in the
  // middleman process. No argument manipulation should happen between this
  // call and the point where the process child is initialized.
  recordreplay::child::InitRecordingOrReplayingProcess(&aArgc, &aArgv);

  {
    // This is a lexical scope for the MessageLoop below.  We want it
    // to go out of scope before NS_LogTerm() so that we don't get
    // spurious warnings about XPCOM objects being destroyed from a
    // static context.

    // Associate this thread with a UI MessageLoop
    MessageLoop uiMessageLoop(uiLoopType);
    {
      nsAutoPtr<ProcessChild> process;

#ifdef XP_WIN
      mozilla::ipc::windows::InitUIThread();
#endif

      switch (XRE_GetProcessType()) {
      case GeckoProcessType_Default:
        MOZ_CRASH("This makes no sense");
        break;

      case GeckoProcessType_Plugin:
        process = new PluginProcessChild(parentPID);
        break;

      case GeckoProcessType_Content:
        process = new ContentProcess(parentPID);
        break;

      case GeckoProcessType_IPDLUnitTest:
#ifdef MOZ_IPDL_TESTS
        process = new IPDLUnitTestProcessChild(parentPID);
#else
        MOZ_CRASH("rebuild with --enable-ipdl-tests");
#endif
        break;

      case GeckoProcessType_GMPlugin:
        process = new gmp::GMPProcessChild(parentPID);
        break;

#if defined(XP_WIN) && defined(MOZ_ENABLE_SKIA_PDF)
      case GeckoProcessType_PDFium:
        process = new widget::PDFiumProcessChild(parentPID);
        break;
#endif
      case GeckoProcessType_GPU:
        process = new gfx::GPUProcessImpl(parentPID);
        break;

      default:
        MOZ_CRASH("Unknown main thread class");
      }

      if (!process->Init(aArgc, aArgv)) {
        return NS_ERROR_FAILURE;
      }

#if defined(XP_WIN)
      // Set child processes up such that they will get killed after the
      // chrome process is killed in cases where the user shuts the system
      // down or logs off.
      ::SetProcessShutdownParameters(0x280 - 1, SHUTDOWN_NORETRY);
#endif

#if defined(MOZ_SANDBOX) && defined(XP_WIN)
      // We need to do this after the process has been initialised, as
      // InitLoggingIfRequired may need access to prefs.
      mozilla::sandboxing::InitLoggingIfRequired(aChildData->ProvideLogFunction);
#endif
      mozilla::FilePreferences::InitDirectoriesWhitelist();
      mozilla::FilePreferences::InitPrefs();

      OverrideDefaultLocaleIfNeeded();

#if defined(MOZ_CONTENT_SANDBOX)
      AddContentSandboxLevelAnnotation();
#endif

      // Run the UI event loop on the main thread.
      uiMessageLoop.MessageLoop::Run();

      // Allow ProcessChild to clean up after itself before going out of
      // scope and being deleted
      process->CleanUp();
      mozilla::Omnijar::CleanUp();

#if defined(XP_MACOSX)
      // Everybody should be done using shared memory by now.
      mozilla::ipc::SharedMemoryBasic::Shutdown();
#endif
    }
  }

  return XRE_DeinitCommandLine();
}

MessageLoop*
XRE_GetIOMessageLoop()
{
  if (sChildProcessType == GeckoProcessType_Default) {
    return BrowserProcessSubThread::GetMessageLoop(BrowserProcessSubThread::IO);
  }
  return IOThreadChild::message_loop();
}

namespace {

class MainFunctionRunnable : public Runnable
{
public:
  NS_DECL_NSIRUNNABLE

  MainFunctionRunnable(MainFunction aFunction, void* aData)
    : mozilla::Runnable("MainFunctionRunnable")
    , mFunction(aFunction)
    , mData(aData)
  {
    NS_ASSERTION(aFunction, "Don't give me a null pointer!");
  }

private:
  MainFunction mFunction;
  void* mData;
};

} /* anonymous namespace */

NS_IMETHODIMP
MainFunctionRunnable::Run()
{
  mFunction(mData);
  return NS_OK;
}

nsresult
XRE_InitParentProcess(int aArgc,
                      char* aArgv[],
                      MainFunction aMainFunction,
                      void* aMainFunctionData)
{
  NS_ENSURE_ARG_MIN(aArgc, 1);
  NS_ENSURE_ARG_POINTER(aArgv);
  NS_ENSURE_ARG_POINTER(aArgv[0]);

  // Set main thread before we initialize the profiler
  NS_SetMainThread();

  mozilla::LogModule::Init(aArgc, aArgv);

  AUTO_PROFILER_INIT;

  ScopedXREEmbed embed;

  gArgc = aArgc;
  gArgv = aArgv;
  nsresult rv = XRE_InitCommandLine(gArgc, gArgv);
  if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;

  {
    embed.Start();

    nsCOMPtr<nsIAppShell> appShell(do_GetService(kAppShellCID));
    NS_ENSURE_TRUE(appShell, NS_ERROR_FAILURE);

    if (aMainFunction) {
      nsCOMPtr<nsIRunnable> runnable =
        new MainFunctionRunnable(aMainFunction, aMainFunctionData);
      NS_ENSURE_TRUE(runnable, NS_ERROR_OUT_OF_MEMORY);

      nsresult rv = NS_DispatchToCurrentThread(runnable);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Do event loop
    if (NS_FAILED(appShell->Run())) {
      NS_WARNING("Failed to run appshell");
      return NS_ERROR_FAILURE;
    }
  }

  return XRE_DeinitCommandLine();
}

#ifdef MOZ_IPDL_TESTS
//-----------------------------------------------------------------------------
// IPDL unit test

int
XRE_RunIPDLTest(int aArgc, char** aArgv)
{
    if (aArgc < 2) {
        fprintf(stderr, "TEST-UNEXPECTED-FAIL | <---> | insufficient #args, need at least 2\n");
        return 1;
    }

    void* data = reinterpret_cast<void*>(aArgv[aArgc-1]);

    nsresult rv =
        XRE_InitParentProcess(
            --aArgc, aArgv, mozilla::_ipdltest::IPDLUnitTestMain, data);
    NS_ENSURE_SUCCESS(rv, 1);

    return 0;
}
#endif  // ifdef MOZ_IPDL_TESTS

nsresult
XRE_RunAppShell()
{
    nsCOMPtr<nsIAppShell> appShell(do_GetService(kAppShellCID));
    NS_ENSURE_TRUE(appShell, NS_ERROR_FAILURE);
#if defined(XP_MACOSX)
    if (XRE_UseNativeEventProcessing()) {
      // In content processes that want XPCOM (and hence want
      // AppShell), we usually run our hybrid event loop through
      // MessagePump::Run(), by way of nsBaseAppShell::Run().  The
      // Cocoa nsAppShell impl, however, implements its own Run()
      // that's unaware of MessagePump.  That's all rather suboptimal,
      // but oddly enough not a problem... usually.
      //
      // The problem with this setup comes during startup.
      // XPCOM-in-subprocesses depends on IPC, e.g. to init the pref
      // service, so we have to init IPC first.  But, IPC also
      // indirectly kinda-depends on XPCOM, because MessagePump
      // schedules work from off-main threads (e.g. IO thread) by
      // using NS_DispatchToMainThread().  If the IO thread receives a
      // Message from the parent before nsThreadManager is
      // initialized, then DispatchToMainThread() will fail, although
      // MessagePump will remember the task.  This race condition
      // isn't a problem when appShell->Run() ends up in
      // MessagePump::Run(), because MessagePump will immediate see it
      // has work to do.  It *is* a problem when we end up in [NSApp
      // run], because it's not aware that MessagePump has work that
      // needs to be processed; that was supposed to be signaled by
      // nsIRunnable(s).
      //
      // So instead of hacking Cocoa nsAppShell or rewriting the
      // event-loop system, we compromise here by processing any tasks
      // that might have been enqueued on MessagePump, *before*
      // MessagePump::ScheduleWork was able to successfully
      // DispatchToMainThread().
      MessageLoop* loop = MessageLoop::current();
      bool couldNest = loop->NestableTasksAllowed();

      loop->SetNestableTasksAllowed(true);
      RefPtr<Runnable> task = new MessageLoop::QuitTask();
      loop->PostTask(task.forget());
      loop->Run();

      loop->SetNestableTasksAllowed(couldNest);
    }
#endif  // XP_MACOSX
    return appShell->Run();
}

void
XRE_ShutdownChildProcess()
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  mozilla::DebugOnly<MessageLoop*> ioLoop = XRE_GetIOMessageLoop();
  MOZ_ASSERT(!!ioLoop, "Bad shutdown order");

  Scheduler::Shutdown();

  // Quit() sets off the following chain of events
  //  (1) UI loop starts quitting
  //  (2) UI loop returns from Run() in XRE_InitChildProcess()
  //  (3) ProcessChild goes out of scope and terminates the IO thread
  //  (4) ProcessChild joins the IO thread
  //  (5) exit()
  MessageLoop::current()->Quit();

#if defined(XP_MACOSX)
  nsCOMPtr<nsIAppShell> appShell(do_GetService(kAppShellCID));
  if (appShell) {
      // On Mac, we might be only above nsAppShell::Run(), not
      // MessagePump::Run().  See XRE_RunAppShell(). To account for
      // that case, we fire off an Exit() here.  If we were indeed
      // above MessagePump::Run(), this Exit() is just superfluous.
      appShell->Exit();
  }
#endif // XP_MACOSX
}

namespace {
ContentParent* gContentParent; //long-lived, manually refcounted
TestShellParent* GetOrCreateTestShellParent()
{
    if (!gContentParent) {
        // Use a "web" child process by default.  File a bug if you don't like
        // this and you're sure you wouldn't be better off writing a "browser"
        // chrome mochitest where you can have multiple types of content
        // processes.
        RefPtr<ContentParent> parent =
            ContentParent::GetNewOrUsedBrowserProcess(
		nullptr, NS_LITERAL_STRING(DEFAULT_REMOTE_TYPE));
        parent.forget(&gContentParent);
    } else if (!gContentParent->IsAlive()) {
        return nullptr;
    }
    TestShellParent* tsp = gContentParent->GetTestShellSingleton();
    if (!tsp) {
        tsp = gContentParent->CreateTestShell();
    }
    return tsp;
}

} // namespace

bool
XRE_SendTestShellCommand(JSContext* aCx,
                         JSString* aCommand,
                         JS::Value* aCallback)
{
    JS::RootedString cmd(aCx, aCommand);
    TestShellParent* tsp = GetOrCreateTestShellParent();
    NS_ENSURE_TRUE(tsp, false);

    nsAutoJSString command;
    NS_ENSURE_TRUE(command.init(aCx, cmd), false);

    if (!aCallback) {
        return tsp->SendExecuteCommand(command);
    }

    TestShellCommandParent* callback = static_cast<TestShellCommandParent*>(
        tsp->SendPTestShellCommandConstructor(command));
    NS_ENSURE_TRUE(callback, false);

    NS_ENSURE_TRUE(callback->SetCallback(aCx, *aCallback), false);

    return true;
}

bool
XRE_ShutdownTestShell()
{
    if (!gContentParent) {
        return true;
    }
    bool ret = true;
    if (gContentParent->IsAlive()) {
        ret = gContentParent->DestroyTestShell(
            gContentParent->GetTestShellSingleton());
    }
    NS_RELEASE(gContentParent);
    return ret;
}

#ifdef MOZ_X11
void
XRE_InstallX11ErrorHandler()
{
#ifdef MOZ_WIDGET_GTK
  InstallGdkErrorHandler();
#else
  InstallX11ErrorHandler();
#endif
}
#endif
