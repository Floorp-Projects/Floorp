/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef downloadmanager___h___
#define downloadmanager___h___

#include "nsIDownloadManager.h"
#include "nsIDownloadProgressListener.h"
#include "nsIFile.h"
#include "nsIStringBundle.h"
#include "nsISupportsPrimitives.h"
#include "nsString.h"

class nsDownloadManager final : public nsIDownloadManager
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOWNLOADMANAGER

  nsresult Init();

  static already_AddRefed<nsDownloadManager> GetSingleton();

  nsDownloadManager()
  {
  }

protected:
  virtual ~nsDownloadManager();

private:
  nsCOMPtr<nsIStringBundle> mBundle;

  static nsDownloadManager *gDownloadManagerService;
};

#endif
