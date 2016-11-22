/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=8:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsXRemoteService.h"
#include "nsIObserverService.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsICommandLineRunner.h"
#include "nsICommandLine.h"

#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsIFile.h"
#include "nsIServiceManager.h"
#include "nsIWeakReference.h"
#include "nsIWidget.h"
#include "nsIAppShellService.h"
#include "nsAppShellCID.h"
#include "nsPIDOMWindow.h"
#include "mozilla/X11Util.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "prprf.h"
#include "prenv.h"
#include "nsCRT.h"

#include "nsXULAppAPI.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>

using namespace mozilla;

#define MOZILLA_VERSION_PROP   "_MOZILLA_VERSION"
#define MOZILLA_LOCK_PROP      "_MOZILLA_LOCK"
#define MOZILLA_RESPONSE_PROP  "_MOZILLA_RESPONSE"
#define MOZILLA_USER_PROP      "_MOZILLA_USER"
#define MOZILLA_PROFILE_PROP   "_MOZILLA_PROFILE"
#define MOZILLA_PROGRAM_PROP   "_MOZILLA_PROGRAM"
#define MOZILLA_COMMANDLINE_PROP "_MOZILLA_COMMANDLINE"

const unsigned char kRemoteVersion[] = "5.1";

#ifdef IS_BIG_ENDIAN
#define TO_LITTLE_ENDIAN32(x) \
    ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >> 8) | \
    (((x) & 0x0000ff00) << 8) | (((x) & 0x000000ff) << 24))
#else
#define TO_LITTLE_ENDIAN32(x) (x)
#endif

// Minimize the roundtrips to the X server by getting all the atoms at once
static const char *XAtomNames[] = {
  MOZILLA_VERSION_PROP,
  MOZILLA_LOCK_PROP,
  MOZILLA_RESPONSE_PROP,
  MOZILLA_USER_PROP,
  MOZILLA_PROFILE_PROP,
  MOZILLA_PROGRAM_PROP,
  MOZILLA_COMMANDLINE_PROP
};
static Atom XAtoms[MOZ_ARRAY_LENGTH(XAtomNames)];

Atom nsXRemoteService::sMozVersionAtom;
Atom nsXRemoteService::sMozLockAtom;
Atom nsXRemoteService::sMozResponseAtom;
Atom nsXRemoteService::sMozUserAtom;
Atom nsXRemoteService::sMozProfileAtom;
Atom nsXRemoteService::sMozProgramAtom;
Atom nsXRemoteService::sMozCommandLineAtom;

nsXRemoteService * nsXRemoteService::sRemoteImplementation = 0;


static bool
FindExtensionParameterInCommand(const char* aParameterName,
                                const nsACString& aCommand,
                                char aSeparator,
                                nsACString* aValue)
{
  nsAutoCString searchFor;
  searchFor.Append(aSeparator);
  searchFor.Append(aParameterName);
  searchFor.Append('=');

  nsACString::const_iterator start, end;
  aCommand.BeginReading(start);
  aCommand.EndReading(end);
  if (!FindInReadable(searchFor, start, end))
    return false;

  nsACString::const_iterator charStart, charEnd;
  charStart = end;
  aCommand.EndReading(charEnd);
  nsACString::const_iterator idStart = charStart, idEnd;
  if (FindCharInReadable(aSeparator, charStart, charEnd)) {
    idEnd = charStart;
  } else {
    idEnd = charEnd;
  }
  *aValue = nsDependentCSubstring(idStart, idEnd);
  return true;
}


nsXRemoteService::nsXRemoteService()
{    
}

void
nsXRemoteService::XRemoteBaseStartup(const char *aAppName, const char *aProfileName)
{
    EnsureAtoms();

    mAppName = aAppName;
    ToLowerCase(mAppName);

    mProfileName = aProfileName;

    nsCOMPtr<nsIObserverService> obs(do_GetService("@mozilla.org/observer-service;1"));
    if (obs) {
      obs->AddObserver(this, "xpcom-shutdown", false);
      obs->AddObserver(this, "quit-application", false);
    }
}

void 
nsXRemoteService::HandleCommandsFor(Window aWindowId)
{
  // set our version
  XChangeProperty(mozilla::DefaultXDisplay(), aWindowId, sMozVersionAtom, XA_STRING,
                  8, PropModeReplace, kRemoteVersion, sizeof(kRemoteVersion) - 1);

  // get our username
  unsigned char *logname;
  logname = (unsigned char*) PR_GetEnv("LOGNAME");
  if (logname) {
    // set the property on the window if it's available
    XChangeProperty(mozilla::DefaultXDisplay(), aWindowId, sMozUserAtom, XA_STRING,
                    8, PropModeReplace, logname, strlen((char*) logname));
  }

  XChangeProperty(mozilla::DefaultXDisplay(), aWindowId, sMozProgramAtom, XA_STRING,
                  8, PropModeReplace, (unsigned char*) mAppName.get(), mAppName.Length());

  if (!mProfileName.IsEmpty()) {
    XChangeProperty(mozilla::DefaultXDisplay(),
                    aWindowId, sMozProfileAtom, XA_STRING,
                    8, PropModeReplace,
                    (unsigned char*) mProfileName.get(), mProfileName.Length());
  }

}

NS_IMETHODIMP
nsXRemoteService::Observe(nsISupports* aSubject,
                          const char *aTopic,
                          const char16_t *aData)
{
  // This can be xpcom-shutdown or quit-application, but it's the same either
  // way.
  Shutdown();
  return NS_OK;
}

bool
nsXRemoteService::HandleNewProperty(XID aWindowId, Display* aDisplay,
                                    Time aEventTime,
                                    Atom aChangedAtom,
                                    nsIWeakReference* aDomWindow)
{

  nsCOMPtr<nsIDOMWindow> window (do_QueryReferent(aDomWindow));

  if (aChangedAtom == sMozCommandLineAtom) {
    // We got a new command atom.
    int result;
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    char *data = 0;

    result = XGetWindowProperty (aDisplay,
                                 aWindowId,
                                 aChangedAtom,
                                 0,                        /* long_offset */
                                 (65536 / sizeof (long)),  /* long_length */
                                 True,                     /* atomic delete after */
                                 XA_STRING,                /* req_type */
                                 &actual_type,             /* actual_type return */
                                 &actual_format,           /* actual_format_return */
                                 &nitems,                  /* nitems_return */
                                 &bytes_after,             /* bytes_after_return */
                                 (unsigned char **)&data); /* prop_return
                                                              (we only care
                                                              about the first ) */

    // Failed to get property off the window?
    if (result != Success)
      return false;

    // Failed to get the data off the window or it was the wrong type?
    if (!data || !TO_LITTLE_ENDIAN32(*reinterpret_cast<int32_t*>(data)))
      return false;

    // cool, we got the property data.
    const char *response = HandleCommandLine(data, window, aEventTime);

    // put the property onto the window as the response
    XChangeProperty (aDisplay, aWindowId,
                     sMozResponseAtom, XA_STRING,
                     8, PropModeReplace,
                     (const unsigned char *)response,
                     strlen (response));
    XFree(data);
    return true;
  }

  else if (aChangedAtom == sMozResponseAtom) {
    // client accepted the response.  party on wayne.
    return true;
  }

  else if (aChangedAtom == sMozLockAtom) {
    // someone locked the window
    return true;
  }

  return false;
}

const char*
nsXRemoteService::HandleCommandLine(char* aBuffer, nsIDOMWindow* aWindow,
                                    uint32_t aTimestamp)
{
  nsresult rv;

  nsCOMPtr<nsICommandLineRunner> cmdline
    (do_CreateInstance("@mozilla.org/toolkit/command-line;1", &rv));
  if (NS_FAILED(rv))
    return "509 internal error";

  // the commandline property is constructed as an array of int32_t
  // followed by a series of null-terminated strings:
  //
  // [argc][offsetargv0][offsetargv1...]<workingdir>\0<argv[0]>\0argv[1]...\0
  // (offset is from the beginning of the buffer)

  int32_t argc = TO_LITTLE_ENDIAN32(*reinterpret_cast<int32_t*>(aBuffer));
  char *wd   = aBuffer + ((argc + 1) * sizeof(int32_t));

  nsCOMPtr<nsIFile> lf;
  rv = NS_NewNativeLocalFile(nsDependentCString(wd), true,
                             getter_AddRefs(lf));
  if (NS_FAILED(rv))
    return "509 internal error";

  nsAutoCString desktopStartupID;

  char **argv = (char**) malloc(sizeof(char*) * argc);
  if (!argv) return "509 internal error";

  int32_t  *offset = reinterpret_cast<int32_t*>(aBuffer) + 1;

  for (int i = 0; i < argc; ++i) {
    argv[i] = aBuffer + TO_LITTLE_ENDIAN32(offset[i]);

    if (i == 0) {
      nsDependentCString cmd(argv[0]);
      FindExtensionParameterInCommand("DESKTOP_STARTUP_ID",
                                      cmd, ' ',
                                      &desktopStartupID);
    }
  }

  rv = cmdline->Init(argc, argv, lf, nsICommandLine::STATE_REMOTE_AUTO);

  free (argv);
  if (NS_FAILED(rv)) {
    return "509 internal error";
  }

  if (aWindow)
    cmdline->SetWindowContext(aWindow);

  if (sRemoteImplementation)
    sRemoteImplementation->SetDesktopStartupIDOrTimestamp(desktopStartupID, aTimestamp);

  rv = cmdline->Run();

  if (NS_ERROR_ABORT == rv)
    return "500 command not parseable";

  if (NS_FAILED(rv))
    return "509 internal error";

  return "200 executed command";
}

void
nsXRemoteService::EnsureAtoms(void)
{
  if (sMozVersionAtom)
    return;

  XInternAtoms(mozilla::DefaultXDisplay(), const_cast<char**>(XAtomNames),
               ArrayLength(XAtomNames), False, XAtoms);

  int i = 0;
  sMozVersionAtom     = XAtoms[i++];
  sMozLockAtom        = XAtoms[i++];
  sMozResponseAtom    = XAtoms[i++];
  sMozUserAtom        = XAtoms[i++];
  sMozProfileAtom     = XAtoms[i++];
  sMozProgramAtom     = XAtoms[i++];
  sMozCommandLineAtom = XAtoms[i++];
}
