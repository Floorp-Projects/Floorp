/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Types.h"

#include <gtk/gtk.h>

#include "nsApplicationChooser.h"
#include "WidgetUtils.h"
#include "nsIMIMEInfo.h"
#include "nsIWidget.h"
#include "nsCExternalHandlerService.h"
#include "nsComponentManagerUtils.h"
#include "nsGtkUtils.h"
#include "nsPIDOMWindow.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsApplicationChooser, nsIApplicationChooser)

nsApplicationChooser::nsApplicationChooser()
{
}

nsApplicationChooser::~nsApplicationChooser()
{
}

NS_IMETHODIMP
nsApplicationChooser::Init(mozIDOMWindowProxy* aParent,
                           const nsACString& aTitle)
{
  NS_ENSURE_TRUE(aParent, NS_ERROR_FAILURE);
  auto* parent = nsPIDOMWindowOuter::From(aParent);
  mParentWidget = widget::WidgetUtils::DOMWindowToWidget(parent);
  mWindowTitle.Assign(aTitle);
  return NS_OK;
}

NS_IMETHODIMP
nsApplicationChooser::Open(const nsACString& aContentType, nsIApplicationChooserFinishedCallback *aCallback)
{
  MOZ_ASSERT(aCallback);
  if (mCallback) {
    NS_WARNING("Chooser is already in progress.");
    return NS_ERROR_ALREADY_INITIALIZED;
  }
  mCallback = aCallback;
  NS_ENSURE_TRUE(mParentWidget, NS_ERROR_FAILURE);
  GtkWindow *parent_widget =
    GTK_WINDOW(mParentWidget->GetNativeData(NS_NATIVE_SHELLWIDGET));

  GtkWidget* chooser =
    gtk_app_chooser_dialog_new_for_content_type(parent_widget,
        (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        PromiseFlatCString(aContentType).get());
  gtk_app_chooser_dialog_set_heading(GTK_APP_CHOOSER_DIALOG(chooser), mWindowTitle.BeginReading());
  NS_ADDREF_THIS();
  g_signal_connect(chooser, "response", G_CALLBACK(OnResponse), this);
  g_signal_connect(chooser, "destroy", G_CALLBACK(OnDestroy), this);
  gtk_widget_show(chooser);
  return NS_OK;
}

/* static */ void
nsApplicationChooser::OnResponse(GtkWidget* chooser, gint response_id, gpointer user_data)
{
  static_cast<nsApplicationChooser*>(user_data)->Done(chooser, response_id);
}

/* static */ void
nsApplicationChooser::OnDestroy(GtkWidget *chooser, gpointer user_data)
{
  static_cast<nsApplicationChooser*>(user_data)->Done(chooser, GTK_RESPONSE_CANCEL);
}

void nsApplicationChooser::Done(GtkWidget* chooser, gint response)
{
  nsCOMPtr<nsILocalHandlerApp> localHandler;
  nsresult rv;
  switch (response) {
    case GTK_RESPONSE_OK:
    case GTK_RESPONSE_ACCEPT:
        {
          localHandler = do_CreateInstance(NS_LOCALHANDLERAPP_CONTRACTID, &rv);
          if (NS_FAILED(rv)) {
            NS_WARNING("Out of memory.");
            break;
          }
          GAppInfo *app_info = gtk_app_chooser_get_app_info(GTK_APP_CHOOSER(chooser));

          nsCOMPtr<nsIFile> localExecutable;
          gchar *fileWithFullPath = g_find_program_in_path(g_app_info_get_executable(app_info));
          rv = NS_NewNativeLocalFile(nsDependentCString(fileWithFullPath), false, getter_AddRefs(localExecutable));
          g_free(fileWithFullPath);
          if (NS_FAILED(rv)) {
            NS_WARNING("Cannot create local filename.");
            localHandler = nullptr;
          } else {
            localHandler->SetExecutable(localExecutable);
            localHandler->SetName(NS_ConvertUTF8toUTF16(g_app_info_get_display_name(app_info)));
          }
          g_object_unref(app_info);
        }

        break;
    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_CLOSE:
    case GTK_RESPONSE_DELETE_EVENT:
        break;
    default:
        NS_WARNING("Unexpected response");
        break;
  }

  // A "response" signal won't be sent again but "destroy" will be.
  g_signal_handlers_disconnect_by_func(chooser, FuncToGpointer(OnDestroy), this);
  gtk_widget_destroy(chooser);

  if (mCallback) {
    mCallback->Done(localHandler);
    mCallback = nullptr;
  }
  NS_RELEASE_THIS();
}

