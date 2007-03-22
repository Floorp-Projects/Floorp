/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* vim:set ts=8 sw=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Christopher Blizzard and Jamie Zawinski.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include <X11/Xatom.h>
#ifdef POLL_WITH_XCONNECTIONNUMBER
#include <poll.h>
#endif

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
#define MAX_PATH 1024
#endif

#define ARRAY_LENGTH(array_) (sizeof(array_)/sizeof(array_[0]))

static PRLogModuleInfo *sRemoteLm = NULL;

XRemoteClient::XRemoteClient()
{
  mDisplay = 0;
  mInitialized = PR_FALSE;
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
static char *XAtomNames[] = {
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
  XInternAtoms(mDisplay, XAtomNames, ARRAY_LENGTH(XAtomNames), False, XAtoms);

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

  mInitialized = PR_TRUE;

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
  mInitialized = PR_FALSE;
  if (mLockData) {
    free(mLockData);
    mLockData = 0;
  }
}

nsresult
XRemoteClient::SendCommand (const char *aProgram, const char *aUsername,
                            const char *aProfile, const char *aCommand,
                            const char* aDesktopStartupID,
                            char **aResponse, PRBool *aWindowFound)
{
  PR_LOG(sRemoteLm, PR_LOG_DEBUG, ("XRemoteClient::SendCommand"));

  *aWindowFound = PR_FALSE;

  Window w = FindBestWindow(aProgram, aUsername, aProfile, PR_FALSE);

  nsresult rv = NS_OK;

  if (w) {
    // ok, let the caller know that we at least found a window.
    *aWindowFound = PR_TRUE;

    // make sure we get the right events on that window
    XSelectInput(mDisplay, w,
                 (PropertyChangeMask|StructureNotifyMask));
        
    PRBool destroyed = PR_FALSE;

    // get the lock on the window
    rv = GetLock(w, &destroyed);

    if (NS_SUCCEEDED(rv)) {
      // send our command
      rv = DoSendCommand(w, aCommand, aDesktopStartupID, aResponse, &destroyed);

      // if the window was destroyed, don't bother trying to free the
      // lock.
      if (!destroyed)
          FreeLock(w); // doesn't really matter what this returns

    }
  }

  PR_LOG(sRemoteLm, PR_LOG_DEBUG, ("SendCommand returning 0x%x\n", rv));

  return rv;
}

nsresult
XRemoteClient::SendCommandLine (const char *aProgram, const char *aUsername,
                                const char *aProfile,
                                PRInt32 argc, char **argv,
                                const char* aDesktopStartupID,
                                char **aResponse, PRBool *aWindowFound)
{
  PR_LOG(sRemoteLm, PR_LOG_DEBUG, ("XRemoteClient::SendCommandLine"));

  *aWindowFound = PR_FALSE;

  Window w = FindBestWindow(aProgram, aUsername, aProfile, PR_TRUE);

  nsresult rv = NS_OK;

  if (w) {
    // ok, let the caller know that we at least found a window.
    *aWindowFound = PR_TRUE;

    // make sure we get the right events on that window
    XSelectInput(mDisplay, w,
                 (PropertyChangeMask|StructureNotifyMask));
        
    PRBool destroyed = PR_FALSE;

    // get the lock on the window
    rv = GetLock(w, &destroyed);

    if (NS_SUCCEEDED(rv)) {
      // send our command
      rv = DoSendCommandLine(w, argc, argv, aDesktopStartupID, aResponse, &destroyed);

      // if the window was destroyed, don't bother trying to free the
      // lock.
      if (!destroyed)
          FreeLock(w); // doesn't really matter what this returns

    }
  }

  PR_LOG(sRemoteLm, PR_LOG_DEBUG, ("SendCommandLine returning 0x%x\n", rv));

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
XRemoteClient::GetLock(Window aWindow, PRBool *aDestroyed)
{
  PRBool locked = PR_FALSE;
  PRBool waited = PR_FALSE;
  *aDestroyed = PR_FALSE;

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
    if (result != Success || actual_type == None) {
      /* It's not now locked - lock it. */
      XChangeProperty (mDisplay, aWindow, mMozLockAtom, XA_STRING, 8,
		       PropModeReplace,
		       (unsigned char *)mLockData,
		       strlen(mLockData));
      locked = True;
    }

    XUngrabServer(mDisplay);
    XSync(mDisplay, False);

    if (!locked) {
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
#ifdef POLL_WITH_XCONNECTIONNUMBER
       struct pollfd fds[1];
       fds[0].fd = XConnectionNumber(mDisplay);
       fds[0].events = POLLIN;
       select_retval = poll(fds,1,10*1000);
#else
	fd_set select_set;
	struct timeval delay;
	delay.tv_sec = 10;
	delay.tv_usec = 0;

	FD_ZERO(&select_set);
	// add the x event queue to the select set
	FD_SET(ConnectionNumber(mDisplay), &select_set);
	select_retval = select(ConnectionNumber(mDisplay) + 1,
			       &select_set, NULL, NULL, &delay);
#endif
	// did we time out?
	if (select_retval == 0) {
	  PR_LOG(sRemoteLm, PR_LOG_DEBUG, ("timed out waiting for window\n"));
	  return NS_ERROR_FAILURE;
	}
	PR_LOG(sRemoteLm, PR_LOG_DEBUG, ("xevent...\n"));
	XNextEvent (mDisplay, &event);
	if (event.xany.type == DestroyNotify &&
	    event.xdestroywindow.window == aWindow) {
	  PR_LOG(sRemoteLm, PR_LOG_DEBUG,
		 ("window 0x%x unexpectedly destroyed.\n",
		  (unsigned int) aWindow));
	  *aDestroyed = PR_TRUE;
	  return NS_ERROR_FAILURE;
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
  } while (!locked);

  if (waited) {
    PR_LOG(sRemoteLm, PR_LOG_DEBUG, ("obtained lock.\n"));
  }

  return NS_OK;
}

Window
XRemoteClient::FindBestWindow(const char *aProgram, const char *aUsername,
                              const char *aProfile,
                              PRBool aSupportsCommandLine)
{
  Window root = RootWindowOfScreen(DefaultScreenOfDisplay(mDisplay));
  Window bestWindow = 0;
  Window root2, parent, *kids;
  unsigned int nkids;
  int i;

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

  for (i=nkids-1; i >= 0; i--) {
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

    PRFloat64 version = PR_strtod((char*) data_return, nsnull);
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

    // If we got this far, this is the best window so far.  It passed
    // all the tests.
    bestWindow = w;
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
                             char **aResponse, PRBool *aDestroyed)
{
  *aDestroyed = PR_FALSE;

  PR_LOG(sRemoteLm, PR_LOG_DEBUG,
     ("(writing " MOZILLA_COMMAND_PROP " \"%s\" to 0x%x)\n",
      aCommand, (unsigned int) aWindow));

  // We add the DESKTOP_STARTUP_ID setting as an extra line of
  // the command string. Firefox ignores all lines but the first.
  static char desktopStartupPrefix[] = "\nDESKTOP_STARTUP_ID=";

  PRInt32 len = strlen(aCommand);
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
XRemoteClient::DoSendCommandLine(Window aWindow, PRInt32 argc, char **argv,
                                 const char* aDesktopStartupID,
                                 char **aResponse, PRBool *aDestroyed)
{
  int i;

  *aDestroyed = PR_FALSE;

  char cwdbuf[MAX_PATH];
  if (!getcwd(cwdbuf, MAX_PATH))
    return NS_ERROR_UNEXPECTED;

  // the commandline property is constructed as an array of PRInt32
  // followed by a series of null-terminated strings:
  //
  // [argc][offsetargv0][offsetargv1...]<workingdir>\0<argv[0]>\0argv[1]...\0
  // (offset is from the beginning of the buffer)

  static char desktopStartupPrefix[] = " DESKTOP_STARTUP_ID=";

  PRInt32 argvlen = strlen(cwdbuf);
  for (i = 0; i < argc; ++i) {
    PRInt32 len = strlen(argv[i]);
    if (i == 0 && aDesktopStartupID) {
      len += sizeof(desktopStartupPrefix) - 1 + strlen(aDesktopStartupID);
    }
    argvlen += len;
  }

  PRInt32* buffer = (PRInt32*) malloc(argvlen + argc + 1 +
                                      sizeof(PRInt32) * (argc + 1));
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
  PRInt32   debug_argc   = TO_LITTLE_ENDIAN32(*buffer);
  char *debug_workingdir = (char*) (buffer + argc + 1);

  printf("Sending command line:\n"
         "  working dir: %s\n"
         "  argc:\t%i",
         debug_workingdir,
         debug_argc);

  PRInt32  *debug_offset = buffer + 1;
  for (int debug_i = 0; debug_i < debug_argc; ++debug_i)
    printf("  argv[%i]:\t%s\n", debug_i,
           ((char*) buffer) + TO_LITTLE_ENDIAN32(debug_offset[debug_i]));
#endif

  XChangeProperty (mDisplay, aWindow, mMozCommandLineAtom, XA_STRING, 8,
                   PropModeReplace, (unsigned char *) buffer,
                   bufend - ((char*) buffer));

  if (!WaitForResponse(aWindow, aResponse, aDestroyed, mMozCommandLineAtom))
    return NS_ERROR_FAILURE;
  
  return NS_OK;
}

PRBool
XRemoteClient::WaitForResponse(Window aWindow, char **aResponse,
                               PRBool *aDestroyed, Atom aCommandAtom)
{
  PRBool done = PR_FALSE;
  PRBool accepted = PR_FALSE;

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
      *aDestroyed = PR_TRUE;
      return PR_FALSE;
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
        done = PR_TRUE;
      }
      else if (!data || strlen((char *) data) < 5) {
        PR_LOG(sRemoteLm, PR_LOG_DEBUG,
               ("invalid data on " MOZILLA_RESPONSE_PROP
                " property of window 0x%0x.\n",
                (unsigned int) aWindow));
        *aResponse = strdup("Server returned invalid data in response.");
        done = PR_TRUE;
      }
      else if (*data == '1') {  /* positive preliminary reply */
        PR_LOG(sRemoteLm, PR_LOG_DEBUG,  ("%s\n", data + 4));
        /* keep going */
        done = PR_FALSE;
      }

      else if (!strncmp ((char *)data, "200", 3)) { /* positive completion */
        *aResponse = strdup((char *)data);
        accepted = PR_TRUE;
        done = PR_TRUE;
      }

      else if (*data == '2') {  /* positive completion */
        PR_LOG(sRemoteLm, PR_LOG_DEBUG, ("%s\n", data + 4));
        *aResponse = strdup((char *)data);
        accepted = PR_TRUE;
        done = PR_TRUE;
      }

      else if (*data == '3') {  /* positive intermediate reply */
        PR_LOG(sRemoteLm, PR_LOG_DEBUG,
               ("internal error: "
                "server wants more information?  (%s)\n",
                data));
        *aResponse = strdup((char *)data);
        done = PR_TRUE;
      }

      else if (*data == '4' ||  /* transient negative completion */
               *data == '5') {  /* permanent negative completion */
        PR_LOG(sRemoteLm, PR_LOG_DEBUG, ("%s\n", data + 4));
        *aResponse = strdup((char *)data);
        done = PR_TRUE;
      }

      else {
        PR_LOG(sRemoteLm, PR_LOG_DEBUG,
               ("unrecognised " MOZILLA_RESPONSE_PROP
                " from window 0x%x: %s\n",
                (unsigned int) aWindow, data));
        *aResponse = strdup((char *)data);
        done = PR_TRUE;
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
