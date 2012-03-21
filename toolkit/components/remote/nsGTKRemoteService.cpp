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
 *   Miika Jarvinen <mjarvin@gmail.com>
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

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsPIDOMWindow.h"
#include "mozilla/ModuleUtils.h"
#include "nsIServiceManager.h"
#include "nsIWeakReference.h"
#include "nsIWidget.h"
#include "nsIAppShellService.h"
#include "nsAppShellCID.h"

#include "nsCOMPtr.h"

#include "nsGTKToolkit.h"

NS_IMPL_ISUPPORTS2(nsGTKRemoteService,
                   nsIRemoteService,
                   nsIObserver)

NS_IMETHODIMP
nsGTKRemoteService::Startup(const char* aAppName, const char* aProfileName)
{
  NS_ASSERTION(aAppName, "Don't pass a null appname!");
  sRemoteImplementation = this;

  if (mServerWindow) return NS_ERROR_ALREADY_INITIALIZED;

  XRemoteBaseStartup(aAppName, aProfileName);

  mServerWindow = gtk_invisible_new();
  gtk_widget_realize(mServerWindow);
  HandleCommandsFor(mServerWindow, nsnull);

  if (!mWindows.IsInitialized())
    mWindows.Init();

  mWindows.EnumerateRead(StartupHandler, this);

  return NS_OK;
}

PLDHashOperator
nsGTKRemoteService::StartupHandler(GtkWidget* aKey,
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

// Set desktop startup ID to the passed ID, if there is one, so that any created
// windows get created with the right window manager metadata, and any windows
// that get new tabs and are activated also get the right WM metadata.
// The timestamp will be used if there is no desktop startup ID, or if we're
// raising an existing window rather than showing a new window for the first time.
void
nsGTKRemoteService::SetDesktopStartupIDOrTimestamp(const nsACString& aDesktopStartupID,
                                                   PRUint32 aTimestamp) {
  nsGTKToolkit* toolkit = nsGTKToolkit::GetToolkit();
  if (!toolkit)
    return;

  if (!aDesktopStartupID.IsEmpty()) {
    toolkit->SetDesktopStartupID(aDesktopStartupID);
  }

  toolkit->SetFocusTimestamp(aTimestamp);
}


void
nsGTKRemoteService::HandleCommandsFor(GtkWidget* widget,
                                      nsIWeakReference* aWindow)
{
  g_signal_connect(G_OBJECT(widget), "property_notify_event",
                   G_CALLBACK(HandlePropertyChange), aWindow);

  gtk_widget_add_events(widget, GDK_PROPERTY_CHANGE_MASK);

  Window window = GDK_WINDOW_XWINDOW(widget->window);
  nsXRemoteService::HandleCommandsFor(window);

}

gboolean
nsGTKRemoteService::HandlePropertyChange(GtkWidget *aWidget,
                                         GdkEventProperty *pevent,
                                         nsIWeakReference *aThis)
{
  if (pevent->state == GDK_PROPERTY_NEW_VALUE) {
    Atom changedAtom = gdk_x11_atom_to_xatom(pevent->atom);

    return HandleNewProperty(GDK_WINDOW_XWINDOW(pevent->window),
                             GDK_DISPLAY(),
                             pevent->time, changedAtom, aThis);
  }
  return FALSE;
}


// {C0773E90-5799-4eff-AD03-3EBCD85624AC}
#define NS_REMOTESERVICE_CID \
  { 0xc0773e90, 0x5799, 0x4eff, { 0xad, 0x3, 0x3e, 0xbc, 0xd8, 0x56, 0x24, 0xac } }

NS_GENERIC_FACTORY_CONSTRUCTOR(nsGTKRemoteService)
NS_DEFINE_NAMED_CID(NS_REMOTESERVICE_CID);

static const mozilla::Module::CIDEntry kRemoteCIDs[] = {
  { &kNS_REMOTESERVICE_CID, false, NULL, nsGTKRemoteServiceConstructor },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kRemoteContracts[] = {
  { "@mozilla.org/toolkit/remote-service;1", &kNS_REMOTESERVICE_CID },
  { NULL }
};

static const mozilla::Module kRemoteModule = {
  mozilla::Module::kVersion,
  kRemoteCIDs,
  kRemoteContracts
};

NSMODULE_DEFN(RemoteServiceModule) = &kRemoteModule;
