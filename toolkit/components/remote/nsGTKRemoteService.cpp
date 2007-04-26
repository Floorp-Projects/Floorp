/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=8:
 */
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
 * Christopher Blizzard.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Benjamin Smedberg <benjamin@smedbergs.us>
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

#include "nsGTKRemoteService.h"

#include <X11/Xatom.h> // for XA_STRING
#include <stdlib.h>
#include <gtk/gtkinvisible.h> // For some reason GTK+ doesn't include this file
                              // automatically from gtk.h
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsPIDOMWindow.h"
#include "nsIGenericFactory.h"
#include "nsILocalFile.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsIWeakReference.h"
#include "nsIWidget.h"
#include "nsIAppShellService.h"
#include "nsAppShellCID.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "prprf.h"
#include "prenv.h"
#include "nsCRT.h"

#ifdef MOZ_WIDGET_GTK2
#include "nsGTKToolkit.h"
#endif

#ifdef MOZ_XUL_APP
#include "nsICommandLineRunner.h"
#include "nsXULAppAPI.h"
#else
#include "nsISuiteRemoteService.h"
#endif

#define MOZILLA_VERSION_PROP   "_MOZILLA_VERSION"
#define MOZILLA_LOCK_PROP      "_MOZILLA_LOCK"
#define MOZILLA_COMMAND_PROP   "_MOZILLA_COMMAND"
#define MOZILLA_RESPONSE_PROP  "_MOZILLA_RESPONSE"
#define MOZILLA_USER_PROP      "_MOZILLA_USER"
#define MOZILLA_PROFILE_PROP   "_MOZILLA_PROFILE"
#define MOZILLA_PROGRAM_PROP   "_MOZILLA_PROGRAM"
#define MOZILLA_COMMANDLINE_PROP "_MOZILLA_COMMANDLINE"

#ifdef IS_BIG_ENDIAN
#define TO_LITTLE_ENDIAN32(x) \
    ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >> 8) | \
    (((x) & 0x0000ff00) << 8) | (((x) & 0x000000ff) << 24))
#else
#define TO_LITTLE_ENDIAN32(x) (x)
#endif

#ifdef MOZ_XUL_APP
const unsigned char kRemoteVersion[] = "5.1";
#else
const unsigned char kRemoteVersion[] = "5.0";
#endif

NS_IMPL_QUERY_INTERFACE2(nsGTKRemoteService,
                         nsIRemoteService,
                         nsIObserver)

NS_IMETHODIMP_(nsrefcnt)
nsGTKRemoteService::AddRef()
{
  return 1;
}

NS_IMETHODIMP_(nsrefcnt)
nsGTKRemoteService::Release()
{
  return 1;
}

NS_IMETHODIMP
nsGTKRemoteService::Startup(const char* aAppName, const char* aProfileName)
{
  NS_ASSERTION(aAppName, "Don't pass a null appname!");

  EnsureAtoms();
  if (mServerWindow) return NS_ERROR_ALREADY_INITIALIZED;

  mAppName = aAppName;
  ToLowerCase(mAppName);

  mProfileName = aProfileName;

  mServerWindow = gtk_invisible_new();
  gtk_widget_realize(mServerWindow);
  HandleCommandsFor(mServerWindow, nsnull);

  if (!mWindows.IsInitialized())
    mWindows.Init();

  mWindows.EnumerateRead(StartupHandler, this);

  nsCOMPtr<nsIObserverService> obs
    (do_GetService("@mozilla.org/observer-service;1"));
  if (obs) {
    obs->AddObserver(this, "xpcom-shutdown", PR_FALSE);
    obs->AddObserver(this, "quit-application", PR_FALSE);
  }

  return NS_OK;
}

PLDHashOperator
nsGTKRemoteService::StartupHandler(const void* aKey,
                                   nsIWeakReference* aData,
                                   void* aClosure)
{
  GtkWidget* widget = (GtkWidget*) aKey;
  nsGTKRemoteService* aThis = (nsGTKRemoteService*) aClosure;

  aThis->HandleCommandsFor(widget, aData);
  return PL_DHASH_NEXT;
}

static nsIWidget* GetMainWidget(nsIDOMWindow* aWindow)
{
  // get the native window for this instance
  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(aWindow));
  NS_ENSURE_TRUE(window, nsnull);

  nsCOMPtr<nsIBaseWindow> baseWindow
    (do_QueryInterface(window->GetDocShell()));
  NS_ENSURE_TRUE(baseWindow, nsnull);

  nsCOMPtr<nsIWidget> mainWidget;
  baseWindow->GetMainWidget(getter_AddRefs(mainWidget));
  return mainWidget;
}

#ifdef MOZ_WIDGET_GTK2
static nsGTKToolkit* GetGTKToolkit()
{
  nsCOMPtr<nsIAppShellService> svc = do_GetService(NS_APPSHELLSERVICE_CONTRACTID);
  if (!svc)
    return nsnull;
  nsCOMPtr<nsIDOMWindowInternal> window;
  svc->GetHiddenDOMWindow(getter_AddRefs(window));
  if (!window)
    return nsnull;
  nsIWidget* widget = GetMainWidget(window);
  if (!widget)
    return nsnull;
  nsIToolkit* toolkit = widget->GetToolkit();
  if (!toolkit)
    return nsnull;
  return NS_STATIC_CAST(nsGTKToolkit*, toolkit);
}
#endif

NS_IMETHODIMP
nsGTKRemoteService::RegisterWindow(nsIDOMWindow* aWindow)
{
  nsIWidget* mainWidget = GetMainWidget(aWindow);
  NS_ENSURE_TRUE(mainWidget, NS_ERROR_FAILURE);

  // walk up the widget tree and find the toplevel window in the
  // hierarchy

  nsIWidget* tempWidget = mainWidget->GetParent();

  while (tempWidget) {
    tempWidget = tempWidget->GetParent();
    if (tempWidget)
      mainWidget = tempWidget;
  }

  GtkWidget* widget =
    (GtkWidget*) mainWidget->GetNativeData(NS_NATIVE_SHELLWIDGET);
  NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);

  nsCOMPtr<nsIWeakReference> weak = do_GetWeakReference(aWindow);
  NS_ENSURE_TRUE(weak, NS_ERROR_FAILURE);

  if (!mWindows.IsInitialized())
    mWindows.Init();

  mWindows.Put(widget, weak);

  // If Startup() has already been called, immediately register this window.
  if (mServerWindow) {
    HandleCommandsFor(widget, weak);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGTKRemoteService::Shutdown()
{
  if (!mServerWindow)
    return NS_ERROR_NOT_INITIALIZED;

  gtk_widget_destroy(mServerWindow);
  mServerWindow = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsGTKRemoteService::Observe(nsISupports* aSubject,
                            const char *aTopic,
                            const PRUnichar *aData)
{
  // This can be xpcom-shutdown or quit-application, but it's the same either
  // way.
  Shutdown();
  return NS_OK;
}

// Minimize the roundtrips to the X server by getting all the atoms at once
static char *XAtomNames[] = {
  MOZILLA_VERSION_PROP,
  MOZILLA_LOCK_PROP,
  MOZILLA_COMMAND_PROP,
  MOZILLA_RESPONSE_PROP,
  MOZILLA_USER_PROP,
  MOZILLA_PROFILE_PROP,
  MOZILLA_PROGRAM_PROP,
  MOZILLA_COMMANDLINE_PROP
};
static Atom XAtoms[NS_ARRAY_LENGTH(XAtomNames)];

void
nsGTKRemoteService::EnsureAtoms(void)
{
  if (sMozVersionAtom)
    return;

  XInternAtoms(GDK_DISPLAY(), XAtomNames, NS_ARRAY_LENGTH(XAtomNames),
               False, XAtoms);
  int i = 0;
  sMozVersionAtom     = XAtoms[i++];
  sMozLockAtom        = XAtoms[i++];
  sMozCommandAtom     = XAtoms[i++];
  sMozResponseAtom    = XAtoms[i++];
  sMozUserAtom        = XAtoms[i++];
  sMozProfileAtom     = XAtoms[i++];
  sMozProgramAtom     = XAtoms[i++];
  sMozCommandLineAtom = XAtoms[i++];
}

#ifndef MOZ_XUL_APP
const char*
nsGTKRemoteService::HandleCommand(char* aCommand, nsIDOMWindow* aWindow, PRUint32 aTimestamp)
{
  nsresult rv;

  nsCOMPtr<nsISuiteRemoteService> remote
    (do_GetService("@mozilla.org/browser/xremoteservice;2"));
  if (!remote)
    return "509 internal error";

  rv = remote->ParseCommand(aCommand, aWindow);
  if (NS_SUCCEEDED(rv))
    return "200 executed command";

  if (NS_ERROR_INVALID_ARG == rv)
    return "500 command not parseable";

  if (NS_ERROR_NOT_IMPLEMENTED == rv)
    return "501 unrecognized command";

  return "509 internal error";
}

#else //MOZ_XUL_APP

// Set desktop startup ID to the passed ID, if there is one, so that any created
// windows get created with the right window manager metadata, and any windows
// that get new tabs and are activated also get the right WM metadata.
// If there is no desktop startup ID, then use the X event's timestamp
// for _NET_ACTIVE_WINDOW when the window gets focused or shown.
static void
SetDesktopStartupIDOrTimestamp(const nsACString& aDesktopStartupID,
                               PRUint32 aTimestamp) {
#ifdef MOZ_WIDGET_GTK2
  nsGTKToolkit* toolkit = GetGTKToolkit();
  if (!toolkit)
    return;
  if (!aDesktopStartupID.IsEmpty()) {
    toolkit->SetDesktopStartupID(aDesktopStartupID);
  } else {
    toolkit->SetFocusTimestamp(aTimestamp);
  }
#endif
}

static PRBool
FindExtensionParameterInCommand(const char* aParameterName,
                                const nsACString& aCommand,
                                char aSeparator,
                                nsACString* aValue)
{
  nsCAutoString searchFor;
  searchFor.Append(aSeparator);
  searchFor.Append(aParameterName);
  searchFor.Append('=');

  nsACString::const_iterator start, end;
  aCommand.BeginReading(start);
  aCommand.EndReading(end);
  if (!FindInReadable(searchFor, start, end))
    return PR_FALSE;

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
  return PR_TRUE;
}

const char*
nsGTKRemoteService::HandleCommand(char* aCommand, nsIDOMWindow* aWindow,
                                  PRUint32 aTimestamp)
{
  nsresult rv;

  nsCOMPtr<nsICommandLineRunner> cmdline
    (do_CreateInstance("@mozilla.org/toolkit/command-line;1", &rv));
  if (NS_FAILED(rv))
    return "509 internal error";

  // 1) Make sure that it looks remotely valid with parens
  // 2) Treat ping() immediately and specially

  nsCAutoString command(aCommand);
  PRInt32 p1, p2;
  p1 = command.FindChar('(');
  p2 = command.FindChar(')');

  if (p1 == kNotFound || p2 == kNotFound || p1 == 0 || p2 < p1) {
    return "500 command not parseable";
  }

  command.Truncate(p1);
  command.Trim(" ", PR_TRUE, PR_TRUE);
  ToLowerCase(command);

#ifdef DEBUG_bsmedberg
  printf("Processing xremote command: %s\n", command.get());
#endif

  if (!command.EqualsLiteral("ping")) {
    nsCAutoString desktopStartupID;
    nsDependentCString cmd(aCommand);
    FindExtensionParameterInCommand("DESKTOP_STARTUP_ID",
                                    cmd, '\n',
                                    &desktopStartupID);

    char* argv[3] = {"dummyappname", "-remote", aCommand};
    rv = cmdline->Init(3, argv, nsnull, nsICommandLine::STATE_REMOTE_EXPLICIT);
    if (NS_FAILED(rv))
      return "509 internal error";

    if (aWindow)
      cmdline->SetWindowContext(aWindow);

    SetDesktopStartupIDOrTimestamp(desktopStartupID, aTimestamp);

    rv = cmdline->Run();
    if (NS_ERROR_ABORT == rv)
      return "500 command not parseable";
    if (NS_FAILED(rv))
      return "509 internal error";
  }

  return "200 executed command";
}

const char*
nsGTKRemoteService::HandleCommandLine(char* aBuffer, nsIDOMWindow* aWindow,
                                      PRUint32 aTimestamp)
{
  nsresult rv;

  nsCOMPtr<nsICommandLineRunner> cmdline
    (do_CreateInstance("@mozilla.org/toolkit/command-line;1", &rv));
  if (NS_FAILED(rv))
    return "509 internal error";

  // the commandline property is constructed as an array of PRInt32
  // followed by a series of null-terminated strings:
  //
  // [argc][offsetargv0][offsetargv1...]<workingdir>\0<argv[0]>\0argv[1]...\0
  // (offset is from the beginning of the buffer)

  PRInt32 argc = TO_LITTLE_ENDIAN32(*NS_REINTERPRET_CAST(PRInt32*, aBuffer));
  char *wd   = aBuffer + ((argc + 1) * sizeof(PRInt32));

#ifdef DEBUG_bsmedberg
  printf("Receiving command line:\n"
         "  wd:\t%s\n"
         "  argc:\t%i\n",
         wd, argc);
#endif

  nsCOMPtr<nsILocalFile> lf;
  rv = NS_NewNativeLocalFile(nsDependentCString(wd), PR_TRUE,
                             getter_AddRefs(lf));
  if (NS_FAILED(rv))
    return "509 internal error";

  nsCAutoString desktopStartupID;

  char **argv = (char**) malloc(sizeof(char*) * argc);
  if (!argv) return "509 internal error";

  PRInt32  *offset = NS_REINTERPRET_CAST(PRInt32*, aBuffer) + 1;

  for (int i = 0; i < argc; ++i) {
    argv[i] = aBuffer + TO_LITTLE_ENDIAN32(offset[i]);

    if (i == 0) {
      nsDependentCString cmd(argv[0]);
      FindExtensionParameterInCommand("DESKTOP_STARTUP_ID",
                                      cmd, ' ',
                                      &desktopStartupID);
    }
#ifdef DEBUG_bsmedberg
    printf("  argv[%i]:\t%s\n", i, argv[i]);
#endif
  }

  rv = cmdline->Init(argc, argv, lf, nsICommandLine::STATE_REMOTE_AUTO);
  free (argv);
  if (NS_FAILED(rv)) {
    return "509 internal error";
  }

  if (aWindow)
    cmdline->SetWindowContext(aWindow);

  SetDesktopStartupIDOrTimestamp(desktopStartupID, aTimestamp);

  rv = cmdline->Run();
  
  if (NS_ERROR_ABORT == rv)
    return "500 command not parseable";
  
  if (NS_FAILED(rv))
    return "509 internal error";

  return "200 executed command";
}
#endif // MOZ_XUL_APP

void
nsGTKRemoteService::HandleCommandsFor(GtkWidget* widget,
                                      nsIWeakReference* aWindow)
{
#ifdef MOZ_WIDGET_GTK2
  g_signal_connect(G_OBJECT(widget), "property_notify_event",
                   G_CALLBACK(HandlePropertyChange), aWindow);
#else // GTK+
  gtk_signal_connect(GTK_OBJECT(widget), "property_notify_event",
                     GTK_SIGNAL_FUNC(HandlePropertyChange), aWindow);
#endif

  gtk_widget_add_events(widget, GDK_PROPERTY_CHANGE_MASK);

  Window window = GDK_WINDOW_XWINDOW(widget->window);

  // set our version
  XChangeProperty(GDK_DISPLAY(), window, sMozVersionAtom, XA_STRING,
                  8, PropModeReplace, kRemoteVersion, sizeof(kRemoteVersion) - 1);

  // get our username
  unsigned char *logname;
  logname = (unsigned char*) PR_GetEnv("LOGNAME");
  if (logname) {
    // set the property on the window if it's available
    XChangeProperty(GDK_DISPLAY(), window, sMozUserAtom, XA_STRING,
                    8, PropModeReplace, logname, strlen((char*) logname));
  }

  XChangeProperty(GDK_DISPLAY(), window, sMozProgramAtom, XA_STRING,
                  8, PropModeReplace, (unsigned char*) mAppName.get(), mAppName.Length());

  if (!mProfileName.IsEmpty()) {
    XChangeProperty(GDK_DISPLAY(), window, sMozProfileAtom, XA_STRING,
                    8, PropModeReplace, (unsigned char*) mProfileName.get(), mProfileName.Length());
  }
}

#ifdef MOZ_WIDGET_GTK2
#define CMP_GATOM_XATOM(gatom,xatom) (gatom == gdk_x11_xatom_to_atom(xatom))
#else
#define CMP_GATOM_XATOM(gatom,xatom) (gatom == xatom)
#endif

gboolean
nsGTKRemoteService::HandlePropertyChange(GtkWidget *aWidget,
                                         GdkEventProperty *pevent,
                                         nsIWeakReference* aThis)
{
  nsCOMPtr<nsIDOMWindow> window (do_QueryReferent(aThis));

  if (pevent->state == GDK_PROPERTY_NEW_VALUE &&
      CMP_GATOM_XATOM(pevent->atom, sMozCommandAtom)) {

    // We got a new command atom.
    int result;
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    char *data = 0;

    result = XGetWindowProperty (GDK_DISPLAY(),
                                 GDK_WINDOW_XWINDOW(pevent->window),
                                 sMozCommandAtom,
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

#ifdef DEBUG_bsmedberg
    printf("Handling command: %s\n", data);
#endif

    // Failed to get property off the window?
    if (result != Success)
      return FALSE;

    // Failed to get the data off the window or it was the wrong type?
    if (!data || !TO_LITTLE_ENDIAN32(*NS_REINTERPRET_CAST(PRInt32*, data)))
      return FALSE;

    // cool, we got the property data.
    const char *response = HandleCommand(data, window, pevent->time);

    // put the property onto the window as the response
    XChangeProperty (GDK_DISPLAY(), GDK_WINDOW_XWINDOW(pevent->window),
                     sMozResponseAtom, XA_STRING,
                     8, PropModeReplace, (const unsigned char *)response, strlen (response));
    XFree(data);
    return TRUE;
  }

#ifdef MOZ_XUL_APP
  if (pevent->state == GDK_PROPERTY_NEW_VALUE &&
      CMP_GATOM_XATOM(pevent->atom, sMozCommandLineAtom)) {

    // We got a new commandline atom.
    int result;
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    char *data = 0;

    result = XGetWindowProperty (GDK_DISPLAY(),
                                 GDK_WINDOW_XWINDOW(pevent->window),
                                 sMozCommandLineAtom,
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
      return FALSE;

    // Failed to get the data off the window or it was the wrong type?
    if (!data || !TO_LITTLE_ENDIAN32(*NS_REINTERPRET_CAST(PRInt32*, data)))
      return FALSE;

    // cool, we got the property data.
    const char *response = HandleCommandLine(data, window, pevent->time);

    // put the property onto the window as the response
    XChangeProperty (GDK_DISPLAY(), GDK_WINDOW_XWINDOW(pevent->window),
                     sMozResponseAtom, XA_STRING,
                     8, PropModeReplace, (const unsigned char *)response, strlen (response));
    XFree(data);
    return TRUE;
  }
#endif //MOZ_XUL_APP

  if (pevent->state == GDK_PROPERTY_NEW_VALUE && 
      CMP_GATOM_XATOM(pevent->atom, sMozResponseAtom)) {
    // client accepted the response.  party on wayne.
    return TRUE;
  }

  if (pevent->state == GDK_PROPERTY_NEW_VALUE && 
      CMP_GATOM_XATOM(pevent->atom, sMozLockAtom)) {
    // someone locked the window
    return TRUE;
  }

  return FALSE;
}

Atom nsGTKRemoteService::sMozVersionAtom;
Atom nsGTKRemoteService::sMozLockAtom;
Atom nsGTKRemoteService::sMozCommandAtom;
Atom nsGTKRemoteService::sMozResponseAtom;
Atom nsGTKRemoteService::sMozUserAtom;
Atom nsGTKRemoteService::sMozProfileAtom;
Atom nsGTKRemoteService::sMozProgramAtom;
Atom nsGTKRemoteService::sMozCommandLineAtom;

// {C0773E90-5799-4eff-AD03-3EBCD85624AC}
#define NS_REMOTESERVICE_CID \
  { 0xc0773e90, 0x5799, 0x4eff, { 0xad, 0x3, 0x3e, 0xbc, 0xd8, 0x56, 0x24, 0xac } }

NS_GENERIC_FACTORY_CONSTRUCTOR(nsGTKRemoteService)

static const nsModuleComponentInfo components[] =
{
  { "Remote Service",
    NS_REMOTESERVICE_CID,
    "@mozilla.org/toolkit/remote-service;1",
    nsGTKRemoteServiceConstructor
  }
};

NS_IMPL_NSGETMODULE(RemoteServiceModule, components)
