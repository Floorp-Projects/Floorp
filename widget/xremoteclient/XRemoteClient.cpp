/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=8:
 */
/* vim:set ts=8 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XRemoteClient.h"
#include "prmem.h"
#include "prprf.h"
#include "plstr.h"
#include "prsystem.h"
#include "prlog.h"
#include "prenv.h"
#include "prdtoa.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <X11/Xatom.h>

#define MOZILLA_VERSION_PROP   "_MOZILLA_VERSION"
#define MOZILLA_LOCK_PROP      "_MOZILLA_LOCK"
#define MOZILLA_COMMAND_PROP   "_MOZILLA_COMMAND"
#define MOZILLA_COMMANDLINE_PROP "_MOZILLA_COMMANDLINE"
#define MOZILLA_RESPONSE_PROP  "_MOZILLA_RESPONSE"
#define MOZILLA_USER_PROP      "_MOZILLA_USER"
#define MOZILLA_PROFILE_PROP   "_MOZILLA_PROFILE"
#define MOZILLA_PROGRAM_PROP   "_MOZILLA_PROGRAM"

#ifdef IS_BIG_ENDIAN
#define TO_LITTLE_ENDIAN32(x) \
    ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >> 8) | \
    (((x) & 0x0000ff00) << 8) | (((x) & 0x000000ff) << 24))
#else
#define TO_LITTLE_ENDIAN32(x) (x)
#endif
    
#ifndef MAX_PATH
#ifdef PATH_MAX
#define MAX_PATH PATH_MAX
#else
#define MAX_PATH 1024
#endif
#endif

#define ARRAY_LENGTH(array_) (sizeof(array_)/sizeof(array_[0]))

static PRLogModuleInfo *sRemoteLm = nullptr;

static int (*sOldHandler)(Display *, XErrorEvent *);
static bool sGotBadWindow;

XRemoteClient::XRemoteClient()
{
  mDisplay = 0;
  mInitialized = false;
  mMozVersionAtom = 0;
  mMozLockAtom = 0;
  mMozCommandAtom = 0;
  mMozResponseAtom = 0;
  mMozWMStateAtom = 0;
  mMozUserAtom = 0;
  mLockData = 0;
  if (!sRemoteLm)
    sRemoteLm = PR_NewLogModule("XRemoteClient");
  PR_LOG(sRemoteLm, PR_LOG_DEBUG, ("XRemoteClient::XRemoteClient"));
}

XRemoteClient::~XRemoteClient()
{
  PR_LOG(sRemoteLm, PR_LOG_DEBUG, ("XRemoteClient::~XRemoteClient"));
  if (mInitialized)
    Shutdown();
}

// Minimize the roundtrips to the X-server
static const char *XAtomNames[] = {
  MOZILLA_VERSION_PROP,
  MOZILLA_LOCK_PROP,
  MOZILLA_COMMAND_PROP,
  MOZILLA_RESPONSE_PROP,
  "WM_STATE",
  MOZILLA_USER_PROP,
  MOZILLA_PROFILE_PROP,
  MOZILLA_PROGRAM_PROP,
  MOZILLA_COMMANDLINE_PROP
};
static Atom XAtoms[ARRAY_LENGTH(XAtomNames)];

nsresult
XRemoteClient::Init()
{
  PR_LOG(sRemoteLm, PR_LOG_DEBUG, ("XRemoteClient::Init"));

  if (mInitialized)
    return NS_OK;

  // try to open the display
  mDisplay = XOpenDisplay(0);
  if (!mDisplay)
    return NS_ERROR_FAILURE;

  // get our atoms
  XInternAtoms(mDisplay, const_cast<char**>(XAtomNames),
               ARRAY_LENGTH(XAtomNames), False, XAtoms);

  int i = 0;
  mMozVersionAtom  = XAtoms[i++];
  mMozLockAtom     = XAtoms[i++];
  mMozCommandAtom  = XAtoms[i++];
  mMozResponseAtom = XAtoms[i++];
  mMozWMStateAtom  = XAtoms[i++];
  mMozUserAtom     = XAtoms[i++];
  mMozProfileAtom  = XAtoms[i++];
  mMozProgramAtom  = XAtoms[i++];
  mMozCommandLineAtom = XAtoms[i++];

  mInitialized = true;

  return NS_OK;
}

void
XRemoteClient::Shutdown (void)
{
  PR_LOG(sRemoteLm, PR_LOG_DEBUG, ("XRemoteClient::Shutdown"));

  if (!mInitialized)
    return;

  // shut everything down
  XCloseDisplay(mDisplay);
  mDisplay = 0;
  mInitialized = false;
  if (mLockData) {
    free(mLockData);
    mLockData = 0;
  }
}

nsresult
XRemoteClient::SendCommand (const char *aProgram, const char *aUsername,
                            const char *aProfile, const char *aCommand,
                            const char* aDesktopStartupID,
                            char **aResponse, bool *aWindowFound)
{
  PR_LOG(sRemoteLm, PR_LOG_DEBUG, ("XRemoteClient::SendCommand"));

  return SendCommandInternal(aProgram, aUsername, aProfile,
                             aCommand, 0, nullptr,
                             aDesktopStartupID,
                             aResponse, aWindowFound);
}

nsresult
XRemoteClient::SendCommandLine (const char *aProgram, const char *aUsername,
                                const char *aProfile,
                                int32_t argc, char **argv,
                                const char* aDesktopStartupID,
                                char **aResponse, bool *aWindowFound)
{
  PR_LOG(sRemoteLm, PR_LOG_DEBUG, ("XRemoteClient::SendCommandLine"));

  return SendCommandInternal(aProgram, aUsername, aProfile,
                             nullptr, argc, argv,
                             aDesktopStartupID,
                             aResponse, aWindowFound);
}

static int
HandleBadWindow(Display *display, XErrorEvent *event)
{
  if (event->error_code == BadWindow) {
    sGotBadWindow = true;
    return 0; // ignored
  }
  else {
    return (*sOldHandler)(display, event);
  }
}

nsresult
XRemoteClient::SendCommandInternal(const char *aProgram, const char *aUsername,
                                   const char *aProfile, const char *aCommand,
                                   int32_t argc, char **argv,
                                   const char* aDesktopStartupID,
                                   char **aResponse, bool *aWindowFound)
{
  *aWindowFound = false;
  bool isCommandLine = !aCommand;

  // FindBestWindow() iterates down the window hierarchy, so catch X errors
  // when windows get destroyed before being accessed.
  sOldHandler = XSetErrorHandler(HandleBadWindow);

  Window w = FindBestWindow(aProgram, aUsername, aProfile, isCommandLine);

  nsresult rv = NS_OK;

  if (w) {
    // ok, let the caller know that we at least found a window.
    *aWindowFound = true;

    // Ignore BadWindow errors up to this point.  The last request from
    // FindBestWindow() was a synchronous XGetWindowProperty(), so no need to
    // Sync.  Leave the error handler installed to detect if w gets destroyed.
    sGotBadWindow = false;

    // make sure we get the right events on that window
    XSelectInput(mDisplay, w,
                 (PropertyChangeMask|StructureNotifyMask));

    bool destroyed = false;

    // get the lock on the window
    rv = GetLock(w, &destroyed);

    if (NS_SUCCEEDED(rv)) {
      // send our command
      if (isCommandLine) {
        rv = DoSendCommandLine(w, argc, argv, aDesktopStartupID, aResponse,
                               &destroyed);
      }
      else {
        rv = DoSendCommand(w, aCommand, aDesktopStartupID, aResponse,
                           &destroyed);
      }

      // if the window was destroyed, don't bother trying to free the
      // lock.
      if (!destroyed)
          FreeLock(w); // doesn't really matter what this returns

    }
  }

  XSetErrorHandler(sOldHandler);

  PR_LOG(sRemoteLm, PR_LOG_DEBUG, ("SendCommandInternal returning 0x%x\n", rv));

  return rv;
}

Window
XRemoteClient::CheckWindow(Window aWindow)
{
  Atom type = None;
  int  format;
  unsigned long nitems, bytesafter;
  unsigned char *data;
  Window innerWindow;

  XGetWindowProperty(mDisplay, aWindow, mMozWMStateAtom,
		     0, 0, False, AnyPropertyType,
		     &type, &format, &nitems, &bytesafter, &data);

  if (type) {
    XFree(data);
    return aWindow;
  }

  // didn't find it here so check the children of this window
  innerWindow = CheckChildren(aWindow);

  if (innerWindow)
    return innerWindow;

  return aWindow;
}

Window
XRemoteClient::CheckChildren(Window aWindow)
{
  Window root, parent;
  Window *children;
  unsigned int nchildren;
  unsigned int i;
  Atom type = None;
  int format;
  unsigned long nitems, after;
  unsigned char *data;
  Window retval = None;
  
  if (!XQueryTree(mDisplay, aWindow, &root, &parent, &children,
		  &nchildren))
    return None;
  
  // scan the list first before recursing into the list of windows
  // which can get quite deep.
  for (i=0; !retval && (i < nchildren); i++) {
    XGetWindowProperty(mDisplay, children[i], mMozWMStateAtom,
		       0, 0, False, AnyPropertyType, &type, &format,
		       &nitems, &after, &data);
    if (type) {
      XFree(data);
      retval = children[i];
    }
  }

  // otherwise recurse into the list
  for (i=0; !retval && (i < nchildren); i++) {
    retval = CheckChildren(children[i]);
  }

  if (children)
    XFree((char *)children);

  return retval;
}

nsresult
XRemoteClient::GetLock(Window aWindow, bool *aDestroyed)
{
  bool locked = false;
  bool waited = false;
  *aDestroyed = false;

  nsresult rv = NS_OK;

  if (!mLockData) {
    
    char pidstr[32];
    char sysinfobuf[SYS_INFO_BUFFER_LENGTH];
    PR_snprintf(pidstr, sizeof(pidstr), "pid%d@", getpid());
    PRStatus status;
    status = PR_GetSystemInfo(PR_SI_HOSTNAME, sysinfobuf,
			      SYS_INFO_BUFFER_LENGTH);
    if (status != PR_SUCCESS) {
      return NS_ERROR_FAILURE;
    }
    
    // allocate enough space for the string plus the terminating
    // char
    mLockData = (char *)malloc(strlen(pidstr) + strlen(sysinfobuf) + 1);
    if (!mLockData)
      return NS_ERROR_OUT_OF_MEMORY;

    strcpy(mLockData, pidstr);
    if (!strcat(mLockData, sysinfobuf))
      return NS_ERROR_FAILURE;
  }

  do {
    int result;
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *data = 0;

    XGrabServer(mDisplay);

    result = XGetWindowProperty (mDisplay, aWindow, mMozLockAtom,
				 0, (65536 / sizeof (long)),
				 False, /* don't delete */
				 XA_STRING,
				 &actual_type, &actual_format,
				 &nitems, &bytes_after,
				 &data);

    // aWindow may have been destroyed before XSelectInput was processed, in
    // which case there may not be any DestroyNotify event in the queue to
    // tell us.  XGetWindowProperty() was synchronous so error responses have
    // now been processed, setting sGotBadWindow.
    if (sGotBadWindow) {
      *aDestroyed = true;
      rv = NS_ERROR_FAILURE;
    }
    else if (result != Success || actual_type == None) {
      /* It's not now locked - lock it. */
      XChangeProperty (mDisplay, aWindow, mMozLockAtom, XA_STRING, 8,
		       PropModeReplace,
		       (unsigned char *)mLockData,
		       strlen(mLockData));
      locked = True;
    }

    XUngrabServer(mDisplay);
    XFlush(mDisplay); // ungrab now!

    if (!locked && !NS_FAILED(rv)) {
      /* We tried to grab the lock this time, and failed because someone
	 else is holding it already.  So, wait for a PropertyDelete event
	 to come in, and try again. */
      PR_LOG(sRemoteLm, PR_LOG_DEBUG, 
	     ("window 0x%x is locked by %s; waiting...\n",
	      (unsigned int) aWindow, data));
      waited = True;
      while (1) {
	XEvent event;
	int select_retval;
	fd_set select_set;
	struct timeval delay;
	delay.tv_sec = 10;
	delay.tv_usec = 0;

	FD_ZERO(&select_set);
	// add the x event queue to the select set
	FD_SET(ConnectionNumber(mDisplay), &select_set);
	select_retval = select(ConnectionNumber(mDisplay) + 1,
			       &select_set, nullptr, nullptr, &delay);
	// did we time out?
	if (select_retval == 0) {
	  PR_LOG(sRemoteLm, PR_LOG_DEBUG, ("timed out waiting for window\n"));
          rv = NS_ERROR_FAILURE;
          break;
	}
	PR_LOG(sRemoteLm, PR_LOG_DEBUG, ("xevent...\n"));
	XNextEvent (mDisplay, &event);
	if (event.xany.type == DestroyNotify &&
	    event.xdestroywindow.window == aWindow) {
	  *aDestroyed = true;
          rv = NS_ERROR_FAILURE;
          break;
	}
	else if (event.xany.type == PropertyNotify &&
		 event.xproperty.state == PropertyDelete &&
		 event.xproperty.window == aWindow &&
		 event.xproperty.atom == mMozLockAtom) {
	  /* Ok!  Someone deleted their lock, so now we can try
	     again. */
	  PR_LOG(sRemoteLm, PR_LOG_DEBUG,
		 ("(0x%x unlocked, trying again...)\n",
		  (unsigned int) aWindow));
		  break;
	}
      }
    }
    if (data)
      XFree(data);
  } while (!locked && !NS_FAILED(rv));

  if (waited && locked) {
    PR_LOG(sRemoteLm, PR_LOG_DEBUG, ("obtained lock.\n"));
  } else if (*aDestroyed) {
    PR_LOG(sRemoteLm, PR_LOG_DEBUG,
           ("window 0x%x unexpectedly destroyed.\n",
            (unsigned int) aWindow));
  }

  return rv;
}

Window
XRemoteClient::FindBestWindow(const char *aProgram, const char *aUsername,
                              const char *aProfile,
                              bool aSupportsCommandLine)
{
  Window root = RootWindowOfScreen(DefaultScreenOfDisplay(mDisplay));
  Window bestWindow = 0;
  Window root2, parent, *kids;
  unsigned int nkids;

  // Get a list of the children of the root window, walk the list
  // looking for the best window that fits the criteria.
  if (!XQueryTree(mDisplay, root, &root2, &parent, &kids, &nkids)) {
    PR_LOG(sRemoteLm, PR_LOG_DEBUG,
           ("XQueryTree failed in XRemoteClient::FindBestWindow"));
    return 0;
  }

  if (!(kids && nkids)) {
    PR_LOG(sRemoteLm, PR_LOG_DEBUG, ("root window has no children"));
    return 0;
  }

  // We'll walk the list of windows looking for a window that best
  // fits the criteria here.

  for (unsigned int i = 0; i < nkids; i++) {
    Atom type;
    int format;
    unsigned long nitems, bytesafter;
    unsigned char *data_return = 0;
    Window w;
    w = kids[i];
    // find the inner window with WM_STATE on it
    w = CheckWindow(w);

    int status = XGetWindowProperty(mDisplay, w, mMozVersionAtom,
                                    0, (65536 / sizeof (long)),
                                    False, XA_STRING,
                                    &type, &format, &nitems, &bytesafter,
                                    &data_return);

    if (!data_return)
      continue;

    double version = PR_strtod((char*) data_return, nullptr);
    XFree(data_return);

    if (aSupportsCommandLine && !(version >= 5.1 && version < 6))
      continue;

    data_return = 0;

    if (status != Success || type == None)
      continue;

    // If someone passed in a program name, check it against this one
    // unless it's "any" in which case, we don't care.  If someone did
    // pass in a program name and this window doesn't support that
    // protocol, we don't include it in our list.
    if (aProgram && strcmp(aProgram, "any")) {
        status = XGetWindowProperty(mDisplay, w, mMozProgramAtom,
                                    0, (65536 / sizeof(long)),
                                    False, XA_STRING,
                                    &type, &format, &nitems, &bytesafter,
                                    &data_return);
        
        // If the return name is not the same as what someone passed in,
        // we don't want this window.
        if (data_return) {
            if (strcmp(aProgram, (const char *)data_return)) {
                XFree(data_return);
                continue;
            }

            // This is actually the success condition.
            XFree(data_return);
        }
        else {
            // Doesn't support the protocol, even though the user
            // requested it.  So we're not going to use this window.
            continue;
        }
    }

    // Check to see if it has the user atom on that window.  If there
    // is then we need to make sure that it matches what we have.
    const char *username;
    if (aUsername) {
      username = aUsername;
    }
    else {
      username = PR_GetEnv("LOGNAME");
    }

    if (username) {
        status = XGetWindowProperty(mDisplay, w, mMozUserAtom,
                                    0, (65536 / sizeof(long)),
                                    False, XA_STRING,
                                    &type, &format, &nitems, &bytesafter,
                                    &data_return);

        // if there's a username compare it with what we have
        if (data_return) {
            // If the IDs aren't equal, we don't want this window.
            if (strcmp(username, (const char *)data_return)) {
                XFree(data_return);
                continue;
            }

            XFree(data_return);
        }
    }

    // Check to see if there's a profile name on this window.  If
    // there is, then we need to make sure it matches what someone
    // passed in.
    if (aProfile) {
        status = XGetWindowProperty(mDisplay, w, mMozProfileAtom,
                                    0, (65536 / sizeof(long)),
                                    False, XA_STRING,
                                    &type, &format, &nitems, &bytesafter,
                                    &data_return);

        // If there's a profile compare it with what we have
        if (data_return) {
            // If the profiles aren't equal, we don't want this window.
            if (strcmp(aProfile, (const char *)data_return)) {
                XFree(data_return);
                continue;
            }

            XFree(data_return);
        }
    }

    // Check to see if the window supports the new command-line passing
    // protocol, if that is requested.

    // If we got this far, this is the best window.  It passed
    // all the tests.
    bestWindow = w;
    break;
  }

  if (kids)
    XFree((char *) kids);

  return bestWindow;
}

nsresult
XRemoteClient::FreeLock(Window aWindow)
{
  int result;
  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char *data = 0;

  result = XGetWindowProperty(mDisplay, aWindow, mMozLockAtom,
                              0, (65536 / sizeof(long)),
                              True, /* atomic delete after */
                              XA_STRING,
                              &actual_type, &actual_format,
                              &nitems, &bytes_after,
                              &data);
  if (result != Success) {
      PR_LOG(sRemoteLm, PR_LOG_DEBUG,
             ("unable to read and delete " MOZILLA_LOCK_PROP
              " property\n"));
      return NS_ERROR_FAILURE;
  }
  else if (!data || !*data){
      PR_LOG(sRemoteLm, PR_LOG_DEBUG,
             ("invalid data on " MOZILLA_LOCK_PROP
              " of window 0x%x.\n",
              (unsigned int) aWindow));
      return NS_ERROR_FAILURE;
  }
  else if (strcmp((char *)data, mLockData)) {
      PR_LOG(sRemoteLm, PR_LOG_DEBUG,
             (MOZILLA_LOCK_PROP " was stolen!  Expected \"%s\", saw \"%s\"!\n",
              mLockData, data));
      return NS_ERROR_FAILURE;
  }

  if (data)
      XFree(data);
  return NS_OK;
}

nsresult
XRemoteClient::DoSendCommand(Window aWindow, const char *aCommand,
                             const char* aDesktopStartupID,
                             char **aResponse, bool *aDestroyed)
{
  *aDestroyed = false;

  PR_LOG(sRemoteLm, PR_LOG_DEBUG,
     ("(writing " MOZILLA_COMMAND_PROP " \"%s\" to 0x%x)\n",
      aCommand, (unsigned int) aWindow));

  // We add the DESKTOP_STARTUP_ID setting as an extra line of
  // the command string. Firefox ignores all lines but the first.
  static char desktopStartupPrefix[] = "\nDESKTOP_STARTUP_ID=";

  int32_t len = strlen(aCommand);
  if (aDesktopStartupID) {
    len += sizeof(desktopStartupPrefix) - 1 + strlen(aDesktopStartupID);
  }
  char* buffer = (char*)malloc(len + 1);
  if (!buffer)
    return NS_ERROR_OUT_OF_MEMORY;

  strcpy(buffer, aCommand);
  if (aDesktopStartupID) {
    strcat(buffer, desktopStartupPrefix);
    strcat(buffer, aDesktopStartupID);
  }

  XChangeProperty (mDisplay, aWindow, mMozCommandAtom, XA_STRING, 8,
           PropModeReplace, (unsigned char *)buffer, len);

  free(buffer);

  if (!WaitForResponse(aWindow, aResponse, aDestroyed, mMozCommandAtom))
    return NS_ERROR_FAILURE;
  
  return NS_OK;
}

/* like strcpy, but return the char after the final null */
static char*
estrcpy(const char* s, char* d)
{
  while (*s)
    *d++ = *s++;

  *d++ = '\0';
  return d;
}

nsresult
XRemoteClient::DoSendCommandLine(Window aWindow, int32_t argc, char **argv,
                                 const char* aDesktopStartupID,
                                 char **aResponse, bool *aDestroyed)
{
  *aDestroyed = false;

  char cwdbuf[MAX_PATH];
  if (!getcwd(cwdbuf, MAX_PATH))
    return NS_ERROR_UNEXPECTED;

  // the commandline property is constructed as an array of int32_t
  // followed by a series of null-terminated strings:
  //
  // [argc][offsetargv0][offsetargv1...]<workingdir>\0<argv[0]>\0argv[1]...\0
  // (offset is from the beginning of the buffer)

  static char desktopStartupPrefix[] = " DESKTOP_STARTUP_ID=";

  int32_t argvlen = strlen(cwdbuf);
  for (int i = 0; i < argc; ++i) {
    int32_t len = strlen(argv[i]);
    if (i == 0 && aDesktopStartupID) {
      len += sizeof(desktopStartupPrefix) - 1 + strlen(aDesktopStartupID);
    }
    argvlen += len;
  }

  int32_t* buffer = (int32_t*) malloc(argvlen + argc + 1 +
                                      sizeof(int32_t) * (argc + 1));
  if (!buffer)
    return NS_ERROR_OUT_OF_MEMORY;

  buffer[0] = TO_LITTLE_ENDIAN32(argc);

  char *bufend = (char*) (buffer + argc + 1);

  bufend = estrcpy(cwdbuf, bufend);

  for (int i = 0; i < argc; ++i) {
    buffer[i + 1] = TO_LITTLE_ENDIAN32(bufend - ((char*) buffer));
    bufend = estrcpy(argv[i], bufend);
    if (i == 0 && aDesktopStartupID) {
      bufend = estrcpy(desktopStartupPrefix, bufend - 1);
      bufend = estrcpy(aDesktopStartupID, bufend - 1);
    }
  }

#ifdef DEBUG_bsmedberg
  int32_t   debug_argc   = TO_LITTLE_ENDIAN32(*buffer);
  char *debug_workingdir = (char*) (buffer + argc + 1);

  printf("Sending command line:\n"
         "  working dir: %s\n"
         "  argc:\t%i",
         debug_workingdir,
         debug_argc);

  int32_t  *debug_offset = buffer + 1;
  for (int debug_i = 0; debug_i < debug_argc; ++debug_i)
    printf("  argv[%i]:\t%s\n", debug_i,
           ((char*) buffer) + TO_LITTLE_ENDIAN32(debug_offset[debug_i]));
#endif

  XChangeProperty (mDisplay, aWindow, mMozCommandLineAtom, XA_STRING, 8,
                   PropModeReplace, (unsigned char *) buffer,
                   bufend - ((char*) buffer));
  free(buffer);

  if (!WaitForResponse(aWindow, aResponse, aDestroyed, mMozCommandLineAtom))
    return NS_ERROR_FAILURE;
  
  return NS_OK;
}

bool
XRemoteClient::WaitForResponse(Window aWindow, char **aResponse,
                               bool *aDestroyed, Atom aCommandAtom)
{
  bool done = false;
  bool accepted = false;

  while (!done) {
    XEvent event;
    XNextEvent (mDisplay, &event);
    if (event.xany.type == DestroyNotify &&
        event.xdestroywindow.window == aWindow) {
      /* Print to warn user...*/
      PR_LOG(sRemoteLm, PR_LOG_DEBUG,
             ("window 0x%x was destroyed.\n",
              (unsigned int) aWindow));
      *aResponse = strdup("Window was destroyed while reading response.");
      *aDestroyed = true;
      return false;
    }
    else if (event.xany.type == PropertyNotify &&
             event.xproperty.state == PropertyNewValue &&
             event.xproperty.window == aWindow &&
             event.xproperty.atom == mMozResponseAtom) {
      Atom actual_type;
      int actual_format;
      unsigned long nitems, bytes_after;
      unsigned char *data = 0;
      Bool result;
      result = XGetWindowProperty (mDisplay, aWindow, mMozResponseAtom,
                                   0, (65536 / sizeof (long)),
                                   True, /* atomic delete after */
                                   XA_STRING,
                                   &actual_type, &actual_format,
                                   &nitems, &bytes_after,
                                   &data);
      if (result != Success) {
        PR_LOG(sRemoteLm, PR_LOG_DEBUG,
               ("failed reading " MOZILLA_RESPONSE_PROP
                " from window 0x%0x.\n",
                (unsigned int) aWindow));
        *aResponse = strdup("Internal error reading response from window.");
        done = true;
      }
      else if (!data || strlen((char *) data) < 5) {
        PR_LOG(sRemoteLm, PR_LOG_DEBUG,
               ("invalid data on " MOZILLA_RESPONSE_PROP
                " property of window 0x%0x.\n",
                (unsigned int) aWindow));
        *aResponse = strdup("Server returned invalid data in response.");
        done = true;
      }
      else if (*data == '1') {  /* positive preliminary reply */
        PR_LOG(sRemoteLm, PR_LOG_DEBUG,  ("%s\n", data + 4));
        /* keep going */
        done = false;
      }

      else if (!strncmp ((char *)data, "200", 3)) { /* positive completion */
        *aResponse = strdup((char *)data);
        accepted = true;
        done = true;
      }

      else if (*data == '2') {  /* positive completion */
        PR_LOG(sRemoteLm, PR_LOG_DEBUG, ("%s\n", data + 4));
        *aResponse = strdup((char *)data);
        accepted = true;
        done = true;
      }

      else if (*data == '3') {  /* positive intermediate reply */
        PR_LOG(sRemoteLm, PR_LOG_DEBUG,
               ("internal error: "
                "server wants more information?  (%s)\n",
                data));
        *aResponse = strdup((char *)data);
        done = true;
      }

      else if (*data == '4' ||  /* transient negative completion */
               *data == '5') {  /* permanent negative completion */
        PR_LOG(sRemoteLm, PR_LOG_DEBUG, ("%s\n", data + 4));
        *aResponse = strdup((char *)data);
        done = true;
      }

      else {
        PR_LOG(sRemoteLm, PR_LOG_DEBUG,
               ("unrecognised " MOZILLA_RESPONSE_PROP
                " from window 0x%x: %s\n",
                (unsigned int) aWindow, data));
        *aResponse = strdup((char *)data);
        done = true;
      }

      if (data)
        XFree(data);
    }

    else if (event.xany.type == PropertyNotify &&
             event.xproperty.window == aWindow &&
             event.xproperty.state == PropertyDelete &&
             event.xproperty.atom == aCommandAtom) {
      PR_LOG(sRemoteLm, PR_LOG_DEBUG,
             ("(server 0x%x has accepted "
              MOZILLA_COMMAND_PROP ".)\n",
              (unsigned int) aWindow));
    }
    
  }

  return accepted;
}
