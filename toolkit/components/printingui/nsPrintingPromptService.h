/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsPrintingPromptService_h
#define __nsPrintingPromptService_h

// {E042570C-62DE-4bb6-A6E0-798E3C07B4DF}
#define NS_PRINTINGPROMPTSERVICE_CID                \
  {                                                 \
    0xe042570c, 0x62de, 0x4bb6, {                   \
      0xa6, 0xe0, 0x79, 0x8e, 0x3c, 0x7, 0xb4, 0xdf \
    }                                               \
  }
#define NS_PRINTINGPROMPTSERVICE_CONTRACTID \
  "@mozilla.org/embedcomp/printingprompt-service;1"

#include "nsCOMPtr.h"
#include "nsIPrintingPromptService.h"
#include "nsIWindowWatcher.h"

// Printing Progress Includes
#include "nsIWebProgressListener.h"
#if !defined(XP_MACOSX)
#  include "nsPrintProgress.h"

class nsIDOMWindow;
class nsIDialogParamBlock;
#endif

class nsPrintingPromptService final : public nsIPrintingPromptService,
                                      public nsIWebProgressListener {
 public:
  static already_AddRefed<nsPrintingPromptService> GetSingleton();

  NS_DECL_NSIPRINTINGPROMPTSERVICE
  NS_DECL_NSIWEBPROGRESSLISTENER
  NS_DECL_ISUPPORTS

 protected:
  nsPrintingPromptService();
  virtual ~nsPrintingPromptService();
  nsresult Init();

 private:
#if !defined(XP_MACOSX)
  nsCOMPtr<nsIWindowWatcher> mWatcher;
  nsCOMPtr<nsIPrintProgress> mPrintProgress;
  nsCOMPtr<nsIWebProgressListener> mWebProgressListener;
#endif
};

#endif
