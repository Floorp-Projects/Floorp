/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsPrintProgress_h
#define __nsPrintProgress_h

#include "nsIPrintProgress.h"

#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsString.h"

class nsIDOMWindow;
class nsIObserver;
class nsIPrintSettings;

class nsPrintProgress : public nsIPrintProgress {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIPRINTPROGRESS
  NS_DECL_NSIWEBPROGRESSLISTENER

  explicit nsPrintProgress(nsIPrintSettings* aPrintSettings);

 protected:
  virtual ~nsPrintProgress();

 private:
  nsresult ReleaseListeners();

  bool m_closeProgress;
  bool m_processCanceled;
  nsString m_pendingStatus;
  int32_t m_pendingStateFlags;
  nsresult m_pendingStateValue;
  nsCOMPtr<nsIDOMWindow> m_dialog;
  nsCOMArray<nsIWebProgressListener> m_listenerList;
  nsCOMPtr<nsIObserver> m_observer;
  nsCOMPtr<nsIPrintSettings> m_PrintSetting;
};

#endif
