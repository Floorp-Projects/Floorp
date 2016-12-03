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
#endif //  defined(MOZ_WIDGET_ANDROID)

#include "mozilla/ipc/BrowserProcessSubThread.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/ipc/IOThreadChild.h"
#include "mozilla/ipc/ProcessChild.h"
#include "ScopedXREEmbed.h"

#include "mozilla/plugins/PluginProcessChild.h"
#include "mozilla/dom/ContentProcess.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentChild.h"

#include "mozilla/ipc/TestShellParent.h"
#include "mozilla/ipc/XPCShellEnvironment.h"
#include "mozilla/WindowsDllBlocklist.h"

#include "GMPProcessChild.h"
#include "GMPLoader.h"
#include "mozilla/gfx/GPUProcessImpl.h"

#include "GeckoProfiler.h"

#include "mozilla/Telemetry.h"

#if defined(MOZ_SANDBOX) && defined(XP_WIN)
#include "mozilla/sandboxTarget.h"
#include "mozilla/sandboxing/loggingCallbacks.h"
#endif

#if defined(MOZ_CONTENT_SANDBOX) && !defined(MOZ_WIDGET_GONK)
#include "mozilla/Preferences.h"
#endif

#if defined(XP_LINUX) && defined(MOZ_GMP_SANDBOX)
#include "mozilla/Sandbox.h"
#include "mozilla/SandboxInfo.h"
#endif

#ifdef MOZ_IPDL_TESTS
#include "mozilla/_ipdltest/IPDLUnitTests.h"
#include "mozilla/_ipdltest/IPDLUnitTestProcessChild.h"

using mozilla::_ipdltest::IPDLUnitTestProcessChild;
#endif  // ifdef MOZ_IPDL_TESTS

#ifdef MOZ_JPROF
#include "jprof.h"
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

using mozilla::gmp::GMPLoader;
using mozilla::gmp::CreateGMPLoader;
using mozilla::gmp::GMPProcessChild;

using mozilla::ipc::TestShellParent;
using mozilla::ipc::TestShellCommandParent;
using mozilla::ipc::XPCShellEnvironment;

using mozilla::startup::sChildProcessType;

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

#ifdef XP_WIN
static const wchar_t kShellLibraryName[] =  L"shell32.dll";
#endif

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
XRE_SetAndroidChildFds (int crashFd, int ipcFd)
{
#if defined(MOZ_CRASHREPORTER)
  CrashReporter::SetNotificationPipeForChild(crashFd);
#endif // defined(MOZ_CRASHREPORTER)
  IPC::Channel::SetClientChannelFd(ipcFd);
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

#if defined(MOZ_CRASHREPORTER)
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
XRE_SetRemoteExceptionHandler(const char* aPipe/*= 0*/)
{
#if defined(XP_WIN) || defined(XP_MACOSX)
  return CrashReporter::SetRemoteExceptionHandler(nsDependentCString(aPipe));
#elif defined(OS_LINUX)
  return CrashReporter::SetRemoteExceptionHandler();
#else
#  error "OOP crash reporter unsupported on this platform"
#endif
}
#endif // if defined(MOZ_CRASHREPORTER)

#if defined(XP_WIN)
void
SetTaskbarGroupId(const nsString& aId)
{
    typedef HRESULT (WINAPI * SetCurrentProcessExplicitAppUserModelIDPtr)(PCWSTR AppID);

    SetCurrentProcessExplicitAppUserModelIDPtr funcAppUserModelID = nullptr;

    HMODULE hDLL = ::LoadLibraryW(kShellLibraryName);

    funcAppUserModelID = (SetCurrentProcessExplicitAppUserModelIDPtr)
                          GetProcAddress(hDLL, "SetCurrentProcessExplicitAppUserModelID");

    if (!funcAppUserModelID) {
        ::FreeLibrary(hDLL);
        return;
    }

    if (FAILED(funcAppUserModelID(aId.get()))) {
        NS_WARNING("SetCurrentProcessExplicitAppUserModelID failed for child process.");
    }

    if (hDLL)
        ::FreeLibrary(hDLL);
}
#endif

#if defined(MOZ_CRASHREPORTER)
#if defined(MOZ_CONTENT_SANDBOX) && !defined(MOZ_WIDGET_GONK)
void
AddContentSandboxLevelAnnotation()
{
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    int level = Preferences::GetInt("security.sandbox.content.level");
    nsAutoCString levelString;
    levelString.AppendInt(level);
    CrashReporter::AnnotateCrashReport(
      NS_LITERAL_CSTRING("ContentSandboxLevel"), levelString);
  }
}
#endif /* MOZ_CONTENT_SANDBOX && !MOZ_WIDGET_GONK */
#endif /* MOZ_CRASHREPORTER */

#if defined (XP_LINUX) && defined(MOZ_GMP_SANDBOX)
namespace {
class LinuxSandboxStarter : public mozilla::gmp::SandboxStarter {
  LinuxSandboxStarter() { }
public:
  static SandboxStarter* Make() {
    if (mozilla::SandboxInfo::Get().CanSandboxMedia()) {
      return new LinuxSandboxStarter();
    } else {
      // Sandboxing isn't possible, but the parent has already
      // checked that this plugin doesn't require it.  (Bug 1074561)
      return nullptr;
    }
    return nullptr;
  }
  virtual bool Start(const char *aLibPath) override {
    mozilla::SetMediaPluginSandbox(aLibPath);
    return true;
  }
};
} // anonymous namespace
#endif // XP_LINUX && MOZ_GMP_SANDBOX

nsresult
XRE_InitChildProcess(int aArgc,
                     char* aArgv[],
                     const XREChildData* aChildData)
{
  NS_ENSURE_ARG_MIN(aArgc, 2);
  NS_ENSURE_ARG_POINTER(aArgv);
  NS_ENSURE_ARG_POINTER(aArgv[0]);
  MOZ_ASSERT(aChildData);

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
    // This has to happen while we're still single-threaded.
    mozilla::SandboxEarlyInit(XRE_GetProcessType());
#endif

#ifdef MOZ_JPROF
  // Call the code to install our handler
  setupProfilingStuff();
#endif

#ifdef XP_LINUX
  // On Fennec, the GMPLoader's code resides inside XUL (because for the time
  // being GMPLoader relies upon NSPR, which we can't use in plugin-container
  // on Android), so we create it here inside XUL and pass it to the GMP code.
  //
  // On desktop Linux, the sandbox code lives in a shared library, and
  // the GMPLoader is in libxul instead of executables to avoid unwanted
  // library dependencies.
  mozilla::gmp::SandboxStarter* starter = nullptr;
#ifdef MOZ_GMP_SANDBOX
  starter = LinuxSandboxStarter::Make();
#endif
  UniquePtr<GMPLoader> loader = CreateGMPLoader(starter);
  GMPProcessChild::SetGMPLoader(loader.get());
#else
  // On non-Linux platforms, the GMPLoader code resides in plugin-container,
  // and we must forward it through to the GMP code here.
  GMPProcessChild::SetGMPLoader(aChildData->gmpLoader.get());
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

  // This is needed by Telemetry to initialize histogram collection.
  // NB: This must be called after NS_LogInit().
  // NS_LogInit must be called before Telemetry::CreateStatisticsRecorder
  // so as to avoid many log messages of the form
  //   WARNING: XPCOM objects created/destroyed from static ctor/dtor: [..]
  // See bug 1279614.
  Telemetry::CreateStatisticsRecorder();

  mozilla::LogModule::Init();

  char aLocal;
  GeckoProfilerInitRAII profiler(&aLocal);

  PROFILER_LABEL("Startup", "XRE_InitChildProcess",
    js::ProfileEntry::Category::OTHER);

  // Complete 'task_t' exchange for Mac OS X. This structure has the same size
  // regardless of architecture so we don't have any cross-arch issues here.
#ifdef XP_MACOSX
  if (aArgc < 1)
    return NS_ERROR_FAILURE;
  const char* const mach_port_name = aArgv[--aArgc];

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

#endif

  SetupErrorHandling(aArgv[0]);  

#if defined(MOZ_CRASHREPORTER)
  if (aArgc < 1)
    return NS_ERROR_FAILURE;
  const char* const crashReporterArg = aArgv[--aArgc];
  
#  if defined(XP_WIN) || defined(XP_MACOSX)
  // on windows and mac, |crashReporterArg| is the named pipe on which the
  // server is listening for requests, or "-" if crash reporting is
  // disabled.
  if (0 != strcmp("-", crashReporterArg) && 
      !XRE_SetRemoteExceptionHandler(crashReporterArg)) {
    // Bug 684322 will add better visibility into this condition
    NS_WARNING("Could not setup crash reporting\n");
  }
#  elif defined(OS_LINUX)
  // on POSIX, |crashReporterArg| is "true" if crash reporting is
  // enabled, false otherwise
  if (0 != strcmp("false", crashReporterArg) && 
      !XRE_SetRemoteExceptionHandler(nullptr)) {
    // Bug 684322 will add better visibility into this condition
    NS_WARNING("Could not setup crash reporting\n");
  }
#  else
#    error "OOP crash reporting unsupported on this platform"
#  endif   
#endif // if defined(MOZ_CRASHREPORTER)

  gArgv = aArgv;
  gArgc = aArgc;

#ifdef MOZ_X11
  XInitThreads();
#endif
#if MOZ_WIDGET_GTK == 2
  XRE_GlibInit();
#endif
#ifdef MOZ_WIDGET_GTK
  // Setting the name here avoids the need to pass this through to gtk_init().
  g_set_prgname(aArgv[0]);
#endif

#ifdef OS_POSIX
  if (PR_GetEnv("MOZ_DEBUG_CHILD_PROCESS") ||
      PR_GetEnv("MOZ_DEBUG_CHILD_PAUSE")) {
    printf_stderr("\n\nCHILDCHILDCHILDCHILD\n  debug me @ %d\n\n",
                  base::GetCurrentProcId());
    sleep(30);
  }
#elif defined(OS_WIN)
  if (PR_GetEnv("MOZ_DEBUG_CHILD_PROCESS")) {
    NS_DebugBreak(NS_DEBUG_BREAK,
                  "Invoking NS_DebugBreak() to debug child process",
                  nullptr, __FILE__, __LINE__);
  } else if (PR_GetEnv("MOZ_DEBUG_CHILD_PAUSE")) {
    printf_stderr("\n\nCHILDCHILDCHILDCHILD\n  debug me @ %d\n\n",
                  base::GetCurrentProcId());
    ::Sleep(10000);
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
      appId.AssignWithConversion(nsDependentCString(appModelUserId));
      // The version string is encased in quotes
      appId.Trim(NS_LITERAL_CSTRING("\"").get());
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
      uiLoopType = MessageLoop::TYPE_DEFAULT;
      break;
  default:
      uiLoopType = MessageLoop::TYPE_UI;
      break;
  }

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

      case GeckoProcessType_Content: {
          process = new ContentProcess(parentPID);
          // If passed in grab the application path for xpcom init
          bool foundAppdir = false;

#if defined(XP_MACOSX) && defined(MOZ_CONTENT_SANDBOX)
          // If passed in grab the profile path for sandboxing
          bool foundProfile = false;
#endif

          for (int idx = aArgc; idx > 0; idx--) {
            if (aArgv[idx] && !strcmp(aArgv[idx], "-appdir")) {
              MOZ_ASSERT(!foundAppdir);
              if (foundAppdir) {
                  continue;
              }
              nsCString appDir;
              appDir.Assign(nsDependentCString(aArgv[idx+1]));
              static_cast<ContentProcess*>(process.get())->SetAppDir(appDir);
              foundAppdir = true;
            }

#if defined(XP_MACOSX) && defined(MOZ_CONTENT_SANDBOX)
            if (aArgv[idx] && !strcmp(aArgv[idx], "-profile")) {
              MOZ_ASSERT(!foundProfile);
              if (foundProfile) {
                continue;
              }
              nsCString profile;
              profile.Assign(nsDependentCString(aArgv[idx+1]));
              static_cast<ContentProcess*>(process.get())->SetProfile(profile);
              foundProfile = true;
            }
            if (foundProfile && foundAppdir) {
              break;
            }
#else
            if (foundAppdir) {
              break;
            }
#endif /* XP_MACOSX && MOZ_CONTENT_SANDBOX */
          }
        }
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

      case GeckoProcessType_GPU:
        process = new gfx::GPUProcessImpl(parentPID);
        break;

      default:
        MOZ_CRASH("Unknown main thread class");
      }

      if (!process->Init()) {
        return NS_ERROR_FAILURE;
      }

#ifdef MOZ_CRASHREPORTER
#if defined(XP_WIN) || defined(XP_MACOSX)
      CrashReporter::InitChildProcessTmpDir();
#endif
#endif

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

      OverrideDefaultLocaleIfNeeded();

#if defined(MOZ_CRASHREPORTER)
#if defined(MOZ_CONTENT_SANDBOX) && !defined(MOZ_WIDGET_GONK)
      AddContentSandboxLevelAnnotation();
#endif
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

  Telemetry::DestroyStatisticsRecorder();
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

  MainFunctionRunnable(MainFunction aFunction,
                       void* aData)
  : mFunction(aFunction),
    mData(aData)
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
    {
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
        RefPtr<ContentParent> parent = ContentParent::GetNewOrUsedBrowserProcess();
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
                         void* aCallback)
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

    JS::Value callbackVal = *reinterpret_cast<JS::Value*>(aCallback);
    NS_ENSURE_TRUE(callback->SetCallback(aCx, callbackVal), false);

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
#if (MOZ_WIDGET_GTK == 3)
  InstallGdkErrorHandler();
#else
  InstallX11ErrorHandler();
#endif
}
#endif
