/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeAppSupportBase.h"
#include "nsNativeAppSupportWin.h"
#include "nsAppRunner.h"
#include "nsContentUtils.h"
#include "nsXULAppAPI.h"
#include "nsString.h"
#include "nsIBrowserDOMWindow.h"
#include "nsCommandLine.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIDOMChromeWindow.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIWindowWatcher.h"
#include "nsPIDOMWindow.h"
#include "nsGlobalWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIBaseWindow.h"
#include "nsIWidget.h"
#include "nsIAppShellService.h"
#include "nsIXULWindow.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPromptService.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "mozilla/Services.h"
#include "nsIFile.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIWebNavigation.h"
#include "nsIWindowMediator.h"
#include "nsNativeCharsetUtils.h"
#include "nsIAppStartup.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/Location.h"

#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <direct.h>
#include <fcntl.h>

using namespace mozilla;

static HWND hwndForDOMWindow(mozIDOMWindowProxy *);

static nsresult GetMostRecentWindow(const char16_t *aType,
                                    mozIDOMWindowProxy **aWindow) {
  nsresult rv;
  nsCOMPtr<nsIWindowMediator> med(
      do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv));
  if (NS_FAILED(rv)) return rv;

  if (med) return med->GetMostRecentWindow(aType, aWindow);

  return NS_ERROR_FAILURE;
}

// Simple Win32 mutex wrapper.
struct Win32Mutex {
  explicit Win32Mutex(const char16_t *name)
      : mName(name), mHandle(0), mState(-1) {
    mHandle = CreateMutexW(0, FALSE, mName.get());
  }
  ~Win32Mutex() {
    if (mHandle) {
      // Make sure we release it if we own it.
      Unlock();

      BOOL rc MOZ_MAYBE_UNUSED = CloseHandle(mHandle);
    }
  }
  BOOL Lock(DWORD timeout) {
    if (mHandle) {
      mState = WaitForSingleObject(mHandle, timeout);
      return mState == WAIT_OBJECT_0 || mState == WAIT_ABANDONED;
    } else {
      return FALSE;
    }
  }
  void Unlock() {
    if (mHandle && mState == WAIT_OBJECT_0) {
      ReleaseMutex(mHandle);
      mState = -1;
    }
  }

 private:
  nsString mName;
  HANDLE mHandle;
  DWORD mState;
};

class MessageWindow;

/*
 * This code implements remoting a command line to an existing instance. It uses
 * a "message window" to detect that Mozilla is already running and to pass
 * the command line of a new instance to the already running instance.
 * We use the mutex semaphore to protect the code that detects whether Mozilla
 * is already running.
 */

class nsNativeAppSupportWin : public nsNativeAppSupportBase,
                              public nsIObserver {
 public:
  NS_DECL_NSIOBSERVER
  NS_DECL_ISUPPORTS_INHERITED

  // Overrides of base implementation.
  NS_IMETHOD Start(bool *aResult) override;
  NS_IMETHOD Quit() override;
  NS_IMETHOD Enable() override;
  // Utility function to handle a Win32-specific command line
  // option: "--console", which dynamically creates a Windows
  // console.
  void CheckConsole();

 private:
  ~nsNativeAppSupportWin() {}
  static void HandleCommandLine(const char *aCmdLineString,
                                nsIFile *aWorkingDir, uint32_t aState);

  static bool mCanHandleRequests;
  static char16_t mMutexName[];
  friend class MessageWindow;
  static MessageWindow *mMsgWindow;
};  // nsNativeAppSupportWin

NS_INTERFACE_MAP_BEGIN(nsNativeAppSupportWin)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END_INHERITING(nsNativeAppSupportBase)

NS_IMPL_ADDREF_INHERITED(nsNativeAppSupportWin, nsNativeAppSupportBase)
NS_IMPL_RELEASE_INHERITED(nsNativeAppSupportWin, nsNativeAppSupportBase)

void UseParentConsole() {
  if (AttachConsole(ATTACH_PARENT_PROCESS)) {
    // Redirect the standard streams to the existing console, but
    // only if they haven't been redirected to a valid file.
    // Visual Studio's _fileno() returns -2 for the standard
    // streams if they aren't associated with an output stream.
    if (_fileno(stdout) == -2) {
      freopen("CONOUT$", "w", stdout);
    }
    // There is no CONERR$, so use CONOUT$ for stderr as well.
    if (_fileno(stderr) == -2) {
      freopen("CONOUT$", "w", stderr);
    }
    if (_fileno(stdin) == -2) {
      freopen("CONIN$", "r", stdin);
    }
  }
}

void nsNativeAppSupportWin::CheckConsole() {
  for (int i = 1; i < gArgc; ++i) {
    if (strcmp("-console", gArgv[i]) == 0 ||
        strcmp("--console", gArgv[i]) == 0 ||
        strcmp("/console", gArgv[i]) == 0) {
      if (AllocConsole()) {
        // Redirect the standard streams to the new console, but
        // only if they haven't been redirected to a valid file.
        // Visual Studio's _fileno() returns -2 for the standard
        // streams if they aren't associated with an output stream.
        if (_fileno(stdout) == -2) {
          freopen("CONOUT$", "w", stdout);
        }
        // There is no CONERR$, so use CONOUT$ for stderr as well.
        if (_fileno(stderr) == -2) {
          freopen("CONOUT$", "w", stderr);
        }
        if (_fileno(stdin) == -2) {
          freopen("CONIN$", "r", stdin);
        }
      }
    } else if (strcmp("-attach-console", gArgv[i]) == 0 ||
               strcmp("--attach-console", gArgv[i]) == 0 ||
               strcmp("/attach-console", gArgv[i]) == 0) {
      UseParentConsole();
    }
  }
}

// Create and return an instance of class nsNativeAppSupportWin.
nsresult NS_CreateNativeAppSupport(nsINativeAppSupport **aResult) {
  nsNativeAppSupportWin *pNative = new nsNativeAppSupportWin;
  if (!pNative) return NS_ERROR_OUT_OF_MEMORY;

  // Check for dynamic console creation request.
  pNative->CheckConsole();

  *aResult = pNative;
  NS_ADDREF(*aResult);

  return NS_OK;
}

// Constants
#define MOZ_MUTEX_NAMESPACE L"Local\\"
#define MOZ_STARTUP_MUTEX_NAME L"StartupMutex"
#define MOZ_MUTEX_START_TIMEOUT 30000

// Static member definitions.
char16_t nsNativeAppSupportWin::mMutexName[128] = {0};
bool nsNativeAppSupportWin::mCanHandleRequests = false;
MessageWindow *nsNativeAppSupportWin::mMsgWindow = nullptr;

// Message window encapsulation.
class MessageWindow final {
 public:
  // ctor/dtor are simplistic
  MessageWindow() {
    // Try to find window.
    mHandle = ::FindWindowW(className(), 0);
  }

  ~MessageWindow() = default;

  // Act like an HWND.
  HWND handle() { return mHandle; }

  // Class name: appName + "MessageWindow"
  static const wchar_t *className() {
    static wchar_t classNameBuffer[128];
    static wchar_t *mClassName = 0;
    if (!mClassName) {
      ::_snwprintf(classNameBuffer,
                   128,  // size of classNameBuffer in PRUnichars
                   L"%s%s",
                   static_cast<const wchar_t *>(
                       NS_ConvertUTF8toUTF16(gAppData->remotingName).get()),
                   L"MessageWindow");
      mClassName = classNameBuffer;
    }
    return mClassName;
  }

  // Create: Register class and create window.
  NS_IMETHOD Create() {
    WNDCLASSW classStruct = {0,                           // style
                             &MessageWindow::WindowProc,  // lpfnWndProc
                             0,                           // cbClsExtra
                             0,                           // cbWndExtra
                             0,                           // hInstance
                             0,                           // hIcon
                             0,                           // hCursor
                             0,                           // hbrBackground
                             0,                           // lpszMenuName
                             className()};                // lpszClassName

    // Register the window class.
    NS_ENSURE_TRUE(::RegisterClassW(&classStruct), NS_ERROR_FAILURE);

    // Create the window.
    NS_ENSURE_TRUE((mHandle = ::CreateWindowW(className(),
                                              0,           // title
                                              WS_CAPTION,  // style
                                              0, 0, 0, 0,  // x, y, cx, cy
                                              0,           // parent
                                              0,           // menu
                                              0,           // instance
                                              0)),         // create struct
                   NS_ERROR_FAILURE);

    return NS_OK;
  }

  // Destory:  Get rid of window and reset mHandle.
  NS_IMETHOD Destroy() {
    nsresult retval = NS_OK;

    if (mHandle) {
      // DestroyWindow can only destroy windows created from
      //  the same thread.
      BOOL desRes = DestroyWindow(mHandle);
      if (FALSE != desRes) {
        mHandle = nullptr;
      } else {
        retval = NS_ERROR_FAILURE;
      }
    }

    return retval;
  }

  // SendRequest: Pass the command line via WM_COPYDATA to message window.
  NS_IMETHOD SendRequest() {
    WCHAR *cmd = ::GetCommandLineW();
    WCHAR cwd[MAX_PATH];
    _wgetcwd(cwd, MAX_PATH);

    // Construct a narrow UTF8 buffer <commandline>\0<workingdir>\0
    NS_ConvertUTF16toUTF8 utf8buffer(cmd);
    utf8buffer.Append('\0');
    WCHAR *cwdPtr = cwd;
    AppendUTF16toUTF8(MakeStringSpan(reinterpret_cast<char16_t *>(cwdPtr)),
                      utf8buffer);
    utf8buffer.Append('\0');

    // We used to set dwData to zero, when we didn't send the working dir.
    // Now we're using it as a version number.
    COPYDATASTRUCT cds = {1, utf8buffer.Length(), (void *)utf8buffer.get()};
    // Bring the already running Mozilla process to the foreground.
    // nsWindow will restore the window (if minimized) and raise it.
    ::SetForegroundWindow(mHandle);
    ::SendMessage(mHandle, WM_COPYDATA, 0, (LPARAM)&cds);
    return NS_OK;
  }

  // Window proc.
  static LRESULT CALLBACK WindowProc(HWND msgWindow, UINT msg, WPARAM wp,
                                     LPARAM lp) {
    if (msg == WM_COPYDATA) {
      if (!nsNativeAppSupportWin::mCanHandleRequests) return FALSE;

      // This is an incoming request.
      COPYDATASTRUCT *cds = (COPYDATASTRUCT *)lp;
      nsCOMPtr<nsIFile> workingDir;

      if (1 >= cds->dwData) {
        char *wdpath = (char *)cds->lpData;
        // skip the command line, and get the working dir of the
        // other process, which is after the first null char
        while (*wdpath) ++wdpath;

        ++wdpath;

        NS_NewLocalFile(NS_ConvertUTF8toUTF16(wdpath), false,
                        getter_AddRefs(workingDir));
      }
      (void)nsNativeAppSupportWin::HandleCommandLine(
          (char *)cds->lpData, workingDir, nsICommandLine::STATE_REMOTE_AUTO);

      // Get current window and return its window handle.
      nsCOMPtr<mozIDOMWindowProxy> win;
      GetMostRecentWindow(0, getter_AddRefs(win));
      return win ? (LRESULT)hwndForDOMWindow(win) : 0;
    }
    return DefWindowProc(msgWindow, msg, wp, lp);
  }

 private:
  HWND mHandle;
};  // class MessageWindow

/* Start: Tries to find the "message window" to determine if it
 *        exists.  If so, then Mozilla is already running.  In that
 *        case, we use the handle to the "message" window and send
 *        a request corresponding to this process's command line
 *        options.
 *
 *        If not, then this is the first instance of Mozilla.  In
 *        that case, we create and set up the message window.
 *
 *        The checking for existence of the message window must
 *        be protected by use of a mutex semaphore.
 */
NS_IMETHODIMP
nsNativeAppSupportWin::Start(bool *aResult) {
  NS_ENSURE_ARG(aResult);
  NS_ENSURE_STATE(gAppData);

  if (getenv("MOZ_NO_REMOTE")) {
    *aResult = true;
    return NS_OK;
  }

  nsresult rv = NS_ERROR_FAILURE;
  *aResult = false;

  // Grab mutex first.

  // Build mutex name from app name.
  ::_snwprintf(
      reinterpret_cast<wchar_t *>(mMutexName),
      sizeof mMutexName / sizeof(char16_t), L"%s%s%s", MOZ_MUTEX_NAMESPACE,
      static_cast<const wchar_t *>(NS_ConvertUTF8toUTF16(gAppData->name).get()),
      MOZ_STARTUP_MUTEX_NAME);
  Win32Mutex startupLock = Win32Mutex(mMutexName);

  NS_ENSURE_TRUE(startupLock.Lock(MOZ_MUTEX_START_TIMEOUT), NS_ERROR_FAILURE);

  // Search for existing message window.
  mMsgWindow = new MessageWindow();
  if (mMsgWindow->handle()) {
    // We are a client process.  Pass request to message window.
    rv = mMsgWindow->SendRequest();
  } else {
    // We will be server.
    rv = mMsgWindow->Create();
    if (NS_SUCCEEDED(rv)) {
      // Tell caller to spin message loop.
      *aResult = true;
    }
  }

  startupLock.Unlock();

  return rv;
}

NS_IMETHODIMP
nsNativeAppSupportWin::Observe(nsISupports *aSubject, const char *aTopic,
                               const char16_t *aData) {
  if (strcmp(aTopic, "quit-application") == 0) {
    Quit();
  } else {
    NS_ERROR("Unexpected observer topic.");
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportWin::Quit() {
  if (!mMsgWindow) {
    return NS_OK;
  }

  // If another process wants to look for the message window, they need
  // to wait to hold the lock, in which case they will not find the
  // window as we will destroy ours under our lock.
  // When the mutex goes off the stack, it is unlocked via destructor.
  Win32Mutex mutexLock(mMutexName);
  NS_ENSURE_TRUE(mutexLock.Lock(MOZ_MUTEX_START_TIMEOUT), NS_ERROR_FAILURE);

  mMsgWindow->Destroy();
  delete mMsgWindow;

  return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportWin::Enable() {
  mCanHandleRequests = true;

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "quit-application", false);
  } else {
    NS_ERROR("No observer service?");
  }

  return NS_OK;
}

void nsNativeAppSupportWin::HandleCommandLine(const char *aCmdLineString,
                                              nsIFile *aWorkingDir,
                                              uint32_t aState) {
  nsresult rv;

  int justCounting = 1;
  char **argv = 0;
  // Flags, etc.
  int init = 1;
  int between, quoted, bSlashCount;
  int argc;
  const char *p;
  nsAutoCString arg;

  nsCOMPtr<nsICommandLineRunner> cmdLine(new nsCommandLine());

  // Parse command line args according to MS spec
  // (see "Parsing C++ Command-Line Arguments" at
  // http://msdn.microsoft.com/library/devprods/vs6/visualc/vclang/_pluslang_parsing_c.2b2b_.command.2d.line_arguments.htm).
  // We loop if we've not finished the second pass through.
  while (1) {
    // Initialize if required.
    if (init) {
      p = aCmdLineString;
      between = 1;
      argc = quoted = bSlashCount = 0;

      init = 0;
    }
    if (between) {
      // We are traversing whitespace between args.
      // Check for start of next arg.
      if (*p != 0 && !isspace(*p)) {
        // Start of another arg.
        between = 0;
        arg = "";
        switch (*p) {
          case '\\':
            // Count the backslash.
            bSlashCount = 1;
            break;
          case '"':
            // Remember we're inside quotes.
            quoted = 1;
            break;
          default:
            // Add character to arg.
            arg += *p;
            break;
        }
      } else {
        // Another space between args, ignore it.
      }
    } else {
      // We are processing the contents of an argument.
      // Check for whitespace or end.
      if (*p == 0 || (!quoted && isspace(*p))) {
        // Process pending backslashes (interpret them
        // literally since they're not followed by a ").
        while (bSlashCount) {
          arg += '\\';
          bSlashCount--;
        }
        // End current arg.
        if (!justCounting) {
          argv[argc] = new char[arg.Length() + 1];
          strcpy(argv[argc], arg.get());
        }
        argc++;
        // We're now between args.
        between = 1;
      } else {
        // Still inside argument, process the character.
        switch (*p) {
          case '"':
            // First, digest preceding backslashes (if any).
            while (bSlashCount > 1) {
              // Put one backsplash in arg for each pair.
              arg += '\\';
              bSlashCount -= 2;
            }
            if (bSlashCount) {
              // Quote is literal.
              arg += '"';
              bSlashCount = 0;
            } else {
              // Quote starts or ends a quoted section.
              if (quoted) {
                // Check for special case of consecutive double
                // quotes inside a quoted section.
                if (*(p + 1) == '"') {
                  // This implies a literal double-quote.  Fake that
                  // out by causing next double-quote to look as
                  // if it was preceded by a backslash.
                  bSlashCount = 1;
                } else {
                  quoted = 0;
                }
              } else {
                quoted = 1;
              }
            }
            break;
          case '\\':
            // Add to count.
            bSlashCount++;
            break;
          default:
            // Accept any preceding backslashes literally.
            while (bSlashCount) {
              arg += '\\';
              bSlashCount--;
            }
            // Just add next char to the current arg.
            arg += *p;
            break;
        }
      }
    }
    // Check for end of input.
    if (*p) {
      // Go to next character.
      p++;
    } else {
      // If on first pass, go on to second.
      if (justCounting) {
        // Allocate argv array.
        argv = new char *[argc];

        // Start second pass
        justCounting = 0;
        init = 1;
      } else {
        // Quit.
        break;
      }
    }
  }

  rv = cmdLine->Init(argc, argv, aWorkingDir, aState);

  // Cleanup.
  while (argc) {
    delete[] argv[--argc];
  }
  delete[] argv;

  if (NS_FAILED(rv)) {
    NS_ERROR("Error initializing command line.");
    return;
  }

  cmdLine->Run();
}

HWND hwndForDOMWindow(mozIDOMWindowProxy *window) {
  if (!window) {
    return 0;
  }
  nsCOMPtr<nsPIDOMWindowOuter> pidomwindow = nsPIDOMWindowOuter::From(window);

  nsCOMPtr<nsIBaseWindow> ppBaseWindow =
      do_QueryInterface(pidomwindow->GetDocShell());
  if (!ppBaseWindow) {
    return 0;
  }

  nsCOMPtr<nsIWidget> ppWidget;
  ppBaseWindow->GetMainWidget(getter_AddRefs(ppWidget));

  return (HWND)(ppWidget->GetNativeData(NS_NATIVE_WIDGET));
}
