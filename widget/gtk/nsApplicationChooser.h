/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsApplicationChooser_h__
#define nsApplicationChooser_h__

#include <gtk/gtk.h>
#include "nsCOMPtr.h"
#include "nsIApplicationChooser.h"
#include "nsString.h"

class nsIWidget;

class nsApplicationChooser final : public nsIApplicationChooser
{
public:
  nsApplicationChooser();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIAPPLICATIONCHOOSER
  void Done(GtkWidget* chooser, gint response);

private:
  ~nsApplicationChooser();
  nsCOMPtr<nsIWidget> mParentWidget;
  nsCString mWindowTitle;
  nsCOMPtr<nsIApplicationChooserFinishedCallback> mCallback;
  static void OnResponse(GtkWidget* chooser, gint response_id, gpointer user_data);
  static void OnDestroy(GtkWidget* chooser, gpointer user_data);
};
#endif
