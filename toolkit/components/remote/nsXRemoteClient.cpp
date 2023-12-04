/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=8:
 */
/* vim:set ts=8 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDebug.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Unused.h"
#include "nsXRemoteClient.h"
#include "RemoteUtils.h"
#include "prsystem.h"
#include "mozilla/Logging.h"
#include "prenv.h"
#include "prdtoa.h"
#include <X11/Xatom.h>
#include <limits.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define MOZILLA_VERSION_PROP "_MOZILLA_VERSION"
#define MOZILLA_LOCK_PROP "_MOZILLA_LOCK"
#define MOZILLA_COMMANDLINE_PROP "_MOZILLA_COMMANDLINE"
#define MOZILLA_RESPONSE_PROP "_MOZILLA_RESPONSE"
#define MOZILLA_USER_PROP "_MOZILLA_USER"
#define MOZILLA_PROFILE_PROP "_MOZILLA_PROFILE"
#define MOZILLA_PROGRAM_PROP "_MOZILLA_PROGRAM"

#ifdef IS_BIG_ENDIAN
#  define TO_LITTLE_ENDIAN32(x)                               \
    ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >> 8) | \
     (((x) & 0x0000ff00) << 8) | (((x) & 0x000000ff) << 24))
#else
#  define TO_LITTLE_ENDIAN32(x) (x)
#endif

#ifndef MAX_PATH
#  ifdef PATH_MAX
#    define MAX_PATH PATH_MAX
#  else
#    define MAX_PATH 1024
#  endif
#endif

using mozilla::LogLevel;
using mozilla::Unused;

static mozilla::LazyLogModule sRemoteLm("nsXRemoteClient");

static int (*sOldHandler)(Display*, XErrorEvent*);
static bool sGotBadWindow;

nsXRemoteClient::nsXRemoteClient() {
  mDisplay = 0;
  mInitialized = false;
  mMozVersionAtom = 0;
  mMozLockAtom = 0;
  mMozCommandLineAtom = 0;
  mMozResponseAtom = 0;
  mMozWMStateAtom = 0;
  mMozUserAtom = 0;
  mMozProfileAtom = 0;
  mMozProgramAtom = 0;
  mLockData = 0;
  MOZ_LOG(sRemoteLm, LogLevel::Debug, ("nsXRemoteClient::nsXRemoteClient"));
}

nsXRemoteClient::~nsXRemoteClient() {
  MOZ_LOG(sRemoteLm, LogLevel::Debug, ("nsXRemoteClient::~nsXRemoteClient"));
  if (mInitialized) Shutdown();
}

// Minimize the roundtrips to the X-server
static const char* XAtomNames[] = {
    MOZILLA_VERSION_PROP, MOZILLA_LOCK_PROP,       MOZILLA_RESPONSE_PROP,
    "WM_STATE",           MOZILLA_USER_PROP,       MOZILLA_PROFILE_PROP,
    MOZILLA_PROGRAM_PROP, MOZILLA_COMMANDLINE_PROP};
static Atom XAtoms[MOZ_ARRAY_LENGTH(XAtomNames)];

nsresult nsXRemoteClient::Init() {
  MOZ_LOG(sRemoteLm, LogLevel::Debug, ("nsXRemoteClient::Init"));

  if (mInitialized) return NS_OK;

  // try to open the display
  mDisplay = XOpenDisplay(0);
  if (!mDisplay) return NS_ERROR_FAILURE;

  // get our atoms
  XInternAtoms(mDisplay, const_cast<char**>(XAtomNames),
               MOZ_ARRAY_LENGTH(XAtomNames), False, XAtoms);

  int i = 0;
  mMozVersionAtom = XAtoms[i++];
  mMozLockAtom = XAtoms[i++];
  mMozResponseAtom = XAtoms[i++];
  mMozWMStateAtom = XAtoms[i++];
  mMozUserAtom = XAtoms[i++];
  mMozProfileAtom = XAtoms[i++];
  mMozProgramAtom = XAtoms[i++];
  mMozCommandLineAtom = XAtoms[i];

  mInitialized = true;

  return NS_OK;
}

void nsXRemoteClient::Shutdown(void) {
  MOZ_LOG(sRemoteLm, LogLevel::Debug, ("nsXRemoteClient::Shutdown"));

  if (!mInitialized) return;

  // shut everything down
  XCloseDisplay(mDisplay);
  mDisplay = 0;
  mInitialized = false;
  if (mLockData) {
    free(mLockData);
    mLockData = 0;
  }
}

static int HandleBadWindow(Display* display, XErrorEvent* event) {
  if (event->error_code == BadWindow) {
    sGotBadWindow = true;
    return 0;  // ignored
  }

  return (*sOldHandler)(display, event);
}

nsresult nsXRemoteClient::SendCommandLine(
    const char* aProgram, const char* aProfile, int32_t argc, char** argv,
    const char* aStartupToken, char** aResponse, bool* aWindowFound) {
  NS_ENSURE_TRUE(aProgram, NS_ERROR_INVALID_ARG);

  MOZ_LOG(sRemoteLm, LogLevel::Debug, ("nsXRemoteClient::SendCommandLine"));

  *aWindowFound = false;

  // FindBestWindow() iterates down the window hierarchy, so catch X errors
  // when windows get destroyed before being accessed.
  sOldHandler = XSetErrorHandler(HandleBadWindow);

  Window w = FindBestWindow(aProgram, aProfile);

  nsresult rv = NS_OK;

  if (w) {
    // ok, let the caller know that we at least found a window.
    *aWindowFound = true;

    // Ignore BadWindow errors up to this point.  The last request from
    // FindBestWindow() was a synchronous XGetWindowProperty(), so no need to
    // Sync.  Leave the error handler installed to detect if w gets destroyed.
    sGotBadWindow = false;

    // make sure we get the right events on that window
    XSelectInput(mDisplay, w, (PropertyChangeMask | StructureNotifyMask));

    bool destroyed = false;

    // get the lock on the window
    rv = GetLock(w, &destroyed);

    if (NS_SUCCEEDED(rv)) {
      // send our command
      rv = DoSendCommandLine(w, argc, argv, aStartupToken, aResponse,
                             &destroyed);

      // if the window was destroyed, don't bother trying to free the
      // lock.
      if (!destroyed) FreeLock(w);  // doesn't really matter what this returns
    }
  }

  XSetErrorHandler(sOldHandler);

  MOZ_LOG(sRemoteLm, LogLevel::Debug,
          ("SendCommandInternal returning 0x%" PRIx32 "\n",
           static_cast<uint32_t>(rv)));

  return rv;
}

Window nsXRemoteClient::CheckWindow(Window aWindow) {
  Atom type = None;
  int format;
  unsigned long nitems, bytesafter;
  unsigned char* data;
  Window innerWindow;

  XGetWindowProperty(mDisplay, aWindow, mMozWMStateAtom, 0, 0, False,
                     AnyPropertyType, &type, &format, &nitems, &bytesafter,
                     &data);

  if (type) {
    XFree(data);
    return aWindow;
  }

  // didn't find it here so check the children of this window
  innerWindow = CheckChildren(aWindow);

  if (innerWindow) return innerWindow;

  return aWindow;
}

Window nsXRemoteClient::CheckChildren(Window aWindow) {
  Window root, parent;
  Window* children;
  unsigned int nchildren;
  unsigned int i;
  Atom type = None;
  int format;
  unsigned long nitems, after;
  unsigned char* data;
  Window retval = None;

  if (!XQueryTree(mDisplay, aWindow, &root, &parent, &children, &nchildren))
    return None;

  // scan the list first before recursing into the list of windows
  // which can get quite deep.
  for (i = 0; !retval && (i < nchildren); i++) {
    XGetWindowProperty(mDisplay, children[i], mMozWMStateAtom, 0, 0, False,
                       AnyPropertyType, &type, &format, &nitems, &after, &data);
    if (type) {
      XFree(data);
      retval = children[i];
    }
  }

  // otherwise recurse into the list
  for (i = 0; !retval && (i < nchildren); i++) {
    retval = CheckChildren(children[i]);
  }

  if (children) XFree((char*)children);

  return retval;
}

nsresult nsXRemoteClient::GetLock(Window aWindow, bool* aDestroyed) {
  bool locked = false;
  bool waited = false;
  *aDestroyed = false;

  nsresult rv = NS_OK;

  if (!mLockData) {
    char pidstr[32];
    char sysinfobuf[SYS_INFO_BUFFER_LENGTH];
    SprintfLiteral(pidstr, "pid%d@", getpid());
    PRStatus status;
    status =
        PR_GetSystemInfo(PR_SI_HOSTNAME, sysinfobuf, SYS_INFO_BUFFER_LENGTH);
    if (status != PR_SUCCESS) {
      return NS_ERROR_FAILURE;
    }

    // allocate enough space for the string plus the terminating
    // char
    mLockData = (char*)malloc(strlen(pidstr) + strlen(sysinfobuf) + 1);
    if (!mLockData) return NS_ERROR_OUT_OF_MEMORY;

    strcpy(mLockData, pidstr);
    if (!strcat(mLockData, sysinfobuf)) return NS_ERROR_FAILURE;
  }

  do {
    int result;
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char* data = 0;

    XGrabServer(mDisplay);

    result = XGetWindowProperty(
        mDisplay, aWindow, mMozLockAtom, 0, (65536 / sizeof(long)),
        False, /* don't delete */
        XA_STRING, &actual_type, &actual_format, &nitems, &bytes_after, &data);

    // aWindow may have been destroyed before XSelectInput was processed, in
    // which case there may not be any DestroyNotify event in the queue to
    // tell us.  XGetWindowProperty() was synchronous so error responses have
    // now been processed, setting sGotBadWindow.
    if (sGotBadWindow) {
      *aDestroyed = true;
      rv = NS_ERROR_FAILURE;
    } else if (result != Success || actual_type == None) {
      /* It's not now locked - lock it. */
      XChangeProperty(mDisplay, aWindow, mMozLockAtom, XA_STRING, 8,
                      PropModeReplace, (unsigned char*)mLockData,
                      strlen(mLockData));
      locked = True;
    }

    XUngrabServer(mDisplay);
    XFlush(mDisplay);  // ungrab now!

    if (!locked && !NS_FAILED(rv)) {
      /* We tried to grab the lock this time, and failed because someone
         else is holding it already.  So, wait for a PropertyDelete event
         to come in, and try again. */
      MOZ_LOG(sRemoteLm, LogLevel::Debug,
              ("window 0x%x is locked by %s; waiting...\n",
               (unsigned int)aWindow, data));
      waited = True;
      while (true) {
        XEvent event;
        int poll_retval;
        struct pollfd pfd;

        pfd.fd = ConnectionNumber(mDisplay);
        pfd.events = POLLIN;

        poll_retval = poll(&pfd, 1, 10 * 1000);
        // did we time out?
        if (poll_retval == 0) {
          MOZ_LOG(sRemoteLm, LogLevel::Debug,
                  ("timed out waiting for window\n"));
          rv = NS_ERROR_FAILURE;
          break;
        }
        MOZ_LOG(sRemoteLm, LogLevel::Debug, ("xevent...\n"));
        // FIXME check the return value from this?
        XNextEvent(mDisplay, &event);
        if (event.xany.type == DestroyNotify &&
            event.xdestroywindow.window == aWindow) {
          *aDestroyed = true;
          rv = NS_ERROR_FAILURE;
          break;
        }
        if (event.xany.type == PropertyNotify &&
            event.xproperty.state == PropertyDelete &&
            event.xproperty.window == aWindow &&
            event.xproperty.atom == mMozLockAtom) {
          /* Ok!  Someone deleted their lock, so now we can try
             again. */
          MOZ_LOG(
              sRemoteLm, LogLevel::Debug,
              ("(0x%x unlocked, trying again...)\n", (unsigned int)aWindow));
          break;
        }
      }
    }
    if (data) XFree(data);
  } while (!locked && !NS_FAILED(rv));

  if (waited && locked) {
    MOZ_LOG(sRemoteLm, LogLevel::Debug, ("obtained lock.\n"));
  } else if (*aDestroyed) {
    MOZ_LOG(sRemoteLm, LogLevel::Debug,
            ("window 0x%x unexpectedly destroyed.\n", (unsigned int)aWindow));
  }

  return rv;
}

Window nsXRemoteClient::FindBestWindow(const char* aProgram,
                                       const char* aProfile) {
  Window root = RootWindowOfScreen(DefaultScreenOfDisplay(mDisplay));
  Window bestWindow = 0;
  Window root2, parent, *kids;
  unsigned int nkids;

  // Get a list of the children of the root window, walk the list
  // looking for the best window that fits the criteria.
  if (!XQueryTree(mDisplay, root, &root2, &parent, &kids, &nkids)) {
    MOZ_LOG(sRemoteLm, LogLevel::Debug,
            ("XQueryTree failed in nsXRemoteClient::FindBestWindow"));
    return 0;
  }

  if (!(kids && nkids)) {
    MOZ_LOG(sRemoteLm, LogLevel::Debug, ("root window has no children"));
    return 0;
  }

  // We'll walk the list of windows looking for a window that best
  // fits the criteria here.

  for (unsigned int i = 0; i < nkids; i++) {
    Atom type;
    int format;
    unsigned long nitems, bytesafter;
    unsigned char* data_return = 0;
    Window w;
    w = kids[i];
    // find the inner window with WM_STATE on it
    w = CheckWindow(w);

    int status = XGetWindowProperty(
        mDisplay, w, mMozVersionAtom, 0, (65536 / sizeof(long)), False,
        XA_STRING, &type, &format, &nitems, &bytesafter, &data_return);

    if (!data_return) continue;

    double version = PR_strtod((char*)data_return, nullptr);
    XFree(data_return);

    if (!(version >= 5.1 && version < 6)) continue;

    data_return = 0;

    if (status != Success || type == None) continue;

    // Check that this window is from the right program.
    Unused << XGetWindowProperty(
        mDisplay, w, mMozProgramAtom, 0, (65536 / sizeof(long)), False,
        XA_STRING, &type, &format, &nitems, &bytesafter, &data_return);

    // If the return name is not the same as this program name, we don't want
    // this window.
    if (data_return) {
      if (strcmp(aProgram, (const char*)data_return)) {
        XFree(data_return);
        continue;
      }

      // This is actually the success condition.
      XFree(data_return);
    } else {
      // Doesn't support the protocol, even though the user
      // requested it.  So we're not going to use this window.
      continue;
    }

    // Check to see if it has the user atom on that window.  If there
    // is then we need to make sure that it matches what we have.
    const char* username = PR_GetEnv("LOGNAME");

    if (username) {
      Unused << XGetWindowProperty(
          mDisplay, w, mMozUserAtom, 0, (65536 / sizeof(long)), False,
          XA_STRING, &type, &format, &nitems, &bytesafter, &data_return);

      // if there's a username compare it with what we have
      if (data_return) {
        // If the IDs aren't equal, we don't want this window.
        if (strcmp(username, (const char*)data_return)) {
          XFree(data_return);
          continue;
        }

        XFree(data_return);
      }
    }

    // Check to see if there's a profile name on this window.  If
    // there is, then we need to make sure it matches what someone
    // passed in.
    Unused << XGetWindowProperty(
        mDisplay, w, mMozProfileAtom, 0, (65536 / sizeof(long)), False,
        XA_STRING, &type, &format, &nitems, &bytesafter, &data_return);

    // If there's a profile compare it with what we have
    if (data_return) {
      // If the profiles aren't equal, we don't want this window.
      if (strcmp(aProfile, (const char*)data_return)) {
        XFree(data_return);
        continue;
      }

      XFree(data_return);
    } else {
      // This isn't the window for this profile.
      continue;
    }

    // Check to see if the window supports the new command-line passing
    // protocol, if that is requested.

    // If we got this far, this is the best window.  It passed
    // all the tests.
    bestWindow = w;
    break;
  }

  if (kids) XFree((char*)kids);

  return bestWindow;
}

nsresult nsXRemoteClient::FreeLock(Window aWindow) {
  int result;
  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char* data = 0;

  result = XGetWindowProperty(
      mDisplay, aWindow, mMozLockAtom, 0, (65536 / sizeof(long)),
      True, /* atomic delete after */
      XA_STRING, &actual_type, &actual_format, &nitems, &bytes_after, &data);
  if (result != Success) {
    MOZ_LOG(sRemoteLm, LogLevel::Debug,
            ("unable to read and delete " MOZILLA_LOCK_PROP " property\n"));
    return NS_ERROR_FAILURE;
  }
  if (!data || !*data) {
    MOZ_LOG(sRemoteLm, LogLevel::Debug,
            ("invalid data on " MOZILLA_LOCK_PROP " of window 0x%x.\n",
             (unsigned int)aWindow));
    return NS_ERROR_FAILURE;
  } else if (strcmp((char*)data, mLockData)) {
    MOZ_LOG(sRemoteLm, LogLevel::Debug,
            (MOZILLA_LOCK_PROP " was stolen!  Expected \"%s\", saw \"%s\"!\n",
             mLockData, data));
    return NS_ERROR_FAILURE;
  }

  if (data) XFree(data);
  return NS_OK;
}

nsresult nsXRemoteClient::DoSendCommandLine(Window aWindow, int32_t argc,
                                            char** argv,
                                            const char* aStartupToken,
                                            char** aResponse,
                                            bool* aDestroyed) {
  *aDestroyed = false;

  int commandLineLength;
  char* commandLine =
      ConstructCommandLine(argc, argv, aStartupToken, &commandLineLength);
  XChangeProperty(mDisplay, aWindow, mMozCommandLineAtom, XA_STRING, 8,
                  PropModeReplace, (unsigned char*)commandLine,
                  commandLineLength);
  free(commandLine);

  if (!WaitForResponse(aWindow, aResponse, aDestroyed, mMozCommandLineAtom))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

bool nsXRemoteClient::WaitForResponse(Window aWindow, char** aResponse,
                                      bool* aDestroyed, Atom aCommandAtom) {
  bool done = false;
  bool accepted = false;

  while (!done) {
    XEvent event;
    XNextEvent(mDisplay, &event);
    if (event.xany.type == DestroyNotify &&
        event.xdestroywindow.window == aWindow) {
      /* Print to warn user...*/
      MOZ_LOG(sRemoteLm, LogLevel::Debug,
              ("window 0x%x was destroyed.\n", (unsigned int)aWindow));
      *aResponse = strdup("Window was destroyed while reading response.");
      *aDestroyed = true;
      return false;
    }
    if (event.xany.type == PropertyNotify &&
        event.xproperty.state == PropertyNewValue &&
        event.xproperty.window == aWindow &&
        event.xproperty.atom == mMozResponseAtom) {
      Atom actual_type;
      int actual_format;
      unsigned long nitems, bytes_after;
      unsigned char* data = 0;
      Bool result;
      result = XGetWindowProperty(mDisplay, aWindow, mMozResponseAtom, 0,
                                  (65536 / sizeof(long)),
                                  True, /* atomic delete after */
                                  XA_STRING, &actual_type, &actual_format,
                                  &nitems, &bytes_after, &data);
      if (result != Success) {
        MOZ_LOG(
            sRemoteLm, LogLevel::Debug,
            ("failed reading " MOZILLA_RESPONSE_PROP " from window 0x%0x.\n",
             (unsigned int)aWindow));
        *aResponse = strdup("Internal error reading response from window.");
        done = true;
      } else if (!data || strlen((char*)data) < 5) {
        MOZ_LOG(sRemoteLm, LogLevel::Debug,
                ("invalid data on " MOZILLA_RESPONSE_PROP
                 " property of window 0x%0x.\n",
                 (unsigned int)aWindow));
        *aResponse = strdup("Server returned invalid data in response.");
        done = true;
      } else if (*data == '1') { /* positive preliminary reply */
        MOZ_LOG(sRemoteLm, LogLevel::Debug, ("%s\n", data + 4));
        /* keep going */
        done = false;
      }

      else if (!strncmp((char*)data, "200", 3)) { /* positive completion */
        *aResponse = strdup((char*)data);
        accepted = true;
        done = true;
      }

      else if (*data == '2') { /* positive completion */
        MOZ_LOG(sRemoteLm, LogLevel::Debug, ("%s\n", data + 4));
        *aResponse = strdup((char*)data);
        accepted = true;
        done = true;
      }

      else if (*data == '3') { /* positive intermediate reply */
        MOZ_LOG(sRemoteLm, LogLevel::Debug,
                ("internal error: "
                 "server wants more information?  (%s)\n",
                 data));
        *aResponse = strdup((char*)data);
        done = true;
      }

      else if (*data == '4' || /* transient negative completion */
               *data == '5') { /* permanent negative completion */
        MOZ_LOG(sRemoteLm, LogLevel::Debug, ("%s\n", data + 4));
        *aResponse = strdup((char*)data);
        done = true;
      }

      else {
        MOZ_LOG(
            sRemoteLm, LogLevel::Debug,
            ("unrecognised " MOZILLA_RESPONSE_PROP " from window 0x%x: %s\n",
             (unsigned int)aWindow, data));
        *aResponse = strdup((char*)data);
        done = true;
      }

      if (data) XFree(data);
    }

    else if (event.xany.type == PropertyNotify &&
             event.xproperty.window == aWindow &&
             event.xproperty.state == PropertyDelete &&
             event.xproperty.atom == aCommandAtom) {
      MOZ_LOG(sRemoteLm, LogLevel::Debug,
              ("(server 0x%x has accepted " MOZILLA_COMMANDLINE_PROP ".)\n",
               (unsigned int)aWindow));
    }
  }

  return accepted;
}
