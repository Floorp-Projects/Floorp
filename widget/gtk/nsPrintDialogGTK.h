/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintDialog_h__
#define nsPrintDialog_h__

#include "nsIPrintDialogService.h"

class nsIPrintSettings;

// Copy the print pages enum here because not all versions
// have SELECTION, which we will use
typedef enum {
  _GTK_PRINT_PAGES_ALL,
  _GTK_PRINT_PAGES_CURRENT,
  _GTK_PRINT_PAGES_RANGES,
  _GTK_PRINT_PAGES_SELECTION
} _GtkPrintPages;

class nsPrintDialogServiceGTK final : public nsIPrintDialogService {
  virtual ~nsPrintDialogServiceGTK();

 public:
  nsPrintDialogServiceGTK();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTDIALOGSERVICE
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsPrintDialogServiceGTK,
                              NS_IPRINTDIALOGSERVICE_IID)

#endif
