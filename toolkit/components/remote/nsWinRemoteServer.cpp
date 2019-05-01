/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWinRemoteServer.h"
#include "nsWinRemoteUtils.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIDOMChromeWindow.h"
#include "nsXPCOM.h"
#include "nsPIDOMWindow.h"
#include "nsIWindowMediator.h"
#include "nsIBaseWindow.h"
#include "nsIWidget.h"
#include "nsICommandLineRunner.h"
#include "nsICommandLine.h"
#include "nsCommandLine.h"
#include "nsIDocShell.h"

HWND hwndForDOMWindow(mozIDOMWindowProxy* window) {
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

static nsresult GetMostRecentWindow(mozIDOMWindowProxy** aWindow) {
  nsresult rv;
  nsCOMPtr<nsIWindowMediator> med(
      do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv));
  if (NS_FAILED(rv)) return rv;

  if (med) return med->GetMostRecentWindow(nullptr, aWindow);

  return NS_ERROR_FAILURE;
}

void HandleCommandLine(const char* aCmdLineString, nsIFile* aWorkingDir,
                       uint32_t aState) {
  nsresult rv;

  int justCounting = 1;
  char** argv = 0;
  // Flags, etc.
  int init = 1;
  int between, quoted, bSlashCount;
  int argc;
  const char* p;
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
        argv = new char*[argc];

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

LRESULT CALLBACK WindowProc(HWND msgWindow, UINT msg, WPARAM wp, LPARAM lp) {
  if (msg == WM_COPYDATA) {
    // This is an incoming request.
    COPYDATASTRUCT* cds = (COPYDATASTRUCT*)lp;
    nsCOMPtr<nsIFile> workingDir;

    if (1 >= cds->dwData) {
      char* wdpath = (char*)cds->lpData;
      // skip the command line, and get the working dir of the
      // other process, which is after the first null char
      while (*wdpath) ++wdpath;

      ++wdpath;

      NS_NewLocalFile(NS_ConvertUTF8toUTF16(wdpath), false,
                      getter_AddRefs(workingDir));
    }
    HandleCommandLine((char*)cds->lpData, workingDir,
                      nsICommandLine::STATE_REMOTE_AUTO);

    // Get current window and return its window handle.
    nsCOMPtr<mozIDOMWindowProxy> win;
    GetMostRecentWindow(getter_AddRefs(win));
    return win ? (LRESULT)hwndForDOMWindow(win) : 0;
  }
  return DefWindowProc(msgWindow, msg, wp, lp);
}

nsresult nsWinRemoteServer::Startup(const char* aAppName,
                                    const char* aProfileName) {
  nsString className;
  BuildClassName(aAppName, aProfileName, className);

  WNDCLASSW classStruct = {0,                 // style
                           &WindowProc,       // lpfnWndProc
                           0,                 // cbClsExtra
                           0,                 // cbWndExtra
                           0,                 // hInstance
                           0,                 // hIcon
                           0,                 // hCursor
                           0,                 // hbrBackground
                           0,                 // lpszMenuName
                           className.get()};  // lpszClassName

  // Register the window class.
  NS_ENSURE_TRUE(::RegisterClassW(&classStruct), NS_ERROR_FAILURE);

  // Create the window.
  mHandle = ::CreateWindowW(className.get(),
                            0,           // title
                            WS_CAPTION,  // style
                            0, 0, 0, 0,  // x, y, cx, cy
                            0,           // parent
                            0,           // menu
                            0,           // instance
                            0);          // create struct

  return mHandle ? NS_OK : NS_ERROR_FAILURE;
}

void nsWinRemoteServer::Shutdown() {
  DestroyWindow(mHandle);
  mHandle = nullptr;
}
