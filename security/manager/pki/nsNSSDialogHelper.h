/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNSSDialogHelper_h
#define nsNSSDialogHelper_h

class mozIDOMWindowProxy;
class nsISupports;

/**
 * Helper class that uses the window watcher service to open a standard dialog,
 * with or without a parent context.
 */
class nsNSSDialogHelper
{
public:
  /**
   * Opens a XUL dialog.
   *
   * @param window
   *        Parent window of the dialog, or nullptr to signal no parent.
   * @param url
   *        URL to the XUL dialog.
   * @param params
   *        Parameters to pass to the dialog. Same semantics as the
   *        nsIWindowWatcher.openWindow() |aArguments| parameter.
   * @param modal
   *        true if the dialog should be modal, false otherwise.
   * @return The result of opening the dialog.
   */
  static nsresult openDialog(mozIDOMWindowProxy* window, const char* url,
                             nsISupports* params, bool modal = true);
};

#endif // nsNSSDialogHelper_h
