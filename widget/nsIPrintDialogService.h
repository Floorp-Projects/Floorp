/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Kenneth Herron <kherron@fmailbox.com>
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsIPrintDialogService_h__
#define nsIPrintDialogService_h__

#include "nsISupports.h"

class nsIDOMWindow;
class nsIPrintSettings;
class nsIWebBrowserPrint;

/*
 * Interface to a print dialog accessed through the widget library.
 */

#define NS_IPRINTDIALOGSERVICE_IID \
{ 0x3715eb1a, 0xb314, 0x447c, \
{ 0x95, 0x33, 0xd0, 0x6a, 0x6d, 0xa6, 0xa6, 0xf0 } }


/**
 *
 */
class nsIPrintDialogService  : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IPRINTDIALOGSERVICE_IID)

  /**
   * Initialize the service.
   * @return NS_OK or a suitable error.
   */
  NS_IMETHOD Init() = 0;

  /**
   * Show the print dialog.
   * @param aParent   A DOM window the dialog will be parented to.
   * @param aSettings On entry, this contains initial settings for the
   *                  print dialog. On return, if the print operation should
   *                  proceed then this contains settings for the print
   *                  operation.
   * @param aWebBrowserPrint A nsIWebBrowserPrint object that can be used for
   *                         retreiving the title of the printed document.
   * @return NS_OK if the print operation should proceed
   * @return NS_ERROR_ABORT if the user indicated not to proceed
   * @return a suitable error for failures to show the print dialog.
   */
  NS_IMETHOD Show(nsIDOMWindow *aParent, nsIPrintSettings *aSettings,
                  nsIWebBrowserPrint *aWebBrowserPrint) = 0;

  /**
   * Show the page setup dialog. Note that there is no way to tell whether the
   * user clicked OK or Cancel on the dialog.
   * @param aParent   A DOM window the dialog will be parented to.
   * @param aSettings On entry, this contains initial settings for the
   *                  page setup dialog. On return, this contains new default
   *                  page setup options.
   * @return NS_OK if everything is OK.
   * @return a suitable error for failures to show the page setup dialog.
   */
  NS_IMETHOD ShowPageSetup(nsIDOMWindow *aParent, nsIPrintSettings *aSettings) = 0;

};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIPrintDialogService, NS_IPRINTDIALOGSERVICE_IID)

#define NS_PRINTDIALOGSERVICE_CONTRACTID ("@mozilla.org/widget/printdialog-service;1")

#endif // nsIPrintDialogService_h__

