/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

