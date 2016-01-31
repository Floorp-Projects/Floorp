/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=8:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

NS_IMPL_ISUPPORTS(nsGTKRemoteService,
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
  HandleCommandsFor(mServerWindow, nullptr);

  for (auto iter = mWindows.Iter(); !iter.Done(); iter.Next()) {
    HandleCommandsFor(iter.Key(), iter.UserData());
  }

  return NS_OK;
}

static nsIWidget* GetMainWidget(nsPIDOMWindowInner* aWindow)
{
  // get the native window for this instance
  nsCOMPtr<nsIBaseWindow> baseWindow
    (do_QueryInterface(aWindow->GetDocShell()));
  NS_ENSURE_TRUE(baseWindow, nullptr);

  nsCOMPtr<nsIWidget> mainWidget;
  baseWindow->GetMainWidget(getter_AddRefs(mainWidget));
  return mainWidget;
}

NS_IMETHODIMP
nsGTKRemoteService::RegisterWindow(mozIDOMWindow* aWindow)
{
  nsIWidget* mainWidget = GetMainWidget(nsPIDOMWindowInner::From(aWindow));
  NS_ENSURE_TRUE(mainWidget, NS_ERROR_FAILURE);

  GtkWidget* widget =
    (GtkWidget*) mainWidget->GetNativeData(NS_NATIVE_SHELLWIDGET);
  NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);

  nsCOMPtr<nsIWeakReference> weak = do_GetWeakReference(aWindow);
  NS_ENSURE_TRUE(weak, NS_ERROR_FAILURE);

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
  mServerWindow = nullptr;
  return NS_OK;
}

// Set desktop startup ID to the passed ID, if there is one, so that any created
// windows get created with the right window manager metadata, and any windows
// that get new tabs and are activated also get the right WM metadata.
// The timestamp will be used if there is no desktop startup ID, or if we're
// raising an existing window rather than showing a new window for the first time.
void
nsGTKRemoteService::SetDesktopStartupIDOrTimestamp(const nsACString& aDesktopStartupID,
                                                   uint32_t aTimestamp) {
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

#if (MOZ_WIDGET_GTK == 2)
  Window window = GDK_WINDOW_XWINDOW(widget->window);
#else
  Window window = gdk_x11_window_get_xid(gtk_widget_get_window(widget));
#endif
  nsXRemoteService::HandleCommandsFor(window);

}

gboolean
nsGTKRemoteService::HandlePropertyChange(GtkWidget *aWidget,
                                         GdkEventProperty *pevent,
                                         nsIWeakReference *aThis)
{
  if (pevent->state == GDK_PROPERTY_NEW_VALUE) {
    Atom changedAtom = gdk_x11_atom_to_xatom(pevent->atom);

#if (MOZ_WIDGET_GTK == 2)
    XID window = GDK_WINDOW_XWINDOW(pevent->window);
#else
    XID window = gdk_x11_window_get_xid(gtk_widget_get_window(aWidget));
#endif
    return HandleNewProperty(window,
                             GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
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
  { &kNS_REMOTESERVICE_CID, false, nullptr, nsGTKRemoteServiceConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kRemoteContracts[] = {
  { "@mozilla.org/toolkit/remote-service;1", &kNS_REMOTESERVICE_CID },
  { nullptr }
};

static const mozilla::Module kRemoteModule = {
  mozilla::Module::kVersion,
  kRemoteCIDs,
  kRemoteContracts
};

NSMODULE_DEFN(RemoteServiceModule) = &kRemoteModule;
