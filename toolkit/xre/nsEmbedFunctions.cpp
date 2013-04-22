/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#if defined(MOZ_WIDGET_QT)
#include "nsQAppInstance.h"
#endif

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

#if defined(OS_LINUX)
#  define XP_LINUX
#endif

#ifdef XP_WIN
#include <process.h>
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
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "chrome/common/child_process.h"
#include "chrome/common/notification_service.h"

#include "mozilla/ipc/BrowserProcessSubThread.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/ipc/IOThreadChild.h"
#include "mozilla/ipc/ProcessChild.h"
#include "ScopedXREEmbed.h"

#include "mozilla/plugins/PluginProcessChild.h"
#include "mozilla/dom/ContentProcess.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentChild.h"

#include "mozilla/jsipc/ContextWrapperParent.h"

#include "mozilla/ipc/TestShellParent.h"
#include "mozilla/ipc/XPCShellEnvironment.h"

#include "GeckoProfiler.h"

#ifdef MOZ_IPDL_TESTS
#include "mozilla/_ipdltest/IPDLUnitTests.h"
#include "mozilla/_ipdltest/IPDLUnitTestProcessChild.h"

using mozilla::_ipdltest::IPDLUnitTestProcessChild;
#endif  // ifdef MOZ_IPDL_TESTS

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

using mozilla::jsipc::PContextWrapperParent;
using mozilla::jsipc::ContextWrapperParent;

using mozilla::ipc::TestShellParent;
using mozilla::ipc::TestShellCommandParent;
using mozilla::ipc::XPCShellEnvironment;

using mozilla::startup::sChildProcessType;

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

#ifdef XP_WIN
static const PRUnichar kShellLibraryName[] =  L"shell32.dll";
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
    kGeckoProcessTypeString[aProcessType] : nullptr;
}

GeckoProcessType
XRE_StringToChildProcessType(const char* aProcessTypeString)
{
  for (int i = 0;
       i < (int) ArrayLength(kGeckoProcessTypeString);
       ++i) {
    if (!strcmp(kGeckoProcessTypeString[i], aProcessTypeString)) {
      return static_cast<GeckoProcessType>(i);
    }
  }
  return GeckoProcessType_Invalid;
}

namespace mozilla {
namespace startup {
GeckoProcessType sChildProcessType = GeckoProcessType_Default;
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

nsresult
XRE_InitChildProcess(int aArgc,
                     char* aArgv[],
                     GeckoProcessType aProcess)
{
  NS_ENSURE_ARG_MIN(aArgc, 2);
  NS_ENSURE_ARG_POINTER(aArgv);
  NS_ENSURE_ARG_POINTER(aArgv[0]);
  profiler_init();
  PROFILER_LABEL("Startup", "XRE_InitChildProcess");

  sChildProcessType = aProcess;

  // Complete 'task_t' exchange for Mac OS X. This structure has the same size
  // regardless of architecture so we don't have any cross-arch issues here.
#ifdef XP_MACOSX
  if (aArgc < 1)
    return NS_ERROR_FAILURE;
  const char* const mach_port_name = aArgv[--aArgc];

  const int kTimeoutMs = 1000;

  MachSendMessage child_message(0);
  if (!child_message.AddDescriptor(mach_task_self())) {
    NS_WARNING("child AddDescriptor(mach_task_self()) failed.");
    return NS_ERROR_FAILURE;
  }

  ReceivePort child_recv_port;
  mach_port_t raw_child_recv_port = child_recv_port.GetPort();
  if (!child_message.AddDescriptor(raw_child_recv_port)) {
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
      !XRE_SetRemoteExceptionHandler(NULL)) {
    // Bug 684322 will add better visibility into this condition
    NS_WARNING("Could not setup crash reporting\n");
  }
#  else
#    error "OOP crash reporting unsupported on this platform"
#  endif   
#endif // if defined(MOZ_CRASHREPORTER)

  gArgv = aArgv;
  gArgc = aArgc;

#if defined(MOZ_WIDGET_GTK)
  g_thread_init(NULL);
#endif

#if defined(MOZ_WIDGET_QT)
  nsQAppInstance::AddRef();
#endif

  if (PR_GetEnv("MOZ_DEBUG_CHILD_PROCESS")) {
#ifdef OS_POSIX
      printf("\n\nCHILDCHILDCHILDCHILD\n  debug me @%d\n\n", getpid());
      sleep(30);
#elif defined(OS_WIN)
      printf("\n\nCHILDCHILDCHILDCHILD\n  debug me @%d\n\n", _getpid());
      Sleep(30000);
#endif
  }

  // child processes launched by GeckoChildProcessHost get this magic
  // argument appended to their command lines
  const char* const parentPIDString = aArgv[aArgc-1];
  NS_ABORT_IF_FALSE(parentPIDString, "NULL parent PID");
  --aArgc;

  char* end = 0;
  base::ProcessId parentPID = strtol(parentPIDString, &end, 10);
  NS_ABORT_IF_FALSE(!*end, "invalid parent PID");

  base::ProcessHandle parentHandle;
  mozilla::DebugOnly<bool> ok = base::OpenProcessHandle(parentPID, &parentHandle);
  NS_ABORT_IF_FALSE(ok, "can't open handle to parent");

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
  NotificationService notificationService;

  NS_LogInit();

  nsresult rv = XRE_InitCommandLine(aArgc, aArgv);
  if (NS_FAILED(rv)) {
    profiler_shutdown();
    NS_LogTerm();
    return NS_ERROR_FAILURE;
  }

  MessageLoop::Type uiLoopType;
  switch (aProcess) {
  case GeckoProcessType_Content:
      // Content processes need the XPCOM/chromium frankenventloop
      uiLoopType = MessageLoop::TYPE_MOZILLA_CHILD;
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

      switch (aProcess) {
      case GeckoProcessType_Default:
        NS_RUNTIMEABORT("This makes no sense");
        break;

      case GeckoProcessType_Plugin:
        process = new PluginProcessChild(parentHandle);
        break;

      case GeckoProcessType_Content: {
          process = new ContentProcess(parentHandle);
          // If passed in grab the application path for xpcom init
          nsCString appDir;
          for (int idx = aArgc; idx > 0; idx--) {
            if (aArgv[idx] && !strcmp(aArgv[idx], "-appdir")) {
              appDir.Assign(nsDependentCString(aArgv[idx+1]));
              static_cast<ContentProcess*>(process.get())->SetAppDir(appDir);
              break;
            }
          }
        }
        break;

      case GeckoProcessType_IPDLUnitTest:
#ifdef MOZ_IPDL_TESTS
        process = new IPDLUnitTestProcessChild(parentHandle);
#else 
        NS_RUNTIMEABORT("rebuild with --enable-ipdl-tests");
#endif
        break;

      default:
        NS_RUNTIMEABORT("Unknown main thread class");
      }

      if (!process->Init()) {
        profiler_shutdown();
        NS_LogTerm();
        return NS_ERROR_FAILURE;
      }

      // Run the UI event loop on the main thread.
      uiMessageLoop.MessageLoop::Run();

      // Allow ProcessChild to clean up after itself before going out of
      // scope and being deleted
      process->CleanUp();
      mozilla::Omnijar::CleanUp();
    }
  }

  profiler_shutdown();
  NS_LogTerm();
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

class MainFunctionRunnable : public nsRunnable
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
      loop->PostTask(FROM_HERE, new MessageLoop::QuitTask());
      loop->Run();

      loop->SetNestableTasksAllowed(couldNest);
    }
#endif  // XP_MACOSX
    return appShell->Run();
}

template<>
struct RunnableMethodTraits<ContentChild>
{
    static void RetainCallee(ContentChild* obj) { }
    static void ReleaseCallee(ContentChild* obj) { }
};

void
XRE_ShutdownChildProcess()
{
  NS_ABORT_IF_FALSE(MessageLoopForUI::current(), "Wrong thread!");

  mozilla::DebugOnly<MessageLoop*> ioLoop = XRE_GetIOMessageLoop();
  NS_ABORT_IF_FALSE(!!ioLoop, "Bad shutdown order");

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
        nsRefPtr<ContentParent> parent = ContentParent::GetNewOrUsed().get();
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
}

bool
XRE_SendTestShellCommand(JSContext* aCx,
                         JSString* aCommand,
                         void* aCallback)
{
    TestShellParent* tsp = GetOrCreateTestShellParent();
    NS_ENSURE_TRUE(tsp, false);

    nsDependentJSString command;
    NS_ENSURE_TRUE(command.init(aCx, aCommand), false);

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
XRE_GetChildGlobalObject(JSContext* aCx, JSObject** aGlobalP)
{
    TestShellParent* tsp = GetOrCreateTestShellParent();
    return tsp && tsp->GetGlobalJSObject(aCx, aGlobalP);
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
  InstallX11ErrorHandler();
}
#endif

#ifdef XP_WIN
static WindowsEnvironmentType
sWindowsEnvironmentType = WindowsEnvironmentType_Desktop;

void
SetWindowsEnvironment(WindowsEnvironmentType aEnvID)
{
  sWindowsEnvironmentType = aEnvID;
}

WindowsEnvironmentType
XRE_GetWindowsEnvironment()
{
  return sWindowsEnvironmentType;
}
#endif // XP_WIN

